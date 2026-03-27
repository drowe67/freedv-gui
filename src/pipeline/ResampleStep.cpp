//=========================================================================
// Name:            ResampeStep.cpp
// Purpose:         Describes a resampling step in the audio pipeline.
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
