//=========================================================================
// Name:            TcpConnectionHandler.h
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

#ifndef TCP_CONNECTION_HANDLER_H
#define TCP_CONNECTION_HANDLER_H

#include "ThreadedObject.h"
#include "ThreadedTimer.h"
#include "GenericFIFO.h"

#include <vector>
#include <future>
#include <atomic>

class TcpConnectionHandler : public ThreadedObject
{
public:
    using OnRecvEndFn = std::function<void()>;

    TcpConnectionHandler();
    virtual ~TcpConnectionHandler();
    
    std::future<void> connect(const char* host, int port, bool enableReconnect);
    std::future<void> disconnect();
    
    std::future<void> send(const char* buf, int length);

    void setOnRecvEndFn(OnRecvEndFn fn);
    
protected:
    std::string host_;
    int port_;
    bool enableReconnect_;
    
    virtual void onConnect_() = 0;
    virtual void onDisconnect_() = 0;
    virtual void onReceive_(char* buf, int length) = 0;
    
private:
    std::thread receiveThread_;
    ThreadedTimer reconnectTimer_;
    std::atomic<int> socket_;
    std::atomic<bool> ipv4Complete_;
    std::atomic<bool> ipv6Complete_;
    std::atomic<bool> cancelConnect_;
    GenericFIFO<char> receiveBuffer_;
    OnRecvEndFn onRecvEndFn_;

    void connectImpl_();
    void disconnectImpl_();
    void sendImpl_(const char* buf, int length);
    void receiveImpl_();
    
    void resolveAddresses_(int addressFamily, const char* host, const char* port, struct addrinfo** result);
    void checkConnections_(std::vector<int>& sockets);
};

#endif // TCP_CONNECTION_HANDLER_H
