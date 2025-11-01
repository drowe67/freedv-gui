//=========================================================================
// Name:            TcpConnectionHandler.cpp
// Purpose:         Handler for TCP connections.
//
// Authors:         Mooneer Salem
// License:
//
//  All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//=========================================================================

#include <chrono>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <unistd.h>
#include <algorithm>
#include <sstream>

#if defined(WIN32) || defined(__MINGW32__)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif // !_WIN32_WINNT

#include <winsock2.h>
#include <ws2tcpip.h>
#include <ws2def.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#endif // defined(WIN32) || defined(__MINGW32__)

#include "TcpConnectionHandler.h"
#include "logging/ulog.h"

using namespace std::chrono_literals;

#define RX_ATTEMPT_INTERVAL_MS (50)
#define RECONNECT_INTERVAL_MS (5000)
#define RX_BUFFER_SIZE (128 * 1024)

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif // INVALID_SOCKET

TcpConnectionHandler::TcpConnectionHandler()
    : enableReconnect_(false)
    , reconnectTimer_(RECONNECT_INTERVAL_MS, [&](ThreadedTimer&) {
        enqueue_(std::bind(&TcpConnectionHandler::connectImpl_, this));
    }, false)
    , socket_(-1)
    , cancelConnect_(false)
    , receiveBuffer_(RX_BUFFER_SIZE)
{
#if defined(WIN32)
    // Initialize Winsock in case it hasn't already been done.
    WSADATA wsaData;
    int result = 0;
    result = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (result != 0)
    {
        log_warn("Winsock could not be initialized: %d", result); 
    }
#endif // defined(WIN32)
}

TcpConnectionHandler::~TcpConnectionHandler()
{
    // Make sure we're disconnected before destroying.
    enableReconnect_ = false;

    auto fut = disconnect();
    fut.wait();

#if defined(WIN32)
    WSACleanup();
#endif // defined(WIN32)
}

std::future<void> TcpConnectionHandler::connect(const char* host, int port, bool enableReconnect)
{
    cancelConnect_ = false;
    host_ = host;
    port_ = port;
    enableReconnect_ = enableReconnect;
    
    std::shared_ptr<std::promise<void>> prom = std::make_shared<std::promise<void> >();
    auto fut = prom->get_future();
    
    enqueue_([&, prom]() {
        connectImpl_();
        prom->set_value();
    });
    return fut;
}

std::future<void> TcpConnectionHandler::disconnect()
{
    // Cancel any pending connections
    cancelConnect_ = true;
    
    std::shared_ptr<std::promise<void>> prom = std::make_shared<std::promise<void> >();
    auto fut = prom->get_future();
    
    enqueue_([&, prom]() {
        disconnectImpl_();
        cancelConnect_ = false;
        prom->set_value();
    });
    return fut;
}

std::future<void> TcpConnectionHandler::send(const char* buf, int length)
{
    std::shared_ptr<std::promise<void>> prom = std::make_shared<std::promise<void> >();
    auto fut = prom->get_future();
    
    char* allocBuf = new char[length];
    assert(allocBuf != nullptr);
    memcpy(allocBuf, buf, length);
    
    enqueue_([&, prom, allocBuf, length]() {
        sendImpl_(allocBuf, length);
        delete[] allocBuf;
        prom->set_value();
    });
    return fut;
}

void TcpConnectionHandler::setOnRecvEndFn(OnRecvEndFn fn)
{
    enqueue_([this, fn]() {
        onRecvEndFn_ = fn;
    });
}

