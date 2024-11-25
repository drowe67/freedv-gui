//=========================================================================
// Name:            RADEReceiveStep.cpp
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
#include "RADEReceiveStep.h"
#include "../defines.h"
#include "lpcnet.h" // from Opus source tree

extern wxString utRxFeatureFile;

RADEReceiveStep::RADEReceiveStep(struct rade* dv, FARGANState* fargan)
    : dv_(dv)
    , fargan_(fargan)
    , inputSampleFifo_(nullptr)
    , outputSampleFifo_(nullptr)
    , featuresFile_(nullptr)
{
    // Set FIFO to be 2x the number of samples per run so we don't lose anything.
    inputSampleFifo_ = codec2_fifo_create(rade_nin_max(dv_) * 2);
    assert(inputSampleFifo_ != nullptr);

    // Enough for one second of audio. Probably way overkill.
    outputSampleFifo_ = codec2_fifo_create(16000);
    assert(outputSampleFifo_ != nullptr);

    if (utRxFeatureFile != "")
    {
        featuresFile_ = fopen((const char*)utRxFeatureFile.ToUTF8(), "wb");
        assert(featuresFile_ != nullptr);
    }
}

RADEReceiveStep::~RADEReceiveStep()
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
        codec2_fifo_free(outputSampleFifo_);
    }
}

int RADEReceiveStep::getInputSampleRate() const
{
    return RADE_MODEM_SAMPLE_RATE;
}

int RADEReceiveStep::getOutputSampleRate() const
{
    return RADE_SPEECH_SAMPLE_RATE;
}

std::shared_ptr<short> RADEReceiveStep::execute(std::shared_ptr<short> inputSamples, int numInputSamples, int* numOutputSamples)
{
    *numOutputSamples = 0;
    short* outputSamples = nullptr;
    
    RADE_COMP input_buf_cplx[rade_nin_max(dv_)];
    short input_buf[rade_nin_max(dv_)];
    float features_out[rade_n_features_in_out(dv_)];
    
    short* inputPtr = inputSamples.get();
    while (numInputSamples > 0 && inputPtr != nullptr)
    {
        codec2_fifo_write(inputSampleFifo_, inputPtr++, 1);
        numInputSamples--;
        
        int   nin = rade_nin(dv_);
        int   nout = 0;
        while (codec2_fifo_read(inputSampleFifo_, input_buf, nin) == 0) 
        {
            assert(nin <= rade_nin_max(dv_));

            // demod per frame processing
            for(int i=0; i<nin; i++) 
            {
                input_buf_cplx[i].real = (float)input_buf[i] / 32767.0;
                input_buf_cplx[i].imag = 0.0;
            }

            // RADE processing (input signal->features).
            nout = rade_rx(dv_, features_out, input_buf_cplx);
            if (featuresFile_)
            {
                fwrite(features_out, sizeof(float), nout, featuresFile_);
            }

            for (int i = 0; i < nout; i++)
            {
                pendingFeatures_.push_back(features_out[i]);
            }

            // FARGAN processing (features->analog audio)
            while (pendingFeatures_.size() >= NB_TOTAL_FEATURES)
            {
                // XXX - lpcnet_demo reads NB_TOTAL_FEATURES from RADE
                // but only processes NB_FEATURES of those for some reason.
                float featuresIn[NB_FEATURES];
                for (int i = 0; i < NB_FEATURES; i++)
                {
                    featuresIn[i] = pendingFeatures_[0];
                    pendingFeatures_.erase(pendingFeatures_.begin());
                }
                for (int i = 0; i < (NB_TOTAL_FEATURES - NB_FEATURES); i++)
                {
                    pendingFeatures_.erase(pendingFeatures_.begin());
                }

                float fpcm[LPCNET_FRAME_SIZE];
                short pcm[LPCNET_FRAME_SIZE];
                fargan_synthesize(fargan_, fpcm, featuresIn);
                for (int i = 0; i < LPCNET_FRAME_SIZE; i++) 
                {
                    pcm[i] = (int)floor(.5 + MIN32(32767, MAX32(-32767, 32768.f*fpcm[i])));
                }

                *numOutputSamples += LPCNET_FRAME_SIZE;
                codec2_fifo_write(outputSampleFifo_, pcm, LPCNET_FRAME_SIZE);
            }
            
            nin = rade_nin(dv_);
        }
    }
   
    if (*numOutputSamples > 0)
    { 
        outputSamples = new short[*numOutputSamples];
        assert(outputSamples != nullptr);
        codec2_fifo_read(outputSampleFifo_, outputSamples, *numOutputSamples);
    }

    return std::shared_ptr<short>(outputSamples, std::default_delete<short[]>());
}
