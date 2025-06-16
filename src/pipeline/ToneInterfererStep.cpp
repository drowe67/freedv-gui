//=========================================================================
// Name:            ToneInterfererStep.cpp
// Purpose:         Describes a tone interferer step step in the audio pipeline.
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

#include <cassert>
#include <cstring>
#include <cmath>
#include "ToneInterfererStep.h"

// M_PI is not available on some compilers, so define it here just in case.
#ifndef M_PI
    #define M_PI 3.1415926535897932384626433832795
#endif

ToneInterfererStep::ToneInterfererStep(
        int sampleRate, std::function<float()> toneFrequencyFn, 
        std::function<float()> toneAmplitudeFn, std::function<float*()> tonePhaseFn)
    : sampleRate_(sampleRate)
    , toneFrequencyFn_(toneFrequencyFn)
    , toneAmplitudeFn_(toneAmplitudeFn)
    , tonePhaseFn_(tonePhaseFn)
{
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    outputSamples_ = std::shared_ptr<short>(
        AllocRealtime_<short>(maxSamples), 
        RealtimeDeleter<short>());
    assert(outputSamples_ != nullptr);
}

ToneInterfererStep::~ToneInterfererStep()
{
    outputSamples_ = nullptr;
}

int ToneInterfererStep::getInputSampleRate() const
{
    return sampleRate_;
}

int ToneInterfererStep::getOutputSampleRate() const
{
    return sampleRate_;
}

std::shared_ptr<short> ToneInterfererStep::execute(
    std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    *numOutputSamples = numInputSamples;
    
    memcpy(outputSamples_.get(), inputSamples.get(), numInputSamples * sizeof(short));
    
    auto toneFrequency = toneFrequencyFn_();
    auto toneAmplitude = toneAmplitudeFn_();
    auto tonePhase = tonePhaseFn_();
    
    float w = 2.0 * M_PI * toneFrequency / sampleRate_;
    for(int i = 0; i < numInputSamples; i++) {
        float s = (float)toneAmplitude * cos(*tonePhase);
        outputSamples_.get()[i] += (int)s;
        *tonePhase += w;
    }
    *tonePhase -= 2.0 * M_PI * floor(*tonePhase / (2.0 * M_PI));
    
    return outputSamples_;
}