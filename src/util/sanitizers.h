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
#define FREEDV_NONBLOCKING noexcept [[clang::nonblocking]]
#define FREEDV_BEGIN_REALTIME_UNSAFE                                     \
    {                                                                    \
        _Pragma("clang diagnostic push")                                 \
        _Pragma("clang diagnostic ignored \"-Wunknown-warning-option\"") \
        _Pragma("clang diagnostic ignored \"-Wfunction-effects\"")       \
        __rtsan_disable();
#define FREEDV_END_REALTIME_UNSAFE                                       \
        __rtsan_enable();                                                \
        _Pragma("clang diagnostic pop")                                  \
    }

#else
#define FREEDV_NONBLOCKING noexcept
#define FREEDV_BEGIN_REALTIME_UNSAFE {
#define FREEDV_END_REALTIME_UNSAFE }
#endif // defined(RTSAN_IS_ENABLED)

#endif // SANITIZERS_H
