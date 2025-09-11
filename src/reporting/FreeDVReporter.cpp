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

FreeDVReporter::FreeDVReporter(std::string hostname, std::string callsign, std::string gridSquare, std::string software, bool rxOnly, bool writeOnly)
    : isConnecting_(false)
    , hostname_(hostname)
    , callsign_(callsign)
    , gridSquare_(gridSquare)
    , software_(software)
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
}

FreeDVReporter::~FreeDVReporter()
{
    delete sioClient_;
}

void FreeDVReporter::connect()
{
    connect_();
}

void FreeDVReporter::requestQSY(std::string sid, uint64_t frequencyHz, std::string message)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    
    if (isValidForReporting())
    {
        nlohmann::json qsyMsg = {
            {"dest_sid", sid},
            {"message", message},
            {"frequency", frequencyHz}
        };

        sioClient_->emit("qsy_request", qsyMsg);
    }
}

void FreeDVReporter::updateMessage(std::string message)
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
    
void FreeDVReporter::addReceiveRecord(std::string callsign, std::string mode, uint64_t frequency, signed char snr)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    
    if (isValidForReporting() && isFullyConnected_)
    {
        nlohmann::json rxData = {
            {"callsign", callsign},
            {"mode", mode},
            {"snr", (int)snr}
        };
    
        sioClient_->emit("rx_report", rxData);
    }
}

void FreeDVReporter::send()
{
    // No implementation needed, we send RX/TX reports live.
}

void FreeDVReporter::setOnReporterConnectFn(ReporterConnectionFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onReporterConnectFn_ = fn;
}

void FreeDVReporter::setOnReporterDisconnectFn(ReporterConnectionFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onReporterDisconnectFn_ = fn;
}

void FreeDVReporter::setOnUserConnectFn(ConnectionDataFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onUserConnectFn_ = fn;
}

void FreeDVReporter::setOnUserDisconnectFn(ConnectionDataFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onUserDisconnectFn_ = fn;
}

void FreeDVReporter::setOnFrequencyChangeFn(FrequencyChangeFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onFrequencyChangeFn_ = fn;
}

void FreeDVReporter::setOnTransmitUpdateFn(TxUpdateFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onTransmitUpdateFn_ = fn;
}

void FreeDVReporter::setOnReceiveUpdateFn(RxUpdateFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onReceiveUpdateFn_ = fn;
}

void FreeDVReporter::setOnQSYRequestFn(QsyRequestFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onQsyRequestFn_ = fn;
}

void FreeDVReporter::setMessageUpdateFn(MessageUpdateFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onMessageUpdateFn_ = fn;
}

void FreeDVReporter::setConnectionSuccessfulFn(ConnectionSuccessfulFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onConnectionSuccessfulFn_ = fn;
}

void FreeDVReporter::setAboutToShowSelfFn(AboutToShowSelfFn fn)
{
    std::unique_lock<std::mutex> lk(objMutex_);
    onAboutToShowSelfFn_ = fn;
}

bool FreeDVReporter::isValidForReporting()
{
    return callsign_ != "" && gridSquare_ != "";
}

