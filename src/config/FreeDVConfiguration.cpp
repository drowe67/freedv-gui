//==========================================================================
// Name:            FreeDVConfiguration.cpp
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

#include <wx/stdpaths.h>

#include "../defines.h"
#include "FreeDVConfiguration.h"

FreeDVConfiguration::FreeDVConfiguration()
    /* First time configuration options */
    : firstTimeUse("/FirstTimeUse", true)
    , freedv2020Allowed("/FreeDV2020/Allowed", false)
        
    /* Position and size of main window */
    , mainWindowLeft("/MainFrame/left", 20)
    , mainWindowTop("/MainFrame/top", 20)
    , mainWindowWidth("/MainFrame/width", 800)
    , mainWindowHeight("/MainFrame/height", 780)

    /* Position and size of Audio Config window */
    , audioConfigWindowLeft("/Windows/AudioConfig/left", -1)
    , audioConfigWindowTop("/Windows/AudioConfig/top", -1)
    , audioConfigWindowWidth("/Windows/AudioConfig/width", -1)
    , audioConfigWindowHeight("/Windows/AudioConfig/height", -1)
        
    /* Position and size of FreeDV Reporter */
    , reporterWindowLeft("/Windows/FreeDVReporter/left", 20)
    , reporterWindowTop("/Windows/FreeDVReporter/top", 20)
    , reporterWindowWidth("/Windows/FreeDVReporter/width", -1)
    , reporterWindowHeight("/Windows/FreeDVReporter/height", -1)
    , reporterWindowVisible("/Windows/FreeDVReporter/visible", false)
        
    /* Current tab view */
    , currentNotebookTab("/MainFrame/rxNbookCtrl", 0)
        
    /* Squelch configuration */
    , squelchActive("/Audio/SquelchActive", 1)
    , squelchLevel("/Audio/SquelchLevel", (int)(SQ_DEFAULT_SNR*2))
        
    /* Misc. audio settings */
    , fifoSizeMs("/Audio/fifoSize_ms", (int)FIFO_SIZE)
    , transmitLevel("/Audio/transmitLevel", 0)
        
    /* Recording settings */
    , playFileToMicInPath("/File/playFileToMicInPath", _(""))
    , recFileFromRadioPath("/File/recFileFromRadioPath", _(""))
    , recFileFromRadioSecs("/File/recFileFromRadioSecs", 60)
    , recFileFromModulatorPath("/File/recFileFromModulatorPath", _(""))
    , recFileFromModulatorSecs("/File/recFileFromModulatorSecs", 60)
    , playFileFromRadioPath("/File/playFileFromRadioPath", _(""))
        
    , enableSpaceBarForPTT("/Rig/EnableSpacebarForPTT", true)
        
    , voiceKeyerWaveFilePath("/VoiceKeyer/WaveFilePath", _(""))
    , voiceKeyerWaveFile("/VoiceKeyer/WaveFile", _("voicekeyer.wav"))
    , voiceKeyerRxPause("/VoiceKeyer/RxPause", 10)
    , voiceKeyerRepeats("/VoiceKeyer/Repeats", 5)
        
    , halfDuplexMode("/Rig/HalfDuplex", true)
    , multipleReceiveEnabled("/Rig/MultipleRx", true)
    , multipleReceiveOnSingleThread("/Rig/SingleRxThread", true)
        
    , quickRecordPath("/QuickRecord/SavePath", _(""))
        
    , freedv700Clip("/FreeDV700/txClip", true)
    , freedv700TxBPF("/FreeDV700/txBPF", true)
        
    , noiseSNR("/Noise/noise_snr", 2)
        
    , debugConsoleEnabled("/Debug/console", false)
        
    , snrSlow("/Audio/snrSlow", false)
        
    , debugVerbose("/Debug/verbose", false)
    , apiVerbose("/Debug/APIverbose", false)
        
    , waterfallColor("/Waterfall/Color", 0)
    , statsResetTimeSecs("/Stats/ResetTime", 10)
        
    , currentFreeDVMode("/Audio/mode", 4)
{
    // empty
}

