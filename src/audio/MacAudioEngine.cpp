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

#include "MacAudioEngine.h"
#include "MacAudioDevice.h"
#include "../util/logging/ulog.h"

#include <CoreAudio/CoreAudio.h>

void MacAudioEngine::start()
{
    // empty - no initialization needed.
}

void MacAudioEngine::stop()
{
    // empty - no teardown needed.
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
    AudioDeviceID ids[deviceCount];
    status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &propertyAddress,
        0,
        nullptr,
        &propertySize,
        &ids
    );
    if (status != noErr)
    {
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, "Could not get audio device IDs", onAudioErrorState);
        }
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
    
    return result;
}

AudioDeviceSpecification MacAudioEngine::getDefaultAudioDevice(AudioDirection direction)
{
    return AudioDeviceSpecification::GetInvalidDevice(); // TBD - stub
}

std::shared_ptr<IAudioDevice> MacAudioEngine::getAudioDevice(wxString deviceName, AudioDirection direction, int sampleRate, int numChannels)
{
    for (auto& dev : getAudioDeviceList(direction))
    {
        if (dev.name == deviceName)
        {
            auto devPtr = new MacAudioDevice(dev.deviceId, direction, numChannels, sampleRate);
            return std::shared_ptr<IAudioDevice>(devPtr);
        }
    }
    return nullptr;
}

std::vector<int> MacAudioEngine::getSupportedSampleRates(wxString deviceName, AudioDirection direction)
{
    std::vector<int> result;
    
    int index = 0;
    while (IAudioEngine::StandardSampleRates[index] != -1)
    {
        result.push_back(IAudioEngine::StandardSampleRates[index]);
        index++;
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
    propertyAddress.mSelector = kAudioDevicePropertyDeviceNameCFString;
    propertySize = sizeof(CFStringRef);
    CFStringRef name = nullptr;
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
    
    std::string deviceName = cfStringToStdString_(name);
    CFRelease(name);
    
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
    device.name = deviceName;
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

/**
 * Converts a CFString to a UTF-8 std::string if possible.
 *
 * @param input A reference to the CFString to convert.
 * @return Returns a std::string containing the contents of CFString converted to UTF-8. Returns
 *  an empty string if the input reference is null or conversion is not possible.
 */
std::string MacAudioEngine::cfStringToStdString_(CFStringRef input)
{
    if (!input)
        return {};

    // Attempt to access the underlying buffer directly. This only works if no conversion or
    //  internal allocation is required.
    auto originalBuffer{ CFStringGetCStringPtr(input, kCFStringEncodingUTF8) };
    if (originalBuffer)
        return originalBuffer;

    // Copy the data out to a local buffer.
    auto lengthInUtf16{ CFStringGetLength(input) };
    auto maxLengthInUtf8{ CFStringGetMaximumSizeForEncoding(lengthInUtf16,
        kCFStringEncodingUTF8) + 1 }; // <-- leave room for null terminator
    std::vector<char> localBuffer(maxLengthInUtf8);

    if (CFStringGetCString(input, localBuffer.data(), maxLengthInUtf8, maxLengthInUtf8))
        return localBuffer.data();

    return {};
}