void FreeDVReporter::connect_()
{        
    // Connect and send initial info.
    nlohmann::json authData;
    if (!isValidForReporting())
    {
        authData = {
            {"role", "view"}
        };
    }
    else
    {
        authData = {
            {"role", writeOnly_ ? "report_wo" : "report"},
            {"callsign", callsign_},
            {"grid_square", gridSquare_},
            {"version", software_},
            {"rx_only", rxOnly_},
            {"os", GetOperatingSystemString()}
        };
    }

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
        isFullyConnected_ = false;
        
        if (onReporterDisconnectFn_)
        {
            onReporterDisconnectFn_();
        }
    });
    
    isConnecting_ = true;
    isFullyConnected_ = false;
    
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
    sioClient_->setAuthDictionary(authData);
    sioClient_->connect(host.c_str(), port, true);
    
    if (onReporterConnectFn_)
    {
        onReporterConnectFn_();
    }
    
    sioClient_->on("new_connection", [&](const nlohmann::json& msgParams) {
        std::unique_lock<std::mutex> lk(objMutex_);
        if (onUserConnectFn_)
        {
            auto& sid = msgParams["sid"];
            auto& lastUpdate = msgParams["last_update"];
            auto& callsign = msgParams["callsign"];
            auto& gridSquare = msgParams["grid_square"];
            auto& version = msgParams["version"];
            auto& rxOnly = msgParams["rx_only"];
        
            // Only call event handler if we received the correct data types
            // for the items in the message.
            if (sid.is_string() &&
                lastUpdate.is_string() &&
                callsign.is_string() &&
                gridSquare.is_string() &&
                version.is_string() &&
                rxOnly.is_boolean())
            {
                onUserConnectFn_(
                    sid.template get<std::string>(),
                    lastUpdate.template get<std::string>(),
                    callsign.template get<std::string>(),
                    gridSquare.template get<std::string>(),
                    version.template get<std::string>(),
                    rxOnly.template get<bool>()
                );
            }
        }
    });

    sioClient_->on("connection_successful", [&](const nlohmann::json&) {
        std::unique_lock<std::mutex> lk(objMutex_);
        isFullyConnected_ = true;
    
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
    });

    sioClient_->on("remove_connection", [&](const nlohmann::json& msgParams) {
        std::unique_lock<std::mutex> lk(objMutex_);
        if (onUserDisconnectFn_)
        {
            auto& sid = msgParams["sid"];
            auto& lastUpdate = msgParams["last_update"];
            auto& callsign = msgParams["callsign"];
            auto& gridSquare = msgParams["grid_square"];
            auto& version = msgParams["version"];
            auto& rxOnly = msgParams["rx_only"];
        
            // Only call event handler if we received the correct data types
            // for the items in the message.
            if (sid.is_string() &&
                lastUpdate.is_string() &&
                callsign.is_string() &&
                gridSquare.is_string() &&
                version.is_string() &&
                rxOnly.is_boolean())
            {
                onUserDisconnectFn_(
                    sid.template get<std::string>(),
                    lastUpdate.template get<std::string>(),
                    callsign.template get<std::string>(),
                    gridSquare.template get<std::string>(),
                    version.template get<std::string>(),
                    rxOnly.template get<bool>()
                );
            }
        }
    });

    sioClient_->on("tx_report", [&](const nlohmann::json& msgParams) {
        std::unique_lock<std::mutex> lk(objMutex_);
        if (onTransmitUpdateFn_)
        {
            auto& sid = msgParams["sid"];
            auto& lastUpdate = msgParams["last_update"];
            auto& callsign = msgParams["callsign"];
            auto& gridSquare = msgParams["grid_square"];
            auto& lastTx = msgParams["last_tx"];
            auto& mode = msgParams["mode"];
            auto& transmitting = msgParams["transmitting"];
        
            // Only call event handler if we received the correct data types
            // for the items in the message.
            if (sid.is_string() &&
                lastUpdate.is_string() &&
                callsign.is_string() &&
                gridSquare.is_string() &&
                mode.is_string() &&
                transmitting.is_boolean())
            {
                onTransmitUpdateFn_(
                    sid.template get<std::string>(),
                    lastUpdate.template get<std::string>(),
                    callsign.template get<std::string>(),
                    gridSquare.template get<std::string>(),
                    mode.template get<std::string>(),
                    transmitting.template get<bool>(),
                    lastTx.is_null() ? "" : lastTx.template get<std::string>()
                );
            }
        }
    });

    sioClient_->on("rx_report", [&](const nlohmann::json& msgParams) {
        std::unique_lock<std::mutex> lk(objMutex_);
        auto& sid = msgParams["sid"];
        auto& lastUpdate = msgParams["last_update"];
        auto& receiverCallsign = msgParams["receiver_callsign"];
        auto& receiverGridSquare = msgParams["receiver_grid_square"];
        auto& callsign = msgParams["callsign"];
        auto& snr = msgParams["snr"];
        auto& mode = msgParams["mode"];
    
        if (onReceiveUpdateFn_)
        {
            bool snrInteger =  snr.is_number_integer();
            bool snrFloat =  snr.is_number_float();
            bool snrValid = snrInteger || snrFloat;

            float snrVal = 0;
            if (snrInteger)
            {
                snrVal = snr.template get<int>();
            }
            else if (snrFloat)
            {
                snrVal = snr.template get<double>();
            }

            // Only call event handler if we received the correct data types
            // for the items in the message.
            if (sid.is_string() &&
                lastUpdate.is_string() &&
                callsign.is_string() &&
                receiverCallsign.is_string() &&
                receiverGridSquare.is_string() &&
                mode.is_string() &&
                snrValid)
            {
                onReceiveUpdateFn_(
                    sid.template get<std::string>(),
                    lastUpdate.template get<std::string>(),
                    receiverCallsign.template get<std::string>(),
                    receiverGridSquare.template get<std::string>(),
                    callsign.template get<std::string>(),
                    snrVal,
                    mode.template get<std::string>()
                );
            }
        }
    });

    sioClient_->on("freq_change", [&](const nlohmann::json& msgParams) {
        std::unique_lock<std::mutex> lk(objMutex_);
        if (onFrequencyChangeFn_)
        {
            auto& sid = msgParams["sid"];
            auto& lastUpdate = msgParams["last_update"];
            auto& callsign = msgParams["callsign"];
            auto& gridSquare = msgParams["grid_square"];
            auto& frequency = msgParams["freq"];
        
            // Only call event handler if we received the correct data types
            // for the items in the message.
            if (sid.is_string() &&
                lastUpdate.is_string() &&
                callsign.is_string() &&
                gridSquare.is_string() &&
                frequency.is_number())
            {
                onFrequencyChangeFn_(
                    sid.template get<std::string>(),
                    lastUpdate.template get<std::string>(),
                    callsign.template get<std::string>(),
                    gridSquare.template get<std::string>(),
                    frequency.template get<uint64_t>()
                );
            }
        }
    });

    sioClient_->on("message_update", [&](const nlohmann::json& msgParams) {  
        std::unique_lock<std::mutex> lk(objMutex_);
        if (onMessageUpdateFn_)
        {
            auto& sid = msgParams["sid"];
            auto& lastUpdate = msgParams["last_update"];
            auto& message = msgParams["message"];

            // Only call event handler if we received the correct data types
            // for the items in the message.
            if (sid.is_string() &&
                lastUpdate.is_string()&&
                message.is_string())
            {
                onMessageUpdateFn_(
                    sid.template get<std::string>(),
                    lastUpdate.template get<std::string>(),
                    message.template get<std::string>()
                );
            }
        }
    });
    
    sioClient_->on("qsy_request", [&](const nlohmann::json& msgParams) {
        std::unique_lock<std::mutex> lk(objMutex_);
        auto& callsign = msgParams["callsign"];
        auto& frequency = msgParams["frequency"];
        auto& message = msgParams["message"];
    
        if (onQsyRequestFn_)
        {
            // Only call event handler if we received the correct data types
            // for the items in the message.
            if (callsign.is_string() &&
                frequency.is_number() &&
                message.is_string())
            {
                onQsyRequestFn_(
                    callsign.template get<std::string>(),
                    frequency.template get<uint64_t>(),
                    message.template get<std::string>()
                );
            }
        }
    });
}

