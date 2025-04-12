//=========================================================================
// Name:            IAudioDevice.h
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

#ifndef I_AUDIO_DEVICE_H
#define I_AUDIO_DEVICE_H

#include <string>
#include <vector>
#include <functional>
#include "AudioDeviceSpecification.h"

class IAudioDevice
{
public:
    typedef std::function<void(IAudioDevice&, void*, size_t, void*)> AudioDataCallbackFn;
    typedef std::function<void(IAudioDevice&, void*)> AudioUnderflowCallbackFn;
    typedef std::function<void(IAudioDevice&, void*)> AudioOverflowCallbackFn;
    typedef std::function<void(IAudioDevice&, std::string, void*)> AudioErrorCallbackFn;
    typedef std::function<void(IAudioDevice&, std::string, void*)> AudioDeviceChangedCallbackFn;
    
    virtual int getNumChannels() = 0;
    virtual int getSampleRate() const = 0;
    
    virtual void start() = 0;
    virtual void stop() = 0;

    virtual bool isRunning() = 0;
    
    virtual int getLatencyInMicroseconds() = 0;

    // Sets user friendly description of device. Not used by all engines.
    void setDescription(std::string desc);
    
    // Set RX/TX ready callback.
    // Callback must take the following parameters:
    //    1. Audio device.
    //    2. Pointer to buffer containing RX/TX data.
    //    3. Size of buffer.
    //    4. Pointer to user-provided state object (typically onAudioDataState, defined below).
    void setOnAudioData(AudioDataCallbackFn fn, void* state);
    
    // Set overflow callback.
    // Callback must take the following parameters:
    //    1. Audio device.
    //    2. Pointer to user-provided state object (typically onAudioOverflowState, defined below).
    void setOnAudioOverflow(AudioOverflowCallbackFn fn, void* state);

    // Set underflow callback.
    // Callback must take the following parameters:
    //    1. Audio device.
    //    2. Pointer to user-provided state object (typically onAudioUnderflowState, defined below).
    void setOnAudioUnderflow(AudioUnderflowCallbackFn fn, void* state);
    
    // Set error callback.
    // Callback must take the following parameters:
    //    1. Audio device.
    //    2. String representing the error encountered.
    //    3. Pointer to user-provided state object (typically onAudioErrorState, defined below).
    void setOnAudioError(AudioErrorCallbackFn fn, void* state);
    
    // Set device changed callback.
    // Callback must take the following parameters:
    //    1. Audio device.
    //    2. String representing the new name of the device.
    //    3. Pointer to user-provided state object (typically onAudioDeviceChangedState, defined below).
    void setOnAudioDeviceChanged(AudioDeviceChangedCallbackFn fn, void* state);
    
protected:
    std::string description;

    AudioDataCallbackFn onAudioDataFunction;
    void* onAudioDataState;
    
    AudioOverflowCallbackFn onAudioOverflowFunction;
    void* onAudioOverflowState;
    
    AudioUnderflowCallbackFn onAudioUnderflowFunction;
    void* onAudioUnderflowState;
    
    AudioErrorCallbackFn onAudioErrorFunction;
    void* onAudioErrorState;
    
    AudioDeviceChangedCallbackFn onAudioDeviceChangedFunction;
    void* onAudioDeviceChangedState;
};

#endif // I_AUDIO_DEVICE_H
