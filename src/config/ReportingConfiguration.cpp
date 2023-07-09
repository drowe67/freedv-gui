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

#include <inttypes.h>

#include "../defines.h"
#include "ReportingConfiguration.h"

ReportingConfiguration::ReportingConfiguration()
    : reportingFreeTextString("/Data/CallSign", _(""))
        
    , reportingEnabled("/Reporting/Enable", false)
    , reportingCallsign("/Reporting/Callsign", _(""))
    , reportingGridSquare("/Reporting/GridSquare", _(""))
        
    , reportingFrequency("/Reporting/Frequency", 0)
        
    , pskReporterEnabled("/Reporting/PSKReporter/Enable", false)
        
    , freedvReporterEnabled("/Reporting/FreeDV/Enable", true)
    , freedvReporterHostname("/Reporting/FreeDV/Hostname", wxT(FREEDV_REPORTER_DEFAULT_HOSTNAME))
        
    , useUTCForReporting("/CallsignList/UseUTCTime", false)
{
    // empty
}

void ReportingConfiguration::load(wxConfigBase* config)
{
    // Migration: Save old parameters so that they can be copied over to the new locations.
    auto oldPskEnable = config->ReadBool(wxT("/PSKReporter/Enable"), false); 
    auto oldPskCallsign = config->Read(wxT("/PSKReporter/Callsign"), wxT(""));
    auto oldGridSquare = config->Read(wxT("/PSKReporter/GridSquare"), wxT(""));
    auto oldFreqStr = config->Read(wxT("/PSKReporter/FrequencyHzStr"), wxT("0"));
    reportingEnabled.setDefaultVal(oldPskEnable);
    reportingCallsign.setDefaultVal(oldPskCallsign);
    reportingGridSquare.setDefaultVal(oldGridSquare);
    
    load_(config, reportingFreeTextString);
 
    load_(config, reportingEnabled);
    load_(config, reportingCallsign);
    load_(config, reportingGridSquare);
    
    load_(config, pskReporterEnabled);
    
    load_(config, freedvReporterEnabled);
    load_(config, freedvReporterHostname);
    
    load_(config, useUTCForReporting);
    
    // Special load handling for reporting below.
    wxString freqStr = config->Read(reportingFrequency.getElementName(), oldFreqStr);
    reportingFrequency.setWithoutProcessing(atoll(freqStr.ToUTF8()));
}

void ReportingConfiguration::save(wxConfigBase* config)
{
    save_(config, reportingFreeTextString);
 
    save_(config, reportingEnabled);
    save_(config, reportingCallsign);
    save_(config, reportingGridSquare);
    
    save_(config, pskReporterEnabled);
    
    save_(config, freedvReporterEnabled);
    save_(config, freedvReporterHostname);
    
    save_(config, useUTCForReporting);
    
    // Special save handling for reporting below.
    wxString tempFreqStr = wxString::Format(wxT("%" PRIu64), reportingFrequency.getWithoutProcessing());
    config->Write(reportingFrequency.getElementName(), tempFreqStr);
}