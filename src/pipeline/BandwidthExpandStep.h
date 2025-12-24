//=========================================================================
// Name:            BandwidthExpandStep.h
// Purpose:         Implements BBWENet from the Opus project (which expands 
//                  audio from 16kHz to 48kHz).
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

#ifndef AUDIO_PIPELINE__BANDWIDTH_EXPAND_STEP_H
#define AUDIO_PIPELINE__BANDWIDTH_EXPAND_STEP_H

#include "IPipelineStep.h"
#include "../util/GenericFIFO.h"

#include <memory>

// TBD - need to wrap in "extern C" to avoid linker errors
extern "C" 
{
#if defined(FREEDV_INTEGRATION)
    #include "fargan_config_integ.h"
#else
    #include "fargan_config.h"
#endif // defined(FREEDV_INTEGRATION)
    #include "../silk/structs.h"
    #include "osce_features.h"
    #include "osce_structs.h"
    #include "osce.h"
}

class BandwidthExpandStep : public IPipelineStep
{
public:
    BandwidthExpandStep();
    virtual ~BandwidthExpandStep();
    
    virtual int getInputSampleRate() const FREEDV_NONBLOCKING override;
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;
    virtual void reset() FREEDV_NONBLOCKING override;
    
private:
    silk_OSCE_BWE_struct *osceBWE_;
    OSCEModel *osce_;
    int arch_;
    
    GenericFIFO<short> inputSampleFifo_;
    std::unique_ptr<short[]> outputSamples_;
    std::unique_ptr<short[]> tmpInput_;

    void resetImpl_() FREEDV_NONBLOCKING;
};


#endif // AUDIO_PIPELINE__BANDWIDTH_EXPAND_STEP_H
