//=========================================================================
// Name:            WASAPIAudioEngine.cpp
// Purpose:         Defines the main interface to the Windows audio system.
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
#include <future>
#include <windows.h>
#include <initguid.h>
#include <mmreg.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <functiondiscoverykeys.h>
#include <mmdeviceapi.h>
#include "WASAPIAudioDevice.h"
#include "WASAPIAudioEngine.h"
#include "../util/logging/ulog.h"

WASAPIAudioEngine::WASAPIAudioEngine()
    : devEnumerator_(nullptr)
    , inputDeviceList_(nullptr)
    , outputDeviceList_(nullptr)
{
    // empty
}

WASAPIAudioEngine::~WASAPIAudioEngine()
{
    stop();
    
    // Release COM pointers before object fully goes away.
    auto prom = std::make_shared<std::promise<void> >(); 
    auto fut = prom->get_future();
    enqueue_([&]() {
        devEnumerator_ = nullptr;
        inputDeviceList_ = nullptr;
        outputDeviceList_ = nullptr;
        prom->set_value();
    });
    fut.wait(); 
}

void WASAPIAudioEngine::start()
{
    auto prom = std::make_shared<std::promise<void> >();
    auto fut = prom->get_future();
    enqueue_([&]() {
        // Get device enumerator
        HRESULT hr = CoCreateInstance(
           CLSID_MMDeviceEnumerator, NULL,
           CLSCTX_ALL, IID_IMMDeviceEnumerator,
           (void**)devEnumerator_.GetAddressOf());
        if (FAILED(hr))
        {
            std::stringstream ss;
            ss << "Could not create MMDeviceEnumerator (hr = " << hr << ")";
            log_error(ss.str().c_str());
            if (onAudioErrorFunction)
            {
                onAudioErrorFunction(*this, ss.str(), onAudioErrorState); 
            }
            prom->set_value();
            return;
        }

        // Get input and output device collections
        hr = devEnumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, outputDeviceList_.GetAddressOf());
        if (FAILED(hr))
        {
            std::stringstream ss;
            ss << "Could not enumerate render endpoints (hr = " << hr << ")";
            log_error(ss.str().c_str());
            if (onAudioErrorFunction)
            {
                onAudioErrorFunction(*this, ss.str(), onAudioErrorState); 
            }

            devEnumerator_ = nullptr;
            prom->set_value();
            return;
        }

        hr = devEnumerator_->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, inputDeviceList_.GetAddressOf());
        if (FAILED(hr))
        {
            std::stringstream ss;
            ss << "Could not enumerate capture endpoints (hr = " << hr << ")";
            log_error(ss.str().c_str());
            if (onAudioErrorFunction)
            {
                onAudioErrorFunction(*this, ss.str(), onAudioErrorState); 
            }

            outputDeviceList_ = nullptr;
            devEnumerator_ = nullptr;
            prom->set_value();
            return;
        }

        prom->set_value();
    });
    fut.wait();
}

void WASAPIAudioEngine::stop()
{
    auto prom = std::make_shared<std::promise<void> >();
    auto fut = prom->get_future();
    enqueue_([&]() {
        devEnumerator_ = nullptr;
        inputDeviceList_ = nullptr;
        outputDeviceList_ = nullptr;
        
        // Invalidate cached devices.
        cachedInputDeviceList_.clear();
        cachedOutputDeviceList_.clear();
        
        prom->set_value();
    });
    fut.wait();
}

