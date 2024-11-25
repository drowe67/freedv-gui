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

#include <cstdio>
#include <vector>
#include "IPipelineStep.h"
#include "../freedv_interface.h"
#include "rade_api.h"
#include "codec2_fifo.h"

// TBD - need to wrap in "extern C" to avoid linker errors
extern "C" 
{
    #include "fargan.h"
}

class RADEReceiveStep : public IPipelineStep
{
public:
    RADEReceiveStep(struct rade* dv, FARGANState* fargan);
    virtual ~RADEReceiveStep();
    
    virtual int getInputSampleRate() const;
    virtual int getOutputSampleRate() const;
    virtual std::shared_ptr<short> execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples);
    
    int getSync() const { return rade_sync(dv_); }
    
private:
    struct rade* dv_;
    FARGANState* fargan_;
    struct FIFO* inputSampleFifo_;
    struct FIFO* outputSampleFifo_;
    std::vector<float> pendingFeatures_;

    FILE* featuresFile_;
};

#endif // AUDIO_PIPELINE__RADE_RECEIVE_STEP_H
