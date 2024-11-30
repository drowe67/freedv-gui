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

ResampleStep::ResampleStep(int inputSampleRate, int outputSampleRate)
    : inputSampleRate_(inputSampleRate)
    , outputSampleRate_(outputSampleRate)
{
    soxr_io_spec_t ioSpec = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
    soxr_quality_spec_t qualSpec = soxr_quality_spec(SOXR_HQ, 0);
    soxr_runtime_spec_t runtimeSpec = soxr_runtime_spec(1);

    runtimeSpec.log2_min_dft_size = 8;
    runtimeSpec.log2_large_dft_size = 12;
    
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
}

ResampleStep::~ResampleStep()
{
    soxr_delete(resampleState_);
}

int ResampleStep::getInputSampleRate() const
{
    return inputSampleRate_;
}

int ResampleStep::getOutputSampleRate() const
{
    return outputSampleRate_;
}

std::shared_ptr<short> ResampleStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    short* outputSamples = nullptr;
    if (numInputSamples > 0)
    {
        double scaleFactor = ((double)outputSampleRate_)/((double)inputSampleRate_);
        int outputArraySize = std::max(numInputSamples, (int)(scaleFactor*numInputSamples));
        assert(outputArraySize > 0);

        outputSamples = new short[outputArraySize];
        assert(outputSamples != nullptr);
        
        size_t inputUsed = 0;
        size_t outputUsed = 0;
        soxr_process(
            resampleState_, inputSamples.get(), numInputSamples, &inputUsed,
            outputSamples, outputArraySize, &outputUsed);
        *numOutputSamples = outputUsed;
    }
    else
    {
        *numOutputSamples = 0;
    }
 
    return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
}
