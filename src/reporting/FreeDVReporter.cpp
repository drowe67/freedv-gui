//=========================================================================
// Name:            FreeDVReporter.h
// Purpose:         Implementation of interface to freedv-reporter
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

#include "FreeDVReporter.h"
#include "../util/SocketIoClient.h"
#include "../os/os_interface.h"

#if defined(__APPLE__)
#include <pthread.h>
#endif // defined(__APPLE__)

using namespace std::chrono_literals;
using namespace std::placeholders;

FreeDVReporter::FreeDVReporter(std::string hostname, std::string callsign, std::string gridSquare, std::string software, bool rxOnly, bool writeOnly)
    : isConnecting_(false)
    , hostname_(std::move(hostname))
    , callsign_(std::move(callsign))
    , gridSquare_(std::move(gridSquare))
    , software_(std::move(software))
    , lastFrequency_(0)
    , tx_(false)
    , rxOnly_(rxOnly)
    , hidden_(false)
    , writeOnly_(writeOnly)
{
    if (hostname_ == "")
    {
        // Default to the main FreeDV Reporter server if the hostname
        // isn't provided.
        hostname_ = FREEDV_REPORTER_DEFAULT_HOSTNAME;
    }

    sioClient_ = new SocketIoClient();
    assert(sioClient_ != nullptr);

    isFullyConnected_.store(false, std::memory_order_release);
}

FreeDVReporter::~FreeDVReporter()
{
    // Make sure all event handlers are nulled out before terminating
    // the connection just in case stuff tries to get called as we're 
    // destroying.
    {
        std::unique_lock<std::mutex> lk(objMutex_);
        
        onReporterConnectFn_ = nullptr;
        onReporterDisconnectFn_ = nullptr;
    
        onUserConnectFn_ = nullptr;
        onUserDisconnectFn_ = nullptr;
        onFrequencyChangeFn_ = nullptr;
        onTransmitUpdateFn_ = nullptr;
        onReceiveUpdateFn_ = nullptr;
    
        onQsyRequestFn_ = nullptr;
        onMessageUpdateFn_ = nullptr;
        onConnectionSuccessfulFn_ = nullptr;
        onAboutToShowSelfFn_ = nullptr;
    
        onRecvEndFn_ = nullptr;
    }
    
    delete sioClient_;
}

void FreeDVReporter::connect()
{
    connect_();
}

void FreeDVReporter::requestQSY(std::string const& sid, uint64_t frequencyHz, std::string const& message)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    
    if (isValidForReporting())
    {
        yyjson_mut_doc* qsyDoc = yyjson_mut_doc_new(nullptr);
        yyjson_mut_val* qsyObj = yyjson_mut_obj(qsyDoc);

        yyjson_mut_obj_add_str(qsyDoc, qsyObj, "dest_sid", sid.c_str());
        yyjson_mut_obj_add_str(qsyDoc, qsyObj, "message", message.c_str());
        yyjson_mut_obj_add_uint(qsyDoc, qsyObj, "frequency", frequencyHz);

        sioClient_->emit("qsy_request", qsyObj);
        yyjson_mut_doc_free(qsyDoc);
    }
}

void FreeDVReporter::updateMessage(std::string const& message)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    
    if (isValidForReporting())
    {
        sendMessageImpl_(message);
    }
}

void FreeDVReporter::inAnalogMode(bool inAnalog)
{
    if (isValidForReporting())
    {
        if (inAnalog)
        {
            hideFromView();
        }
        else
        {
            showOurselves();
        }
    }
}

void FreeDVReporter::freqChange(uint64_t frequency)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    
    if (isValidForReporting())
    {
        freqChangeImpl_(frequency);
    }
}

void FreeDVReporter::transmit(std::string mode, bool tx)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    
    if (isValidForReporting())
    {
        transmitImpl_(mode, tx);
    }
}

