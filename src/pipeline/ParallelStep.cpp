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

#include <chrono>
#include <cassert>
#include <cstring>
#include <sstream>
#include "../defines.h"
#include "ParallelStep.h"
#include "AudioPipeline.h"
#include "../util/logging/ulog.h"
#include "codec2_alloc.h"

using namespace std::chrono_literals;

ParallelStep::ParallelStep(
    int inputSampleRate, int outputSampleRate,
    bool runMultiThreaded,
    realtime_fp<int(ParallelStep*)> const& inputRouteFn,
    realtime_fp<int(ParallelStep*)> const& outputRouteFn,
    std::vector<IPipelineStep*> const& parallelSteps,
    std::shared_ptr<void> state,
    void* callbackState,
    std::shared_ptr<IRealtimeHelper> realtimeHelper)
    : inputSampleRate_(inputSampleRate)
    , outputSampleRate_(outputSampleRate)
    , runMultiThreaded_(runMultiThreaded)
    , inputRouteFn_(inputRouteFn)
    , outputRouteFn_(outputRouteFn)
    , realtimeHelper_(std::move(realtimeHelper))
    , state_(std::move(state))
    , callbackState_(callbackState)
{
    for (auto& step : parallelSteps)
    {
        parallelSteps_.push_back(step);
        auto pipeline = new AudioPipeline(inputSampleRate, outputSampleRate);
        pipeline->appendPipelineStep(step);
        
        auto threadState = new ThreadInfo();
        assert(threadState != nullptr);
        threads_.push_back(threadState);
        
        threadState->step = std::unique_ptr<IPipelineStep>(pipeline);
        threadState->inputFifo = codec2_fifo_create(inputSampleRate);
        assert(threadState->inputFifo != nullptr);
        threadState->outputFifo = codec2_fifo_create(outputSampleRate);
        assert(threadState->outputFifo != nullptr);
        
        threadState->tempOutput = std::make_unique<short[]>(outputSampleRate);
        threadState->tempInput = std::make_unique<short[]>(inputSampleRate);
         
        threadState->exitingThread.store(false, std::memory_order_release);
        if (runMultiThreaded)
        {
            // Initialize semaphore
#if defined(_WIN32)
            threadState->sem = CreateSemaphore(nullptr, 0, 1, nullptr);
            if (threadState->sem == nullptr)
            {
                std::stringstream ss;
                ss << "Could not create semaphore (err = " << GetLastError() << ")";
                log_warn(ss.str().c_str());
            }
#elif defined(__APPLE__)
            threadState->sem = dispatch_semaphore_create(0);
            if (threadState->sem == nullptr)
            {
                log_warn("Could not set up semaphore");
            }
#else
            if (sem_init(&threadState->sem, 0, 0) < 0)
            {
                log_warn("Could not set up semaphore (errno = %d)", errno);
            }
#endif // defined(_WIN32) || defined(__APPLE__)
            
            threadState->thread = std::thread([this](ThreadInfo* s) 
            {
                // Ensure that O(1) memory allocator is used for Codec2
                // instead of standard malloc().
                codec2_initialize_realtime(CODEC2_REAL_TIME_MEMORY_SIZE);
                
                if (realtimeHelper_)
                {
                    realtimeHelper_->setHelperRealTime();
                }
            
                while(!s->exitingThread.load(std::memory_order_acquire))
                {
                    bool fallbackToSleep = false;
                    auto beginTime = std::chrono::high_resolution_clock::now();
                    
#if defined(_WIN32) || defined(__APPLE__)
                    fallbackToSleep = s->sem == nullptr;
#endif // defined(_WIN32) || defined(__APPLE__)
                    
                    executeRunnerThread_(s);
                    
                    if (!fallbackToSleep)
                    {
#if defined(_WIN32)
                        DWORD result = WaitForSingleObject(s->sem, INFINITE);
                        if (result != WAIT_TIMEOUT && result != WAIT_OBJECT_0)
                        {
                            fallbackToSleep = true;
                        }
#elif defined(__APPLE__)
                        if (s->sem != nullptr)
                        {
                            dispatch_semaphore_wait(s->sem, DISPATCH_TIME_FOREVER);
                        }
#else
                        if (sem_wait(&s->sem) < 0)
                        {
                            fallbackToSleep = true;
                        }
#endif // defined(_WIN32) || defined(__APPLE__)
                    }
                    
                    if (fallbackToSleep)
                    {
                        std::this_thread::sleep_until(beginTime + std::chrono::milliseconds(1000 * FRAME_DURATION_MS));
                    }
                }
                
                if (realtimeHelper_)
                {
                    realtimeHelper_->clearHelperRealTime();
                }
                codec2_disable_realtime();
            }, threadState);
        }
    }
}
    
