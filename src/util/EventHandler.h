//=========================================================================
// Name:            EventHandler.h
// Purpose:         List of functions that handle an event
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

#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <functional>
#include <vector>
#include <utility>

template<typename... FnArgs>
class EventHandler
{
public:
    using FnType = std::function<void(FnArgs...)>;
    
    EventHandler() = default;
    virtual ~EventHandler() = default;
    
    void operator() (FnArgs... args);
    EventHandler<FnArgs...>& operator+=(FnType const& fn);
    void clear();
    
private:
    std::vector<FnType> fnList_;
};

template<typename... FnArgs>
void EventHandler<FnArgs...>::operator() (FnArgs... args)
{
    for (auto& fn : fnList_)
    {
        fn(std::forward<FnArgs>(args)...);
    }
}

template<typename... FnArgs>
EventHandler<FnArgs...>& EventHandler<FnArgs...>::operator+=(FnType const& fn)
{
    fnList_.push_back(fn);
    return *this;
}

template<typename... FnArgs>
void EventHandler<FnArgs...>::clear()
{
    fnList_.clear();
}

#endif // EVENT_HANDLER_H
