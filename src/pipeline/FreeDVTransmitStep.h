//=========================================================================
// Name:            FreeDVTransmitStep.h
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

#ifndef AUDIO_PIPELINE__FREEDV_TRANSMIT_STEP_H
#define AUDIO_PIPELINE__FREEDV_TRANSMIT_STEP_H

#include <functional>

#include "IPipelineStep.h"
#include "../freedv_interface.h"

// Forward definition of structs from Codec2.
extern "C"
{
    struct FIFO;
    struct freedv;
}

class FreeDVTransmitStep : public IPipelineStep
{
public:
    FreeDVTransmitStep(struct freedv* dv, std::function<float()> getFreqOffsetFn);
    virtual ~FreeDVTransmitStep();
    
    virtual int getInputSampleRate() const override;
    virtual int getOutputSampleRate() const override;
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples) override;
    virtual void reset() override;
    
private:
    struct freedv* dv_;
    std::function<float()> getFreqOffsetFn_;
    struct FIFO* inputSampleFifo_;
    COMP txFreqOffsetPhaseRectObj_;

    COMP* txFdm_;
    COMP* txFdmOffset_;
    short* codecInput_;
    short* tmpOutput_;
    std::shared_ptr<short> outputSamples_;
};

#endif // AUDIO_PIPELINE__FREEDV_TRANSMIT_STEP_H
