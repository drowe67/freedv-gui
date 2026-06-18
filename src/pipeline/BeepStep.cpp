//=========================================================================
// Name:            BeepStep.cpp
// Purpose:         Describes a beep generator step in the audio pipeline.
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

#include <cmath>
#include <cassert>

#include "BeepStep.h"

static constexpr int TONE_AMPLITUDE = 8192;
static constexpr int SILENCE_BETWEEN_REPEATS_MS = 80;

// M_PI is not available on some compilers, so define it here just in case.
#ifndef M_PI
    #define M_PI 3.1415926535897932384626433832795
#endif

BeepStep::BeepStep(
    int sampleRate, int frequency, int durationMs, int repeats, 
    realtime_fp<bool()> const& isActiveFn,
    realtime_fp<void(BeepStep&)> const& onCompleteFn)
    : sampleRate_(sampleRate)
    , frequency_(frequency)
    , repeats_(repeats)
    , isActiveFn_(isActiveFn)
    , onCompleteFn_(onCompleteFn)
    
    // SR = (samples / 1s) * (1s / 1000ms) = samples / msec
    , samplesToGenerate_((sampleRate * durationMs) / 1000)
    , silenceToGenerate_((sampleRate * SILENCE_BETWEEN_REPEATS_MS) / 1000)
    , rampLength_((sampleRate * 5) / 1000) // 5 ms Hann ramp

    , sampleCtr_(0)
    , repeatCtr_(0)
    , silenceCtr_(0)
{
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    outputSamples_ = std::make_unique<short[]>(sampleRate);
    assert(outputSamples_ != nullptr);
}

BeepStep::~BeepStep()
{
    // empty
}
    
int BeepStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}

int BeepStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}    

short* BeepStep::execute(short*, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    *numOutputSamples = numInputSamples;

    short* outPtr = outputSamples_.get();
    for (int index = 0; index < numInputSamples; index++)
    {
        if (repeatCtr_ < repeats_ && isActiveFn_())
        {
            if (sampleCtr_ < samplesToGenerate_)
            {
                // First phase: actually generate the sine wave.
                // Beginning and end of beep should be ramped up and down.
                double env;
                if (sampleCtr_ < rampLength_)
                {
                    env = 0.5 * (1.0 - cosf(M_PI * sampleCtr_ / rampLength_));
                }
                else if (sampleCtr_ >= (samplesToGenerate_ - rampLength_))
                {
                    env = 0.5 * (1.0 + cosf(M_PI * (sampleCtr_ - (samplesToGenerate_ - rampLength_)) / rampLength_));
                }
                else
                {
                    env = 1.0;
                }
                outPtr[index] = (short)(TONE_AMPLITUDE * env * cosf((2.0 * M_PI * frequency_ * sampleCtr_) / sampleRate_));
                sampleCtr_++;
            }
            else if (silenceCtr_ < silenceToGenerate_)
            {
                // Second phase: add silence before the next repeat.
                outPtr[index] = 0;
                silenceCtr_++;

                if (silenceCtr_ == silenceToGenerate_)
                {
                    // Increment repeat counter, either start over at (1)
                    // or pass through input audio.
                    sampleCtr_ = 0;
                    silenceCtr_ = 0;
                    repeatCtr_++;
                }
            }
        }
        else
        {
            outPtr[index] = 0; // ignoring input, only generates beeps
            if (repeatCtr_ == repeats_)
            {
                // Call completion function.
                onCompleteFn_(*this);
            }
        }
    }

    return outPtr;
}

void BeepStep::reset() FREEDV_NONBLOCKING
{
    sampleCtr_ = 0;
    repeatCtr_ = 0;
    silenceCtr_ = 0;
}
