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
#include "sio_client.h"

using namespace std::chrono_literals;

FreeDVReporter::FreeDVReporter(std::string hostname, std::string callsign, std::string gridSquare, std::string software, bool rxOnly)
    : isExiting_(false)
    , isConnecting_(false)
    , hostname_(hostname)
    , callsign_(callsign)
    , gridSquare_(gridSquare)
    , software_(software)
    , lastFrequency_(0)
    , tx_(false)
    , rxOnly_(rxOnly)
{
    sioClient_ = new sio::client();
    assert(sioClient_ != nullptr);
}

FreeDVReporter::~FreeDVReporter()
{
    if (fnQueueThread_.joinable())
    {
        isExiting_ = true;
        fnQueueConditionVariable_.notify_one();
        fnQueueThread_.join();
    
        // Workaround for race condition in sioclient
        while (isConnecting_)
        {
            std::this_thread::sleep_for(20ms);
        }
    }
    
    delete sioClient_;
}

void FreeDVReporter::connect()
{    
    fnQueueThread_ = std::thread(std::bind(&FreeDVReporter::threadEntryPoint_, this));
}

void FreeDVReporter::requestQSY(std::string sid, uint64_t frequencyHz, std::string message)
{
    sio::message::ptr qsyPtr = sio::object_message::create();
    auto qsyData = (sio::object_message*)qsyPtr.get();

    qsyData->insert("dest_sid", sid);
    qsyData->insert("message", message);
    qsyData->insert("frequency", sio::int_message::create(frequencyHz));

    sioClient_->socket()->emit("qsy_request", qsyPtr);
}

void FreeDVReporter::inAnalogMode(bool inAnalog)
{
    std::unique_lock<std::mutex> lk(fnQueueMutex_);
    fnQueue_.push_back([&, inAnalog]() {
        if (inAnalog)
        {
            // Workaround for race condition in sioclient
            while (isConnecting_)
            {
                std::this_thread::sleep_for(20ms);
            }
            
            sioClient_->sync_close();
        }
        else
        {
            connect_();
        }
    });
    fnQueueConditionVariable_.notify_one();
}

void FreeDVReporter::freqChange(uint64_t frequency)
{
    std::unique_lock<std::mutex> lk(fnQueueMutex_);
    fnQueue_.push_back(std::bind(&FreeDVReporter::freqChangeImpl_, this, frequency));
    fnQueueConditionVariable_.notify_one();
}

void FreeDVReporter::transmit(std::string mode, bool tx)
{
    std::unique_lock<std::mutex> lk(fnQueueMutex_);
    fnQueue_.push_back(std::bind(&FreeDVReporter::transmitImpl_, this, mode, tx));
    fnQueueConditionVariable_.notify_one();
}
    
void FreeDVReporter::addReceiveRecord(std::string callsign, std::string mode, uint64_t frequency, char snr)
{
    std::unique_lock<std::mutex> lk(fnQueueMutex_);
    fnQueue_.push_back([&, mode, snr, callsign]() {
        sio::message::ptr rxDataPtr = sio::object_message::create();
        auto rxData = (sio::object_message*)rxDataPtr.get();
    
        rxData->insert("callsign", callsign);
        rxData->insert("mode", mode);
        rxData->insert("snr", sio::int_message::create(snr));
    
        sioClient_->socket()->emit("rx_report", rxDataPtr);
    });
    fnQueueConditionVariable_.notify_one();
}

void FreeDVReporter::send()
{
    // No implementation needed, we send RX/TX reports live.
}

void FreeDVReporter::setOnReporterConnectFn(ReporterConnectionFn fn)
{
    onReporterConnectFn_ = fn;
}

void FreeDVReporter::setOnReporterDisconnectFn(ReporterConnectionFn fn)
{
    onReporterDisconnectFn_ = fn;
}

void FreeDVReporter::setOnUserConnectFn(ConnectionDataFn fn)
{
    onUserConnectFn_ = fn;
}

void FreeDVReporter::setOnUserDisconnectFn(ConnectionDataFn fn)
{
    onUserDisconnectFn_ = fn;
}

void FreeDVReporter::setOnFrequencyChangeFn(FrequencyChangeFn fn)
{
    onFrequencyChangeFn_ = fn;
}

void FreeDVReporter::setOnTransmitUpdateFn(TxUpdateFn fn)
{
    onTransmitUpdateFn_ = fn;
}

void FreeDVReporter::setOnReceiveUpdateFn(RxUpdateFn fn)
{
    onReceiveUpdateFn_ = fn;
}

void FreeDVReporter::setOnQSYRequestFn(QsyRequestFn fn)
{
    onQsyRequestFn_ = fn;
}

