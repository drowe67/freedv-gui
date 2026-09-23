//=========================================================================
// Name:            UsDataSegments.h
// Purpose:         Where US rules permit a data transmission.
//
// Authors:         FreeDV text messaging contributors
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

#ifndef TEXT_MESSAGING__US_DATA_SEGMENTS_H
#define TEXT_MESSAGING__US_DATA_SEGMENTS_H

#include <cstdint>
#include <cstdio>
#include <string>

namespace TextMessaging
{

// The chat's bursts are data emissions, and US rules authorise emissions by
// band segment (47 CFR 97.305(c)): below 30 MHz, these segments carry phone
// and image but not data, and they are where FreeDV voice is usually worked.
// Elsewhere in the MF and HF bands data is authorised; 160 m, 60 m and 30 m
// permit it throughout. The 75 m edge is 97.301's Region 2 allocation.
struct UsPhoneOnlySegment
{
    uint64_t lowHz;
    uint64_t highHz;
};

inline constexpr UsPhoneOnlySegment US_PHONE_ONLY_SEGMENTS[] = {
    {3600000, 4000000},   // 75 m
    {7125000, 7300000},   // 40 m
    {14150000, 14350000}, // 20 m
    {18110000, 18168000}, // 17 m
    {21200000, 21450000}, // 15 m
    {24930000, 24990000}, // 12 m
    {28300000, 29700000}, // 10 m
};

// The frequency known is the dial; the signal sits an audio offset away from
// it on either side depending on sideband. Anything within this of a phone
// segment is treated as in it.
constexpr uint64_t DATA_SEGMENT_MARGIN_HZ = 3000;

// Empty if US rules permit a data transmission at this dial frequency;
// otherwise why not, for the operator. A frequency of zero means FreeDV does
// not know where the radio is, and nothing is permitted until it does.
inline std::string usDataTransmitRestriction(uint64_t dialHz)
{
    if (dialHz == 0)
    {
        return "The operating frequency is not known, so text chat will not transmit. "
               "Enter it in the main window, or enable rig control.";
    }

    uint64_t lowest = dialHz > DATA_SEGMENT_MARGIN_HZ ? dialHz - DATA_SEGMENT_MARGIN_HZ : 0;
    uint64_t highest = dialHz + DATA_SEGMENT_MARGIN_HZ;

    for (const UsPhoneOnlySegment& segment : US_PHONE_ONLY_SEGMENTS)
    {
        if (highest < segment.lowHz || lowest > segment.highHz) continue;

        char reason[200];
        snprintf(reason, sizeof(reason),
                 "Text chat will not transmit on %.3f MHz: US rules permit data only outside "
                 "the %.3f-%.3f MHz phone segment (47 CFR 97.305).",
                 dialHz / 1e6, segment.lowHz / 1e6, segment.highHz / 1e6);
        return reason;
    }

    return "";
}

} // namespace TextMessaging

#endif // TEXT_MESSAGING__US_DATA_SEGMENTS_H
