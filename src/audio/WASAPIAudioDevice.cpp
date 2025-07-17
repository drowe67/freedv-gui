//=========================================================================
// Name:            WASAPIAudioDevice.cpp
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

#include "WASAPIAudioDevice.h"

#include <sstream>
#include <chrono>
#include <thread>
#include <future>
#include <avrt.h>
#include "../util/logging/ulog.h"

#define BLOCK_TIME_NS (10000000)

// Nanoseconds per REFERENCE_TIME unit
#define NS_PER_REFTIME (100)

thread_local HANDLE WASAPIAudioDevice::HelperTask_ = nullptr;
    
WASAPIAudioDevice::WASAPIAudioDevice(ComPtr<IAudioClient> client, IAudioEngine::AudioDirection direction, int sampleRate, int numChannels)
    : client_(client)
    , renderClient_(nullptr)
    , captureClient_(nullptr)
    , direction_(direction)
    , sampleRate_(sampleRate)
    , numChannels_(numChannels)
    , bufferFrameCount_(0)
    , initialized_(false)
    , latencyFrames_(0)
    , renderCaptureEvent_(nullptr)
    , isRenderCaptureRunning_(false)
    , semaphore_(nullptr)
    , tmpBuf_(nullptr)
{
    // empty
}

WASAPIAudioDevice::~WASAPIAudioDevice()
{
    stop();

    // Release IAudioClient here as it can normally be
    // reused after stop().
    auto prom = std::make_shared<std::promise<void> >(); 
    auto fut = prom->get_future();
    enqueue_([&]() {
        client_ = nullptr;
        renderClient_ = nullptr;
        captureClient_ = nullptr;
        prom->set_value();
    });
    fut.wait(); 
}

int WASAPIAudioDevice::getNumChannels()
{
    return numChannels_;
}

int WASAPIAudioDevice::getSampleRate() const
{
    return sampleRate_;
}
 
