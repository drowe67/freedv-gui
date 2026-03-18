//=========================================================================
// Name:            MuteStep.h
// Purpose:         Zeros out incoming audio.
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

#ifndef AUDIO_PIPELINE__MUTE_STEP_H
#define AUDIO_PIPELINE__MUTE_STEP_H

#include <memory>
#include "IPipelineStep.h"

class MuteStep : public IPipelineStep
{
public:
    MuteStep(int outputSampleRate);
    virtual ~MuteStep() = default;
    
    // Returns required input sample rate.
    virtual int getInputSampleRate() const FREEDV_NONBLOCKING override { return sampleRate_; }

    // Returns output sample rate after performing the pipeline step.
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override { return sampleRate_; }
    
    // Executes pipeline step.
    // Required parameters:
    //     inputSamples: Array of int16 values corresponding to input audio.
    //     numInputSamples: Number of samples in the input array.
    //     numOutputSamples: Location to store number of output samples.
    // Returns: Array of int16 values corresponding to result audio.
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;

private:
    int sampleRate_;
    std::unique_ptr<short[]> outputSamples_;
};

#endif // AUDIO_PIPELINE__MUTE_STEP_H