void TcpConnectionHandler::connectImpl_()
{
    // Convert port to string (needed by getaddrinfo() below).
    std::stringstream portStream;
    portStream << port_;
    
    log_info("Attempting connection to %s port %d", host_.c_str(), port_);
    
    // Happy Eyeballs v2.0:
    //     * Begin asynchronous DNS requests (IPv6 followed by IPv4)
    //     * If IPv6 received first: immediately attempt IPv6 connection
    //                         Else: wait 50 ms for IPv6 DNS to come back before attempting IPv4
    //     * Start new connection every 250ms until one succeeds or all fail.
    // Ref: https://datatracker.ietf.org/doc/html/rfc8305
    struct addrinfo* results[] = {nullptr, nullptr}; // [0] = IPv6, [1] = IPv4
    struct addrinfo* heads[] = {nullptr, nullptr};
    
    ipv4Complete_ = false;
    ipv6Complete_ = false;

    std::shared_ptr<std::promise<struct addrinfo*> > ipv6ResultPromise = 
        std::make_shared<std::promise<struct addrinfo*> >();
    std::shared_ptr<std::promise<struct addrinfo*> > ipv4ResultPromise = 
        std::make_shared<std::promise<struct addrinfo*> >();
    std::future<struct addrinfo*> ipv6ResultFuture = ipv6ResultPromise->get_future();
    std::future<struct addrinfo*> ipv4ResultFuture = ipv4ResultPromise->get_future();

    std::string portStr = portStream.str();
    std::thread ipv6ResolveThread([&, portStr, ipv6ResultPromise]() {
        struct addrinfo *result = nullptr;
        resolveAddresses_(AF_INET6, host_.c_str(), portStr.c_str(), &result);
        ipv6ResultPromise->set_value(result);
        ipv6Complete_ = true;
    });
    
    std::thread ipv4ResolveThread([&, portStr, ipv4ResultPromise]() {
        struct addrinfo *result = nullptr;
        resolveAddresses_(AF_INET, host_.c_str(), portStr.c_str(), &result);
        ipv4ResultPromise->set_value(result);
        ipv4Complete_ = true;
    });
    
    log_info("waiting for DNS");
    
    while (!cancelConnect_ && ipv4Complete_ == false && ipv6Complete_ == false)
    {
        std::this_thread::sleep_for(1ms);
    }
    
    log_info("some DNS results are available (v4 = %d, v6 = %d)", (bool)ipv4Complete_, (bool)ipv6Complete_);

    if (ipv6Complete_ == true)
    {
        heads[0] = ipv6ResultFuture.get();
        results[0] = heads[0];
    }

    if (ipv4Complete_ == true)
    {
        heads[1] = ipv4ResultFuture.get();
        results[1] = heads[1];
    }

    if (ipv6Complete_ == false)
    {
        log_info("waiting additional time for IPv6 DNS results");
        std::this_thread::sleep_for(50ms);
        if (ipv6Complete_ == true)
        {
            heads[0] = ipv6ResultFuture.get();
            results[0] = heads[0];
        }
    }
    if (ipv4Complete_ == false && ipv6Complete_ == true && results[0] == nullptr)
    {
        log_info("no valid IPv6 results, need to wait for IPv4 before continuing.");
        while (ipv4Complete_ == false)
        {
            std::this_thread::sleep_for(1ms);
        }
        heads[1] = ipv4ResultFuture.get();
        results[1] = heads[1];
    }

    int whichIndex = 0;
    if (ipv6Complete_ == true && results[0] != nullptr)
    {
        log_info("starting with IPv6 connection");
    }
    else if (ipv4Complete_ == true && results[1] != nullptr)
    {
        log_info("starting with IPv4 connection");
        whichIndex = 1;
    }

#if !defined(WIN32)
    int flags = 0;
    std::vector<int> pendingSockets;
#else
    std::vector<SOCKET> pendingSockets;
#endif // !defined(WIN32)
    
    while (!cancelConnect_ && (results[0] || results[1]))
    {
        struct addrinfo* current = results[whichIndex];
        int ret = 0;
#if defined(WIN32)
        u_long mode = 1;  // 1 to enable non-blocking socket
#endif // defined(WIN32)
        
        log_debug("create socket");
#if defined(WIN32)
        SOCKET fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (fd == INVALID_SOCKET)
        {
            log_warn("cannot open socket (err=%d)", WSAGetLastError());
            goto next_fd;
        }
#else
        int fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if(fd < 0)
        {
            log_warn("cannot open socket (err=%d)", errno);
            goto next_fd;
        }
#endif // defined(WIN32)

        pendingSockets.push_back(fd);
            
        // Set socket as non-blocking. Required to enable Happy Eyeballs behavior. 
        log_debug("set socket non-blocking"); 
#if defined(WIN32)
        if (ioctlsocket(fd, FIONBIO, &mode) != 0)
        {
            log_warn("cannot set socket as nonblocking (err=%d)", WSAGetLastError());
            closesocket(fd);
            pendingSockets.pop_back();
            goto next_fd;
        }
#else
        flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1)
        {
            log_warn("cannot get socket flags (err=%d)", errno);
            close(fd);
            pendingSockets.pop_back();
            goto next_fd;
        }
        
        flags |= O_NONBLOCK;
        if (fcntl(fd, F_SETFL, flags) != 0)
        {
            log_warn("cannot set socket as non-blocking (err=%d)", errno);
            close(fd);
            pendingSockets.pop_back();
            goto next_fd;
        }
#endif // defined(WIN32)
        
        // Start connection attempt
        char buf[256];
        getnameinfo(current->ai_addr, current->ai_addrlen, buf, sizeof(buf), nullptr, 0, NI_NUMERICHOST);

        log_info("attempting connection to %s", buf);
        ret = ::connect(fd, current->ai_addr, current->ai_addrlen);
        if (ret == 0)
        {
            // Connection succeeded immediately -- no need to attempt any other IPs
            socket_ = fd;
            break;
        }
#if defined(WIN32)
        else if (WSAGetLastError() != WSAEWOULDBLOCK)
        {
            int lastErr = WSAGetLastError();
            log_warn("cannot start connection to %s (err=%d)", buf, lastErr);
            closesocket(fd);
            pendingSockets.pop_back();
            goto next_fd;
        }
#else
        else if (ret == -1 && errno != EINPROGRESS)
        {
            int err = errno;
            log_warn("cannot start connection to %s (err=%d: %s)", buf, err, strerror(err));
            close(fd);
            pendingSockets.pop_back();
            goto next_fd;
        }
#endif // defined(WIN32)        

        // Check socket list to see if there have been any connections yet.
        checkConnections_(pendingSockets);
        if (socket_ > 0)
        {
            break;
        }
        
next_fd:
        results[whichIndex] = results[whichIndex]->ai_next;
        whichIndex = (whichIndex + 1) % 2;

        // See if we got any new DNS results.
        if (whichIndex == 0 && ipv6ResultFuture.valid())
        {
            heads[0] = ipv6ResultFuture.get();
            results[0] = heads[0];
        }
        if (whichIndex == 1 && ipv4ResultFuture.valid())
        {
            heads[1] = ipv4ResultFuture.get();
            results[1] = heads[1];
        }

        if (results[whichIndex] == nullptr)
        {
            whichIndex = (whichIndex + 1) % 2;
            
            if (results[whichIndex] == nullptr && (ipv4Complete_ == false || ipv6Complete_ == false))
            {
                // No more addresses to go through, so we *really* need to make sure
                // we're done with DNS before exiting the loop.
                log_info("ran out of addresses to check but DNS requests are still pending");
                while (ipv4Complete_ == false || ipv6Complete_ == false)
                {
                    std::this_thread::sleep_for(1ms);
                }
                
                if (ipv4ResultFuture.valid())
                {
                    heads[1] = ipv4ResultFuture.get();
                    results[1] = heads[1];
                    whichIndex = 1;
                }
                
                if (ipv6ResultFuture.valid())
                {
                    heads[0] = ipv6ResultFuture.get();
                    results[0] = heads[0];
                    if (results[0] != nullptr)
                    {
                        // Prioritize IPv6 over IPv4 if we finally got results for the former.
                        whichIndex = 0;
                    }
                }
            }
        }
    }
    
    // Keep checking connections until one connects or we all time out.
    while (!cancelConnect_ && pendingSockets.size() > 0 && socket_ == INVALID_SOCKET)
    {
        checkConnections_(pendingSockets);
    }
    
    // Close any connections still pending.
    for (auto& sock : pendingSockets)
    {
        if (sock != socket_)
        {
#if defined(WIN32)
            closesocket(sock);
#else
            close(sock);
#endif // defined(WIN32)
        }
    }
    
    if (socket_ != INVALID_SOCKET)
    {
        // Call connect handler (defined by child class).
        char buf[256];
        struct sockaddr_storage addr;
        socklen_t len = sizeof(addr);
        if (getpeername(socket_, (struct sockaddr*)&addr, &len) != 0)
        {
#if defined(WIN32)
            int err = WSAGetLastError();
#else
            int err = errno;
#endif // defined(WIN32)
            log_warn("could not get IP address of socket (err=%d)", err);
        }
        int nameErr = 0;
        if ((nameErr = getnameinfo((struct sockaddr*)&addr, len, buf, sizeof(buf), nullptr, 0, NI_NUMERICHOST)) != 0)
        {
#if defined(WIN32)
            nameErr = WSAGetLastError();
#endif // defined(WIN32)
            log_warn("could not get string representation of IP address (err=%d)", nameErr);
        }

        log_info("connection succeeded to %s", buf);
        enqueue_(std::bind(&TcpConnectionHandler::onConnect_, this));
        
        // Start receive thread
        receiveThread_ = std::thread(std::bind(&TcpConnectionHandler::receiveImpl_, this));
    }
    else if (enableReconnect_)
    {
        // Attempt reconnect
        log_warn("connection failed, waiting to reconnect");
        reconnectTimer_.start();
    }
    
    // Free address info objects
    ipv6ResolveThread.detach();
    ipv4ResolveThread.detach();
    while (!ipv4Complete_ || !ipv6Complete_)
    {
        std::this_thread::sleep_for(1ms);
    }
    
    if (ipv4ResultFuture.valid())
    {
        heads[1] = ipv4ResultFuture.get();
    }
    if (ipv6ResultFuture.valid())
    {
        heads[0] = ipv6ResultFuture.get();
    }    
    if (heads[0] != nullptr)
    {
        freeaddrinfo(heads[0]);
    }
    if (heads[1] != nullptr)
    {
        freeaddrinfo(heads[1]);
    }
}

