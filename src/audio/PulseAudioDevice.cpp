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
#include <chrono>
#include <sched.h>
#include <sys/resource.h>
#include <signal.h>

#include "PulseAudioDevice.h"

#if defined(USE_RTKIT)
#include "rtkit.h"
#endif // defined(USE_RTKIT)

#include "../util/logging/ulog.h"

using namespace std::chrono_literals;

// Target latency. This controls e.g. how long it takes for
// TX audio to reach the radio.
#define PULSE_TARGET_LATENCY_US 10000

thread_local std::chrono::high_resolution_clock::time_point PulseAudioDevice::StartTime_;
thread_local bool PulseAudioDevice::MustStopWork_ = false;

PulseAudioDevice::PulseAudioDevice(pa_threaded_mainloop *mainloop, pa_context* context, wxString devName, IAudioEngine::AudioDirection direction, int sampleRate, int numChannels)
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

bool PulseAudioDevice::isRunning()
{
    return stream_ != nullptr;
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
            onAudioErrorFunction(*this, std::string("Could not create PulseAudio stream for ") + (const char*)devName_.ToUTF8(), onAudioErrorState);
        }
        pa_threaded_mainloop_unlock(mainloop_);
        return;
    }
    
    pa_stream_set_underflow_callback(stream_, &PulseAudioDevice::StreamUnderflowCallback_, this);
    pa_stream_set_overflow_callback(stream_, &PulseAudioDevice::StreamOverflowCallback_, this);
    pa_stream_set_moved_callback(stream_, &PulseAudioDevice::StreamMovedCallback_, this);
    pa_stream_set_state_callback(stream_, &PulseAudioDevice::StreamStateCallback_, this);
#if 0
    pa_stream_set_latency_update_callback(stream_, &PulseAudioDevice::StreamLatencyCallback_, this);
#endif // 0

    // recommended settings, i.e. server uses sensible values
    pa_buffer_attr buffer_attr; 
    buffer_attr.tlength = pa_usec_to_bytes(PULSE_TARGET_LATENCY_US, &sample_specification);
    buffer_attr.maxlength = (uint32_t)-1;
    buffer_attr.prebuf = 0; // Ensure that we can recover during an underrun
    buffer_attr.minreq = (uint32_t) -1;
    buffer_attr.fragsize = buffer_attr.tlength;
    
    // Stream flags
    pa_stream_flags_t flags = pa_stream_flags_t(
         PA_STREAM_INTERPOLATE_TIMING |
         PA_STREAM_AUTO_TIMING_UPDATE | 
         PA_STREAM_ADJUST_LATENCY);
    
    int result = 0;
    if (direction_ == IAudioEngine::AUDIO_ENGINE_OUT)
    {
        log_info("Connecting to playback device %s", (const char*)devName_.ToUTF8());
        
        pa_stream_set_write_callback(stream_, &PulseAudioDevice::StreamWriteCallback_, this);
        result = pa_stream_connect_playback(
            stream_, devName_.c_str(), &buffer_attr, 
            flags, NULL, NULL);
        if (result == 0) pa_stream_trigger(stream_, nullptr, nullptr);
    }
    else
    {
        log_info("Connecting to record device %s", (const char*)devName_.ToUTF8());
        
        pa_stream_set_read_callback(stream_, &PulseAudioDevice::StreamReadCallback_, this);
        result = pa_stream_connect_record(
            stream_, devName_.c_str(), &buffer_attr, flags);
    }
    
    if (result != 0)
    {
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, std::string("Could not connect PulseAudio stream to ") + (const char*)devName_.ToUTF8(), onAudioErrorState);
        }
    }
    else
    {
        // Set up semaphore for signaling workers
        if (sem_init(&sem_, 0, 0) < 0)
        {
            log_warn("Could not set up semaphore (errno = %d)", errno);
        }
    }

    pa_threaded_mainloop_unlock(mainloop_);
}

