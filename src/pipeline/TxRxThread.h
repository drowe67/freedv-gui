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

#include <wx/thread.h>
#include <mutex>

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
// class txRxThread - experimental tx/rx processing thread
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=--=-=-=-=
class TxRxThread : public wxThread
{
public:
    TxRxThread(bool tx) 
        : wxThread(wxTHREAD_JOINABLE)
        , m_tx(tx)
        , m_run(1) { /* empty */ }

    // thread execution starts here
    void *Entry()
    {
        while (m_run)
        {
            {
                std::unique_lock<std::mutex> lk(m_processingMutex);
                if (m_processingCondVar.wait_for(lk, std::chrono::milliseconds(100)) == std::cv_status::timeout)
                {
                    fprintf(stderr, "txRxThread: timeout while waiting for CV, tx = %d\n", m_tx);
                }
            }
            if (m_tx) txProcessing();
            else rxProcessing();
        }
        
        return NULL;
    }

    // called when the thread exits - whether it terminates normally or is
    // stopped with Delete() (but not when it is Kill()ed!)
    void OnExit() { }

    void terminateThread()
    {
        m_run = 0;
        notify();
    }

    void notify()
    {
        {
            std::unique_lock<std::mutex> lk(m_processingMutex);
            m_processingCondVar.notify_all();
        }
    }

    std::mutex m_processingMutex;
    std::condition_variable m_processingCondVar;

private:
    bool  m_tx;
    bool  m_run;
    
    void txProcessing();
    void rxProcessing();
};

#endif // AUDIO_PIPELINE__TX_RX_THREAD_H