void TcpConnectionHandler::disconnectImpl_()
{
    if (socket_ > 0)
    {
        auto tmp = socket_.load();
        socket_ = INVALID_SOCKET;

#if defined(WIN32)
        closesocket(tmp);
#else
        close(tmp);
#endif // defined(WIN32)

        if (receiveThread_.joinable())
        {
            receiveThread_.join();
        }

        onDisconnect_();
        
        if (enableReconnect_)
        {
            reconnectTimer_.start();
        }
    }
}

void TcpConnectionHandler::sendImpl_(const char* buf, int length)
{
    if (socket_ > 0)
    {
        // Simulate blocking socket for write. Most of the time this should
        // complete immediately anyway.
        while (length > 0)
        {
            fd_set writeSet;

            FD_ZERO(&writeSet);
            FD_SET(socket_, &writeSet);

            int rv = select(socket_ + 1, nullptr, &writeSet, nullptr, nullptr);
            if (rv > 0)
            {
#if defined(WIN32)
                int numWritten = ::send(socket_, buf, length, 0);
#else
                int numWritten = write(socket_, buf, length);
#endif // defined(WIN32)
                if (numWritten > 0)
                {
                    buf += numWritten;
                    length -= numWritten;
                }
                else if (numWritten == 0)
                {
                    log_warn("write failed without error");
                    enqueue_([&]() {
                        disconnectImpl_();
                    });
                    break;
                }
                else if (numWritten < 0)
                {
#if defined(WIN32)
                    log_warn("write failed (errno=%d)", WSAGetLastError());
#else
                    log_warn("write failed (errno=%d)", errno);
#endif // defined(WIN32)
                    enqueue_([&]() {
                        disconnectImpl_();
                    });
                    break;
                }
            }
            else if (rv < 0)
            {
#if defined(WIN32)
                log_warn("write failed (errno=%d)", WSAGetLastError());
#else
                log_warn("write failed (errno=%d)", errno);
#endif // defined(WIN32)
                enqueue_([&]() {
                    disconnectImpl_();
                });
            }
        }
    }
}

