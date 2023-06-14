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

using namespace std::chrono_literals;

FreeDVReporter::FreeDVReporter(std::string hostname, std::string callsign, std::string gridSquare, std::string software)
    : isExiting_(false)
    , isConnecting_(false)
    , hostname_(hostname)
    , callsign_(callsign)
    , gridSquare_(gridSquare)
    , software_(software)
    , lastFrequency_(0)
    , tx_(false)
{
    // empty
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
}

void FreeDVReporter::connect()
{    
    fnQueueThread_ = std::thread(std::bind(&FreeDVReporter::threadEntryPoint_, this));
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
            
            sioClient_.sync_close();
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
    fnQueue_.push_back([&, mode, snr]() {
        sio::message::ptr rxDataPtr = sio::object_message::create();
        auto rxData = (sio::object_message*)rxDataPtr.get();
    
        rxData->insert("callsign", callsign);
        rxData->insert("mode", mode);
        rxData->insert("snr", sio::int_message::create(snr));
    
        sioClient_.socket()->emit("rx_report", rxDataPtr);
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

void FreeDVReporter::connect_()
{        
    // Connect and send initial info.
    sio::message::ptr authPtr = sio::object_message::create();
    auto auth = (sio::object_message*)authPtr.get();
    auth->insert("callsign", callsign_);
    auth->insert("grid_square", gridSquare_);
    auth->insert("version", software_);
    auth->insert("role", "report");
    
    // Reconnect listener should re-report frequency so that "unknown"
    // doesn't appear.
    sioClient_.set_socket_open_listener([&](std::string)
    {
        isConnecting_ = false;        
        freqChangeImpl_(lastFrequency_);
        transmitImpl_(mode_, tx_);
    });
    
    sioClient_.set_reconnect_listener([&](unsigned, unsigned)
    {
        isConnecting_ = false;
        
        if (onReporterConnectFn_)
        {
            onReporterConnectFn_();
        }
        
        freqChangeImpl_(lastFrequency_);
        transmitImpl_(mode_, tx_);
    });
    
    sioClient_.set_fail_listener([&]() {
        isConnecting_ = false;
    });
    
    sioClient_.set_close_listener([&](sio::client::close_reason const&) {
        isConnecting_ = false;
        
        if (onReporterDisconnectFn_)
        {
            onReporterDisconnectFn_();
        }
    });
    
    isConnecting_ = true;
    sioClient_.connect(std::string("http://") + hostname_ + "/", authPtr);
    
    sioClient_.socket()->on("new_connection", [&](sio::event& ev) {
        auto msgParams = ev.get_message()->get_map();

        if (onUserConnectFn_)
        {
            onUserConnectFn_(
                msgParams["sid"]->get_string(),
                msgParams["last_update"]->get_string(),
                msgParams["callsign"]->get_string(),
                msgParams["grid_square"]->get_string(),
                msgParams["version"]->get_string()
            );
        }
    });

    sioClient_.socket()->on("remove_connection", [&](sio::event& ev) {
        auto msgParams = ev.get_message()->get_map();

        if (onUserDisconnectFn_)
        {
            onUserDisconnectFn_(
                msgParams["sid"]->get_string(),
                msgParams["last_update"]->get_string(),
                msgParams["callsign"]->get_string(),
                msgParams["grid_square"]->get_string(),
                msgParams["version"]->get_string()
            );
        }
    });

    sioClient_.socket()->on("tx_report", [&](sio::event& ev) {
        auto msgParams = ev.get_message()->get_map();

        if (onTransmitUpdateFn_)
        {
            auto lastTx = msgParams["last_tx"];
        
            onTransmitUpdateFn_(
                msgParams["sid"]->get_string(),
                msgParams["last_update"]->get_string(),
                msgParams["callsign"]->get_string(),
                msgParams["grid_square"]->get_string(),
                msgParams["mode"]->get_string(),
                msgParams["transmitting"]->get_bool(),
                lastTx->get_flag() == sio::message::flag_null ? "" : lastTx->get_string()
            );
        }
    });

    sioClient_.socket()->on("rx_report", [&](sio::event& ev) {
        auto msgParams = ev.get_message()->get_map();

        if (onReceiveUpdateFn_)
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
    });

    sioClient_.socket()->on("freq_change", [&](sio::event& ev) {
        auto msgParams = ev.get_message()->get_map();

        if (onFrequencyChangeFn_)
        {
            onFrequencyChangeFn_(
                msgParams["sid"]->get_string(),
                msgParams["last_update"]->get_string(),
                msgParams["callsign"]->get_string(),
                msgParams["grid_square"]->get_string(),
                msgParams["freq"]->get_int()
            );
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
    sioClient_.socket()->emit("freq_change", freqDataPtr);

    // Save last frequency in case we need to reconnect.
    lastFrequency_ = frequency;
}

void FreeDVReporter::transmitImpl_(std::string mode, bool tx)
{
    sio::message::ptr txDataPtr = sio::object_message::create();
    auto txData = (sio::object_message*)txDataPtr.get();
    txData->insert("mode", mode);
    txData->insert("transmitting", sio::bool_message::create(tx));
    sioClient_.socket()->emit("tx_report", txDataPtr);

    // Save last mode and TX state in case we have to reconnect.
    mode_ = mode;
    tx_ = tx;
}
