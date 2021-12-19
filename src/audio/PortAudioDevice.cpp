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

#include "PortAudioDevice.h"
#include "portaudio.h"

PortAudioDevice::PortAudioDevice(int deviceId, IAudioEngine::AudioDirection direction, int sampleRate, int numChannels)
    : deviceId_(deviceId)
    , direction_(direction)
    , sampleRate_(sampleRate)
    , numChannels_(numChannels)
    , deviceStream_(nullptr)
{
    // empty
}

PortAudioDevice::~PortAudioDevice()
{
    if (deviceStream_ != nullptr)
    {
        stop();
    }
}

void PortAudioDevice::start()
{
    PaStreamParameters streamParameters;
    
    streamParameters.device = deviceId_;
    streamParameters.channelCount = numChannels_;
    streamParameters.sampleFormat = paInt16;
    streamParameters.suggestedLatency = 0;
    streamParameters.hostApiSpecificStreamInfo = NULL;
    
    auto error = Pa_OpenStream(
        &deviceStream_,
        direction_ == IAudioEngine::IN ? &streamParameters : nullptr,
        direction_ == IAudioEngine::OUT ? &streamParameters : nullptr,
        sampleRate_,
        0,
        paClipOff,
        &OnPortAudioStreamCallback_,
        this
    );
        
    if (error == paNoError)
    {
        error = Pa_StartStream(deviceStream_);
        if (error != paNoError)
        {
            Pa_CloseStream(deviceStream_);
            deviceStream_ = nullptr;
        }
    }
    else
    {
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
    
    if (thisObj->onAudioUnderflowFunction && statusFlags & 0x1) 
    {
        // underflow
        thisObj->onAudioUnderflowFunction(*thisObj, thisObj->onAudioUnderflowState);
    }
    
    if (thisObj->onAudioOverflowFunction && statusFlags & 0x2) 
    {
        // overflow
        thisObj->onAudioOverflowFunction(*thisObj, thisObj->onAudioOverflowState);
    }
    
    if (thisObj->onAudioDataFunction)
    {
        if (thisObj->direction_ == IAudioEngine::IN)
        {
            thisObj->onAudioDataFunction(*thisObj, const_cast<void*>(input), frameCount, thisObj->onAudioDataState);
        }
        else
        {
            thisObj->onAudioDataFunction(*thisObj, const_cast<void*>(output), frameCount, thisObj->onAudioDataState);
        }
    }
    
    return paContinue;
}