void TcpConnectionHandler::receiveImpl_()
{
    constexpr int READ_SIZE_BYTES = 1024;

    char buf[READ_SIZE_BYTES];

    while (socket_ > 0)
    {
        struct timeval tv = {0, 250000}; // 250ms
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socket_, &readSet);

        int rv = select(socket_ + 1, &readSet, nullptr, nullptr, &tv);
        if (rv > 0)
        {
            int numRead = 0;
            int numHaveRead = 0;
#if defined(WIN32)
            while ((numRead = recv(socket_, buf, READ_SIZE_BYTES, 0)) > 0)
#else
            while ((numRead = read(socket_, buf, READ_SIZE_BYTES)) > 0)
#endif // defined(WIN32)
            {
                // Queue RX handler
                numHaveRead += numRead;
                receiveBuffer_.write(buf, numRead);

                if (numRead < READ_SIZE_BYTES)
                {
                    break;
                }
            }
            if (numHaveRead > 0)
            {
                enqueue_([&]() {
                    char tmp[READ_SIZE_BYTES];
                    int toRead = std::min(receiveBuffer_.numUsed(), READ_SIZE_BYTES);
                    while (toRead > 0)
                    {
                        receiveBuffer_.read(tmp, toRead);
                        onReceive_(tmp, toRead);
                        toRead = std::min(receiveBuffer_.numUsed(), READ_SIZE_BYTES);
                    }
                    if (onRecvEndFn_)
                    {
                        onRecvEndFn_();
                    }
                });
            } 
            else if (numRead == 0)
            {
                log_warn("EOF received");
                enqueue_([&]() {
                    disconnectImpl_();
                });
            }
            else if (numRead < 0)
            {
#if defined(WIN32)
                log_warn("read failed (errno=%d)", WSAGetLastError());
#else
                log_warn("read failed (errno=%d)", errno);
#endif // defined(WIN32)
                enqueue_([&]() {
                    disconnectImpl_();
                });
            }
        }
        else if (rv < 0 && socket_ > 0)
        {
#if defined(WIN32)
            log_warn("read failed (errno=%d)", WSAGetLastError());
#else
            log_warn("read failed (errno=%d)", errno);
#endif // defined(WIN32)
            enqueue_([&]() {
                disconnectImpl_();
            });
        }
    }
}

