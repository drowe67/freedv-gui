//=========================================================================
// Name:            SocketIoClient.cpp
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

#include <iostream>
#include <sstream>

#include "SocketIoClient.h"
#include "logging/ulog.h"

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

#define SOCKET_IO_TX_PREFIX "42"
#define DEFAULT_PING_TIMER_INTERVAL_MS (30000)

SocketIoClient::SocketIoClient()
    : pingTimer_(DEFAULT_PING_TIMER_INTERVAL_MS, [&](ThreadedTimer&) {
        log_warn("did not receive ping from server in time");
        disconnect();
    }, true)
{
    // empty
}

SocketIoClient::~SocketIoClient()
{
    // Note: not currently done in the underlying object due to
    // "pure virtual" function exceptions.
    enableReconnect_ = false;
    auto fut = disconnect();
    fut.wait();
}

void SocketIoClient::setAuthDictionary(nlohmann::json authJson)
{
    jsonAuthObj_ = authJson;
}

void SocketIoClient::on(std::string eventName, SioMessageReceivedFn fn)
{
    eventFnMap_[eventName] = fn;
}

void SocketIoClient::emit(std::string eventName, nlohmann::json params)
{
    enqueue_([&, eventName, params]() {
        emitImpl_(eventName, params);
    });
}

void SocketIoClient::emit(std::string eventName)
{
    enqueue_([&, eventName]() {
        emitImpl_(eventName);
    });
}

void SocketIoClient::setOnConnectFn(OnConnectionStateChangeFn fn)
{
    onConnectFn_ = fn;
}

void SocketIoClient::setOnDisconnectFn(OnConnectionStateChangeFn fn)
{
    onDisconnectFn_ = fn;
}

void SocketIoClient::onConnect_()
{
    // websocketpp: initialize logging
    client_.clear_access_channels(websocketpp::log::alevel::all);
    client_.set_access_channels(websocketpp::log::alevel::connect);
    client_.set_access_channels(websocketpp::log::alevel::disconnect);
    client_.set_access_channels(websocketpp::log::alevel::app);
    
    // Register message handler
    client_.set_message_handler(bind(&SocketIoClient::handleWebsocketRequest_, this, &client_, ::_1, ::_2));
    
    // Register open handler
    client_.set_open_handler([&](websocketpp::connection_hdl) {
        std::string namespaceOpen = "40";
        auto tmp = jsonAuthObj_.dump();
        namespaceOpen += tmp;
        
        connection_->send(namespaceOpen);
    });
    
    // Register fail handler
    client_.set_fail_handler([&](websocketpp::connection_hdl) {
        disconnect();
    });
    
    // Register write handler
    client_.set_write_handler([&](websocketpp::connection_hdl, char const *buf, size_t len) {
        send(buf, len);
        return websocketpp::lib::error_code();
    });
    
    // "Start" connection
    websocketpp::lib::error_code ec;
    std::stringstream ss;
    ss << "ws://" << host_ << ":" << port_ << "/socket.io/?EIO=4&transport=websocket";
    connection_ = client_.get_connection(ss.str(), ec);
    client_.connect(connection_);
}

void SocketIoClient::onDisconnect_()
{
    connection_->eof();
    pingTimer_.stop();
    
    if (onDisconnectFn_)
    {
        onDisconnectFn_();
    }
}

void SocketIoClient::onReceive_(char* buf, int length)
{
    connection_->read_some(buf, length);
}

void SocketIoClient::emitImpl_(std::string eventName, nlohmann::json params)
{
    nlohmann::json msgEmit = {eventName, params};
    std::string msgToSend = SOCKET_IO_TX_PREFIX + msgEmit.dump();
    connection_->send(msgToSend);
}

void SocketIoClient::emitImpl_(std::string eventName)
{
    nlohmann::json msgEmit = {eventName};
    std::string msgToSend = SOCKET_IO_TX_PREFIX + msgEmit.dump();
    connection_->send(msgToSend);
}

void SocketIoClient::handleWebsocketRequest_(WebSocketClient* s, websocketpp::connection_hdl hdl, message_ptr msg)
{
    if (msg->get_opcode() == websocketpp::frame::opcode::text)
    {
        std::string body = msg->get_payload();
        handleEngineIoMessage_((char*)body.c_str(), body.size());
    }
}

void SocketIoClient::handleEngineIoMessage_(char* ptr, int length)
{
    log_debug("got engine.io message %c of length %d", ptr[0], length);

    switch(ptr[0])
    {
        case '0':
        {
            log_info("engine.io open");

            // "open" -- ready to receive socket.io messages
            // Grab ping timings and start ping timer
            nlohmann::json sessionInfo = nlohmann::json::parse(ptr + 1);
            pingTimer_.setTimeout(
                sessionInfo["pingInterval"].template get<int>() + 
                sessionInfo["pingTimeout"].template get<int>());
            pingTimer_.start();
            
            break;
        }
        case '1':
        {
            log_info("engine.io close");

            // "close" -- we're being closed.
            disconnect();
            break;
        }
        case '2':
        {
            //log_info("engine.io ping");

            // "ping" -- send pong
            connection_->send("3");
            pingTimer_.stop();
            pingTimer_.start();
            break;
        }
        case '4':
        {
            // "message" -- process socket.io message
            handleSocketIoMessage_(ptr + 1, length - 1);
            break;
        }
        default:
            // ignore all others as they're related to transport upgrades, but if we got
            // something invalid, we should treat it as a disconnection.
            if (!isdigit(ptr[0]))
            {
                log_warn("invalid data received from engine.io -- reconnecting");

                // "close" -- we're being closed
                disconnect();
            }
            break;
    }
}

void SocketIoClient::handleSocketIoMessage_(char* ptr, int length)
{
    switch(ptr[0])
    {
        case '0':
        {
            log_info("socket.io connect");

            // connection successful
            if (onConnectFn_)
            {
                onConnectFn_();
            }
            
            break;
        }
        case '2':
        {
            // event received from server
            nlohmann::json parsedEvent = nlohmann::json::parse(ptr + 1);
            std::string eventName = parsedEvent[0];
            if (eventFnMap_[eventName])
            {
                if (parsedEvent.size() > 1)
                {
                    (eventFnMap_[eventName])(parsedEvent[1]);
                }
                else   
                {
                    (eventFnMap_[eventName])({});
                }
            }
            break;
        }
        case '4':
        {
            // error connecting to namespace, close connection and retry
            disconnect();
            break;
        }
        default:
        {
            // current server doesn't use anything else, so ignore.
            break;
        }
    }
}
