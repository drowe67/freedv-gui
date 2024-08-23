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

#ifndef AUDIO_PIPELINE__PARALLEL_STEP_H
#define AUDIO_PIPELINE__PARALLEL_STEP_H

#include "ResampleStep.h"
#include "IPipelineStep.h"
#include <functional>
#include <future>
#include <thread>
#include <vector>
#include <queue>
#include <map>

class ParallelStep : public IPipelineStep
{
public:
    ParallelStep(
        int inputSampleRate, int outputSampleRate,
        bool runMultiThreaded,
        std::function<int(ParallelStep*)> inputRouteFn,
        std::function<int(ParallelStep*)> outputRouteFn,
        std::vector<IPipelineStep*> parallelSteps,
        std::shared_ptr<void> state);
    virtual ~ParallelStep();
    
    virtual int getInputSampleRate() const override;
    virtual int getOutputSampleRate() const override;
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples) override;
    
    const std::vector<std::shared_ptr<IPipelineStep>> getParallelSteps() const { return parallelSteps_; }

    std::shared_ptr<void> getState() { return state_; }

    virtual void dump(int indentLevel = 0) override;
        
private:
    typedef std::pair<std::shared_ptr<short>, int> TaskResult;
    typedef std::packaged_task<TaskResult(std::shared_ptr<short>, int)> ThreadTask;
    
    struct TaskEntry
    {
        ThreadTask task;
        std::shared_ptr<short> inputSamples;
        int numInputSamples;
    };
    
    struct ThreadInfo
    {
        std::thread thread;
        std::mutex queueMutex;
        std::condition_variable queueCV;
        std::queue<TaskEntry> tasks;
        bool exitingThread;
    };
    
    int inputSampleRate_;
    int outputSampleRate_;
    bool runMultiThreaded_;
    std::function<int(ParallelStep*)> inputRouteFn_;
    std::function<int(ParallelStep*)> outputRouteFn_;
    std::vector<std::shared_ptr<IPipelineStep>> parallelSteps_;
    std::map<std::pair<int, int>, std::shared_ptr<ResampleStep>> resamplers_;
    std::vector<ThreadInfo*> threads_;
    std::shared_ptr<void> state_;

    void executeRunnerThread_(ThreadInfo* threadState);
    std::future<TaskResult> enqueueTask_(ThreadInfo* taskQueueThread, IPipelineStep* step, std::shared_ptr<short> inputSamples, int numInputSamples);
};

#endif // AUDIO_PIPELINE__PARALLEL_STEP_H
