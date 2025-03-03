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

PulseAudioEngine::PulseAudioEngine()
    : initialized_(false)
{
    // empty
}

PulseAudioEngine::~PulseAudioEngine()
{
    if (initialized_)
    {
        stop();
    }
}

void PulseAudioEngine::start()
{
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
    context_ = pa_context_new(mainloopApi_, "FreeDV HF Digital Voice");
    
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
    
    pa_context_set_state_callback(context_, [](pa_context* context, void* mainloop) {
        pa_threaded_mainloop *threadedML = static_cast<pa_threaded_mainloop *>(mainloop);
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
        op = pa_context_get_sink_info_list(context_, [](pa_context *c, const pa_sink_info *i, int eol, void *userdata) {
            PulseAudioDeviceListTemp* tempObj = static_cast<PulseAudioDeviceListTemp*>(userdata);
            
            if (eol)
            {
                pa_threaded_mainloop_signal(tempObj->thisPtr->mainloop_, 0);
                return;
            }

            AudioDeviceSpecification device;
            device.deviceId = i->index;
            device.name = i->name;
            device.apiName = "PulseAudio";
            device.maxChannels = i->sample_spec.channels;
            device.minChannels = 1; // TBD: can minimum be >1 on PulseAudio or pipewire?
            device.defaultSampleRate = i->sample_spec.rate;
            
            tempObj->result.push_back(device);
            
        }, &tempObj);
    }
    else
    {
        op = pa_context_get_source_info_list(context_, [](pa_context *c, const pa_source_info *i, int eol, void *userdata) {
            PulseAudioDeviceListTemp* tempObj = static_cast<PulseAudioDeviceListTemp*>(userdata);
            
            if (eol)
            {
                pa_threaded_mainloop_signal(tempObj->thisPtr->mainloop_, 0);
                return;
            }

            AudioDeviceSpecification device;
            device.deviceId = i->index;
            device.name = i->name;
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
    pa_threaded_mainloop_unlock(mainloop_);
    
    return tempObj.result;
}

std::vector<int> PulseAudioEngine::getSupportedSampleRates(wxString deviceName, AudioDirection direction)
{
    std::vector<int> result;
    
    int index = 0;
    while (IAudioEngine::StandardSampleRates[index] != -1)
    {
        if (IAudioEngine::StandardSampleRates[index] <= 192000)
        {
            result.push_back(IAudioEngine::StandardSampleRates[index]);
        }
        index++;
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
    auto op = pa_context_get_server_info(context_, [](pa_context *c, const pa_server_info *i, void *userdata) {
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

std::shared_ptr<IAudioDevice> PulseAudioEngine::getAudioDevice(wxString deviceName, AudioDirection direction, int sampleRate, int numChannels, bool exclusive)
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
