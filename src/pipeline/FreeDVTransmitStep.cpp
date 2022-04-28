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
#include "codec2_fifo.h"
#include "FreeDVTransmitStep.h"
#include "freedv_api.h"

FreeDVTransmitStep::FreeDVTransmitStep(FreeDVInterface& iface, std::function<float()> getFreqOffsetFn)
    : interface_(iface)
    , getFreqOffsetFn_(getFreqOffsetFn)
    , inputSampleFifo_(nullptr)
    , samplesUsedForFifo_(0)
    , sampleRateUsedForFifo_(0)
{
    // empty
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
    return interface_.getTxSpeechSampleRate();
}

int FreeDVTransmitStep::getOutputSampleRate() const
{
    return interface_.getTxModemSampleRate();
}

std::shared_ptr<short> FreeDVTransmitStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    short* outputSamples = nullptr;

    // Recreate FIFO if we switch modes or sample rates.
    if (samplesUsedForFifo_ != interface_.getTxNumSpeechSamples() ||
        sampleRateUsedForFifo_ != getInputSampleRate())
    {
        if (inputSampleFifo_ != nullptr)
        {
            codec2_fifo_free(inputSampleFifo_);
        }
        
        samplesUsedForFifo_ = interface_.getTxNumSpeechSamples();
        sampleRateUsedForFifo_ = getInputSampleRate();
        
        // Set FIFO to be 2x the number of samples per run so we don't lose anything.
        inputSampleFifo_ = codec2_fifo_create(samplesUsedForFifo_ * 2);
        assert(inputSampleFifo_ != nullptr);
    }
    
    int numTransmitRuns = (codec2_fifo_used(inputSampleFifo_) + numInputSamples) / samplesUsedForFifo_;
    if (numTransmitRuns)
    {
        auto mode = interface_.getTxMode();
    
        *numOutputSamples = interface_.getTxNNomModemSamples() * numTransmitRuns;
        outputSamples = new short[*numOutputSamples];
        assert(outputSamples != nullptr);
        
        short codecInput[samplesUsedForFifo_];
        short* tmpOutput = outputSamples;
        short* tmpInput = inputSamples.get();
        
        while (numInputSamples > 0)
        {
            codec2_fifo_write(inputSampleFifo_, tmpInput++, 1);
            numInputSamples--;
            
            if (codec2_fifo_used(inputSampleFifo_) >= samplesUsedForFifo_)
            {
                codec2_fifo_read(inputSampleFifo_, codecInput, samplesUsedForFifo_);
                
                if (mode == FREEDV_MODE_800XA || mode == FREEDV_MODE_2400B) 
                {
                    /* 800XA doesn't support complex output just yet */
                    interface_.transmit(tmpOutput, codecInput);
                }
                else 
                {
                    interface_.complexTransmit(tmpOutput, codecInput, getFreqOffsetFn_(), interface_.getTxNNomModemSamples());
                }
                
                tmpOutput += samplesUsedForFifo_;
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
