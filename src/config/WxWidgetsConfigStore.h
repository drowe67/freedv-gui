//==========================================================================
// Name:            WxWidgetsConfigStore.h
// Purpose:         Implements wxWidgets-specific configuration handling
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

#ifndef WXWIDGETS_CONFIG_STORE_H
#define WXWIDGETS_CONFIG_STORE_H

#include <vector>
#include <wx/config.h>
#include "ConfigurationDataElement.h"

class WxWidgetsConfigStore
{
public:
    virtual ~WxWidgetsConfigStore() = default;
    
    virtual void load(wxConfigBase* config) = 0;
    virtual void save(wxConfigBase* config) = 0;
    
protected:
    template<typename UnderlyingDataType>
    void load_(wxConfigBase* config, ConfigurationDataElement<UnderlyingDataType>& configElement);
    
    template<typename UnderlyingDataType>
    void save_(wxConfigBase* config, ConfigurationDataElement<UnderlyingDataType>& configElement);
    
    wxString generateStringFromArray_(std::vector<wxString> const& vec);
    std::vector<wxString> generateArayFromString_(wxString const& str);
};

template<typename UnderlyingDataType>
void WxWidgetsConfigStore::load_(wxConfigBase* config, ConfigurationDataElement<UnderlyingDataType>& configElement)
{
    UnderlyingDataType val;
    config->Read(configElement.getElementName(), &val, configElement.getDefaultVal());
    configElement.setWithoutProcessing(val);
}

template<typename UnderlyingDataType>
void WxWidgetsConfigStore::save_(wxConfigBase* config, ConfigurationDataElement<UnderlyingDataType>& configElement)
{
    config->Write(configElement.getElementName(), configElement.getWithoutProcessing());
}

template<>
void WxWidgetsConfigStore::load_<unsigned int>(wxConfigBase* config, ConfigurationDataElement<unsigned int>& configElement);

// Special handling for loading and saving string arrays.
template<>
void WxWidgetsConfigStore::load_<std::vector<wxString> >(wxConfigBase* config, ConfigurationDataElement<std::vector<wxString> >& configElement);
template<>
void WxWidgetsConfigStore::save_<std::vector<wxString> >(wxConfigBase* config, ConfigurationDataElement<std::vector<wxString> >& configElement);

#endif // WXWIDGETS_CONFIG_STORE_H
