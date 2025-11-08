//=========================================================================
// Name:            PlaybackStep.cpp
// Purpose:         Describes a playback step in the audio pipeline.
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

#include "PlaybackStep.h"

#include <cassert>
#include <chrono>
#include "wx/thread.h"

#include "../util/logging/ulog.h"

#if defined(__APPLE__)
#include <pthread.h>
#endif // defined(__APPLE__)

extern wxMutex g_mutexProtectingCallbackData;

using namespace std::chrono_literals;

#define NUM_SECONDS_TO_READ 1

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
    nonRtThreadEnding_ = true;
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

    while (!nonRtThreadEnding_)
    {
        g_mutexProtectingCallbackData.Lock();
        auto playFile = getSndFileFn_();
        if (playFile != nullptr)
        {
            auto fileSampleRate = fileSampleRateFn_();
            if (getInputSampleRate() != fileSampleRate && (
                playbackResampler_ == nullptr || 
                playbackResampler_->getInputSampleRate() != fileSampleRate ||
                playbackResampler_->getOutputSampleRate() != getInputSampleRate()))
            {
                //log_info("Create SR (in = %d, out = %d)", fileSampleRate, inputSampleRate_);
                if (playbackResampler_ != nullptr)
                {
                    delete playbackResampler_;
                }
                playbackResampler_ = new ResampleStep(fileSampleRate, inputSampleRate_);
                assert(playbackResampler_ != nullptr);
            }

            if (buf == nullptr)
            {
                buf = std::make_unique<short[]>(fileSampleRate * NUM_SECONDS_TO_READ);
                assert(buf != nullptr);
            } 

            unsigned int nsf = outputFifo_.numFree();
            //log_info("nsf = %d", (int)nsf);
            if (nsf > 0)
            {
                int samplesAtSourceRate = nsf * fileSampleRate / inputSampleRate_;
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
            }
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
