//=========================================================================
// Name:            MacAudioEngine.h
// Purpose:         Defines the main interface to Core Audio.
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

#ifndef MAC_AUDIO_ENGINE_H
#define MAC_AUDIO_ENGINE_H

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreAudio/CoreAudio.h>
#include "IAudioEngine.h"

class IAudioDevice;

class MacAudioEngine : public IAudioEngine
{
public:
    MacAudioEngine() = default;
    virtual ~MacAudioEngine() = default;
    
    virtual void start() override;
    virtual void stop() override;
    virtual std::vector<AudioDeviceSpecification> getAudioDeviceList(AudioDirection direction) override;
    virtual AudioDeviceSpecification getDefaultAudioDevice(AudioDirection direction) override;
    virtual std::shared_ptr<IAudioDevice> getAudioDevice(wxString deviceName, AudioDirection direction, int sampleRate, int numChannels) override;
    virtual std::vector<int> getSupportedSampleRates(wxString deviceName, AudioDirection direction) override;
    
private:    
    friend class MacAudioDevice;

    AudioDeviceSpecification getAudioSpecification_(int coreAudioId, AudioDirection direction);
    int getNumChannels_(int coreAudioId, AudioDirection direction);

    void requestRestart_();
    void register_(IAudioDevice* device);
    void unregister_(IAudioDevice* device);

    std::vector<IAudioDevice*> activeDevices_;
    std::recursive_mutex activeDeviceMutex_;

    static int OnDeviceListChange_(
        AudioObjectID                       inObjectID,
        UInt32                              inNumberAddresses,
        const AudioObjectPropertyAddress    inAddresses[],
        void*                               inClientData);
};

#endif // MAC_AUDIO_ENGINE_H
