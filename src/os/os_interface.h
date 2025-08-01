//==========================================================================
// Name:            os_interface.h
//
// Purpose:         Provides C wrapper to needed Objective-C calls.
// Created:         Nov. 23, 2019
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
#ifndef __OS_INTERFACE__
#define __OS_INTERFACE__

#include <future>
#include <string>

// Checks whether FreeDV has permissions to access the microphone on OSX Catalina
// and above. If the user doesn't grant permissions (returns FALSE), the GUI 
// should be able to properly handle the situation.
void VerifyMicrophonePermissions(std::promise<bool>& promise);

// macOS does color space conversions in the background when rendering PlotWaterfall.
// This causes FreeDV to use ~30-50% more CPU that it otherwise would (despite wxWidgets
// using sRGB internally). Because of this, we reset the main window's color space to 
// sRGB ourselves on every frame render.
//
// See https://github.com/OpenTTD/OpenTTD/issues/7644 and https://trac.wxwidgets.org/ticket/18516
// for more details.
extern "C" void ResetMainWindowColorSpace();

// Retrieves a string representing the operating system that FreeDV is running on.
// This can be either "windows", "linux", "macos" or "other".
std::string GetOperatingSystemString();

// Tells the operating system that we need the lowest latency available.
// Note: only implemented on macOS.
void StartLowLatencyActivity();

// Tells the operating system that we no longer need low latency.
void StopLowLatencyActivity();

#endif // __OS_INTERFACE__