ParallelStep::~ParallelStep()
{
    // Exit all spawned threads, if any.
    for (auto& taskThread : threads_)
    {        
        taskThread->exitingThread.store(true, std::memory_order_release);
        if (taskThread->thread.joinable())
        {
            // Destroy semaphore
#if defined(_WIN32)
            if (taskThread->sem != nullptr)
            {
                auto tmpSem = taskThread->sem;
                taskThread->sem = nullptr;
                ReleaseSemaphore(tmpSem, 1, nullptr);
                CloseHandle(tmpSem);
            }
#elif defined(__APPLE__)
            if (taskThread->sem != nullptr)
            {
                dispatch_semaphore_signal(taskThread->sem);
                dispatch_release(taskThread->sem);
            }
#else
            sem_post(&taskThread->sem);
            sem_destroy(&taskThread->sem);
#endif // defined(_WIN32) || defined(__APPLE__)
            
            // Join thread
            taskThread->thread.join();
        }
                
        codec2_fifo_destroy(taskThread->inputFifo);
        codec2_fifo_destroy(taskThread->outputFifo);
        taskThread->step = nullptr;
        taskThread->tempInput = nullptr;
        taskThread->tempOutput = nullptr;
        delete taskThread;
    }

    // Force immediate memory clear
    parallelSteps_.clear();
}

int ParallelStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return inputSampleRate_;
}

int ParallelStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return outputSampleRate_;
}

short* ParallelStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    // Step 1: determine what steps to execute.
    auto stepToExecute = inputRouteFn_(this);
    assert(stepToExecute == -1 || (stepToExecute >= 0 && (size_t)stepToExecute < threads_.size()));
    
    // Step 2: execute steps
    for (size_t index = 0; index < threads_.size(); index++)
    {
        ThreadInfo* threadInfo = threads_[index];
        
        if (index == (size_t)stepToExecute || stepToExecute == -1)
        {
            codec2_fifo_write(threadInfo->inputFifo, inputSamples, numInputSamples);
            if (!runMultiThreaded_)
            {
                executeRunnerThread_(threadInfo);
            }
            else
            {
                // Wake up thread
                FREEDV_BEGIN_VERIFIED_SAFE
#if defined(_WIN32)
                if (threadInfo->sem != nullptr)
                {
                    ReleaseSemaphore(threadInfo->sem, 1, nullptr);
                }
#elif defined(__APPLE__)
                if (threadInfo->sem != nullptr)
                {
                    dispatch_semaphore_signal(threadInfo->sem);
                }
#else
                sem_post(&threadInfo->sem);
#endif // defined(_WIN32) || defined(__APPLE__)
                FREEDV_END_VERIFIED_SAFE
            }
            
            if (stepToExecute != -1) break;
        }
    }
    
    // Step 3: determine which output to return
    auto stepToOutput = outputRouteFn_(this);    
    assert(stepToOutput >= 0 && (size_t)stepToOutput < parallelSteps_.size());
    
    ThreadInfo* outputTask = threads_[stepToOutput];
    *numOutputSamples = codec2_fifo_used(outputTask->outputFifo);
    if (numInputSamples > 0)
    {
        *numOutputSamples = std::min(*numOutputSamples, numInputSamples);
    }
    if (*numOutputSamples == 0)
    {
        memset(outputTask->tempOutput.get(), 0, sizeof(short) * outputSampleRate_);
    }
    codec2_fifo_read(outputTask->outputFifo, outputTask->tempOutput.get(), *numOutputSamples);    
    return outputTask->tempOutput.get();
}

void ParallelStep::executeRunnerThread_(ThreadInfo* threadState) noexcept
#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
[[clang::nonblocking]]
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)
{
    int samplesIn = std::min((inputSampleRate_ * FRAME_DURATION_MS) / MS_TO_SEC, codec2_fifo_used(threadState->inputFifo));
    do
    {
        int samplesOut = 0;
        if (codec2_fifo_read(threadState->inputFifo, threadState->tempInput.get(), samplesIn) != 0)
        {
            samplesIn = 0;
        }

        auto output = threadState->step->execute(threadState->tempInput.get(), samplesIn, &samplesOut);
        if (samplesOut > 0)
        {
            codec2_fifo_write(threadState->outputFifo, output, samplesOut);
        }
        samplesIn = std::min((inputSampleRate_ * FRAME_DURATION_MS) / MS_TO_SEC, codec2_fifo_used(threadState->inputFifo));
    } while (samplesIn > 0 && !threadState->exitingThread.load(std::memory_order_acquire));
}

void ParallelStep::reset() FREEDV_NONBLOCKING
{
    for (size_t index = 0; index < threads_.size(); index++)
    {
        auto fifo = threads_[index]->inputFifo;
        short buf;
        
        while (codec2_fifo_used(fifo) > 0)
        {
            codec2_fifo_read(fifo, &buf, 1);
        }
        fifo = threads_[index]->outputFifo;
        while (codec2_fifo_used(fifo) > 0)
        {
            codec2_fifo_read(fifo, &buf, 1);
        }
    }
}