#if defined(WIN32)
void TcpConnectionHandler::checkConnections_(std::vector<SOCKET>& sockets)
#else
void TcpConnectionHandler::checkConnections_(std::vector<int>& sockets)
#endif // defined(WIN32)
{
    fd_set writeSet;
    struct timeval tv = {0, 250000}; // 250ms
    int err = 0;

    FD_ZERO(&writeSet);
#if defined(WIN32)
    SOCKET maxSocket = INVALID_SOCKET;
#else
    int maxSocket = INVALID_SOCKET;
#endif // defined(WIN32)
    for (auto& sock : sockets)
    {
        FD_SET(sock, &writeSet);
        if (sock > maxSocket) maxSocket = sock;
    }
    
    if (select(maxSocket + 1, nullptr, &writeSet, nullptr, &tv) > 0)
    {
        int sockErrCode = 0;
        socklen_t resultLength = sizeof(sockErrCode);
        
        std::vector<int> socketsToDelete;
        for (auto& sock : sockets)
        {
            if (FD_ISSET(sock, &writeSet))
            {
#if defined(WIN32)
                auto sockOptError = getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&sockErrCode, &resultLength);
                if (sockOptError != 0 && WSAGetLastError() != WSAEINPROGRESS)
                {
                    err = WSAGetLastError();
                    goto socket_error;
                }
#else
                auto sockOptError = getsockopt(sock, SOL_SOCKET, SO_ERROR, &sockErrCode, &resultLength);
                if (sockOptError < 0)
                {
                    err = errno;
                    goto socket_error;
                }
                else if (sockErrCode != 0 && sockErrCode != EINPROGRESS)
                {
                    err = sockErrCode;
                    goto socket_error;
                }
#endif // defined(WIN32)
                else if (sockErrCode == 0)
                {
                    // Connection succeeded.
                    socket_ = sock;
                    break;
                }
                
socket_error:
                log_warn("Got socket error %d (%s) while connecting", err, strerror(err));
#if defined(WIN32)
                closesocket(sock);
#else
                close(sock);
#endif // defined(WIN32)
                socketsToDelete.push_back(sock);
            }
        }
        
        for (auto& toDelete : socketsToDelete)
        {
            sockets.erase(std::find(sockets.begin(), sockets.end(), toDelete));
        }
    }
}

void TcpConnectionHandler::resolveAddresses_(int addressFamily, const char* host, const char* port, struct addrinfo** result)
{
    log_info("attempting to resolve %s:%s using family %d", host, port, addressFamily);
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = addressFamily;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
#ifdef WIN32
    hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;
#else
    hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED | AI_NUMERICSERV;
#endif // WIN32
    int err = getaddrinfo(host, port, &hints, result);
    if (err != 0) 
    {
        log_warn("cannot resolve %s:%s using family %d (err=%d)", host, port, addressFamily, err);
    }
    
    log_info("resolution of %s:%s using family %d complete", host, port, addressFamily);
}
