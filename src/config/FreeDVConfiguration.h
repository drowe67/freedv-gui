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
#include <wx/string.h>
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
    
    ConfigurationDataElement<long> mainWindowLeft;
    ConfigurationDataElement<long> mainWindowTop;
    ConfigurationDataElement<long> mainWindowWidth;
    ConfigurationDataElement<long> mainWindowHeight;

    ConfigurationDataElement<long> audioConfigWindowLeft;
    ConfigurationDataElement<long> audioConfigWindowTop;
    ConfigurationDataElement<long> audioConfigWindowWidth;
    ConfigurationDataElement<long> audioConfigWindowHeight;
    
    ConfigurationDataElement<long> reporterWindowLeft;
    ConfigurationDataElement<long> reporterWindowTop;
    ConfigurationDataElement<long> reporterWindowWidth;
    ConfigurationDataElement<long> reporterWindowHeight;
    ConfigurationDataElement<bool> reporterWindowVisible;
    ConfigurationDataElement<long> msgEditDialogWidth;
    ConfigurationDataElement<int> reporterWindowCurrentSort;
    ConfigurationDataElement<bool> reporterWindowCurrentSortDirection;
    
    ConfigurationDataElement<long> currentNotebookTab;
    
    ConfigurationDataElement<int> fifoSizeMs;
    ConfigurationDataElement<int> transmitLevel;
    ConfigurationDataElement<int> tuneLevel;
    ConfigurationDataElement<std::map<wxString, int>> txAttenByBand;
    ConfigurationDataElement<std::map<wxString, int>> tuneAttenByBand;
    
    ConfigurationDataElement<wxString> playFileToMicInPath;
    ConfigurationDataElement<wxString> playFileFromRadioPath;
    
    ConfigurationDataElement<bool> enableSpaceBarForPTT;
    ConfigurationDataElement<int> pttKeyCode;
    ConfigurationDataElement<bool> pttMomentaryMode;

    ConfigurationDataElement<wxString> voiceKeyerWaveFilePath;
    ConfigurationDataElement<wxString> voiceKeyerWaveFile;
    ConfigurationDataElement<int> voiceKeyerRxPause;
    ConfigurationDataElement<int> voiceKeyerRepeats;
    
    ConfigurationDataElement<bool> halfDuplexMode;
    
    ConfigurationDataElement<wxString> quickRecordRawPath;
    ConfigurationDataElement<wxString> quickRecordDecodedPath;
    
    ConfigurationDataElement<bool> debugConsoleEnabled; // note: Windows only
    
    ConfigurationDataElement<bool> snrSlow;
    
    ConfigurationDataElement<bool> debugVerbose;
    ConfigurationDataElement<bool> apiVerbose;
    
    ConfigurationDataElement<int> waterfallColor;
    ConfigurationDataElement<unsigned int> statsResetTimeSecs;
        
    ConfigurationDataElement<int> currentSpectrumAveraging;
    
    ConfigurationDataElement<bool> experimentalFeatures;
    ConfigurationDataElement<wxString> tabLayout;

    ConfigurationDataElement<bool> monitorVoiceKeyerAudio;
    ConfigurationDataElement<float> monitorVoiceKeyerAudioVol;
    ConfigurationDataElement<bool> monitorTxAudio;
    ConfigurationDataElement<float> monitorTxAudioVol;

    ConfigurationDataElement<int> txRxDelayMilliseconds;

    ConfigurationDataElement<int> reportingUserMsgColWidth;
    
    ConfigurationDataElement<bool> showDecodeStats;

    ConfigurationDataElement<bool> autoStartOnLaunch;

    // Independent user-toggleable visibility for the main window's optional
    // group boxes (Show menu), separate from the feature-driven visibility
    // of e.g. Radio Freq/Stats.
    ConfigurationDataElement<bool> showSnrBox;
    ConfigurationDataElement<bool> showLevelBox;
    ConfigurationDataElement<bool> showSyncBox;
    ConfigurationDataElement<bool> showAudioRecordingBox;
    ConfigurationDataElement<bool> showLoggingBox;
    ConfigurationDataElement<bool> showReportingBox;
    ConfigurationDataElement<bool> showTxAttenuationBox;
    ConfigurationDataElement<bool> showSpeakerLevelBox;

    // Group box tint colour/strength (Display options tab), replacing the
    // old FREEDV_GROUPBOX_TINT testing-only environment variable.
    ConfigurationDataElement<wxString> groupBoxTintColor;
    ConfigurationDataElement<int> groupBoxTintPercent;

    // Ordered lists of the movable "Show menu" group boxes currently on each
    // side of the main window (right-click a box's title to move it), using
    // the same box index enumeration as ID_SHOW_GROUPBOX_BASE/OnShowGroupBox:
    // 0=SNR, 1=Level, 2=Sync, 3=AudioRecording, 4=Logging, 5=FDVReporting,
    // 6=TXAttenuation, 7=SpeakerLevel. Defaults match the original hardcoded
    // layout. A box's presence in a list is independent of its show/hide
    // state -- hiding a box leaves it in place in whichever list already
    // has it, so it reappears in the same position when shown again.
    ConfigurationDataElement<std::vector<int>> groupBoxLeftOrder;
    ConfigurationDataElement<std::vector<int>> groupBoxRightOrder;

    virtual void load(wxConfigBase* config) override;
    virtual void save(wxConfigBase* config) override;
};

#endif // FREEDV_CONFIGURATION_H
