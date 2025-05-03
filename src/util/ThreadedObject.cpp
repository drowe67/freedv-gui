//=========================================================================
// Name:            ThreadedObject.cpp
// Purpose:         Wrapper to allow class to execute in its own thread.
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

#include <chrono>
#include "ThreadedObject.h"

using namespace std::chrono_literals;

ThreadedObject::ThreadedObject()
    : isDestroying_(false)
{
    // Instantiate thread here rather than the initializer since otherwise
    // we might not be able to guarantee that the mutex is initialized first.
    objectThread_ = std::thread(std::bind(&ThreadedObject::eventLoop_, this));
}

ThreadedObject::~ThreadedObject()
{
    isDestroying_ = true;
    eventQueueCV_.notify_one();
    objectThread_.join();
}

void ThreadedObject::enqueue_(std::function<void()> fn, int timeoutMilliseconds)
{
    std::unique_lock<std::recursive_mutex> lk(eventQueueMutex_, std::defer_lock_t());

    if (timeoutMilliseconds == 0)
    {
        lk.lock();
    }
    else
    {
        auto beginTime = std::chrono::high_resolution_clock::now();
        auto endTime = std::chrono::high_resolution_clock::now();
        bool locked = false;

        do
        {
            if (lk.try_lock())
            {
                locked = true;
                break;
            }
            std::this_thread::sleep_for(1ms);
            endTime = std::chrono::high_resolution_clock::now();
        } while ((endTime - beginTime) < std::chrono::milliseconds(timeoutMilliseconds));
        
        if (!locked)
        {
            // could not lock, so we're not bothering to enqueue.
            return;
        }
    }

    eventQueue_.push_back(fn);
    lk.unlock();

    eventQueueCV_.notify_one();
}

void ThreadedObject::eventLoop_()
{
    while (!isDestroying_)
    {
        std::function<void()> fn;
        
        {
            std::unique_lock<std::recursive_mutex> lk(eventQueueMutex_);
            eventQueueCV_.wait_for(lk, 10ms, [&]() {
                return isDestroying_ || eventQueue_.size() > 0;
            });
            
            if (isDestroying_)
            {
                break;
            }

            if (eventQueue_.size() == 0)
            {
                // Start over and wait again if nothing's in the queue.
                continue;
            }
            
            fn = eventQueue_[0];
            eventQueue_.erase(eventQueue_.begin());
        }
        
        if (fn)
        {
            fn();
        }
    }
}