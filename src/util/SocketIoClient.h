//=========================================================================
// Name:            SocketIoClient.h
// Purpose:         Handler for socket.io connections.
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

#ifndef SOCKET_IO_CLIENT_H
#define SOCKET_IO_CLIENT_H

#include <map>
#include "websocketpp_config.h"
#include <websocketpp/client.hpp>

#include "TcpConnectionHandler.h"
#include "yyjson.h"

class SocketIoClient : public TcpConnectionHandler
{
public:
    using SioMessageReceivedFn = std::function<void(yyjson_val*)>;
    using OnConnectionStateChangeFn = std::function<void()>;
    
    SocketIoClient();
    virtual ~SocketIoClient();
    
    void setAuthDictionary(yyjson_mut_doc* authJson);
    void on(std::string eventName, SioMessageReceivedFn fn);
    void emit(std::string eventName, yyjson_mut_val* params);
    void emit(std::string eventName);
    
    void setOnConnectFn(OnConnectionStateChangeFn fn);
    void setOnDisconnectFn(OnConnectionStateChangeFn fn);
    
protected:
    virtual void onConnect_() override;
    virtual void onDisconnect_() override;
    virtual void onReceive_(char* buf, int length) override;
    
private:
    using WebSocketClient = websocketpp::client<websocketpp::config::custom_config>;
    using message_ptr = WebSocketClient::message_ptr;
    
    std::string authObj_;
    WebSocketClient client_;
    WebSocketClient::connection_ptr connection_;
    std::map<std::string, SioMessageReceivedFn> eventFnMap_;
    OnConnectionStateChangeFn onConnectFn_;
    OnConnectionStateChangeFn onDisconnectFn_;
    ThreadedTimer pingTimer_;
    
    void handleWebsocketRequest_(WebSocketClient* s, websocketpp::connection_hdl hdl, message_ptr msg);
    void handleSocketIoMessage_(char* ptr, int length);
    void handleEngineIoMessage_(char* ptr, int length);
};

#endif // SOCKET_IO_CLIENT_H
