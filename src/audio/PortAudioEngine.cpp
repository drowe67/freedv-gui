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

#include <sstream>
#include <wx/string.h>
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
        std::string errText = Pa_GetErrorText(error);
        if (error == paUnanticipatedHostError)
        {
            std::stringstream ss;
            auto errInfo = Pa_GetLastHostErrorInfo();
            ss << " (error code " << std::hex << errInfo->errorCode << " - " << std::string(errInfo->errorText) << ")";
            errText += ss.str();
        }

        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, errText, onAudioErrorState);
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
            hostApiName.find("surround") != std::string::npos ||
            //hostApiName.find("Windows WASAPI") != std::string::npos ||
            hostApiName.find("MME") != std::string::npos ||
            hostApiName.find("Windows WDM-KS") != std::string::npos)
        {
            // Skip non-MME/Core Audio devices as that was the old behavior.
            // Note: DirectSound in particular hasn't been shown 
            //       due to poor real-time performance.
            continue;
        }
        
        if ((direction == AUDIO_ENGINE_IN && deviceInfo->maxInputChannels > 0) || 
            (direction == AUDIO_ENGINE_OUT && deviceInfo->maxOutputChannels > 0))
        {
            // Detect the minimum number of channels available as PortAudio doesn't
            // provide this info. This should in theory be 1 but at least one device 
            // (Focusrite Scarlett) will not accept anything less than 4 channels 
            // on Windows.
            PaStreamParameters streamParameters;
            streamParameters.device = index;
            streamParameters.channelCount = 1; 
            streamParameters.sampleFormat = paInt16;
            streamParameters.suggestedLatency = Pa_GetDeviceInfo(index)->defaultHighInputLatency;
            streamParameters.hostApiSpecificStreamInfo = NULL;
            
            int maxChannels = direction == AUDIO_ENGINE_IN ? deviceInfo->maxInputChannels : deviceInfo->maxOutputChannels;
            bool isDeviceWithKnownMinimum = 
                !strcmp(deviceInfo->name, "sysdefault") ||            
                !strcmp(deviceInfo->name, "front") ||            
                !strcmp(deviceInfo->name, "surround") ||            
                !strcmp(deviceInfo->name, "samplerate") ||            
                !strcmp(deviceInfo->name, "speexrate") ||            
                !strcmp(deviceInfo->name, "pulse") ||            
                !strcmp(deviceInfo->name, "upmix") ||            
                !strcmp(deviceInfo->name, "vdownmix") ||            
                !strcmp(deviceInfo->name, "dmix") ||            
                !strcmp(deviceInfo->name, "default");
            if (!isDeviceWithKnownMinimum)
            {
                while (streamParameters.channelCount < maxChannels)
                {
                    PaError err = Pa_IsFormatSupported(
                        direction == AUDIO_ENGINE_IN ? &streamParameters : NULL, 
                        direction == AUDIO_ENGINE_OUT ? &streamParameters : NULL, 
                        deviceInfo->defaultSampleRate);
                
                    if (err == paFormatIsSupported)
                    {
                        break;
                    }

                    streamParameters.channelCount++;
                }
            }

            // Add information about this device to the result array.
            AudioDeviceSpecification device;
            device.deviceId = index;
            device.name = wxString::FromUTF8(deviceInfo->name);
            device.apiName = hostApiName;
            device.minChannels = streamParameters.channelCount;
            device.maxChannels = 
                direction == AUDIO_ENGINE_IN ? deviceInfo->maxInputChannels : deviceInfo->maxOutputChannels;
            device.defaultSampleRate = deviceInfo->defaultSampleRate;
            
            result.push_back(device);
        }
    }
    
    return result;
}

std::vector<int> PortAudioEngine::getSupportedSampleRates(wxString deviceName, AudioDirection direction)
{
    std::vector<int> result;
    auto devInfo = getAudioDeviceList(direction);
    
    for (auto& device : devInfo)
    {
        if (device.name.Find(deviceName) == 0)
        {
            PaStreamParameters streamParameters;
            
            streamParameters.device = device.deviceId;
            streamParameters.channelCount = device.minChannels;
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

std::shared_ptr<IAudioDevice> PortAudioEngine::getAudioDevice(wxString deviceName, AudioDirection direction, int sampleRate, int numChannels)
{
    auto deviceList = getAudioDeviceList(direction);
    
    auto supportedSampleRates = getSupportedSampleRates(deviceName, direction);
    bool found = false;
    for (auto& rate : supportedSampleRates)
    {
        if (rate == sampleRate)
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        // Zero out the input sample rate. The device object will use the default sample rate
        // instead.
        sampleRate = 0;
    }
    
    for (auto& dev : deviceList)
    {
        if (dev.name.Find(deviceName) == 0)
        {
            // Ensure that the passed-in number of channels is within the allowed range.
            numChannels = std::max(numChannels, dev.minChannels);
            numChannels = std::min(numChannels, dev.maxChannels);

            // Create device object.
            auto devObj = new PortAudioDevice(dev.deviceId, direction, sampleRate, numChannels);
            return std::shared_ptr<IAudioDevice>(devObj);
        }
    }

    return nullptr;
}