void FreeDVReporter::connect_()
{        
    // Connect and send initial info.
    sio::message::ptr authPtr = sio::object_message::create();
    auto auth = (sio::object_message*)authPtr.get();
    auth->insert("callsign", callsign_);
    auth->insert("grid_square", gridSquare_);
    auth->insert("version", software_);
    auth->insert("role", "report");
    auth->insert("rx_only", sio::bool_message::create(rxOnly_));
    
    // Reconnect listener should re-report frequency so that "unknown"
    // doesn't appear.
    sioClient_->set_socket_open_listener([&](std::string)
    {
        isConnecting_ = false;
        
        freqChangeImpl_(lastFrequency_);
        transmitImpl_(mode_, tx_);
    });
    
    sioClient_->set_reconnect_listener([&](unsigned, unsigned)
    {
        isConnecting_ = false;
        
        freqChangeImpl_(lastFrequency_);
        transmitImpl_(mode_, tx_);
    });
    
    sioClient_->set_fail_listener([&]() {
        isConnecting_ = false;
    });
    
    sioClient_->set_close_listener([&](sio::client::close_reason const&) {
        isConnecting_ = false;
        
        if (onReporterDisconnectFn_)
        {
            onReporterDisconnectFn_();
        }
    });
    
    isConnecting_ = true;
    sioClient_->connect(std::string("http://") + hostname_ + "/", authPtr);
    
    if (onReporterConnectFn_)
    {
        onReporterConnectFn_();
    }
    
    sioClient_->socket()->off("new_connection");
    sioClient_->socket()->on("new_connection", [&](sio::event& ev) {
        auto msgParams = ev.get_message()->get_map();

        if (onUserConnectFn_)
        {
            auto sid = msgParams["sid"];
            auto lastUpdate = msgParams["last_update"];
            auto callsign = msgParams["callsign"];
            auto gridSquare = msgParams["grid_square"];
            auto version = msgParams["version"];
            auto rxOnly = msgParams["rx_only"];
            
            // Only call event handler if we received the correct data types
            // for the items in the message.
            if (sid->get_flag() == sio::message::flag_string &&
                lastUpdate->get_flag() == sio::message::flag_string &&
                callsign->get_flag() == sio::message::flag_string &&
                gridSquare->get_flag() == sio::message::flag_string &&
                version->get_flag() == sio::message::flag_string &&
                rxOnly->get_flag() == sio::message::flag_boolean)
            {
                onUserConnectFn_(
                    sid->get_string(),
                    lastUpdate->get_string(),
                    callsign->get_string(),
                    gridSquare->get_string(),
                    version->get_string(),
                    rxOnly->get_bool()
                );
            }
        }
    });

    sioClient_->socket()->off("remove_connection");
    sioClient_->socket()->on("remove_connection", [&](sio::event& ev) {
        auto msgParams = ev.get_message()->get_map();

        if (onUserDisconnectFn_)
        {
            auto sid = msgParams["sid"];
            auto lastUpdate = msgParams["last_update"];
            auto callsign = msgParams["callsign"];
            auto gridSquare = msgParams["grid_square"];
            auto version = msgParams["version"];
            auto rxOnly = msgParams["rx_only"];
            
            // Only call event handler if we received the correct data types
            // for the items in the message.
            if (sid->get_flag() == sio::message::flag_string &&
                lastUpdate->get_flag() == sio::message::flag_string &&
                callsign->get_flag() == sio::message::flag_string &&
                gridSquare->get_flag() == sio::message::flag_string &&
                version->get_flag() == sio::message::flag_string &&
                rxOnly->get_flag() == sio::message::flag_boolean)
            {
                onUserDisconnectFn_(
                    sid->get_string(),
                    lastUpdate->get_string(),
                    callsign->get_string(),
                    gridSquare->get_string(),
                    version->get_string(),
                    rxOnly->get_bool()
                );
            }
        }
    });

    sioClient_->socket()->off("tx_report");
    sioClient_->socket()->on("tx_report", [&](sio::event& ev) {
        auto msgParams = ev.get_message()->get_map();

        if (onTransmitUpdateFn_)
        {
            auto sid = msgParams["sid"];
            auto lastUpdate = msgParams["last_update"];
            auto callsign = msgParams["callsign"];
            auto gridSquare = msgParams["grid_square"];
            auto lastTx = msgParams["last_tx"];
            auto mode = msgParams["mode"];
            auto transmitting = msgParams["transmitting"];
            
            // Only call event handler if we received the correct data types
            // for the items in the message.
            if (sid->get_flag() == sio::message::flag_string &&
                lastUpdate->get_flag() == sio::message::flag_string &&
                callsign->get_flag() == sio::message::flag_string &&
                gridSquare->get_flag() == sio::message::flag_string &&
                mode->get_flag() == sio::message::flag_string &&
                transmitting->get_flag() == sio::message::flag_boolean)
            {
                onTransmitUpdateFn_(
                    sid->get_string(),
                    lastUpdate->get_string(),
                    callsign->get_string(),
                    gridSquare->get_string(),
                    mode->get_string(),
                    transmitting->get_bool(),
                    lastTx->get_flag() == sio::message::flag_null ? "" : lastTx->get_string()
                );
            }
        }
    });

    sioClient_->socket()->off("rx_report");
    sioClient_->socket()->on("rx_report", [&](sio::event& ev) {
        auto msgParams = ev.get_message()->get_map();
        
        auto sid = msgParams["sid"];
        auto lastUpdate = msgParams["last_update"];
        auto receiverCallsign = msgParams["receiver_callsign"];
        auto receiverGridSquare = msgParams["receiver_grid_square"];
        auto callsign = msgParams["callsign"];
        auto snr = msgParams["snr"];
        auto mode = msgParams["mode"];
        
        if (onReceiveUpdateFn_)
        {
            // Only call event handler if we received the correct data types
            // for the items in the message.
            if (sid->get_flag() == sio::message::flag_string &&
                lastUpdate->get_flag() == sio::message::flag_string &&
                callsign->get_flag() == sio::message::flag_string &&
                receiverCallsign->get_flag() == sio::message::flag_string &&
                receiverGridSquare->get_flag() == sio::message::flag_string &&
                mode->get_flag() == sio::message::flag_string &&
                snr->get_flag() == sio::message::flag_double)
            {
                onReceiveUpdateFn_(
                    msgParams["sid"]->get_string(),
                    msgParams["last_update"]->get_string(),
                    msgParams["receiver_callsign"]->get_string(),
                    msgParams["receiver_grid_square"]->get_string(),
                    msgParams["callsign"]->get_string(),
                    msgParams["snr"]->get_double(),
                    msgParams["mode"]->get_string()
                );
            }
        }
    });

    sioClient_->socket()->off("freq_change");
    sioClient_->socket()->on("freq_change", [&](sio::event& ev) {
        auto msgParams = ev.get_message()->get_map();

        if (onFrequencyChangeFn_)
        {
            auto sid = msgParams["sid"];
            auto lastUpdate = msgParams["last_update"];
            auto callsign = msgParams["callsign"];
            auto gridSquare = msgParams["grid_square"];
            auto frequency = msgParams["freq"];
            
            // Only call event handler if we received the correct data types
            // for the items in the message.
            if (sid->get_flag() == sio::message::flag_string &&
                lastUpdate->get_flag() == sio::message::flag_string &&
                callsign->get_flag() == sio::message::flag_string &&
                gridSquare->get_flag() == sio::message::flag_string &&
                frequency->get_flag() == sio::message::flag_integer)
            {
                onFrequencyChangeFn_(
                    sid->get_string(),
                    lastUpdate->get_string(),
                    callsign->get_string(),
                    gridSquare->get_string(),
                    frequency->get_int()
                );
            }
        }
    });
    
    sioClient_->socket()->off("qsy_request");
    sioClient_->socket()->on("qsy_request", [&](sio::event& ev) {
        auto msgParams = ev.get_message()->get_map();
        
        auto callsign = msgParams["callsign"];
        auto frequency = msgParams["frequency"];
        auto message = msgParams["message"];
        
        if (onQsyRequestFn_)
        {
            // Only call event handler if we received the correct data types
            // for the items in the message.
            if (callsign->get_flag() == sio::message::flag_string &&
                frequency->get_flag() == sio::message::flag_integer &&
                message->get_flag() == sio::message::flag_string)
            {
                onQsyRequestFn_(
                    callsign->get_string(),
                    frequency->get_int(),
                    message->get_string()
                );
            }
        }
    });
}

