//=========================================================================
// Name:            MinimalTxRxThread.cpp
// Purpose:         Implements the main processing thread for audio I/O.
//
// Authors:         Mooneer Salem
// License:
//
//  All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANYg_eoo_enqueued
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//=========================================================================

#include <chrono>
using namespace std::chrono_literals;

#include "MinimalTxRxThread.h"
#include "../pipeline/paCallbackData.h"

#include "../../pipeline/AgcStep.h"
#include "../../pipeline/SpeexStep.h"
#include "../../pipeline/ResampleStep.h"
#include "../../pipeline/LevelAdjustStep.h"
#include "../../pipeline/RADEReceiveStep.h"
#include "../../pipeline/BandwidthExpandStep.h"

#include "../../util/logging/ulog.h"
#include "../../os/os_interface.h"

#include "../../pipeline/pipeline_defines.h"

#include "codec2_alloc.h"

extern std::atomic<int> g_tx;
extern bool endingTx;
bool g_eoo_enqueued;

static const float TxScaleFactor_ = 2.0;

int MinimalTxRxThread::getTxNNomModemSamples() const
{
    const int NUM_SAMPLES_SILENCE = 60 * RADE_MODEM_SAMPLE_RATE / 1000;
    return std::max(rade_n_tx_out(rade_), rade_n_tx_eoo_out(rade_) + NUM_SAMPLES_SILENCE);
}

int MinimalTxRxThread::getRxNumSpeechSamples() const
{
    return 1920;
}

// Experimental options for potential future release:
//
// * ENABLE_FASTER_PLOTS: This uses a faster resampling algorithm to reduce the CPU
//   usage required to generate various plots in the user interface. (Tech note: When 
//   enabled, libsamplerate is directed to use SRC_LINEAR for the plot resampling.)
// * ENABLE_PROCESSING_STATS: This causes execution statistics to be collected for RX and TX
//   processing and output in the log after the user pushes Stop. (Define in .h file.)

#define ENABLE_FASTER_PLOTS

void MinimalTxRxThread::initializePipeline_()
{
    pipeline_ = std::make_unique<AudioPipeline>(inputSampleRate_, outputSampleRate_);
    
    if (m_tx)
    {
        txStep_ = new RADETransmitStep(rade_, encState_);
        auto agcStep = new AgcStep(txStep_->getInputSampleRate());
        auto speexStep = new SpeexStep(txStep_->getInputSampleRate());
        pipeline_->appendPipelineStep(speexStep);
        pipeline_->appendPipelineStep(agcStep);
        pipeline_->appendPipelineStep(txStep_);
        
        auto levelAdjustStep = new LevelAdjustStep(outputSampleRate_, +[]() FREEDV_NONBLOCKING { return TxScaleFactor_; });
        pipeline_->appendPipelineStep(levelAdjustStep);
    }
    else
    {
        auto radeRxStep = new RADEReceiveStep(rade_, farganState_, radeText_, +[](RADEReceiveStep* step) FREEDV_NONBLOCKING { 
            MinimalTxRxThread* thisObj = (MinimalTxRxThread*)step->getStateObj();
            thisObj->snr_.store(step->getSnr(), std::memory_order_release);
            thisObj->sync_.store(step->getSync(), std::memory_order_release); 
        });
        radeRxStep->setStateObj(this);
        pipeline_->appendPipelineStep(radeRxStep);
        
        auto bwExpandStep = new BandwidthExpandStep();
        pipeline_->appendPipelineStep(bwExpandStep);

        // Clear anything in the FIFO before resuming decode.
        clearFifos_();
    }
}

void* MinimalTxRxThread::Entry()
{
    // Get raw pointer so we don't need to constantly access the shared_ptr
    // and thus constantly increment/decrement refcounts.
    IRealtimeHelper* helper = helper_.get();
    
    // Ensure that O(1) memory allocator is used for Codec2
    // instead of standard malloc().
    codec2_initialize_realtime(CODEC2_REAL_TIME_MEMORY_SIZE);
    
    initializePipeline_();
    
    // Request real-time scheduling from the operating system.
    helper->setHelperRealTime();

#if defined(ENABLE_PROCESSING_STATS)
    resetStats_();
#endif // defined(ENABLE_PROCESSING_STATS)

#if defined(__linux__)
    const char* threadName = nullptr;
    if (m_tx) threadName = "FreeDV txThread";
    else threadName = "FreeDV rxThread";
    pthread_setname_np(pthread_self(), threadName);
#endif // defined(__linux__)

    // Make sure we don't start processing until
    // the main thread is ready.
    readySem_.signal();
    startSem_.wait();
    clearFifos_();

    while (m_run)
    {
        if (!m_run) break;
        
        //log_info("thread woken up: m_tx=%d", (int)m_tx);
        helper->startRealTimeWork();

        if (m_tx) txProcessing_(helper);
        else rxProcessing_(helper);

        // Determine whether we need to pause for a shorter amount
        // of time to avoid dropouts.
        auto outFifo = cbData_->outfifo1;
        if (!m_tx)
        {
            outFifo = cbData_->outfifo2;
        }
        auto totalFifoCapacity = outFifo->capacity();
        auto fifoUsed = outFifo->numUsed();
        helper->stopRealTimeWork(fifoUsed < totalFifoCapacity / 2);
    }

#if defined(ENABLE_PROCESSING_STATS)
    reportStats_();
#endif // defined(ENABLE_PROCESSING_STATS)

    // Force pipeline to delete itself when we're done with the thread.
    pipeline_ = nullptr;
    
    // Return to normal scheduling
    helper->clearHelperRealTime();
    
    codec2_disable_realtime();
    
    return NULL;
}

