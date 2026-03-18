//=========================================================================
// Name:            ReportingController.h
// Purpose:         Controller responsible for reporting to FreeDV Reporter.
//
// Authors:         Mooneer Salem
// License:
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// - Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//=========================================================================

#ifndef REPORTING_CONTROLLER_H
#define REPORTING_CONTROLLER_H

#include <string>
#include "../util/ThreadedObject.h"
#include "../util/ThreadedTimer.h"
#include "../reporting/FreeDVReporter.h"
#include "../reporting/pskreporter.h"

class ReportingController : public ThreadedObject
{
public:
    ReportingController(std::string softwareName, bool rxOnly = false, bool disableReporting = false);
    virtual ~ReportingController();

    void updateRadioCallsign(std::string const& newCallsign);
    void updateRadioGridSquare(std::string const& newGridSquare);
    void updateUserMessage(std::string const& newUserMessage);
    void reportCallsign(std::string const& callsign, char snr);
    void showSelf();
    void hideSelf();
    void changeFrequency(uint64_t freqHz);
    void transmit(bool transmit);

    bool isHidden();

private:
    std::string softwareName_;
    ThreadedTimer pskReporterSendTimer_;
    FreeDVReporter* freedvReporterConnection_;
    PskReporter* pskReporterConnection_;

    std::string currentGridSquare_;
    std::string radioCallsign_;
    std::string userMessage_;
    bool userHidden_;
    uint64_t currentFreq_;
    bool rxOnly_;
    bool reportingDisabled_;

    void updateReporterState_();
    std::string getVersionString_();
};

#endif // REPORTING_CONTROLLER_H
