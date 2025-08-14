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

ThreadedObject::ThreadedObject(ThreadedObject* parent)
    : parent_(parent)
    , isDestroying_(false)
{
    // Instantiate thread here rather than the initializer since otherwise
    // we might not be able to guarantee that the mutex is initialized first.
    if (parent_ == nullptr)
    {
        objectThread_ = std::thread(std::bind(&ThreadedObject::eventLoop_, this));
    }
}

ThreadedObject::~ThreadedObject()
{
    if (objectThread_.joinable())
    {
        isDestroying_ = true;
        eventQueueCV_.notify_one();
        objectThread_.join();
    }
}

void ThreadedObject::enqueue_(std::function<void()> fn, int timeoutMilliseconds)
{
    if (parent_ != nullptr)
    {
        parent_->enqueue_(fn, timeoutMilliseconds);
    }
    else
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
}

void ThreadedObject::eventLoop_()
{
#if defined(__APPLE__)
    // Downgrade thread QoS to Utility to avoid thread contention issues.
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY,0);
#endif // defined(__APPLE__)

    while (!isDestroying_)
    {
        std::function<void()> fn;
        
        int count = 0;

        do
        {
            {
                std::unique_lock<std::recursive_mutex> lk(eventQueueMutex_);

                if (count == 0)
                {
                    eventQueueCV_.wait(lk, [&]() {
                        return isDestroying_ || eventQueue_.size() > 0;
                    });
                    
                    count = eventQueue_.size();
                }

                if (isDestroying_ || count == 0)
                {
                    break;
                }
                
                fn = eventQueue_[0];
                eventQueue_.pop_front();
            }
        
            if (fn)
            {
                fn();
            }

            count--;
        } while (count > 0);
    }
}
