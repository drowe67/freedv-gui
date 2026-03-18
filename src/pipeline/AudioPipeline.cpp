//=========================================================================
// Name:            AudioPipeline.cpp
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

#include <cassert>
#include "AudioPipeline.h"
#include "../util/logging/ulog.h"

AudioPipeline::AudioPipeline(int inputSampleRate, int outputSampleRate)
    : inputSampleRate_(inputSampleRate)
    , outputSampleRate_(outputSampleRate)
{
    if (inputSampleRate_ != outputSampleRate_) reloadResultResampler_();
}

AudioPipeline::~AudioPipeline()
{
    // empty, unique_ptr will automatically deallocate.
}

int AudioPipeline::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return inputSampleRate_;
}

int AudioPipeline::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return outputSampleRate_;
}

void AudioPipeline::dumpSetup() const
{
    auto stepSize = pipelineSteps_.size();
    log_debug("Start at SR %d", getInputSampleRate());
    log_debug("Number of pipeline steps: %d", stepSize);
    for (size_t index = 0; index < stepSize; index++)
    {
        if (resamplers_[index] != nullptr)
        {
            log_debug("  Step %d: resample %d to %d", index, resamplers_[index]->getInputSampleRate(), resamplers_[index]->getOutputSampleRate());
        }
        else
        {
            log_debug("  Step %d: no resampler");
        }
        log_debug("  Step %d: input SR %d, output SR %d", index, pipelineSteps_[index]->getInputSampleRate(), pipelineSteps_[index]->getOutputSampleRate());
    }
    if (resultSampler_ != nullptr)
    {
        log_debug("  Result: resample %d to %d", resultSampler_->getInputSampleRate(), resultSampler_->getOutputSampleRate());
    }
    log_debug("End at SR %d", getOutputSampleRate());
}

short* AudioPipeline::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    short* tempInput = inputSamples;
    short* tempResult = inputSamples;
    int tempInputSamples = numInputSamples;
    int tempOutputSamples = tempInputSamples;

    auto numSteps = pipelineSteps_.size();
    for (size_t index = 0; index < numSteps; index++)
    {
        auto& resampler = resamplers_[index];
        auto& step = pipelineSteps_[index];
        if (resampler != nullptr)
        {
            assert (resampler->getOutputSampleRate() == step->getInputSampleRate());
            tempResult = resampler->execute(tempInput, tempInputSamples, &tempOutputSamples);
            tempInput = tempResult;
            tempInputSamples = tempOutputSamples;
        }
        
        tempResult = step->execute(tempInput, tempInputSamples, &tempOutputSamples);
        tempInput = tempResult;
        tempInputSamples = tempOutputSamples;        
    }
    
    if (resultSampler_ != nullptr)
    {
        tempResult = resultSampler_->execute(tempInput, tempInputSamples, &tempOutputSamples);
    }
    
    *numOutputSamples = tempOutputSamples;
    return tempResult;
}

void AudioPipeline::appendPipelineStep(IPipelineStep* pipelineStep)
{
    pipelineSteps_.push_back(std::unique_ptr<IPipelineStep>(pipelineStep)); // take ownership of pointer
    resamplers_.push_back(nullptr); // will be updated by reloadResampler_() below.
    reloadResampler_(pipelineSteps_.size() - 1);
    reloadResultResampler_();
}

void AudioPipeline::reloadResampler_(int index)
{
    ResampleStep* resampleStep = resamplers_[index].get();
    bool createResampler = 
        resampleStep == nullptr || resampleStep->getOutputSampleRate() == pipelineSteps_[index]->getInputSampleRate();
    
    if (index != 0 && !createResampler)
    {
        // note: input sample rate for the pipeline as a whole is fixed.
        createResampler |= resampleStep->getInputSampleRate() != pipelineSteps_[index - 1]->getOutputSampleRate();
    }
    
    if (createResampler)
    {
        if (index == 0)
        {
            if (pipelineSteps_[0]->getInputSampleRate() != getInputSampleRate())
            {
                resampleStep = new ResampleStep(getInputSampleRate(), pipelineSteps_[0]->getInputSampleRate());
            }
            else
            {
                resampleStep = nullptr;
            }
        }
        else
        {
            int prevOutputSampleRate = pipelineSteps_[index - 1]->getOutputSampleRate();
            if (pipelineSteps_[index]->getInputSampleRate() != prevOutputSampleRate)
            {
                resampleStep = new ResampleStep(prevOutputSampleRate, pipelineSteps_[index]->getInputSampleRate());
            }
            else
            {
                resampleStep = nullptr;
            }
        }
    }
    
    resamplers_[index] = std::unique_ptr<ResampleStep>(resampleStep); // NOLINT
}

void AudioPipeline::reloadResultResampler_()
{
    bool createResampler = 
        resultSampler_ == nullptr || 
        (pipelineSteps_.size() == 0 && 
            (resultSampler_->getInputSampleRate() != getInputSampleRate() || resultSampler_->getOutputSampleRate() != getOutputSampleRate())) ||
        (pipelineSteps_.size() > 0 && resultSampler_->getInputSampleRate() != pipelineSteps_[pipelineSteps_.size() - 1]->getOutputSampleRate());
    
    if (createResampler)
    {
        int lastOutputSampleRate = 
            (pipelineSteps_.size() > 0) ? 
            (pipelineSteps_[pipelineSteps_.size() - 1]->getOutputSampleRate()) :
            getInputSampleRate();
        if (lastOutputSampleRate != getOutputSampleRate())
        {
            resultSampler_ = std::make_unique<ResampleStep>(lastOutputSampleRate, getOutputSampleRate());
        }
        else
        {
            resultSampler_ = nullptr;
        }
    }
}

void AudioPipeline::reset() FREEDV_NONBLOCKING
{
    for (auto& step : pipelineSteps_)
    {
        step->reset();
    }
    
    for (auto& step : resamplers_)
    {
        if (step != nullptr)
        {
            step->reset();
        }
    }
 
    if (resultSampler_ != nullptr)
    {
        resultSampler_->reset();
    }
}
