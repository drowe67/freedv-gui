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

#include <cassert>

#include "ThreadedObject.h"

constexpr static int MS_TO_NSEC = 1000000;

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

    group_ = dispatch_group_create();
    assert(group_ != nil);
}

ThreadedObject::~ThreadedObject()
{
    // Wait for anything pending to finish executing.
    waitForAllTasksComplete_();

    // Release GCD objects
    dispatch_release(group_);
    dispatch_release(queue_);
}

void ThreadedObject::enqueue_(std::function<void()> fn, int) // NOLINT
{
    if (suppressEnqueue_.load(std::memory_order_acquire)) return;

    // note: timeout not implemented
    dispatch_group_async(group_, queue_, ^() {
        fn();
    });
}

void ThreadedObject::waitForAllTasksComplete_()
{
    suppressEnqueue_.store(true, std::memory_order_release);

    constexpr int MAX_TIME_TO_WAIT_NSEC = MS_TO_NSEC * 100;
    dispatch_group_wait(group_, dispatch_time(DISPATCH_TIME_NOW, MAX_TIME_TO_WAIT_NSEC));

    suppressEnqueue_.store(false, std::memory_order_release);
}