void PulseAudioDevice::stop()
{
    if (stream_ != nullptr)
    {
        std::unique_lock<std::mutex> lk(streamStateMutex_);

        pa_threaded_mainloop_lock(mainloop_);
        pa_stream_disconnect(stream_);
        pa_threaded_mainloop_unlock(mainloop_);

        streamStateCondVar_.wait(lk);

        pa_stream_unref(stream_);

        stream_ = nullptr;
        sem_destroy(&sem_);
    }
}

int PulseAudioDevice::getLatencyInMicroseconds()
{
    pa_threaded_mainloop_lock(mainloop_);
    pa_usec_t latency = 0;
    if (stream_ != nullptr)
    {
        int neg = 0;
        pa_stream_get_latency(stream_, &latency, &neg); // ignore error and assume 0
    }
    pa_threaded_mainloop_unlock(mainloop_);
    return (int)latency;
}

void PulseAudioDevice::setHelperRealTime()
{
    // XXX: We can't currently enable RT scheduling on Linux
    // due to unreliable behavior surrounding how long it takes to
    // go through a single RX or TX cycle. This unreliability is 
    // likely due to the use of Python for some parts of RADE. Since
    // timing is so unreliable and due to the fact that Linux actually
    // kills processes that it deems as using "too much" CPU while in
    // real-time, it's better just to use normal scheduling for now.
#if 0
    // Set RLIMIT_RTTIME, required for rtkit
    struct rlimit rlim;
    memset(&rlim, 0, sizeof(rlim));
    rlim.rlim_cur = 10000ULL; // 10ms
    rlim.rlim_max = 200000ULL; // 200ms

    if ((setrlimit(RLIMIT_RTTIME, &rlim) < 0))
    {
        log_warn("Could not set RLIMIT_RTTIME (errno = %d)", errno);
    }

    // Prerequisite: SCHED_RR and SCHED_RESET_ON_FORK need to be set.
    struct sched_param p;
    memset(&p, 0, sizeof(p));

    p.sched_priority = 3;
    if (sched_setscheduler(0, SCHED_RR|SCHED_RESET_ON_FORK, &p) < 0 && errno == EPERM)
    {
#if defined(USE_RTKIT)
        DBusError error;
        DBusConnection* bus = nullptr;
        int result = 0;

        dbus_error_init(&error);
        if (!(bus = dbus_bus_get(DBUS_BUS_SYSTEM, &error)))
        {
            log_warn("Could not connect to system bus: %s", error.message);
        }
        else if ((result = rtkit_make_realtime(bus, 0, p.sched_priority)) < 0)
        {
            log_warn("rtkit could not make real-time: %s", strerror(-result));
        }

        if (bus != nullptr)
        {
            dbus_connection_unref(bus);
        }
#else
        log_warn("No permission to make real-time");
#endif // defined(USE_RTKIT)
    }

    // Set up signal handling for SIGXCPU
    struct sigaction action;
    action.sa_flags = SA_SIGINFO;     
    action.sa_sigaction = HandleXCPU_;
    sigaction(SIGXCPU, &action, NULL);

    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGXCPU);
    sigprocmask(SIG_UNBLOCK, &signal_set, NULL);
#endif // 0
}

void PulseAudioDevice::startRealTimeWork()
{
    StartTime_ = std::chrono::high_resolution_clock::now();

    sleepFallback_ = false;
    if (clock_gettime(CLOCK_REALTIME, &ts_) == -1)
    {
        sleepFallback_ = true;
    }
}
void PulseAudioDevice::stopRealTimeWork()
{
    if (sleepFallback_)
    {
        // Fallback to simple sleep.
        IAudioDevice::stopRealTimeWork();
        return;
    }

    auto latency = getLatencyInMicroseconds();
    if (latency == 0)
    {
        latency = PULSE_TARGET_LATENCY_US;
    }

    ts_.tv_nsec += latency * 1000;
    if (ts_.tv_nsec >= 1000000000)
    {
        ts_.tv_sec++;
        ts_.tv_nsec -= 1000000000;
    }

    if (sem_timedwait(&sem_, &ts_) < 0 && errno != ETIMEDOUT)
    {
        // Fallback to simple sleep.
        IAudioDevice::stopRealTimeWork();
    }
    else if (errno == ETIMEDOUT)
    {
        auto endTime = std::chrono::high_resolution_clock::now();
        if ((endTime - StartTime_) >= std::chrono::microseconds(PULSE_TARGET_LATENCY_US * 10))
        {
            // Took a lot longer than expected. Force a sleep so we don't get killed by rtkit.
            std::this_thread::sleep_for(std::chrono::microseconds(PULSE_TARGET_LATENCY_US));
        }
    }

    MustStopWork_ = false;
}

