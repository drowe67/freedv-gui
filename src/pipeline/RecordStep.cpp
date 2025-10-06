//=========================================================================
// Name:            RecordStep.cpp
// Purpose:         Describes a record step in the audio pipeline.
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

#include "RecordStep.h"

#include <chrono>
#include "wx/thread.h"

#if defined(__APPLE__)
#include <pthread.h>
#endif // defined(__APPLE__)

extern wxMutex g_mutexProtectingCallbackData;

using namespace std::chrono_literals;

RecordStep::RecordStep(
    int inputSampleRate, std::function<SNDFILE*()> getSndFileFn, 
    std::function<void(int)> isFileCompleteFn)
    : inputSampleRate_(inputSampleRate)
    , getSndFileFn_(getSndFileFn)
    , isFileCompleteFn_(isFileCompleteFn)
    , fileIoThreadEnding_(false)
{
    inputFifo_ = codec2_fifo_create(inputSampleRate_);
    assert(inputFifo_ != nullptr);
    
    fileIoThread_ = std::thread(std::bind(&RecordStep::fileIoThreadEntry_, this));
}

RecordStep::~RecordStep()
{
    fileIoThreadEnding_ = true;
    fileIoThreadSem_.signal();
    if (fileIoThread_.joinable())
    {
        fileIoThread_.join();
    }
    
    codec2_fifo_destroy(inputFifo_);
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
    codec2_fifo_write(inputFifo_, inputSamples, numInputSamples);
    fileIoThreadSem_.signal();
    
    *numOutputSamples = 0;
    return nullptr;
}

void RecordStep::reset() FREEDV_NONBLOCKING
{
    short buf;
    while (codec2_fifo_used(inputFifo_) > 0)
    {
        codec2_fifo_read(inputFifo_, &buf, 1);
    }
}

void RecordStep::fileIoThreadEntry_()
{
    short* buf = new short[inputSampleRate_];
    assert(buf != nullptr);

#if defined(__APPLE__)
    // Downgrade thread QoS to Utility to avoid thread contention issues.        
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#endif // defined(__APPLE__)

    while (!fileIoThreadEnding_)
    {
        g_mutexProtectingCallbackData.Lock();
        auto recordFile = getSndFileFn_();
        if (recordFile != nullptr)
        {
            int numInputSamples = codec2_fifo_used(inputFifo_);
            codec2_fifo_read(inputFifo_, buf, numInputSamples);
            
            sf_write_short(recordFile, buf, numInputSamples);
            isFileCompleteFn_(numInputSamples);
        }
        g_mutexProtectingCallbackData.Unlock();
        
        fileIoThreadSem_.wait();
    }
    
    delete[] buf;
}
