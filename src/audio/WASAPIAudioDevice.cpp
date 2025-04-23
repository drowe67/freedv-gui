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

#define BLOCK_TIME_NS (0)

// Nanoseconds per REFERENCE_TIME unit
#define NS_PER_REFTIME (100)

thread_local HANDLE WASAPIAudioDevice::helperTask_ = nullptr;
    
WASAPIAudioDevice::WASAPIAudioDevice(IAudioClient* client, IAudioEngine::AudioDirection direction, int sampleRate, int numChannels)
    : client_(client)
    , renderClient_(nullptr)
    , captureClient_(nullptr)
    , direction_(direction)
    , sampleRate_(sampleRate)
    , numChannels_(numChannels)
    , bufferFrameCount_(0)
    , initialized_(false)
    , lowLatencyTask_(nullptr)
    , latencyFrames_(0)
    , renderCaptureEvent_(nullptr)
    , isRenderCaptureRunning_(false)
    , semaphore_(nullptr)
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
        client_->Release();
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
        WAVEFORMATEX streamFormat;

        // Populate stream format based on requested sample
        // rate/number of channels.
        // NOTE: this should already have been determined valid
        // by the audio engine!
        streamFormat.wFormatTag = WAVE_FORMAT_PCM;
        streamFormat.nChannels = numChannels_;
        streamFormat.nSamplesPerSec = sampleRate_;
        streamFormat.wBitsPerSample = 16;
        streamFormat.nBlockAlign = (numChannels_ * streamFormat.wBitsPerSample) / 8;
        streamFormat.nAvgBytesPerSec = streamFormat.nSamplesPerSec * streamFormat.nBlockAlign;
        streamFormat.cbSize = 0;

        if (!initialized_)
        {
            // Initialize the audio client with the above format
            HRESULT hr = client_->Initialize(
                AUDCLNT_SHAREMODE_SHARED,
                AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                BLOCK_TIME_NS / NS_PER_REFTIME, // REFERENCE_TIME is in 100ns units
                0,
                &streamFormat,
                nullptr);
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
        HRESULT hr = client_->SetEventHandle(renderCaptureEvent_);
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
                (void**)&captureClient_);
        }
        else
        {
            hr = client_->GetService(
                IID_IAudioRenderClient,
                (void**)&renderClient_);
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
                renderClient_->Release();
                renderClient_ = nullptr;
                prom->set_value();
                return;
            }

            memset(data, 0, bufferFrameCount_ * numChannels_ * sizeof(short));
            if (onAudioDataFunction)
            {
                onAudioDataFunction(*this, (short*)data, bufferFrameCount_, onAudioDataState);
            }

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
                renderClient_->Release();
                renderClient_ = nullptr;
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
            if (renderClient_ != nullptr)
            {
                renderClient_->Release();
                renderClient_ = nullptr;
            }
            if (captureClient_ != nullptr)
            {
                captureClient_->Release();
                captureClient_ = nullptr;
            }
        }

        // Start render/capture thread.
        isRenderCaptureRunning_ = true;
        renderCaptureThread_ = std::thread([&]() {
            log_info("Starting render/capture thread");

            HRESULT res = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
            if (FAILED(res))
            {
                log_warn("Could not initialize COM (res = %d)", res);
            }

            // Temporarily raise priority of task
            DWORD taskIndex = 0;
            lowLatencyTask_ = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
            if (lowLatencyTask_ == nullptr)
            {
                log_warn("Could not increase thread priority");
            }

            while (isRenderCaptureRunning_)
            {
                WaitForSingleObject(renderCaptureEvent_, 100);
                if (direction_ == IAudioEngine::AUDIO_ENGINE_OUT)
                {
                    renderAudio_();
                }
                else
                {
                    captureAudio_();
                }
            }

            log_info("Exiting render/capture thread");

            if (lowLatencyTask_ != nullptr)
            {
                AvRevertMmThreadCharacteristics(lowLatencyTask_);
                lowLatencyTask_ = nullptr;
            }

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
        
        if (renderClient_ != nullptr || captureClient_ != nullptr)
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

        if (renderClient_ != nullptr)
        {
            renderClient_->Release();
            renderClient_ = nullptr;
        }
        if (captureClient_ != nullptr)
        {
            captureClient_->Release();
            captureClient_ = nullptr;
        }

        if (semaphore_ != nullptr)
        {
            CloseHandle(semaphore_);
            semaphore_ = nullptr;
        }

        prom->set_value();
    });
    fut.wait();
}

bool WASAPIAudioDevice::isRunning()
{
    return (renderClient_ != nullptr) || (captureClient_ != nullptr);
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
    helperTask_ = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
    if (helperTask_ == nullptr)
    {
        log_warn("Could not increase thread priority");
    }
}

void WASAPIAudioDevice::startRealTimeWork() 
{
    // empty
}

void WASAPIAudioDevice::stopRealTimeWork() 
{
    if (semaphore_ == nullptr)
    {
        // Fallback to base class behavior
        IAudioDevice::stopRealTimeWork();
        return;
    }

    // Wait a maximum of (bufferSize / sampleRate) seconds for the semaphore to return
    DWORD result = WaitForSingleObject(semaphore_, (1000 * bufferFrameCount_) / sampleRate_);

    if (result != WAIT_TIMEOUT && result != WAIT_OBJECT_0)
    {
        // Fallback to a simple sleep.
        IAudioDevice::stopRealTimeWork();
    }
}

void WASAPIAudioDevice::clearHelperRealTime()
{
    if (helperTask_ != nullptr)
    {
        AvRevertMmThreadCharacteristics(helperTask_);
        helperTask_ = nullptr;
    }
}

void WASAPIAudioDevice::renderAudio_()
{
    // If client is no longer available, abort
    if (renderClient_ == nullptr)
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
    hr = renderClient_->GetBuffer(framesAvailable, &data);
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
    memset(data, 0, framesAvailable * numChannels_ * sizeof(short));
    if (onAudioDataFunction)
    {
        onAudioDataFunction(*this, (short*)data, framesAvailable, onAudioDataState);
    }

    // Release render buffer
    hr = renderClient_->ReleaseBuffer(framesAvailable, 0);
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

void WASAPIAudioDevice::captureAudio_()
{
    // If client is no longer available, abort
    if (captureClient_ == nullptr)
    {
        return;
    }

    // Get packet length
    UINT32 packetLength = 0;
    HRESULT hr = captureClient_->GetNextPacketSize(&packetLength);
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

        hr = captureClient_->GetBuffer(
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

        // Fill buffer with silence if told to do so.
        if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
        {
            memset(data, 0, numFramesAvailable * numChannels_ * sizeof(short));
        }

        // Pass data to higher level code
        if (onAudioDataFunction)
        {
            onAudioDataFunction(*this, (short*)data, numFramesAvailable, onAudioDataState);
        }

        // Release buffer
        hr = captureClient_->ReleaseBuffer(numFramesAvailable);
        if (FAILED(hr))
        {
            // Note: don't call to event handler to avoid annoying the user
            // with popups.
            std::stringstream ss;
            ss << "Could not release capture buffer (hr = " << hr << ")";
            log_error(ss.str().c_str());
            return;
        }

        hr = captureClient_->GetNextPacketSize(&packetLength);
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
