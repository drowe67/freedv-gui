//=========================================================================
// Name:            IAudioEngine.h
// Purpose:         Defines the main interface to the selected audio engine.
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

#ifndef I_AUDIO_ENGINE_H
#define I_AUDIO_ENGINE_H

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "AudioDeviceSpecification.h"

class IAudioDevice;

class IAudioEngine
{
public:
    typedef std::function<void(IAudioEngine&, std::string, void*)> AudioErrorCallbackFn;
    
    enum AudioDirection { AUDIO_ENGINE_IN, AUDIO_ENGINE_OUT };
    
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual std::vector<AudioDeviceSpecification> getAudioDeviceList(AudioDirection direction) = 0;
    virtual AudioDeviceSpecification getDefaultAudioDevice(AudioDirection direction) = 0;
    virtual std::shared_ptr<IAudioDevice> getAudioDevice(wxString deviceName, AudioDirection direction, int sampleRate, int numChannels, bool exclusive) = 0;
    virtual std::vector<int> getSupportedSampleRates(wxString deviceName, AudioDirection direction) = 0;
    
    // Set error callback.
    // Callback must take the following parameters:
    //    1. Audio engine.
    //    2. String representing the error encountered.
    //    3. Pointer to user-provided state object (typically onAudioUnderflowState, defined below).
    void setOnEngineError(AudioErrorCallbackFn fn, void* state);
    
protected:
    static int StandardSampleRates[];
    
    AudioErrorCallbackFn onAudioErrorFunction;
    void* onAudioErrorState;
};

#endif // AUDIO_ENGINE_H