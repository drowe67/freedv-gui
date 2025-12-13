//=========================================================================
// Name:            UdpHandler.h
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

#ifndef UDP_HANDLER_H
#define UDP_HANDLER_H

#include "ThreadedObject.h"
#include "ThreadedTimer.h"

#include <vector>
#include <future>
#include <atomic>

class UdpHandler : public ThreadedObject
{
public:
    UdpHandler();
    virtual ~UdpHandler();
    
    // Note: host is an IP here, not hostname.
    std::future<void> open(const char* host, int port);
    std::future<void> close();
    std::future<void> send(const char* host, int port, const char* buf, int length);
    
protected:
    std::string host_;
    int port_;
    
    virtual void onReceive_(const char* host, int port, char* buf, int length) = 0;
    
private:
    std::thread receiveThread_;
#if defined(WIN32)
    std::atomic<SOCKET> socket_;
#else
    std::atomic<int> socket_;
#endif // defined(WIN32)

    void openImpl_();
    void closeImpl_();
    void sendImpl_(const char* host, int port, const char* buf, int length);
    void receiveImpl_();
    
    struct addrinfo* resolveIpAddress_(const char* host, int port);
};

#endif // UDP_HANDLER_H
