//=========================================================================
// Name:            realtime_fp.h
// Purpose:         RT-safe equivalent to std::function.
//                  Note: adapted from Clang function effects analysis docs.
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

#ifndef REALTIME_FP_H
#define REALTIME_FP_H

#include <utility>
#include "sanitizers.h"

template <typename>
class realtime_fp;

template <typename R, typename... Args>
class realtime_fp<R(Args...)> {
public:
  using impl_t = R (*)(Args...) FREEDV_NONBLOCKING_EXCEPT;

  realtime_fp() = default;
  realtime_fp(impl_t f) : mImpl{ f } {}
  virtual ~realtime_fp() = default;

  virtual R operator()(Args... args) const FREEDV_NONBLOCKING
  {
    return mImpl(std::forward<Args>(args)...);
  }

protected:
  impl_t mImpl{ nullptr };
};

// deduction guide (like std::function's)
template< class R, class... ArgTypes >
realtime_fp( R(*)(ArgTypes...) ) -> realtime_fp<R(ArgTypes...)>;

#endif // REALTIME_FP_H
