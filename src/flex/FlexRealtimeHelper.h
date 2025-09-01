//=========================================================================
// Name:            FlexRealtimeHelper.h
// Purpose:         Realtime helper for Flex waveform.
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

#ifndef FLEX_REALTIME_HELPER_H
#define FLEX_REALTIME_HELPER_H

#include <thread>
#include <chrono>
#include "../util/IRealtimeHelper.h"

using namespace std::chrono_literals;

class FlexRealtimeHelper : public IRealtimeHelper
{
public:
    FlexRealtimeHelper() = default;
    virtual ~FlexRealtimeHelper() = default;
    
    // Configures current thread for real-time priority. This should be
    // called from the thread that will be operating on received audio.
    virtual void setHelperRealTime() override { /* empty */ }
    
    // Lets audio system know that we're beginning to do work with the
    // received audio.
    virtual void startRealTimeWork() override { /* empty */ }
    
    // Lets audio system know that we're done with the work on the received
    // audio.
    virtual void stopRealTimeWork(bool fastMode = false) override { std::this_thread::sleep_for(10ms); }
    
    // Reverts real-time priority for current thread.
    virtual void clearHelperRealTime() override { /* empty */ }

    // Returns true if real-time thread MUST sleep ASAP. Failure to do so
    // may result in SIGKILL being sent to the process by the kernel.
    virtual bool mustStopWork() override { return false; }
};

#endif // FLEX_REALTIME_HELPER_H