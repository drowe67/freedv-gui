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

// Brought over from previous implementation. "Optimal" value of 0 (per PA
// documentation) causes occasional audio pops/cracks on start for macOS.
#define PA_FPB 256

PortAudioDevice::PortAudioDevice(int deviceId, IAudioEngine::AudioDirection direction, int sampleRate, int numChannels)
    : deviceId_(deviceId)
    , direction_(direction)
    , sampleRate_(sampleRate)
    , numChannels_(numChannels)
    , deviceStream_(nullptr)
{
    auto deviceInfo = Pa_GetDeviceInfo(deviceId_);
    std::string hostApiName = Pa_GetHostApiInfo(deviceInfo->hostApi)->name;

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
    auto deviceInfo = Pa_GetDeviceInfo(deviceId_);

    streamParameters.device = deviceId_;
    streamParameters.channelCount = numChannels_;
    streamParameters.sampleFormat = paInt16;
    streamParameters.suggestedLatency = deviceInfo->defaultHighInputLatency;
    streamParameters.hostApiSpecificStreamInfo = NULL;
    
    auto error = Pa_OpenStream(
        &deviceStream_,
        direction_ == IAudioEngine::AUDIO_ENGINE_IN ? &streamParameters : nullptr,
        direction_ == IAudioEngine::AUDIO_ENGINE_OUT ? &streamParameters : nullptr,
        sampleRate_,
        PA_FPB,
        paClipOff,
        &OnPortAudioStreamCallback_,
        this
    );
        
    if (error == paNoError)
    {
        error = Pa_StartStream(deviceStream_);
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
            
            Pa_CloseStream(deviceStream_);
            deviceStream_ = nullptr;
        }
    }
    else
    {
        std::string errText = Pa_GetErrorText(error);
        if (error == paUnanticipatedHostError)
        {
            std::stringstream ss;
            auto errInfo = Pa_GetLastHostErrorInfo();
            ss << " (error code " << std::hex << errInfo->errorCode << " - " << std::string(errInfo->errorText) << ")";
            errText += ss.str();
        }
        fprintf(stderr, "PortAudio error: %s, device: %s\n", errText.c_str()); 

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
        Pa_StopStream(deviceStream_);
        Pa_CloseStream(deviceStream_);
        deviceStream_ = nullptr;
    }
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
