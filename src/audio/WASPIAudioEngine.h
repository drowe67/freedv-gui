//=========================================================================
// Name:            WASPIAudioEngine.h
// Purpose:         Defines the main interface to the Windows audio system.
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

#ifndef WASPI_AUDIO_ENGINE_H
#define WASPI_AUDIO_ENGINE_H

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <initguid.h>
#include <mmdeviceapi.h>
#include "../util/Win32COMObject.h"
#include "IAudioEngine.h"

class WASPIAudioEngine : public Win32COMObject, public IAudioEngine
{
public:
    WASPIAudioEngine();
    virtual ~WASPIAudioEngine();

    virtual void start() override;
    virtual void stop() override;
    virtual std::vector<AudioDeviceSpecification> getAudioDeviceList(AudioDirection direction) override;
    virtual AudioDeviceSpecification getDefaultAudioDevice(AudioDirection direction) override;
    virtual std::shared_ptr<IAudioDevice> getAudioDevice(wxString deviceName, AudioDirection direction, int sampleRate, int numChannels) override;
    virtual std::vector<int> getSupportedSampleRates(wxString deviceName, AudioDirection direction) override;
    
protected:

private:
    IMMDeviceEnumerator* devEnumerator_;
    IMMDeviceCollection* inputDeviceList_;
    IMMDeviceCollection* outputDeviceList_;

    AudioDeviceSpecification getDeviceSpecification_(IMMDevice* device);
    std::string getUTF8String_(LPWSTR str);
};

#endif // WASPI_AUDIO_ENGINE_H
