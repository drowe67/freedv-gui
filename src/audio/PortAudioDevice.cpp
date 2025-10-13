//=========================================================================
// Name:            PortAudioDevice.cpp
// Purpose:         Defines the interface to a PortAudio device.
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
#include <iomanip>
#include <cstring>
#include "PortAudioDevice.h"
#include "portaudio.h"
#if defined(WIN32)
#include "pa_win_wasapi.h"
#elif defined(__APPLE__)
#include "pa_mac_core.h"
#endif // defined(WIN32)

#define PA_FPB 0

PortAudioDevice::PortAudioDevice(std::shared_ptr<PortAudioInterface> library, int deviceId, IAudioEngine::AudioDirection direction, int sampleRate, int numChannels)
    : deviceId_(deviceId)
    , direction_(direction)
    , sampleRate_(sampleRate)
    , numChannels_(numChannels)
    , deviceStream_(nullptr)
    , portAudioLibrary_(library)
{
    auto deviceInfo = portAudioLibrary_->GetDeviceInfo(deviceId_).get();
    std::string hostApiName = portAudioLibrary_->GetHostApiInfo(deviceInfo->hostApi).get()->name;

    // Windows only: we are switching from MME to WASAPI. A side effect
    // of this is that we really only support one sample rate. Instead
    // of erroring out, let's just use the device's default sample rate 
    // instead to prevent users from needing to reconfigure as much as
    // possible.
    //
    // Additionally, if we somehow get a sample rate of 0 (which normally
    // wouldn't happen, but just in case), use the default sample rate
    // as well. The correct sample rate will eventually be retrieved by
    // higher level code and re-saved.
    if (hostApiName.find("Windows WASAPI") != std::string::npos ||
        sampleRate == 0)
    {
        sampleRate_ = deviceInfo->defaultSampleRate;
    }
}

PortAudioDevice::~PortAudioDevice()
{
    if (deviceStream_ != nullptr)
    {
        stop();
    }
}

bool PortAudioDevice::isRunning()
{
    return deviceStream_ != nullptr;
}

void PortAudioDevice::start()
{
    PaStreamParameters streamParameters;
    auto deviceInfo = portAudioLibrary_->GetDeviceInfo(deviceId_).get();

    streamParameters.device = deviceId_;
    streamParameters.channelCount = numChannels_;
    streamParameters.sampleFormat = paInt16;
    streamParameters.suggestedLatency = 
        IAudioEngine::AUDIO_ENGINE_IN ? deviceInfo->defaultLowInputLatency : deviceInfo->defaultLowOutputLatency;

#if defined(WIN32)
    PaWasapiStreamInfo wasapiInfo;
    wasapiInfo.size = sizeof(PaWasapiStreamInfo);
    wasapiInfo.hostApiType = paWASAPI;
    wasapiInfo.version = 1;
    wasapiInfo.flags = paWinWasapiThreadPriority;
    wasapiInfo.channelMask = NULL;
    wasapiInfo.hostProcessorOutput = NULL;
    wasapiInfo.hostProcessorInput = NULL;
    wasapiInfo.threadPriority = eThreadPriorityProAudio;
    streamParameters.hostApiSpecificStreamInfo = &wasapiInfo;
#elif defined(__APPLE__)
    PaMacCoreStreamInfo macInfo;
    PaMacCore_SetupStreamInfo(&macInfo, paMacCorePro);
    streamParameters.hostApiSpecificStreamInfo = &macInfo;
#else
    streamParameters.hostApiSpecificStreamInfo = NULL;
#endif // defined(WIN32)
    
    auto error = portAudioLibrary_->OpenStream(
        &deviceStream_,
        direction_ == IAudioEngine::AUDIO_ENGINE_IN ? &streamParameters : nullptr,
        direction_ == IAudioEngine::AUDIO_ENGINE_OUT ? &streamParameters : nullptr,
        sampleRate_,
        PA_FPB,
        paClipOff,
        &OnPortAudioStreamCallback_,
        this
    ).get();
        
    if (error == paNoError)
    {
        error = portAudioLibrary_->StartStream(deviceStream_).get();
        if (error != paNoError)
        {
            std::string errText = portAudioLibrary_->GetErrorText(error).get();
            if (error == paUnanticipatedHostError)
            {
                std::stringstream ss;
                auto errInfo = portAudioLibrary_->GetLastHostErrorInfo().get();
                ss << " (error code " << std::hex << errInfo->errorCode << " - " << std::string(errInfo->errorText) << ")";
                errText += ss.str();
            }

            if (onAudioErrorFunction)
            {
                onAudioErrorFunction(*this, errText, onAudioErrorState);
            }
            
            portAudioLibrary_->CloseStream(deviceStream_).wait();
            deviceStream_ = nullptr;
        }
    }
    else
    {
        std::string errText = portAudioLibrary_->GetErrorText(error).get();
        if (error == paUnanticipatedHostError)
        {
            std::stringstream ss;
            auto errInfo = portAudioLibrary_->GetLastHostErrorInfo().get();
            ss << " (error code " << std::hex << errInfo->errorCode << " - " << std::string(errInfo->errorText) << ")";
            errText += ss.str();
        }

        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, errText, onAudioErrorState);
        }
        deviceStream_ = nullptr;
    }
}

void PortAudioDevice::stop()
{
    if (deviceStream_ != nullptr)
    {
        portAudioLibrary_->StopStream(deviceStream_).wait();
        portAudioLibrary_->CloseStream(deviceStream_).wait();
        deviceStream_ = nullptr;
    }
}

int64_t PortAudioDevice::getLatencyInMicroseconds()
{
    int64_t latency = 0;
    if (deviceStream_ != nullptr)
    {
        auto streamInfo = portAudioLibrary_->GetStreamInfo(deviceStream_).get();
        latency = 1000000 * (direction_ == IAudioEngine::AUDIO_ENGINE_IN ? streamInfo->inputLatency : streamInfo->outputLatency);
    }

    return latency;
}

int PortAudioDevice::OnPortAudioStreamCallback_(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    PortAudioDevice* thisObj = static_cast<PortAudioDevice*>(userData);
    
    unsigned int overflowFlag = 0;
    unsigned int underflowFlag = 0;
    
    if (thisObj->direction_ == IAudioEngine::AUDIO_ENGINE_IN)
    {
        underflowFlag = 0x1;
        overflowFlag = 0x2;
    }
    else
    {
        underflowFlag = 0x4;
        overflowFlag = 0x8;
    }
    
    if (thisObj->onAudioUnderflowFunction && statusFlags & underflowFlag) 
    {
        // underflow
        thisObj->onAudioUnderflowFunction(*thisObj, thisObj->onAudioUnderflowState);
    }
    
    if (thisObj->onAudioOverflowFunction && statusFlags & overflowFlag) 
    {
        // overflow
        thisObj->onAudioOverflowFunction(*thisObj, thisObj->onAudioOverflowState);
    }
    
    void* dataPtr = 
        thisObj->direction_ == IAudioEngine::AUDIO_ENGINE_IN ? 
        const_cast<void*>(input) :
        const_cast<void*>(output);
    
    if (thisObj->direction_ == IAudioEngine::AUDIO_ENGINE_OUT)
    {
        // Zero out samples by default in case we don't have any data available.
        memset(dataPtr, 0, sizeof(short) * thisObj->getNumChannels() * frameCount);
    }

    if (thisObj->onAudioDataFunction)
    {
        thisObj->onAudioDataFunction(*thisObj, dataPtr, frameCount, thisObj->onAudioDataState);
    }
    
    return paContinue;
}
