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
#include "../os/os_interface.h"
#include "ThreadedObject.h"

using namespace std::chrono_literals;

ThreadedObject::ThreadedObject(std::string name, ThreadedObject* parent)
    : suppressEnqueue_(false)
    , parent_(parent)
    , name_(std::move(name))
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
        waitForAllTasksComplete_();
        
        // Clear all remaining items from the event queue before ending the thread.
        // This helps make sure we don't accidentally execute code when this object
        // is no longer alive.
        {
            std::unique_lock<std::recursive_mutex> lk(eventQueueMutex_);
            eventQueue_.clear();
            isDestroying_.store(true, std::memory_order_release);
            eventQueueCV_.notify_one();
        }

        objectThread_.join();
    }
}

void ThreadedObject::enqueue_(std::function<void()> fn, int timeoutMilliseconds)
{
    if (suppressEnqueue_.load(std::memory_order_acquire)) return;

    if (parent_ != nullptr)
    {
        parent_->enqueue_(std::move(fn), timeoutMilliseconds);
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

        eventQueue_.push_back(std::move(fn));
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

    SetThreadName(name_);

    while (!isDestroying_.load(std::memory_order_acquire))
    {
        std::function<void()> fn;
        
        int count = 0;

        do
        {
            {
                std::unique_lock<std::recursive_mutex> lk(eventQueueMutex_);

                count = eventQueue_.size();
                if (count == 0 && !isDestroying_.load(std::memory_order_acquire))
                {
                    eventQueueCV_.wait(lk, [&]() {
                        return isDestroying_.load(std::memory_order_acquire) || eventQueue_.size() > 0;
                    });
                    
                    count = eventQueue_.size();
                }

                if (isDestroying_.load(std::memory_order_acquire) || count == 0)
                {
                    break;
                }
                
                fn = eventQueue_[0];
                eventQueue_.pop_front();
            }
        
            if (!isDestroying_.load(std::memory_order_acquire) && fn)
            {
                fn();
            }

            count--;
        } while (!isDestroying_.load(std::memory_order_acquire) && count > 0);
    }
}

void ThreadedObject::waitForAllTasksComplete_()
{
    std::unique_lock<std::recursive_mutex> lk(eventQueueMutex_);
    suppressEnqueue_.store(true, std::memory_order_release);
    auto count = eventQueue_.size();
    lk.unlock();

    constexpr int MAX_TIMEOUT_COUNT = 250; // should be ~250ms
    int timeoutCount = 0;
    while (count > 0 && timeoutCount < MAX_TIMEOUT_COUNT)
    {
        std::this_thread::sleep_for(1ms);
        lk.lock();
        count = eventQueue_.size();
        lk.unlock();

        timeoutCount++;
    }

    suppressEnqueue_.store(false, std::memory_order_release);
}