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

#define BLOCK_TIME_NS (40000000)

// Nanoseconds per REFERENCE_TIME unit
#define NS_PER_REFTIME (100)
    
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
                0,
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

        // Get actual allocated buffer size
        HRESULT hr = client_->GetBufferSize(&bufferFrameCount_);
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

            // Queue render handler
            enqueue_([&]() {
                renderAudio_();
            });
        }
        else
        {
            // Queue capture handler
            enqueue_([&]() {
                captureAudio_();
            });
        }

        // Temporarily raise priority of task
        DWORD taskIndex = 0;
        lowLatencyTask_ = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
        if (lowLatencyTask_ == nullptr)
        {
            log_warn("Could not increase thread priority");
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
            if (lowLatencyTask_ != nullptr)
            {
                AvRevertMmThreadCharacteristics(lowLatencyTask_);
                lowLatencyTask_ = nullptr;
            }
        }

        lastRenderCaptureTime_ = std::chrono::steady_clock::now();
 
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

            if (lowLatencyTask_ != nullptr)
            {
                AvRevertMmThreadCharacteristics(lowLatencyTask_);
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

void WASAPIAudioDevice::renderAudio_()
{
    // If client is no longer available, abort
    if (renderClient_ == nullptr)
    {
        return;
    }

    // Sleep 1/2 of the buffer duration
    int sleepDurationInMsec = 1000 * (double)bufferFrameCount_ / sampleRate_ / 2;
    std::this_thread::sleep_until(lastRenderCaptureTime_ + std::chrono::milliseconds(sleepDurationInMsec));

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
        goto render_again;
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
        goto render_again;
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
        goto render_again;
    }

render_again:
    enqueue_([&]() {
        renderAudio_();
    });

    lastRenderCaptureTime_ = std::chrono::steady_clock::now();
}

void WASAPIAudioDevice::captureAudio_()
{
    // If client is no longer available, abort
    if (captureClient_ == nullptr)
    {
        return;
    }

    // Sleep 1/2 of the buffer duration
    int sleepDurationInMsec = 1000 * (double)bufferFrameCount_ / sampleRate_ / 2;
    std::this_thread::sleep_until(lastRenderCaptureTime_ + std::chrono::milliseconds(sleepDurationInMsec));

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
        goto capture_again;
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
            goto capture_again;
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
            goto capture_again;
        }

        hr = captureClient_->GetNextPacketSize(&packetLength);
        if (FAILED(hr))
        {
            // Note: don't call to event handler to avoid annoying the user
            // with popups.
            std::stringstream ss;
            ss << "Could not get packet length (hr = " << hr << ")";
            log_error(ss.str().c_str());
            goto capture_again;
        }
    }

capture_again:
    enqueue_([&]() {
        captureAudio_();
    });

    lastRenderCaptureTime_ = std::chrono::steady_clock::now();
}
