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
    inputSampleFifo_ = codec2_fifo_create(freedv_get_n_speech_samples(dv_) * 2);
    assert(inputSampleFifo_ != nullptr);
    
    txFreqOffsetPhaseRectObj_.real = cos(0.0);
    txFreqOffsetPhaseRectObj_.imag = sin(0.0);
}

FreeDVTransmitStep::~FreeDVTransmitStep()
{
    if (inputSampleFifo_ != nullptr)
    {
        codec2_fifo_free(inputSampleFifo_);
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
    short* outputSamples = nullptr;

    int mode = freedv_get_mode(dv_);
    int samplesUsedForFifo = freedv_get_n_speech_samples(dv_);
    int numTransmitRuns = (codec2_fifo_used(inputSampleFifo_) + numInputSamples) / samplesUsedForFifo;
    int nfreedv = freedv_get_n_nom_modem_samples(dv_);
    
    if (numTransmitRuns)
    {
        *numOutputSamples = freedv_get_n_nom_modem_samples(dv_) * numTransmitRuns;
        outputSamples = new short[*numOutputSamples];
        assert(outputSamples != nullptr);
        
        short codecInput[samplesUsedForFifo];
        short* tmpOutput = outputSamples;
        short* tmpInput = inputSamples.get();
        
        while (numInputSamples > 0)
        {
            codec2_fifo_write(inputSampleFifo_, tmpInput++, 1);
            numInputSamples--;
            
            if (codec2_fifo_used(inputSampleFifo_) >= samplesUsedForFifo)
            {
                codec2_fifo_read(inputSampleFifo_, codecInput, samplesUsedForFifo);
                
                if (mode == FREEDV_MODE_800XA || mode == FREEDV_MODE_2400B) 
                {
                    /* 800XA doesn't support complex output just yet */
                    freedv_tx(dv_, tmpOutput, codecInput);
                }
                else 
                {
                    COMP tx_fdm[nfreedv];
                    COMP tx_fdm_offset[nfreedv];
                    
                    freedv_comptx(dv_, tx_fdm, codecInput);
                    
                    freq_shift_coh(tx_fdm_offset, tx_fdm, getFreqOffsetFn_(), getOutputSampleRate(), &txFreqOffsetPhaseRectObj_, nfreedv);
                    for(int i = 0; i<nfreedv; i++)
                        tmpOutput[i] = tx_fdm_offset[i].real;
                }
                
                tmpOutput += samplesUsedForFifo;
            }
        }
    }
    else
    {
        codec2_fifo_write(inputSampleFifo_, inputSamples.get(), numInputSamples);
        *numOutputSamples = 0;
    }

    return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
}
