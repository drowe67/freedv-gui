//==========================================================================
// Name:            FilterConfiguration.cpp
// Purpose:         Implements the filter configuration for FreeDV
// Created:         July 2, 2023
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

#include "FilterConfiguration.h"

DEFINE_FILTER_CONFIG_NAMES(MicIn);
DEFINE_FILTER_CONFIG_NAMES(SpkOut);
DEFINE_FILTER_CONFIG_NAMES_OLD(MicIn);
DEFINE_FILTER_CONFIG_NAMES_OLD(SpkOut);

FilterConfiguration::FilterConfiguration()
    : noiseReductionEnable("/Filter/speexpp_enable", true)
    , agcEnabled("/Filter/agcEnable", true)
    , bwExpandEnabled("/Filter/bwExpandEnable", true)
{
    // empty
}

void FilterConfiguration::load(wxConfigBase* config)
{    
    micInChannel.load(config);
    spkOutChannel.load(config);
    
    load_(config, noiseReductionEnable);
    load_(config, agcEnabled);
    load_(config, bwExpandEnabled);
}

void FilterConfiguration::save(wxConfigBase* config)
{
    micInChannel.save(config);
    spkOutChannel.save(config);
    
    save_(config, noiseReductionEnable);
    save_(config, agcEnabled);
    save_(config, bwExpandEnabled);
}
