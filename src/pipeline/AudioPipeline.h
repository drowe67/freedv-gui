//=========================================================================
// Name:            AudioPipeline.h
// Purpose:         Describes an audio pipeline.
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
    
    virtual int getInputSampleRate() const;
    virtual int getOutputSampleRate() const;
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples);
    
    void appendPipelineStep(std::shared_ptr<IPipelineStep> pipelineStep);

    virtual void dump(int indentLevel = 0) override;
    
private:
    int inputSampleRate_;
    int outputSampleRate_;
    
    std::vector<std::shared_ptr<IPipelineStep>> pipelineSteps_;
    std::vector<std::shared_ptr<ResampleStep>> resamplers_;
    std::shared_ptr<ResampleStep> resultSampler_;
    
    void reloadResampler_(int pipelineStepIndex);
    void reloadResultResampler_();
};

#endif // AUDIO_PIPELINE__AUDIO_PIPELINE_H
