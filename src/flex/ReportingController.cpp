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

#include <cinttypes>
#include <sstream>
#include "git_version.h"
#include "ReportingController.h"
#include "../util/logging/ulog.h"

// FreeDV Reporter constants and helpers
#define SOFTWARE_NAME "freedv-flex"
std::string GetVersionString()
{   
    std::stringstream ss;
    ss << SOFTWARE_NAME << " " << GetFreeDVVersion();
    return ss.str();
}
#define SOFTWARE_GRID_SQUARE "AA00"
#define MODE_STRING "RADEV1"

ReportingController::ReportingController()
    : reporterConnection_(nullptr)
    , currentGridSquare_(SOFTWARE_GRID_SQUARE)
    , radioCallsign_("")
    , userHidden_(true)
    , currentFreq_(0)
{   
    // empty
}

void ReportingController::transmit(bool transmit)
{   
    enqueue_([&, transmit]() {
        log_info("Reporting TX state %d", (int)transmit);
        if (reporterConnection_ != nullptr)
        {   
            reporterConnection_->transmit(MODE_STRING, transmit);
        }
    });
}

void ReportingController::changeFrequency(uint64_t freqHz)
{   
    enqueue_([&, freqHz]() {
        log_info("Reporting frequency %" PRIu64, freqHz);
        if (reporterConnection_ != nullptr)
        {
            reporterConnection_->freqChange(freqHz);
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
    if (reporterConnection_ != nullptr && userHidden_)
    {
        log_info("Disconnecting from FreeDV Reporter");
        delete reporterConnection_;
        reporterConnection_ = nullptr;
    }

    if (reporterConnection_ == nullptr && !userHidden_)
    {
        log_info("Connecting to FreeDV Reporter (callsign = %s, grid square = %s, version = %s)", radioCallsign_.c_str(), currentGridSquare_.c_str(), GetVersionString().c_str());
        reporterConnection_ = new FreeDVReporter("", radioCallsign_, currentGridSquare_, GetVersionString(), false, true);
        reporterConnection_->connect();
    }
}

void ReportingController::updateRadioCallsign(std::string newCallsign)
{   
    enqueue_([&, newCallsign]() {
        log_info("Updating callsign to %s", newCallsign.c_str());
        bool changed = newCallsign != radioCallsign_;
        radioCallsign_ = newCallsign;
        if (changed)
        {
            if (reporterConnection_ != nullptr)
            {
                log_info("Disconnecting from FreeDV Reporter");
                delete reporterConnection_;
                reporterConnection_ = nullptr;
            }
            updateReporterState_();
        }
    });
}

void ReportingController::reportCallsign(std::string callsign, char snr)
{
    enqueue_([&, callsign, snr]() {
        log_info("Reporting RX callsign %s (SNR %d) to FreeDV Reporter", callsign.c_str(), (int)snr);
        reporterConnection_->addReceiveRecord(callsign, MODE_STRING, currentFreq_, snr);
    });
}

void ReportingController::updateRadioGridSquare(std::string newGridSquare)
{
    enqueue_([&, newGridSquare]() {
        if (newGridSquare == "") return;

        log_info("Grid square updated to %s", newGridSquare.c_str());
        bool changed = newGridSquare != currentGridSquare_;
        currentGridSquare_ = newGridSquare;
        if (changed)
        {
            if (reporterConnection_ != nullptr)
            {
                delete reporterConnection_;
                reporterConnection_ = nullptr;
            }
            updateReporterState_();
        }
    });
}

