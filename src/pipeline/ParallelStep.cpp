//=========================================================================
// Name:            ParallelStep.h
// Purpose:         Describes a parallel step in the audio pipeline.
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

#include <chrono>
#include <cassert>
#include <cstring>
#include "ParallelStep.h"
#include "AudioPipeline.h"
#include "../util/logging/ulog.h"

using namespace std::chrono_literals;

ParallelStep::ParallelStep(
    int inputSampleRate, int outputSampleRate,
    bool runMultiThreaded,
    std::function<int(ParallelStep*)> inputRouteFn,
    std::function<int(ParallelStep*)> outputRouteFn,
    std::vector<IPipelineStep*> parallelSteps,
    std::shared_ptr<void> state,
    std::shared_ptr<IRealtimeHelper> realtimeHelper)
    : inputSampleRate_(inputSampleRate)
    , outputSampleRate_(outputSampleRate)
    , runMultiThreaded_(runMultiThreaded)
    , inputRouteFn_(inputRouteFn)
    , outputRouteFn_(outputRouteFn)
    , realtimeHelper_(realtimeHelper)
    , state_(state)
{
    for (auto& step : parallelSteps)
    {
        auto sharedStep = std::shared_ptr<IPipelineStep>(step);
        parallelSteps_.push_back(sharedStep);
        auto pipeline = std::make_shared<AudioPipeline>(inputSampleRate, outputSampleRate);
        pipeline->appendPipelineStep(sharedStep);
        
        auto threadState = new ThreadInfo();
        assert(threadState != nullptr);
        threads_.push_back(threadState);
        
        threadState->step = pipeline;
        threadState->inputFifo = codec2_fifo_create(inputSampleRate);
        assert(threadState->inputFifo != nullptr);
        threadState->outputFifo = codec2_fifo_create(outputSampleRate);
        assert(threadState->outputFifo != nullptr);
        
        threadState->tempOutput = std::shared_ptr<short>(
            new short[outputSampleRate],
            std::default_delete<short[]>());
        threadState->tempInput = std::shared_ptr<short>(
            new short[inputSampleRate],
            std::default_delete<short[]>());
         
        threadState->exitingThread = false;
        if (runMultiThreaded)
        {
            threadState->thread = std::thread([this](ThreadInfo* s) 
            {
                if (realtimeHelper_)
                {
                    realtimeHelper_->setHelperRealTime();
                }
            
                while(!s->exitingThread)
                {
                    auto beginTime = std::chrono::high_resolution_clock::now();
                    executeRunnerThread_(s);
                    std::this_thread::sleep_until(beginTime + 10ms);
                }
                
                if (realtimeHelper_)
                {
                    realtimeHelper_->clearHelperRealTime();
                }
            }, threadState);
        }
    }
}
    
ParallelStep::~ParallelStep()
{
    // Exit all spawned threads, if any.
    for (auto& taskThread : threads_)
    {
        taskThread->exitingThread = true;
        if (taskThread->thread.joinable())
        {
            taskThread->thread.join();
        }
        
        codec2_fifo_destroy(taskThread->inputFifo);
        codec2_fifo_destroy(taskThread->outputFifo);
        delete taskThread;
    }
}

int ParallelStep::getInputSampleRate() const
{
    return inputSampleRate_;
}

int ParallelStep::getOutputSampleRate() const
{
    return outputSampleRate_;
}

std::shared_ptr<short> ParallelStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    // Step 1: determine what steps to execute.
    auto stepToExecute = inputRouteFn_(this);
    assert(stepToExecute == -1 || (stepToExecute >= 0 && (size_t)stepToExecute < threads_.size()));
    
    // Step 2: execute steps
    *numOutputSamples = numInputSamples * outputSampleRate_ / inputSampleRate_;
    for (size_t index = 0; index < threads_.size(); index++)
    {
        ThreadInfo* threadInfo = threads_[index];
        memset(threadInfo->tempOutput.get(), 0, sizeof(short) * outputSampleRate_);
        
        if (index == (size_t)stepToExecute || stepToExecute == -1)
        {
            codec2_fifo_write(threadInfo->inputFifo, inputSamples.get(), numInputSamples);
            if (!runMultiThreaded_)
            {
                executeRunnerThread_(threadInfo);
            }
            codec2_fifo_read(threadInfo->outputFifo, threadInfo->tempOutput.get(), *numOutputSamples);
        }
    }
    
    // Step 3: determine which output to return
    auto stepToOutput = outputRouteFn_(this);    
    assert(stepToOutput >= 0 && (size_t)stepToOutput < parallelSteps_.size());
    
    ThreadInfo* outputTask = threads_[stepToOutput];
    return outputTask->tempOutput;
}

void ParallelStep::executeRunnerThread_(ThreadInfo* threadState)
{
    int samplesIn = 0.01 * inputSampleRate_;
    if (codec2_fifo_read(threadState->inputFifo, threadState->tempInput.get(), samplesIn) == 0)
    {
        int samplesOut = 0;
        auto output = threadState->step->execute(threadState->tempInput, samplesIn, &samplesOut);
        if (samplesOut > 0)
        {
            codec2_fifo_write(threadState->outputFifo, output.get(), samplesOut);
        }
    }
}
