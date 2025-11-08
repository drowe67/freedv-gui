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
#include <atomic>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#endif // defined(_WIN32) || defined(__APPLE__)

#include "../util/IRealtimeHelper.h"
#include "../util/realtime_fp.h"
#include "codec2_fifo.h"

class ParallelStep : public IPipelineStep
{
public:
    ParallelStep(
        int inputSampleRate, int outputSampleRate,
        bool runMultiThreaded,
        realtime_fp<int(ParallelStep*)> const& inputRouteFn,
        realtime_fp<int(ParallelStep*)> const& outputRouteFn,
        std::vector<IPipelineStep*> const& parallelSteps,
        std::shared_ptr<void> state,
        void* callbackState,
        std::shared_ptr<IRealtimeHelper> realtimeHelper);
    virtual ~ParallelStep();
    
    virtual int getInputSampleRate() const FREEDV_NONBLOCKING override;
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;
    virtual void reset() FREEDV_NONBLOCKING override;
    
    const std::vector<IPipelineStep*>& getParallelSteps() const { return parallelSteps_; }
    
    void* getState() { return state_.get(); }
    void* getCallbackState() const { return callbackState_; }

private:    
    struct ThreadInfo
    {
        std::thread thread;
        std::atomic<bool> exitingThread;
        std::unique_ptr<IPipelineStep> step;
        FIFO* inputFifo;
        FIFO* outputFifo;
        std::unique_ptr<short[]> tempInput;
        std::unique_ptr<short[]> tempOutput;
        
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
    realtime_fp<int(ParallelStep*)> inputRouteFn_;
    realtime_fp<int(ParallelStep*)> outputRouteFn_;
    std::vector<ThreadInfo*> threads_;
    std::shared_ptr<IRealtimeHelper> realtimeHelper_;
    std::shared_ptr<void> state_;
    std::vector<IPipelineStep*> parallelSteps_;
    void* callbackState_;

    void executeRunnerThread_(ThreadInfo* threadState) noexcept
#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
[[clang::nonblocking]]
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)
    ;
};

#endif // AUDIO_PIPELINE__PARALLEL_STEP_H