#if defined(ENABLE_PROCESSING_STATS)
void MinimalTxRxThread::resetStats_()
{
    numTimeSamples_ = 0;
    minDuration_ = 1e9;
    maxDuration_ = 0;
    sumDuration_ = 0;
    sumDoubleDuration_ = 0; 
}

void MinimalTxRxThread::startTimer_()
{
    timeStart_ = std::chrono::high_resolution_clock::now();
}

void MinimalTxRxThread::endTimer_()
{
    auto e = std::chrono::high_resolution_clock::now();
    auto d = std::chrono::duration_cast<std::chrono::nanoseconds>(e - timeStart_).count();
    numTimeSamples_++; 
    if (d < minDuration_)
    {
        minDuration_ = d;
        minTime_ = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    }
    if (d > maxDuration_)
    {
        maxDuration_ = d;
        maxTime_ = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    }
    sumDuration_ += d; sumDoubleDuration_ += pow(d, 2);
}

void MinimalTxRxThread::reportStats_()
{
    if (numTimeSamples_ > 0)
    {
        std::tm * minTm = std::localtime(&minTime_);
        std::tm * maxTm = std::localtime(&maxTime_);
        char bufMin[32];
        char bufMax[32];
        std::strftime(bufMin, 32, "%H:%M:%S", minTm);
        std::strftime(bufMax, 32, "%H:%M:%S", maxTm);
        
        log_info("m_tx = %d, min = %f ns [%s], max = %f ns [%s], mean = %f ns, stdev = %f ns (n = %d)", m_tx, minDuration_, bufMin, maxDuration_, bufMax, sumDuration_ / numTimeSamples_, sqrt((sumDoubleDuration_ - pow(sumDuration_, 2)/numTimeSamples_) / (numTimeSamples_ - 1)), numTimeSamples_);
    }
}
#endif // defined(ENABLE_PROCESSING_STATS)

void MinimalTxRxThread::clearFifos_()
{
    if (m_tx)
    {
        cbData_->infifo1->reset();
        cbData_->outfifo1->reset();
    }
    else
    {
        cbData_->infifo2->reset();
        cbData_->outfifo2->reset();
    }
}

//---------------------------------------------------------------------------------------------
// Main real time processing for tx and rx of FreeDV signals, run in its own threads
//---------------------------------------------------------------------------------------------

