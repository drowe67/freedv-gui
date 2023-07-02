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
        
    /* Sound device configuration */
    , soundCard1InDeviceName("/Audio/soundCard1InDeviceName", "none")
    , soundCard1InSampleRate("/Audio/soundCard1InSampleRate", -1)
    , soundCard1OutDeviceName("/Audio/soundCard1OutDeviceName", "none")
    , soundCard1OutSampleRate("/Audio/soundCard1OutSampleRate", -1)
    , soundCard2InDeviceName("/Audio/soundCard2InDeviceName", "none")
    , soundCard2InSampleRate("/Audio/soundCard2InSampleRate", -1)
    , soundCard2OutDeviceName("/Audio/soundCard2OutDeviceName", "none")
    , soundCard2OutSampleRate("/Audio/soundCard2OutSampleRate", -1)
        
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
{
    // empty
}

void FreeDVConfiguration::load(wxConfigBase* config)
{
    load_(config, firstTimeUse);
    load_(config, freedv2020Allowed);
    
    load_(config, mainWindowLeft);
    load_(config, mainWindowTop);
    load_(config, mainWindowWidth);
    load_(config, mainWindowHeight);
    
    load_(config, currentNotebookTab);
    
    load_(config, squelchActive);
    load_(config, squelchLevel);
    
    // Migration -- grab old sample rates from older FreeDV configuration
    int oldSoundCard1SampleRate = config->Read(wxT("/Audio/soundCard1SampleRate"), -1);
    int oldSoundCard2SampleRate = config->Read(wxT("/Audio/soundCard2SampleRate"), -1);
    soundCard1InSampleRate.setDefaultVal(oldSoundCard1SampleRate);
    soundCard1OutSampleRate.setDefaultVal(oldSoundCard1SampleRate);
    soundCard2InSampleRate.setDefaultVal(oldSoundCard2SampleRate);
    soundCard2OutSampleRate.setDefaultVal(oldSoundCard2SampleRate);
    
    load_(config, soundCard1InDeviceName);
    load_(config, soundCard1InSampleRate);
    load_(config, soundCard1OutDeviceName);
    load_(config, soundCard1OutSampleRate);
    load_(config, soundCard2InDeviceName);
    load_(config, soundCard2InSampleRate);
    load_(config, soundCard2OutDeviceName);
    load_(config, soundCard2OutSampleRate);
    
    load_(config, fifoSizeMs);
    load_(config, transmitLevel);
    
    load_(config, playFileToMicInPath);
    load_(config, recFileFromRadioPath);
    load_(config, recFileFromRadioSecs);
    load_(config, recFileFromModulatorPath);
    load_(config, recFileFromModulatorSecs);
    load_(config, playFileFromRadioPath);
}

void FreeDVConfiguration::save(wxConfigBase* config)
{
    save_(config, firstTimeUse);
    save_(config, freedv2020Allowed);
    
    save_(config, mainWindowLeft);
    save_(config, mainWindowTop);
    save_(config, mainWindowWidth);
    save_(config, mainWindowHeight);
    
    save_(config, currentNotebookTab);
    
    save_(config, squelchActive);
    save_(config, squelchLevel);
    
    save_(config, soundCard1InDeviceName);
    save_(config, soundCard1InSampleRate);
    save_(config, soundCard1OutDeviceName);
    save_(config, soundCard1OutSampleRate);
    save_(config, soundCard2InDeviceName);
    save_(config, soundCard2InSampleRate);
    save_(config, soundCard2OutDeviceName);
    save_(config, soundCard2OutSampleRate);
    
    save_(config, fifoSizeMs);
    save_(config, transmitLevel);
    
    save_(config, playFileToMicInPath);
    save_(config, recFileFromRadioPath);
    save_(config, recFileFromRadioSecs);
    save_(config, recFileFromModulatorPath);
    save_(config, recFileFromModulatorSecs);
    save_(config, playFileFromRadioPath);
    
    config->Flush();
}