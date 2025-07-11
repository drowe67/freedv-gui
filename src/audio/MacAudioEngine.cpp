//=========================================================================
// Name:            MacAudioEngine.cpp
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

#include <sstream>

#include "MacAudioEngine.h"
#include "MacAudioDevice.h"
#include "../util/logging/ulog.h"

#include <CoreAudio/CoreAudio.h>

static const int kAdmMaxDeviceNameSize = 128;

void MacAudioEngine::start()
{
    // "Undocumented" call that's supposedly required for queries/changes to properly
    // occur. Might not actually be needed but doesn't hurt to keep it.
    CFRunLoopRef theRunLoop = NULL;
    AudioObjectPropertyAddress property = { 
        kAudioHardwarePropertyRunLoop,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster 
    };
    OSStatus result = AudioObjectSetPropertyData( 
        kAudioObjectSystemObject, &property, 0, NULL, 
        sizeof(CFRunLoopRef), &theRunLoop);
    if (result != noErr)
    {
        log_warn("Could not set run loop -- audio device changes by system may break things");
    }

    // Add listener for device configuration changes. This is so that we can restart
    // devices and keep things working. 
    property.mSelector = kAudioHardwarePropertyDevices;
    result = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &property, OnDeviceListChange_, this);
    if (result != noErr)
    {
        std::stringstream ss;
        ss << "Could not set device configuration listener (err " << result << ")";
        log_warn(ss.str().c_str());
    }
}

void MacAudioEngine::stop()
{
    AudioObjectPropertyAddress property = { 
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster 
    };
    AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &property, OnDeviceListChange_, this);
}

std::vector<AudioDeviceSpecification> MacAudioEngine::getAudioDeviceList(AudioDirection direction)
{
    std::vector<AudioDeviceSpecification> result;
    
    // Tells the system where to find audio devices.
    AudioObjectPropertyAddress propertyAddress = {
        .mSelector = kAudioHardwarePropertyDevices,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };
    
    uint32_t propertySize = 0;
    OSStatus status = noErr;
    
    // Determine the number of audio devices in the system.
    status = AudioObjectGetPropertyDataSize(
        kAudioObjectSystemObject,
        &propertyAddress,
        0,
        nullptr,
        &propertySize
    );
    if (status != noErr)
    {
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, "Could not get number of audio devices", onAudioErrorState);
        }
        return result;
    }
    
    // Get audio device IDs.
    auto deviceCount = propertySize / sizeof(AudioDeviceID);
    AudioDeviceID* ids = new AudioDeviceID[deviceCount];
    assert(ids != nullptr);

    status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &propertyAddress,
        0,
        nullptr,
        &propertySize,
        ids
    );
    if (status != noErr)
    {
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, "Could not get audio device IDs", onAudioErrorState);
        }

        delete[] ids;
        return result;
    }
    
    for (auto index = 0; index < deviceCount; index++)
    {
        // Get device ID
        auto deviceID = ids[index];
        auto device = getAudioSpecification_(deviceID, direction);
        
        if (device.isValid())
        {
            result.push_back(device);
        }
    }
    
    delete[] ids;
    return result;
}

AudioDeviceSpecification MacAudioEngine::getDefaultAudioDevice(AudioDirection direction)
{
    AudioObjectPropertyAddress propertyAddress = {
        .mSelector = kAudioHardwarePropertyDevices,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };
    
    AudioDeviceID deviceId = 0;
    uint32_t propertySize = sizeof(deviceId);
    OSStatus status = noErr;
    
    propertyAddress.mSelector = 
        (direction == AUDIO_ENGINE_IN) ?
        kAudioHardwarePropertyDefaultInputDevice :
        kAudioHardwarePropertyDefaultOutputDevice;
        
    status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &propertyAddress,
        0,
        nullptr,
        &propertySize,
        &deviceId
    );
    if (status != noErr)
    {
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, "Could not get default audio device", onAudioErrorState);
        }
        return AudioDeviceSpecification::GetInvalidDevice();
    }
    
    return getAudioSpecification_(deviceId, direction);
}

std::shared_ptr<IAudioDevice> MacAudioEngine::getAudioDevice(wxString deviceName, AudioDirection direction, int sampleRate, int numChannels)
{
    for (auto& dev : getAudioDeviceList(direction))
    {
        if (dev.name == deviceName)
        {
            // Make sure provided sample rate is valid. If not valid,
            // just use the device's default sample rate.
            auto sampleRates = getSupportedSampleRates(deviceName, direction);
            bool found = false;
            for (auto& rate : sampleRates)
            {
                if (rate == sampleRate)
                {
                    found = true;
                    break;
                }
            }
            
            if (!found)
            {
                sampleRate = dev.defaultSampleRate;
            }
            
            // Ensure that the passed-in number of channels is within the allowed range.
            numChannels = std::max(numChannels, dev.minChannels);
            numChannels = std::min(numChannels, dev.maxChannels);
            
            auto devPtr = new MacAudioDevice(this, (const char*)dev.name.ToUTF8(), dev.deviceId, direction, numChannels, sampleRate);
            return std::shared_ptr<IAudioDevice>(devPtr);
        }
    }
    return nullptr;
}

