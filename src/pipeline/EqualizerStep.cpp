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

EqualizerStep::EqualizerStep(int sampleRate, bool* enableFilter, void** bassFilter, void** midFilter, void** trebleFilter, void** volFilter)
    : sampleRate_(sampleRate)
    , enableFilter_(enableFilter)
    , bassFilter_(bassFilter)
    , midFilter_(midFilter)
    , trebleFilter_(trebleFilter)
    , volFilter_(volFilter)
{
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    outputSamples_ = std::shared_ptr<short>(
        new short[maxSamples], 
        std::default_delete<short[]>());
    assert(outputSamples_ != nullptr);
}

EqualizerStep::~EqualizerStep()
{
    // empty
}

int EqualizerStep::getInputSampleRate() const
{
    return sampleRate_;
}

int EqualizerStep::getOutputSampleRate() const
{
    return sampleRate_;
}

std::shared_ptr<short> EqualizerStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    memcpy(outputSamples_.get(), inputSamples.get(), sizeof(short)*numInputSamples);
    
    if (*enableFilter_)
    {
        if (*bassFilter_)
        {
            sox_biquad_filter(*bassFilter_, outputSamples_.get(), outputSamples_.get(), numInputSamples);
        }
        if (*trebleFilter_)
        {
            sox_biquad_filter(*trebleFilter_, outputSamples_.get(), outputSamples_.get(), numInputSamples);
        }
        if (*midFilter_)
        {
            sox_biquad_filter(*midFilter_, outputSamples_.get(), outputSamples_.get(), numInputSamples);
        }
        if (*volFilter_)
        {
            sox_biquad_filter(*volFilter_, outputSamples_.get(), outputSamples_.get(), numInputSamples);
        }
    }
    
    *numOutputSamples = numInputSamples;
    return outputSamples_;
}