void FreeDVReporter::hideFromView()
{
    std::unique_lock<std::mutex> lk(objMutex_);
    
    if (isValidForReporting())
    {
        hideFromViewImpl_();
    }
}

void FreeDVReporter::showOurselves()
{
    std::unique_lock<std::mutex> lk(objMutex_);
    
    if (isValidForReporting())
    {
        showOurselvesImpl_();
    }
}
    
void FreeDVReporter::addReceiveRecord(std::string callsign, std::string mode, uint64_t, signed char snr)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    
    if (isValidForReporting() && isFullyConnected_.load(std::memory_order_acquire))
    {
        yyjson_mut_doc* rxDoc = yyjson_mut_doc_new(nullptr);
        yyjson_mut_val* rxData = yyjson_mut_obj(rxDoc);

        yyjson_mut_obj_add_str(rxDoc, rxData, "callsign", callsign.c_str());
        yyjson_mut_obj_add_str(rxDoc, rxData, "mode", mode.c_str());
        yyjson_mut_obj_add_int(rxDoc, rxData, "snr", (int)snr);

        sioClient_->emit("rx_report", rxData);
        yyjson_mut_doc_free(rxDoc);
    }
}

void FreeDVReporter::send()
{
    // No implementation needed, we send RX/TX reports live.
}

void FreeDVReporter::setOnReporterConnectFn(ReporterConnectionFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onReporterConnectFn_ = std::move(fn);
}

void FreeDVReporter::setOnReporterDisconnectFn(ReporterConnectionFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onReporterDisconnectFn_ = std::move(fn);
}

void FreeDVReporter::setOnUserConnectFn(ConnectionDataFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onUserConnectFn_ = std::move(fn);
}

void FreeDVReporter::setOnUserDisconnectFn(ConnectionDataFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onUserDisconnectFn_ = std::move(fn);
}

void FreeDVReporter::setOnFrequencyChangeFn(FrequencyChangeFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onFrequencyChangeFn_ = std::move(fn);
}

void FreeDVReporter::setOnTransmitUpdateFn(TxUpdateFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onTransmitUpdateFn_ = std::move(fn);
}

void FreeDVReporter::setOnReceiveUpdateFn(RxUpdateFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onReceiveUpdateFn_ = std::move(fn);
}

void FreeDVReporter::setOnQSYRequestFn(QsyRequestFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onQsyRequestFn_ = std::move(fn);
}

void FreeDVReporter::setMessageUpdateFn(MessageUpdateFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onMessageUpdateFn_ = std::move(fn);
}

void FreeDVReporter::setConnectionSuccessfulFn(ConnectionSuccessfulFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onConnectionSuccessfulFn_ = std::move(fn);
}

void FreeDVReporter::setAboutToShowSelfFn(AboutToShowSelfFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onAboutToShowSelfFn_ = std::move(fn);
}

void FreeDVReporter::setRecvEndFn(RecvEndFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onRecvEndFn_ = std::move(fn);
}

bool FreeDVReporter::isValidForReporting()
{
    return callsign_ != "" && gridSquare_ != "";
}

