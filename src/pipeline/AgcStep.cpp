//=========================================================================
// Name:            AgcStep.cpp
// Purpose:         Describes an AGC step in the audio pipeline.
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

#include <atomic>

#include "AgcStep.h"
#include "../defines.h"
#include "../util/logging/ulog.h"
#include "ebur128.h" // from libebur128

#include <assert.h>

// AGC settings
constexpr float AGC_LOUDNESS_TARGET_LUFS = -23.0;
constexpr float AGC_MAX_GAIN_DB = 12.0;
constexpr float AGC_MIN_GAIN_DB = -20;
constexpr float AGC_ATTACK_TIME_SEC = 0.5;
constexpr float AGC_RELEASE_TIME_SEC = 6.0;
constexpr int TEN_MS_DIVIDER = 100;
constexpr float SILENCE_THRESHOLD_LUFS = -70.0; // from https://en.wikipedia.org/wiki/EBU_R_128
constexpr int MAX_AGC_SAMPLES = 160;

AgcStep::AgcStep(int sampleRate)
    : sampleRate_(sampleRate == 8000 || sampleRate == 16000 || sampleRate == 32000 || sampleRate == 48000 ? sampleRate : 48000)
    , targetGainDb_(0.0)
    , currentGainDb_(0.0)
    , inputSampleFifo_(sampleRate / 2)
{
    numSamplesPerRun_ = std::min(MAX_AGC_SAMPLES, sampleRate_ / TEN_MS_DIVIDER); // 10ms blocks, 160 max samples
    assert(numSamplesPerRun_ > 0);

    // Configure WebRTC as solely a limiter (i.e. no AGC)
    agcState_ = WebRtcAgc_Create();
    assert(agcState_ != nullptr);
 
    auto status = WebRtcAgc_Init(agcState_, 0, 255, kAgcModeUnchanged, sampleRate_);
    if (status != 0)
    {
        log_error("Could not initialize WebRTC AGC (err = %d)", status);
        WebRtcAgc_Free(agcState_);
        agcState_ = nullptr;
    }

    // Set AGC configuration
    agcConfig_.compressionGaindB = 0; // default 9 dB
    agcConfig_.limiterEnable = 1; // default kAgcTrue (on)
    agcConfig_.targetLevelDbfs = 3; // default 3 (-3 dBOv)
    status = WebRtcAgc_set_config(agcState_, agcConfig_);
    if (status != 0)
    {
        log_error("Could not initialize WebRTC AGC config (err = %d)", status);
        WebRtcAgc_Free(agcState_);
        agcState_ = nullptr;
    }

    ebur128State_ = ebur128_init(1, sampleRate_, EBUR128_MODE_S);
    assert(ebur128State_ != nullptr);

    // Pre-allocate buffers so we don't have to do so during real-time operation.
    outputSamples_ = std::make_unique<short[]>(sampleRate);
    assert(outputSamples_ != nullptr);

    tmpInput_ = std::make_unique<short[]>(numSamplesPerRun_);
    assert(tmpInput_ != nullptr);
}

AgcStep::~AgcStep()
{
    outputSamples_ = nullptr;
    WebRtcAgc_Free(agcState_);
    ebur128_destroy((ebur128_state**)&ebur128State_);
}

int AgcStep::getInputSampleRate() const
{
    return sampleRate_;
}

int AgcStep::getOutputSampleRate() const
{
    return sampleRate_;
}

short* AgcStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples)
{
    ebur128_state* state = static_cast<ebur128_state*>(ebur128State_);

    *numOutputSamples = 0;
    short* outputSamples = outputSamples_.get();

    int numRuns = (inputSampleFifo_.numUsed() + numInputSamples) / numSamplesPerRun_;
    if (numRuns > 0)
    {
        *numOutputSamples = numRuns * numSamplesPerRun_;
        
        short* tmpOutput = outputSamples;
        short* tmpInput = tmpInput_.get();

        inputSampleFifo_.write(inputSamples, numInputSamples);
        while (inputSampleFifo_.numUsed() >= numSamplesPerRun_)
        {
            inputSampleFifo_.read(tmpInput, numSamplesPerRun_);

            // Step 1: feed samples into ebur128 and return current
            // loudness in LUFS.
            ebur128_add_frames_short(state, tmpInput, numSamplesPerRun_);
            double lufs = 0.0;
            auto result = ebur128_loudness_momentary(state, &lufs);
            if (result == EBUR128_SUCCESS && lufs != -HUGE_VAL && lufs > SILENCE_THRESHOLD_LUFS)
            {
                // Returned loudness is valid.
                // Step 2: calculate target gain. Assume LUFS = dbFS (?)
                targetGainDb_ = AGC_LOUDNESS_TARGET_LUFS - lufs;
                if (targetGainDb_ >= AGC_MAX_GAIN_DB) targetGainDb_ = AGC_MAX_GAIN_DB;
                if (targetGainDb_ <= AGC_MIN_GAIN_DB) targetGainDb_ = AGC_MIN_GAIN_DB;

                // Step 3: increment/decrement current gain in the direction of target.
                float agcInterval = 0;
                if (targetGainDb_ < currentGainDb_)
                {
                    agcInterval = AGC_ATTACK_TIME_SEC;
                }
                else
                {
                    agcInterval = AGC_RELEASE_TIME_SEC;
                }
                currentGainDb_ += ((targetGainDb_ - currentGainDb_) / agcInterval) * ((float)numSamplesPerRun_ / sampleRate_);

                //log_info("LUFS: %f, targetGain: %f, currentGain: %f", lufs, targetGainDb_, currentGainDb_);
            }

            // Scale samples based on current gain.
            float scaleFactor = exp(currentGainDb_/20.0 * log(10.0));
            for (auto ctr = 0; ctr < numSamplesPerRun_; ctr++)
            {
                tmpInput[ctr] *= scaleFactor;
            }

            // Run WebRTC to make sure we don't clip.
            int outMicLevel = 0;
            int inMicLevel = 0;
            short echo = 0;
            unsigned char saturationWarning = 1;
            auto status = WebRtcAgc_Process(
                agcState_, const_cast<const int16_t *const *>(&tmpInput), 1, numSamplesPerRun_, 
                const_cast<int16_t *const *>(&tmpOutput), inMicLevel, &outMicLevel, echo, &saturationWarning);
            if (status != 0)
            {
                // XXX - not RT-safe
                log_error("Failed processing AGC (err = %d)", status);
            }
            tmpOutput += numSamplesPerRun_;
        }
    }
    else if (numInputSamples > 0 && inputSamples != nullptr)
    {
        inputSampleFifo_.write(inputSamples, numInputSamples);
    }
    
    return outputSamples;
}

void AgcStep::reset()
{
    inputSampleFifo_.reset();
    currentGainDb_ = 0;
    targetGainDb_ = 0;
}
