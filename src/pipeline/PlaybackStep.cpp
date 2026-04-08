//=========================================================================
// Name:            PlaybackStep.cpp
// Purpose:         Describes a playback step in the audio pipeline.
//
// Authors:         Mooneer Salem
// License:
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//=========================================================================

#include "PlaybackStep.h"

#include <cassert>
#include <chrono>
#include "wx/thread.h"

#include "../util/logging/ulog.h"

#if defined(__APPLE__)
#include <pthread.h>
#endif // defined(__APPLE__)

#include "../os/os_interface.h"

extern wxMutex g_mutexProtectingCallbackData;

using namespace std::chrono_literals;

#define NUM_SECONDS_TO_READ 5
#define NUM_SECONDS_PER_OPERATION 1 /* to ensure we don't overflow ResampleStep buffer */

PlaybackStep::PlaybackStep(
    int inputSampleRate, std::function<int()> fileSampleRateFn, 
    std::function<SNDFILE*()> getSndFileFn, std::function<void()> fileCompleteFn)
    : inputSampleRate_(inputSampleRate)
    , fileSampleRateFn_(std::move(fileSampleRateFn))
    , getSndFileFn_(std::move(getSndFileFn))
    , fileCompleteFn_(std::move(fileCompleteFn))
    , nonRtThreadEnding_(false)
    , playbackResampler_(nullptr)
    , outputFifo_(inputSampleRate * NUM_SECONDS_TO_READ)
{
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    outputSamples_ = std::make_unique<short[]>(inputSampleRate_);
    assert(outputSamples_ != nullptr);
    
    // Create non-RT thread to perform audio I/O
    nonRtThread_ = std::thread(std::bind(&PlaybackStep::nonRtThreadEntry_, this));
}

PlaybackStep::~PlaybackStep()
{
    nonRtThreadEnding_.store(true, std::memory_order_release);
    fileIoThreadSem_.signal();
    if (nonRtThread_.joinable())
    {
        nonRtThread_.join();
    }
}

int PlaybackStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return inputSampleRate_;
}

int PlaybackStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return inputSampleRate_;
}

short* PlaybackStep::execute(short*, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    unsigned int nsf = numInputSamples * getOutputSampleRate()/getInputSampleRate();
    *numOutputSamples = std::min((unsigned int)outputFifo_.numUsed(), nsf);
    
    if (*numOutputSamples > 0)
    {
        outputFifo_.read(outputSamples_.get(), *numOutputSamples);
    }
   
    fileIoThreadSem_.signal();
    return outputSamples_.get();
}

void PlaybackStep::nonRtThreadEntry_()
{
    std::unique_ptr<short[]> buf = nullptr;

#if defined(__APPLE__)
    // Downgrade thread QoS to Utility to avoid thread contention issues. 
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#endif // defined(__APPLE__)

    SetThreadName("PlayStep");

    while (!nonRtThreadEnding_.load(std::memory_order_acquire))
    {
        g_mutexProtectingCallbackData.Lock();
        auto playFile = getSndFileFn_();
        if (playFile != nullptr)
        {
            auto fileSampleRate = fileSampleRateFn_();
            if (playbackResampler_ == nullptr || 
                playbackResampler_->getInputSampleRate() != fileSampleRate ||
                playbackResampler_->getOutputSampleRate() != getInputSampleRate())
            {
                log_info("Create SR (in = %d, out = %d)", fileSampleRate, inputSampleRate_);
                if (playbackResampler_ != nullptr)
                {
                    delete playbackResampler_;
                }
                playbackResampler_ = new ResampleStep(fileSampleRate, inputSampleRate_);
                assert(playbackResampler_ != nullptr);
                
                buf = nullptr;
                outputFifo_.reset();
            }

            if (buf == nullptr)
            {
                buf = std::make_unique<short[]>(fileSampleRate * NUM_SECONDS_TO_READ);
                assert(buf != nullptr);
            } 

            unsigned int nsf = outputFifo_.numFree();
            //log_info("nsf = %d", (int)nsf);
            while (nsf > 0)
            {
                int samplesAtSourceRate = nsf * fileSampleRate / inputSampleRate_;
                samplesAtSourceRate = std::min(samplesAtSourceRate, NUM_SECONDS_PER_OPERATION * fileSampleRate);
                unsigned int numRead = sf_read_short(playFile, buf.get(), samplesAtSourceRate);
         
                //log_info("samplesAtSource = %d, numRead = %u", samplesAtSourceRate, numRead);
                if (numRead > 0)
                {
                    if (playbackResampler_ != nullptr)
                    {
                        int outSamples = 0;
                        auto outBuf = playbackResampler_->execute(buf.get(), numRead, &outSamples);
                        //log_info("Resampled %u samples and created %d samples", numRead, outSamples);
                        if (outputFifo_.write(outBuf, outSamples) != 0)
                        {
                            log_warn("Could not write %d samples to buffer, dropping", outSamples);
                        }
                    }
                    else
                    {
                        //log_info("Writing %u samples to FIFO", numRead);
                        if (outputFifo_.write(buf.get(), numRead) != 0)
                        {
                            log_warn("Could not write %d samples to buffer, dropping", numRead);
                        }
                    }
                }

                if ((int)numRead == 0 && outputFifo_.numUsed() == 0)
                {
                    //log_info("file read complete");
                    buf = nullptr;

                    // Unlock prior to calling completion function just in case
                    // something in here causes the lock to be taken.
                    g_mutexProtectingCallbackData.Unlock();
                    fileCompleteFn_();
                    g_mutexProtectingCallbackData.Lock();
                }

                if ((int)numRead == 0)
                {
                    // early break out of loop if nothing more to read
                    break;
                }

                nsf = outputFifo_.numFree();
            }
        }
        else if (playbackResampler_ != nullptr)
        {
            log_info("Detected playback stop, reset");
            delete playbackResampler_;
            playbackResampler_ = nullptr;
            
            buf = nullptr;    
            outputFifo_.reset();
        }

        g_mutexProtectingCallbackData.Unlock();
        fileIoThreadSem_.wait();
    }

    if (playbackResampler_ != nullptr)
    {
        delete playbackResampler_;
    }
}

void PlaybackStep::reset() FREEDV_NONBLOCKING
{
    outputFifo_.reset();
}
