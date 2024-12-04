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
    soxr_quality_spec_t qualSpec = soxr_quality_spec(SOXR_HQ, SOXR_HI_PREC_CLOCK | SOXR_DOUBLE_PRECISION);
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

    inputFifo_ = codec2_fifo_create(inputSampleRate_ + 1);
    assert(inputFifo_ != nullptr);
    outputFifo_ = codec2_fifo_create(outputSampleRate_ + 1);
    assert(outputFifo_ != nullptr);
}

ResampleStep::~ResampleStep()
{
    soxr_delete(resampleState_);
    codec2_fifo_free(inputFifo_);
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
    const double NUM_SECONDS_PER_BLOCK = 0.01; // 10ms

    short* outputSamples = nullptr;
    *numOutputSamples = 0;

    if (numInputSamples > 0)
    {
        codec2_fifo_write(inputFifo_, inputSamples.get(), numInputSamples);
    
        int inputSamplesPerBlock = NUM_SECONDS_PER_BLOCK * inputSampleRate_;
        int expectedNumOutputSamples  = (double)numInputSamples * ((double)outputSampleRate_ / (double)inputSampleRate_);
        while (codec2_fifo_used(inputFifo_) > 0 && codec2_fifo_used(outputFifo_) < expectedNumOutputSamples)
        {
            short inputBuffer[inputSamplesPerBlock];
            short outputBuffer[expectedNumOutputSamples];
            
            int toRead = std::min(codec2_fifo_used(inputFifo_), inputSamplesPerBlock);
            codec2_fifo_read(inputFifo_, inputBuffer, toRead);

            size_t inputUsed = 0;
            size_t outputUsed = 0;
            soxr_process(
                resampleState_, inputBuffer, toRead, &inputUsed,
                outputBuffer, expectedNumOutputSamples, &outputUsed);
 
            if (outputUsed > 0)
            {
                codec2_fifo_write(outputFifo_, outputBuffer, outputUsed);
            }
        }

        // Because of how soxr works, we may get outputUsed == 0 for a significant amount of time
        // before getting a huge batch of samples at once. This logic is intended to smooth that out
        // and improve playback further down the line.
        //log_info("used = %d, expected = %d", codec2_fifo_used(outputFifo_), expectedNumOutputSamples);
        //if (codec2_fifo_used(outputFifo_) >= expectedNumOutputSamples)
        {
            *numOutputSamples = std::min(codec2_fifo_used(outputFifo_), expectedNumOutputSamples);
            outputSamples = new short[*numOutputSamples];
            assert(outputSamples != nullptr);

            codec2_fifo_read(outputFifo_, outputSamples, *numOutputSamples);
            //*numOutputSamples = expectedNumOutputSamples;
        }
    }
    else if (codec2_fifo_used(outputFifo_) > 0)
    {
        // If we have anything in our output FIFO, go ahead and send that over now.
        *numOutputSamples = codec2_fifo_used(outputFifo_);
        outputSamples = new short[*numOutputSamples];
        assert(outputSamples != nullptr);

        codec2_fifo_read(outputFifo_, outputSamples, *numOutputSamples);
    }
 
    return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
}