std::vector<int> MacAudioEngine::getSupportedSampleRates(wxString deviceName, AudioDirection direction)
{
    std::vector<int> result;
    
    for (auto& dev : getAudioDeviceList(direction))
    {
        if (dev.name == deviceName)
        {
#if 0
            AudioObjectPropertyAddress propertyAddress = {
                .mSelector = kAudioDevicePropertyAvailableNominalSampleRates,
                .mScope = kAudioObjectPropertyScopeGlobal,
                .mElement = kAudioObjectPropertyElementMain
            };
            
            propertyAddress.mScope = 
                (direction == AUDIO_ENGINE_IN) ?
                kAudioDevicePropertyScopeInput :
                kAudioDevicePropertyScopeOutput;
    
            uint32_t propertySize = 0;
            OSStatus status = noErr;
    
            status = AudioObjectGetPropertyDataSize(
                dev.deviceId,
                &propertyAddress,
                0,
                nullptr,
                &propertySize
            );
            if (status != noErr)
            {
                if (onAudioErrorFunction)
                {
                    onAudioErrorFunction(*this, "Could not get size of sample rate list", onAudioErrorState);
                }
                return result;
            }

            // Get audio device IDs.
            auto rateCount = propertySize / sizeof(AudioValueRange);
            AudioValueRange* rates = (AudioValueRange*)malloc(propertySize);
            if (rates == nullptr)
            {
                if (onAudioErrorFunction)
                {
                    onAudioErrorFunction(*this, "Could not allocate memory for sample rate list", onAudioErrorState);
                }
                return result;
            }
            
            status = AudioObjectGetPropertyData(
                dev.deviceId,
                &propertyAddress,
                0,
                nullptr,
                &propertySize,
                rates
            );
            if (status != noErr)
            {
                if (onAudioErrorFunction)
                {
                    onAudioErrorFunction(*this, "Could not get supported sample rates", onAudioErrorState);
                }
                return result;
            }
            
            for (int index = 0; index < rateCount; index++)
            {
                for (int rate = rates[index].mMinimum; rate <= rates[index].mMaximum; rate++)
                {
                    result.push_back(rate);
                }
            }
            
            free(rates);
#endif // 0
            result.push_back(dev.defaultSampleRate);
            break;
        }
    }
    
    return result;
}

AudioDeviceSpecification MacAudioEngine::getAudioSpecification_(int coreAudioId, AudioDirection direction)
{
    AudioObjectPropertyAddress propertyAddress = {
        .mSelector = kAudioHardwarePropertyDevices,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };
    
    uint32_t propertySize = 0;
    OSStatus status = noErr;
    
    // Get device name
    propertyAddress.mSelector = kAudioDevicePropertyDeviceName;

    char name[kAdmMaxDeviceNameSize];
    propertySize = sizeof(name);

    status = AudioObjectGetPropertyData(
        coreAudioId,
        &propertyAddress,
        0,
        nullptr,
        &propertySize,
        &name
    );
    if (status != noErr)
    {
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, "Could not get audio device name", onAudioErrorState);
        }
        return AudioDeviceSpecification::GetInvalidDevice();
    }
    
    std::string deviceName = name;
    
    // Get HW sample rate
    double sampleRate = 0;
    propertyAddress.mSelector = kAudioDevicePropertyNominalSampleRate;
    propertySize = sizeof(sampleRate);
    status = AudioObjectGetPropertyData(
        coreAudioId,
        &propertyAddress,
        0,
        nullptr,
        &propertySize,
        &sampleRate
    );
    if (status != noErr)
    {
        // Use fallback SR if we can't retrieve it for some reason.
        sampleRate = 44100;
    }
    
    // Get number of input and output channels
    int numChannels = getNumChannels_(coreAudioId, direction);
    if (numChannels == 0)
    {
        // problem getting number of channels or device doesn't support 
        // given direction.
        return AudioDeviceSpecification::GetInvalidDevice();
    }
    
    // Add to device list.
    AudioDeviceSpecification device;
    device.deviceId = coreAudioId;
    device.name = wxString::FromUTF8(deviceName);
    device.cardName = device.name;
    device.apiName = "Core Audio";
    device.maxChannels = numChannels;
    device.minChannels = 1;
    device.defaultSampleRate = sampleRate;
    
    return device;
}

int MacAudioEngine::getNumChannels_(int coreAudioId, AudioDirection direction)
{
    AudioObjectPropertyAddress propertyAddress = {
        .mSelector = kAudioHardwarePropertyDevices,
        .mScope = kAudioObjectPropertyScopeGlobal,
        .mElement = kAudioObjectPropertyElementMain
    };
    
    uint32_t propertySize = 0;
    OSStatus status = noErr;
    
    propertyAddress.mSelector = kAudioDevicePropertyStreamConfiguration;
    propertyAddress.mScope = 
        (direction == AUDIO_ENGINE_IN) ?
        kAudioDevicePropertyScopeInput :
        kAudioDevicePropertyScopeOutput;
        
    status = AudioObjectGetPropertyDataSize(
        coreAudioId, 
        &propertyAddress, 
        0, 
        nullptr, 
        &propertySize);
    if (status != noErr)
    {
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, "Could not get audio stream configuration size", onAudioErrorState);
        }
        return 0;
    }
    
    AudioBufferList bufferListPointer;
    status = AudioObjectGetPropertyData(
        coreAudioId, 
        &propertyAddress, 
        0,
        nullptr, 
        &propertySize, 
        &bufferListPointer);
    if (status != noErr)
    {
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, "Could not get audio stream configuration", onAudioErrorState);
        }
        return 0;
    }
    
    int numChannels = 0;
    for (int index = 0; index < bufferListPointer.mNumberBuffers; index++)
    {
        numChannels += bufferListPointer.mBuffers[index].mNumberChannels;
    }
    
    return numChannels;
}

int MacAudioEngine::OnDeviceListChange_(
    AudioObjectID                       inObjectID,
    UInt32                              inNumberAddresses,
    const AudioObjectPropertyAddress    inAddresses[],
    void*                               inClientData)
{
    #if 0
    MacAudioEngine* thisObj = (MacAudioEngine*)inClientData;
    log_info("Detected device list change--restarting devices to keep audio flowing");
    thisObj->requestRestart_();
    #endif // 0
    return noErr;
}
