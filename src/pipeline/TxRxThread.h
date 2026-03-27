//=========================================================================
// Name:            TxRxThread.h
// Purpose:         Implements the main processing thread for audio I/O.
//
// Authors:         Mooneer Salem
// License:
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//=========================================================================

#ifndef AUDIO_PIPELINE__TX_RX_THREAD_H
#define AUDIO_PIPELINE__TX_RX_THREAD_H

#include <assert.h>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "AudioPipeline.h"
#include "util/IRealtimeHelper.h"
#include "util/Semaphore.h"
#include "util/sanitizers.h"

// Forward declarations
class LinkStep;

//#define ENABLE_PROCESSING_STATS

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// class txRxThread - tx/rx processing thread
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class TxRxThread
{
public:
    TxRxThread(bool tx, int inputSampleRate, int outputSampleRate, std::shared_ptr<LinkStep> micAudioLink, std::shared_ptr<IRealtimeHelper> helper) 
        : m_tx(tx)
        , m_run(1)
        , pipeline_(nullptr)
        , inputSampleRate_(inputSampleRate)
        , outputSampleRate_(outputSampleRate)
        , equalizedMicAudioLink_(std::move(micAudioLink))
        , hasEooBeenSent_(false)
        , helper_(std::move(helper))
        , deferReset_(false)
    { 
        assert(inputSampleRate_ > 0);
        assert(outputSampleRate_ > 0);

        auto numSamples = std::max(inputSampleRate_, outputSampleRate_);
        inputSamples_ = std::make_unique<short[]>(numSamples);
        assert(inputSamples_ != nullptr);
        inputSamplesZeros_ = std::make_unique<short[]>(numSamples);
        assert(inputSamplesZeros_ != nullptr);
        memset(inputSamplesZeros_.get(), 0, numSamples * sizeof(short));
    }
    
    virtual ~TxRxThread()
    {
        // Free allocated buffer
        stop();
        inputSamples_ = nullptr;
    }

    void start()
    {
        thread_ = std::thread(std::bind(&TxRxThread::Entry, this));
    }

    void stop()
    {
        m_run = false;
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

    // thread execution starts here
    void *Entry() noexcept;

    void waitForReady() { readySem_.wait(); }
    void signalToStart() { startSem_.signal(); }

private:
    bool  m_tx;
    std::atomic<bool>  m_run;
    std::unique_ptr<AudioPipeline> pipeline_;
    int inputSampleRate_;
    int outputSampleRate_;
    std::shared_ptr<LinkStep> equalizedMicAudioLink_;
    bool hasEooBeenSent_;
    std::shared_ptr<IRealtimeHelper> helper_;
    std::unique_ptr<short[]> inputSamples_;
    std::unique_ptr<short[]> inputSamplesZeros_;
    bool deferReset_;
    std::thread thread_;

    Semaphore readySem_;
    Semaphore startSem_;

#if defined(ENABLE_PROCESSING_STATS)
    int numTimeSamples_;
    double minDuration_;
    std::time_t minTime_;
    double maxDuration_;
    std::time_t maxTime_;
    double sumDuration_;
    double sumDoubleDuration_;
    std::chrono::time_point<std::chrono::high_resolution_clock> timeStart_;
    
    void resetStats_();
    void startTimer_();
    void endTimer_();
    void reportStats_();    
#endif // defined(ENABLE_PROCESSING_STATS)
    
    void initializePipeline_();
    void txProcessing_(IRealtimeHelper* helper) FREEDV_NONBLOCKING;
    void rxProcessing_(IRealtimeHelper* helper) FREEDV_NONBLOCKING;
    void clearFifos_() FREEDV_NONBLOCKING;
};

#endif // AUDIO_PIPELINE__TX_RX_THREAD_H
