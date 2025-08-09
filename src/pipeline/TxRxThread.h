//=========================================================================
// Name:            TxRxThread.h
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
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//=========================================================================

#ifndef AUDIO_PIPELINE__TX_RX_THREAD_H
#define AUDIO_PIPELINE__TX_RX_THREAD_H

#include <assert.h>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "AudioPipeline.h"
#include "util/IRealtimeHelper.h"
#include "util/Semaphore.h"

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
        , equalizedMicAudioLink_(micAudioLink)
        , hasEooBeenSent_(false)
        , helper_(helper)
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
    void *Entry();

    void waitForReady() { readySem_.wait(); }
    void signalToStart() { startSem_.signal(); }

private:
    bool  m_tx;
    bool  m_run;
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
    void txProcessing_(IRealtimeHelper* helper) noexcept
#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
[[clang::nonblocking]]
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)
    ;

    void rxProcessing_(IRealtimeHelper* helper) noexcept
#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
[[clang::nonblocking]]
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#endif // defined(__clang__)
    ;

    void clearFifos_();
};

#endif // AUDIO_PIPELINE__TX_RX_THREAD_H
