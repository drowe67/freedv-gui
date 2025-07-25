//=========================================================================
// Name:            ThreadedObject.h
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

#ifndef THREADED_OBJECT_H
#define THREADED_OBJECT_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>

class ThreadedObject
{
public:
    virtual ~ThreadedObject();

protected:
    ThreadedObject();
    
    // Enqueues some code to run on a different thread.
    // @param timeoutMilliseconds Timeout to wait for lock. Note: if we can't get a lock within the timeout, the function doesn't run!
    void enqueue_(std::function<void()> fn, int timeoutMilliseconds = 0);
    
private:
    bool isDestroying_;
    std::thread objectThread_;
    std::deque<std::function<void()> > eventQueue_;
    std::recursive_mutex eventQueueMutex_;
    std::condition_variable_any eventQueueCV_;

    void eventLoop_();
};

#endif // THREADED_OBJECT_H
