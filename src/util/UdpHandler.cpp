//=========================================================================
// Name:            UdpHandler.cpp
// Purpose:         Handler for UDP sockets.
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

#include "UdpHandler.h"
#include "logging/ulog.h"
#include "../os/os_interface.h"

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif // INVALID_SOCKET

UdpHandler::UdpHandler()
    : ThreadedObject("UdpHandler")
    , socket_(-1)
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

UdpHandler::~UdpHandler()
{
    auto fut = close();
    fut.wait();

#if defined(WIN32)
    WSACleanup();
#endif // defined(WIN32)
}

std::future<void> UdpHandler::open(const char* host, int port)
{
    host_ = host;
    port_ = port;
    
    std::shared_ptr<std::promise<void>> prom = std::make_shared<std::promise<void> >();
    auto fut = prom->get_future();
    
    enqueue_([&, prom]() {
        openImpl_();
        prom->set_value();
    });
    return fut;
}

std::future<void> UdpHandler::close()
{
    std::shared_ptr<std::promise<void>> prom = std::make_shared<std::promise<void> >();
    auto fut = prom->get_future();
    
    enqueue_([&, prom]() {
        closeImpl_();
        prom->set_value();
    });
    return fut;
}

std::future<void> UdpHandler::send(const char* host, int port, const char* buf, int length)
{
    std::shared_ptr<std::promise<void>> prom = std::make_shared<std::promise<void> >();
    auto fut = prom->get_future();
    
    enqueue_([&, prom, host, port, buf, length]() {
        sendImpl_(host, port, buf, length);
        prom->set_value();
    });
    return fut;
}

void UdpHandler::openImpl_()
{
    std::stringstream portStream;
    portStream << port_;
    
    log_info("Opening UDP socket (host %s port %d)", host_.c_str(), port_);
    
    // If empty hostname, use INADDR_ANY
    if (host_ == "")
    {
#if defined(WIN32)
        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == INVALID_SOCKET)
        {
            log_warn("cannot open socket (err=%d)", WSAGetLastError());
        }
#else
        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if(socket_ < 0)
        {
            log_warn("cannot open socket (err=%d)", errno);
        }
#endif // defined(WIN32)

        // XXX - macOS isn't able to set V6ONLY. Forcing IPv4 for now.
#if 0
        // Make sure we can receive IPv4 too
        int opt_false = 0;
#if defined(WIN32)
        auto rv = setsockopt(socket_, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&opt_false, sizeof(opt_false));
        if (rv != 0)
        {
            log_warn("cannot set socket options (err=%d)", WSAGetLastError());
        }
#else
        auto rv = setsockopt(socket_, IPPROTO_IPV6, IPV6_V6ONLY, &opt_false, sizeof(opt_false));
        if (rv != 0)
        {
            log_warn("cannot set socket options (err=%d)", errno);
        }
#endif // defined(WIN32)
        
        if (port_ > 0)
        {
            struct sockaddr_in6 sin6;
            sin6.sin6_family = AF_INET6;
            memcpy(sin6.sin6_addr.s6_addr, in6addr_any.s6_addr, 16);
            sin6.sin6_port = htons(port_);
            sin6.sin6_flowinfo = 0;
            sin6.sin6_scope_id = 0;
            auto rv = ::bind(socket_, (struct sockaddr*)&sin6, sizeof(sin6));
            if (rv != 0)
            {
    #if defined(WIN32)
                log_warn("cannot bind socket (err=%d)", WSAGetLastError());
    #else
                log_warn("cannot bind socket (err=%d)", errno);
    #endif // defined(WIN32)
            }
        }
#endif
    }
    else
    {
        struct addrinfo* result = resolveIpAddress_(host_.c_str(), port_);

        if (result != nullptr)
        {
#if defined(WIN32)
            socket_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
            if (socket_ == INVALID_SOCKET)
            {
                log_warn("cannot open socket (err=%d)", WSAGetLastError());
            }
#else
            socket_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
            if(socket_ < 0)
            {
                log_warn("cannot open socket (err=%d)", errno);
            }
#endif // defined(WIN32)

            auto err = ::bind(socket_, result->ai_addr, result->ai_addrlen);
            if (err != 0)
            {
#if defined(WIN32)
                log_warn("cannot bind socket (err=%d)", WSAGetLastError());
#else
                log_warn("cannot bind socket (err=%d)", errno);
#endif // defined(WIN32)
            }
            else
            {                
                // Start receive thread
                receiveThread_ = std::thread(std::bind(&UdpHandler::receiveImpl_, this));
            }
            
            freeaddrinfo(result);           /* No longer needed */
        }
    }
    
    // TBD - join multicast if required
}

void UdpHandler::closeImpl_()
{
    auto tmp = socket_.load(std::memory_order_acquire);
    if (tmp != INVALID_SOCKET)
    {
        socket_.store(INVALID_SOCKET, std::memory_order_release);

#if defined(WIN32)
        closesocket(tmp);
#else
        ::close(tmp);
#endif // defined(WIN32)

        if (receiveThread_.joinable())
        {
            receiveThread_.join();
        }
    }
}

void UdpHandler::sendImpl_(const char* host, int port, const char* buf, int length)
{
    struct addrinfo* result = resolveIpAddress_(host, port);
    if (result != nullptr)
    {
        auto rv = sendto(socket_, buf, length, 0, result->ai_addr, result->ai_addrlen);
        if (rv < 0)
        {
#if defined(WIN32)
            log_warn("cannot send to host %s port %d (err=%d)", host, port, WSAGetLastError());
#else
            log_warn("cannot send to host %s port %d (err=%d)", host, port, errno);
#endif // defined(WIN32)
        }
        
        freeaddrinfo(result);
    }
}

void UdpHandler::receiveImpl_()
{
    // TBD - not handling RX for now. This class is mainly for WSJT-X style logging, so we 
    // shouldn't *need* to handle anything being received
}

struct addrinfo* UdpHandler::resolveIpAddress_(const char* host, int port)
{
    std::stringstream portStream;
    portStream << port;
    
    struct addrinfo* result = nullptr;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
#ifdef WIN32
    hints.ai_flags = AI_NUMERICHOST;
#else
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;
#endif // WIN32
    int err = getaddrinfo(host, portStream.str().c_str(), &hints, &result);
    if (err != 0) 
    {
        log_warn("cannot resolve %s:%s (err=%d)", host_.c_str(), port_, err);
    }
    
    return result;
}