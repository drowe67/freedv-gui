//=========================================================================
// Name:            sanitizers.h
// Purpose:         Defines items needed to enable LLVM sanitizer execution.
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

#ifndef SANITIZERS_H
#define SANITIZERS_H

#if defined(__clang__)
#if defined(__has_feature) && __has_feature(realtime_sanitizer)
#define RTSAN_IS_ENABLED
#else
#define FREEDV_NONBLOCKING noexcept
#endif // defined(__has_feature) && __has_feature(realtime_sanitizer)
#else
#define FREEDV_NONBLOCKING noexcept
#endif // defined(__clang__)

#if defined(RTSAN_IS_ENABLED)
#include <sanitizer/rtsan_interface.h>
#define FREEDV_NONBLOCKING_EXCEPT [[clang::nonblocking]]
#define FREEDV_NONBLOCKING noexcept [[clang::nonblocking]]

// *_VERIFIED_SAFE are intended for use where the code was manually verified
// to be RT-safe (i.e. for third party libraries). rtsan run-time checks are
// still enabled in case this ever changes in a future release of rtsan and/or
// the library in question.
#define FREEDV_BEGIN_VERIFIED_SAFE                                       \
        _Pragma("clang diagnostic push")                                 \
        _Pragma("clang diagnostic ignored \"-Wunknown-warning-option\"") \
        _Pragma("clang diagnostic ignored \"-Wfunction-effects\"")       
#define FREEDV_END_VERIFIED_SAFE                                         \
        _Pragma("clang diagnostic pop")                                  

#define FREEDV_BEGIN_REALTIME_UNSAFE                                     \
    {                                                                    \
        FREEDV_BEGIN_VERIFIED_SAFE                                       \
        __rtsan_disable();
#define FREEDV_END_REALTIME_UNSAFE                                       \
        __rtsan_enable();                                                \
        FREEDV_END_VERIFIED_SAFE                                         \
    }

#else
#define FREEDV_NONBLOCKING_EXCEPT
#define FREEDV_NONBLOCKING noexcept
#define FREEDV_BEGIN_VERIFIED_SAFE
#define FREEDV_END_VERIFIED_SAFE
#define FREEDV_BEGIN_REALTIME_UNSAFE {
#define FREEDV_END_REALTIME_UNSAFE }
#endif // defined(RTSAN_IS_ENABLED)

#endif // SANITIZERS_H
