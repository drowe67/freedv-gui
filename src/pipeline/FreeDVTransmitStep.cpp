//=========================================================================
// Name:            FreeDVTransmitStep.cpp
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

#if defined(__has_feature) && __has_feature(realtime_sanitizer)
#include <sanitizer/rtsan_interface.h>
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)

#include <cstring>
#include <cassert>
#include <cmath>
#include "codec2_fifo.h"
#include "FreeDVTransmitStep.h"
#include "freedv_api.h"

extern void freq_shift_coh(COMP rx_fdm_fcorr[], COMP rx_fdm[], float foff, float Fs, COMP *foff_phase_rect, int nin);

FreeDVTransmitStep::FreeDVTransmitStep(struct freedv* dv, std::function<float()> getFreqOffsetFn)
    : dv_(dv)
    , getFreqOffsetFn_(getFreqOffsetFn)
    , inputSampleFifo_(nullptr)
{
    // Set FIFO to be 2x the number of samples per run so we don't lose anything.
    inputSampleFifo_ = codec2_fifo_create(freedv_get_speech_sample_rate(dv_));
    assert(inputSampleFifo_ != nullptr);
    
    txFreqOffsetPhaseRectObj_.real = cos(0.0);
    txFreqOffsetPhaseRectObj_.imag = sin(0.0);

    // Pre-allocate buffers so we don't have to do so during real-time operation.
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    outputSamples_ = std::shared_ptr<short>(
        new short[maxSamples], 
        std::default_delete<short[]>());
    assert(outputSamples_ != nullptr);

    codecInput_ = new short[maxSamples];
    assert(codecInput_ != nullptr);

    tmpOutput_ = new short[maxSamples];
    assert(tmpOutput_ != nullptr);

    int nfreedv = freedv_get_n_nom_modem_samples(dv_);

    txFdm_ = new COMP[nfreedv];
    assert(txFdm_ != nullptr);

    txFdmOffset_ = new COMP[nfreedv];
    assert(txFdmOffset_ != nullptr);
}

FreeDVTransmitStep::~FreeDVTransmitStep()
{
    delete[] codecInput_;
    delete[] txFdm_;
    delete[] txFdmOffset_;
    delete[] tmpOutput_;

    if (inputSampleFifo_ != nullptr)
    {
        codec2_fifo_destroy(inputSampleFifo_);
    }
}

int FreeDVTransmitStep::getInputSampleRate() const
{
    return freedv_get_speech_sample_rate(dv_);
}

int FreeDVTransmitStep::getOutputSampleRate() const
{
    return freedv_get_modem_sample_rate(dv_);
}

std::shared_ptr<short> FreeDVTransmitStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{   
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    int mode = freedv_get_mode(dv_);
    int samplesUsedForFifo = freedv_get_n_speech_samples(dv_);
    int nfreedv = freedv_get_n_nom_modem_samples(dv_);
    
    *numOutputSamples = 0;
    
    short* inputPtr = inputSamples.get();
    while (numInputSamples > 0 && inputPtr)
    {
        codec2_fifo_write(inputSampleFifo_, inputPtr++, 1);
        numInputSamples--;
        
        if ((*numOutputSamples + nfreedv) < maxSamples && codec2_fifo_used(inputSampleFifo_) >= samplesUsedForFifo)
        {
            codec2_fifo_read(inputSampleFifo_, codecInput_, samplesUsedForFifo);
        
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
            __rtsan_disable();
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
    
            if (mode == FREEDV_MODE_800XA) 
            {
                /* 800XA doesn't support complex output just yet */
                freedv_tx(dv_, tmpOutput_, codecInput_);
            }
            else 
            {                
                freedv_comptx(dv_, txFdm_, codecInput_);
                
                freq_shift_coh(txFdmOffset_, txFdm_, getFreqOffsetFn_(), getOutputSampleRate(), &txFreqOffsetPhaseRectObj_, nfreedv);
                for(int i = 0; i<nfreedv; i++)
                    tmpOutput_[i] = txFdmOffset_[i].real;
            }
                       
            #if defined(__has_feature) && __has_feature(realtime_sanitizer)
                __rtsan_disable();
            #endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
                   
            memcpy(outputSamples_.get() + *numOutputSamples, tmpOutput_, nfreedv * sizeof(short));
            *numOutputSamples += nfreedv;
        }
    }
    
    return outputSamples_;
}
