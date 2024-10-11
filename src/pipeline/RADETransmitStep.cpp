//=========================================================================
// Name:            RADETransmitStep.cpp
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
#include "RADETransmitStep.h"

RADETransmitStep::RADETransmitStep(struct rade* dv, LPCNetEncState* encState)
    : dv_(dv)
    , encState_(encState)
    , inputSampleFifo_(nullptr)
    , outputSampleFifo_(nullptr)
{
    inputSampleFifo_ = codec2_fifo_create(RADE_SPEECH_SAMPLE_RATE);
    assert(inputSampleFifo_ != nullptr);
    outputSampleFifo_ = codec2_fifo_create(RADE_MODEM_SAMPLE_RATE);
    assert(outputSampleFifo_ != nullptr);
}

RADETransmitStep::~RADETransmitStep()
{
    if (inputSampleFifo_ != nullptr)
    {
        codec2_fifo_free(inputSampleFifo_);
    }

    if (outputSampleFifo_ != nullptr)
    {
        codec2_fifo_free(inputSampleFifo_);
    }
}

int RADETransmitStep::getInputSampleRate() const
{
    return RADE_SPEECH_SAMPLE_RATE;
}

int RADETransmitStep::getOutputSampleRate() const
{
    return RADE_MODEM_SAMPLE_RATE;
}

std::shared_ptr<short> RADETransmitStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    short* outputSamples = nullptr;
    *numOutputSamples = 0;
    
    short* inputPtr = inputSamples.get();
    while (numInputSamples > 0)
    {
        codec2_fifo_write(inputSampleFifo_, inputPtr++, 1);
        numInputSamples--;
        
        if (codec2_fifo_used(inputSampleFifo_) >= LPCNET_FRAME_SIZE)
        {
            int numRequiredFeaturesForRADE = rade_n_features_in_out(dv_);
            int numOutputSamples = rade_n_tx_out(dv_);
            short pcm[LPCNET_FRAME_SIZE];
            float features[NB_TOTAL_FEATURES];
            RADE_COMP radeOut[numOutputSamples];
            short radeOutShort[numOutputSamples];

            int arch = opus_select_arch();

            // Feature extraction
            codec2_fifo_read(inputSampleFifo_, pcm, LPCNET_FRAME_SIZE);
            lpcnet_compute_single_frame_features(encState_, pcm, features, arch);
            for (int index = 0; index < NB_TOTAL_FEATURES; index++)
            {
                featureList_.push_back(features[index]);
            }

            // RADE TX handling
            while (featureList_.size() >= numRequiredFeaturesForRADE)
            {
                rade_tx(dv_, radeOut, &featureList_[0]);
                for (int index = 0; index < numRequiredFeaturesForRADE; index++)
                {
                    featureList_.erase(featureList_.begin());
                }
                for (int index = 0; index < numOutputSamples; index++)
                {
                    // We only need the real component for TX.
                    radeOutShort[index] = radeOut[index].real * 32767;
                }
                codec2_fifo_write(outputSampleFifo_, radeOutShort, numOutputSamples);
            }
        }
    }

    *numOutputSamples = codec2_fifo_used(outputSampleFifo_);
    if (*numOutputSamples > 0)
    {
        outputSamples = new short[*numOutputSamples];
        assert(outputSamples != nullptr);

        codec2_fifo_read(outputSampleFifo_, outputSamples, *numOutputSamples);
    }
    
    return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
}

void RADETransmitStep::restartVocoder()
{
    // Queues up EOO for return on the next call to this pipeline step.
    int numEOOSamples = rade_n_tx_eoo_out(dv_);
    RADE_COMP eooOut[numEOOSamples];
    short eooOutShort[numEOOSamples];

    rade_tx_eoo(dv_, eooOut);

    for (int index = 0; index < numEOOSamples; index++)
    {
        eooOutShort[index] = eooOut[index].real * 32767;
    }

    codec2_fifo_write(outputSampleFifo_, eooOutShort, numEOOSamples);
}
