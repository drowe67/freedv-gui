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
    soxr_io_spec_t ioSpec = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
    soxr_quality_spec_t qualSpec = soxr_quality_spec(SOXR_HQ, SOXR_HI_PREC_CLOCK | SOXR_DOUBLE_PRECISION);
    soxr_runtime_spec_t runtimeSpec = soxr_runtime_spec(1);

    qualSpec.passband_end = 0.8; // experimentally determined to reduce latency to acceptable levels (default 0.913)

    resampleState_ = soxr_create(
        inputSampleRate_,
        outputSampleRate_,
        1,
        nullptr,
        &ioSpec,
        &qualSpec,
        &runtimeSpec
    );
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
    soxr_delete(resampleState_);
    
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
    
    *numOutputSamples = 0;

    auto inputPtr = inputSamples;
    auto outputPtr = outputSamples_.get();
    while (numInputSamples > 0)
    {
        size_t inputSize = std::min(numInputSamples, inputSampleRate_ * 10 / 1000);
        size_t outputSize = ((float)outputSampleRate_ / inputSampleRate_) * inputSize;

        size_t inputUsed = 0;
        size_t outputUsed = 0;
        soxr_process(
            resampleState_, inputPtr, inputSize, &inputUsed,
            outputPtr, outputSize, &outputUsed);

        outputPtr += outputUsed;
        inputPtr += inputUsed;
        numInputSamples -= inputUsed;
        *numOutputSamples += outputUsed;
    }
    
    return outputSamples_.get();
}
