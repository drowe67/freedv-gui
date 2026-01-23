//==========================================================================
// Name:            windows_interface.cpp
//
// Purpose:         Implements Windowes specific logic for OS functions.
// Created:         December 31, 2024
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

#include <wx/wx.h>
#include <wx/msw/registry.h>

void VerifyMicrophonePermissions(std::promise<bool>& micPromise)
{
#if 0
    bool microphonePermissionsGranted = false;
    
    // General Microphone enable/disable (applies to all users)
    wxRegKey localMachineMicrophoneKey(wxRegKey::HKLM, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\microphone");

    bool localMachineMicrophoneAllowed = false;
    if (localMachineMicrophoneKey.Exists())
    {
        wxString regValue;
        localMachineMicrophoneKey.QueryValue("Value", regValue, true);
        localMachineMicrophoneAllowed = regValue == "Allow";
    }
    
    microphonePermissionsGranted = localMachineMicrophoneAllowed;
    micPromise.set_value(microphonePermissionsGranted);
#else
    // Bypass all microphone checks for now. On some systems, the above registry key isn't correct and I'm
    // not sure why (or what, if any key, should be used instead). This check will either need to be fixed 
    // for a future release or fully eliminated.
    micPromise.set_value(true);
#endif // 0
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
    return "windows";
}

void SetThreadName(std::string const& name)
{
    // XXX - assumes ASCII. This is probably fine, though, since
    // this is debug code.
    std::string fullName = "FDV ";
    fullName += name;
    std::wstring stemp = std::wstring(fullName.begin(), fullName.end());
    SetThreadDescription(GetCurrentThread(), stemp.c_str());
}
