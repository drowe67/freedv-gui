//=========================================================================
// Name:            IAudioEngine.cpp
// Purpose:         Defines the main interface to the selected audio engine.
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

#include "IAudioEngine.h"

int IAudioEngine::StandardSampleRates[] =
{
    16000,    22050,
    24000,    32000,
    44100,    48000,
    88200,    96000,
    176400,   192000,
    352800,   384000,
    -1          // negative terminated  list
};

void IAudioEngine::setOnEngineError(AudioErrorCallbackFn fn, void* state)
{
    onAudioErrorFunction = std::move(fn);
    onAudioErrorState = state;
}
