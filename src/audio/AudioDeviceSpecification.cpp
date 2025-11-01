//=========================================================================
// Name:            AudioDeviceSpecification.cpp
// Purpose:         Describes an audio device on the user's system.
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

#include "AudioDeviceSpecification.h"

bool AudioDeviceSpecification::isValid() const
{
    return deviceId != -1;
}

AudioDeviceSpecification AudioDeviceSpecification::GetInvalidDevice()
{
    AudioDeviceSpecification result = {
        .deviceId = -1,
        .name = "",
        .cardName = "",
        .cardIndex = -1,
        .portName = "",
        .apiName = "",
        .defaultSampleRate = -1,
        .maxChannels = -1,
        .minChannels = -1
    };
    
    return result;
}
