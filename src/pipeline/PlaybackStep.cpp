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
    outputSamples_ = std::shared_ptr<short>(
        new short[maxSamples], 
        std::default_delete<short[]>());
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
    
    if (playbackResampler_ != nullptr)
    {
        delete playbackResampler_;
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

std::shared_ptr<short> PlaybackStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    unsigned int nsf = numInputSamples * getOutputSampleRate()/getInputSampleRate();
    *numOutputSamples = std::min((unsigned int)codec2_fifo_used(outputFifo_), nsf);
    
    if (*numOutputSamples > 0)
    {
        codec2_fifo_read(outputFifo_, outputSamples_.get(), *numOutputSamples);
    }

    return outputSamples_;
}

void PlaybackStep::nonRtThreadEntry_()
{
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    auto buf = std::shared_ptr<short>(
        new short[maxSamples],
        std::default_delete<short[]>());
    assert(buf != nullptr);
    
    while (!nonRtThreadEnding_)
    {
        g_mutexProtectingCallbackData.Lock();
        auto playFile = getSndFileFn_();
        if (playFile != nullptr)
        {
            auto fileSampleRate = fileSampleRateFn_();
            if (playbackResampler_ == nullptr || playbackResampler_->getInputSampleRate() != fileSampleRate)
            {
                if (playbackResampler_ != nullptr)
                {
                    delete playbackResampler_;
                }
                playbackResampler_ = new ResampleStep(fileSampleRate, inputSampleRate_);
                assert(playbackResampler_ != nullptr);
            }

            unsigned int nsf = codec2_fifo_free(outputFifo_);
            if (nsf > 0)
            {
                auto samplesAtSourceRate = nsf * inputSampleRate_ / fileSampleRate;
                unsigned int numRead = sf_read_short(playFile, buf.get(), samplesAtSourceRate);
                if (numRead < samplesAtSourceRate && codec2_fifo_used(outputFifo_) == 0)
                {
                    fileCompleteFn_();
                }
         
                int outSamples = 0; 
                auto outBuf = playbackResampler_->execute(buf, samplesAtSourceRate, &outSamples);
                codec2_fifo_write(outputFifo_, outBuf.get(), outSamples);
            }
        }
        g_mutexProtectingCallbackData.Unlock();
        
        std::this_thread::sleep_for(100ms);
    }
}
