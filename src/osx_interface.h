//==========================================================================
// Name:            osx_interface.h
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
#ifndef __OSX_INTERFACE__
#define __OSX_INTERFACE__

// Checks whether FreeDV has permissions to access the microphone on OSX Catalina
// and above. If the user doesn't grant permissions (returns FALSE), the GUI 
// should be able to properly handle the situation.
#ifdef __APPLE__
extern "C" bool VerifyMicrophonePermissions();
#else
// Stub for non-Apple platforms
extern "C" bool VerifyMicrophonePermissions() { return true; }
#endif // __APPLE__

#endif // __OSX_INTERFACE__
