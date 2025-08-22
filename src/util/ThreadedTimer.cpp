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

ThreadedTimer::ThreadedTimer()
    : isDestroying_(false)
    , isRestarting_(false)
    , repeat_(false)
    , timeoutMilliseconds_(0)
{
    // empty
}

ThreadedTimer::ThreadedTimer(int milliseconds, TimerCallbackFn fn, bool repeat)
    : isDestroying_(false)
{
    setTimeout(milliseconds);
    setCallback(fn);
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
    fn_ = fn;
}

void ThreadedTimer::setRepeat(bool repeat)
{
    std::unique_lock<std::mutex> lk(timerMutex_);
    repeat_ = repeat;
}

bool ThreadedTimer::isRunning()
{
    return objectThread_.joinable();
}
    
void ThreadedTimer::start()
{
    stop();
    
    isDestroying_ = false;
    objectThread_ = std::thread(std::bind(&ThreadedTimer::eventLoop_, this));
}

void ThreadedTimer::stop()
{
    if (objectThread_.joinable())
    {
        isDestroying_ = true;
        timerCV_.notify_one();
        objectThread_.join();
    }
}

void ThreadedTimer::restart()
{
    if (objectThread_.joinable())
    {
        isRestarting_ = true;
        timerCV_.notify_one();
    }
    else
    {
        start();
    }
}

void ThreadedTimer::eventLoop_()
{
#if defined(__APPLE__)
    // Downgrade thread QoS to Utility to avoid thread contention issues.
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY,0);
#endif // defined(__APPLE__)

    do
    {
        std::unique_lock<std::mutex> lk(timerMutex_);
        isRestarting_ = false;
        if (!timerCV_.wait_for(lk, std::chrono::milliseconds(timeoutMilliseconds_), [&]() { return isDestroying_ || isRestarting_; }) && fn_)
        {
            fn_(*this);
        }
    } while (!isDestroying_ && (isRestarting_ || repeat_));
}
