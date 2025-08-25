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
#include <cmath>
#include <algorithm>
#include "wx/thread.h"

#include "../util/logging/ulog.h"

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
    
    
    
    limiterInit_();
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
    std::shared_ptr<short> buf = nullptr;

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
                buf = std::shared_ptr<short>(
                    new short[fileSampleRate],
                    std::default_delete<short[]>());
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
                        auto outBuf = playbackResampler_->execute(buf, numRead, &outSamples);
                        if (limiterEnabled_) limiterProcess_(outBuf.get(), outSamples);
                        if (codec2_fifo_write(outputFifo_, outBuf.get(), outSamples) != 0)
                        {
                           log_warn("Could not write %d samples to buffer, dropping", outSamples);
                        }
                    }
                    else
                    {
                        //log_info("Writing %u samples to FIFO", numRead);
                        if (limiterEnabled_) limiterProcess_(buf.get(), (int)numRead);
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
        
        std::this_thread::sleep_for(100ms);
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
    // Limiter state reset
    limiterGain_ = 1.0f;
    limW_ = limR_ = 0;
    std::fill(limiterDL_.begin(), limiterDL_.end(), 0.0f);
}


// ---------- Limiter impl ----------
static inline float clamp1f(float x) {
    return x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x);
}

void PlaybackStep::limiterInit_() {
    // Defaults: threshold -1 dBFS, look-ahead 5 ms, attack 1 ms, release 50 ms
    const float lookahead_ms = 5.0f;
    const float attack_ms    = 1.0f;
    const float release_ms   = 50.0f;
    limiterDelayN_ = std::max(1, (int)std::lround(lookahead_ms * getInputSampleRate() / 1000.0));
    limiterDL_.assign((size_t)limiterDelayN_, 0.0f);
    limW_ = limR_ = 0;
    limiterGain_ = 1.0f;
    auto ms2a = [&](float ms){
        float denom = std::max(1.0f, ms) * getInputSampleRate() / 1000.0f;
        return std::exp(-1.0f / denom);
    };
    limiterAttackA_  = ms2a(attack_ms);
    limiterReleaseA_ = ms2a(release_ms);
}

void PlaybackStep::limiterProcess_(short* buf, int n) {
    for (int i = 0; i < n; ++i) {
        // write current sample into delay
        float x = (float)buf[i] * (1.0f / 32768.0f);
        limiterDL_[limW_] = x;

        // detector on current sample
        float pk = std::fabs(x);
        float gT = std::min(1.0f, limiterThr_ / std::max(pk, 1e-12f));
        limiterGain_ = (gT < limiterGain_) ? (limiterAttackA_ * limiterGain_ + (1.0f - limiterAttackA_) * gT)
                                           : (limiterReleaseA_ * limiterGain_ + (1.0f - limiterReleaseA_) * gT);
        float y = clamp1f(limiterDL_[limR_] * limiterGain_);
        buf[i] = (short)std::lround(y * 32767.0f);
        limW_ = (limW_ + 1) % (size_t)limiterDelayN_;
        limR_ = (limR_ + 1) % (size_t)limiterDelayN_;
    }
}
