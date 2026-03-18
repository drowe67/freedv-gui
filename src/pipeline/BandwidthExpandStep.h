//=========================================================================
// Name:            BandwidthExpandStep.h
// Purpose:         Implements BBWENet from the Opus project (which expands 
//                  audio from 16kHz to 48kHz).
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

#ifndef AUDIO_PIPELINE__BANDWIDTH_EXPAND_STEP_H
#define AUDIO_PIPELINE__BANDWIDTH_EXPAND_STEP_H

#include "IPipelineStep.h"
#include "../util/GenericFIFO.h"

#include <memory>

// TBD - need to wrap in "extern C" to avoid linker errors
extern "C" 
{
    #include "fargan_config.h"
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
