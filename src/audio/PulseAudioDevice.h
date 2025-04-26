//=========================================================================
// Name:            PulseAudioDevice.h
// Purpose:         Defines the interface to a PulseAudio device.
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

#ifndef PULSE_AUDIO_DEVICE_H
#define PULSE_AUDIO_DEVICE_H

#include <mutex>
#include <thread>
#include <condition_variable>
#include <wx/string.h>
#include <pulse/pulseaudio.h>
#include <semaphore.h>

#include "IAudioEngine.h"
#include "IAudioDevice.h"

class PulseAudioDevice : public IAudioDevice
{
public:
    virtual ~PulseAudioDevice();
    
    virtual int getNumChannels() override { return numChannels_; }
    virtual int getSampleRate() const override { return sampleRate_; }
    
    virtual void start() override;
    virtual void stop() override;

    virtual bool isRunning() override;
    
    virtual int getLatencyInMicroseconds() override;

    // Configures current thread for real-time priority. This should be
    // called from the thread that will be operating on received audio.
    virtual void setHelperRealTime() override;

    // Lets audio system know that we're starting work on received audio.
    virtual void startRealTimeWork() override;

    // Lets audio system know that we're done with the work on the received
    // audio.
    virtual void stopRealTimeWork() override;

    // Reverts real-time priority for current thread.
    virtual void clearHelperRealTime() override;

protected:
    // PulseAudioDevice cannot be created directly, only via PulseAudioEngine.
    friend class PulseAudioEngine;
    
    PulseAudioDevice(pa_threaded_mainloop *mainloop, pa_context* context, wxString devName, IAudioEngine::AudioDirection direction, int sampleRate, int numChannels);
    
private:
    pa_context* context_;
    pa_threaded_mainloop* mainloop_;
    pa_stream* stream_;

    wxString devName_;
    IAudioEngine::AudioDirection direction_;
    int sampleRate_;
    int numChannels_;
    std::mutex streamStateMutex_;
    std::condition_variable streamStateCondVar_;

    sem_t sem_;
    struct timespec ts_;
    bool sleepFallback_;

    static void StreamReadCallback_(pa_stream *s, size_t length, void *userdata);
    static void StreamWriteCallback_(pa_stream *s, size_t length, void *userdata);
    static void StreamUnderflowCallback_(pa_stream *p, void *userdata);
    static void StreamOverflowCallback_(pa_stream *p, void *userdata);
    static void StreamMovedCallback_(pa_stream *p, void *userdata);
    static void StreamStateCallback_(pa_stream *p, void *userdata);
#if 0
    static void StreamLatencyCallback_(pa_stream *p, void *userdata);
#endif // 0
};

#endif // PULSE_AUDIO_DEVICE_H