void FreeDVReporter::connect_()
{        
    // Connect and send initial info.
    yyjson_mut_doc* authDoc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val* authData = yyjson_mut_obj(authDoc);
    yyjson_mut_doc_set_root(authDoc, authData);

    auto osString = GetOperatingSystemString();
    if (!isValidForReporting())
    {
        yyjson_mut_obj_add_str(authDoc, authData, "role", "view");
    }
    else
    {
        yyjson_mut_obj_add_str(authDoc, authData, "role", writeOnly_ ? "report_wo" : "report");
        yyjson_mut_obj_add_str(authDoc, authData, "callsign", callsign_.c_str());
        yyjson_mut_obj_add_str(authDoc, authData, "grid_square", gridSquare_.c_str());
        yyjson_mut_obj_add_str(authDoc, authData, "version", software_.c_str());
        yyjson_mut_obj_add_bool(authDoc, authData, "rx_only", rxOnly_);
        yyjson_mut_obj_add_str(authDoc, authData, "os", osString.c_str());
    }

    yyjson_mut_obj_add_int(authDoc, authData, "protocol_version", FREEDV_REPORTER_PROTOCOL_VERSION);
    
    sioClient_->setOnRecvEndFn([&]() {
        std::unique_lock<std::mutex> lk(objMutex_);
        if (onRecvEndFn_)
        {
            onRecvEndFn_();
        }
    });
    
    // Reconnect listener should re-report frequency so that "unknown"
    // doesn't appear.
    sioClient_->setOnConnectFn([&]()
    {
        std::unique_lock<std::mutex> lk(objMutex_);
        isConnecting_ = false;
    });
    
    sioClient_->setOnDisconnectFn([&]() {
        std::unique_lock<std::mutex> lk(objMutex_);
        isConnecting_ = false;
        isFullyConnected_.store(false, std::memory_order_release);
        
        if (onReporterDisconnectFn_)
        {
            onReporterDisconnectFn_();
        }
    });
    
    isConnecting_ = true;
    isFullyConnected_.store(false, std::memory_order_release);
    
    std::stringstream ss;
    ss << hostname_;
    
    std::string host;
    std::string portStr;
    int port = 80;
    std::getline(ss, host, ':');
    std::getline(ss, portStr, ':');
    if (portStr != "")
    {
        port = atoi(portStr.c_str());
    }
    sioClient_->setAuthDictionary(authDoc); // frees authDoc
    sioClient_->connect(host.c_str(), port, true);
    
    if (onReporterConnectFn_)
    {
        onReporterConnectFn_();
    }
    
    sioClient_->on("new_connection", std::bind(&FreeDVReporter::onFreeDVReporterNewConnection_, this, _1));
    sioClient_->on("connection_successful", std::bind(&FreeDVReporter::onFreeDVReporterConnectionSuccessful_, this, _1));
    sioClient_->on("remove_connection", std::bind(&FreeDVReporter::onFreeDVReporterRemoveConnection_, this, _1));
    sioClient_->on("tx_report", std::bind(&FreeDVReporter::onFreeDVReporterTransmitReport_, this, _1));
    sioClient_->on("rx_report", std::bind(&FreeDVReporter::onFreeDVReporterReceiveReport_, this, _1));
    sioClient_->on("freq_change", std::bind(&FreeDVReporter::onFreeDVReporterFrequencyChange_, this, _1));
    sioClient_->on("message_update", std::bind(&FreeDVReporter::onFreeDVReporterMessageUpdate_, this, _1));
    sioClient_->on("qsy_request", std::bind(&FreeDVReporter::onFreeDVReporterQsyRequest_, this, _1));
    sioClient_->on("bulk_update", std::bind(&FreeDVReporter::onFreeDVReporterBulkUpdate_, this, _1));
}

void FreeDVReporter::onFreeDVReporterNewConnection_(yyjson_val* msgParams)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    if (onUserConnectFn_)
    {
        auto sidJson = yyjson_obj_get(msgParams, "sid");
        auto lastUpdateJson = yyjson_obj_get(msgParams, "last_update");
        auto callsignJson = yyjson_obj_get(msgParams, "callsign");
        auto gridSquareJson = yyjson_obj_get(msgParams, "grid_square");
        auto versionJson = yyjson_obj_get(msgParams, "version");
        auto rxOnlyJson = yyjson_obj_get(msgParams, "rx_only");
        auto connTimeJson = yyjson_obj_get(msgParams, "connect_time");
    
        // Only call event handler if we received the correct data types
        // for the items in the message.
        if (yyjson_is_str(sidJson) &&
            yyjson_is_str(lastUpdateJson) &&
            yyjson_is_str(callsignJson) &&
            yyjson_is_str(gridSquareJson) &&
            yyjson_is_str(versionJson) &&
            yyjson_is_bool(rxOnlyJson) &&
            yyjson_is_str(connTimeJson))
        {
            onUserConnectFn_(
                yyjson_get_str(sidJson),
                yyjson_get_str(lastUpdateJson),
                yyjson_get_str(callsignJson),
                yyjson_get_str(gridSquareJson),
                yyjson_get_str(versionJson),
                yyjson_get_bool(rxOnlyJson),
                yyjson_get_str(connTimeJson)
            );
        }
    }
}

