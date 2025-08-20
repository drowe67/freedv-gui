//==========================================================================
// Name:            FilterConfiguration.h
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

#ifndef FILTER_CONFIGURATION_H
#define FILTER_CONFIGURATION_H

#include "../defines.h"
#include "WxWidgetsConfigStore.h"
#include "ConfigurationDataElement.h"

class FilterConfiguration : public WxWidgetsConfigStore
{
public:
    enum Channel { MicIn, SpkOut };
    
    template<FilterConfiguration::Channel ChannelId>
    class FilterChannelConfigName
    {
    public:
        static const char* GetBassFreqHzConfigName() { assert(0); return nullptr; }
        static const char* GetBassGaindBConfigName() { assert(0); return nullptr; }
        static const char* GetTrebleFreqHzConfigName() { assert(0); return nullptr; }
        static const char* GetTrebleGaindBConfigName() { assert(0); return nullptr; }
        static const char* GetMidFreqHzConfigName() { assert(0); return nullptr; }
        static const char* GetMidGaindBConfigName() { assert(0); return nullptr; }
        static const char* GetMidQConfigName() { assert(0); return nullptr; }
        static const char* GetVolInDBConfigName() { assert(0); return nullptr; }
        static const char* GetEQEnableConfigName() { assert(0); return nullptr; }
        
        static const char* GetOldBassFreqHzConfigName() { assert(0); return nullptr; }
        static const char* GetOldBassGaindBConfigName() { assert(0); return nullptr; }
        static const char* GetOldTrebleFreqHzConfigName() { assert(0); return nullptr; }
        static const char* GetOldTrebleGaindBConfigName() { assert(0); return nullptr; }
        static const char* GetOldMidFreqHzConfigName() { assert(0); return nullptr; }
        static const char* GetOldMidGaindBConfigName() { assert(0); return nullptr; }
        static const char* GetOldMidQConfigName() { assert(0); return nullptr; }
        static const char* GetOldVolInDBConfigName() { assert(0); return nullptr; }
        static const char* GetOldEQEnableConfigName() { assert(0); return nullptr; }
    };
    
    template<FilterConfiguration::Channel ChannelId>
    class FilterChannel : public WxWidgetsConfigStore
    {
    public:
        FilterChannel();
        virtual ~FilterChannel() = default;
        
        ConfigurationDataElement<float> bassFreqHz;
        ConfigurationDataElement<float> bassGaindB;
        ConfigurationDataElement<float> trebleFreqHz;
        ConfigurationDataElement<float> trebleGaindB;
        ConfigurationDataElement<float> midFreqHz;
        ConfigurationDataElement<float> midGainDB;
        ConfigurationDataElement<float> midQ;
        ConfigurationDataElement<float> volInDB;
        ConfigurationDataElement<bool> eqEnable;
        
        virtual void load(wxConfigBase* config) override;
        virtual void save(wxConfigBase* config) override;
    };
    
    FilterConfiguration();
    virtual ~FilterConfiguration() = default;
    
    FilterChannel<MicIn> micInChannel;
    FilterChannel<SpkOut> spkOutChannel;
    
    ConfigurationDataElement<bool> codec2LPCPostFilterEnable;
    ConfigurationDataElement<bool> codec2LPCPostFilterBassBoost;
    ConfigurationDataElement<float> codec2LPCPostFilterGamma;
    ConfigurationDataElement<float> codec2LPCPostFilterBeta;
    
    ConfigurationDataElement<bool> speexppEnable;
    ConfigurationDataElement<bool> agcEnabled;
    ConfigurationDataElement<bool> enable700CEqualizer;
    
    virtual void load(wxConfigBase* config) override;
    virtual void save(wxConfigBase* config) override;
};

#define DECLARE_FILTER_CONFIG_NAME(type, name) \
    template<> \
    const char* FilterConfiguration::FilterChannelConfigName<FilterConfiguration::type>::Get ## name ## ConfigName()
        
#define DECLARE_FILTER_CONFIG_NAMES(type) \
    DECLARE_FILTER_CONFIG_NAME(type, BassFreqHz); \
    DECLARE_FILTER_CONFIG_NAME(type, BassGaindB); \
    DECLARE_FILTER_CONFIG_NAME(type, TrebleFreqHz); \
    DECLARE_FILTER_CONFIG_NAME(type, TrebleGaindB); \
    DECLARE_FILTER_CONFIG_NAME(type, MidFreqHz); \
    DECLARE_FILTER_CONFIG_NAME(type, MidGaindB); \
    DECLARE_FILTER_CONFIG_NAME(type, MidQ); \
    DECLARE_FILTER_CONFIG_NAME(type, VolInDB); \
    DECLARE_FILTER_CONFIG_NAME(type, EQEnable);

