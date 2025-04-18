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
    
    // Configures current thread for real-time priority. This should be
    // called from the thread that will be operating on received audio.
    virtual void setHelperRealTime() override;

    // Lets audio system know that we're beginning to do work with the
    // received audio.
    virtual void startRealTimeWork() override;

    // Lets audio system know that we're done with the work on the received
    // audio.
    virtual void stopRealTimeWork() override;

    // Reverts real-time priority for current thread.
    virtual void clearHelperRealTime() override;

protected:
    friend class MacAudioEngine;
    
    MacAudioDevice(int coreAudioId, IAudioEngine::AudioDirection direction, int numChannels, int sampleRate);
    
private:
    const double FRAME_TIME_SEC_ = 0.01;
    
    int coreAudioId_;
    IAudioEngine::AudioDirection direction_;
    int numChannels_;
    int sampleRate_;
    void* engine_; // actually AVAudioEngine but this file is shared with C++ code
    void* player_; // actually AVAudioPlayerNode
    
    void* workgroup_;
    void* joinToken_;
};

#endif // MAC_AUDIO_DEVICE_H