void FreeDVReporter::onFreeDVReporterConnectionSuccessful_(yyjson_val* msgParams)
{
    (void)msgParams;
    
    std::unique_lock<std::mutex> lk(objMutex_);
    isFullyConnected_.store(true, std::memory_order_release);

    if (onConnectionSuccessfulFn_)
    {
        onConnectionSuccessfulFn_();
    }

    // Send initial data now that we're fully connected to the server.
    // This was originally done right on socket.io connect, but on some
    // machines this caused the built-in FreeDV Reporter client to be
    // unhappy. 	
    if (hidden_)
    {
        hideFromViewImpl_();
    }
    else
    {
        freqChangeImpl_(lastFrequency_);
        transmitImpl_(mode_, tx_);
        sendMessageImpl_(message_);
    }
}

void FreeDVReporter::onFreeDVReporterRemoveConnection_(yyjson_val* msgParams)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    if (onUserDisconnectFn_)
    {
        auto sidJson = yyjson_obj_get(msgParams, "sid");
        auto lastUpdateJson = yyjson_obj_get(msgParams, "last_update");
        auto callsignJson = yyjson_obj_get(msgParams, "callsign");
        auto gridSquareJson = yyjson_obj_get(msgParams, "grid_square");
        auto versionJson = yyjson_obj_get(msgParams, "version");
        auto rxOnlyJson = yyjson_obj_get(msgParams, "rx_only");
    
        // Only call event handler if we received the correct data types
        // for the items in the message.
        if (yyjson_is_str(sidJson) &&
            yyjson_is_str(lastUpdateJson) &&
            yyjson_is_str(callsignJson) &&
            yyjson_is_str(gridSquareJson) &&
            yyjson_is_str(versionJson) &&
            yyjson_is_bool(rxOnlyJson))
        {
            onUserDisconnectFn_(
                yyjson_get_str(sidJson),
                yyjson_get_str(lastUpdateJson),
                yyjson_get_str(callsignJson),
                yyjson_get_str(gridSquareJson),
                yyjson_get_str(versionJson),
                yyjson_get_bool(rxOnlyJson),
                "" // connect time isn't relevant if disconnecting
            );
        }
    }
}

void FreeDVReporter::onFreeDVReporterTransmitReport_(yyjson_val* msgParams)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    if (onTransmitUpdateFn_)
    {
        auto sidJson = yyjson_obj_get(msgParams, "sid");
        auto lastUpdateJson = yyjson_obj_get(msgParams, "last_update");
        auto callsignJson = yyjson_obj_get(msgParams, "callsign");
        auto gridSquareJson = yyjson_obj_get(msgParams, "grid_square");
        auto lastTxJson = yyjson_obj_get(msgParams, "last_tx");
        auto modeJson = yyjson_obj_get(msgParams, "mode");
        auto transmittingJson = yyjson_obj_get(msgParams, "transmitting");
    
        // Only call event handler if we received the correct data types
        // for the items in the message.
        if (yyjson_is_str(sidJson) &&
            yyjson_is_str(lastUpdateJson) &&
            yyjson_is_str(callsignJson) &&
            yyjson_is_str(gridSquareJson) &&
            yyjson_is_str(modeJson) &&
            yyjson_is_bool(transmittingJson))
        {
            onTransmitUpdateFn_(
                yyjson_get_str(sidJson),
                yyjson_get_str(lastUpdateJson),
                yyjson_get_str(callsignJson),
                yyjson_get_str(gridSquareJson),
                yyjson_get_str(modeJson),
                yyjson_get_bool(transmittingJson),
                yyjson_is_null(lastTxJson) ? "" : yyjson_get_str(lastTxJson)
            );
        }
    }
}

