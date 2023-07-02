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

#include "FreeDVConfiguration.h"

FreeDVConfiguration::FreeDVConfiguration()
    : firstTimeUse("/FirstTimeUse", true)
    , freedv2020Allowed("/FreeDV2020/Allowed", false)
{
    // empty
}

void FreeDVConfiguration::load(wxConfigBase* config)
{
    load_(config, firstTimeUse);
    load_(config, freedv2020Allowed);
}

void FreeDVConfiguration::save(wxConfigBase* config)
{
    save_(config, firstTimeUse);
    save_(config, freedv2020Allowed);
    
    config->Flush();
}