void PulseAudioDevice::clearHelperRealTime()
{
    IAudioDevice::clearHelperRealTime();
}

bool PulseAudioDevice::mustStopWork()
{
    return MustStopWork_;
}

void PulseAudioDevice::StreamReadCallback_(pa_stream *s, size_t length, void *userdata)
{
    const void* data = nullptr;
    PulseAudioDevice* thisObj = static_cast<PulseAudioDevice*>(userdata);

    do
    {
        pa_stream_peek(s, &data, &length);
        if (!data || length == 0) 
        {
            return; //break;
        }

        if (thisObj->onAudioDataFunction)
        {
            thisObj->onAudioDataFunction(*thisObj, const_cast<void*>(data), length / thisObj->getNumChannels() / sizeof(short), thisObj->onAudioDataState);
        }
        sem_post(&thisObj->sem_);
        pa_stream_drop(s);
    } while (pa_stream_readable_size(s) > 0);
}

void PulseAudioDevice::StreamWriteCallback_(pa_stream *s, size_t length, void *userdata)
{
    if (length > 0)
    {
        // Note that PulseAudio gives us lengths in terms of number of bytes, not samples.
        int numSamples = length / sizeof(short);
        short data[numSamples];
        memset(data, 0, sizeof(data));

        PulseAudioDevice* thisObj = static_cast<PulseAudioDevice*>(userdata);

        if (thisObj->onAudioDataFunction)
        {
            thisObj->onAudioDataFunction(*thisObj, data, numSamples / thisObj->getNumChannels(), thisObj->onAudioDataState);
        } 
        pa_stream_write(s, &data[0], length, NULL, 0LL, PA_SEEK_RELATIVE);
    }
}

void PulseAudioDevice::StreamStateCallback_(pa_stream *p, void *userdata)
{
    PulseAudioDevice* thisObj = static_cast<PulseAudioDevice*>(userdata);

    // This method is only used for termination to ensure that it's synchronous and 
    // does not accidentally refer to already freed memory.
    if (pa_stream_get_state(p) == PA_STREAM_TERMINATED)
    {
        std::unique_lock<std::mutex> lk(thisObj->streamStateMutex_);
        thisObj->streamStateCondVar_.notify_all();
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

    log_info("Stream named %s has been moved to %s", (const char*)thisObj->devName_.ToUTF8(), newDevName);
    
    thisObj->devName_ = newDevName;
    
    if (thisObj->onAudioDeviceChangedFunction) 
    {
        thisObj->onAudioDeviceChangedFunction(*thisObj, (const char*)thisObj->devName_.ToUTF8(), thisObj->onAudioDeviceChangedState);
    }
}

#if 0
void PulseAudioDevice::StreamLatencyCallback_(pa_stream *p, void *userdata)
{
    PulseAudioDevice* thisObj = static_cast<PulseAudioDevice*>(userdata);
    pa_usec_t latency;
    int isNeg;

    pa_stream_get_latency(p, &latency, &isNeg);

    fprintf(stderr, "Current target buffer size for %s: %d\n", (const char*)thisObj->devName_.ToUTF8(), thisObj->targetOutputPendingLength_);
}
#endif // 0

void PulseAudioDevice::HandleXCPU_(int signum, siginfo_t *info, void *extra)
{
    // Notify thread that it has to stop work immediately and sleep.
    log_warn("Taking too much CPU handling real-time tasks, pausing for a bit");
    MustStopWork_ = true;
}
