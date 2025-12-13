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

#include <wx/tokenzr.h>
#include <wx/numformatter.h>
#include <inttypes.h>

#include "../defines.h"
#include "reporting/FreeDVReporter.h"
#include "ReportingConfiguration.h"

ReportingConfiguration::ReportingConfiguration()
    : reportingFreeTextString("/Data/CallSign", _(""))
        
    , reportingEnabled("/Reporting/Enable", false)
    , reportingCallsign("/Reporting/Callsign", _(""))
    , reportingGridSquare("/Reporting/GridSquare", _(""))
        
    , reportingFrequency("/Reporting/Frequency", 0)
        
    , manualFrequencyReporting("/Reporting/ManualFrequencyReporting", false)
        
    , pskReporterEnabled("/Reporting/PSKReporter/Enable", true)
        
    , freedvReporterEnabled("/Reporting/FreeDV/Enable", true)
    , freedvReporterHostname("/Reporting/FreeDV/Hostname", wxT(FREEDV_REPORTER_DEFAULT_HOSTNAME))
    , freedvReporterBandFilter("/Reporting/FreeDV/CurrentBandFilter", 0)
    , useMetricDistances("/Reporting/FreeDV/UseMetricDistances", true)
    , freedvReporterBandFilterTracksFrequency("/Reporting/FreeDV/BandFilterTracksFrequency", false)
    , freedvReporterForceReceiveOnly("/Reporting/FreeDV/ForceReceiveOnly", false)
    , freedvReporterBandFilterTracksFreqBand("/Reporting/FreeDV/BandFilterTracking/TracksFreqBand", true)
    , freedvReporterBandFilterTracksExactFreq("/Reporting/FreeDV/BandFilterTracking/TracksExactFreq", false)
    , freedvReporterStatusText("/Reporting/FreeDV/StatusText", _(""))
    , freedvReporterRecentStatusTexts("/Reporting/FreeDV/RecentStatusTexts", {})
        
    , udpReportingEnabled("/Reporting/UDP/Enable", false)
    , udpReportingHostname("/Reporting/UDP/Hostname", wxT("127.0.0.1"))
    , udpReportingPort("/Reporting/UDP/Port", 2237)
        
    , useUTCForReporting("/CallsignList/UseUTCTime", false)
        
    , reportingFrequencyList("/Reporting/FrequencyList", {
        _("1.8700"),
        _("3.6250"),
        _("3.6430"),
        _("3.6930"),
        _("3.6970"),
        _("3.8030"),
        _("5.4035"),
        _("5.3665"),
        _("5.3685"),
        _("7.1770"),
        _("7.1970"),
        _("14.2360"),
        _("14.2400"),
        _("18.1180"),
        _("21.3130"),
        _("24.9330"),
        _("28.3300"),
        _("28.7200"),
        _("10489.6400"),
    })
    , freedvReporterTxRowBackgroundColor("/Reporting/FreeDV/TxRowBackgroundColor", "#fc4500")
    , freedvReporterTxRowForegroundColor("/Reporting/FreeDV/TxRowForegroundColor", "#000000")
    , freedvReporterRxRowBackgroundColor("/Reporting/FreeDV/RxRowBackgroundColor", "#379baf")
    , freedvReporterRxRowForegroundColor("/Reporting/FreeDV/RxRowForegroundColor", "#000000")
    , freedvReporterMsgRowBackgroundColor("/Reporting/FreeDV/MsgRowBackgroundColor", "#E58BE5")
    , freedvReporterMsgRowForegroundColor("/Reporting/FreeDV/MsgRowForegroundColor", "#000000")
        
    , reportingFrequencyAsKhz("/Reporting/FrequencyAsKHz", false)
    , reportingDirectionAsCardinal("/Reporting/DirectionAsCardinal", false)
{
    // Special handling for the frequency list to properly handle locales
    reportingFrequencyList.setLoadProcessor([this](std::vector<wxString> const& list) {
        std::vector<wxString> newList;
        for (auto& val : list)
        {
            // Frequencies are unfortunately saved in US format (legacy behavior). We need 
            // to manually parse and convert to Hz, then output MHz values in the user's current
            // locale.
            wxStringTokenizer tok(val, ".");
            wxString wholeNumber = tok.GetNextToken();
            wxString fraction = "0";
            if (tok.HasMoreTokens())
            {
                fraction = tok.GetNextToken();
            }

            if (fraction.Length() < 6)
            {
                fraction += wxString('0', 6 - fraction.Length());
            }

            long long hz = 0;
            long long mhz = 0;
            wholeNumber.ToLongLong(&mhz);
            fraction.ToLongLong(&hz);
            uint64_t freq = mhz * 1000000 + hz;

            if (reportingFrequencyAsKhz)
            {
                double khzFloat = freq / 1000.0;
                newList.push_back(wxNumberFormatter::ToString(khzFloat, 1));
            }
            else
            {
                double mhzFloat = freq / 1000000.0;
                newList.push_back(wxNumberFormatter::ToString(mhzFloat, 4));
            }
        }

        return newList;
    });

    reportingFrequencyList.setSaveProcessor([this](std::vector<wxString> const& list) {
        std::vector<wxString> newList;
        for (auto& val : list)
        {
            // Frequencies are unfortunately saved in US format (legacy behavior). We need 
            // to manually parse and convert to Hz, then output MHz values in US format.
            double mhz = 0.0;
            wxNumberFormatter::FromString(val, &mhz);
            
            if (reportingFrequencyAsKhz)
            {
                // Frequencies are in kHz, so divide one more time to get MHz.
                mhz /= 1000.0;
            }

            uint64_t hz = mhz * 1000000;
            uint64_t mhzInt = hz / 1000000;
            hz = hz % 1000000;

            wxString newVal = wxString::Format("%" PRIu64 ".%06" PRIu64, mhzInt, hz);
            newList.push_back(newVal);
        }
        return newList;
    });
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
    load_(config, freedvReporterBandFilter);
    load_(config, useMetricDistances);
    load_(config, freedvReporterBandFilterTracksFrequency);
    load_(config, freedvReporterForceReceiveOnly);
    load_(config, freedvReporterBandFilterTracksFreqBand);
    load_(config, freedvReporterBandFilterTracksExactFreq);
    load_(config, freedvReporterStatusText);
    load_(config, freedvReporterRecentStatusTexts);
    
    load_(config, udpReportingEnabled);
    load_(config, udpReportingHostname);
    load_(config, udpReportingPort);

    load_(config, useUTCForReporting);
    
    // Note: this needs to be loaded before the frequency list so that
    // we get the values formatted as kHz (if so configured).
    load_(config, reportingFrequencyAsKhz);
    
    load_(config, reportingFrequencyList);
    
    load_(config, manualFrequencyReporting);

    load_(config, freedvReporterTxRowBackgroundColor);
    load_(config, freedvReporterTxRowForegroundColor);
    load_(config, freedvReporterRxRowBackgroundColor);
    load_(config, freedvReporterRxRowForegroundColor);
    load_(config, freedvReporterMsgRowBackgroundColor);
    load_(config, freedvReporterMsgRowForegroundColor);

    load_(config, reportingDirectionAsCardinal);

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
    save_(config, freedvReporterBandFilter);
    save_(config, useMetricDistances);
    save_(config, freedvReporterBandFilterTracksFrequency);
    save_(config, freedvReporterForceReceiveOnly);
    save_(config, freedvReporterBandFilterTracksFreqBand);
    save_(config, freedvReporterBandFilterTracksExactFreq);
    save_(config, freedvReporterStatusText);
    save_(config, freedvReporterRecentStatusTexts);
    
    save_(config, udpReportingEnabled);
    save_(config, udpReportingHostname);
    save_(config, udpReportingPort);
    
    save_(config, useUTCForReporting);
    
    save_(config, reportingFrequencyAsKhz);
    save_(config, reportingFrequencyList);
    
    save_(config, manualFrequencyReporting);

    save_(config, freedvReporterTxRowBackgroundColor);
    save_(config, freedvReporterTxRowForegroundColor);
    save_(config, freedvReporterRxRowBackgroundColor);
    save_(config, freedvReporterRxRowForegroundColor);
    save_(config, freedvReporterMsgRowBackgroundColor);
    save_(config, freedvReporterMsgRowForegroundColor);

    save_(config, reportingDirectionAsCardinal);
    
    // Special save handling for reporting below.
    wxString tempFreqStr = wxString::Format(wxT("%" PRIu64), reportingFrequency.getWithoutProcessing());
    config->Write(reportingFrequency.getElementName(), tempFreqStr);
}
