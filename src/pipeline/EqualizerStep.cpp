//=========================================================================
// Name:            EqualizerStep.cpp
// Purpose:         Describes an equalizer step in the audio pipeline.
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
    if (filterLock_.try_lock())
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
    }

    return outputSamples_.get();
}
