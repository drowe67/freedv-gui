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
    enableReconnect_.store(false, std::memory_order_release);
    auto fut = disconnect();
    fut.wait();
}

void SocketIoClient::setAuthDictionary(yyjson_mut_doc* authJson)
{
    auto tmp = yyjson_mut_write(authJson, 0, nullptr);
    authObj_ = tmp;
    free(tmp);
    yyjson_mut_doc_free(authJson);
}

void SocketIoClient::on(std::string const& eventName, SioMessageReceivedFn fn)
{
    eventFnMap_[eventName] = std::move(fn);
}

void SocketIoClient::emit(std::string const& eventName, yyjson_mut_val* params)
{
    yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val* eventArray = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, eventArray);
    yyjson_mut_arr_add_str(doc, eventArray, eventName.c_str());
    yyjson_mut_arr_append(eventArray, params);

    auto outJson = yyjson_mut_write(doc, 0, nullptr);
    std::string msgToSend = std::string(SOCKET_IO_TX_PREFIX) + outJson;
    free(outJson);
    yyjson_mut_doc_free(doc);

    enqueue_([this, msgToSend]() {
        if (connection_)
        {
            connection_->send(msgToSend);
        }
    });
}

void SocketIoClient::emit(std::string const& eventName)
{
    yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val* eventArray = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, eventArray);
    yyjson_mut_arr_add_str(doc, eventArray, eventName.c_str());

    auto outJson = yyjson_mut_write(doc, 0, nullptr);
    std::string msgToSend = std::string(SOCKET_IO_TX_PREFIX) + outJson;
    free(outJson);
    yyjson_mut_doc_free(doc);

    enqueue_([this, msgToSend]() {
        if (connection_)
        {
            connection_->send(msgToSend);
        }
    });
}

void SocketIoClient::setOnConnectFn(OnConnectionStateChangeFn fn)
{
    onConnectFn_ = std::move(fn);
}

void SocketIoClient::setOnDisconnectFn(OnConnectionStateChangeFn fn)
{
    onDisconnectFn_ = std::move(fn);
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
    client_.set_open_handler([&](websocketpp::connection_hdl const&) {
        std::string namespaceOpen = "40";
        namespaceOpen += authObj_;
        
        connection_->send(namespaceOpen);
    });
    
    // Register fail handler
    client_.set_fail_handler([&](websocketpp::connection_hdl const&) {
        disconnect();
    });
    
    // Register write handler
    client_.set_write_handler([&](websocketpp::connection_hdl const&, char const *buf, size_t len) {
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
    if (connection_)
    {
        connection_->eof();
    }

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

void SocketIoClient::handleWebsocketRequest_(WebSocketClient*, websocketpp::connection_hdl const&, message_ptr const& msg)
{
    if (msg->get_opcode() == websocketpp::frame::opcode::text)
    {
        std::string body = msg->get_payload();
        handleEngineIoMessage_((char*)body.c_str(), body.size());
    }
}

void SocketIoClient::handleEngineIoMessage_(char* ptr, int length)
{
    switch(ptr[0])
    {
        case '0':
        {
            log_info("engine.io open");

            // "open" -- ready to receive socket.io messages
            // Grab ping timings and start ping timer
            yyjson_doc* sessionInfo = yyjson_read(ptr + 1, strlen(ptr + 1), 0);

            yyjson_val* rootVal = yyjson_doc_get_root(sessionInfo);
            auto pingInterval = yyjson_obj_get(rootVal, "pingInterval");
            auto pingTimeout = yyjson_obj_get(rootVal, "pingTimeout");

            pingTimer_.setTimeout(
                yyjson_get_int(pingInterval) +
                yyjson_get_int(pingTimeout));
            pingTimer_.start();

            yyjson_doc_free(sessionInfo);
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
            pingTimer_.restart();
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

void SocketIoClient::fireEvent(std::string const& eventName, yyjson_val* params)
{
    if (eventFnMap_[eventName])
    {
        (eventFnMap_[eventName])(params); // eventArgs is nullptr if not provided
    }
}

void SocketIoClient::handleSocketIoMessage_(char* ptr, int)
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
            yyjson_doc* eventDoc = yyjson_read(ptr + 1, strlen(ptr + 1), 0);
            yyjson_val* eventRoot = yyjson_doc_get_root(eventDoc);
            yyjson_val* eventName = yyjson_arr_get(eventRoot, 0);
            yyjson_val* eventArgs = yyjson_arr_get(eventRoot, 1);

            std::string eventNameStr = yyjson_get_str(eventName);
            fireEvent(eventNameStr, eventArgs);
            yyjson_doc_free(eventDoc);
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
