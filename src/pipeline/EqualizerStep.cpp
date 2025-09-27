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

EqualizerStep::EqualizerStep(int sampleRate, bool* enableFilter, std::shared_ptr<void>* bassFilter, std::shared_ptr<void>* midFilter, std::shared_ptr<void>* trebleFilter, std::shared_ptr<void>* volFilter)
    : sampleRate_(sampleRate)
    , enableFilter_(enableFilter)
    , bassFilter_(bassFilter)
    , midFilter_(midFilter)
    , trebleFilter_(trebleFilter)
    , volFilter_(volFilter)
{
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    outputSamples_ = std::make_unique<short[]>(maxSamples);
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
    std::shared_ptr<void> tmpVolFilter = *volFilter_;

    if (tmpVolFilter != nullptr)
    {
        memcpy(outputSamples_.get(), inputSamples, sizeof(short)*numInputSamples);
        copiedToOutput = true;
        *numOutputSamples = numInputSamples;
        sox_biquad_filter(tmpVolFilter.get(), outputSamples_.get(), outputSamples_.get(), numInputSamples);
    }
    
    if (*enableFilter_)
    {
        if (!copiedToOutput)
        {
            memcpy(outputSamples_.get(), inputSamples, sizeof(short)*numInputSamples);
        }

        std::shared_ptr<void> tmpBassFilter = *bassFilter_;
        std::shared_ptr<void> tmpTrebleFilter = *trebleFilter_;
        std::shared_ptr<void> tmpMidFilter = *midFilter_;
        if (tmpBassFilter != nullptr)
        {
            sox_biquad_filter(tmpBassFilter.get(), outputSamples_.get(), outputSamples_.get(), numInputSamples);
        }
        if (tmpTrebleFilter != nullptr)
        {
            sox_biquad_filter(tmpTrebleFilter.get(), outputSamples_.get(), outputSamples_.get(), numInputSamples);
        }
        if (tmpMidFilter != nullptr)
        {
            sox_biquad_filter(tmpMidFilter.get(), outputSamples_.get(), outputSamples_.get(), numInputSamples);
        }
    } 
    else if (!copiedToOutput)
    {
        return inputSamples;
    }

    return outputSamples_.get();
}
