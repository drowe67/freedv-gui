//=========================================================================
// Name:            Win32COMObject.cpp
// Purpose:         Wrapper to ThreadedObject to guarantee COM initialization
//                  and teardown.
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

#include "Win32COMObject.h"
#include "logging/ulog.h"
#include <windows.h>

#include <future>

Win32COMObject::Win32COMObject(std::string name)
    : ThreadedObject(name)
{
    enqueue_([&]() {
        HRESULT res = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(res))
        {
            log_warn("Could not initialize COM (res = %d)", res);
        }
    });
}

Win32COMObject::~Win32COMObject()
{
    std::shared_ptr<std::promise<void> > prom = std::make_shared<std::promise<void> >();
    auto fut = prom->get_future();
    enqueue_([&]() {
        CoUninitialize();
        prom->set_value();
    });
    fut.wait();
}
