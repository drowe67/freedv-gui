//=========================================================================
// Name:            EqualizerStep.cpp
// Purpose:         Describes an equalizer step in the audio pipeline.
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

#include "EqualizerStep.h"

#include <algorithm>
#include <cstring>
#include "../sox_biquad.h"
#include <assert.h>

EqualizerStep::EqualizerStep(int sampleRate, bool* enableFilter, void** bassFilter, void** midFilter, void** trebleFilter, void** volFilter, audio_spin_mutex& filterLock)
    : sampleRate_(sampleRate)
    , enableFilter_(enableFilter)
    , bassFilter_(bassFilter)
    , midFilter_(midFilter)
    , trebleFilter_(trebleFilter)
    , volFilter_(volFilter)
    , filterLock_(filterLock)
{
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    outputSamples_ = std::make_unique<short[]>(sampleRate_);
    assert(outputSamples_ != nullptr);
}

EqualizerStep::~EqualizerStep()
{
    // empty
}

int EqualizerStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}

int EqualizerStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}

short* EqualizerStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    bool copiedToOutput = false;

    // Note: if we can't lock, an update is in progress. Just assume no filters enabled
    // until update completes.
    if (numInputSamples > 0 && filterLock_.try_lock())
    {
        if (*volFilter_ != nullptr)
        {
            memcpy(outputSamples_.get(), inputSamples, sizeof(short)*numInputSamples);
            copiedToOutput = true;
            *numOutputSamples = numInputSamples;
            sox_biquad_filter(*volFilter_, outputSamples_.get(), outputSamples_.get(), numInputSamples);
        }
    
        if (*enableFilter_)
        {
            if (!copiedToOutput)
            {
                memcpy(outputSamples_.get(), inputSamples, sizeof(short)*numInputSamples);
            }

            if (*bassFilter_ != nullptr)
            {
                sox_biquad_filter(*bassFilter_, outputSamples_.get(), outputSamples_.get(), numInputSamples);
            }
            if (*trebleFilter_ != nullptr)
            {
                sox_biquad_filter(*trebleFilter_, outputSamples_.get(), outputSamples_.get(), numInputSamples);
            }
            if (*midFilter_ != nullptr)
            {
                sox_biquad_filter(*midFilter_, outputSamples_.get(), outputSamples_.get(), numInputSamples);
            }
        } 
        else if (!copiedToOutput)
        {
            filterLock_.unlock();
            return inputSamples;
        }
        filterLock_.unlock();

        return outputSamples_.get();
    }
    else
    {
        *numOutputSamples = numInputSamples;
        return inputSamples;
    }
}
