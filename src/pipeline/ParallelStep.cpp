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

#include <iostream>
#include <chrono>
#include <cassert>
#include "ParallelStep.h"

using namespace std::chrono_literals;

ParallelStep::ParallelStep(
    int inputSampleRate, int outputSampleRate,
    bool runMultiThreaded,
    std::function<int(ParallelStep*)> inputRouteFn,
    std::function<int(ParallelStep*)> outputRouteFn,
    std::vector<IPipelineStep*> parallelSteps,
    std::shared_ptr<void> state)
    : inputSampleRate_(inputSampleRate)
    , outputSampleRate_(outputSampleRate)
    , runMultiThreaded_(runMultiThreaded)
    , inputRouteFn_(inputRouteFn)
    , outputRouteFn_(outputRouteFn)
    , state_(state)
{
    for (auto& step : parallelSteps)
    {
        parallelSteps_.push_back(std::shared_ptr<IPipelineStep>(step));
        if (runMultiThreaded)
        {
            auto state = new ThreadInfo();
            assert(state != nullptr);
            
            state->exitingThread = false;
            state->thread = std::thread([this](ThreadInfo* threadState) 
            {
                executeRunnerThread_(threadState);
            }, state);
            
            threads_.push_back(state);
        }
    }
}
    
ParallelStep::~ParallelStep()
{
    // Exit all spawned threads, if any.
    for (auto& taskThread : threads_)
    {
        taskThread->exitingThread = true;
        taskThread->queueCV.notify_one();
        taskThread->thread.join();
        
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
    // Step 0: determine what steps to execute.
    auto stepToExecute = inputRouteFn_(this);
    assert(stepToExecute == -1 || (stepToExecute >= 0 && (size_t)stepToExecute < parallelSteps_.size()));
    
    // Step 1: resample inputs as needed
    std::map<int, std::future<TaskResult>> resampledInputFutures;
    std::map<int, TaskResult> resampledInputs;
    for (size_t index = 0; index < parallelSteps_.size(); index++)
    {
        if (index == (size_t)stepToExecute || stepToExecute == -1)
        {
            int destinationSampleRate = parallelSteps_[index]->getInputSampleRate();
            if (destinationSampleRate != inputSampleRate_)
            {
                ThreadInfo* threadInfo = nullptr;
                if (runMultiThreaded_)
                {
                    threadInfo = threads_[index];
                }
                
                if (resampledInputFutures.find(destinationSampleRate) == resampledInputFutures.end())
                {
                    if (resamplers_.find(std::pair<int, int>(inputSampleRate_, destinationSampleRate)) == resamplers_.end())
                    {
                        auto tmpStep = std::make_shared<ResampleStep>(inputSampleRate_, destinationSampleRate);
                        resamplers_[std::pair<int, int>(inputSampleRate_, destinationSampleRate)] = tmpStep;
                    }
                    
                    resampledInputFutures[destinationSampleRate] = enqueueTask_(threadInfo, resamplers_[std::pair<int, int>(inputSampleRate_, destinationSampleRate)].get(), inputSamples, numInputSamples);
                }
            }
            else
            {
                resampledInputs[inputSampleRate_] = TaskResult(inputSamples, numInputSamples);
            }
        }
    }
    
    for (auto& fut : resampledInputFutures)
    {
        resampledInputs[fut.first] = fut.second.get();
    }
    
    // Step 2: execute steps
    std::vector<std::future<TaskResult>> executedResultFutures;
    std::vector<TaskResult> executedResults;
    for (size_t index = 0; index < parallelSteps_.size(); index++)
    {
        if (index == (size_t)stepToExecute || stepToExecute == -1)
        {
            ThreadInfo* threadInfo = nullptr;
            if (runMultiThreaded_)
            {
                threadInfo = threads_[index];
            }
            
            int destinationSampleRate = parallelSteps_[index]->getInputSampleRate();
            auto resampledInput = resampledInputs[destinationSampleRate];
            executedResultFutures.push_back(enqueueTask_(threadInfo, parallelSteps_[index].get(), resampledInput.first, resampledInput.second));
        }
        else
        {
            std::promise<TaskResult> tempPromise;
            tempPromise.set_value(TaskResult(nullptr, 0));
            
            std::future<TaskResult> tempFuture = tempPromise.get_future();
            executedResultFutures.push_back(std::move(tempFuture));
        }
    }
    
    for (auto& fut : executedResultFutures)
    {
        executedResults.push_back(fut.get());
    }
    
    // Step 3: determine which output to return
    auto stepToOutput = outputRouteFn_(this);
    
    assert(stepToOutput >= 0 && (size_t)stepToOutput < executedResults.size());
    
    TaskResult output = executedResults[stepToOutput];
    
    // Step 4: resample to destination rate
    int sourceRate = parallelSteps_[stepToOutput]->getOutputSampleRate();
    if (sourceRate == outputSampleRate_)
    {
        *numOutputSamples = output.second;
        return output.first;
    }
    else
    {
        if (resamplers_.find(std::pair<int, int>(sourceRate, outputSampleRate_)) == resamplers_.end())
        {
            auto tmpStep = std::make_shared<ResampleStep>(sourceRate, outputSampleRate_);
            resamplers_[std::pair<int, int>(sourceRate, outputSampleRate_)] = tmpStep;
        }
    
        return resamplers_[std::pair<int, int>(sourceRate, outputSampleRate_)]->execute(output.first, output.second, numOutputSamples);
    }
}

void ParallelStep::executeRunnerThread_(ThreadInfo* threadState)
{
#if defined(__linux__)
    pthread_setname_np(pthread_self(), "FreeDV PS");
#endif // defined(__linux__)

    while(!threadState->exitingThread)
    {
        std::unique_lock<std::mutex> lock(threadState->queueMutex);
        threadState->queueCV.wait_for(lock, 20ms);
        if (!threadState->exitingThread && threadState->tasks.size() > 0)
        {
            auto& taskEntry = threadState->tasks.front();
            lock.unlock();
            taskEntry.task(taskEntry.inputSamples, taskEntry.numInputSamples);
            lock.lock();
            threadState->tasks.pop();
        }
    }
}

std::future<ParallelStep::TaskResult> ParallelStep::enqueueTask_(ThreadInfo* taskQueueThread, IPipelineStep* step, std::shared_ptr<short> inputSamples, int numInputSamples)
{
    TaskEntry taskEntry;
    taskEntry.task = ThreadTask([step](std::shared_ptr<short> inSamples, int numSamples)
    {
        int numOutputSamples = 0;
        auto result = step->execute(inSamples, numSamples, &numOutputSamples);
        return TaskResult(result, numOutputSamples);
    });
    
    taskEntry.inputSamples = inputSamples;
    taskEntry.numInputSamples = numInputSamples;
    
    auto theFuture = taskEntry.task.get_future();
    if (taskQueueThread != nullptr)
    {
        std::unique_lock<std::mutex> lock(taskQueueThread->queueMutex);
        taskQueueThread->tasks.push(std::move(taskEntry));
        taskQueueThread->queueCV.notify_one();
    }
    else
    {
        // Execute the task immediately as there's no thread to post it to.
        taskEntry.task(inputSamples, numInputSamples);
    }
    
    return theFuture;
}

void ParallelStep::dump(int indentLevel)
{
    IPipelineStep::dump(indentLevel);
    indentLevel += 4;

    for (int index = 0; index < parallelSteps_.size(); index++)
    {
        std::cout << std::string(indentLevel, ' ') << "[" << index << "] ";
        if (getInputSampleRate() != parallelSteps_[index]->getInputSampleRate())
        {
            std::cout << "[resample from " << getInputSampleRate() << " to " << parallelSteps_[index]->getInputSampleRate() << "] ";
        }
        parallelSteps_[index]->dump(indentLevel);
        if (parallelSteps_[index]->getOutputSampleRate() != getOutputSampleRate())
        {
            std::cout << std::string(indentLevel + 4, ' ') << "[resample from " << parallelSteps_[index]->getOutputSampleRate() << " to " << getOutputSampleRate() << "] " << std::endl;
        }
    }
}
