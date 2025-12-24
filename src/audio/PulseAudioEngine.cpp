//=========================================================================
// Name:            PulseAudioEngine.cpp
// Purpose:         Defines the interface to the PulseAudio audio engine.
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

#include "PulseAudioDevice.h"
#include "PulseAudioEngine.h"

#include <map>
#include "../util/logging/ulog.h"

#if defined(USE_RTKIT)
#include "rtkit.h"
#endif // defined(USE_RTKIT)

PulseAudioEngine::PulseAudioEngine()
    : initialized_(false)
{
    // empty
}

PulseAudioEngine::~PulseAudioEngine()
{
    if (initialized_)
    {
        stopImpl_();
    }
}

void PulseAudioEngine::start()
{
    std::unique_lock<std::mutex> lk(startStopMtx_);

    // Allocate PA main loop and context.
    mainloop_ = pa_threaded_mainloop_new();
    
    if (mainloop_ == nullptr)
    {
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, "Could not allocate PulseAudio main loop.", onAudioErrorState);
        }
        return;
    }
    
    mainloopApi_ = pa_threaded_mainloop_get_api(mainloop_);
    context_ = pa_context_new(mainloopApi_, "FreeDV");
    
    if (context_ == nullptr)
    {
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, "Could not allocate PulseAudio context.", onAudioErrorState);
        }
        
        pa_threaded_mainloop_free(mainloop_);
        mainloop_ = nullptr;
        return;
    }
    
    pa_context_set_state_callback(context_, [](pa_context*, void* mainloop) {
        pa_threaded_mainloop *threadedML = static_cast<pa_threaded_mainloop *>(mainloop);

#if defined(USE_RTKIT)
        if (pa_threaded_mainloop_in_thread(threadedML))
        {
            DBusError error;
            DBusConnection* bus = nullptr;
            int result = 0;

            dbus_error_init(&error);
            if (!(bus = dbus_bus_get(DBUS_BUS_SYSTEM, &error)))
            {
                log_warn("Could not connect to system bus: %s", error.message);
            }
            else
            {
                int minNiceLevel = 0;
                constexpr int ERROR_BUFFER_SIZE = 1024;
                char tmpBuf[ERROR_BUFFER_SIZE];
                if ((result = rtkit_get_min_nice_level(bus, &minNiceLevel)) < 0)
                {
#if (_POSIX_C_SOURCE >= 200112L) && !_GNU_SOURCE
                    strerror_r(-result, tmpBuf, ERROR_BUFFER_SIZE);
                    log_warn("rtkit could not get minimum nice level: %s", tmpBuf);
#else
                    auto ptr = strerror_r(-result, tmpBuf, ERROR_BUFFER_SIZE);
                    if (ptr != 0)
                    {
                        strncpy(tmpBuf, "(null)", 7);
                    }
                    log_warn("rtkit could not get minimum nice level: %s", tmpBuf);
#endif // (_POSIX_C_SOURCE >= 200112L) && !_GNU_SOURCE
                }
                else if ((result = rtkit_make_high_priority(bus, 0, minNiceLevel)) < 0)
                {
#if (_POSIX_C_SOURCE >= 200112L) && !_GNU_SOURCE
                    strerror_r(-result, tmpBuf, ERROR_BUFFER_SIZE);
                    log_warn("rtkit could not make high priority: %s", tmpBuf);
#else
                    auto ptr = strerror_r(-result, tmpBuf, ERROR_BUFFER_SIZE);
                    if (ptr != 0)
                    {
                        strncpy(tmpBuf, "(null)", 7);
                    }
                    log_warn("rtkit could not make high priority: %s", tmpBuf);
#endif // (_POSIX_C_SOURCE >= 200112L) && !_GNU_SOURCE
                }
            }
    
            if (bus != nullptr)
            {
                dbus_connection_unref(bus);
            }
        }
#endif // defined(USE_RTKIT)

        pa_threaded_mainloop_signal(threadedML, 0);
    }, mainloop_);
    
    // Start main loop.
    pa_threaded_mainloop_lock(mainloop_);
    if (pa_threaded_mainloop_start(mainloop_) != 0)
    {
        pa_threaded_mainloop_unlock(mainloop_);
        
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, "Could not start PulseAudio main loop.", onAudioErrorState);
        }
        
        pa_context_unref(context_);
        pa_threaded_mainloop_free(mainloop_);
        mainloop_ = nullptr;
        context_ = nullptr;
        return;
    }
    
    // Connect context to default PA server.
    if (pa_context_connect(context_, NULL, PA_CONTEXT_NOFLAGS, NULL) != 0)
    {
        pa_threaded_mainloop_unlock(mainloop_);
        
        if (onAudioErrorFunction)
        {
            onAudioErrorFunction(*this, "Could not connect PulseAudio context.", onAudioErrorState);
        }
        
        pa_threaded_mainloop_stop(mainloop_);
        pa_context_unref(context_);
        pa_threaded_mainloop_free(mainloop_);
        return;
    }

    // Wait for the context to be ready
    for(;;) 
    {
        pa_context_state_t context_state = pa_context_get_state(context_);
        assert(PA_CONTEXT_IS_GOOD(context_state));
        if (context_state == PA_CONTEXT_READY) break;
        pa_threaded_mainloop_wait(mainloop_);
    }
    
    pa_threaded_mainloop_unlock(mainloop_);
    initialized_ = true;
}

