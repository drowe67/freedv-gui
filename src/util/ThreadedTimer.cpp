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

#include <cinttypes>

#if defined(__APPLE__)
#include <pthread.h>
#endif // defined(__APPLE__)

#define PERCENT_TOLERANCE 0.05

#if !defined(__APPLE__)
ThreadedTimer::TimerServer ThreadedTimer::TheTimerServer_;

ThreadedTimer::TimerServer::TimerServer()
    : isDestroying_(false)
    , objectThread_(std::bind(&ThreadedTimer::TimerServer::eventLoop_, this))
{
    // empty
}

ThreadedTimer::TimerServer::~TimerServer()
{
    isDestroying_ = true;
    timerCV_.notify_one();
    objectThread_.join();
}

void ThreadedTimer::TimerServer::registerTimer(ThreadedTimer* timer)
{
    std::unique_lock<std::mutex> lk(mutex_);
    timerQueue_.push(timer);
    timerCV_.notify_one(); // update wait time
}

void ThreadedTimer::TimerServer::unregisterTimer(ThreadedTimer* timer)
{
    std::unique_lock<std::mutex> lk(mutex_);

    // XXX - need to find a more optimal way of doing this
    std::vector<ThreadedTimer*> tmpTimerList;
    while (!timerQueue_.empty())
    {
        auto tmp = timerQueue_.top();
        timerQueue_.pop();

        if (tmp != timer)
        {
            tmpTimerList.push_back(tmp);
        }
    }

    for (auto& tmp : tmpTimerList)
    {
        timerQueue_.push(tmp);
    }
    timerCV_.notify_one(); // update wait time
}

void ThreadedTimer::TimerServer::eventLoop_()
{
    std::chrono::time_point<std::chrono::steady_clock> nextWaitTime;

    while (!isDestroying_.load(std::memory_order_acquire))
    {
        std::unique_lock<std::mutex> lk(mutex_);

        if (timerQueue_.empty())
        {
            timerCV_.wait(lk, [&]() {
                return isDestroying_.load(std::memory_order_acquire) || !timerQueue_.empty();
            });
        }
        else
        {
            auto& tmpTimer = timerQueue_.top();
            nextWaitTime = tmpTimer->nextFireTime_;
            timerCV_.wait_until(lk, nextWaitTime, [&]() { 
                return isDestroying_.load(std::memory_order_acquire);
            });
        }

        auto currentTime = std::chrono::steady_clock::now();

        // Execute timers that have fired.
        while (
            !isDestroying_.load(std::memory_order_acquire) && 
            !timerQueue_.empty() && timerQueue_.top()->nextFireTime_ <= currentTime)
        {
            ThreadedTimer* tmpTimer = timerQueue_.top();

            // Set next fire time if repeating, otherwise deregister
            // NOTE: we have to drop the lock here to avoid deadlocks.
            timerQueue_.pop();
            lk.unlock();
            if (tmpTimer->repeat_)
            {
                tmpTimer->nextFireTime_ = currentTime + std::chrono::milliseconds(tmpTimer->timeoutMilliseconds_);
                registerTimer(tmpTimer);
            }
            else
            {
                std::unique_lock<std::mutex> timerLock(tmpTimer->timerMutex_);
                tmpTimer->isRunning_ = false; 
            }

            tmpTimer->fn_(*tmpTimer);
            lk.lock();
        }        
    }
}
#endif // !defined(__APPLE__)

ThreadedTimer::ThreadedTimer()
    : 
#if defined(__APPLE__)
      internalTimer_(nullptr),
#endif // defined(__APPLE__)
      repeat_(false)
    , timeoutMilliseconds_(0)
{
    // empty
}

ThreadedTimer::ThreadedTimer(int milliseconds, TimerCallbackFn fn, bool repeat)
#if defined(__APPLE__)
    : internalTimer_(nullptr)
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
    return isRunning_;
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
    nextFireTime_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMilliseconds_);
    TheTimerServer_.registerTimer(this);
    isRunning_ = true;
#endif // defined(__APPLE__)
}

void ThreadedTimer::stop()
{
    std::unique_lock<std::mutex> lk(timerMutex_);
#if defined(__APPLE__)
    if (internalTimer_ != nullptr)
    {
        dispatch_source_cancel(internalTimer_);
        dispatch_release(internalTimer_);
        internalTimer_ = nullptr;
    }
#else
    TheTimerServer_.unregisterTimer(this);
    isRunning_ = false;
#endif // defined(__APPLE__)
}

void ThreadedTimer::restart()
{
    stop();
    start();
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
#endif // !defined(__APPLE__)
