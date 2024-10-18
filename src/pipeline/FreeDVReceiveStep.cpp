//=========================================================================
// Name:            FreeDVReceiveStep.h
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
    inputSampleFifo_ = codec2_fifo_create(freedv_get_n_max_modem_samples(dv_) * 2);
    assert(inputSampleFifo_ != nullptr);
    
    rxFreqOffsetPhaseRectObjs_.real = cos(0.0);
    rxFreqOffsetPhaseRectObjs_.imag = sin(0.0);
}

FreeDVReceiveStep::~FreeDVReceiveStep()
{
    if (inputSampleFifo_ != nullptr)
    {
        codec2_fifo_free(inputSampleFifo_);
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
    *numOutputSamples = 0;
    short* outputSamples = nullptr;
    
    short input_buf[freedv_get_n_max_modem_samples(dv_)];
    short output_buf[freedv_get_n_speech_samples(dv_)];
    COMP  rx_fdm[freedv_get_n_max_modem_samples(dv_)];
    COMP  rx_fdm_offset[freedv_get_n_max_modem_samples(dv_)];
    
    short* inputPtr = inputSamples.get();
    while (numInputSamples > 0 && inputPtr != nullptr)
    {
        codec2_fifo_write(inputSampleFifo_, inputPtr++, 1);
        numInputSamples--;
        
        int   nin = freedv_nin(dv_);
        int   nout = 0;
        while (codec2_fifo_read(inputSampleFifo_, input_buf, nin) == 0) 
        {
            assert(nin <= freedv_get_n_max_modem_samples(dv_));

            // demod per frame processing
            for(int i=0; i<nin; i++) {
                rx_fdm[i].real = (float)input_buf[i];
                rx_fdm[i].imag = 0.0;
            }

            // Optional channel noise
            if (channelNoiseEnabled_) {
                fdmdv_simulate_channel(&sigPwrAvg_, rx_fdm, nin, channelNoiseSnr_);
            }

            // Optional frequency shifting
            freq_shift_coh(rx_fdm_offset, rx_fdm, freqOffsetHz_, freedv_get_modem_sample_rate(dv_), &rxFreqOffsetPhaseRectObjs_, nin);
            nout = freedv_comprx(dv_, output_buf, rx_fdm_offset);
    
            short* newOutputSamples = new short[*numOutputSamples + nout];
            assert(newOutputSamples != nullptr);
        
            if (outputSamples != nullptr)
            {
                memcpy(newOutputSamples, outputSamples, *numOutputSamples * sizeof(short));
                delete[] outputSamples;
            }
        
            memcpy(newOutputSamples + *numOutputSamples, output_buf, nout * sizeof(short));
            *numOutputSamples += nout;
            outputSamples = newOutputSamples;
            
            nin = freedv_nin(dv_);
        }
    }
    
    return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
}
