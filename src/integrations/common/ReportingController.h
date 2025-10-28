//=========================================================================
// Name:            ReportingController.h
// Purpose:         Controller responsible for reporting to FreeDV Reporter.
//
// Authors:         Mooneer Salem
// License:
//
//  All rights reserved.
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANYg_eoo_enqueued
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//=========================================================================

#ifndef REPORTING_CONTROLLER_H
#define REPORTING_CONTROLLER_H

#include <string>
#include "../util/ThreadedObject.h"
#include "../reporting/FreeDVReporter.h"

class ReportingController : public ThreadedObject
{
public:
    ReportingController(std::string softwareName, bool rxOnly = false);
    virtual ~ReportingController() = default;

    void updateRadioCallsign(std::string newCallsign);
    void updateRadioGridSquare(std::string newGridSquare);
    void reportCallsign(std::string callsign, char snr);
    void showSelf();
    void hideSelf();
    void changeFrequency(uint64_t freqHz);
    void transmit(bool transmit);

private:
    std::string softwareName_;
    FreeDVReporter* reporterConnection_;
    std::string currentGridSquare_;
    std::string radioCallsign_;
    bool userHidden_;
    uint64_t currentFreq_;
    bool rxOnly_;

    void updateReporterState_();
    std::string getVersionString_();
};

#endif // REPORTING_CONTROLLER_H
