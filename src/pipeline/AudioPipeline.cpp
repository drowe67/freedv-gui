//=========================================================================
// Name:            AudioPipeline.cpp
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
    // empty, shared_ptr will automatically deallocate.
}

int AudioPipeline::getInputSampleRate() const
{
    return inputSampleRate_;
}

int AudioPipeline::getOutputSampleRate() const
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

std::shared_ptr<short> AudioPipeline::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    std::shared_ptr<short> tempInput = inputSamples;
    std::shared_ptr<short> tempResult = inputSamples;
    int tempInputSamples = numInputSamples;
    int tempOutputSamples = tempInputSamples;
    
    for (size_t index = 0; index < pipelineSteps_.size(); index++)
    {
        if (resamplers_[index] != nullptr)
        {
            if (resamplers_[index]->getOutputSampleRate() != pipelineSteps_[index]->getInputSampleRate())
            {
                resamplers_[index] = nullptr;
                reloadResampler_(index);
            }

            tempResult = resamplers_[index]->execute(tempInput, tempInputSamples, &tempOutputSamples);
            tempInput = tempResult;
            tempInputSamples = tempOutputSamples;
        }
        
        tempResult = pipelineSteps_[index]->execute(tempInput, tempInputSamples, &tempOutputSamples);
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

void AudioPipeline::appendPipelineStep(std::shared_ptr<IPipelineStep> pipelineStep)
{
    pipelineSteps_.push_back(pipelineStep);
    resamplers_.resize(pipelineSteps_.size(), nullptr); // will be updated by reloadResampler_() below.
    reloadResampler_(pipelineSteps_.size() - 1);
    reloadResultResampler_();
}

void AudioPipeline::reloadResampler_(int index)
{
    std::shared_ptr<ResampleStep> resampleStep = resamplers_[index];
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
                resampleStep = std::shared_ptr<ResampleStep>(
                    new ResampleStep(getInputSampleRate(), pipelineSteps_[0]->getInputSampleRate()));
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
                resampleStep = std::shared_ptr<ResampleStep>(
                    new ResampleStep(prevOutputSampleRate, pipelineSteps_[index]->getInputSampleRate()));
            }
            else
            {
                resampleStep = nullptr;
            }
        }
    }
    
    resamplers_[index] = resampleStep;
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
            resultSampler_ = std::shared_ptr<ResampleStep>(
                new ResampleStep(lastOutputSampleRate, getOutputSampleRate()));
        }
        else
        {
            resultSampler_ = nullptr;
        }
    }
}

void AudioPipeline::reset()
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
