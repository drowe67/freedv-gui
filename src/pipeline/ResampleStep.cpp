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

    qualSpec.passband_end = 0.912; // experimentally determined to reduce latency to acceptable levels (default 0.913)
 
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

    outputFifo_ = codec2_fifo_create(std::max(inputSampleRate_, outputSampleRate_));
    assert(outputFifo_ != nullptr);
}

ResampleStep::~ResampleStep()
{
    soxr_delete(resampleState_);
    codec2_fifo_free(outputFifo_);
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
    *numOutputSamples = 0;

    if (numInputSamples > 0)
    {
        short outputBuffer[std::max(inputSampleRate_, outputSampleRate_)];
        int expectedNumOutputSamples  = (double)numInputSamples * ((double)outputSampleRate_ / (double)inputSampleRate_);
        size_t inputUsed = 0;
        size_t outputUsed = 0;
        soxr_process(
            resampleState_, inputSamples.get(), numInputSamples, &inputUsed,
            outputBuffer, sizeof(outputBuffer) / sizeof(short), &outputUsed);

        if (outputUsed > 0)
        {
            codec2_fifo_write(outputFifo_, outputBuffer, outputUsed);
        }

        // Because of how soxr works, we may get outputUsed == 0 for a significant amount of time
        // before getting a huge batch of samples at once. This logic is intended to smooth that out
        // and improve playback further down the line.
        if (codec2_fifo_used(outputFifo_) >= expectedNumOutputSamples)
        {
            outputSamples = new short[expectedNumOutputSamples];
            assert(outputSamples != nullptr);

            codec2_fifo_read(outputFifo_, outputSamples, expectedNumOutputSamples);
            *numOutputSamples = expectedNumOutputSamples;
        }
    }
 
    return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
}
