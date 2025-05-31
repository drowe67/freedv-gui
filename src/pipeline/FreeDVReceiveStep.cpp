//=========================================================================
// Name:            FreeDVReceiveStep.cpp
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

#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
#include <sanitizer/rtsan_interface.h>
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)

#include <cassert>
#include "FreeDVReceiveStep.h"
#include "freedv_api.h"
#include "codec2_fifo.h"
#include "codec2_fdmdv.h"
#include "../defines.h"

extern void freq_shift_coh(COMP rx_fdm_fcorr[], COMP rx_fdm[], float foff, float Fs, COMP *foff_phase_rect, int nin);

FreeDVReceiveStep::FreeDVReceiveStep(struct freedv* dv)
    : dv_(dv)
    , inputSampleFifo_(nullptr)
    , channelNoiseEnabled_(false)
    , channelNoiseSnr_(0)
    , freqOffsetHz_(0)
{
    // Set FIFO to be 2x the number of samples per run so we don't lose anything.
    inputSampleFifo_ = codec2_fifo_create(freedv_get_modem_sample_rate(dv_));
    assert(inputSampleFifo_ != nullptr);
    
    rxFreqOffsetPhaseRectObjs_.real = cos(0.0);
    rxFreqOffsetPhaseRectObjs_.imag = sin(0.0);

    // Pre-allocate buffers so we don't have to do so during real-time operation.
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    outputSamples_ = std::shared_ptr<short>(
        new short[maxSamples], 
        std::default_delete<short[]>());
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

int FreeDVReceiveStep::getInputSampleRate() const
{
    return freedv_get_modem_sample_rate(dv_);
}

int FreeDVReceiveStep::getOutputSampleRate() const
{
    return freedv_get_speech_sample_rate(dv_);
}

std::shared_ptr<short> FreeDVReceiveStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    auto maxSpeechSamples = freedv_get_n_max_speech_samples(dv_);
    *numOutputSamples = 0;

    short* inputPtr = inputSamples.get();
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
            if (channelNoiseEnabled_) {
                fdmdv_simulate_channel(&sigPwrAvg_, rxFdm_, nin, channelNoiseSnr_);
            }

#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
            __rtsan_disable();
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)

            // Optional frequency shifting
            freq_shift_coh(rxFdmOffset_, rxFdm_, freqOffsetHz_, freedv_get_modem_sample_rate(dv_), &rxFreqOffsetPhaseRectObjs_, nin);
            nout = freedv_comprx(dv_, outputSamples_.get() + *numOutputSamples, rxFdmOffset_);
            *numOutputSamples += nout;
            
            nin = freedv_nin(dv_);

#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
            __rtsan_enable();
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)

        }
    }

    return outputSamples_;
}

void FreeDVReceiveStep::reset()
{
    while (codec2_fifo_used(inputSampleFifo_) > 0)
    {
        short tmp;
        codec2_fifo_read(inputSampleFifo_, &tmp, 1);
    }
}