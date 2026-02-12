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
    , inputFifo_(inputSampleRate + 1)
{
    int src_error;
    
    resampleState_ = speex_resampler_init(1, inputSampleRate_, outputSampleRate_, forPlotsOnly ? 5 : 7, &src_error);
    assert(resampleState_ != nullptr);
    
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    outputSamples_ = std::make_unique<short[]>(outputSampleRate);
    assert(outputSamples_ != nullptr);
    
    tempInput_ = new short[inputSampleRate_];
    assert(tempInput_ != nullptr);
}

ResampleStep::~ResampleStep()
{
    speex_resampler_destroy(resampleState_);
    delete[] tempInput_;
}

int ResampleStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return inputSampleRate_;
}

int ResampleStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return outputSampleRate_;
}

short* ResampleStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
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
    
    inputFifo_.write(inputSamples, numInputSamples);
    
    uint32_t inSampleSize = inputFifo_.numUsed();
    uint32_t newInSampleSize = inSampleSize;
    uint32_t outSampleSize = outputSampleRate_;
    inputFifo_.read(tempInput_, inSampleSize);
    auto outputPtr = outputSamples_.get();
        
    speex_resampler_process_int(resampleState_, 0, tempInput_, &newInSampleSize, outputPtr, &outSampleSize);

    if (newInSampleSize != inSampleSize)
    {
        inputFifo_.write(tempInput_ + newInSampleSize, inSampleSize - newInSampleSize);
    }
    
    *numOutputSamples = outSampleSize; 
    return outputSamples_.get();
}

void ResampleStep::reset() FREEDV_NONBLOCKING
{
    inputFifo_.reset();
    speex_resampler_reset_mem(resampleState_);
}