void PulseAudioEngine::stop()
{
    stopImpl_();
}

void PulseAudioEngine::stopImpl_()
{
    std::unique_lock<std::mutex> lk(startStopMtx_);

    if (initialized_)
    {
        pa_threaded_mainloop_lock(mainloop_);
        pa_context_disconnect(context_);
        pa_threaded_mainloop_unlock(mainloop_);
    
        pa_threaded_mainloop_stop(mainloop_);
        pa_context_unref(context_);
        pa_threaded_mainloop_free(mainloop_);

        mainloop_ = nullptr;
        mainloopApi_ = nullptr;
        context_ = nullptr;
        initialized_ = false;
    }
}

struct PulseAudioDeviceListTemp
{
    std::vector<AudioDeviceSpecification> result;
    std::map<int, std::string> cardResult;
    PulseAudioEngine* thisPtr;
};

std::vector<AudioDeviceSpecification> PulseAudioEngine::getAudioDeviceList(AudioDirection direction)
{
    PulseAudioDeviceListTemp tempObj;
    tempObj.thisPtr = this;
    
    pa_operation* op = nullptr;
    
    pa_threaded_mainloop_lock(mainloop_);
    if (direction == AUDIO_ENGINE_OUT)
    {
        op = pa_context_get_sink_info_list(context_, [](pa_context *, const pa_sink_info *i, int eol, void *userdata) {
            PulseAudioDeviceListTemp* tempObj = static_cast<PulseAudioDeviceListTemp*>(userdata);
            
            if (eol)
            {
                pa_threaded_mainloop_signal(tempObj->thisPtr->mainloop_, 0);
                return;
            }

            AudioDeviceSpecification device;
            device.deviceId = i->index;
            device.name = wxString::FromUTF8(i->name);
            device.cardIndex = i->card;
            device.apiName = "PulseAudio";
            device.maxChannels = i->sample_spec.channels;
            device.minChannels = 1; // TBD: can minimum be >1 on PulseAudio or pipewire?
            device.defaultSampleRate = i->sample_spec.rate;
            
            tempObj->result.push_back(device);
            
        }, &tempObj);
    }
    else
    {
        op = pa_context_get_source_info_list(context_, [](pa_context *, const pa_source_info *i, int eol, void *userdata) {
            PulseAudioDeviceListTemp* tempObj = static_cast<PulseAudioDeviceListTemp*>(userdata);
            
            if (eol)
            {
                pa_threaded_mainloop_signal(tempObj->thisPtr->mainloop_, 0);
                return;
            }

            AudioDeviceSpecification device;
            device.deviceId = i->index;
            device.name = wxString::FromUTF8(i->name);
            device.cardIndex = i->card;
            device.apiName = "PulseAudio";
            device.maxChannels = i->sample_spec.channels;
            device.minChannels = 1; // TBD: can minimum be >1 on PulseAudio or pipewire?
            device.defaultSampleRate = i->sample_spec.rate;
            
            tempObj->result.push_back(device);
        }, &tempObj);
    }
    
    // Wait for the operation to complete
    for(;;) 
    {
        if (pa_operation_get_state(op) != PA_OPERATION_RUNNING) break;
        pa_threaded_mainloop_wait(mainloop_);
    }

    pa_operation_unref(op);
    
    // Get list of cards
    op = pa_context_get_card_info_list(context_, [](pa_context *, const pa_card_info *i, int eol, void *userdata)
    {
        PulseAudioDeviceListTemp* tempObj = static_cast<PulseAudioDeviceListTemp*>(userdata);
        
        if (eol)
        {
            pa_threaded_mainloop_signal(tempObj->thisPtr->mainloop_, 0);
            return;
        }
        
        tempObj->cardResult[i->index] = i->name;
    }, &tempObj);
    
    // Wait for the operation to complete
    for(;;) 
    {
        if (pa_operation_get_state(op) != PA_OPERATION_RUNNING) break;
        pa_threaded_mainloop_wait(mainloop_);
    }

    pa_operation_unref(op);
    pa_threaded_mainloop_unlock(mainloop_);

    // Iterate over result and populate cardName
    for (auto& obj : tempObj.result)
    {
        if (tempObj.cardResult.find(obj.cardIndex) != tempObj.cardResult.end())
        {
            obj.cardName = wxString::FromUTF8(tempObj.cardResult[obj.cardIndex].c_str());
        }
    }
    
    return tempObj.result;
}

