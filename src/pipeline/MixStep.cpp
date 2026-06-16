//=========================================================================
// Name:            MixStep.cpp
// Purpose:         Describes an audio mixing step in the audio pipeline.
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

#include <cassert>
#include <algorithm>

#include "MixStep.h"

MixStep::MixStep(IPipelineStep* firstChannel, IPipelineStep* secondChannel)
    : firstChannel_(firstChannel)
    , secondChannel_(secondChannel)
    , firstOutputFIFO_(firstChannel->getOutputSampleRate())
    , secondOutputFIFO_(secondChannel->getOutputSampleRate())
{
    assert(firstChannel_->getInputSampleRate() == secondChannel_->getInputSampleRate());
    assert(firstChannel_->getOutputSampleRate() == secondChannel_->getOutputSampleRate());

    // Pre-allocate buffers so we don't have to do so during real-time operation.
    outputSamples_ = std::make_unique<short[]>(firstChannel_->getOutputSampleRate());
    assert(outputSamples_ != nullptr);
}

MixStep::~MixStep()
{
    // empty
}
    
int MixStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return firstChannel_->getInputSampleRate();
}

int MixStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return firstChannel_->getOutputSampleRate();
}    

short* MixStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    int firstOutputNumSamples = 0;
    auto firstOutputPtr = firstChannel_->execute(inputSamples, numInputSamples, &firstOutputNumSamples);
    firstOutputFIFO_.write(firstOutputPtr, firstOutputNumSamples);

    int secondOutputNumSamples = 0;
    auto secondOutputPtr = secondChannel_->execute(inputSamples, numInputSamples, &secondOutputNumSamples);
    secondOutputFIFO_.write(secondOutputPtr, secondOutputNumSamples);

    *numOutputSamples = std::min(firstOutputFIFO_.numUsed(), secondOutputFIFO_.numUsed());
    if (*numOutputSamples == 0)
    {
        // If a FIFO doesn't have any data available, we want to pass through
        // the one that does ASAP.
        *numOutputSamples = std::max(firstOutputFIFO_.numUsed(), secondOutputFIFO_.numUsed());
    }
    
    short* outPtr = outputSamples_.get();
    for (int index = 0; index < *numOutputSamples; index++)
    {
        // Mixing algorithm:
        //
        // 1. Retrieve current sample from both channels. If one channel is shorter than the other,
        //    use 0.
        // 2. Convert samples to floating-point, add together and divide by sqrt(2).
        // 3. Just in case, use tanh() to soft clip result. 
        // 4. Convert result back to short and store in output.
        //
        // See https://dsp.stackexchange.com/questions/3581/algorithms-to-mix-audio-signals-without-clipping.
        short sampleLeft = 0;
        short sampleRight = 0;
        firstOutputFIFO_.read(&sampleLeft, 1);
        secondOutputFIFO_.read(&sampleRight, 1);

        float sampleLeftFloat = 0.f;
        float sampleRightFloat = 0.f;

        ConvertToFloatSampleType_<float, short>(&sampleLeft, &sampleLeftFloat, 1);
        ConvertToFloatSampleType_<float, short>(&sampleRight, &sampleRightFloat, 1);

        float mixedSample = std::tanh((sampleLeftFloat + sampleRightFloat) / std::sqrt(2.f));

        ConvertToIntSampleType_<short, float>(&mixedSample, &outPtr[index], 1);
    }

    return outPtr;
}

void MixStep::reset() FREEDV_NONBLOCKING
{
    firstChannel_->reset();
    secondChannel_->reset();
    firstOutputFIFO_.reset();
    secondOutputFIFO_.reset();
}