void FreeDVReporter::onFreeDVReporterReceiveReport_(yyjson_val* msgParams)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    auto sid = yyjson_obj_get(msgParams, "sid");
    auto lastUpdate = yyjson_obj_get(msgParams, "last_update");
    auto receiverCallsign = yyjson_obj_get(msgParams, "receiver_callsign");
    auto receiverGridSquare = yyjson_obj_get(msgParams, "receiver_grid_square");
    auto callsign = yyjson_obj_get(msgParams, "callsign");
    auto snr = yyjson_obj_get(msgParams, "snr");
    auto mode = yyjson_obj_get(msgParams, "mode");

    if (onReceiveUpdateFn_)
    {
        bool snrInteger = yyjson_is_int(snr);
        bool snrFloat = yyjson_is_real(snr);
        bool snrValid = snrInteger || snrFloat;

        float snrVal = 0;
        if (snrInteger)
        {
            snrVal = yyjson_get_int(snr);
        }
        else if (snrFloat)
        {
            snrVal = yyjson_get_real(snr);
        }

        // Only call event handler if we received the correct data types
        // for the items in the message.
        if (yyjson_is_str(sid) &&
            yyjson_is_str(lastUpdate) &&
            yyjson_is_str(callsign) &&
            yyjson_is_str(receiverCallsign) &&
            yyjson_is_str(receiverGridSquare) &&
            yyjson_is_str(mode) &&
            snrValid)
        {
            onReceiveUpdateFn_(
                yyjson_get_str(sid),
                yyjson_get_str(lastUpdate),
                yyjson_get_str(receiverCallsign),
                yyjson_get_str(receiverGridSquare),
                yyjson_get_str(callsign),
                snrVal,
                yyjson_get_str(mode)
            );
        }
    }
}

void FreeDVReporter::onFreeDVReporterFrequencyChange_(yyjson_val* msgParams)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    if (onFrequencyChangeFn_)
    {
        auto sid = yyjson_obj_get(msgParams, "sid");
        auto lastUpdate = yyjson_obj_get(msgParams, "last_update");
        auto callsign = yyjson_obj_get(msgParams, "callsign");
        auto gridSquare = yyjson_obj_get(msgParams, "grid_square");
        auto frequency = yyjson_obj_get(msgParams, "freq");
    
        // Only call event handler if we received the correct data types
        // for the items in the message.
        if (yyjson_is_str(sid) &&
            yyjson_is_str(lastUpdate) &&
            yyjson_is_str(callsign) &&
            yyjson_is_str(gridSquare) &&
            yyjson_is_uint(frequency))
        {
            onFrequencyChangeFn_(
                yyjson_get_str(sid),
                yyjson_get_str(lastUpdate),
                yyjson_get_str(callsign),
                yyjson_get_str(gridSquare),
                yyjson_get_uint(frequency)
            );
        }
    }
}

void FreeDVReporter::onFreeDVReporterMessageUpdate_(yyjson_val* msgParams)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    if (onMessageUpdateFn_)
    {
        auto sid = yyjson_obj_get(msgParams, "sid");
        auto lastUpdate = yyjson_obj_get(msgParams, "last_update");
        auto message = yyjson_obj_get(msgParams, "message");

        // Only call event handler if we received the correct data types
        // for the items in the message.
        if (yyjson_is_str(sid) &&
            yyjson_is_str(lastUpdate) &&
            yyjson_is_str(message))
        {
            onMessageUpdateFn_(
                yyjson_get_str(sid),
                yyjson_get_str(lastUpdate),
                yyjson_get_str(message)
            );
        }
    }
}

