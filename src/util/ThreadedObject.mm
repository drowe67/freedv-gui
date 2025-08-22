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

ThreadedObject::ThreadedObject(ThreadedObject* parent)
    : parent_(parent)
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
}

ThreadedObject::~ThreadedObject()
{
    // empty
}

void ThreadedObject::enqueue_(std::function<void()> fn, int timeoutMilliseconds)
{
    // note: timeout not implemented
    dispatch_async(queue_, ^() {
        fn();
    });
}