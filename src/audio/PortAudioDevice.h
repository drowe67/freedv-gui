//=========================================================================
// Name:            PortAudioDevice.h
// Purpose:         Defines the interface to a PortAudio device.
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

#ifndef PORT_AUDIO_DEVICE_H
#define PORT_AUDIO_DEVICE_H

#include "portaudio.h"
#include "IAudioEngine.h"
#include "IAudioDevice.h"
#include "PortAudioInterface.h"

class PortAudioDevice : public IAudioDevice
{
public:
    virtual ~PortAudioDevice();
    
    virtual int getNumChannels() override { return numChannels_; }
    virtual int getSampleRate() const override { return sampleRate_; }
    
    virtual void start() override;
    virtual void stop() override;

    virtual bool isRunning() override;
    
protected:
    // PortAudioDevice cannot be created directly, only via PortAudioEngine.
    friend class PortAudioEngine;
    
    PortAudioDevice(std::shared_ptr<PortAudioInterface> library, int deviceId, IAudioEngine::AudioDirection direction, int sampleRate, int numChannels, bool exclusive);
    
private:
    int deviceId_;
    IAudioEngine::AudioDirection direction_;
    int sampleRate_;
    int numChannels_;
    PaStream* deviceStream_;
    std::shared_ptr<PortAudioInterface> portAudioLibrary_;
    bool isExclusive_;
    
    static int OnPortAudioStreamCallback_(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData);
};

#endif // PORT_AUDIO_DEVICE_H