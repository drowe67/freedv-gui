//=========================================================================
// Name:            ResampeStep.cpp
// Purpose:         Describes a resampling step in the audio pipeline.
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

#include "ResampleStep.h"

#include <algorithm>
#include <assert.h>
#include <cstdio>

#include "../util/logging/ulog.h"

ResampleStep::ResampleStep(int inputSampleRate, int outputSampleRate, bool forPlotsOnly)
    : inputSampleRate_(inputSampleRate)
    , outputSampleRate_(outputSampleRate)
{
    int src_error;
    
    resampleState_ = speex_resampler_init(1, inputSampleRate_, outputSampleRate_, forPlotsOnly ? 5 : 8, &src_error);
    assert(resampleState_ != nullptr);
    
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    outputSamples_ = std::make_unique<short[]>(outputSampleRate);
    assert(outputSamples_ != nullptr);
    
    tempInput_ = new float[inputSampleRate * 10 / 1000];
    assert(tempInput_ != nullptr);

    tempOutput_ = new float[outputSampleRate * 10 / 1000];
    assert(tempOutput_ != nullptr);
}

ResampleStep::~ResampleStep()
{
    speex_resampler_destroy(resampleState_);

    delete[] tempInput_;
    delete[] tempOutput_;
}

int ResampleStep::getInputSampleRate() const
{
    return inputSampleRate_;
}

int ResampleStep::getOutputSampleRate() const
{
    return outputSampleRate_;
}

short* ResampleStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples)
{
    if (numInputSamples == 0)
    {
        // Not generating any samples if we haven't gotten any.
        *numOutputSamples = 0;
        return inputSamples;
    }

    if (inputSampleRate_ == outputSampleRate_)
    {
        // shortcut - just return what we got.
        *numOutputSamples = numInputSamples;
        return inputSamples;
    }
    
    auto inputPtr = inputSamples;
    auto outputPtr = outputSamples_.get();

    uint32_t inSampleSize = numInputSamples;
    uint32_t outSampleSize = outputSampleRate_;
    speex_resampler_process_int(resampleState_, 0, inputPtr, &inSampleSize, outputPtr, &outSampleSize);
    *numOutputSamples = outSampleSize;
 
    return outputSamples_.get();
}
