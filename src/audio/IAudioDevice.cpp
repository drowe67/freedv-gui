//=========================================================================
// Name:            IAudioDevice.cpp
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

#include "IAudioDevice.h"

void IAudioDevice::setDescription(std::string desc)
{
    description = std::move(desc);
}

void IAudioDevice::setOnAudioData(AudioDataCallbackFn fn, void* state)
{
    onAudioDataFunction = fn;
    onAudioDataState = state;
}

void IAudioDevice::setOnAudioOverflow(AudioOverflowCallbackFn fn, void* state)
{
    onAudioOverflowFunction = fn;
    onAudioOverflowState = state;
}

void IAudioDevice::setOnAudioUnderflow(AudioUnderflowCallbackFn fn, void* state)
{
    onAudioUnderflowFunction = fn;
    onAudioUnderflowState = state;
}

void IAudioDevice::setOnAudioError(AudioErrorCallbackFn fn, void* state)
{
    onAudioErrorFunction = fn;
    onAudioErrorState = state;
}

void IAudioDevice::setOnAudioDeviceChanged(AudioDeviceChangedCallbackFn fn, void* state)
{
    onAudioDeviceChangedFunction = fn;
    onAudioDeviceChangedState = state;
}
