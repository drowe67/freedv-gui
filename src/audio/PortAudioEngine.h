//=========================================================================
// Name:            PortAudioEngine.h
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

#ifndef PORT_AUDIO_ENGINE_H
#define PORT_AUDIO_ENGINE_H

#include <memory>
#include "IAudioEngine.h"
#include "PortAudioInterface.h"

class PortAudioEngine : public IAudioEngine
{
public:
    PortAudioEngine();
    virtual ~PortAudioEngine();
    
    virtual void start();
    virtual void stop();
    virtual std::vector<AudioDeviceSpecification> getAudioDeviceList(AudioDirection direction);
    virtual AudioDeviceSpecification getDefaultAudioDevice(AudioDirection direction);
    virtual std::shared_ptr<IAudioDevice> getAudioDevice(wxString deviceName, AudioDirection direction, int sampleRate, int numChannels, bool exclusive);
    virtual std::vector<int> getSupportedSampleRates(wxString deviceName, AudioDirection direction);
    
private:
    bool initialized_;
    std::shared_ptr<PortAudioInterface> portAudioLibrary_;

    static bool IsDeviceWhitelisted_(const char* devName);
};

#endif // PORT_AUDIO_ENGINE_H