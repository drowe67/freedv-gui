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

PlaybackStep::PlaybackStep(
    int inputSampleRate, std::function<int()> fileSampleRateFn, 
    std::function<SNDFILE*()> getSndFileFn, std::function<void()> fileCompleteFn)
    : inputSampleRate_(inputSampleRate)
    , fileSampleRateFn_(fileSampleRateFn)
    , getSndFileFn_(getSndFileFn)
    , fileCompleteFn_(fileCompleteFn)
    , nonRtThreadEnding_(false)
    , playbackResampler_(nullptr)
{
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    outputSamples_ = std::make_unique<short[]>(maxSamples);
    assert(outputSamples_ != nullptr);
    
    // Create output FIFO
    outputFifo_ = codec2_fifo_create(maxSamples);
    assert(outputFifo_ != nullptr);
    
    // Create non-RT thread to perform audio I/O
    nonRtThread_ = std::thread(std::bind(&PlaybackStep::nonRtThreadEntry_, this));
}

PlaybackStep::~PlaybackStep()
{
    nonRtThreadEnding_ = true;
    if (nonRtThread_.joinable())
    {
        nonRtThread_.join();
    }
    
    codec2_fifo_destroy(outputFifo_);
}

int PlaybackStep::getInputSampleRate() const
{
    return inputSampleRate_;
}

int PlaybackStep::getOutputSampleRate() const
{
    return inputSampleRate_;
}

short* PlaybackStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples)
{
    unsigned int nsf = numInputSamples * getOutputSampleRate()/getInputSampleRate();
    *numOutputSamples = std::min((unsigned int)codec2_fifo_used(outputFifo_), nsf);
    
    if (*numOutputSamples > 0)
    {
        codec2_fifo_read(outputFifo_, outputSamples_.get(), *numOutputSamples);
    }

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
                buf = std::make_unique<short[]>(fileSampleRate);
                assert(buf != nullptr);
            } 

            unsigned int nsf = codec2_fifo_free(outputFifo_);
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
                        if (codec2_fifo_write(outputFifo_, outBuf, outSamples) != 0)
                        {
                            log_warn("Could not write %d samples to buffer, dropping", outSamples);
                        }
                    }
                    else
                    {
                        //log_info("Writing %u samples to FIFO", numRead);
                        if (codec2_fifo_write(outputFifo_, buf.get(), numRead) != 0)
                        {
                            log_warn("Could not write %d samples to buffer, dropping", numRead);
                        }
                    }
                }

                if ((int)numRead < samplesAtSourceRate && codec2_fifo_used(outputFifo_) == 0)
                {
                    //log_info("file read complete");
                    buf = nullptr;
                    fileCompleteFn_();
                }
            }
        }
        g_mutexProtectingCallbackData.Unlock();
        
        std::this_thread::sleep_for(101ms); // using prime number to reduce likelihood of contention
    }

    if (playbackResampler_ != nullptr)
    {
        delete playbackResampler_;
    }
}

void PlaybackStep::reset()
{
    short buf;
    while (codec2_fifo_used(outputFifo_) > 0)
    {
        codec2_fifo_read(outputFifo_, &buf, 1);
    }
}
