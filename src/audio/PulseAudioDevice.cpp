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

#include "PulseAudioDevice.h"

#if defined(USE_RTKIT)
#include "rtkit.h"
#endif // defined(USE_RTKIT)

#include "../util/logging/ulog.h"

using namespace std::chrono_literals;

// Optimal settings based on ones used for PortAudio.
#define PULSE_FPB 256
#define PULSE_TARGET_LATENCY_US 20000

PulseAudioDevice::PulseAudioDevice(pa_threaded_mainloop *mainloop, pa_context* context, wxString devName, IAudioEngine::AudioDirection direction, int sampleRate, int numChannels)
    : context_(context)
    , mainloop_(mainloop)
    , stream_(nullptr)
    , outputPending_(nullptr)
    , outputPendingLength_(0)
    , outputPendingThreadActive_(false)
    , outputPendingThread_(nullptr)
    , targetOutputPendingLength_(PULSE_FPB * numChannels * 2)
    , inputPending_(nullptr)
    , inputPendingLength_(0)
    , inputPendingThreadActive_(false)
    , inputPendingThread_(nullptr)
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
    buffer_attr.maxlength = (uint32_t)-1;
    buffer_attr.tlength = pa_usec_to_bytes(PULSE_TARGET_LATENCY_US, &sample_specification);
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

        // Start data collection thread. This thread
        // is necessary in order to ensure that we can 
        // provide data to PulseAudio at a rate expected
        // for the actual latency of the sound device.
        outputPending_ = nullptr;
        outputPendingLength_ = 0;
        targetOutputPendingLength_ = PULSE_FPB * getNumChannels() * 2;
        outputPendingThreadActive_ = true;
        inputPending_ = nullptr;
        inputPendingLength_ = 0;
        if (direction_ == IAudioEngine::AUDIO_ENGINE_IN)
        {
            inputPendingThreadActive_ = true;
            inputPendingThread_ = new std::thread([&]() {
#if defined(__linux__)
                pthread_setname_np(pthread_self(), "FreeDV PAIn");
#endif // defined(__linux__)
         
                while(inputPendingThreadActive_)
                {
                    auto currentTime = std::chrono::steady_clock::now();
                    int currentLength = 0;

                    {
                        std::unique_lock<std::mutex> lk(inputPendingMutex_);
                        currentLength = inputPendingLength_;
                    }

                    currentLength = std::min(currentLength, PULSE_FPB * getNumChannels());
                    if (currentLength > 0)
                    {
                        short data[currentLength];
                        {
                            std::unique_lock<std::mutex> lk(inputPendingMutex_);
                            memcpy(data, inputPending_, currentLength * sizeof(short));

                            short* newInputPending = nullptr;
                            if (inputPendingLength_ > currentLength)
                            {
                                newInputPending = new short[inputPendingLength_ - currentLength];
                                assert(newInputPending != nullptr);
                                memcpy(newInputPending, inputPending_ + currentLength, (inputPendingLength_ - currentLength) * sizeof(short));
                            }
                            delete[] inputPending_;
                            inputPending_ = newInputPending;
                            inputPendingLength_ = inputPendingLength_ - currentLength;
                        }

                        if (onAudioDataFunction)
                        {
                            onAudioDataFunction(*this, data, currentLength / getNumChannels(), onAudioDataState);
                        }
			sem_post(&sem_);

                        // Sleep up to the number of milliseconds corresponding to the data received
                        int numMilliseconds = 1000.0 * ((double)currentLength / getNumChannels()) / (double)getSampleRate();
                        std::this_thread::sleep_until(currentTime + std::chrono::milliseconds(numMilliseconds));
                    }
                    else
                    {
                        // Sleep up to 20ms by default if there's no data available.
                        std::this_thread::sleep_until(currentTime + 20ms);
                    }
                }
            });
            assert(inputPendingThread_ != nullptr);
        }
