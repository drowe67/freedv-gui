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
#include "IPipelineStep.h"
#include "../freedv_interface.h"
#include "rade_api.h"
#include "lpcnet.h"

class RADETransmitStep : public IPipelineStep
{
public:
    RADETransmitStep(struct rade* dv, LPCNetEncState* encState);
    virtual ~RADETransmitStep();
    
    virtual int getInputSampleRate() const override;
    virtual int getOutputSampleRate() const override;
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples) override;
    virtual void reset() override;
    
    // For triggering EOO
    void restartVocoder();
    
private:
    struct rade* dv_;
    LPCNetEncState* encState_;
    struct FIFO* inputSampleFifo_;
    struct FIFO* outputSampleFifo_;
    float* featureList_;
    int featureListIdx_;
    int arch_;

    FILE* featuresFile_;

    std::shared_ptr<short> outputSamples_;
    RADE_COMP* radeOut_;
    short* radeOutShort_;
    RADE_COMP* eooOut_;
    short* eooOutShort_;
};

#endif // AUDIO_PIPELINE__RADE_TRANSMIT_STEP_H