void MinimalTxRxThread::txProcessing_(IRealtimeHelper* helper) noexcept
#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
[[clang::nonblocking]]
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)
{
    // Buffers re-used by tx and rx processing.  We take samples from
    // the sound card, and resample them for the freedv modem input
    // sample rate.  Typically the sound card is running at 48 or 44.1
    // kHz, and the modem at 8kHz

    //
    //  TX side processing --------------------------------------------
    //

    if (g_tx.load(std::memory_order_acquire)) {        
        if (deferReset_)
        {
            // We just entered TX from RX.
            // Reset pipeline and wipe anything in the FIFO.
            deferReset_ = false;
            pipeline_->reset();
            clearFifos_();
        }

        // This while loop locks the modulator to the sample rate of
        // the input sound card.  We want to make sure that modulator samples
        // are uninterrupted by differences in sample rate between
        // this sound card and the output sound card.

        // Run code inside this while loop as soon as we have enough
        // room for one frame of modem samples.  Aim is to keep
        // outfifo1 nice and full so we don't have any gaps in tx
        // signal.

        unsigned int nsam_one_modem_frame = getTxNNomModemSamples() * ((float)outputSampleRate_ / (float)RADE_MODEM_SAMPLE_RATE);

        int nsam_in_48 = (inputSampleRate_ * FRAME_DURATION_MS) / MS_TO_SEC;
        assert(nsam_in_48 > 0);

        int             nout;

        while(!helper->mustStopWork() && (unsigned)cbData_->outfifo1->numFree() >= nsam_one_modem_frame) {        
            // OK to generate a frame of modem output samples we need
            // an input frame of speech samples from the microphone.
            
#if defined(ENABLE_PROCESSING_STATS)
            startTimer_();
#endif // defined(ENABLE_PROCESSING_STATS)

            // infifo2 is written to by another sound card so it may
            // over or underflow, but we don't really care.  It will
            // just result in a short interruption in audio being fed
            // to codec2_enc, possibly making a click every now and
            // again in the decoded audio at the other end.

            // There may be recorded audio left to encode while ending TX. To handle this,
            // we keep reading from the FIFO until we have less than nsam_in_48 samples available.
            auto inputPtr = inputSamples_.get();
            int nread = cbData_->infifo1->read(inputPtr, nsam_in_48);            
            if (nread != 0)
            {
                inputPtr = inputSamplesZeros_.get();
            }
            if (nread != 0 && endingTx)
            {
                if (!hasEooBeenSent_)
                {
                    // Special case for handling RADE EOT
                    txStep_->restartVocoder();
                    hasEooBeenSent_ = true;
                }

                auto outputSamples = pipeline_->execute(inputPtr, 0, &nout);
                if (nout > 0 && outputSamples != nullptr)
                {
                    if (cbData_->outfifo1->write(outputSamples, nout) != 0)
                    {
                        log_warn("Could not inject resampled EOO samples (space remaining in FIFO = %d)", cbData_->outfifo1->numFree());
                    }
                }
                else
                {
                    if (!g_eoo_enqueued)
                    {
                        // Add 40ms of additional silence as Flex will otherwise cut off EOO.
                        cbData_->outfifo1->write(inputSamplesZeros_.get(), 40 * outputSampleRate_ / 1000);
                    }
                    g_eoo_enqueued = true;
                }
                break;
            }
            else
            {
                g_eoo_enqueued = false;
                hasEooBeenSent_ = false;
            }
            
            auto outputSamples = pipeline_->execute(inputPtr, nsam_in_48, &nout);
            
            if (outputSamples != nullptr)
            {
                cbData_->outfifo1->write(outputSamples, nout);
            }
            
#if defined(ENABLE_PROCESSING_STATS)
            endTimer_();
#endif // defined(ENABLE_PROCESSING_STATS)
        }
    }
    else
    {
        // Defer reset until next time we go into TX.
        deferReset_ = true;
    }
}

void MinimalTxRxThread::rxProcessing_(IRealtimeHelper* helper) noexcept
#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
[[clang::nonblocking]]
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)
{
    // Buffers re-used by tx and rx processing.  We take samples from
    // the sound card, and resample them for the freedv modem input
    // sample rate.  Typically the sound card is running at 48 or 44.1
    // kHz, and the modem at 8kHz.

    //
    //  RX side processing --------------------------------------------
    //
    
    // Attempt to read one processing frame (about 20ms) of receive samples,  we 
    // keep this frame duration constant across modes and sound card sample rates
    int nsam = (inputSampleRate_ * FRAME_DURATION_MS) / MS_TO_SEC;
    assert(nsam > 0);

    int             nout;


    bool tmpTx = g_tx.load(std::memory_order_acquire);
    bool processInputFifo =  !tmpTx;
    if (!processInputFifo)
    {
        clearFifos_();
    }

    int nsam_one_speech_frame = getRxNumSpeechSamples() * ((float)outputSampleRate_ / (float)RADE_SPEECH_SAMPLE_RATE);
    auto outFifo = cbData_->outfifo2;

    // while we have enough input samples available and enough space in the output FIFO ... 
    while (!helper->mustStopWork() && processInputFifo && outFifo->numFree() >= nsam_one_speech_frame && cbData_->infifo2->read(inputSamples_.get(), nsam) == 0) {
        
#if defined(ENABLE_PROCESSING_STATS)
        startTimer_();
#endif // defined(ENABLE_PROCESSING_STATS)
        
        // send latest squelch level to FreeDV API, as it handles squelch internally
        auto outputSamples = pipeline_->execute(inputSamples_.get(), nsam, &nout);
        
        if (nout > 0 && outputSamples != nullptr)
        {
            outFifo->write(outputSamples, nout);
        }
        
        tmpTx = g_tx.load(std::memory_order_acquire);
        processInputFifo = !tmpTx;
        
#if defined(ENABLE_PROCESSING_STATS)
        endTimer_();
#endif // defined(ENABLE_PROCESSING_STATS)
    }
}
