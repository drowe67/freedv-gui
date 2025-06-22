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

#include <thread>
#include <dispatch/dispatch.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>

#include "../util/ThreadedObject.h"
#include "IAudioEngine.h"
#include "IAudioDevice.h"

class MacAudioEngine;

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
    virtual void stopRealTimeWork(bool fastMode = false) override;

    // Reverts real-time priority for current thread.
    virtual void clearHelperRealTime() override;

protected:
    friend class MacAudioEngine;
    
    MacAudioDevice(MacAudioEngine* parent, std::string deviceName, int coreAudioId, IAudioEngine::AudioDirection direction, int numChannels, int sampleRate);
    
private:
    int coreAudioId_;
    IAudioEngine::AudioDirection direction_;
    int numChannels_;
    int sampleRate_;
    void* engine_; // actually AVAudioEngine but this file is shared with C++ code
    void* player_; // actually AVAudioPlayerNode
    std::string deviceName_;
    void* observer_;
    short* inputFrames_;
    dispatch_semaphore_t sem_;
    bool isDefaultDevice_;
    MacAudioEngine* parent_;

    void joinWorkgroup_();
    void leaveWorkgroup_();

    static thread_local void* Workgroup_;
    static thread_local void* JoinToken_;
    static thread_local int CurrentCoreAudioId_;

    static int DeviceIsAliveCallback_(
        AudioObjectID                       inObjectID,
        UInt32                              inNumberAddresses,
        const AudioObjectPropertyAddress    inAddresses[],
        void*                               inClientData);

    static int DeviceOverloadCallback_(
        AudioObjectID                       inObjectID,
        UInt32                              inNumberAddresses,
        const AudioObjectPropertyAddress    inAddresses[],
        void*                               inClientData);
};

#endif // MAC_AUDIO_DEVICE_H
