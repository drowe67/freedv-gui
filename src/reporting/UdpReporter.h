#ifndef UDP_REPORTER_H
#define UDP_REPORTER_H

//=========================================================================
// Name:            UdpReporter.h
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

#include <string>
#include "IReporter.h"
#include "../util/UdpHandler.h"

class UdpReporter : protected UdpHandler, public IReporter
{
public:
    // address: IPv4 or IPv6 address to send UDP datagrams to
    // port:    UDP destination port number
    UdpReporter(std::string address, int port);
    virtual ~UdpReporter();

    virtual void freqChange(uint64_t frequency) override;
    virtual void transmit(std::string mode, bool tx) override;
    virtual void inAnalogMode(bool inAnalog) override;

    // Serialises a received callsign as a JSON UDP datagram and sends it.
    virtual void addReceiveRecord(std::string callsign, std::string mode,
                                  uint64_t frequency, signed char snr) override;

    virtual void send() override;

protected:
    // We only send — ignore anything received on the socket.
    virtual void onReceive_(const char*, int, char*, int) override {}

private:
    std::string address_;
    int         port_;
    uint64_t    currentFrequency_;

    static constexpr int JSON_MESSAGE_VERSION = 1;
};

#endif // UDP_REPORTER_H
