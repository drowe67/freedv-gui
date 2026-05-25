//=========================================================================
// Name:            CsvReporter.cpp
// Purpose:         Reports received callsigns to a CSV file.
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

#include <cassert>
#include <cstdlib>
#include <cinttypes>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "CsvReporter.h"
#include "../util/logging/ulog.h"

CsvReporter::CsvReporter(std::string const& filename)
    : file_(filename, std::ios::app)
{
    if (!file_.is_open())
    {
        log_warn("CsvReporter: could not open log file %s", filename.c_str());
        return;
    }

    log_info("CsvReporter: opening log file %s", filename.c_str());

    // Write header only when creating a new (empty) file.
    if (file_.tellp() == 0)
    {
        file_ << "date,time,callsign,mode,frequency_hz,snr_db\n";
    }
}

void CsvReporter::freqChange(uint64_t) {}
void CsvReporter::transmit(std::string, bool) {}
void CsvReporter::inAnalogMode(bool) {}
void CsvReporter::send() {}

void CsvReporter::addReceiveRecord(std::string callsign, std::string mode, uint64_t frequency, signed char snr)
{
    if (!file_.is_open())
    {
        return;
    }

    int snrInt = static_cast<int>(snr);
    std::time_t now = std::time(nullptr);
    struct tm* curTime = (struct tm*)malloc(sizeof(struct tm));
    assert(curTime != NULL);
#if defined(_WIN32)
    gmtime_s(curTime, &now);
#else
    gmtime_r(&now, curTime);
#endif // defined(_WIN32) 

    char dateBuf[100]; // YYYY-MM-DD\0
    char timeBuf[100];  // HH:MM:SS\0
    std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", curTime);
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", curTime);

    free(curTime);

    log_info("CsvReporter: adding record for callsign %s, mode %s, frequency %" PRIu64 ", SNR %d",
             callsign.c_str(), mode.c_str(), frequency, snrInt);

    file_ << dateBuf << ","
          << timeBuf << ","
          << callsign << ","
          << mode << ","
          << frequency << ","
          << snrInt << "\n";
    file_.flush();
}