#define DECLARE_FILTER_CONFIG_NAME_OLD(type, name) \
    template<> \
    const char* FilterConfiguration::FilterChannelConfigName<FilterConfiguration::type>::GetOld ## name ## ConfigName()
        
#define DECLARE_FILTER_CONFIG_NAMES_OLD(type) \
    DECLARE_FILTER_CONFIG_NAME_OLD(type, BassFreqHz); \
    DECLARE_FILTER_CONFIG_NAME_OLD(type, BassGaindB); \
    DECLARE_FILTER_CONFIG_NAME_OLD(type, TrebleFreqHz); \
    DECLARE_FILTER_CONFIG_NAME_OLD(type, TrebleGaindB); \
    DECLARE_FILTER_CONFIG_NAME_OLD(type, MidFreqHz); \
    DECLARE_FILTER_CONFIG_NAME_OLD(type, MidGaindB); \
    DECLARE_FILTER_CONFIG_NAME_OLD(type, MidQ); \
    DECLARE_FILTER_CONFIG_NAME_OLD(type, VolInDB); \
    DECLARE_FILTER_CONFIG_NAME_OLD(type, EQEnable);
    
#define DEFINE_FILTER_CONFIG_NAME(type, name) \
    DECLARE_FILTER_CONFIG_NAME(type, name) { return "/Filter/" #type "/" #name; }
    
#define DEFINE_FILTER_CONFIG_NAME_OLD(type, name) \
    DECLARE_FILTER_CONFIG_NAME_OLD(type, name) { return "/Filter/" #type #name; }

#define DEFINE_FILTER_CONFIG_NAMES(type) \
    DEFINE_FILTER_CONFIG_NAME(type, BassFreqHz) \
    DEFINE_FILTER_CONFIG_NAME(type, BassGaindB) \
    DEFINE_FILTER_CONFIG_NAME(type, TrebleFreqHz) \
    DEFINE_FILTER_CONFIG_NAME(type, TrebleGaindB) \
    DEFINE_FILTER_CONFIG_NAME(type, MidFreqHz) \
    DEFINE_FILTER_CONFIG_NAME(type, MidGaindB) \
    DEFINE_FILTER_CONFIG_NAME(type, MidQ) \
    DEFINE_FILTER_CONFIG_NAME(type, VolInDB) \
    DEFINE_FILTER_CONFIG_NAME(type, EQEnable)
        
#define DEFINE_FILTER_CONFIG_NAMES_OLD(type) \
    DEFINE_FILTER_CONFIG_NAME_OLD(type, BassFreqHz) \
    DEFINE_FILTER_CONFIG_NAME_OLD(type, BassGaindB) \
    DEFINE_FILTER_CONFIG_NAME_OLD(type, TrebleFreqHz) \
    DEFINE_FILTER_CONFIG_NAME_OLD(type, TrebleGaindB) \
    DEFINE_FILTER_CONFIG_NAME_OLD(type, MidFreqHz) \
    DEFINE_FILTER_CONFIG_NAME_OLD(type, MidGaindB) \
    DEFINE_FILTER_CONFIG_NAME_OLD(type, MidQ) \
    DEFINE_FILTER_CONFIG_NAME_OLD(type, VolInDB) \
    DEFINE_FILTER_CONFIG_NAME_OLD(type, EQEnable)

DECLARE_FILTER_CONFIG_NAMES(MicIn);
DECLARE_FILTER_CONFIG_NAMES(SpkOut);
DECLARE_FILTER_CONFIG_NAMES_OLD(MicIn);
DECLARE_FILTER_CONFIG_NAMES_OLD(SpkOut);

template<FilterConfiguration::Channel ChannelId>
FilterConfiguration::FilterChannel<ChannelId>::FilterChannel()
    : bassFreqHz(FilterConfiguration::FilterChannelConfigName<ChannelId>::GetBassFreqHzConfigName(), 100)
    , bassGaindB(FilterConfiguration::FilterChannelConfigName<ChannelId>::GetBassGaindBConfigName(), ((long)0)/10.0)
    , trebleFreqHz(FilterConfiguration::FilterChannelConfigName<ChannelId>::GetTrebleFreqHzConfigName(), 3000)
    , trebleGaindB(FilterConfiguration::FilterChannelConfigName<ChannelId>::GetTrebleGaindBConfigName(), ((long)0)/10.0)
    , midFreqHz(FilterConfiguration::FilterChannelConfigName<ChannelId>::GetMidFreqHzConfigName(), 1500)
    , midGainDB(FilterConfiguration::FilterChannelConfigName<ChannelId>::GetMidGaindBConfigName(), ((long)0)/10.0)
    , midQ(FilterConfiguration::FilterChannelConfigName<ChannelId>::GetMidQConfigName(), ((long)100)/100.0)
    , volInDB(FilterConfiguration::FilterChannelConfigName<ChannelId>::GetVolInDBConfigName(), ((long)0)/10.0)
    , eqEnable(FilterConfiguration::FilterChannelConfigName<ChannelId>::GetEQEnableConfigName(), false)
{
    // empty
}

template<FilterConfiguration::Channel ChannelId>
void FilterConfiguration::FilterChannel<ChannelId>::load(wxConfigBase* config)
{
    // Migration: these values were using incorrect data types, so we have to
    // move to new names in order to prevent Windows errors (for example:)
    //
    // 14:03:28: Registry value "HKCU\Software\freedv\Filter\MicInBassFreqHz" is not text (but of type DWORD)
    // 14:03:28: Registry value "HKCU\Software\freedv\Filter\MicInBassGaindB" is not text (but of type DWORD)
    // 14:03:28: Registry value "HKCU\Software\freedv\Filter\MicInTrebleFreqHz" is not text (but of type DWORD)
    // 14:03:28: Registry value "HKCU\Software\freedv\Filter\MicInTrebleGaindB" is not text (but of type DWORD)
    // 14:03:28: Registry value "HKCU\Software\freedv\Filter\MicInMidFreqHz" is not text (but of type DWORD)
    // 14:03:28: Registry value "HKCU\Software\freedv\Filter\MicInMidGaindB" is not text (but of type DWORD)
    // 14:03:28: Registry value "HKCU\Software\freedv\Filter\MicInMidQ" is not text (but of type DWORD)
    // 14:03:28: Registry value "HKCU\Software\freedv\Filter\MicInVolInDB" is not text (but of type DWORD)
        
    float oldBassFreqHz = (float)config->Read(
        FilterConfiguration::FilterChannelConfigName<ChannelId>::GetOldBassFreqHzConfigName(), 
        100);
    bassFreqHz.setDefaultVal(oldBassFreqHz);
    
    float oldBassGaindB = (float)config->Read(
        FilterConfiguration::FilterChannelConfigName<ChannelId>::GetOldBassGaindBConfigName(), 
        (long)0)/10.0;
    bassGaindB.setDefaultVal(oldBassGaindB);
    
    float oldTrebleFreqHz = (float)config->Read(
        FilterConfiguration::FilterChannelConfigName<ChannelId>::GetOldTrebleFreqHzConfigName(),
        3000);
    trebleFreqHz.setDefaultVal(oldTrebleFreqHz);
    
    float oldTrebleGaindB = (float)config->Read(
        FilterConfiguration::FilterChannelConfigName<ChannelId>::GetOldTrebleGaindBConfigName(), 
        (long)0)/10.0;
    trebleGaindB.setDefaultVal(oldTrebleGaindB);
    
    float oldMidFreqHz = (float)config->Read(
        FilterConfiguration::FilterChannelConfigName<ChannelId>::GetOldMidFreqHzConfigName(),
        1500);
    midFreqHz.setDefaultVal(oldMidFreqHz);
    
    float oldMidGaindB = (float)config->Read(
        FilterConfiguration::FilterChannelConfigName<ChannelId>::GetOldMidGaindBConfigName(),
        (long)0)/10.0;
    midGainDB.setDefaultVal(oldMidGaindB);
    
    float oldMidQ = (float)config->Read(
        FilterConfiguration::FilterChannelConfigName<ChannelId>::GetOldMidQConfigName(),
        (long)100)/100.0;
    midQ.setDefaultVal(oldMidQ);
    
    float oldVolInDB = (float)config->Read(
        FilterConfiguration::FilterChannelConfigName<ChannelId>::GetOldVolInDBConfigName(),
        (long)0)/10.0;
    volInDB.setDefaultVal(oldVolInDB);
    
    load_(config, bassFreqHz);
    load_(config, bassGaindB);
    load_(config, trebleFreqHz);
    load_(config, trebleGaindB);
    load_(config, midFreqHz);
    load_(config, midGainDB);
    load_(config, midQ);
    load_(config, volInDB);
    load_(config, eqEnable);
}

template<FilterConfiguration::Channel ChannelId>
void FilterConfiguration::FilterChannel<ChannelId>::save(wxConfigBase* config)
{
    save_(config, bassFreqHz);
    save_(config, bassGaindB);
    save_(config, trebleFreqHz);
    save_(config, trebleGaindB);
    save_(config, midFreqHz);
    save_(config, midGainDB);
    save_(config, midQ);
    save_(config, volInDB);
    save_(config, eqEnable);
}

#endif // FILTER_CONFIGURATION_H