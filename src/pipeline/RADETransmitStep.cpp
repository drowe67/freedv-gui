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
#include "../defines.h"
#include "codec2_fifo.h"
#include "RADETransmitStep.h"

const int RADE_SCALING_FACTOR = 16383;

extern wxString utTxFeatureFile;

RADETransmitStep::RADETransmitStep(struct rade* dv, LPCNetEncState* encState)
    : dv_(dv)
    , encState_(encState)
    , inputSampleFifo_(nullptr)
    , outputSampleFifo_(nullptr)
    , featuresFile_(nullptr)
{
    inputSampleFifo_ = codec2_fifo_create(RADE_SPEECH_SAMPLE_RATE);
    assert(inputSampleFifo_ != nullptr);
    outputSampleFifo_ = codec2_fifo_create(RADE_MODEM_SAMPLE_RATE);
    assert(outputSampleFifo_ != nullptr);
    
    if (utTxFeatureFile != "")
    {
        featuresFile_ = fopen((const char*)utTxFeatureFile.ToUTF8(), "wb");
        assert(featuresFile_ != nullptr);
    }
}

RADETransmitStep::~RADETransmitStep()
{
    if (featuresFile_ != nullptr)
    {
        fclose(featuresFile_);
    }
    
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

    if (numInputSamples == 0)
    {
        // Special case logic for EOO -- make sure we only send 20ms worth at a time
        *numOutputSamples = std::min(codec2_fifo_used(outputSampleFifo_), (int)(RADE_MODEM_SAMPLE_RATE * .02));
        if (*numOutputSamples > 0)
        {
            outputSamples = new short[*numOutputSamples];
            assert(outputSamples != nullptr);

            codec2_fifo_read(outputSampleFifo_, outputSamples, *numOutputSamples);

            log_info("Returning %d EOO samples (remaining in FIFO: %d)", *numOutputSamples, codec2_fifo_used(outputSampleFifo_));
        }

        return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());;
    }
    
    short* inputPtr = inputSamples.get();
    while (numInputSamples > 0 && inputPtr != nullptr)
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
            
            if (featuresFile_)
            {
                fwrite(features, sizeof(float), NB_TOTAL_FEATURES, featuresFile_);
            }
            
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
                    radeOutShort[index] = radeOut[index].real * RADE_SCALING_FACTOR;
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
    const int NUM_SAMPLES_SILENCE = 60 * getOutputSampleRate() / 1000;
    int numEOOSamples = rade_n_tx_eoo_out(dv_);
    RADE_COMP eooOut[numEOOSamples];
    short eooOutShort[numEOOSamples];

    rade_tx_eoo(dv_, eooOut);

    memset(eooOutShort, 0, sizeof(eooOutShort));
    for (int index = 0; index < numEOOSamples; index++)
    {
        eooOutShort[index] = eooOut[index].real * RADE_SCALING_FACTOR;
    }

    log_info("Queueing %d EOO samples to output FIFO", numEOOSamples + NUM_SAMPLES_SILENCE);
    if (codec2_fifo_write(outputSampleFifo_, eooOutShort, numEOOSamples + NUM_SAMPLES_SILENCE) != 0)
    {
        log_warn("Could not queue EOO samples (remaining space in FIFO = %d)", codec2_fifo_free(outputSampleFifo_));
    }
}
