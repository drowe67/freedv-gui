//=========================================================================
// Name:            AudioDeviceSpecification.h
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

#ifndef AUDIO_DEVICE_SPECIFICATION_H
#define AUDIO_DEVICE_SPECIFICATION_H

#include <wx/string.h>
#include <vector>

struct AudioDeviceSpecification
{
    int deviceId;
    wxString name;     // Display/config name of device
    wxString cardName; // Name of the audio device
    int cardIndex;     // TBD - internal data for PulseAudio to look up cardName
    wxString portName; // Name of the port from the above audio device (e.g. "Speakers" on Windows). Optional.
    wxString apiName;  // Name of the active audio API
    int defaultSampleRate;
    int maxChannels;
    int minChannels;
    
    bool isValid() const;
    static AudioDeviceSpecification GetInvalidDevice();
};

#endif // AUDIO_DEVICE_SPECIFICATION_H
