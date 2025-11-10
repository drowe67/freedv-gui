//==========================================================================
// Name:            WxWidgetsConfigStore.cpp
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

#include <wx/tokenzr.h>
#include "WxWidgetsConfigStore.h"

template<>
void WxWidgetsConfigStore::load_<unsigned int>(wxConfigBase* config, ConfigurationDataElement<unsigned int>& configElement)
{
    long val;
    config->Read(configElement.getElementName(), &val, (long)configElement.getDefaultVal());
    configElement.setWithoutProcessing((unsigned int)val);
}

/* Note: for string arrays, we're treating them as a list of strings separated by commas. */

template<>
void WxWidgetsConfigStore::load_<std::vector<wxString> >(wxConfigBase* config, ConfigurationDataElement<std::vector<wxString> >& configElement)
{
    wxString val;
    wxString defaultVal = generateStringFromArray_(configElement.getDefaultVal());
    
    config->Read(configElement.getElementName(), &val, defaultVal);
    configElement.setWithoutProcessing(generateArayFromString_(val));
}

template<>
void WxWidgetsConfigStore::save_<std::vector<wxString> >(wxConfigBase* config, ConfigurationDataElement<std::vector<wxString> >& configElement)
{
    wxString val = generateStringFromArray_(configElement.getWithoutProcessing());
    config->Write(configElement.getElementName(), val);
}

wxString WxWidgetsConfigStore::generateStringFromArray_(std::vector<wxString> const& vec)
{
    wxString rv = "";
    
    int count = vec.size();
    for (auto& item : vec)
    {
        rv += item;
        count--;
        
        if (count > 0)
        {
            rv += ",";
        }
    }
    
    return rv;
}

std::vector<wxString> WxWidgetsConfigStore::generateArayFromString_(wxString const& str)
{
    std::vector<wxString> rv;
    
    wxStringTokenizer tokenizer(str, ",");
    while ( tokenizer.HasMoreTokens() )
    {
        wxString token = tokenizer.GetNextToken();
        rv.push_back(token);
    }
    
    return rv;
}
