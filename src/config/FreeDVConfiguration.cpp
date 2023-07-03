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
    
    config->Flush();
}