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
}

void ResetMainWindowColorSpace()
{
    // empty
}

std::string GetOperatingSystemString()
{
    return "windows";
}