std::vector<AudioDeviceSpecification> WASAPIAudioEngine::getAudioDeviceList(AudioDirection direction) 
{
    auto prom = std::make_shared<std::promise<std::vector<AudioDeviceSpecification> > >();
    auto fut = prom->get_future();
    enqueue_([&, direction]() {
        std::vector<AudioDeviceSpecification> result;

        // Just used the cached results if they exist, no need to call into Windows again.
        if (direction == AudioDirection::AUDIO_ENGINE_IN && cachedInputDeviceList_.size() > 0)
        {
            prom->set_value(cachedInputDeviceList_);
            return;
        }
        else if (cachedOutputDeviceList_.size() > 0)
        {
            prom->set_value(cachedOutputDeviceList_);
            return;
        }
        
        ComPtr<IMMDeviceCollection> coll = 
            (direction == AudioDirection::AUDIO_ENGINE_IN) ?
            inputDeviceList_ :
            outputDeviceList_;

        UINT deviceCount = 0;
        HRESULT hr = coll->GetCount(&deviceCount);

        for (UINT index = 0; index < deviceCount; index++)
        {
            ComPtr<IMMDevice> device = nullptr;
            hr = coll->Item(index, device.GetAddressOf());
            if (FAILED(hr))
            {
                std::stringstream ss;
                ss << "Could not get device " << index << " (hr = " << hr << ")";
                log_error(ss.str().c_str());
                if (onAudioErrorFunction)
                {
                    onAudioErrorFunction(*this, ss.str(), onAudioErrorState); 
                }
                prom->set_value(result);
                return;
            }

            auto devSpec = getDeviceSpecification_(device);
            if (devSpec.isValid())
            {
                devSpec.deviceId = index;
                result.push_back(devSpec);
                log_debug("Found device %s (card = %s, port = %s)", (const char*)devSpec.name.ToUTF8(), (const char*)devSpec.cardName.ToUTF8(), (const char*)devSpec.portName.ToUTF8());
            }
        }

        if (direction == AudioDirection::AUDIO_ENGINE_IN)
        {
            cachedInputDeviceList_ = result;
        }
        else
        {
            cachedOutputDeviceList_ = result;
        }
        
        prom->set_value(result);
    });
    return fut.get();
}

AudioDeviceSpecification WASAPIAudioEngine::getDefaultAudioDevice(AudioDirection direction)
{
    auto specList = getAudioDeviceList(direction);
    auto prom = std::make_shared<std::promise<AudioDeviceSpecification> >();
    auto fut = prom->get_future();
    enqueue_([&, specList, direction]() {
        ComPtr<IMMDevice> defaultDevice = nullptr;
        HRESULT hr = devEnumerator_->GetDefaultAudioEndpoint(
            (direction == AudioDirection::AUDIO_ENGINE_IN) ? eCapture : eRender,
            eConsole,
            defaultDevice.GetAddressOf());
        if (FAILED(hr))
        {
            std::stringstream ss;
            ss << "Could not get default device (hr = " << hr << ")";
            log_error(ss.str().c_str());
            if (onAudioErrorFunction)
            {
                onAudioErrorFunction(*this, ss.str(), onAudioErrorState); 
            }
            prom->set_value(AudioDeviceSpecification::GetInvalidDevice());
            return;
        }

        auto defaultSpec = getDeviceSpecification_(defaultDevice);
        for (auto& spec : specList)
        {
            if (defaultSpec.name == spec.name)
            {
                prom->set_value(spec);
                return;
            }
        }
        log_warn("Could not get device ID for default audio device");
        prom->set_value(AudioDeviceSpecification::GetInvalidDevice());
    });
    return fut.get();
}
    
