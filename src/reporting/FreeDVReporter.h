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
#include "sio_client.h"
#include "IReporter.h"

class FreeDVReporter : public IReporter
{
public:
    FreeDVReporter(std::string hostname, std::string callsign, std::string gridSquare, std::string software);
    virtual ~FreeDVReporter();

    virtual void freqChange(uint64_t frequency) override;
    virtual void transmit(std::string mode, bool tx) override;
    
    virtual void inAnalogMode(bool inAnalog) override;
    
    virtual void addReceiveRecord(std::string callsign, std::string mode, uint64_t frequency, char snr) override;
    virtual void send() override;
    
private:
    // Required elements to implement execution thread for FreeDV Reporter.
    std::vector<std::function<void()> > fnQueue_;
    std::mutex fnQueueMutex_;
    std::condition_variable fnQueueConditionVariable_;
    bool isExiting_;
    std::thread fnQueueThread_;
    bool isConnecting_;
    
    sio::client sioClient_;
    std::string hostname_;
    std::string callsign_;
    std::string gridSquare_;
    std::string software_;
    uint64_t lastFrequency_;
    std::string mode_;
    bool tx_;
    
    void connect_();
    
    void threadEntryPoint_();
    void freqChangeImpl_(uint64_t frequency);
    void transmitImpl_(std::string mode, bool tx);
};

#endif // FREEDV_REPORTER_H