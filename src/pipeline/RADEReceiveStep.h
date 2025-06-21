//=========================================================================
// Name:            RADEReceiveStep.h
// Purpose:         Describes a demodulation step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__RADE_RECEIVE_STEP_H
#define AUDIO_PIPELINE__RADE_RECEIVE_STEP_H

#include <atomic>
#include <cstdio>
#include "IPipelineStep.h"
#include "../freedv_interface.h"
#include "rade_api.h"
#include "codec2_fifo.h"
#include "rade_text.h"

// TBD - need to wrap in "extern C" to avoid linker errors
extern "C" 
{
    #include "fargan.h"
}

class RADEReceiveStep : public IPipelineStep
{
public:
    RADEReceiveStep(struct rade* dv, FARGANState* fargan, rade_text_t textPtr);
    virtual ~RADEReceiveStep();
    
    virtual int getInputSampleRate() const override;
    virtual int getOutputSampleRate() const override;
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples) override;
    virtual void reset() override;
    
    int getSync() const { return syncState_.load(); }
    
private:
    std::atomic<int> syncState_;
    struct rade* dv_;
    FARGANState* fargan_;
    struct FIFO* inputSampleFifo_;
    struct FIFO* outputSampleFifo_;
    float* pendingFeatures_;
    int pendingFeaturesIdx_;
    FILE* featuresFile_;
    rade_text_t textPtr_;

    RADE_COMP* inputBufCplx_;
    short* inputBuf_;
    float* featuresOut_;
    float* eooOut_;
    std::shared_ptr<short> outputSamples_;
};

#endif // AUDIO_PIPELINE__RADE_RECEIVE_STEP_H
