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

#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
#include <sanitizer/rtsan_interface.h>
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)

#include <cassert>
#include <functional>
#include "RADEReceiveStep.h"
#include "../defines.h"
#include "lpcnet.h" // from Opus source tree

#if defined(__APPLE__)
#include <pthread.h>
#include <sys/resource.h>
#endif // defined(__APPLE__)

using namespace std::chrono_literals;

extern wxString utRxFeatureFile;

#define FEATURE_FIFO_SIZE ((RADE_SPEECH_SAMPLE_RATE / LPCNET_FRAME_SIZE) * rade_n_features_in_out(dv_))

RADEReceiveStep::RADEReceiveStep(struct rade* dv, FARGANState* fargan, rade_text_t textPtr, std::function<void(RADEReceiveStep*)> syncFn)
    : dv_(dv)
    , fargan_(fargan)
    , pendingFeatures_(nullptr)
    , pendingFeaturesIdx_(0)
    , featuresFile_(nullptr)
    , textPtr_(textPtr)
    , syncFn_(syncFn)
    , exitingFeatureThread_(false)
{
    assert(syncState_.is_lock_free());

    if (utRxFeatureFile != "")
    {
        featuresFile_ = fopen((const char*)utRxFeatureFile.ToUTF8(), "wb");
        assert(featuresFile_ != nullptr);
        
        utFeatureThread_ = std::thread(std::bind(&RADEReceiveStep::utFeatureThreadEntry_, this));
    }

    // Pre-allocate buffers so we don't have to do so during real-time operation.
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    outputSamples_ = std::make_unique<short[]>(maxSamples);
    assert(outputSamples_ != nullptr);

    inputBufCplx_ = new RADE_COMP[rade_nin_max(dv_)];
    assert(inputBufCplx_ != nullptr);

    inputBuf_ = new short[rade_nin_max(dv_)];
    assert(inputBuf_ != nullptr);

    featuresOut_ = new float[rade_n_features_in_out(dv_)];
    assert(featuresOut_ != nullptr);

    eooOut_ = new float[rade_n_eoo_bits(dv_)];
    assert(eooOut_ != nullptr);

    pendingFeatures_ = new float[NB_TOTAL_FEATURES];
    assert(pendingFeatures_ != nullptr);
}

RADEReceiveStep::~RADEReceiveStep()
{
    delete[] inputBuf_;
    delete[] inputBufCplx_;
    delete[] featuresOut_;
    delete[] eooOut_;
    delete[] pendingFeatures_;
    outputSamples_ = nullptr;

    if (featuresFile_ != nullptr)
    {
        exitingFeatureThread_ = true;
        featuresAvailableSem_.signal();
        utFeatureThread_.join();
        fclose(featuresFile_);
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

short* RADEReceiveStep::execute(short* inputSamples, int numInputSamples, int* numOutputSamples)
{
    auto maxSamples = std::max(getInputSampleRate(), getOutputSampleRate());
    *numOutputSamples = 0;
    
    inputSampleFifo_.write(inputSamples, numInputSamples);
        
    int   nin = rade_nin(dv_);
    int   nout = 0;
    while ((*numOutputSamples + LPCNET_FRAME_SIZE) < maxSamples && inputSampleFifo_.read(inputBuf_, nin) == 0) 
    {
        assert(nin <= rade_nin_max(dv_));

        // demod per frame processing
        for(int i=0; i<nin; i++) 
        {
            inputBufCplx_[i].real = inputBuf_[i] / 32767.0;
            inputBufCplx_[i].imag = 0.0;
        }

        // RADE processing (input signal->features).
        int hasEooOut = 0;

#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
        __rtsan_disable();
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)

        nout = rade_rx(dv_, featuresOut_, &hasEooOut, eooOut_, inputBufCplx_);

#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
        __rtsan_enable();
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)

        if (hasEooOut && textPtr_ != nullptr)
        {
#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
            __rtsan_disable();
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)

            // Handle RX of bits from EOO.
            rade_text_rx(textPtr_, eooOut_, rade_n_eoo_bits(dv_) / 2);

#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
            __rtsan_enable();
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)

        }
        else if (!hasEooOut)
        {
            if (featuresFile_)
            {
                utFeatures_.write(featuresOut_, nout);
                if (utFeatures_.numUsed() > (0.1 * utFeatures_.capacity()))
                {
                    featuresAvailableSem_.signal();
                }
            }

            for (int i = 0; i < nout; i++)
            {
                pendingFeatures_[pendingFeaturesIdx_++] = featuresOut_[i];
                if (pendingFeaturesIdx_ == NB_TOTAL_FEATURES)
                {
                    pendingFeaturesIdx_ = 0;

                    // FARGAN processing (features->analog audio)
                    float fpcm[LPCNET_FRAME_SIZE];
                    short pcm[LPCNET_FRAME_SIZE];
                    fargan_synthesize(fargan_, fpcm, pendingFeatures_);
                    for (int i = 0; i < LPCNET_FRAME_SIZE; i++) 
                    {
                        pcm[i] = (int)floor(.5 + MIN32(32767, MAX32(-32767, 32768.f*fpcm[i])));
                    }

                    *numOutputSamples += LPCNET_FRAME_SIZE;
                    outputSampleFifo_.write(pcm, LPCNET_FRAME_SIZE);
                }
            }
        }

        nin = rade_nin(dv_);
    }
   
    if (*numOutputSamples > 0)
    { 
        outputSampleFifo_.read(outputSamples_.get(), *numOutputSamples);
    }

#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
    __rtsan_disable();
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)

    syncState_.store(rade_sync(dv_), std::memory_order_release);
    snr_.store(rade_snrdB_3k_est(dv_), std::memory_order_release);

#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
    __rtsan_enable();
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)
    
    syncFn_(this);

    return outputSamples_.get();
}

void RADEReceiveStep::reset()
{
    inputSampleFifo_.reset();
    outputSampleFifo_.reset();
    pendingFeaturesIdx_ = 0;
}

void RADEReceiveStep::utFeatureThreadEntry_()
{
#if defined(__APPLE__)
    // Downgrade thread QoS to Utility to avoid thread contention issues.
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
    
    // Make sure other I/O can throttle us.
    setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_THREAD, IOPOL_THROTTLE);
#endif // defined(__APPLE__)

    std::vector<float*> featureList;
    std::vector<int> featureCnt;
    while (!exitingFeatureThread_)
    {
        auto numToRead = std::min(utFeatures_.numUsed(), utFeatures_.capacity());
        while (numToRead > 0)
        {
            float* fifoRead = new float[numToRead];
            assert(fifoRead != nullptr);
            
            utFeatures_.read(fifoRead, numToRead);
            featureList.push_back(fifoRead);
            featureCnt.push_back(numToRead);
            numToRead = std::min(utFeatures_.numUsed(), utFeatures_.capacity());
        }
        
        featuresAvailableSem_.wait();
    }
    
    auto numToRead = std::min(utFeatures_.numUsed(), utFeatures_.capacity());
    while (numToRead > 0)
    {
        float* fifoRead = new float[numToRead];
        assert(fifoRead != nullptr);
        
        utFeatures_.read(fifoRead, numToRead);
        featureList.push_back(fifoRead);
        featureCnt.push_back(numToRead);
        numToRead = std::min(utFeatures_.numUsed(), utFeatures_.capacity());
    }
    
    int index = 0;
    for (auto& feature : featureList)
    {
        fwrite(feature, sizeof(float) * featureCnt[index++], 1, featuresFile_);
        delete[] feature;
    }
}
