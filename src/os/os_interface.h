//==========================================================================
// Name:            os_interface.h
//
// Purpose:         Provides C wrapper to needed Objective-C calls.
// Created:         Nov. 23, 2019
// Authors:         Mooneer Salem
// 
// License:
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

// Sets the name of the current thread in the OS.
void SetThreadName(std::string const& name);

#endif // __OS_INTERFACE__
