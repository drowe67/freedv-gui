//=========================================================================
// Name:            PortAudioInterface.h
// Purpose:         Wrapper to enforce thread safety around PortAudio.
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

#ifndef PORT_AUDIO_INTERFACE_H
#define PORT_AUDIO_INTERFACE_H

#include <future>
#include "portaudio.h"
#include "../util/ThreadedObject.h"

class PortAudioInterface : public ThreadedObject
{
public:
    PortAudioInterface()
        : ThreadedObject("PortAudio") {}
    virtual ~PortAudioInterface()
    {
        waitForAllTasksComplete_();
    };
    
    std::future<PaError> Initialize();
    std::future<PaError> Terminate();
    std::future<const char*> GetErrorText(PaError error);
    std::future<const PaHostErrorInfo*> GetLastHostErrorInfo();
    std::future<PaDeviceIndex> GetDeviceCount();
    std::future<const PaDeviceInfo*> GetDeviceInfo(PaDeviceIndex device);
    std::future<const PaHostApiInfo*> GetHostApiInfo(PaHostApiIndex hostApi);
    std::future<PaError> IsFormatSupported(
        const PaStreamParameters* inputParameters,
        const PaStreamParameters* outputParameters,
        double sampleRate 
    );
    std::future<PaDeviceIndex> GetDefaultInputDevice(void); 
    std::future<PaDeviceIndex> GetDefaultOutputDevice(void);
    std::future<PaError> OpenStream(PaStream **stream, const PaStreamParameters *inputParameters, const PaStreamParameters *outputParameters, double sampleRate, unsigned long framesPerBuffer, PaStreamFlags streamFlags, PaStreamCallback *streamCallback, void *userData);
    std::future<PaError> StartStream(PaStream *stream); 
    std::future<PaError> StopStream(PaStream *stream);
    std::future<PaError> CloseStream(PaStream *stream);
    
    std::future<const PaStreamInfo*> GetStreamInfo(PaStream* stream);
};

#endif // PORT_AUDIO_INTERFACE_H
