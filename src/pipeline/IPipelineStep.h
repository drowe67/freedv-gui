//=========================================================================
// Name:            IPipelineStep.h
// Purpose:         Describes a step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__I_PIPELINE_STEP_H
#define AUDIO_PIPELINE__I_PIPELINE_STEP_H

#include <cmath>
#include <limits>
#include "../util/sanitizers.h"

class IPipelineStep
{
public:
    virtual ~IPipelineStep();
    
    // Returns required input sample rate.
    virtual int getInputSampleRate() const FREEDV_NONBLOCKING = 0;
    
    // Returns output sample rate after performing the pipeline step.
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING = 0;
    
    // Executes pipeline step.
    // Required parameters:
    //     inputSamples: Array of int16 values corresponding to input audio.
    //     numInputSamples: Number of samples in the input array.
    //     numOutputSamples: Location to store number of output samples.
    // Returns: Array of int16 values corresponding to result audio.
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING = 0;
    
    // Resets internal state of the pipeline step.
    virtual void reset() FREEDV_NONBLOCKING { /* empty */ }

protected:
    // Helper method that converts integer samples to floating point samples
    template<typename DstType, typename SrcType, SrcType SampleDivisor = std::numeric_limits<SrcType>::max()>
    static void ConvertToFloatSampleType_(SrcType* sourceSamples, DstType* destSamples, std::size_t numSamples);

    // Helper method that converts floating point samples to integer samples
    template<typename DstType, typename SrcType, DstType SampleMultiplier = std::numeric_limits<DstType>::max()>
    static void ConvertToIntSampleType_(SrcType* sourceSamples, DstType* destSamples, std::size_t numSamples);
};

template<typename DstType, typename SrcType, SrcType SampleDivisor>
void IPipelineStep::ConvertToFloatSampleType_(SrcType* sourceSamples, DstType* destSamples, std::size_t numSamples)
{
    // Make sure template types are correct
    static_assert(std::numeric_limits<DstType>::is_iec559);
    static_assert(std::numeric_limits<SrcType>::is_integer);

    // Make sure divisor isn't zero
    static_assert(SampleDivisor != 0);

    // Iterate through samples, performing division (if needed) and typecasting
    for (std::size_t index = 0; index < numSamples; index++)
    {
        destSamples[index] = (DstType)sourceSamples[index] / ((DstType)SampleDivisor);
    }
}

template<typename DstType, typename SrcType, DstType SampleMultiplier>
void IPipelineStep::ConvertToIntSampleType_(SrcType* sourceSamples, DstType* destSamples, std::size_t numSamples)
{
    // Make sure template types are correct
    static_assert(std::numeric_limits<SrcType>::is_iec559);
    static_assert(std::numeric_limits<DstType>::is_integer);

    // Iterate through samples, performing multiplication (if needed) and typecasting
    for (std::size_t index = 0; index < numSamples; index++)
    {
        SrcType temp = sourceSamples[index] * (SrcType)SampleMultiplier;
        if (temp <= std::numeric_limits<DstType>::min())
        {
            destSamples[index] = std::numeric_limits<DstType>::min();
        }
        else if (temp >= std::numeric_limits<DstType>::max())
        {
            destSamples[index] = std::numeric_limits<DstType>::max();
        }
        else
        {
            destSamples[index] = static_cast<DstType>(std::llround(temp));
        }
    }
}

#endif // AUDIO_PIPELINE__I_PIPELINE_STEP_H
