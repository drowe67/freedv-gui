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
#include <wx/numformatter.h>
#include "WxWidgetsConfigStore.h"

template<>
void WxWidgetsConfigStore::load_<unsigned int>(wxConfigBase* config, ConfigurationDataElement<unsigned int>& configElement)
{
    long val;
    config->Read(configElement.getElementName(), &val, (long)configElement.getDefaultVal());
    configElement.setWithoutProcessing((unsigned int)val);
}

template<>
void WxWidgetsConfigStore::load_<std::vector<int> >(wxConfigBase* config, ConfigurationDataElement<std::vector<int> >& configElement)
{
    wxString val;
    wxString defaultVal = generateStringFromArray_(configElement.getDefaultVal());
    
    config->Read(configElement.getElementName(), &val, defaultVal);
    configElement.setWithoutProcessing(generateNumArrayFromString_(val));
}

template<>
void WxWidgetsConfigStore::save_<std::vector<int> >(wxConfigBase* config, ConfigurationDataElement<std::vector<int> >& configElement)
{
    wxString val = generateStringFromArray_(configElement.getWithoutProcessing());
    config->Write(configElement.getElementName(), val);
}

template<>
void WxWidgetsConfigStore::load_<std::vector<bool> >(wxConfigBase* config, ConfigurationDataElement<std::vector<bool> >& configElement)
{
    wxString val;
    wxString defaultVal = generateStringFromArray_(configElement.getDefaultVal());
    
    config->Read(configElement.getElementName(), &val, defaultVal);
    configElement.setWithoutProcessing(generateBoolArrayFromString_(val));
}

template<>
void WxWidgetsConfigStore::save_<std::vector<bool> >(wxConfigBase* config, ConfigurationDataElement<std::vector<bool> >& configElement)
{
    wxString val = generateStringFromArray_(configElement.getWithoutProcessing());
    config->Write(configElement.getElementName(), val);
}

/* Note: for string arrays, we're treating them as a list of strings separated by commas. */

template<>
void WxWidgetsConfigStore::load_<std::vector<wxString> >(wxConfigBase* config, ConfigurationDataElement<std::vector<wxString> >& configElement)
{
    wxString val;
    wxString defaultVal = generateStringFromArray_(configElement.getDefaultVal());
    
    config->Read(configElement.getElementName(), &val, defaultVal);
    configElement.setWithoutProcessing(generateStrArrayFromString_(val));
}

template<>
void WxWidgetsConfigStore::save_<std::vector<wxString> >(wxConfigBase* config, ConfigurationDataElement<std::vector<wxString> >& configElement)
{
    wxString val = generateStringFromArray_(configElement.getWithoutProcessing());
    config->Write(configElement.getElementName(), val);
}

wxString WxWidgetsConfigStore::generateStringFromArray_(std::vector<int> const& vec)
{
    wxString rv = "";
    
    int count = vec.size();
    for (auto& item : vec)
    {
        wxString numAsString = wxNumberFormatter::ToString((long)item, wxNumberFormatter::Style_None);
        rv += numAsString;
        count--;
        
        if (count > 0)
        {
            rv += ",";
        }
    }
    
    return rv;
}

wxString WxWidgetsConfigStore::generateStringFromArray_(std::vector<bool> const& vec)
{
    std::vector<int> tmpVec;
    tmpVec.reserve(vec.size());
    for (auto v : vec)
    {
        tmpVec.push_back(v ? 1 : 0);
    }
    return generateStringFromArray_(tmpVec);
}

std::vector<bool> WxWidgetsConfigStore::generateBoolArrayFromString_(wxString const& str)
{
    std::vector<int> tmpVec = generateNumArrayFromString_(str);
    std::vector<bool> rv;
    
    rv.reserve(tmpVec.size());
    for (auto& v : tmpVec)
    {
        rv.push_back(v == 1 ? true : false);
    }

    return rv;
}

std::vector<int> WxWidgetsConfigStore::generateNumArrayFromString_(wxString const& str)
{
    std::vector<int> rv;
    
    wxStringTokenizer tokenizer(str, ",");
    while ( tokenizer.HasMoreTokens() )
    {
        wxString token = tokenizer.GetNextToken();
        long tmp = 0;
        wxNumberFormatter::FromString(token, &tmp);
        rv.push_back((int)tmp);
    }
    
    return rv;
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

std::vector<wxString> WxWidgetsConfigStore::generateStrArrayFromString_(wxString const& str)
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
