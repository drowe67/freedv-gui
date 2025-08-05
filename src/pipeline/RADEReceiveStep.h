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
#include <thread>

#include "IPipelineStep.h"
#include "../freedv_interface.h"
#include "rade_api.h"
#include "rade_text.h"
#include "../util/GenericFIFO.h"

// TBD - need to wrap in "extern C" to avoid linker errors
extern "C" 
{
    #include "fargan.h"
}

class RADEReceiveStep : public IPipelineStep
{
public:
    RADEReceiveStep(struct rade* dv, FARGANState* fargan, rade_text_t textPtr, std::function<void(RADEReceiveStep*)> syncFn);
    virtual ~RADEReceiveStep();
    
    virtual int getInputSampleRate() const override;
    virtual int getOutputSampleRate() const override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) override;
    virtual void reset() override;
    
    int getSync() const { return syncState_.load(std::memory_order_acquire); }
    int getSnr() const { return snr_.load(std::memory_order_acquire); }
    
private:
    std::atomic<int> syncState_;
    std::atomic<int> snr_;
    struct rade* dv_;
    FARGANState* fargan_;
    PreAllocatedFIFO<short, RADE_MODEM_SAMPLE_RATE> inputSampleFifo_;
    PreAllocatedFIFO<short, RADE_SPEECH_SAMPLE_RATE> outputSampleFifo_;
    float* pendingFeatures_;
    int pendingFeaturesIdx_;
    FILE* featuresFile_;
    rade_text_t textPtr_;
    std::function<void(RADEReceiveStep*)> syncFn_;

    RADE_COMP* inputBufCplx_;
    short* inputBuf_;
    float* featuresOut_;
    float* eooOut_;
    std::unique_ptr<short[]> outputSamples_;
    
    PreAllocatedFIFO<float, 8192> utFeatures_;
    std::thread utFeatureThread_;
    bool exitingFeatureThread_;
};

#endif // AUDIO_PIPELINE__RADE_RECEIVE_STEP_H
