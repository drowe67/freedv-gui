//=========================================================================
// Name:            AudioPipeline.h
// Purpose:         Describes an audio pipeline.
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

#ifndef AUDIO_PIPELINE__AUDIO_PIPELINE_H
#define AUDIO_PIPELINE__AUDIO_PIPELINE_H

#include <vector>
#include "IPipelineStep.h"
#include "ResampleStep.h"

class AudioPipeline : public IPipelineStep
{
public:
    AudioPipeline(int inputSampleRate, int outputSampleRate);
    virtual ~AudioPipeline();
    
    virtual int getInputSampleRate() const FREEDV_NONBLOCKING override;
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;
    virtual void reset() FREEDV_NONBLOCKING override;
    
    void appendPipelineStep(IPipelineStep* pipelineStep);
    
    void dumpSetup() const;
private:
    int inputSampleRate_;
    int outputSampleRate_;
    
    std::vector<std::unique_ptr<IPipelineStep>> pipelineSteps_;
    std::vector<std::unique_ptr<ResampleStep>> resamplers_;
    std::unique_ptr<ResampleStep> resultSampler_;
    
    void reloadResampler_(int pipelineStepIndex);
    void reloadResultResampler_();
};

#endif // AUDIO_PIPELINE__AUDIO_PIPELINE_H
