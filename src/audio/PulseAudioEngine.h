//=========================================================================
// Name:            PulseAudioEngine.h
// Purpose:         Defines the interface to the PortAudio audio engine.
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

#ifndef PULSE_AUDIO_ENGINE_H
#define PULSE_AUDIO_ENGINE_H

#include <pulse/pulseaudio.h>
#include "IAudioEngine.h"

class PulseAudioEngine : public IAudioEngine
{
public:
    PulseAudioEngine();
    virtual ~PulseAudioEngine();
    
    virtual void start();
    virtual void stop();
    virtual std::vector<AudioDeviceSpecification> getAudioDeviceList(AudioDirection direction);
    virtual AudioDeviceSpecification getDefaultAudioDevice(AudioDirection direction);
    virtual std::shared_ptr<IAudioDevice> getAudioDevice(wxString deviceName, AudioDirection direction, int sampleRate, int numChannels);
    virtual std::vector<int> getSupportedSampleRates(wxString deviceName, AudioDirection direction);
    
private:
    bool initialized_;
    pa_threaded_mainloop *mainloop_;
    pa_mainloop_api *mainloopApi_;
    pa_context* context_;
};

#endif // PULSE_AUDIO_ENGINE_H
