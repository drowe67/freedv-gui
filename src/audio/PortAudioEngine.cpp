//=========================================================================
// Name:            PortAudioEngine.cpp
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

#include "portaudio.h"
#include "PortAudioDevice.h"
#include "PortAudioEngine.h"

PortAudioEngine::PortAudioEngine()
    : initialized_(false)
{
    // empty
}

PortAudioEngine::~PortAudioEngine()
{
    if (initialized_)
    {
        stop();
    }
}

void PortAudioEngine::start()
{
    auto error = Pa_Initialize();
    if (error != paNoError)
    {
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, Pa_GetErrorText(error), onAudioErrorState);
        }
    }
    else
    {
        initialized_ = true;
    }
}

void PortAudioEngine::stop()
{
    Pa_Terminate();
    initialized_ = false;
}

std::vector<AudioDeviceSpecification> PortAudioEngine::getAudioDeviceList(AudioDirection direction)
{
    int numDevices = Pa_GetDeviceCount();
    std::vector<AudioDeviceSpecification> result;
    
    for (int index = 0; index < numDevices; index++)
    {
        const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(index);
        
        std::string hostApiName = Pa_GetHostApiInfo(deviceInfo->hostApi)->name;
        if (hostApiName.find("DirectSound") != std::string::npos ||
            hostApiName.find("surround") != std::string::npos)
        {
            // Skip DirectSound devices due to poor real-time performance.
            continue;
        }
        
        if ((direction == AUDIO_ENGINE_IN && deviceInfo->maxInputChannels > 0) || 
            (direction == AUDIO_ENGINE_OUT && deviceInfo->maxOutputChannels > 0))
        {
            AudioDeviceSpecification device;
            device.deviceId = index;
            device.name = deviceInfo->name;
            device.apiName = hostApiName;
            device.maxChannels = 
                direction == AUDIO_ENGINE_IN ? deviceInfo->maxInputChannels : deviceInfo->maxOutputChannels;
            device.defaultSampleRate = deviceInfo->defaultSampleRate;
            
            result.push_back(device);
        }
    }
    
    return result;
}

std::vector<int> PortAudioEngine::getSupportedSampleRates(std::string deviceName, AudioDirection direction)
{
    std::vector<int> result;
    auto devInfo = getAudioDeviceList(direction);
    
    for (auto& device : devInfo)
    {
        if (deviceName == device.name)
        {
            PaStreamParameters streamParameters;
            
            streamParameters.device = device.deviceId;
            streamParameters.channelCount = 1;
            streamParameters.sampleFormat = paInt16;
            streamParameters.suggestedLatency = Pa_GetDeviceInfo(device.deviceId)->defaultHighInputLatency;
            streamParameters.hostApiSpecificStreamInfo = NULL;
            
            int rateIndex = 0;
            while (IAudioEngine::StandardSampleRates[rateIndex] != -1)
            {
                PaError err = Pa_IsFormatSupported(
                    direction == AUDIO_ENGINE_IN ? &streamParameters : NULL, 
                    direction == AUDIO_ENGINE_OUT ? &streamParameters : NULL, 
                    IAudioEngine::StandardSampleRates[rateIndex]);
                
                if (err == paFormatIsSupported)
                {
                    result.push_back(IAudioEngine::StandardSampleRates[rateIndex]);
                }
                
                rateIndex++;
            }            
        }
    }
    
    return result;
}

AudioDeviceSpecification PortAudioEngine::getDefaultAudioDevice(AudioDirection direction)
{
    auto devices = getAudioDeviceList(direction);
    PaDeviceIndex defaultDeviceIndex = 
        direction == AUDIO_ENGINE_IN ? Pa_GetDefaultInputDevice() : Pa_GetDefaultOutputDevice();
    
    if (defaultDeviceIndex != paNoDevice)
    {
        for (auto& device : devices)
        {
            if (device.deviceId == defaultDeviceIndex)
            {
                return device;
            }
        }
    }
    
    return AudioDeviceSpecification::GetInvalidDevice();
}

std::shared_ptr<IAudioDevice> PortAudioEngine::getAudioDevice(std::string deviceName, AudioDirection direction, int sampleRate, int numChannels)
{
    auto deviceList = getAudioDeviceList(direction);
    
    for (auto& dev : deviceList)
    {
        if (dev.name == deviceName)
        {
            auto devObj = new PortAudioDevice(dev.deviceId, direction, sampleRate, dev.maxChannels >= numChannels ? numChannels : dev.maxChannels);
            return std::shared_ptr<IAudioDevice>(devObj);
        }
    }

    return nullptr;
}