//=========================================================================
// Name:            MinimalRealTimeHelper.cpp
// Purpose:         Realtime helper for Flex waveform.
//
// Authors:         Mooneer Salem
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
        constexpr int ERROR_BUFFER_SIZE = 1024;
        char tmpBuf[ERROR_BUFFER_SIZE];
        if ((result = rtkit_get_min_nice_level(bus, &minNiceLevel)) < 0)
        {
            log_warn("rtkit could not get minimum nice level: %s", strerror_r(-result, tmpBuf, ERROR_BUFFER_SIZE));
        }
        else if ((result = rtkit_make_high_priority(bus, 0, minNiceLevel)) < 0)
        {
            log_warn("rtkit could not make high priority: %s", strerror_r(-result, tmpBuf, ERROR_BUFFER_SIZE));
        }
    }

    if (bus != nullptr)
    {
        dbus_connection_unref(bus);
    }
#endif // defined(USE_RTKIT)
}
