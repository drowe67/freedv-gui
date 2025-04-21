//=========================================================================
// Name:            MacAudioDevice.h
// Purpose:         Defines the interface to a Core Audio device.
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

#ifndef MAC_AUDIO_DEVICE_H
#define MAC_AUDIO_DEVICE_H

#include "../util/ThreadedObject.h"
#include "IAudioEngine.h"
#include "IAudioDevice.h"

class MacAudioDevice : public ThreadedObject, public IAudioDevice
{
public:
    virtual ~MacAudioDevice();
    
    virtual int getNumChannels() override;
    virtual int getSampleRate() const override;
    
    virtual void start() override;
    virtual void stop() override;

    virtual bool isRunning() override;
    
    virtual int getLatencyInMicroseconds() override;

protected:
    friend class MacAudioEngine;
    
    MacAudioDevice(int coreAudioId, IAudioEngine::AudioDirection direction, int numChannels, int sampleRate);
    
private:
    int coreAudioId_;
    IAudioEngine::AudioDirection direction_;
    int numChannels_;
    int sampleRate_;
    void* engine_; // actually AVAudioEngine but this file is shared with C++ code
    void* player_; // actually AVAudioPlayerNode

    short* inputFrames_;
};

#endif // MAC_AUDIO_DEVICE_H
