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

#include "ThreadedObject.h"

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

void ThreadedObject::enqueue_(std::function<void()> fn)
{
    std::unique_lock<std::recursive_mutex> lk(eventQueueMutex_);
    eventQueue_.push_back(fn);
    eventQueueCV_.notify_one();
}

void ThreadedObject::eventLoop_()
{
    while (!isDestroying_)
    {
        std::function<void()> fn;
        
        {
            std::unique_lock<std::recursive_mutex> lk(eventQueueMutex_);
            eventQueueCV_.wait(lk, [&]() {
                return isDestroying_ || eventQueue_.size() > 0;
            });
            
            if (isDestroying_)
            {
                break;
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