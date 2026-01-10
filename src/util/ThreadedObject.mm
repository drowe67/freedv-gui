//=========================================================================
// Name:            ThreadedObject.mm
// Purpose:         macOS-specific implementation of ThreadedObject using GCD.
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
#include <cassert>

#include "ThreadedObject.h"

using namespace std::chrono_literals;

ThreadedObject::ThreadedObject(std::string name, ThreadedObject* parent)
    : parent_(parent)
    , name_(std::move(name))
    , suppressEnqueue_(false)
{
    dispatch_queue_t parentQueue;
    
    if (parent_ != nullptr)
    {
        parentQueue = parent_->queue_;
    }
    else
    {
        parentQueue = dispatch_get_global_queue(QOS_CLASS_UTILITY, 0);
    }
    
    queue_ = dispatch_queue_create_with_target(nullptr, DISPATCH_QUEUE_SERIAL, parentQueue);
    assert(queue_ != nil);
    
    numQueued_ = 0;
}

ThreadedObject::~ThreadedObject()
{
    // Wait for anything pending to finish executing.
    waitForAllTasksComplete_();

    // Release GCD objects
    dispatch_release(queue_);
}

void ThreadedObject::enqueue_(std::function<void()> fn, int) // NOLINT
{
    if (suppressEnqueue_.load(std::memory_order_acquire)) return;

    // note: timeout not implemented
    numQueued_++;
    dispatch_async(queue_, ^() {
        fn();
        numQueued_--;
    });
}

void ThreadedObject::waitForAllTasksComplete_()
{
    suppressEnqueue_.store(true, std::memory_order_release);

    // We wait forever here instead of 250ms as with the non-macOS implementation
    // since we don't have a way to just clear out the queued events in the dispatch
    // queue after the timeout.
    while (numQueued_ > 0)
    {
        std::this_thread::sleep_for(10ms);
    }

    suppressEnqueue_.store(false, std::memory_order_release);
}
