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

// M_PI is not available on some compilers, so define it here just in case.
#ifndef M_PI
    #define M_PI 3.1415926535897932384626433832795
#endif

BeepStep::BeepStep(int sampleRate, int frequency, int durationMs, realtime_fp<void(BeepStep&)> const& onCompleteFn)
    : sampleRate_(sampleRate)
    , frequency_(frequency)
    , onCompleteFn_(onCompleteFn)
    
    // SR = (samples / 1s) * (1s / 1000ms) = samples / msec
    , samplesToGenerate_((sampleRate * durationMs) / 1000)

    , sampleCtr_(0)
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

short* BeepStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    *numOutputSamples = numInputSamples;

    short* outPtr = outputSamples_.get();
    for (int index = 0; index < numInputSamples; index++)
    {
        if (sampleCtr_ < samplesToGenerate_)
        {
            constexpr int TONE_AMPLITUDE = 8192; // TBD
            outPtr[index] = TONE_AMPLITUDE * cosf((2.0 * M_PI * frequency_ * sampleCtr_) / sampleRate_);
        }
        else
        {
            outPtr[index] = inputSamples[index];
        }

        if (sampleCtr_ == samplesToGenerate_)
        {
            onCompleteFn_(*this);
        }

        sampleCtr_++;
    }

    return outPtr;
}

void BeepStep::reset() FREEDV_NONBLOCKING
{
    sampleCtr_ = 0;
}
