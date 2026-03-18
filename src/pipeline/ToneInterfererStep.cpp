//=========================================================================
// Name:            ToneInterfererStep.cpp
// Purpose:         Describes a tone interferer step step in the audio pipeline.
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

#include <cassert>
#include <cstring>
#include <cmath>
#include <functional>
#include "ToneInterfererStep.h"

// M_PI is not available on some compilers, so define it here just in case.
#ifndef M_PI
    #define M_PI 3.1415926535897932384626433832795
#endif

ToneInterfererStep::ToneInterfererStep(
        int sampleRate, realtime_fp<float()> const& toneFrequencyFn, 
        realtime_fp<float()> const& toneAmplitudeFn, realtime_fp<float*()> const& tonePhaseFn)
    : sampleRate_(sampleRate)
    , toneFrequencyFn_(toneFrequencyFn)
    , toneAmplitudeFn_(toneAmplitudeFn)
    , tonePhaseFn_(tonePhaseFn)
{
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    outputSamples_ = std::make_unique<short[]>(sampleRate_);
    assert(outputSamples_ != nullptr);
}

ToneInterfererStep::~ToneInterfererStep()
{
    // empty
}

int ToneInterfererStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}

int ToneInterfererStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}

short* ToneInterfererStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    *numOutputSamples = numInputSamples;
    
    memcpy(outputSamples_.get(), inputSamples, numInputSamples * sizeof(short));
    
    auto toneFrequency = toneFrequencyFn_();
    auto toneAmplitude = toneAmplitudeFn_();
    auto tonePhase = tonePhaseFn_();
    
    float w = 2.0 * M_PI * toneFrequency / sampleRate_;
    for(int i = 0; i < numInputSamples; i++) {
        float s = (float)toneAmplitude * cosf(*tonePhase);
        outputSamples_.get()[i] += (int)s;
        *tonePhase += w;
    }
    *tonePhase -= 2.0 * M_PI * floor(*tonePhase / (2.0 * M_PI));
    
    return outputSamples_.get();
}
