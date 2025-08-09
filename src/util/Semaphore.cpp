//=========================================================================
// Name:            Semaphore.cpp
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

#include <sstream>

#include "Semaphore.h"
#include "logging/ulog.h"

Semaphore::Semaphore()
{
#if defined(_WIN32)
    sem_ = CreateSemaphore(nullptr, 0, 1, nullptr);
    if (sem_ == nullptr)
    {
        std::stringstream ss;
        ss << "Could not create semaphore (err = " << GetLastError() << ")";
        log_error(ss.str().c_str());
    }
#elif defined(__APPLE__)
    sem_ = dispatch_semaphore_create(0);
    if (sem_ == nullptr)
    {
        log_error("Could not set up semaphore");
    }
#else
    if (sem_init(&sem_, 0, 0) < 0)
    {
        log_error("Could not set up semaphore (errno = %d)", errno);
    }
#endif // defined(_WIN32) || defined(__APPLE__)
}

Semaphore::~Semaphore()
{
#if defined(_WIN32)
    if (sem_ != nullptr)
    {
        auto tmpSem = sem_;
        sem_ = nullptr;
        ReleaseSemaphore(tmpSem, 1, nullptr);
        CloseHandle(tmpSem);
    }
#elif defined(__APPLE__)
    if (sem_ != nullptr)
    {
        dispatch_semaphore_signal(sem_);
        dispatch_release(sem_);
    }
#else
    sem_post(&sem_);
    sem_destroy(&sem_);
#endif // defined(_WIN32) || defined(__APPLE__)
}

void Semaphore::signal()
{
#if defined(_WIN32)
    if (sem_ != nullptr)
    {
        ReleaseSemaphore(sem_, 1, nullptr);
    }
#elif defined(__APPLE__)
    if (sem_ != nullptr)
    {
        dispatch_semaphore_signal(sem_);
    }
#else
    sem_post(&sem_);
#endif // defined(_WIN32) || defined(__APPLE__)
}

void Semaphore::wait()
{
#if defined(_WIN32)
    DWORD result = WaitForSingleObject(sem_, INFINITE);
    if (result != WAIT_TIMEOUT && result != WAIT_OBJECT_0)
    {
        std::stringstream ss;
        ss << "Could not wait on semaphore (err = " << GetLastError() << ")";
        log_error(ss.str().c_str());
    }
#elif defined(__APPLE__)
    if (sem_ != nullptr)
    {
        dispatch_semaphore_wait(sem_, DISPATCH_TIME_FOREVER);
    }
#else
    if (sem_wait(&sem_) < 0)
    {
        log_error("Could not wait on semaphore (errno = %d)", errno);
    }
#endif // defined(_WIN32) || defined(__APPLE__)
}