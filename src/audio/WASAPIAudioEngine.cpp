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
           (void**)&devEnumerator_);
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
        hr = devEnumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &outputDeviceList_);
        if (FAILED(hr))
        {
            std::stringstream ss;
            ss << "Could not enumerate render endpoints (hr = " << hr << ")";
            log_error(ss.str().c_str());
            if (onAudioErrorFunction)
            {
                onAudioErrorFunction(*this, ss.str(), onAudioErrorState); 
            }

            devEnumerator_->Release();
            devEnumerator_ = nullptr;

            prom->set_value();
            return;
        }

        hr = devEnumerator_->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &inputDeviceList_);
        if (FAILED(hr))
        {
            std::stringstream ss;
            ss << "Could not enumerate capture endpoints (hr = " << hr << ")";
            log_error(ss.str().c_str());
            if (onAudioErrorFunction)
            {
                onAudioErrorFunction(*this, ss.str(), onAudioErrorState); 
            }

            outputDeviceList_->Release();
            outputDeviceList_ = nullptr;

            devEnumerator_->Release();
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
        if (inputDeviceList_ != nullptr)
        {
            inputDeviceList_->Release();
            inputDeviceList_ = nullptr;
        }

        if (outputDeviceList_ != nullptr)
        {
            outputDeviceList_->Release();
            outputDeviceList_ = nullptr;
        }

        if (devEnumerator_ != nullptr)
        {
            devEnumerator_->Release();
            devEnumerator_ = nullptr;
        }

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

        IMMDeviceCollection* coll = 
            (direction == AudioDirection::AUDIO_ENGINE_IN) ?
            inputDeviceList_ :
            outputDeviceList_;

        UINT deviceCount = 0;
        HRESULT hr = coll->GetCount(&deviceCount);

        for (UINT index = 0; index < deviceCount; index++)
        {
            IMMDevice* device = nullptr;
            hr = coll->Item(index, &device);
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
            }
            device->Release();
        }

        prom->set_value(result);
    });
    return fut.get();
}

AudioDeviceSpecification WASAPIAudioEngine::getDefaultAudioDevice(AudioDirection direction)
{
    auto prom = std::make_shared<std::promise<AudioDeviceSpecification> >();
    auto fut = prom->get_future();
    enqueue_([&, direction]() {
        IMMDevice* defaultDevice = nullptr;
        HRESULT hr = devEnumerator_->GetDefaultAudioEndpoint(
            (direction == AudioDirection::AUDIO_ENGINE_IN) ? eCapture : eRender,
            eConsole,
            &defaultDevice);
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
        auto specList = getAudioDeviceList(direction);
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
    auto prom = std::make_shared<std::promise<std::shared_ptr<IAudioDevice> > >();
    auto fut = prom->get_future();
    enqueue_([&]() {
        prom->set_value(nullptr); // TBD - stub
    });
    return fut.get();
}

std::vector<int> WASAPIAudioEngine::getSupportedSampleRates(wxString deviceName, AudioDirection direction)
{
    auto devList = getAudioDeviceList(direction);

    // Note: WASAPI only supports the device's native sample rate!
    auto prom = std::make_shared<std::promise<std::vector<int> > >();
    auto fut = prom->get_future();
    enqueue_([&, devList]() {
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

AudioDeviceSpecification WASAPIAudioEngine::getDeviceSpecification_(IMMDevice* device)
{
    // Get device name
    IPropertyStore* propStore = nullptr;
    HRESULT hr = device->OpenPropertyStore(STGM_READ, &propStore);
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
        propStore->Release();
        return AudioDeviceSpecification::GetInvalidDevice();
    }

    if (friendlyName.vt == VT_EMPTY)
    {
        log_warn("Device does not have a friendly name!");
        PropVariantClear(&friendlyName);
        propStore->Release();
        return AudioDeviceSpecification::GetInvalidDevice();
    }

    AudioDeviceSpecification spec;
    spec.name = getUTF8String_(friendlyName.pwszVal);
    spec.apiName = "Windows WASAPI";

    // Activate IAudioClient so we can obtain format info
    IAudioClient* audioClient = nullptr;
    hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&audioClient);
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
        propStore->Release();
        return AudioDeviceSpecification::GetInvalidDevice();
    }

    WAVEFORMATEX* streamFormat;
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
        audioClient->Release();
        PropVariantClear(&friendlyName);
        propStore->Release();
        return AudioDeviceSpecification::GetInvalidDevice();
    }

    spec.defaultSampleRate = streamFormat->nSamplesPerSec;
    spec.maxChannels = streamFormat->nChannels;
    spec.minChannels = streamFormat->nChannels;

    CoTaskMemFree(streamFormat);
    audioClient->Release();
    PropVariantClear(&friendlyName);
    propStore->Release();

    return spec; // note: deviceId needs to be filled in by caller
}

std::string WASAPIAudioEngine::getUTF8String_(LPWSTR str)
{
    std::vector<char> buffer; 
    std::string val = "";  
    int size = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
    if (size > 0) 
    {
        buffer.resize(size);
        WideCharToMultiByte(CP_UTF8, 0, str, -1, static_cast<LPSTR>(&buffer[0]), buffer.size(), NULL, NULL);
        val = std::string(&buffer[0]);
    }
    return val;
}