std::shared_ptr<IAudioDevice> WASAPIAudioEngine::getAudioDevice(wxString deviceName, AudioDirection direction, int sampleRate, int numChannels)
{
    auto devList = getAudioDeviceList(direction);

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
    
    if (!found && sampleRates.size() > 0)
    {
        sampleRate = sampleRates[0];
    }

    auto prom = std::make_shared<std::promise<std::shared_ptr<IAudioDevice> > >();
    auto fut = prom->get_future();
    enqueue_([&, devList, deviceName, direction, sampleRate, numChannels]() {
        std::shared_ptr<IAudioDevice> result;
        int finalSampleRate = sampleRate;
        ComPtr<IMMDeviceCollection> coll = 
            (direction == AudioDirection::AUDIO_ENGINE_IN) ?
            inputDeviceList_ :
            outputDeviceList_;

        for (auto& dev : devList)
        {
            if (dev.name == deviceName)
            {
                log_info("Creating WASAPIAudioDevice for device %s (ID %d, direction = %d, sample rate = %d, number of channels = %d)", (const char*)deviceName.ToUTF8(), (int)dev.deviceId, (int)direction, sampleRate, numChannels);

                ComPtr<IMMDevice> device = nullptr;
                ComPtr<IAudioClient> client = nullptr;

                HRESULT hr = coll->Item(dev.deviceId, device.GetAddressOf());
                if (FAILED(hr))
                {
                    std::stringstream ss;
                    ss << "Could not get device " << dev.deviceId << " (hr = " << hr << ")";
                    log_error(ss.str().c_str());
                    if (onAudioErrorFunction)
                    {
                         onAudioErrorFunction(*this, ss.str(), onAudioErrorState); 
                    }
                    break;
                }

                hr = device->Activate(
                    IID_IAudioClient, CLSCTX_ALL,
                    nullptr, (void**)client.GetAddressOf()); 
                if (FAILED(hr))
                {
                    std::stringstream ss;
                    ss << "Could not get client for device " << dev.deviceId << " (hr = " << hr << ")";
                    log_error(ss.str().c_str());
                    if (onAudioErrorFunction)
                    {
                         onAudioErrorFunction(*this, ss.str(), onAudioErrorState); 
                    }
                    break;
                }

                if (!found)
                {
                    finalSampleRate = dev.defaultSampleRate;
                }

                // Ensure that the passed-in number of channels is within the allowed range.
                int finalNumChannels = std::max(numChannels, dev.minChannels);
                finalNumChannels = std::min(finalNumChannels, dev.maxChannels);

                auto devPtr = new WASAPIAudioDevice(client, device, direction, finalSampleRate, finalNumChannels);
                result = std::shared_ptr<IAudioDevice>(devPtr);
            }
        }
        prom->set_value(result);
    });
    return fut.get();
}

std::vector<int> WASAPIAudioEngine::getSupportedSampleRates(wxString deviceName, AudioDirection direction)
{
    auto devList = getAudioDeviceList(direction);

    // Note: WASAPI only supports the device's native sample rate!
    auto prom = std::make_shared<std::promise<std::vector<int> > >();
    auto fut = prom->get_future();
    enqueue_([&, devList, deviceName]() {
        std::vector<int> result;
        for (auto& spec : devList)
        {
            if (deviceName == spec.name)
            {
                result.push_back(spec.defaultSampleRate);
                break;
            }
        }
        prom->set_value(result);
    });
    return fut.get();
}