void FreeDVReporter::freqChangeImpl_(uint64_t frequency)
{
    if (isFullyConnected_)
    {
        nlohmann::json freqData = {
            {"freq", frequency}
        };
        sioClient_->emit("freq_change", freqData);
    }
    
    // Save last frequency in case we need to reconnect.
    lastFrequency_ = frequency;
}

void FreeDVReporter::transmitImpl_(std::string mode, bool tx)
{
    if (isFullyConnected_)
    {
        nlohmann::json txData = {
            {"mode", mode},
            {"transmitting", tx}
        };
        sioClient_->emit("tx_report", txData);
    }
    
    // Save last mode and TX state in case we have to reconnect.
    mode_ = mode;
    tx_ = tx;
}

void FreeDVReporter::sendMessageImpl_(std::string message)
{
    if (isFullyConnected_)
    {
        nlohmann::json txData = {
            {"message", message}
        };
        sioClient_->emit("message_update", txData);
    }
    
    // Save last mode and TX state in case we have to reconnect.
    message_ = message;
}

void FreeDVReporter::hideFromViewImpl_()
{
    hidden_ = true;
    if (isFullyConnected_)
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
    if (isFullyConnected_)
    {
        sioClient_->emit("show_self");
    }
    
    freqChangeImpl_(lastFrequency_);
    transmitImpl_(mode_, tx_);
    sendMessageImpl_(message_);
}
