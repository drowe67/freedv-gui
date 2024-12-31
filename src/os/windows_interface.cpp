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
    
    wxRegKey currentUserMicrophoneKey(wxRegKey::HKCU, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\microphone");
    wxRegKey currentUserNonPackagedKey(wxRegKey::HKCU, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\microphone\\NonPackaged");
    
    bool currentUserMicrophoneAllowed = false;
    if (currentUserMicrophoneKey.Exists())
    {
        wxString regValue;
        currentUserMicrophoneKey.QueryValue("Value", regValue, true);
        currentUserMicrophoneAllowed = regValue == "Allow";
    }
    
    bool currentUserNonPackagedAllowed = false;
    if (currentUserNonPackagedKey.Exists())
    {
        wxString regValue;
        currentUserNonPackagedKey.QueryValue("Value", regValue, true);
        currentUserNonPackagedAllowed = regValue == "Allow";
    }
    
    microphonePermissionsGranted = currentUserMicrophoneAllowed && currentUserNonPackagedAllowed;
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