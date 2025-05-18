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

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#endif // defined(_WIN32) || defined(__APPLE__)

#include "../util/IRealtimeHelper.h"
#include "codec2_fifo.h"

class ParallelStep : public IPipelineStep
{
public:
    ParallelStep(
        int inputSampleRate, int outputSampleRate,
        bool runMultiThreaded,
        std::function<int(ParallelStep*)> inputRouteFn,
        std::function<int(ParallelStep*)> outputRouteFn,
        std::vector<IPipelineStep*> parallelSteps,
        std::shared_ptr<void> state,
        std::shared_ptr<IRealtimeHelper> realtimeHelper);
    virtual ~ParallelStep();
    
    virtual int getInputSampleRate() const;
    virtual int getOutputSampleRate() const;
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples);
    
    const std::vector<std::shared_ptr<IPipelineStep>> getParallelSteps() const { return parallelSteps_; }
    
    std::shared_ptr<void> getState() { return state_; }
    
private:    
    struct ThreadInfo
    {
        std::thread thread;
        bool exitingThread;
        std::shared_ptr<IPipelineStep> step;
        FIFO* inputFifo;
        FIFO* outputFifo;
        std::shared_ptr<short> tempInput;
        std::shared_ptr<short> tempOutput;
        
#if defined(_WIN32)
        HANDLE sem;
#elif defined(__APPLE__)
        dispatch_semaphore_t sem;
#else
        sem_t sem;
#endif // defined(_WIN32) || defined(__APPLE__)
    };
    
    int inputSampleRate_;
    int outputSampleRate_;
    bool runMultiThreaded_;
    std::function<int(ParallelStep*)> inputRouteFn_;
    std::function<int(ParallelStep*)> outputRouteFn_;
    std::vector<ThreadInfo*> threads_;
    std::shared_ptr<IRealtimeHelper> realtimeHelper_;
    std::shared_ptr<void> state_;
    std::vector<std::shared_ptr<IPipelineStep>> parallelSteps_;

    void executeRunnerThread_(ThreadInfo* threadState);
};

#endif // AUDIO_PIPELINE__PARALLEL_STEP_H
