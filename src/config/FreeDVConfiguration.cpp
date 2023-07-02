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
    
    config->Flush();
}