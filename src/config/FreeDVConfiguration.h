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

#include <wx/config.h>
#include "ConfigurationDataElement.h"

class FreeDVConfiguration
{
public:
    FreeDVConfiguration();
    virtual ~FreeDVConfiguration() = default;
    
    ConfigurationDataElement<bool> firstTimeUse;
    ConfigurationDataElement<bool> freedv2020Allowed;
    
    ConfigurationDataElement<long> mainWindowLeft;
    ConfigurationDataElement<long> mainWindowTop;
    ConfigurationDataElement<long> mainWindowWidth;
    ConfigurationDataElement<long> mainWindowHeight;
    
    ConfigurationDataElement<long> currentNotebookTab;
    
    ConfigurationDataElement<long> squelchActive;
    ConfigurationDataElement<long> squelchLevel;
    
    ConfigurationDataElement<wxString> soundCard1InDeviceName;
    ConfigurationDataElement<int> soundCard1InSampleRate;
    ConfigurationDataElement<wxString> soundCard1OutDeviceName;
    ConfigurationDataElement<int> soundCard1OutSampleRate;
    ConfigurationDataElement<wxString> soundCard2InDeviceName;
    ConfigurationDataElement<int> soundCard2InSampleRate;
    ConfigurationDataElement<wxString> soundCard2OutDeviceName;
    ConfigurationDataElement<int> soundCard2OutSampleRate;
    
    void load(wxConfigBase* config);
    void save(wxConfigBase* config);
    
private:
    template<typename UnderlyingDataType>
    void load_(wxConfigBase* config, ConfigurationDataElement<UnderlyingDataType>& configElement);
    
    template<typename UnderlyingDataType>
    void save_(wxConfigBase* config, ConfigurationDataElement<UnderlyingDataType>& configElement);
};

template<typename UnderlyingDataType>
void FreeDVConfiguration::load_(wxConfigBase* config, ConfigurationDataElement<UnderlyingDataType>& configElement)
{
    UnderlyingDataType val;
    config->Read(configElement.getElementName(), &val, configElement.getDefaultVal());
    configElement = val;
}

template<typename UnderlyingDataType>
void FreeDVConfiguration::save_(wxConfigBase* config, ConfigurationDataElement<UnderlyingDataType>& configElement)
{
    config->Write(configElement.getElementName(), (UnderlyingDataType)configElement);
}

#endif // FREEDV_CONFIGURATION_H