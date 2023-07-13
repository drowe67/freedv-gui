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
    : codec2LPCPostFilterEnable("/Filter/codec2LPCPostFilterEnable", true)
    , codec2LPCPostFilterBassBoost("/Filter/codec2LPCPostFilterBassBoost", true)
    , codec2LPCPostFilterGamma("/Filter/codec2LPCPostFilter/Gamma", CODEC2_LPC_PF_GAMMA*100)
    , codec2LPCPostFilterBeta("/Filter/codec2LPCPostFilter/Beta", CODEC2_LPC_PF_BETA*100)
    , speexppEnable("/Filter/speexpp_enable", true)
    , enable700CEqualizer("/Filter/700C_EQ", true)
{
    std::function<float(float)> gammaBetaSaveProcessor = [](float val) {
        return val * 100.0;
    };
    
    std::function<float(float)> gammaBetaLoadProcessor = [](float val) {
        return val / 100.0;
    };
    
    codec2LPCPostFilterGamma.setSaveProcessor(gammaBetaSaveProcessor);
    codec2LPCPostFilterBeta.setSaveProcessor(gammaBetaSaveProcessor);
    codec2LPCPostFilterGamma.setLoadProcessor(gammaBetaLoadProcessor);
    codec2LPCPostFilterBeta.setLoadProcessor(gammaBetaLoadProcessor);
}

void FilterConfiguration::load(wxConfigBase* config)
{
    // Migration: these values were using incorrect data types, so we have to
    // move to new names in order to prevent Windows errors (for example:)
    //
    // 14:03:28: Registry value "HKCU\Software\freedv\Filter\codec2LPCPostFilterGamma" is not text (but of type DWORD)
    // 14:03:28: Registry value "HKCU\Software\freedv\Filter\codec2LPCPostFilterBeta" is not text (but of type DWORD)
    auto oldPostFilterGamma = (float)config->Read(
        wxT("/Filter/codec2LPCPostFilterGamma"), 
        CODEC2_LPC_PF_GAMMA*100);
    codec2LPCPostFilterGamma.setDefaultVal(oldPostFilterGamma);
    
    auto oldPostFilterBeta = (float)config->Read(
        wxT("/Filter/codec2LPCPostFilterBeta"), 
        CODEC2_LPC_PF_BETA*100);
    codec2LPCPostFilterBeta.setDefaultVal(oldPostFilterBeta);
    
    micInChannel.load(config);
    spkOutChannel.load(config);
    
    load_(config, codec2LPCPostFilterEnable);
    load_(config, codec2LPCPostFilterBassBoost);
    load_(config, codec2LPCPostFilterGamma);
    load_(config, codec2LPCPostFilterBeta);
    
    load_(config, speexppEnable);
    load_(config, enable700CEqualizer);
}

void FilterConfiguration::save(wxConfigBase* config)
{
    micInChannel.save(config);
    spkOutChannel.save(config);
    
    save_(config, codec2LPCPostFilterEnable);
    save_(config, codec2LPCPostFilterBassBoost);
    save_(config, codec2LPCPostFilterGamma);
    save_(config, codec2LPCPostFilterBeta);
    
    save_(config, speexppEnable);
    save_(config, enable700CEqualizer);
}