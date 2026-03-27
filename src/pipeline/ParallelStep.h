//=========================================================================
// Name:            ParallelStep.h
// Purpose:         Describes a parallel step in the audio pipeline.
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
