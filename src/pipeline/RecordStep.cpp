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
    if (fileIoThread_.joinable())
    {
        fileIoThread_.join();
    }
    
    codec2_fifo_destroy(inputFifo_);
}

int RecordStep::getInputSampleRate() const
{
    return inputSampleRate_;
}

int RecordStep::getOutputSampleRate() const
{
    return inputSampleRate_;
}

std::shared_ptr<short> RecordStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{    
    codec2_fifo_write(inputFifo_, inputSamples.get(), numInputSamples);
    
    *numOutputSamples = 0;    
    return std::shared_ptr<short>((short*)nullptr);
}

void RecordStep::reset()
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
        
        std::this_thread::sleep_for(100ms);
    }
    
    delete[] buf;
}