#if 0
        else if (direction_ == IAudioEngine::AUDIO_ENGINE_OUT)
        {
            outputPendingThread_ = new std::thread([&]() {
#if defined(__linux__)
                pthread_setname_np(pthread_self(), "FreeDV PAOut");
#endif // defined(__linux__)

                while(outputPendingThreadActive_)
                {
                    auto currentTime = std::chrono::steady_clock::now();
                    int currentLength = 0;

                    {
                        std::unique_lock<std::mutex> lk(outputPendingMutex_);
                        currentLength = outputPendingLength_;
                    }

                    if (currentLength < targetOutputPendingLength_)
                    {                        
                        short data[PULSE_FPB * getNumChannels()];
                        memset(data, 0, sizeof(data));
                        
                        if (onAudioDataFunction)
                        {
                            onAudioDataFunction(*this, data, PULSE_FPB, onAudioDataState);
                        }
    
                        {
                            std::unique_lock<std::mutex> lk(outputPendingMutex_);
                            short* temp = new short[outputPendingLength_ + PULSE_FPB * getNumChannels()];
                            assert(temp != nullptr);
    
                            if (outputPendingLength_ > 0)
                            {
                                memcpy(temp, outputPending_, outputPendingLength_ * sizeof(short));
    
                                delete[] outputPending_;
                                outputPending_ = nullptr;
                            }
                            memcpy(temp + outputPendingLength_, data, sizeof(data));
    
                            outputPending_ = temp;
                            outputPendingLength_ += PULSE_FPB * getNumChannels();
                        }
                    }

                    // Sleep the required amount of time to ensure we call onAudioDataFunction
                    // every PULSE_FPB samples.
                    int sleepTimeMilliseconds = ((double)PULSE_FPB)/((double)sampleRate_) * 1000.0;
                    std::this_thread::sleep_until(currentTime + 
                        std::chrono::milliseconds(sleepTimeMilliseconds));
                }
            });
            assert(outputPendingThread_ != nullptr);
        }
#endif
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

        outputPendingThreadActive_ = false;
        if (outputPendingThread_ != nullptr)
        {
            outputPendingThread_->join();

            delete[] outputPending_;
            outputPending_ = nullptr;
            outputPendingLength_ = 0;

            delete outputPendingThread_;
            outputPendingThread_ = nullptr;
        }
        inputPendingThreadActive_ = false;
        if (inputPendingThread_ != nullptr)
        {
            inputPendingThread_->join();

            delete[] inputPending_;
            inputPending_ = nullptr;
            inputPendingLength_ = 0;

            delete inputPendingThread_;
            inputPendingThread_ = nullptr;
        }

        sem_destroy(&sem_);
    }
}

int PulseAudioDevice::getLatencyInMicroseconds()
{
    pa_usec_t latency = 0;
    if (stream_ != nullptr)
    {
        int neg = 0;
        pa_stream_get_latency(stream_, &latency, &neg); // ignore error and assume 0
    }
    return (int)latency;
}

void PulseAudioDevice::setHelperRealTime()
{
    // Set RLIMIT_RTTIME, required for rtkit
    struct rlimit rlim;
    memset(&rlim, 0, sizeof(rlim));
    rlim.rlim_cur = 100000ULL; // 100ms
    rlim.rlim_max = rlim.rlim_cur;

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
}

void PulseAudioDevice::stopRealTimeWork()
{
    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
    {
        // Fallback to simple sleep.
        IAudioDevice::stopRealTimeWork();
        return;
    }
    
    ts.tv_nsec += 10000000;
    ts.tv_sec += (ts.tv_nsec / 1000000000);
    ts.tv_nsec = ts.tv_nsec % 1000000000;

    if (sem_timedwait(&sem_, &ts) < 0 && errno != ETIMEDOUT)
    {
        // Fallback to simple sleep.
        IAudioDevice::stopRealTimeWork();
    }
}

void PulseAudioDevice::clearHelperRealTime()
{
    IAudioDevice::clearHelperRealTime();
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
            break;
        }

        // Append received audio to pending block
        {
            std::unique_lock<std::mutex> lk(thisObj->inputPendingMutex_);
            short* temp = new short[thisObj->inputPendingLength_ + length / sizeof(short)];
            assert(temp != nullptr);
    
            if (thisObj->inputPendingLength_ > 0)
            {
                memcpy(temp, thisObj->inputPending_, thisObj->inputPendingLength_ * sizeof(short));
                delete[] thisObj->inputPending_;
                thisObj->inputPending_ = nullptr;
            }
            memcpy(temp + thisObj->inputPendingLength_, data, length);
            thisObj->inputPending_ = temp;
            thisObj->inputPendingLength_ += length / sizeof(short);
        }

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
#if 0
        {
            std::unique_lock<std::mutex> lk(thisObj->outputPendingMutex_);
            if (thisObj->outputPendingLength_ >= numSamples)
            {
                memcpy(data, thisObj->outputPending_, sizeof(data));

                short* tmp = new short[thisObj->outputPendingLength_ - numSamples];
                assert(tmp != nullptr);

                thisObj->outputPendingLength_ -= numSamples;
                memcpy(tmp, thisObj->outputPending_ + numSamples, sizeof(short) * thisObj->outputPendingLength_);

                delete[] thisObj->outputPending_;
                thisObj->outputPending_ = tmp;
            }

            thisObj->targetOutputPendingLength_ = std::max(thisObj->targetOutputPendingLength_, 2 * numSamples);
        }
#endif

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