void FreeDVReporter::threadEntryPoint_()
{    
    connect_();
    
    while (!isExiting_)
    {
        std::unique_lock<std::mutex> lk(fnQueueMutex_);
        fnQueueConditionVariable_.wait_for(
            lk, 
            100ms,
            [&]() { return isExiting_ || (!isConnecting_ && fnQueue_.size() > 0); });
        
        // Execute queued method
        while (!isExiting_ && !isConnecting_ && fnQueue_.size() > 0)
        {
            auto fn = fnQueue_.front();
            fnQueue_.erase(fnQueue_.begin());
            
            lk.unlock();
            fn();
            lk.lock();
        }
    }
}

void FreeDVReporter::freqChangeImpl_(uint64_t frequency)
{
    sio::message::ptr freqDataPtr = sio::object_message::create();
    auto freqData = (sio::object_message*)freqDataPtr.get();
    freqData->insert("freq", sio::int_message::create(frequency));
    sioClient_->socket()->emit("freq_change", freqDataPtr);

    // Save last frequency in case we need to reconnect.
    lastFrequency_ = frequency;
}

void FreeDVReporter::transmitImpl_(std::string mode, bool tx)
{
    sio::message::ptr txDataPtr = sio::object_message::create();
    auto txData = (sio::object_message*)txDataPtr.get();
    txData->insert("mode", mode);
    txData->insert("transmitting", sio::bool_message::create(tx));
    sioClient_->socket()->emit("tx_report", txDataPtr);

    // Save last mode and TX state in case we have to reconnect.
    mode_ = mode;
    tx_ = tx;
}
