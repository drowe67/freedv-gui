//==========================================================================
// Name:            AudioConfiguration.h
// Purpose:         Implements the audio device configuration for FreeDV
// Created:         July 2, 2023
// Authors:         Mooneer Salem
// 
// License:
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
//==========================================================================

#ifndef AUDIO_CONFIGURATION_H
#define AUDIO_CONFIGURATION_H

#include "WxWidgetsConfigStore.h"
#include "ConfigurationDataElement.h"

class AudioConfiguration : public WxWidgetsConfigStore
{
public:
    enum Direction { IN, OUT };
    
    template<int DeviceId, AudioConfiguration::Direction AudioDirection>
    class AudioDeviceConfigName
    {
    public:
        static const char* GetDeviceNameConfigName();
        static const char* GetSampleRateConfigName();
    };
    
    template<int DeviceId, AudioConfiguration::Direction AudioDirection>
    class SoundDevice : public WxWidgetsConfigStore
    {
    public:
        SoundDevice();
        virtual ~SoundDevice() = default;
        
        ConfigurationDataElement<wxString> deviceName;
        ConfigurationDataElement<int> sampleRate;
        
        virtual void load(wxConfigBase* config) override;
        virtual void save(wxConfigBase* config) override;
    };
    
    AudioConfiguration() = default;
    virtual ~AudioConfiguration() = default;
    
    SoundDevice<1, AudioConfiguration::IN> soundCard1In;
    SoundDevice<1, AudioConfiguration::OUT> soundCard1Out;
    SoundDevice<2, AudioConfiguration::IN> soundCard2In;
    SoundDevice<2, AudioConfiguration::OUT> soundCard2Out;
    
    virtual void load(wxConfigBase* config) override;
    virtual void save(wxConfigBase* config) override;
};

template<int DeviceId, AudioConfiguration::Direction AudioDirection>
const char* AudioConfiguration::AudioDeviceConfigName<DeviceId, AudioDirection>::GetDeviceNameConfigName()
{
    assert(0);
}

template<>
const char* AudioConfiguration::AudioDeviceConfigName<1, AudioConfiguration::IN>::GetDeviceNameConfigName();

template<>
const char* AudioConfiguration::AudioDeviceConfigName<1, AudioConfiguration::OUT>::GetDeviceNameConfigName();

template<>
const char* AudioConfiguration::AudioDeviceConfigName<2, AudioConfiguration::IN>::GetDeviceNameConfigName();

template<>
const char* AudioConfiguration::AudioDeviceConfigName<2, AudioConfiguration::OUT>::GetDeviceNameConfigName();

template<int DeviceId, AudioConfiguration::Direction AudioDirection>
const char* AudioConfiguration::AudioDeviceConfigName<DeviceId, AudioDirection>::GetSampleRateConfigName()
{
    assert(0);
}

template<>
const char* AudioConfiguration::AudioDeviceConfigName<1, AudioConfiguration::IN>::GetSampleRateConfigName();

template<>
const char* AudioConfiguration::AudioDeviceConfigName<1, AudioConfiguration::OUT>::GetSampleRateConfigName();

template<>
const char* AudioConfiguration::AudioDeviceConfigName<2, AudioConfiguration::IN>::GetSampleRateConfigName();

template<>
const char* AudioConfiguration::AudioDeviceConfigName<2, AudioConfiguration::OUT>::GetSampleRateConfigName();

template<int DeviceId, AudioConfiguration::Direction AudioDirection>
AudioConfiguration::SoundDevice<DeviceId, AudioDirection>::SoundDevice()
    : deviceName(AudioConfiguration::AudioDeviceConfigName<DeviceId, AudioDirection>::GetDeviceNameConfigName(), _("none"))
    , sampleRate(AudioConfiguration::AudioDeviceConfigName<DeviceId, AudioDirection>::GetSampleRateConfigName(), -1)
{
    // empty
}

template<int DeviceId, AudioConfiguration::Direction AudioDirection>
void AudioConfiguration::SoundDevice<DeviceId, AudioDirection>::load(wxConfigBase* config)
{
    load_(config, deviceName);
    load_(config, sampleRate);
}

template<int DeviceId, AudioConfiguration::Direction AudioDirection>
void AudioConfiguration::SoundDevice<DeviceId, AudioDirection>::save(wxConfigBase* config)
{
    save_(config, deviceName);
    save_(config, sampleRate);
}

#endif // AUDIO_CONFIGURATION_H