std::vector<int> PulseAudioEngine::getSupportedSampleRates(wxString deviceName, AudioDirection direction)
{
    std::vector<int> result;
    
#if 0
    int index = 0;
    while (IAudioEngine::StandardSampleRates[index] != -1)
    {
        if (IAudioEngine::StandardSampleRates[index] <= 192000)
        {
            result.push_back(IAudioEngine::StandardSampleRates[index]);
        }
        index++;
    }
#endif
    
    auto devList = getAudioDeviceList(direction);
    for (auto& dev : devList)
    {
        if (dev.name == deviceName)
        {
            result.push_back(dev.defaultSampleRate);
            break;
        }
    }
    
    return result;
}

struct PaDefaultAudioDeviceTemp
{
    std::string defaultSink;
    std::string defaultSource;
    pa_threaded_mainloop *mainloop;
};

AudioDeviceSpecification PulseAudioEngine::getDefaultAudioDevice(AudioDirection direction)
{
    PaDefaultAudioDeviceTemp tempData;
    tempData.mainloop = mainloop_;
    
    pa_threaded_mainloop_lock(mainloop_);
    auto op = pa_context_get_server_info(context_, [](pa_context *, const pa_server_info *i, void *userdata) {
        PaDefaultAudioDeviceTemp* tempData = static_cast<PaDefaultAudioDeviceTemp*>(userdata);
        
        tempData->defaultSink = i->default_sink_name;
        tempData->defaultSource = i->default_source_name;
        pa_threaded_mainloop_signal(tempData->mainloop, 0);
    }, &tempData);
    
    // Wait for the operation to complete
    for(;;) 
    {
        if (pa_operation_get_state(op) != PA_OPERATION_RUNNING) break;
        pa_threaded_mainloop_wait(mainloop_);
    }
    
    pa_operation_unref(op);    
    pa_threaded_mainloop_unlock(mainloop_);
    
    auto devices = getAudioDeviceList(direction);
    std::string defaultDeviceName = direction == AUDIO_ENGINE_IN ? tempData.defaultSource : tempData.defaultSink;
    for (auto& device : devices)
    {
        if (device.name == defaultDeviceName)
        {
            return device;
        }
    }
    
    return AudioDeviceSpecification::GetInvalidDevice();
}

std::shared_ptr<IAudioDevice> PulseAudioEngine::getAudioDevice(wxString deviceName, AudioDirection direction, int sampleRate, int numChannels)
{
    auto deviceList = getAudioDeviceList(direction);
    
    auto supportedSampleRates = getSupportedSampleRates(deviceName, direction);
    bool found = false;
    for (auto& rate : supportedSampleRates)
    {
        if (rate == sampleRate)
        {
            found = true;
            break;
        }
    }

    for (auto& dev : deviceList)
    {
        if (dev.name == deviceName)
        {
            if (!found)
            {
                // Use device's default sample rate if we somehow got an unsupported one.
                sampleRate = dev.defaultSampleRate;
            }

            // Cap number of channels to allowed range.
            numChannels = std::max(numChannels, dev.minChannels);
            numChannels = std::min(numChannels, dev.maxChannels);

            // Create device object.
            auto devObj = 
                new PulseAudioDevice(
                    mainloop_, context_, deviceName, direction, sampleRate, 
                    dev.maxChannels >= numChannels ? numChannels : dev.maxChannels);
            return std::shared_ptr<IAudioDevice>(devObj);
        }
    }
    
    return nullptr;
}
