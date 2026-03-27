//=========================================================================
// Name:            MinimalRealtimeHelper.h
// Purpose:         Realtime helper for FreeDV integrations.
//
// Authors:         Mooneer Salem
// License:
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//=========================================================================

#ifndef MINIMAL_REALTIME_HELPER_H
#define MINIMAL_REALTIME_HELPER_H

#include <thread>
#include <chrono>
#include "../util/IRealtimeHelper.h"

using namespace std::chrono_literals;

class MinimalRealtimeHelper : public IRealtimeHelper
{
public:
    MinimalRealtimeHelper() = default;
    virtual ~MinimalRealtimeHelper() = default;
    
    // Configures current thread for real-time priority. This should be
    // called from the thread that will be operating on received audio.
    virtual void setHelperRealTime() override;
    
    // Lets audio system know that we're beginning to do work with the
    // received audio.
    virtual void startRealTimeWork() override { /* empty */ }
    
    // Lets audio system know that we're done with the work on the received
    // audio.
    virtual void stopRealTimeWork(bool fastMode = false) override { (void)fastMode; std::this_thread::sleep_for(10ms); }
    
    // Reverts real-time priority for current thread.
    virtual void clearHelperRealTime() override { /* empty */ }

    // Returns true if real-time thread MUST sleep ASAP. Failure to do so
    // may result in SIGKILL being sent to the process by the kernel.
    virtual bool mustStopWork() FREEDV_NONBLOCKING override { return false; }
};

#endif // MINIMAL_REALTIME_HELPER_H
