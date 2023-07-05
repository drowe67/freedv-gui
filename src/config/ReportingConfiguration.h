//==========================================================================
// Name:            ReportingConfiguration.h
// Purpose:         Implements the reporting configuration for FreeDV
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

#ifndef REPORTING_CONFIGURATION_H
#define REPORTING_CONFIGURATION_H

#include "WxWidgetsConfigStore.h"
#include "ConfigurationDataElement.h"

class ReportingConfiguration : public WxWidgetsConfigStore
{
public:
    ReportingConfiguration();
    virtual ~ReportingConfiguration() = default;
    
    // Old format reporting (FEC-free text field)
    ConfigurationDataElement<wxString> reportingFreeTextString;
    
    ConfigurationDataElement<bool> reportingEnabled;
    ConfigurationDataElement<wxString> reportingCallsign;
    ConfigurationDataElement<wxString> reportingGridSquare;
    
    // NOTE: this needs special handling for load/save as it's a string on disk but
    // uint64_t inside the FreeDV application.
    ConfigurationDataElement<uint64_t> reportingFrequency; 
    
    ConfigurationDataElement<bool> pskReporterEnabled;
    
    ConfigurationDataElement<bool> freedvReporterEnabled;
    ConfigurationDataElement<wxString> freedvReporterHostname;
    
    ConfigurationDataElement<bool> useUTCForReporting;
    
    ConfigurationDataElement<std::vector<wxString> > reportingFrequencyList;
    
    virtual void load(wxConfigBase* config) override;
    virtual void save(wxConfigBase* config) override;
};

#endif // REPORTING_CONFIGURATION_H