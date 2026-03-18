//=========================================================================
// Name:            ToneInterfererStep.h
// Purpose:         Describes a tone interferer step step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__TONE_INTERFERER_STEP_H
#define AUDIO_PIPELINE__TONE_INTERFERER_STEP_H

#include <memory>
#include "../util/realtime_fp.h"
#include "IPipelineStep.h"

class ToneInterfererStep : public IPipelineStep
{
public:
    ToneInterfererStep(
        int sampleRate, realtime_fp<float()> const& toneFrequencyFn, 
        realtime_fp<float()> const& toneAmplitudeFn, realtime_fp<float*()> const& tonePhaseFn);
    virtual ~ToneInterfererStep();
    
    virtual int getInputSampleRate() const FREEDV_NONBLOCKING override;
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;
    
private:
    int sampleRate_;
    realtime_fp<float()> toneFrequencyFn_;
    realtime_fp<float()> toneAmplitudeFn_;
    realtime_fp<float*()> tonePhaseFn_;
    std::unique_ptr<short[]> outputSamples_;
};

#endif // AUDIO_PIPELINE__TONE_INTERFERER_STEP_H
