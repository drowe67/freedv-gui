//=========================================================================
// Name:            RecordStep.h
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

#ifndef AUDIO_PIPELINE__RECORD_STEP_H
#define AUDIO_PIPELINE__RECORD_STEP_H

#include "IPipelineStep.h"

#include <functional>
#include <sndfile.h>
#include <thread>
#include <atomic>
#include "codec2_fifo.h"
#include "../util/Semaphore.h"
#include "../util/GenericFIFO.h"

class RecordStep : public IPipelineStep
{
public:
    RecordStep(
        int inputSampleRate, std::function<SNDFILE*()> getSndFileFn, std::function<void(int)> isFileCompleteFn);
    virtual ~RecordStep();
    
    virtual int getInputSampleRate() const FREEDV_NONBLOCKING override;
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;
    virtual void reset() FREEDV_NONBLOCKING override;
    
private:
    int inputSampleRate_;
    std::function<int()> fileSampleRateFn_;
    std::function<SNDFILE*()> getSndFileFn_;
    std::function<void(int)> isFileCompleteFn_;
    std::thread fileIoThread_;
    GenericFIFO<short> inputFifo_;
    std::atomic<bool> fileIoThreadEnding_;
    Semaphore fileIoThreadSem_;
    
    void fileIoThreadEntry_();
};

#endif // AUDIO_PIPELINE__RECORD_STEP_H
