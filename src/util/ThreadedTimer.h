//=========================================================================
// Name:            ThreadedTimer.h
// Purpose:         Timer object with minimal dependencies.
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

#ifndef THREADED_TIMER_H
#define THREADED_TIMER_H

#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif // defined(__APPLE__)

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <functional>
#include <atomic>

class ThreadedTimer
{
public:
    using TimerCallbackFn = std::function<void(ThreadedTimer&)>;
    
    ThreadedTimer();
    ThreadedTimer(int milliseconds, TimerCallbackFn fn, bool repeat);
    virtual ~ThreadedTimer();

    void setTimeout(int milliseconds);
    void setCallback(TimerCallbackFn fn);
    void setRepeat(bool repeat);
    
    void start();
    void stop();
    void restart();
    
    bool isRunning();
    
private:
    std::mutex timerMutex_;
    std::atomic<bool> isDestroying_;
    std::atomic<bool> isRestarting_;
    
#if defined(__APPLE__)
    dispatch_source_t internalTimer_;
    
    static void OnHandleTimer_(void* context);
#else
    std::thread objectThread_;
    std::condition_variable timerCV_;
#endif // defined(__APPLE__)
    
    TimerCallbackFn fn_;
    bool repeat_;
    int timeoutMilliseconds_;

#if !defined(__APPLE__)
    void eventLoop_();
#endif // !defined(__APPLE__)
};

#endif // THREADED_TIMER_H
