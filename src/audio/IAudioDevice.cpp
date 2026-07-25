//=========================================================================
// Name:            IAudioDevice.cpp
// Purpose:         Defines the interface to an audio device.
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

#include "IAudioDevice.h"

using namespace std::chrono_literals;

void IAudioDevice::setDescription(std::string desc)
{
    description = std::move(desc);
}

void IAudioDevice::setOnAudioData(AudioDataCallbackFn fn, void* state)
{
    onAudioDataFunction = fn;
    onAudioDataState = state;
}

void IAudioDevice::setOnAudioOverflow(AudioOverflowCallbackFn fn, void* state)
{
    onAudioOverflowFunction = fn;
    onAudioOverflowState = state;
}

void IAudioDevice::setOnAudioUnderflow(AudioUnderflowCallbackFn fn, void* state)
{
    onAudioUnderflowFunction = fn;
    onAudioUnderflowState = state;
}

void IAudioDevice::setOnAudioError(AudioErrorCallbackFn fn, void* state)
{
    onAudioErrorFunction = fn;
    onAudioErrorState = state;
}

void IAudioDevice::setOnAudioDeviceChanged(AudioDeviceChangedCallbackFn fn, void* state)
{
    onAudioDeviceChangedFunction = fn;
    onAudioDeviceChangedState = state;
}

void IAudioDevice::startRealTimeWork()
{
    startTime_ = std::chrono::steady_clock::now();
}

void IAudioDevice::stopRealTimeWork(bool fastMode)
{
    auto sleepTime = fastMode ? 10ms : 20ms;
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(sleepTime - (endTime - startTime_) - std::chrono::nanoseconds(extraTimeNs_));
    
    if (duration > 0ns)
    {
        auto startSleepTime = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(duration);
        auto endSleepTime = std::chrono::steady_clock::now();
        
        auto sleepDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(endSleepTime - startSleepTime).count() - duration.count();
        extraTimeNs_ = std::max((int64_t)0, (int64_t)sleepDuration); // cap extra time to >= 0.
    }
    else
    {
        extraTimeNs_ = 0;
    }
}