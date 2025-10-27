//=========================================================================
// Name:            Semaphore.h
// Purpose:         Implements a semaphore.
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

#ifndef UTIL_SEMAPHORE_H
#define UTIL_SEMAPHORE_H

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#endif // defined(_WIN32) || defined(__APPLE__)

#include "sanitizers.h"

class Semaphore
{
public:
    Semaphore();
    virtual ~Semaphore();
    
    void wait();
    void signal() FREEDV_NONBLOCKING;
    
private:
#if defined(_WIN32)
    HANDLE sem_;
#elif defined(__APPLE__)
    dispatch_semaphore_t sem_;
#else
    sem_t sem_;
#endif // defined(_WIN32) || defined(__APPLE__)
};

#endif // UTIL_SEMAPHORE_H
