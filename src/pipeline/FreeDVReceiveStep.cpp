//=========================================================================
// Name:            FreeDVReceiveStep.cpp
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

#include <cassert>
#include "FreeDVReceiveStep.h"
#include "freedv_api.h"
#include "codec2_fifo.h"
#include "codec2_fdmdv.h"
#include "../defines.h"

extern void freq_shift_coh(COMP rx_fdm_fcorr[], COMP rx_fdm[], float foff, float Fs, COMP *foff_phase_rect, int nin) FREEDV_NONBLOCKING;

FreeDVReceiveStep::FreeDVReceiveStep(struct freedv* dv)
    : dv_(dv)
    , inputSampleFifo_(nullptr)
    , channelNoiseEnabled_(false)
    , channelNoiseSnr_(0)
    , freqOffsetHz_(0)
{
    assert(syncState_.is_lock_free());

    // Set FIFO to be 2x the number of samples per run so we don't lose anything.
    inputSampleFifo_ = codec2_fifo_create(freedv_get_modem_sample_rate(dv_));
    assert(inputSampleFifo_ != nullptr);
    
    rxFreqOffsetPhaseRectObjs_.real = cos(0.0);
    rxFreqOffsetPhaseRectObjs_.imag = sin(0.0);

    // Pre-allocate buffers so we don't have to do so during real-time operation.
    auto maxSamples = std::max(
        freedv_get_modem_sample_rate(dv_),
        freedv_get_speech_sample_rate(dv_));
    outputSamples_ = std::make_unique<short[]>(maxSamples);
    assert(outputSamples_ != nullptr);

    inputBuf_ = new short[freedv_get_n_max_modem_samples(dv_)];
    assert(inputBuf_ != nullptr);

    rxFdm_ = new COMP[freedv_get_n_max_modem_samples(dv_)];
    assert(rxFdm_ != nullptr);

    rxFdmOffset_ = new COMP[freedv_get_n_max_modem_samples(dv_)];
    assert(rxFdmOffset_ != nullptr);
}

FreeDVReceiveStep::~FreeDVReceiveStep()
{
    delete[] inputBuf_;
    delete[] rxFdm_;
    delete[] rxFdmOffset_;
    outputSamples_ = nullptr;

    if (inputSampleFifo_ != nullptr)
    {
        codec2_fifo_destroy(inputSampleFifo_);
    }
}

int FreeDVReceiveStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return freedv_get_modem_sample_rate(dv_);
}

int FreeDVReceiveStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return freedv_get_speech_sample_rate(dv_);
}

short* FreeDVReceiveStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    auto maxSpeechSamples = freedv_get_n_max_speech_samples(dv_);
    *numOutputSamples = 0;

    short* inputPtr = inputSamples;
    while (numInputSamples > 0 && inputPtr != nullptr)
    {
        codec2_fifo_write(inputSampleFifo_, inputPtr++, 1);
        numInputSamples--;
        
        int   nin = freedv_nin(dv_);
        int   nout = 0;
        while ((*numOutputSamples + maxSpeechSamples) < maxSamples && codec2_fifo_read(inputSampleFifo_, inputBuf_, nin) == 0) 
        {
            assert(nin <= freedv_get_n_max_modem_samples(dv_));

            // demod per frame processing
            for(int i=0; i<nin; i++) {
                rxFdm_[i].real = (float)inputBuf_[i];
                rxFdm_[i].imag = 0.0;
            }

            // Optional channel noise
            if (channelNoiseEnabled_.load(std::memory_order_acquire)) {
                fdmdv_simulate_channel(&sigPwrAvg_, rxFdm_, nin, channelNoiseSnr_.load(std::memory_order_acquire));
            }

            // Optional frequency shifting
            freq_shift_coh(rxFdmOffset_, rxFdm_, freqOffsetHz_.load(std::memory_order_acquire), freedv_get_modem_sample_rate(dv_), &rxFreqOffsetPhaseRectObjs_, nin);

            // Legacy modes, marked safe due to use of o1alloc().
            FREEDV_BEGIN_VERIFIED_SAFE
            nout = freedv_comprx(dv_, outputSamples_.get() + *numOutputSamples, rxFdmOffset_);
            FREEDV_END_VERIFIED_SAFE

            *numOutputSamples += nout;
            
            nin = freedv_nin(dv_);
        }
    }

    syncState_.store(freedv_get_sync(dv_), std::memory_order_release);
    return outputSamples_.get();
}

void FreeDVReceiveStep::reset() FREEDV_NONBLOCKING
{
    while (codec2_fifo_used(inputSampleFifo_) > 0)
    {
        short tmp;
        codec2_fifo_read(inputSampleFifo_, &tmp, 1);
    }
}
