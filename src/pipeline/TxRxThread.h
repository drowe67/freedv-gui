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
#include <wx/thread.h>
#include <mutex>
#include <condition_variable>

#include "AudioPipeline.h"
#include "util/IRealtimeHelper.h"

// Forward declarations
class LinkStep;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// class txRxThread - experimental tx/rx processing thread
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class TxRxThread : public wxThread
{
public:
    TxRxThread(bool tx, int inputSampleRate, int outputSampleRate, std::shared_ptr<LinkStep> micAudioLink, std::shared_ptr<IRealtimeHelper> helper) 
        : wxThread(wxTHREAD_JOINABLE)
        , m_tx(tx)
        , m_run(1)
        , pipeline_(nullptr)
        , inputSampleRate_(inputSampleRate)
        , outputSampleRate_(outputSampleRate)
        , equalizedMicAudioLink_(micAudioLink)
        , hasEooBeenSent_(false)
        , helper_(helper)
    { 
        assert(inputSampleRate_ > 0);
        assert(outputSampleRate_ > 0);

        inputSamples_ = std::shared_ptr<short>(
            new short[std::max(inputSampleRate_, outputSampleRate_)], 
            std::default_delete<short[]>());
    }
    
    virtual ~TxRxThread()
    {
        // Free allocated buffer
        inputSamples_ = nullptr;
    }

    // thread execution starts here
    void *Entry();

    // called when the thread exits - whether it terminates normally or is
    // stopped with Delete() (but not when it is Kill()ed!)
    void OnExit();

    void terminateThread();
    void notify();

    std::mutex m_processingMutex;
    std::condition_variable m_processingCondVar;

private:
    bool  m_tx;
    bool  m_run;
    std::shared_ptr<AudioPipeline> pipeline_;
    int inputSampleRate_;
    int outputSampleRate_;
    std::shared_ptr<LinkStep> equalizedMicAudioLink_;
    bool hasEooBeenSent_;
    std::shared_ptr<IRealtimeHelper> helper_;
    std::shared_ptr<short> inputSamples_;
    
    void initializePipeline_();
    void txProcessing_();
    void rxProcessing_();
    void clearFifos_();
};

#endif // AUDIO_PIPELINE__TX_RX_THREAD_H
