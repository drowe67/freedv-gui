//=========================================================================
// Name:            ReportingController.cpp
// Purpose:         Controller responsible for reporting to FreeDV Reporter.
//
// Authors:         Mooneer Salem
// License:
//
//  All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANYg_eoo_enqueued
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//=========================================================================

#include <future>
#include <cinttypes>
#include <sstream>
#include "git_version.h"
#include "ReportingController.h"
#include "../util/logging/ulog.h"

// FreeDV Reporter constants and helpers
std::string ReportingController::getVersionString_()
{   
    std::stringstream ss;
    ss << softwareName_ << " " << GetFreeDVVersion();
    return ss.str();
}
#define SOFTWARE_GRID_SQUARE "AA00"
#define MODE_STRING "RADEV1"

#define PSKREPORTER_REPORT_INTERVAL_MS (5*60*1000)

ReportingController::ReportingController(std::string softwareName, bool rxOnly)
    : ThreadedObject("Reporting")
    , softwareName_(std::move(softwareName))
    , pskReporterSendTimer_(PSKREPORTER_REPORT_INTERVAL_MS, [&](ThreadedTimer&) {
        enqueue_([&]() {
            if (pskReporterConnection_ != nullptr)
            {
                pskReporterConnection_->send();
            }
        });
    }, true)
    , freedvReporterConnection_(nullptr)
    , pskReporterConnection_(nullptr)
    , currentGridSquare_(SOFTWARE_GRID_SQUARE)
    , radioCallsign_("")
    , userHidden_(true)
    , currentFreq_(0)
    , rxOnly_(rxOnly)
{   
    // empty
}

ReportingController::~ReportingController()
{   
    enqueue_([&]() {
        if (freedvReporterConnection_ != nullptr)
        {
            delete freedvReporterConnection_;
        }
        if (pskReporterConnection_ != nullptr)
        {
            delete pskReporterConnection_;
        }
    });
        
    waitForAllTasksComplete_();
}

bool ReportingController::isHidden()
{
    auto prom = std::make_shared<std::promise<bool>>();
    auto fut = prom->get_future();
    enqueue_([&, prom]() {
        prom->set_value(userHidden_);
    });
    return fut.get();
}

void ReportingController::transmit(bool transmit)
{   
    enqueue_([&, transmit]() {
        log_info("Reporting TX state %d", (int)transmit);
        if (freedvReporterConnection_ != nullptr)
        {   
            freedvReporterConnection_->transmit(MODE_STRING, transmit);
        }
    });
}

void ReportingController::changeFrequency(uint64_t freqHz)
{   
    enqueue_([&, freqHz]() {
        log_info("Reporting frequency %" PRIu64, freqHz);
        if (freedvReporterConnection_ != nullptr)
        {
            freedvReporterConnection_->freqChange(freqHz);
        }
        currentFreq_ = freqHz;
    });
}

void ReportingController::showSelf()
{
    enqueue_([&]() {
        log_info("Showing ourselves on FreeDV Reporter");
        userHidden_ = false;
        updateReporterState_();
    });
}

void ReportingController::hideSelf()
{
    enqueue_([&]() {
        log_info("Hiding ourselves on FreeDV Reporter");
        userHidden_ = true;
        updateReporterState_();
    });
}

void ReportingController::updateReporterState_()
{
    if (freedvReporterConnection_ != nullptr && userHidden_)
    {
        log_info("Disconnecting from FreeDV Reporter");
        delete freedvReporterConnection_;
        freedvReporterConnection_ = nullptr;
    }

    if (pskReporterConnection_ != nullptr)
    {
        log_info("Disconnecting from PSK Reporter");
        pskReporterSendTimer_.stop();
        delete pskReporterConnection_;
        pskReporterConnection_ = nullptr;
    }

    if (freedvReporterConnection_ == nullptr && !userHidden_)
    {
        log_info("Connecting to FreeDV Reporter (callsign = %s, grid square = %s, version = %s)", radioCallsign_.c_str(), currentGridSquare_.c_str(), getVersionString_().c_str());
        freedvReporterConnection_ = new FreeDVReporter("", radioCallsign_, currentGridSquare_, getVersionString_(), rxOnly_, true);
        freedvReporterConnection_->connect();
    }

    if (pskReporterConnection_ == nullptr && currentGridSquare_ != SOFTWARE_GRID_SQUARE)
    {
        log_info("Connecting to PSK Reporter (callsign = %s, grid square = %s, version = %s)", radioCallsign_.c_str(), currentGridSquare_.c_str(), getVersionString_().c_str());
        pskReporterConnection_ = new PskReporter(radioCallsign_, currentGridSquare_, getVersionString_());
        pskReporterSendTimer_.start();
    }
}

void ReportingController::updateRadioCallsign(std::string const& newCallsign)
{   
    enqueue_([&, newCallsign]() {
        log_info("Updating callsign to %s", newCallsign.c_str());
        bool changed = newCallsign != radioCallsign_;
        radioCallsign_ = newCallsign;
        if (changed)
        {
            if (freedvReporterConnection_ != nullptr)
            {
                log_info("Disconnecting from FreeDV Reporter");
                delete freedvReporterConnection_;
                freedvReporterConnection_ = nullptr;
            }
            updateReporterState_();
        }
    });
}

void ReportingController::reportCallsign(std::string const& callsign, char snr)
{
    enqueue_([&, callsign, snr]() {
        log_info("Reporting RX callsign %s (SNR %d) to FreeDV Reporter", callsign.c_str(), (int)snr);
        if (freedvReporterConnection_ != nullptr)
        {
            freedvReporterConnection_->addReceiveRecord(callsign, MODE_STRING, currentFreq_, snr);
        }
        if (pskReporterConnection_ != nullptr)
        {
            pskReporterConnection_->addReceiveRecord(callsign, MODE_STRING, currentFreq_, snr);
        }
    });
}

void ReportingController::updateRadioGridSquare(std::string const& newGridSquare)
{
    enqueue_([&, newGridSquare]() {
        if (newGridSquare == "") return;

        log_info("Grid square updated to %s", newGridSquare.c_str());
        bool changed = newGridSquare != currentGridSquare_;
        currentGridSquare_ = newGridSquare;
        if (changed)
        {
            if (freedvReporterConnection_ != nullptr)
            {
                delete freedvReporterConnection_;
                freedvReporterConnection_ = nullptr;
            }
            updateReporterState_();
        }
    });
}

