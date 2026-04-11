//=========================================================================
// Name:            RNNoiseStep.cpp
// Purpose:         Describes a noise reduction step in the audio pipeline.
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

#include <atomic>
#include <cmath>

#include "RNNoiseStep.h"
#include "pipeline_defines.h"

#include <limits.h>
#include <assert.h>

#define RNNOISE_SAMPLE_RATE (48000)
#define RNNOISE_FRAME_SIZE (480) /* 1ms */

RNNoiseStep::RNNoiseStep()
    : firstFrame_(true)
    , inputSampleFifo_(RNNOISE_SAMPLE_RATE / 2)
{
    rnnoise_ = rnnoise_create(nullptr);
    assert(rnnoise_ != nullptr);

    // Pre-allocate buffers so we don't have to do so during real-time operation.
    outputSamples_ = std::make_unique<short[]>(RNNOISE_SAMPLE_RATE);
    assert(outputSamples_ != nullptr);
}

RNNoiseStep::~RNNoiseStep()
{
    outputSamples_ = nullptr;
    rnnoise_destroy(rnnoise_);
}

int RNNoiseStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return RNNOISE_SAMPLE_RATE;
}

int RNNoiseStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return RNNOISE_SAMPLE_RATE;
}

short* RNNoiseStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    *numOutputSamples = 0;

    short* outputSamples = outputSamples_.get();

    int numRNNoiseRuns = (inputSampleFifo_.numUsed() + numInputSamples) / RNNOISE_FRAME_SIZE;
    if (numRNNoiseRuns > 0)
    {
        *numOutputSamples = numRNNoiseRuns * RNNOISE_FRAME_SIZE;
        
        short* tmpOutput = outputSamples;
        
        inputSampleFifo_.write(inputSamples, numInputSamples);
        while (inputSampleFifo_.numUsed() >= RNNOISE_FRAME_SIZE)
        {
            inputSampleFifo_.read(tmpOutput, RNNOISE_FRAME_SIZE);

            float tmpFloat[RNNOISE_FRAME_SIZE];
            for (int index = 0; index < RNNOISE_FRAME_SIZE; index++)
            {
                tmpFloat[index] = (float)tmpOutput[index];
            }

            // Note: RNNoise is unlikely to use RT-unsafe constructs in normal operation
            // (per existing RTSan-enabled tests). Verified on 2025-09-30.
            FREEDV_BEGIN_VERIFIED_SAFE
            rnnoise_process_frame(rnnoise_, tmpFloat, tmpFloat);
            FREEDV_END_VERIFIED_SAFE

            if (!firstFrame_)
            {
                for (int index = 0; index < RNNOISE_FRAME_SIZE; index++)
                {
                    float val = tmpFloat[index] * 32768.0;
                    if (val > 32767.0f) *tmpOutput = 32767;
                    else if (val < -32768.0f) *tmpOutput = -32768;
                    else *tmpOutput = static_cast<short>(lrintf(val));
                    tmpOutput++;
                }
            }

            firstFrame_ = false;
        }
    }
    else if (numInputSamples > 0 && inputSamples != nullptr)
    {
        inputSampleFifo_.write(inputSamples, numInputSamples);
    }
    
    return outputSamples;
}

void RNNoiseStep::reset() FREEDV_NONBLOCKING
{
    inputSampleFifo_.reset();
    firstFrame_ = true;
    rnnoise_init(rnnoise_, nullptr);
}
