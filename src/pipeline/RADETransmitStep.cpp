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
#include <functional>
#include "../defines.h"
#include "codec2_fifo.h"
#include "RADETransmitStep.h"

#if defined(__APPLE__)
#include <pthread.h>
#include <sys/resource.h>
#endif // defined(__APPLE__)

using namespace std::chrono_literals;

#define FEATURE_FIFO_SIZE ((RADE_SPEECH_SAMPLE_RATE / LPCNET_FRAME_SIZE) * rade_n_features_in_out(dv_))

const int RADE_SCALING_FACTOR = 16383;

extern wxString utTxFeatureFile;

RADETransmitStep::RADETransmitStep(struct rade* dv, LPCNetEncState* encState)
    : dv_(dv)
    , encState_(encState)
    , featureList_(nullptr)
    , featureListIdx_(0)
    , featuresFile_(nullptr)
    , exitingFeatureThread_(false)
{
    if (utTxFeatureFile != "")
    {
        utFeatures_ = new PreAllocatedFIFO<float, NUM_FEATURES_TO_STORE>;
        assert(utFeatures_ != nullptr);

        featuresFile_ = fopen((const char*)utTxFeatureFile.ToUTF8(), "wb");
        assert(featuresFile_ != nullptr);
        
        utFeatureThread_ = std::thread(std::bind(&RADETransmitStep::utFeatureThreadEntry_, this));
    }

    // Pre-allocate buffers so we don't have to do so during real-time operation.
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    outputSamples_ = std::make_unique<short[]>(maxSamples);
    assert(outputSamples_ != nullptr);

    int numOutputSamples = rade_n_tx_out(dv_);

    radeOut_ = new RADE_COMP[numOutputSamples];
    assert(radeOut_ != nullptr);

    radeOutShort_ = new short[numOutputSamples];
    assert(radeOutShort_ != nullptr);

    const int NUM_SAMPLES_SILENCE = 60 * getOutputSampleRate() / 1000;
    int numEOOSamples = rade_n_tx_eoo_out(dv_);

    eooOut_ = new RADE_COMP[numEOOSamples];
    assert(eooOut_ != nullptr);

    eooOutShort_ = new short[numEOOSamples + NUM_SAMPLES_SILENCE];
    assert(eooOutShort_ != nullptr);

    featureList_ = new float[rade_n_features_in_out(dv_)];
    assert(featureList_ != nullptr);

    arch_ = opus_select_arch();
}

RADETransmitStep::~RADETransmitStep()
{
    delete[] radeOut_;
    delete[] radeOutShort_;
    delete[] eooOut_;
    delete[] eooOutShort_;
    delete[] featureList_;
    outputSamples_ = nullptr;

    if (featuresFile_ != nullptr)
    {
        exitingFeatureThread_ = true;
        featuresAvailableSem_.signal();
        utFeatureThread_.join();
        fclose(featuresFile_);

        delete utFeatures_;
    }
}

int RADETransmitStep::getInputSampleRate() const FREEDV_NONBLOCKING
{
    return RADE_SPEECH_SAMPLE_RATE;
}

int RADETransmitStep::getOutputSampleRate() const FREEDV_NONBLOCKING
{
    return RADE_MODEM_SAMPLE_RATE;
}

short* RADETransmitStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples) FREEDV_NONBLOCKING
{
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    int numSamplesPerTx = rade_n_tx_out(dv_);
    
    *numOutputSamples = 0;

    if (numInputSamples == 0)
    {
        // Special case logic for EOO
        *numOutputSamples = outputSampleFifo_.numUsed();
        if (*numOutputSamples > 0)
        {
            outputSampleFifo_.read(outputSamples_.get(), *numOutputSamples);
        }

        return outputSamples_.get();
    }
    
    inputSampleFifo_.write(inputSamples, numInputSamples);
    while ((*numOutputSamples + numSamplesPerTx) < maxSamples && inputSampleFifo_.numUsed() >= LPCNET_FRAME_SIZE)
    {
        int numRequiredFeaturesForRADE = rade_n_features_in_out(dv_);
        short pcm[LPCNET_FRAME_SIZE];
        float features[NB_TOTAL_FEATURES];

        // Feature extraction
        inputSampleFifo_.read(pcm, LPCNET_FRAME_SIZE);
        lpcnet_compute_single_frame_features(encState_, pcm, features, arch_);
            
        if (featuresFile_)
        {
            utFeatures_->write(features, NB_TOTAL_FEATURES);
            if (utFeatures_->numUsed() > (0.75 * utFeatures_->capacity()))
            {
                featuresAvailableSem_.signal();
            }
        }
            
        for (int index = 0; index < NB_TOTAL_FEATURES; index++)
        {
            featureList_[featureListIdx_++] = features[index];
            if (featureListIdx_ == numRequiredFeaturesForRADE)
            {
                featureListIdx_ = 0;

                // RADE TX handling
		FREEDV_BEGIN_REALTIME_UNSAFE
                    rade_tx(dv_, radeOut_, &featureList_[0]);
                FREEDV_END_REALTIME_UNSAFE

                for (int index = 0; index < numSamplesPerTx; index++)
                {
                    // We only need the real component for TX.
                    radeOutShort_[index] = radeOut_[index].real * RADE_SCALING_FACTOR;
                }
                outputSampleFifo_.write(radeOutShort_, numSamplesPerTx);
            }
        }

        *numOutputSamples = outputSampleFifo_.numUsed();
    }

    if (*numOutputSamples > 0)
    {
        outputSampleFifo_.read(outputSamples_.get(), *numOutputSamples);
    }
    
    return outputSamples_.get();
}

void RADETransmitStep::restartVocoder()
{
    // Queues up EOO for return on the next call to this pipeline step.
    const int NUM_SAMPLES_SILENCE = 60 * getOutputSampleRate() / 1000;
    int numEOOSamples = rade_n_tx_eoo_out(dv_);

    FREEDV_BEGIN_REALTIME_UNSAFE
        rade_tx_eoo(dv_, eooOut_);
    FREEDV_END_REALTIME_UNSAFE

    memset(eooOutShort_, 0, sizeof(short) * (numEOOSamples + NUM_SAMPLES_SILENCE));
    for (int index = 0; index < numEOOSamples; index++)
    {
        eooOutShort_[index] = eooOut_[index].real * RADE_SCALING_FACTOR;
    }

    if (outputSampleFifo_.write(eooOutShort_, numEOOSamples + NUM_SAMPLES_SILENCE) != 0)
    {
        log_warn("Could not queue EOO samples (remaining space in FIFO = %d)", outputSampleFifo_.numFree());
    }
}

void RADETransmitStep::reset() FREEDV_NONBLOCKING
{
    inputSampleFifo_.reset();
    outputSampleFifo_.reset();
    featureListIdx_ = 0;
}

void RADETransmitStep::utFeatureThreadEntry_()
{
#if defined(__APPLE__)
    // Downgrade thread QoS to Utility to avoid thread contention issues.
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
    
    // Make sure other I/O can throttle us.
    setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_THREAD, IOPOL_THROTTLE);
#endif // defined(__APPLE__)

    float* featureBuf = new float[utFeatures_->capacity()];
    assert(featureBuf != nullptr);

    while (!exitingFeatureThread_)
    {
        auto numToRead = std::min(utFeatures_->numUsed(), utFeatures_->capacity());
        while (numToRead > 0)
        {
            utFeatures_->read(featureBuf, numToRead);
            fwrite(featureBuf, sizeof(float) * numToRead, 1, featuresFile_);
            numToRead = std::min(utFeatures_->numUsed(), utFeatures_->capacity());
        }
        
        featuresAvailableSem_.wait();
    }

    auto numToRead = std::min(utFeatures_->numUsed(), utFeatures_->capacity());
    while (numToRead > 0)
    {
        utFeatures_->read(featureBuf, numToRead);
        fwrite(featureBuf, sizeof(float) * numToRead, 1, featuresFile_);
        numToRead = std::min(utFeatures_->numUsed(), utFeatures_->capacity());
    }

    delete[] featureBuf;
}
