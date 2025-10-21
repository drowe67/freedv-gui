//=========================================================================
// Name:            MinimalRealTimeHelper.cpp
// Purpose:         Realtime helper for Flex waveform.
//
// Authors:         Mooneer Salem
// License:
//
//  All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANYg_eoo_enqueued
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//=========================================================================

#include "MinimalRealTimeHelper.h"

#if defined(USE_RTKIT)
#include "../audio/rtkit.h"
#endif // defined(USE_RTKIT)

#include <string.h>
#include "../util/logging/ulog.h"

void MinimalRealtimeHelper::setHelperRealTime()
{
#if defined(USE_RTKIT)
    DBusError error;
    DBusConnection* bus = nullptr;
    int result = 0;

    dbus_error_init(&error);
    if (!(bus = dbus_bus_get(DBUS_BUS_SYSTEM, &error)))
    {
        log_warn("Could not connect to system bus: %s", error.message);
    }
    else
    {
        int minNiceLevel = 0;
        if ((result = rtkit_get_min_nice_level(bus, &minNiceLevel)) < 0)
        {
            log_warn("rtkit could not get minimum nice level: %s", strerror(-result));
        }
        else if ((result = rtkit_make_high_priority(bus, 0, minNiceLevel)) < 0)
        {
            log_warn("rtkit could not make high priority: %s", strerror(-result));
        }
    }

    if (bus != nullptr)
    {
        dbus_connection_unref(bus);
    }
#endif // defined(USE_RTKIT)
}
