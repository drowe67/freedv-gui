//==========================================================================
// Name:            FreeDVConfiguration.h
// Purpose:         Implements the configuration for FreeDV
// Created:         July 1, 2023
// Authors:         Mooneer Salem
// 
// License:
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
//==========================================================================

#ifndef FREEDV_CONFIGURATION_H
#define FREEDV_CONFIGURATION_H

#include <inttypes.h>
#include "WxWidgetsConfigStore.h"
#include "ConfigurationDataElement.h"
#include "AudioConfiguration.h"
#include "FilterConfiguration.h"
#include "RigControlConfiguration.h"
#include "ReportingConfiguration.h"

class FreeDVConfiguration : public WxWidgetsConfigStore
{
public:
    FreeDVConfiguration();
    virtual ~FreeDVConfiguration() = default;
    
    AudioConfiguration audioConfiguration;
    FilterConfiguration filterConfiguration;
    RigControlConfiguration rigControlConfiguration;
    ReportingConfiguration reportingConfiguration;
    
    ConfigurationDataElement<bool> firstTimeUse;
    ConfigurationDataElement<bool> freedv2020Allowed;
    
    ConfigurationDataElement<long> mainWindowLeft;
    ConfigurationDataElement<long> mainWindowTop;
    ConfigurationDataElement<long> mainWindowWidth;
    ConfigurationDataElement<long> mainWindowHeight;
    
    ConfigurationDataElement<long> currentNotebookTab;
    
    ConfigurationDataElement<long> squelchActive;
    ConfigurationDataElement<long> squelchLevel;
    
    ConfigurationDataElement<int> fifoSizeMs;
    ConfigurationDataElement<int> transmitLevel;
    
    ConfigurationDataElement<wxString> playFileToMicInPath;
    ConfigurationDataElement<wxString> recFileFromRadioPath;
    ConfigurationDataElement<unsigned int> recFileFromRadioSecs;
    ConfigurationDataElement<wxString> recFileFromModulatorPath;
    ConfigurationDataElement<unsigned int> recFileFromModulatorSecs;
    ConfigurationDataElement<wxString> playFileFromRadioPath;
    
    ConfigurationDataElement<bool> enableSpaceBarForPTT;
    
    ConfigurationDataElement<wxString> voiceKeyerWaveFilePath;
    ConfigurationDataElement<wxString> voiceKeyerWaveFile;
    ConfigurationDataElement<int> voiceKeyerRxPause;
    ConfigurationDataElement<int> voiceKeyerRepeats;
    
    ConfigurationDataElement<bool> halfDuplexMode;
    ConfigurationDataElement<bool> multipleReceiveEnabled;
    ConfigurationDataElement<bool> multipleReceiveOnSingleThread;
    
    ConfigurationDataElement<wxString> quickRecordPath;
    
    ConfigurationDataElement<bool> freedv700Clip;
    ConfigurationDataElement<bool> freedv700TxBPF;
    
    ConfigurationDataElement<int> noiseSNR;
    
    ConfigurationDataElement<bool> debugConsoleEnabled; // note: Windows only
    
    ConfigurationDataElement<bool> snrSlow;
    
    ConfigurationDataElement<bool> debugVerbose;
    ConfigurationDataElement<bool> apiVerbose;
    
    ConfigurationDataElement<int> waterfallColor;
    ConfigurationDataElement<unsigned int> statsResetTimeSecs;
    
    ConfigurationDataElement<int> currentFreeDVMode;
    
    virtual void load(wxConfigBase* config) override;
    virtual void save(wxConfigBase* config) override;
};

#endif // FREEDV_CONFIGURATION_H
