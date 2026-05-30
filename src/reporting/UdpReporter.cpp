//=========================================================================
// Name:            UdpReporter.cpp
// Purpose:         Reports received callsigns via UDP broadcast as JSON.
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

#include "UdpReporter.h"

#include <cinttypes>
#include <cstdlib>
#include <ctime>
#include <cstring>

#include "../3rdparty/yyjson/yyjson.h"
#include "../util/logging/ulog.h"

UdpReporter::UdpReporter(std::string address, int port)
    : address_(std::move(address))
    , port_(port)
    , currentFrequency_(0)
{
    // Open a send-only socket directed at the configured address/port.
    // Passing an empty bind host with the destination IP lets UdpHandler
    // pick the right address family (AF_INET vs AF_INET6) automatically.
    UdpHandler::open("", 0, address_.c_str(), port_).wait();
    log_info("UdpReporter: opened UDP socket to %s:%d", address_.c_str(), port_);
}

UdpReporter::~UdpReporter()
{
    UdpHandler::close().wait();
}

// ── IReporter overrides ──────────────────────────────────────────────────────

void UdpReporter::freqChange(uint64_t frequency)
{
    currentFrequency_ = frequency;
}

void UdpReporter::transmit(std::string /*mode*/, bool /*tx*/) {}
void UdpReporter::inAnalogMode(bool /*inAnalog*/) {}
void UdpReporter::send() {}

void UdpReporter::addReceiveRecord(std::string callsign, std::string mode,
                                   uint64_t frequency, signed char snr)
{
    // ── Build ISO-8601 UTC timestamp ─────────────────────────────────────────
    std::time_t now = std::time(nullptr);
    std::tm* utc = std::gmtime(&now); // NOLINT(concurrency-mt-unsafe)

    char timestampBuf[21]; // "YYYY-MM-DDTHH:MM:SSZ\0"
    std::strftime(timestampBuf, sizeof(timestampBuf), "%Y-%m-%dT%H:%M:%SZ", utc);

    // ── Serialise to JSON using yyjson ───────────────────────────────────────
    yyjson_mut_doc* doc = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val* root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_str( doc, root, "type",         "fdv_callsign");
    yyjson_mut_obj_add_int( doc, root, "version",      JSON_MESSAGE_VERSION);
    yyjson_mut_obj_add_str( doc, root, "timestamp",    timestampBuf);
    yyjson_mut_obj_add_str( doc, root, "callsign",     callsign.c_str());
    yyjson_mut_obj_add_str( doc, root, "mode",         mode.c_str());
    yyjson_mut_obj_add_int( doc, root, "snr",          snr);
    yyjson_mut_obj_add_uint(doc, root, "frequency_hz", frequency);

    size_t jsonLen = 0;
    char* jsonStr = yyjson_mut_write(doc, 0, &jsonLen);

    yyjson_mut_doc_free(doc);

    if (jsonStr == nullptr || jsonLen == 0)
    {
        log_warn("UdpReporter: failed to serialise JSON for callsign %s",
                 callsign.c_str());
        free(jsonStr);
        return;
    }

    log_info("UdpReporter: sending record for callsign %s, mode %s, "
             "frequency %" PRIu64 " to %s:%d",
             callsign.c_str(), mode.c_str(), frequency,
             address_.c_str(), port_);

    // Send and wait for completion before freeing the buffer.
    UdpHandler::send(address_.c_str(), port_, jsonStr,
                     static_cast<int>(jsonLen)).wait();

    free(jsonStr);
}