void WASAPIAudioDevice::start() 
{
    log_info("Starting device with direction %d, sample rate %d, num channels %d", direction_, sampleRate_, numChannels_);

    auto prom = std::make_shared<std::promise<void> >(); 
    auto fut = prom->get_future();
    enqueue_([&]() {
        WAVEFORMATEX* streamFormatPtr = nullptr;
        WAVEFORMATEX streamFormat;
        bool freeStreamFormat = false;

        // Populate stream format based on requested sample
        // rate/number of channels.
        // NOTE: this should already have been determined valid
        // by the audio engine!
        HRESULT hr = client_->GetMixFormat(&streamFormatPtr);
        if (SUCCEEDED(hr))
        {
            freeStreamFormat = true;
            streamFormatPtr->nChannels = numChannels_;
            streamFormatPtr->nSamplesPerSec = sampleRate_;
            streamFormatPtr->nBlockAlign = (numChannels_ * streamFormatPtr->wBitsPerSample) / 8;
            streamFormatPtr->nAvgBytesPerSec = streamFormatPtr->nSamplesPerSec * streamFormatPtr->nBlockAlign;
        }
        else
        {
            streamFormatPtr = &streamFormat;
            streamFormat.wFormatTag = WAVE_FORMAT_PCM;
            streamFormat.wBitsPerSample = 16;
            streamFormat.nChannels = numChannels_;
            streamFormat.nSamplesPerSec = sampleRate_;
            streamFormat.nBlockAlign = (numChannels_ * streamFormat.wBitsPerSample) / 8;
            streamFormat.nAvgBytesPerSec = streamFormat.nSamplesPerSec * streamFormat.nBlockAlign;
            streamFormat.cbSize = 0;
        }

        // Set up for conversion to mix format
        if (streamFormatPtr->wFormatTag == WAVE_FORMAT_PCM)
        {
            containerBits_ = streamFormatPtr->wBitsPerSample;
            validBits_ = streamFormatPtr->wBitsPerSample;
            isFloatingPoint_ = false;

            log_info("Mix format is integer (container bits: %d, valid bits: %d)", containerBits_, validBits_);
        }
        else if (streamFormatPtr->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        {
            containerBits_ = streamFormatPtr->wBitsPerSample;
            validBits_ = streamFormatPtr->wBitsPerSample;
            isFloatingPoint_ = true;
            log_info("Mix format is floating point (container bits: %d, valid bits: %d)", containerBits_, validBits_);
        }
        else if (streamFormatPtr->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        {
            WAVEFORMATEXTENSIBLE* extFormat = (WAVEFORMATEXTENSIBLE*)streamFormatPtr;
            containerBits_ = streamFormatPtr->wBitsPerSample;
            validBits_ = extFormat->Samples.wValidBitsPerSample;
            if (extFormat->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
            {
                isFloatingPoint_ = false;
                log_info("Mix format is integer (container bits: %d, valid bits: %d)", containerBits_, validBits_);
            }
            else if (extFormat->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
            {
                isFloatingPoint_ = true;
                log_info("Mix format is floating point (container bits: %d, valid bits: %d)", containerBits_, validBits_);
            }
            else
            {
                std::stringstream ss;
                ss << "Unknown mix format found: " << GuidToString_(&extFormat->SubFormat);
                log_error(ss.str().c_str());
                if (onAudioErrorFunction)
                {
                    onAudioErrorFunction(*this, ss.str(), onAudioErrorState);
                }
                if (freeStreamFormat)
                {
                    CoTaskMemFree(streamFormatPtr);
                }
                prom->set_value();
                return;
            }
        }
        else
        {
            std::stringstream ss;
            ss << "Unknown mix format found: " << streamFormatPtr->wFormatTag;
            log_error(ss.str().c_str());
            if (onAudioErrorFunction)
            {
                onAudioErrorFunction(*this, ss.str(), onAudioErrorState);
            }
            if (freeStreamFormat)
            {
                CoTaskMemFree(streamFormatPtr);
            }
            prom->set_value();
            return;
        }

        if (!initialized_)
        {
            // Initialize the audio client with the above format
            hr = client_->Initialize(
                AUDCLNT_SHAREMODE_SHARED,
                AUDCLNT_STREAMFLAGS_EVENTCALLBACK | 
                    AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                    AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                BLOCK_TIME_NS / NS_PER_REFTIME, // REFERENCE_TIME is in 100ns units
                0,
                streamFormatPtr,
                nullptr);
            if (freeStreamFormat)
            {
                CoTaskMemFree(streamFormatPtr);
            }
            if (FAILED(hr))
            {
                std::stringstream ss;
                ss << "Could not initialize AudioClient (hr = " << hr << ")";
                log_error(ss.str().c_str());
                if (onAudioErrorFunction)
                {
                    onAudioErrorFunction(*this, ss.str(), onAudioErrorState);
                }
                prom->set_value();
                return;
            }
            initialized_ = true;
        }
        else if (freeStreamFormat)
        {
            CoTaskMemFree(streamFormatPtr);
        }

        // Create render/capture event
        renderCaptureEvent_ = CreateEvent(nullptr, false, false, nullptr);
        if (renderCaptureEvent_ == nullptr)
        {
            std::stringstream ss;
            ss << "Could not create event (hr = " << GetLastError() << ")";
            log_error(ss.str().c_str());
            if (onAudioErrorFunction)
            {
                onAudioErrorFunction(*this, ss.str(), onAudioErrorState);
            }
            prom->set_value();
            return;
        }

        // Assign render/capture event
        hr = client_->SetEventHandle(renderCaptureEvent_);
        if (FAILED(hr))
        {
            std::stringstream ss;
            ss << "Could not assign event handle (hr = " << hr << ")";
            log_error(ss.str().c_str());
            if (onAudioErrorFunction)
            {
                onAudioErrorFunction(*this, ss.str(), onAudioErrorState);
            }
            CloseHandle(renderCaptureEvent_);
            prom->set_value();
            return;
        }

        // Get actual allocated buffer size
        hr = client_->GetBufferSize(&bufferFrameCount_);
        if (FAILED(hr))
        {
            std::stringstream ss;
            ss << "Could not get buffer size (hr = " << hr << ")";
            log_error(ss.str().c_str());
            if (onAudioErrorFunction)
            {
                onAudioErrorFunction(*this, ss.str(), onAudioErrorState);
            }
            prom->set_value();
            return;
        }
        log_info("Allocated %d frames for audio buffers", bufferFrameCount_);
        
        // Get latency
        latencyFrames_ = bufferFrameCount_;
                
        REFERENCE_TIME latency = 0;
        hr = client_->GetStreamLatency(&latency);
        if (FAILED(hr))
        {
            std::stringstream ss;
            ss << "Could not get latency (hr = " << hr << ")";
            log_warn(ss.str().c_str());
        }
        else
        {
            latencyFrames_ += sampleRate_ * ((double)(NS_PER_REFTIME * latency) / 1e9);
        }

        // Get capture/render client
        if (direction_ == IAudioEngine::AUDIO_ENGINE_IN)
        {
            hr = client_->GetService(
                IID_IAudioCaptureClient,
                (void**)captureClient_.GetAddressOf());
        }
        else
        {
            hr = client_->GetService(
                IID_IAudioRenderClient,
                (void**)renderClient_.GetAddressOf());
        }
        if (FAILED(hr))
        {
            std::stringstream ss;
            ss << "Could not get render/capture client (hr = " << hr << ")";
            log_error(ss.str().c_str());
            if (onAudioErrorFunction)
            {
                onAudioErrorFunction(*this, ss.str(), onAudioErrorState);
            }
            prom->set_value();
            return;
        }

        // Allocate temporary buffer
        tmpBuf_ = new short[sampleRate_];
        assert(tmpBuf_ != nullptr);

        if (direction_ == IAudioEngine::AUDIO_ENGINE_OUT)
        {
            // Perform initial population of audio buffer
            BYTE* data = nullptr;
            hr = renderClient_->GetBuffer(bufferFrameCount_, &data);
            if (FAILED(hr))
            {
                std::stringstream ss;
                ss << "Could not get render buffer (hr = " << hr << ")";
                log_error(ss.str().c_str());
                if (onAudioErrorFunction)
                {
                    onAudioErrorFunction(*this, ss.str(), onAudioErrorState);
                }
                renderClient_ = nullptr;

                delete[] tmpBuf_;
                tmpBuf_ = nullptr;

                prom->set_value();
                return;
            }

            memset(tmpBuf_, 0, bufferFrameCount_ * numChannels_ * sizeof(short));
            if (onAudioDataFunction)
            {
                onAudioDataFunction(*this, tmpBuf_, bufferFrameCount_, onAudioDataState);
            }

            copyToWindowsBuffer_(data, bufferFrameCount_);

            hr = renderClient_->ReleaseBuffer(bufferFrameCount_, 0);
            if (FAILED(hr))
            {
                std::stringstream ss;
                ss << "Could not release render buffer (hr = " << hr << ")";
                log_error(ss.str().c_str());
                if (onAudioErrorFunction)
                {
                    onAudioErrorFunction(*this, ss.str(), onAudioErrorState);
                }
                renderClient_ = nullptr;

                delete[] tmpBuf_;
                tmpBuf_ = nullptr;

                prom->set_value();
                return;
            }
        }

        // Create semaphore
        semaphore_ = CreateSemaphore(nullptr, 0, 1, nullptr);
        if (semaphore_ == nullptr)
        {
            std::stringstream ss;
            ss << "Could not create semaphore (err = " << GetLastError() << ")";
            log_warn(ss.str().c_str());
        }

        // Start render/capture
        hr = client_->Start();
        if (FAILED(hr))
        {
            std::stringstream ss;
            ss << "Could not start audio device (hr = " << hr << ")";
            log_error(ss.str().c_str());
            if (onAudioErrorFunction)
            {
                onAudioErrorFunction(*this, ss.str(), onAudioErrorState);
            }
            renderClient_ = nullptr;
            captureClient_ = nullptr;

            delete[] tmpBuf_;
            tmpBuf_ = nullptr;

            prom->set_value();
            return;
        }

        // Start render/capture thread.
        isRenderCaptureRunning_ = true;
        renderCaptureThread_ = std::thread([this]() {
            log_info("Starting render/capture thread");
            
            // Capture references for use by this thread.
            ComPtr<IAudioRenderClient> renderClientRef = renderClient_;
            ComPtr<IAudioCaptureClient> captureClientRef = captureClient_;
            ComPtr<IAudioClient> clientRef = client_;

            HRESULT res = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
            if (FAILED(res))
            {
                log_warn("Could not initialize COM (res = %d)", res);
            }
            
            // Temporarily raise priority of task
            setHelperRealTime();

            while (isRenderCaptureRunning_)
            {
                WaitForSingleObject(renderCaptureEvent_, 100);
                if (isRenderCaptureRunning_)
                {
                    if (direction_ == IAudioEngine::AUDIO_ENGINE_OUT)
                    {
                        renderAudio_(renderClientRef);
                    }
                    else
                    {
                        captureAudio_(captureClientRef);
                    }
                }
            }

            log_info("Exiting render/capture thread");

            clearHelperRealTime();                  
            CoUninitialize();
        });

        prom->set_value();
    });

    fut.wait();
}

void WASAPIAudioDevice::stop()
{
    log_info("Stopping device with direction %d, sample rate %d, num channels %d", direction_, sampleRate_, numChannels_);

    auto prom = std::make_shared<std::promise<void> >(); 
    auto fut = prom->get_future();
    enqueue_([&]() {
        isRenderCaptureRunning_ = false;
        if (renderCaptureThread_.joinable())
        {
            renderCaptureThread_.join();
        }
        
        if (renderClient_ || captureClient_)
        {
            HRESULT hr = client_->Stop();
            if (FAILED(hr))
            {
                std::stringstream ss;
                ss << "Could not stop audio device (hr = " << hr << ")";
                log_error(ss.str().c_str());
                if (onAudioErrorFunction)
                {
                    onAudioErrorFunction(*this, ss.str(), onAudioErrorState);
                }
            }
        }
        
        renderClient_ = nullptr;
        captureClient_ = nullptr;

        if (renderCaptureEvent_ != nullptr)
        {
            CloseHandle(renderCaptureEvent_);
            renderCaptureEvent_ = nullptr;
        }
        
        if (semaphore_ != nullptr)
        {
            // Set semaphore_ to nullptr first in case someone could be potentially
            // using it. Then for those currently waiting, release the semaphore 
            // to get them unstuck. THEN we can close it. Otherwise, heap corruption
            // occurs!
            auto tmpSem = semaphore_;
            semaphore_ = nullptr;
            ReleaseSemaphore(tmpSem, 1, nullptr);
            CloseHandle(tmpSem);
        }

        if (tmpBuf_ != nullptr)
        {
            delete[] tmpBuf_;
            tmpBuf_ = nullptr;
        }

        prom->set_value();
    });
    fut.wait();
}

bool WASAPIAudioDevice::isRunning()
{
    return (renderClient_) || (captureClient_);
}

int WASAPIAudioDevice::getLatencyInMicroseconds()
{
    // Note: latencyFrames_ isn't expected to change, so we don't need to 
    // wrap this call in an enqueue_() like with the other public methods.
    return 1000000 * latencyFrames_ / sampleRate_;
}

void WASAPIAudioDevice::setHelperRealTime()
{
    DWORD taskIndex = 0;
    HelperTask_ = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
    if (HelperTask_ == nullptr)
    {
        log_warn("Could not increase thread priority");
    }
}

void WASAPIAudioDevice::startRealTimeWork() 
{
    // empty
}

void WASAPIAudioDevice::stopRealTimeWork(bool fastMode) 
{
    if (semaphore_ == nullptr)
    {
        // Fallback to base class behavior
        IAudioDevice::stopRealTimeWork();
        return;
    }

    // Wait a maximum of (bufferSize / sampleRate) seconds for the semaphore to return
    DWORD result = WaitForSingleObject(semaphore_, ((1000 * bufferFrameCount_) / sampleRate_) >> (fastMode ? 1 : 0));

    if (result != WAIT_TIMEOUT && result != WAIT_OBJECT_0)
    {
        // Fallback to a simple sleep.
        IAudioDevice::stopRealTimeWork(fastMode);
    }
}

void WASAPIAudioDevice::clearHelperRealTime()
{
    if (HelperTask_ != nullptr)
    {
        AvRevertMmThreadCharacteristics(HelperTask_);
        HelperTask_ = nullptr;
    }
}

void WASAPIAudioDevice::renderAudio_(ComPtr<IAudioRenderClient> renderClient)
{
    // If client is no longer available, abort
    if (!renderClient)
    {
        return;
    }

    // Get available buffer space
    UINT32 padding = 0;
    UINT32 framesAvailable = 0;
    BYTE* data = nullptr;
    HRESULT hr = client_->GetCurrentPadding(&padding);
    if (FAILED(hr))
    {
        // Note: don't call to event handler to avoid annoying the user
        // with popups.
        std::stringstream ss;
        ss << "Could not get current padding (hr = " << hr << ")";
        log_error(ss.str().c_str());
        return;
    }

    framesAvailable = bufferFrameCount_ - padding;
    hr = renderClient->GetBuffer(framesAvailable, &data);
    if (FAILED(hr))
    {
        // Note: don't call to event handler to avoid annoying the user
        // with popups.
        std::stringstream ss;
        ss << "Could not get render buffer (hr = " << hr << ")";
        log_error(ss.str().c_str());
        return;
    }

    // Grab audio data from higher level code
    if (framesAvailable > 0 && data != nullptr)
    {
        memset(tmpBuf_, 0, framesAvailable * numChannels_ * sizeof(short));
        if (onAudioDataFunction)
        {
            onAudioDataFunction(*this, tmpBuf_, framesAvailable, onAudioDataState);
        }
        copyToWindowsBuffer_(data, framesAvailable);
    }

    // Release render buffer
    hr = renderClient->ReleaseBuffer(framesAvailable, 0);
    if (FAILED(hr))
    {
        // Note: don't call to event handler to avoid annoying the user
        // with popups.
        std::stringstream ss;
        ss << "Could not release render buffer (hr = " << hr << ")";
        log_error(ss.str().c_str());
        return;
    }
}

void WASAPIAudioDevice::captureAudio_(ComPtr<IAudioCaptureClient> captureClient)
{
    // If client is no longer available, abort
    if (!captureClient)
    {
        return;
    }

    // Get packet length
    UINT32 packetLength = 0;
    HRESULT hr = captureClient->GetNextPacketSize(&packetLength);
    if (FAILED(hr))
    {
        // Note: don't call to event handler to avoid annoying the user
        // with popups.
        std::stringstream ss;
        ss << "Could not get packet length (hr = " << hr << ")";
        log_error(ss.str().c_str());
        return;
    }

    while(packetLength != 0)
    {
        BYTE* data = nullptr;
        UINT32 numFramesAvailable = 0;
        DWORD flags = 0;

        hr = captureClient->GetBuffer(
            &data,
            &numFramesAvailable,
            &flags,
            nullptr,
            nullptr);
        if (FAILED(hr))
        {
            // Note: don't call to event handler to avoid annoying the user
            // with popups.
            std::stringstream ss;
            ss << "Could not get capture buffer (hr = " << hr << ")";
            log_error(ss.str().c_str());
            return;
        }

        if (numFramesAvailable > 0 && data != nullptr)
        {
            // Fill buffer with silence if told to do so.
            if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
            {
                memset(tmpBuf_, 0, numFramesAvailable * numChannels_ * sizeof(short));
            }
            else
            {
                copyFromWindowsBuffer_(data, numFramesAvailable);
            }

            // Pass data to higher level code
            if (onAudioDataFunction)
            {
                onAudioDataFunction(*this, tmpBuf_, numFramesAvailable, onAudioDataState);
            }
        }

        // Release buffer
        hr = captureClient->ReleaseBuffer(numFramesAvailable);
        if (FAILED(hr))
        {
            // Note: don't call to event handler to avoid annoying the user
            // with popups.
            std::stringstream ss;
            ss << "Could not release capture buffer (hr = " << hr << ")";
            log_error(ss.str().c_str());
            return;
        }

        hr = captureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr))
        {
            // Note: don't call to event handler to avoid annoying the user
            // with popups.
            std::stringstream ss;
            ss << "Could not get packet length (hr = " << hr << ")";
            log_error(ss.str().c_str());
            return;
        }
    }

    if (semaphore_ != nullptr)
    {
        // Notify worker threads
        ReleaseSemaphore(semaphore_, 1, nullptr);
    }
}

void WASAPIAudioDevice::copyFromWindowsBuffer_(void* buf, int numFrames)
{
    if (isFloatingPoint_)
    {
        if (validBits_ == sizeof(float) * 8)
        {
            copyFloatToShort_<float>((float*)buf, numFrames);
        }
        else if (validBits_ == sizeof(double) * 8)
        {
            copyFloatToShort_<double>((double*)buf, numFrames);
        }
    }
    else
    {
        if (containerBits_ == 8)
        {
            copyIntToShort_<char>((char*)buf, numFrames);
        }
        else if (containerBits_ == 16 && validBits_ == 16)
        {
            // Shortcut -- can just memcpy into tmpBuf_.
            memcpy(tmpBuf_, buf, numFrames * numChannels_ * sizeof(short));
        }
        else if (containerBits_ == 16)
        {
            copyIntToShort_<short>((short*)buf, numFrames);
        }
        else if (containerBits_ == 32)
        {
            copyIntToShort_<int32_t>((int32_t*)buf, numFrames);
        }
    }
}

void WASAPIAudioDevice::copyToWindowsBuffer_(void* buf, int numFrames)
{
    if (isFloatingPoint_)
    {
        if (validBits_ == sizeof(float) * 8)
        {
            copyShortToFloat_<float>((float*)buf, numFrames);
        }
        else if (validBits_ == sizeof(double) * 8)
        {
            copyShortToFloat_<double>((double*)buf, numFrames);
        }
    }
    else
    {
        if (containerBits_ == 8)
        {
            copyShortToInt_<char>((char*)buf, numFrames);
        }
        else if (containerBits_ == 16 && validBits_ == 16)
        {
            // Shortcut -- can just memcpy from tmpBuf_.
            memcpy(buf, tmpBuf_, numFrames * numChannels_ * sizeof(short));
        }
        else if (containerBits_ == 16)
        {
            copyShortToInt_<short>((short*)buf, numFrames);
        }
        else if (containerBits_ == 32)
        {
            copyShortToInt_<int32_t>((int32_t*)buf, numFrames);
        }
    }
}

std::string WASAPIAudioDevice::GuidToString_(GUID *guid)
{
    char guid_string[37]; // 32 hex chars + 4 hyphens + null terminator
    snprintf(
          guid_string, sizeof(guid_string),
          "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
          (unsigned int)guid->Data1, (unsigned int)guid->Data2, (unsigned int)guid->Data3,
          guid->Data4[0], guid->Data4[1], guid->Data4[2],
          guid->Data4[3], guid->Data4[4], guid->Data4[5],
          guid->Data4[6], guid->Data4[7]);
    return guid_string;
}