void FreeDVConfiguration::load(wxConfigBase* config)
{
    audioConfiguration.load(config);
    filterConfiguration.load(config);
    rigControlConfiguration.load(config);
    reportingConfiguration.load(config);
    
    load_(config, firstTimeUse);
    load_(config, freedv2020Allowed);
    
    load_(config, mainWindowLeft);
    load_(config, mainWindowTop);
    load_(config, mainWindowWidth);
    load_(config, mainWindowHeight);

    load_(config, audioConfigWindowLeft);
    load_(config, audioConfigWindowTop);
    load_(config, audioConfigWindowWidth);
    load_(config, audioConfigWindowHeight);
    
    load_(config, reporterWindowLeft);
    load_(config, reporterWindowTop);
    load_(config, reporterWindowWidth);
    load_(config, reporterWindowHeight);
    load_(config, reporterWindowVisible);
    
    load_(config, currentNotebookTab);
    
    load_(config, squelchActive);
    load_(config, squelchLevel);
    
    load_(config, fifoSizeMs);
    load_(config, transmitLevel);
    
    load_(config, playFileToMicInPath);
    load_(config, recFileFromRadioPath);
    load_(config, recFileFromRadioSecs);
    load_(config, recFileFromModulatorPath);
    load_(config, recFileFromModulatorSecs);
    load_(config, playFileFromRadioPath);
    
    load_(config, enableSpaceBarForPTT);
    
    load_(config, voiceKeyerWaveFilePath);
    load_(config, voiceKeyerWaveFile);
    load_(config, voiceKeyerRxPause);
    load_(config, voiceKeyerRepeats);
    
    load_(config, halfDuplexMode);
    load_(config, multipleReceiveEnabled);
    load_(config, multipleReceiveOnSingleThread);
    
    load_(config, freedv700Clip);
    load_(config, freedv700TxBPF);
    
    load_(config, noiseSNR);
    
    load_(config, debugConsoleEnabled);
    
    load_(config, snrSlow);
    
    load_(config, debugVerbose);
    load_(config, apiVerbose);
    
    load_(config, waterfallColor);
    
    load_(config, statsResetTimeSecs);
    load_(config, currentFreeDVMode);
    
    auto wxStandardPathObj = wxStandardPaths::Get();
    auto documentsDir = wxStandardPathObj.GetDocumentsDir();
    quickRecordPath.setDefaultVal(documentsDir);
    load_(config, quickRecordPath);
}

void FreeDVConfiguration::save(wxConfigBase* config)
{
    audioConfiguration.save(config);
    filterConfiguration.save(config);
    rigControlConfiguration.save(config);
    reportingConfiguration.save(config);
    
    save_(config, firstTimeUse);
    save_(config, freedv2020Allowed);
    
    save_(config, mainWindowLeft);
    save_(config, mainWindowTop);
    save_(config, mainWindowWidth);
    save_(config, mainWindowHeight);

    save_(config, audioConfigWindowLeft);
    save_(config, audioConfigWindowTop);
    save_(config, audioConfigWindowWidth);
    save_(config, audioConfigWindowHeight);
    
    save_(config, reporterWindowLeft);
    save_(config, reporterWindowTop);
    save_(config, reporterWindowWidth);
    save_(config, reporterWindowHeight);
    save_(config, reporterWindowVisible);
    
    save_(config, currentNotebookTab);
    
    save_(config, squelchActive);
    save_(config, squelchLevel);
    
    save_(config, fifoSizeMs);
    save_(config, transmitLevel);
    
    save_(config, playFileToMicInPath);
    save_(config, recFileFromRadioPath);
    save_(config, recFileFromRadioSecs);
    save_(config, recFileFromModulatorPath);
    save_(config, recFileFromModulatorSecs);
    save_(config, playFileFromRadioPath);
    
    save_(config, enableSpaceBarForPTT);
    
    save_(config, voiceKeyerWaveFilePath);
    save_(config, voiceKeyerWaveFile);
    save_(config, voiceKeyerRxPause);
    save_(config, voiceKeyerRepeats);
    
    save_(config, halfDuplexMode);
    save_(config, multipleReceiveEnabled);
    save_(config, multipleReceiveOnSingleThread);
    
    save_(config, quickRecordPath);
    
    save_(config, freedv700Clip);
    save_(config, freedv700TxBPF);
    
    save_(config, noiseSNR);
    
    save_(config, debugConsoleEnabled);
    
    save_(config, snrSlow);
    
    save_(config, debugVerbose);
    save_(config, apiVerbose);
    
    save_(config, waterfallColor);
    
    save_(config, statsResetTimeSecs);
    save_(config, currentFreeDVMode);
    
    config->Flush();
}