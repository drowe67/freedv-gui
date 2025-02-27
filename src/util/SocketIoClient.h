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
#include <websocketpp/config/core_client.hpp>
#include <websocketpp/client.hpp>

#include "TcpConnectionHandler.h"
#include "json.hpp"

class SocketIoClient : public TcpConnectionHandler
{
public:
    using SioMessageReceivedFn = std::function<void(nlohmann::json)>;
    using OnConnectionStateChangeFn = std::function<void()>;
    
    SocketIoClient();
    virtual ~SocketIoClient();
    
    void setAuthDictionary(nlohmann::json authJson);
    void on(std::string eventName, SioMessageReceivedFn fn);
    void emit(std::string eventName, nlohmann::json params);
    
    void setOnConnectFn(OnConnectionStateChangeFn fn);
    void setOnDisconnectFn(OnConnectionStateChangeFn fn);
    
protected:
    virtual void onConnect_() override;
    virtual void onDisconnect_() override;
    virtual void onReceive_(char* buf, int length) override;
    
private:
    using WebSocketClient = websocketpp::client<websocketpp::config::core_client>;
    using message_ptr = WebSocketClient::message_ptr;
    
    nlohmann::json jsonAuthObj_;
    WebSocketClient client_;
    WebSocketClient::connection_ptr connection_;
    std::map<std::string, SioMessageReceivedFn> eventFnMap_;
    OnConnectionStateChangeFn onConnectFn_;
    OnConnectionStateChangeFn onDisconnectFn_;
    
    void emitImpl_(std::string eventName, nlohmann::json params);
    
    void handleWebsocketRequest_(WebSocketClient* s, websocketpp::connection_hdl hdl, message_ptr msg);
    void handleSocketIoMessage_(char* ptr, int length);
    void handleEngineIoMessage_(char* ptr, int length);
};

#endif // SOCKET_IO_CLIENT_H