//=========================================================================
// Name:            PulseAudioDevice.cpp
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

#include <cstring>
#include <cstdio>

#include "PulseAudioDevice.h"

PulseAudioDevice::PulseAudioDevice(pa_threaded_mainloop *mainloop, pa_context* context, std::string devName, IAudioEngine::AudioDirection direction, int sampleRate, int numChannels)
    : context_(context)
    , mainloop_(mainloop)
    , stream_(nullptr)
    , devName_(devName)
    , direction_(direction)
    , sampleRate_(sampleRate)
    , numChannels_(numChannels)
{
    // Set default description
    setDescription("PulseAudio Device");
}

PulseAudioDevice::~PulseAudioDevice()
{
    if (stream_ != nullptr)
    {
        stop();
    }
}

void PulseAudioDevice::start()
{
    pa_sample_spec sample_specification;
    sample_specification.format = PA_SAMPLE_S16LE;
    sample_specification.rate = sampleRate_;
    sample_specification.channels = numChannels_;
    
    pa_threaded_mainloop_lock(mainloop_);
    stream_ = pa_stream_new(context_, description.c_str(), &sample_specification, nullptr);
    if (stream_ == nullptr)
    {
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, std::string("Could not create PulseAudio stream for ") + devName_, onAudioErrorState);
        }
        pa_threaded_mainloop_unlock(mainloop_);
        return;
    }
    
    pa_stream_set_underflow_callback(stream_, &PulseAudioDevice::StreamUnderflowCallback_, this);
    pa_stream_set_overflow_callback(stream_, &PulseAudioDevice::StreamOverflowCallback_, this);
    pa_stream_set_moved_callback(stream_, &PulseAudioDevice::StreamMovedCallback_, this);
    
    // recommended settings, i.e. server uses sensible values
    pa_buffer_attr buffer_attr; 
    buffer_attr.maxlength = (uint32_t)-1;
    buffer_attr.tlength = pa_usec_to_bytes(20000, &sample_specification);
    buffer_attr.prebuf = 0; // Ensure that we can recover during an underrun
    buffer_attr.minreq = (uint32_t) -1;
    buffer_attr.fragsize = buffer_attr.tlength;
    
    // Stream flags
    pa_stream_flags_t flags = pa_stream_flags_t(
         PA_STREAM_INTERPOLATE_TIMING |
         PA_STREAM_AUTO_TIMING_UPDATE | 
         PA_STREAM_FAIL_ON_SUSPEND |
         PA_STREAM_ADJUST_LATENCY);
    
    int result = 0;
    if (direction_ == IAudioEngine::AUDIO_ENGINE_OUT)
    {
        pa_stream_set_write_callback(stream_, &PulseAudioDevice::StreamWriteCallback_, this);
        result = pa_stream_connect_playback(
            stream_, devName_.c_str(), &buffer_attr, 
            flags, NULL, NULL);
    }
    else
    {
        pa_stream_set_read_callback(stream_, &PulseAudioDevice::StreamReadCallback_, this);
        result = pa_stream_connect_record(
            stream_, devName_.c_str(), &buffer_attr, flags);
    }
    
    if (result != 0)
    {
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, std::string("Could not connect PulseAudio stream to ") + devName_, onAudioErrorState);
        }
    }
    
    pa_threaded_mainloop_unlock(mainloop_);
}

void PulseAudioDevice::stop()
{
    if (stream_ != nullptr)
    {
        pa_threaded_mainloop_lock(mainloop_);
        pa_stream_disconnect(stream_);
        pa_threaded_mainloop_unlock(mainloop_);
        pa_stream_unref(stream_);
        
        stream_ = nullptr;
    }
}

void PulseAudioDevice::StreamReadCallback_(pa_stream *s, size_t length, void *userdata)
{
    const void* data = nullptr;
    PulseAudioDevice* thisObj = static_cast<PulseAudioDevice*>(userdata);
    void* fullBlock = nullptr;
    size_t fullSize = 0;

    do
    {
        pa_stream_peek(s, &data, &length);
        if (!data || length == 0) 
        {
            break;
        }

        fullSize += length;
        fullBlock = realloc(fullBlock, fullSize);
        assert(fullBlock != nullptr);

        memcpy(fullBlock + fullSize - length, data, length);
        
        pa_stream_drop(s);
    } while (pa_stream_readable_size(s) > 0);

    if (thisObj->onAudioDataFunction)
    {
        thisObj->onAudioDataFunction(*thisObj, fullBlock, fullSize / (sizeof(short) * thisObj->getNumChannels()), thisObj->onAudioDataState);
    }

    free(fullBlock);
}

void PulseAudioDevice::StreamWriteCallback_(pa_stream *s, size_t length, void *userdata)
{
    if (length > 0)
    {
        short data[length];
        memset(data, 0, sizeof(short) * length);

        PulseAudioDevice* thisObj = static_cast<PulseAudioDevice*>(userdata);
    
        if (thisObj->onAudioDataFunction)
        {
            thisObj->onAudioDataFunction(*thisObj, data, length / (sizeof(short) * thisObj->getNumChannels()), thisObj->onAudioDataState);
        }
    
        pa_stream_write(s, &data[0], length, NULL, 0LL, PA_SEEK_RELATIVE);
    }
}

void PulseAudioDevice::StreamUnderflowCallback_(pa_stream *p, void *userdata)
{
    PulseAudioDevice* thisObj = static_cast<PulseAudioDevice*>(userdata);
    
    if (thisObj->onAudioUnderflowFunction) 
    {
        thisObj->onAudioUnderflowFunction(*thisObj, thisObj->onAudioUnderflowState);
    }
}

void PulseAudioDevice::StreamOverflowCallback_(pa_stream *p, void *userdata)
{
    PulseAudioDevice* thisObj = static_cast<PulseAudioDevice*>(userdata);
    
    if (thisObj->onAudioOverflowFunction) 
    {
        thisObj->onAudioOverflowFunction(*thisObj, thisObj->onAudioOverflowState);
    }
}

void PulseAudioDevice::StreamMovedCallback_(pa_stream *p, void *userdata)
{
    auto newDevName = pa_stream_get_device_name(p);
    PulseAudioDevice* thisObj = static_cast<PulseAudioDevice*>(userdata);

    fprintf(stderr, "%s is being renamed to %s\n", thisObj->devName_.c_str(), newDevName);
    thisObj->devName_ = newDevName;
    
    if (thisObj->onAudioDeviceChangedFunction) 
    {
        thisObj->onAudioDeviceChangedFunction(*thisObj, thisObj->devName_, thisObj->onAudioOverflowState);
    }
}
