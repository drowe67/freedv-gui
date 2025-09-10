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

#include <assert.h>

// Constants from example WebRTC AGC program
constexpr int MAX_AGC_SAMPLES = 160;
constexpr int MIN_AGC_LEVEL = 0;
constexpr int MAX_AGC_LEVEL = 255;
constexpr int NUM_AGC_BANDS = 1; // number of audio channels?

AgcStep::AgcStep(int sampleRate)
    : sampleRate_(sampleRate == 8000 || sampleRate == 16000 || sampleRate == 32000 || sampleRate == 48000 ? sampleRate : 48000)
    , micLevel_(0)
    , inputSampleFifo_(sampleRate / 2)
{
    numSamplesPerRun_ = std::min(MAX_AGC_SAMPLES, sampleRate_ / 100); // 10ms blocks, 160 max samples
    assert(numSamplesPerRun_ > 0);
   
    agcState_ = WebRtcAgc_Create();
    assert(agcState_ != nullptr);

    auto status = WebRtcAgc_Init(agcState_, MIN_AGC_LEVEL, MAX_AGC_LEVEL, kAgcModeAdaptiveDigital, sampleRate_);
    if (status != 0)
    {
        log_error("Could not initialize WebRTC AGC (err = %d)", status);
        WebRtcAgc_Free(agcState_);
        agcState_ = nullptr;
    }

    // Set AGC configuration
    agcConfig_.compressionGaindB = 9; // default 9 dB
    agcConfig_.limiterEnable = 1; // default kAgcTrue (on)
    agcConfig_.targetLevelDbfs = 3; // default 3 (-3 dBOv)
    status = WebRtcAgc_set_config(agcState_, agcConfig_);
    if (status != 0)
    {
        log_error("Could not initialize WebRTC AGC config (err = %d)", status);
        WebRtcAgc_Free(agcState_);
        agcState_ = nullptr;
    }

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
    if (agcState_ == nullptr)
    {
        // Was not able to properly initialize - just pass through audio.
        *numOutputSamples = numInputSamples;
        return inputSamples;
    }

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
            int outMicLevel = -1;
            short echo = 0;
            unsigned char saturationWarning = 1;

            inputSampleFifo_.read(tmpInput, numSamplesPerRun_);
            auto status = WebRtcAgc_VirtualMic(
                agcState_, const_cast<int16_t *const *>(&tmpInput), NUM_AGC_BANDS, numSamplesPerRun_, micLevel_, &micLevel_);
            if (status != 0)
            {
                // XXX - not RT-safe
                log_error("Failed processing AGC AddMic (err = %d)", status);
            }
            status = WebRtcAgc_Process(
                agcState_, const_cast<const int16_t *const *>(&tmpInput), NUM_AGC_BANDS, numSamplesPerRun_, 
                const_cast<int16_t *const *>(&tmpOutput), micLevel_, &outMicLevel, echo, &saturationWarning);
            if (status != 0)
            {
                // XXX - not RT-safe
                log_error("Failed processing AGC (err = %d)", status);
            }
            tmpOutput += numSamplesPerRun_;
            micLevel_ = outMicLevel;
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
    micLevel_ = 0;
}
