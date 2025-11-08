//=========================================================================
// Name:            ThreadedTimer.cpp
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

#include "ThreadedTimer.h"

#if defined(__APPLE__)
#include <pthread.h>
#endif // defined(__APPLE__)

#define PERCENT_TOLERANCE 0.05

ThreadedTimer::ThreadedTimer()
    : isDestroying_(false)
    , isRestarting_(false)
#if defined(__APPLE__)
    , internalTimer_(nullptr)
#endif // defined(__APPLE__)
    , repeat_(false)
    , timeoutMilliseconds_(0)
{
    // empty
}

ThreadedTimer::ThreadedTimer(int milliseconds, TimerCallbackFn fn, bool repeat)
    : isDestroying_(false)
#if defined(__APPLE__)
    , internalTimer_(nullptr)
#endif // defined(__APPLE__)
{
    setTimeout(milliseconds);
    setCallback(std::move(fn));
    setRepeat(repeat);
}

ThreadedTimer::~ThreadedTimer()
{
    stop();
}

void ThreadedTimer::setTimeout(int milliseconds)
{
    std::unique_lock<std::mutex> lk(timerMutex_);
    timeoutMilliseconds_ = milliseconds;
}

void ThreadedTimer::setCallback(TimerCallbackFn fn)
{
    std::unique_lock<std::mutex> lk(timerMutex_);
    fn_ = std::move(fn);
}

void ThreadedTimer::setRepeat(bool repeat)
{
    std::unique_lock<std::mutex> lk(timerMutex_);
    repeat_ = repeat;
}

bool ThreadedTimer::isRunning()
{
#if defined(__APPLE__)
    return internalTimer_ != nullptr;
#else
    return objectThread_.joinable();
#endif // defined(__APPLE__)
}
    
void ThreadedTimer::start()
{
    stop();

#if defined(__APPLE__)
    {
        std::unique_lock<std::mutex> lk(timerMutex_);
        internalTimer_ =
            dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
        if (internalTimer_ != nullptr)
        {
            auto timeoutNanoseconds = timeoutMilliseconds_ * NSEC_PER_MSEC;
            auto leeway = timeoutNanoseconds * PERCENT_TOLERANCE;
            dispatch_set_context(internalTimer_, this);
            dispatch_source_set_timer(internalTimer_, dispatch_time(DISPATCH_TIME_NOW, timeoutNanoseconds), timeoutNanoseconds, leeway);
            dispatch_source_set_event_handler_f(internalTimer_, &ThreadedTimer::OnHandleTimer_);
            dispatch_resume(internalTimer_);
        }
    }
#else
    isDestroying_.store(false, std::memory_order_release);
    objectThread_ = std::thread(std::bind(&ThreadedTimer::eventLoop_, this));
#endif // defined(__APPLE__)
}

void ThreadedTimer::stop()
{
#if defined(__APPLE__)
    std::unique_lock<std::mutex> lk(timerMutex_);
    if (internalTimer_ != nullptr)
    {
        dispatch_source_cancel(internalTimer_);
        dispatch_release(internalTimer_);
        internalTimer_ = nullptr;
    }
#else
    if (objectThread_.joinable())
    {
        isDestroying_.store(true, std::memory_order_release);
        timerCV_.notify_one();
        objectThread_.join();
    }
#endif // defined(__APPLE__)
}

void ThreadedTimer::restart()
{
#if defined(__APPLE__)
    stop();
    start();
#else
    if (objectThread_.joinable())
    {
        isRestarting_.store(true, std::memory_order_release);
        timerCV_.notify_one();
    }
    else
    {
        start();
    }
#endif // defined(__APPLE__)
}

#if defined(__APPLE__)
void ThreadedTimer::OnHandleTimer_(void* context)
{
    ThreadedTimer* thisObj = (ThreadedTimer*)context;
    if (!thisObj->repeat_)
    {
        thisObj->stop();
    }
    {
        std::unique_lock<std::mutex> lk(thisObj->timerMutex_);
        thisObj->fn_(*thisObj);
    }
}
#else
void ThreadedTimer::eventLoop_()
{
#if defined(__APPLE__)
    // Downgrade thread QoS to Utility to avoid thread contention issues.
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY,0);
#endif // defined(__APPLE__)

    do
    {
        std::unique_lock<std::mutex> lk(timerMutex_);
        isRestarting_.store(false, std::memory_order_release);
        if (!timerCV_.wait_for(lk, std::chrono::milliseconds(timeoutMilliseconds_), [&]() { 
            return 
                isDestroying_.load(std::memory_order_acquire) || 
                isRestarting_.load(std::memory_order_acquire);
            }) && fn_)
        {
            fn_(*this);
        }
    } while (!isDestroying_.load(std::memory_order_acquire) && (isRestarting_.load(std::memory_order_acquire) || repeat_));
}
#endif // !defined(__APPLE__)
