//=========================================================================
// Name:            WASAPIAudioDevice.h
// Purpose:         Defines the interface to a Windows audio device.
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

#ifndef WASAPI_AUDIO_DEVICE_H
#define WASAPI_AUDIO_DEVICE_H

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <thread>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include "IAudioEngine.h"
#include "IAudioDevice.h"
#include "../util/Win32COMObject.h"

class WASAPIAudioDevice : public Win32COMObject, public IAudioDevice
{
public:
    virtual ~WASAPIAudioDevice();

    virtual int getNumChannels() override;
    virtual int getSampleRate() const override;
    
    virtual void start() override;
    virtual void stop() override;

    virtual bool isRunning() override;

    virtual int getLatencyInMicroseconds() override;

    // Configures current thread for real-time priority. This should be
    // called from the thread that will be operating on received audio.
    virtual void setHelperRealTime() override;

    // Lets audio system know that we're beginning to do work with the
    // received audio.
    virtual void startRealTimeWork() override;

    // Lets audio system know that we're done with the work on the received
    // audio.
    virtual void stopRealTimeWork(bool fastMode = false) override;

    // Reverts real-time priority for current thread.
    virtual void clearHelperRealTime() override;
    
protected:
    friend class WASAPIAudioEngine;

    WASAPIAudioDevice(IAudioClient* client, IAudioEngine::AudioDirection direction, int sampleRate, int numChannels);

private:
    IAudioClient* client_;
    IAudioRenderClient* renderClient_;
    IAudioCaptureClient* captureClient_;
    IAudioEngine::AudioDirection direction_;
    int sampleRate_;
    int numChannels_;
    UINT32 bufferFrameCount_;
    bool initialized_;
    int latencyFrames_;
    std::thread renderCaptureThread_;
    HANDLE renderCaptureEvent_;
    bool isRenderCaptureRunning_;
    HANDLE semaphore_;

    // For mix<->int16 conversions
    int containerBits_;
    int validBits_;
    bool isFloatingPoint_;
    short* tmpBuf_;

    void renderAudio_();
    void captureAudio_();
    void copyFromWindowsBuffer_(void* buf, int numFrames);
    void copyToWindowsBuffer_(void* buf, int numFrames);

    template<typename T>
    void copyFloatToShort_(T* source, int numFrames);
    
    template<typename T>
    void copyShortToFloat_(T* dest, int numFrames);
    
    template<typename T>
    void copyIntToShort_(T* source, int numFrames);

    template<typename T>
    void copyShortToInt_(T* dest, int numFrames);

    static std::string GuidToString_(GUID* guid);
    static thread_local HANDLE HelperTask_;
};

template<typename T>
void WASAPIAudioDevice::copyFloatToShort_(T* source, int numFrames)
{
    for (int index = 0; index < numFrames; index++)
    {
        for (int innerIndex = 0; innerIndex < numChannels_; innerIndex++)
        {
            tmpBuf_[index * numChannels_ + innerIndex] = source[index * numChannels_ + innerIndex] * std::numeric_limits<short>::max();
        }
    }
}

template<typename T>
void WASAPIAudioDevice::copyShortToFloat_(T* dest, int numFrames)
{
    for (int index = 0; index < numFrames; index++)
    {
        for (int innerIndex = 0; innerIndex < numChannels_; innerIndex++)
        {
            dest[index * numChannels_ + innerIndex] = (T)tmpBuf_[index * numChannels_ + innerIndex] / std::numeric_limits<short>::max();
        }
    }
}

template<typename T>
void WASAPIAudioDevice::copyIntToShort_(T* source, int numFrames)
{
    for (int index = 0; index < numFrames; index++)
    {
        for (int innerIndex = 0; innerIndex < numChannels_; innerIndex++)
        {
            T temp = source[index * numChannels_ + innerIndex] >> (containerBits_ - validBits_);
            tmpBuf_[index * numChannels_ + innerIndex] = temp >> (validBits_ >= 16 ? validBits_ - 16 : 0);
        }
    }
}

template<typename T>
void WASAPIAudioDevice::copyShortToInt_(T* dest, int numFrames)
{
    for (int index = 0; index < numFrames; index++)
    {
        for (int innerIndex = 0; innerIndex < numChannels_; innerIndex++)
        {
            T temp = tmpBuf_[index * numChannels_ + innerIndex] << (validBits_ >= 16 ? validBits_ - 16 : 0);
            dest[index * numChannels_ + innerIndex] = temp << (containerBits_ - validBits_);
        }
    }
}

#endif // WASAPI_AUDIO_DEVICE_H
