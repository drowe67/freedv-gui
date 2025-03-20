//=========================================================================
// Name:            AudioEngineFactory.cpp
// Purpose:         Creates AudioEngines for the current platform.
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

#include "AudioEngineFactory.h"
#if defined(AUDIO_ENGINE_PULSEAUDIO_ENABLE)
#include "PulseAudioEngine.h"
#elif __APPLE__
#include "MacAudioEngine.h"
#else
#include "PortAudioEngine.h"
#endif // defined(AUDIO_ENGINE_PULSEAUDIO_ENABLE)

std::shared_ptr<IAudioEngine> AudioEngineFactory::SystemEngine_;

std::shared_ptr<IAudioEngine> AudioEngineFactory::GetAudioEngine()
{
    if (!SystemEngine_)
    {
#if defined(AUDIO_ENGINE_PULSEAUDIO_ENABLE)
        SystemEngine_ = std::shared_ptr<IAudioEngine>(new PulseAudioEngine());
#elif __APPLE__
        SystemEngine_ = std::shared_ptr<IAudioEngine>(new MacAudioEngine());
#else
        SystemEngine_ = std::shared_ptr<IAudioEngine>(new PortAudioEngine());
#endif // defined(AUDIO_ENGINE_PULSEAUDIO_ENABLE)
    }
    
    return SystemEngine_;
}