void FreeDVReporter::onFreeDVReporterQsyRequest_(yyjson_val* msgParams)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    auto callsign = yyjson_obj_get(msgParams, "callsign");
    auto frequency = yyjson_obj_get(msgParams, "frequency");
    auto message = yyjson_obj_get(msgParams, "message");

    if (onQsyRequestFn_)
    {
        // Only call event handler if we received the correct data types
        // for the items in the message.
        if (yyjson_is_str(callsign) &&
            yyjson_is_uint(frequency) &&
            yyjson_is_str(message))
        {
            onQsyRequestFn_(
                yyjson_get_str(callsign),
                yyjson_get_uint(frequency),
                yyjson_get_str(message)
            );
        }
    }
}

void FreeDVReporter::onFreeDVReporterBulkUpdate_(yyjson_val* msgParams)
{
    // Note: don't lock here as the inner event handlers will
    size_t idx, maxIdx;
    yyjson_val* msg;
    yyjson_arr_foreach(msgParams, idx, maxIdx, msg) {
        (void)idx;
        (void)maxIdx;
        
        auto eventName = yyjson_arr_get(msg, 0);
        auto eventArgs = yyjson_arr_get(msg, 1);
        
        if (yyjson_is_str(eventName))
        {
            std::string eventNameStr = yyjson_get_str(eventName);
            sioClient_->fireEvent(eventNameStr, eventArgs);
        }
    }
}

void FreeDVReporter::freqChangeImpl_(uint64_t frequency)
{
    if (isFullyConnected_.load(std::memory_order_acquire))
    {
        yyjson_mut_doc* freqDoc = yyjson_mut_doc_new(nullptr);
        yyjson_mut_val* freqData = yyjson_mut_obj(freqDoc);

        yyjson_mut_obj_add_uint(freqDoc, freqData, "freq", frequency);
        sioClient_->emit("freq_change", freqData);

        yyjson_mut_doc_free(freqDoc);
    }
    
    // Save last frequency in case we need to reconnect.
    lastFrequency_ = frequency;
}

void FreeDVReporter::transmitImpl_(std::string const& mode, bool tx)
{
    if (isFullyConnected_.load(std::memory_order_acquire))
    {
        yyjson_mut_doc* txDoc = yyjson_mut_doc_new(nullptr);
        yyjson_mut_val* txData = yyjson_mut_obj(txDoc);

        yyjson_mut_obj_add_str(txDoc, txData, "mode", mode.c_str());
        yyjson_mut_obj_add_bool(txDoc, txData, "transmitting", tx);
        sioClient_->emit("tx_report", txData);
        yyjson_mut_doc_free(txDoc);
    }
    
    // Save last mode and TX state in case we have to reconnect.
    mode_ = mode;
    tx_ = tx;
}

void FreeDVReporter::sendMessageImpl_(std::string const& message)
{
    if (isFullyConnected_.load(std::memory_order_acquire))
    {
        yyjson_mut_doc* txDoc = yyjson_mut_doc_new(nullptr);
        yyjson_mut_val* txData = yyjson_mut_obj(txDoc);

        yyjson_mut_obj_add_str(txDoc, txData, "message", message.c_str());
        sioClient_->emit("message_update", txData);
        yyjson_mut_doc_free(txDoc);
    }
    
    // Save last mode and TX state in case we have to reconnect.
    message_ = message;
}

void FreeDVReporter::hideFromViewImpl_()
{
    hidden_ = true;
    if (isFullyConnected_.load(std::memory_order_acquire))
    {
        sioClient_->emit("hide_self");
    }
}

void FreeDVReporter::showOurselvesImpl_()
{
    if (onAboutToShowSelfFn_)
    {
        onAboutToShowSelfFn_();
    }
    
    hidden_ = false;
    if (isFullyConnected_.load(std::memory_order_acquire))
    {
        sioClient_->emit("show_self");
    }
    
    freqChangeImpl_(lastFrequency_);
    transmitImpl_(mode_, tx_);
    sendMessageImpl_(message_);
}
