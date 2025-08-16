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
    outputSampleFifo_ = codec2_fifo_create(freedv_get_modem_sample_rate(dv_));
    assert(outputSampleFifo_ != nullptr);
    
    txFreqOffsetPhaseRectObj_.real = cos(0.0);
    txFreqOffsetPhaseRectObj_.imag = sin(0.0);

    // Pre-allocate buffers so we don't have to do so during real-time operation.
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    outputSamples_ = std::make_unique<short[]>(maxSamples);
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

    if (outputSampleFifo_ != nullptr)
    {
        codec2_fifo_destroy(outputSampleFifo_);
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

short* FreeDVTransmitStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples)
{   
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    int mode = freedv_get_mode(dv_);
    int samplesUsedForFifo = freedv_get_n_speech_samples(dv_);
    int nfreedv = freedv_get_n_nom_modem_samples(dv_);
    
    *numOutputSamples = 0;
    
    short* inputPtr = inputSamples;
    int ctr = numInputSamples;
    while (ctr > 0 && inputPtr)
    {
        codec2_fifo_write(inputSampleFifo_, inputPtr++, 1);
        ctr--;
        
        if (/*(*numOutputSamples + nfreedv) < maxSamples &&*/ codec2_fifo_used(inputSampleFifo_) >= samplesUsedForFifo)
        {
            codec2_fifo_read(inputSampleFifo_, codecInput_, samplesUsedForFifo);
        
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
             
            codec2_fifo_write(outputSampleFifo_, tmpOutput_, nfreedv);          
            //memcpy(outputSamples_.get() + *numOutputSamples, tmpOutput_, nfreedv * sizeof(short));
            //*numOutputSamples += nfreedv;
        }
    }
   
    *numOutputSamples = std::min(codec2_fifo_used(outputSampleFifo_), (numInputSamples * freedv_get_modem_sample_rate(dv_)) / freedv_get_speech_sample_rate(dv_));
    codec2_fifo_read(outputSampleFifo_, outputSamples_.get(), *numOutputSamples); 
    return outputSamples_.get();
}

void FreeDVTransmitStep::reset()
{
    short tmp;
    while (codec2_fifo_used(inputSampleFifo_) > 0)
    {
        codec2_fifo_read(inputSampleFifo_, &tmp, 1);
    }
    while (codec2_fifo_used(outputSampleFifo_) > 0)
    {
        codec2_fifo_read(outputSampleFifo_, &tmp, 1);
    }
}
