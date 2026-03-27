//=========================================================================
// Name:            RADETransmitStep.h
// Purpose:         Describes a modulation step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__RADE_TRANSMIT_STEP_H
#define AUDIO_PIPELINE__RADE_TRANSMIT_STEP_H

#include <cstdio>
#include <thread>
#include <atomic>

#include "IPipelineStep.h"
#include "rade_api.h"
#include "../util/GenericFIFO.h"
#include "../util/Semaphore.h"

// TBD - need to wrap in "extern C" to avoid linker errors
extern "C"
{
    #include "fargan_config.h"
    #include "fargan.h"
    #include "lpcnet.h"
}

// Number of features to store. This is set to be close to the 
// typical size for RX/TX features for the rade_loss ctest to
// avoid contention with normal RADE operation.
#define NUM_FEATURES_TO_STORE (256 * 1024)

class RADETransmitStep : public IPipelineStep
{
public:
    RADETransmitStep(struct rade* dv, LPCNetEncState* encState);
    virtual ~RADETransmitStep();
    
    virtual int getInputSampleRate() const FREEDV_NONBLOCKING override;
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;
    virtual void reset() FREEDV_NONBLOCKING override;
    
    // For triggering EOO
    void restartVocoder() FREEDV_NONBLOCKING;
    
private:
    struct rade* dv_;
    LPCNetEncState* encState_;
    PreAllocatedFIFO<short, RADE_SPEECH_SAMPLE_RATE> inputSampleFifo_;
    PreAllocatedFIFO<short, RADE_MODEM_SAMPLE_RATE> outputSampleFifo_;
    float* featureList_;
    int featureListIdx_;
    int arch_;

    FILE* featuresFile_;

    std::unique_ptr<short[]> outputSamples_;
    RADE_COMP* radeOut_;
    short* radeOutShort_;
    RADE_COMP* eooOut_;
    short* eooOutShort_;
    
    PreAllocatedFIFO<float, NUM_FEATURES_TO_STORE>* utFeatures_;
    std::thread utFeatureThread_;
    std::atomic<bool> exitingFeatureThread_;
    Semaphore featuresAvailableSem_;

    void utFeatureThreadEntry_();
};

#endif // AUDIO_PIPELINE__RADE_TRANSMIT_STEP_H
