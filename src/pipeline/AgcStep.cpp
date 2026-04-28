//=========================================================================
// Name:            AgcStep.cpp
// Purpose:         Describes an AGC step in the audio pipeline.
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

#include <atomic>
#include <cmath>

#include "AgcStep.h"
#include "../util/logging/ulog.h"
#include "ebur128.h" // from libebur128

#include <assert.h>

// AGC settings
constexpr float AGC_LOUDNESS_TARGET_LUFS = -23.0;
constexpr float AGC_MAX_GAIN_DB = 12.0;
constexpr float AGC_MIN_GAIN_DB = -20.0;
constexpr float AGC_ATTACK_TIME_SEC = 0.5;
constexpr float AGC_RELEASE_TIME_SEC = 6.0;
constexpr float SILENCE_THRESHOLD_LUFS = -33.0;
constexpr int LIMITER_LEVEL_DB = -1;

constexpr int TEN_MS_DIVIDER = 100;
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
    agcConfig_.targetLevelDbfs = -LIMITER_LEVEL_DB; // default 3 (-3 dBOv)
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

int AgcStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}

int AgcStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}

short* AgcStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
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
            double lufs = 0.0;

            // Note: libebur128 is unlikely to use RT-unsafe constructs in normal operation
            // (per existing RTSan-enabled tests). Verified on 2025-09-30.
            FREEDV_BEGIN_VERIFIED_SAFE
            ebur128_add_frames_short(state, tmpInput, numSamplesPerRun_);
            auto result = ebur128_loudness_momentary(state, &lufs);
            FREEDV_END_VERIFIED_SAFE

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
            }

            // Scale samples based on current gain.
            float scaleFactor = expf(currentGainDb_/20.0f * logf(10.0f));
            float temp = 0;
            for (auto ctr = 0; ctr < numSamplesPerRun_; ctr++)
            {
                ConvertToFloatSampleType_<float, short>(&tmpInput[ctr], &temp, 1);
                temp *= scaleFactor;
                ConvertToIntSampleType_<short, float>(&temp, &tmpInput[ctr], 1);
            }

            // Run WebRTC to make sure we don't clip.
            int outMicLevel = 0;
            int inMicLevel = 0;
            short echo = 0;
            unsigned char saturationWarning = 1;
            WebRtcAgc_Process(
                agcState_, const_cast<const int16_t *const *>(&tmpInput), 1, numSamplesPerRun_, 
                const_cast<int16_t *const *>(&tmpOutput), inMicLevel, &outMicLevel, echo, &saturationWarning);
            tmpOutput += numSamplesPerRun_;
        }
    }
    else if (numInputSamples > 0 && inputSamples != nullptr)
    {
        inputSampleFifo_.write(inputSamples, numInputSamples);
    }
    
    return outputSamples;
}

void AgcStep::reset() FREEDV_NONBLOCKING
{
    inputSampleFifo_.reset();
    currentGainDb_ = 0;
    targetGainDb_ = 0;
}
