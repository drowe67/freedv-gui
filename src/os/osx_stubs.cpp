//==========================================================================
// Name:            osx_stubs.cpp
//
// Purpose:         Provides stubs for unimplemented Objective-C calls.
// Created:         October 11, 2023
// Authors:         Mooneer Salem
// 
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================

#include "os_interface.h"

void VerifyMicrophonePermissions(std::promise<bool>& micPromise)
{
    micPromise.set_value(true);
}

void ResetMainWindowColorSpace()
{
    // empty
}

void StartLowLatencyActivity()
{
    // empty
}

void StopLowLatencyActivity()
{
    // empty
}

std::string GetOperatingSystemString()
{
#ifdef __linux__
    return "linux";
#else
    return "other";
#endif // __linux__
}
