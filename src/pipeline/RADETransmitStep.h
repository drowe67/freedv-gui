//=========================================================================
// Name:            RADETransmitStep.h
// Purpose:         Describes a modulation step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__RADE_TRANSMIT_STEP_H
#define AUDIO_PIPELINE__RADE_TRANSMIT_STEP_H

#include <cstdio>
#include <thread>

#include "IPipelineStep.h"
#include "../freedv_interface.h"
#include "rade_api.h"
#include "lpcnet.h"
#include "../util/GenericFIFO.h"
#include "../util/Semaphore.h"

// Number of features to store. This is set to be close to the 
// typical size for RX/TX features for the rade_loss ctest to
// avoid contention with normal RADE operation.
#define NUM_FEATURES_TO_STORE (256 * 1024)

class RADETransmitStep : public IPipelineStep
{
public:
    RADETransmitStep(struct rade* dv, LPCNetEncState* encState);
    virtual ~RADETransmitStep();
    
    virtual int getInputSampleRate() const override;
    virtual int getOutputSampleRate() const override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) override;
    virtual void reset() override;
    
    // For triggering EOO
    void restartVocoder();
    
private:
    struct rade* dv_;
    LPCNetEncState* encState_;
    PreAllocatedFIFO<short, RADE_SPEECH_SAMPLE_RATE / 2> inputSampleFifo_;
    PreAllocatedFIFO<short, RADE_MODEM_SAMPLE_RATE / 2> outputSampleFifo_;
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
    bool exitingFeatureThread_;
    Semaphore featuresAvailableSem_;

    void utFeatureThreadEntry_();
};

#endif // AUDIO_PIPELINE__RADE_TRANSMIT_STEP_H
