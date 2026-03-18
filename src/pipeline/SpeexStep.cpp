//=========================================================================
// Name:            SpeexStep.cpp
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

#include "SpeexStep.h"
#include "pipeline_defines.h"

#include <assert.h>

SpeexStep::SpeexStep(int sampleRate)
    : sampleRate_(sampleRate)
    , numSamplesPerSpeexRun_((FRAME_DURATION_MS * sampleRate_) / MS_TO_SEC)
    , inputSampleFifo_(sampleRate / 2)
{
    assert(numSamplesPerSpeexRun_ > 0);
    
    speexStateObj_ = speex_preprocess_state_init(
                numSamplesPerSpeexRun_,
                sampleRate_);
    assert(speexStateObj_ != nullptr);
    
    // Pre-allocate buffers so we don't have to do so during real-time operation.
    outputSamples_ = std::make_unique<short[]>(sampleRate);
    assert(outputSamples_ != nullptr);
}

SpeexStep::~SpeexStep()
{
    outputSamples_ = nullptr;
    speex_preprocess_state_destroy(speexStateObj_);
}

int SpeexStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}

int SpeexStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return sampleRate_;
}

short* SpeexStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    *numOutputSamples = 0;

    short* outputSamples = outputSamples_.get();

    int numSpeexRuns = (inputSampleFifo_.numUsed() + numInputSamples) / numSamplesPerSpeexRun_;
    if (numSpeexRuns > 0)
    {
        *numOutputSamples = numSpeexRuns * numSamplesPerSpeexRun_;
        
        short* tmpOutput = outputSamples;
        
        inputSampleFifo_.write(inputSamples, numInputSamples);
        while (inputSampleFifo_.numUsed() >= numSamplesPerSpeexRun_)
        {
            inputSampleFifo_.read(tmpOutput, numSamplesPerSpeexRun_);

            // Note: Speex is unlikely to use RT-unsafe constructs in normal operation
            // (per existing RTSan-enabled tests). Verified on 2025-09-30.
            FREEDV_BEGIN_VERIFIED_SAFE
            speex_preprocess_run(speexStateObj_, tmpOutput);
            FREEDV_END_VERIFIED_SAFE

            tmpOutput += numSamplesPerSpeexRun_;
        }
    }
    else if (numInputSamples > 0 && inputSamples != nullptr)
    {
        inputSampleFifo_.write(inputSamples, numInputSamples);
    }
    
    return outputSamples;
}

void SpeexStep::reset() FREEDV_NONBLOCKING
{
    inputSampleFifo_.reset();
}