AudioDeviceSpecification WASAPIAudioEngine::getDeviceSpecification_(ComPtr<IMMDevice> device)
{
    // Get device name
    ComPtr<IPropertyStore> propStore = nullptr;
    HRESULT hr = device->OpenPropertyStore(STGM_READ, propStore.GetAddressOf());
    if (FAILED(hr))
    {
        std::stringstream ss;
        ss << "Could not open device property store (hr = " << hr << ")";
        log_error(ss.str().c_str());
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, ss.str(), onAudioErrorState); 
        }
        return AudioDeviceSpecification::GetInvalidDevice();
    }

    PROPVARIANT friendlyName;
    PropVariantInit(&friendlyName);
    hr = propStore->GetValue(PKEY_Device_FriendlyName, &friendlyName);
    if (FAILED(hr))
    {
        std::stringstream ss;
        ss << "Could not get device friendly name (hr = " << hr << ")";
        log_error(ss.str().c_str());
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, ss.str(), onAudioErrorState); 
        }
        PropVariantClear(&friendlyName);
        return AudioDeviceSpecification::GetInvalidDevice();
    }

    if (friendlyName.vt == VT_EMPTY)
    {
        log_warn("Device does not have a friendly name!");
        PropVariantClear(&friendlyName);
        return AudioDeviceSpecification::GetInvalidDevice();
    }

    AudioDeviceSpecification spec;
    spec.name = wxString::FromUTF8(getUTF8String_(friendlyName.pwszVal));
    spec.apiName = "Windows WASAPI";
    
    // Get card and port info
    hr = propStore->GetValue(PKEY_DeviceInterface_FriendlyName, &friendlyName);
    if (FAILED(hr))
    {
        std::stringstream ss;
        ss << "Could not get card name (hr = " << hr << ")";
        log_error(ss.str().c_str());
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, ss.str(), onAudioErrorState); 
        }
        PropVariantClear(&friendlyName);
        return AudioDeviceSpecification::GetInvalidDevice();
    }

    if (friendlyName.vt == VT_EMPTY)
    {
        log_warn("Device does not have a card name!");
        PropVariantClear(&friendlyName);
        return AudioDeviceSpecification::GetInvalidDevice();
    }
    
    spec.cardName = wxString::FromUTF8(getUTF8String_(friendlyName.pwszVal));

    hr = propStore->GetValue(PKEY_Device_DeviceDesc, &friendlyName);
    if (FAILED(hr))
    {
        std::stringstream ss;
        ss << "Could not get port name (hr = " << hr << ")";
        log_error(ss.str().c_str());
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, ss.str(), onAudioErrorState); 
        }
        PropVariantClear(&friendlyName);
        return AudioDeviceSpecification::GetInvalidDevice();
    }

    if (friendlyName.vt == VT_EMPTY)
    {
        log_warn("Device does not have a port name!");
        PropVariantClear(&friendlyName);
        return AudioDeviceSpecification::GetInvalidDevice();
    }
    
    spec.portName = wxString::FromUTF8(getUTF8String_(friendlyName.pwszVal));
    bool noPortName = spec.portName.Length() == 0 && spec.cardName != spec.name;
    bool isFlexRadio = spec.cardName.StartsWith("FlexRadio");

    if (noPortName)
    {
        // If there's no port name, just use the same name for cardName
        // as the card's full name. 
        spec.cardName = spec.name;
    }
    else if (isFlexRadio)
    {
        // We also have a carveout for FlexRadio virtual audio devices 
        // so that Easy Setup can automatically group the RX and TX 
        // devices together.
        spec.cardName = spec.portName;
    }

    // Activate IAudioClient so we can obtain format info
    ComPtr<IAudioClient> audioClient = nullptr;
    hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)audioClient.GetAddressOf());
    if (FAILED(hr))
    {
        std::stringstream ss;
        ss << "Could not activate IAudioClient for device " << spec.name << " (hr = " << hr << ")";
        log_error(ss.str().c_str());
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, ss.str(), onAudioErrorState); 
        }
        PropVariantClear(&friendlyName);
        return AudioDeviceSpecification::GetInvalidDevice();
    }

    WAVEFORMATEX* streamFormat = nullptr;
    hr = audioClient->GetMixFormat(&streamFormat);
    if (FAILED(hr))
    {
        std::stringstream ss;
        ss << "Could not get stream format for device " << spec.name << " (hr = " << hr << ")";
        log_error(ss.str().c_str());
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, ss.str(), onAudioErrorState); 
        }
        PropVariantClear(&friendlyName);
        return AudioDeviceSpecification::GetInvalidDevice();
    }

    spec.defaultSampleRate = streamFormat->nSamplesPerSec;
    spec.maxChannels = streamFormat->nChannels;
    spec.minChannels = streamFormat->nChannels;
    
    // Determine minimum number of channels supported.
    for (int channel = spec.maxChannels - 1; channel >= 1; channel--)
    {
        WAVEFORMATEX* closestFormat = nullptr;
        streamFormat->nChannels = channel;
        if (audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, streamFormat, &closestFormat) == S_OK)
        {
            spec.minChannels = channel;
        }
        if (closestFormat != nullptr)
        {
            CoTaskMemFree(closestFormat);
        }
        
        if (spec.minChannels != channel)
        {
            break;
        }
    }

    CoTaskMemFree(streamFormat);
    PropVariantClear(&friendlyName);

    return spec; // note: deviceId needs to be filled in by caller
}

std::string WASAPIAudioEngine::getUTF8String_(LPWSTR str)
{
    std::string val = "";  
    int size = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
    if (size > 0) 
    {
        char* tmp = new char[size];
        assert(tmp != nullptr);
        WideCharToMultiByte(CP_UTF8, 0, str, -1, tmp, size, NULL, NULL);
        val = tmp;
        delete[] tmp;
    }
    return val;
}

