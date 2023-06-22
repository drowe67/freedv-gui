#ifndef FREEDV_REPORTER_H
#define FREEDV_REPORTER_H

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

#include <functional>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <string>
#include <thread>
#include <functional>
#include "IReporter.h"

namespace sio
{
    class client; // forward declaration
}

class FreeDVReporter : public IReporter
{
public:
    using ReporterConnectionFn = std::function<void()>;
    
    // sid, last_update, callsign, grid_square, version, rx_only
    // Used for both connect and disconnect
    using ConnectionDataFn = std::function<void(std::string, std::string, std::string, std::string, std::string, bool)>;
    
    // sid, last_update, callsign, grid_square, frequency
    using FrequencyChangeFn = std::function<void(std::string, std::string, std::string, std::string, uint64_t)>;
    
    // sid, last_update, callsign, grid_square, txMode, transmitting, lastTx
    using TxUpdateFn = std::function<void(std::string, std::string, std::string, std::string, std::string, bool, std::string)>;
    
    // sid, last_update, callsign, grid_square, received_callsign, snr, rxMode
    using RxUpdateFn = std::function<void(std::string, std::string, std::string, std::string, std::string, float, std::string)>;
    
    // callsign, frequency, message
    using QsyRequestFn = std::function<void(std::string, uint64_t, std::string)>;
    
    FreeDVReporter(std::string hostname, std::string callsign, std::string gridSquare, std::string software, bool rxOnly);
    virtual ~FreeDVReporter();

    void connect();
    void requestQSY(std::string sid, uint64_t frequencyHz, std::string message);
    
    virtual void freqChange(uint64_t frequency) override;
    virtual void transmit(std::string mode, bool tx) override;
    
    virtual void inAnalogMode(bool inAnalog) override;
    
    virtual void addReceiveRecord(std::string callsign, std::string mode, uint64_t frequency, char snr) override;
    virtual void send() override;
    
    void setOnReporterConnectFn(ReporterConnectionFn fn);
    void setOnReporterDisconnectFn(ReporterConnectionFn fn);
    
    void setOnUserConnectFn(ConnectionDataFn fn);
    void setOnUserDisconnectFn(ConnectionDataFn fn);
    void setOnFrequencyChangeFn(FrequencyChangeFn fn);
    void setOnTransmitUpdateFn(TxUpdateFn fn);
    void setOnReceiveUpdateFn(RxUpdateFn fn);
    
    void setOnQSYRequestFn(QsyRequestFn fn);
    
private:
    // Required elements to implement execution thread for FreeDV Reporter.
    std::vector<std::function<void()> > fnQueue_;
    std::mutex fnQueueMutex_;
    std::condition_variable fnQueueConditionVariable_;
    bool isExiting_;
    std::thread fnQueueThread_;
    bool isConnecting_;
    
    sio::client* sioClient_;
    std::string hostname_;
    std::string callsign_;
    std::string gridSquare_;
    std::string software_;
    uint64_t lastFrequency_;
    std::string mode_;
    bool tx_;
    bool rxOnly_;
    
    ReporterConnectionFn onReporterConnectFn_;
    ReporterConnectionFn onReporterDisconnectFn_;
    
    ConnectionDataFn onUserConnectFn_;
    ConnectionDataFn onUserDisconnectFn_;
    FrequencyChangeFn onFrequencyChangeFn_;
    TxUpdateFn onTransmitUpdateFn_;
    RxUpdateFn onReceiveUpdateFn_;
    
    QsyRequestFn onQsyRequestFn_;
        
    void connect_();
    
    void threadEntryPoint_();
    void freqChangeImpl_(uint64_t frequency);
    void transmitImpl_(std::string mode, bool tx);
};

#endif // FREEDV_REPORTER_H
