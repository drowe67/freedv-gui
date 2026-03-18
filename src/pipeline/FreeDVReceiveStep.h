//=========================================================================
// Name:            FreeDVReceiveStep.h
// Purpose:         Describes a demodulation step in the audio pipeline.
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

#ifndef AUDIO_PIPELINE__FREEDV_RECEIVE_STEP_H
#define AUDIO_PIPELINE__FREEDV_RECEIVE_STEP_H

#include <atomic>
#include <functional>
#include "IPipelineStep.h"
#include "../freedv_interface.h"
#include "freedv_api.h"

// Forward declarations of structs implemented by Codec2 
extern "C"
{
    struct FIFO;
    struct freedv;
}

class FreeDVReceiveStep : public IPipelineStep
{
public:
    FreeDVReceiveStep(struct freedv* dv);
    virtual ~FreeDVReceiveStep();
    
    virtual int getInputSampleRate() const FREEDV_NONBLOCKING override;
    virtual int getOutputSampleRate() const FREEDV_NONBLOCKING override;
    virtual short* execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING override;
    virtual void reset() FREEDV_NONBLOCKING override;
    
    void setSigPwrAvg(float newVal) { sigPwrAvg_ = newVal; }
    float getSigPwrAvg() const { return sigPwrAvg_; }
    int getSync() const { return syncState_.load(std::memory_order_acquire); }
    void setChannelNoiseEnable(bool enabled, int snr) 
    { 
        channelNoiseEnabled_.store(enabled, std::memory_order_release); 
        channelNoiseSnr_.store(snr, std::memory_order_release);
    }
    void setFreqOffset(float freq) { freqOffsetHz_.store(freq, std::memory_order_release); }

private:
    std::atomic<int> syncState_;
    struct freedv* dv_;
    struct FIFO* inputSampleFifo_;
    COMP rxFreqOffsetPhaseRectObjs_;
    float sigPwrAvg_;
    std::atomic<bool> channelNoiseEnabled_;
    std::atomic<int> channelNoiseSnr_;
    std::atomic<float> freqOffsetHz_;

    std::unique_ptr<short[]> outputSamples_;
    short* inputBuf_;
    COMP* rxFdm_;
    COMP* rxFdmOffset_;
};

#endif // AUDIO_PIPELINE__FREEDV_RECEIVE_STEP_H
