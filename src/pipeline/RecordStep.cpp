//=========================================================================
// Name:            RecordStep.cpp
// Purpose:         Describes a record step in the audio pipeline.
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

#include "RecordStep.h"

#include <chrono>
#include "wx/thread.h"

#if defined(__APPLE__)
#include <pthread.h>
#endif // defined(__APPLE__)

#include "../os/os_interface.h"

extern wxMutex g_mutexProtectingCallbackData;

using namespace std::chrono_literals;

RecordStep::RecordStep(
    int inputSampleRate, std::function<SNDFILE*()> getSndFileFn, 
    std::function<void(int)> isFileCompleteFn)
    : inputSampleRate_(inputSampleRate)
    , getSndFileFn_(std::move(getSndFileFn))
    , isFileCompleteFn_(std::move(isFileCompleteFn))
    , inputFifo_(inputSampleRate_)
    , fileIoThreadEnding_(false)
{
    fileIoThread_ = std::thread(std::bind(&RecordStep::fileIoThreadEntry_, this));
}

RecordStep::~RecordStep()
{
    fileIoThreadEnding_.store(true, std::memory_order_release);
    fileIoThreadSem_.signal();
    if (fileIoThread_.joinable())
    {
        fileIoThread_.join();
    }
}

int RecordStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return inputSampleRate_;
}

int RecordStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return inputSampleRate_;
}

short* RecordStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{    
    inputFifo_.write(inputSamples, numInputSamples);
    fileIoThreadSem_.signal();
    
    *numOutputSamples = 0;
    return nullptr;
}

void RecordStep::reset() FREEDV_NONBLOCKING
{
    inputFifo_.reset();
}

void RecordStep::fileIoThreadEntry_()
{
    short* buf = new short[inputSampleRate_];
    assert(buf != nullptr);

#if defined(__APPLE__)
    // Downgrade thread QoS to Utility to avoid thread contention issues.        
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#endif // defined(__APPLE__)

    SetThreadName("RecordStep");

    while (!fileIoThreadEnding_.load(std::memory_order_acquire))
    {
        g_mutexProtectingCallbackData.Lock();
        auto recordFile = getSndFileFn_();
        if (recordFile != nullptr)
        {
            int numInputSamples = inputFifo_.numUsed();
            inputFifo_.read(buf, numInputSamples);
            
            sf_write_short(recordFile, buf, numInputSamples);
            isFileCompleteFn_(numInputSamples);
        }
        g_mutexProtectingCallbackData.Unlock();
        
        fileIoThreadSem_.wait();
    }
    
    delete[] buf;
}
