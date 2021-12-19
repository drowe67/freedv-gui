//=========================================================================
// Name:            IAudioDevice.h
// Purpose:         Defines the interface to an audio device.
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

#ifndef I_AUDIO_DEVICE_H
#define I_AUDIO_DEVICE_H

#include <string>
#include <vector>
#include "AudioDeviceSpecification.h"

class IAudioDevice
{
public:
    virtual void start() = 0;
    virtual void stop() = 0;
};

#endif // AUDIO_ENGINE_H