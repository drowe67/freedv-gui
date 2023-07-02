//==========================================================================
// Name:            AudioConfiguration.cpp
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

#include "AudioConfiguration.h"

template<>
const char* AudioConfiguration::AudioDeviceConfigName<1, AudioConfiguration::IN>::GetDeviceNameConfigName()
{
    return "/Audio/soundCard1InDeviceName";
}

template<>
const char* AudioConfiguration::AudioDeviceConfigName<1, AudioConfiguration::OUT>::GetDeviceNameConfigName()
{
    return "/Audio/soundCard1OutDeviceName";
}

template<>
const char* AudioConfiguration::AudioDeviceConfigName<2, AudioConfiguration::IN>::GetDeviceNameConfigName()
{
    return "/Audio/soundCard2InDeviceName";
}

template<>
const char* AudioConfiguration::AudioDeviceConfigName<2, AudioConfiguration::OUT>::GetDeviceNameConfigName()
{
    return "/Audio/soundCard2OutDeviceName";
}

template<>
const char* AudioConfiguration::AudioDeviceConfigName<1, AudioConfiguration::IN>::GetSampleRateConfigName()
{
    return "/Audio/soundCard1InSampleRate";
}

template<>
const char* AudioConfiguration::AudioDeviceConfigName<1, AudioConfiguration::OUT>::GetSampleRateConfigName()
{
    return "/Audio/soundCard1OutSampleRate";
}

template<>
const char* AudioConfiguration::AudioDeviceConfigName<2, AudioConfiguration::IN>::GetSampleRateConfigName()
{
    return "/Audio/soundCard2InSampleRate";
}

template<>
const char* AudioConfiguration::AudioDeviceConfigName<2, AudioConfiguration::OUT>::GetSampleRateConfigName()
{
    return "/Audio/soundCard2OutSampleRate";
}

void AudioConfiguration::load(wxConfigBase* config)
{
    // Migration -- grab old sample rates from older FreeDV configuration
    int oldSoundCard1SampleRate = config->Read(wxT("/Audio/soundCard1SampleRate"), -1);
    int oldSoundCard2SampleRate = config->Read(wxT("/Audio/soundCard2SampleRate"), -1);
    soundCard1In.sampleRate.setDefaultVal(oldSoundCard1SampleRate);
    soundCard1Out.sampleRate.setDefaultVal(oldSoundCard1SampleRate);
    soundCard2In.sampleRate.setDefaultVal(oldSoundCard2SampleRate);
    soundCard2Out.sampleRate.setDefaultVal(oldSoundCard2SampleRate);
    
    soundCard1In.load(config);
    soundCard1Out.load(config);
    soundCard2In.load(config);
    soundCard2Out.load(config);
}

void AudioConfiguration::save(wxConfigBase* config)
{
    soundCard1In.save(config);
    soundCard1Out.save(config);
    soundCard2In.save(config);
    soundCard2Out.save(config);
}