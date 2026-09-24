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
// band segment (47 CFR 97.305(c)). This is the list of US amateur segments
// where data is authorised, with the band edges from 97.301(b) where the
// table says "entire band". Everything not listed is refused: the phone
// segments where FreeDV voice is usually worked, frequencies outside the
// amateur bands, and bands left out on purpose -- 2200 m and 630 m, which
// need notice to the utilities council before any operation (97.303(g)),
// and 1.25 m's 219-220 MHz, which is for fixed digital message forwarding
// only. It does not know the operator's licence class; staying within the
// operator's own privileges is still the operator's business.
struct UsDataSegment
{
    uint64_t lowHz;
    uint64_t highHz;
    const char* band;
};

inline constexpr UsDataSegment US_DATA_SEGMENTS[] = {
    {1800000, 2000000, "160 m"},
    {3500000, 3600000, "80 m"},
    {5351500, 5366500, "60 m"},
    {7000000, 7125000, "40 m"},
    {10100000, 10150000, "30 m"},
    {14000000, 14150000, "20 m"},
    {18068000, 18110000, "17 m"},
    {21000000, 21200000, "15 m"},
    {24890000, 24930000, "12 m"},
    {28000000, 28300000, "10 m"},
    {50100000, 54000000, "6 m"},
    {144100000, 148000000, "2 m"},
    {222000000, 225000000, "1.25 m"},
    {420000000, 450000000, "70 cm"},
    {902000000, 928000000, "33 cm"},
    {1240000000, 1300000000, "23 cm"},
};

// The frequency known is the dial; the signal sits an audio offset away from
// it on either side depending on sideband. So the dial has to be this far
// inside a segment for the transmission to be.
constexpr uint64_t DATA_SEGMENT_MARGIN_HZ = 3000;

// 60 m also permits data on four channels (97.303(h)), each only 2.8 kHz wide
// -- narrower than the margin -- and centred on these frequencies. A channel
// is used in upper sideband with the dial 1.5 kHz below its centre, which puts
// the chat signal, centred 1.5 kHz up the audio passband, on the centre; the
// dial has to be that close to right.
inline constexpr uint64_t US_60M_DATA_CHANNEL_CENTRES_HZ[] = {5332000, 5348000, 5373000, 5405000};
constexpr uint64_t CHANNEL_DIAL_BELOW_CENTRE_HZ = 1500;
constexpr uint64_t CHANNEL_DIAL_TOLERANCE_HZ = 100;

inline bool usDataTransmitPermitted(uint64_t dialHz)
{
    if (dialHz == 0) return false;

    for (const UsDataSegment& segment : US_DATA_SEGMENTS)
    {
        if (dialHz >= segment.lowHz + DATA_SEGMENT_MARGIN_HZ &&
            dialHz + DATA_SEGMENT_MARGIN_HZ <= segment.highHz)
        {
            return true;
        }
    }

    for (uint64_t centre : US_60M_DATA_CHANNEL_CENTRES_HZ)
    {
        uint64_t dial = centre - CHANNEL_DIAL_BELOW_CENTRE_HZ;
        uint64_t off = dialHz > dial ? dialHz - dial : dial - dialHz;
        if (off <= CHANNEL_DIAL_TOLERANCE_HZ) return true;
    }

    return false;
}

// Empty if US rules permit a data transmission at this dial frequency;
// otherwise why not, for the operator, naming the nearest segment where it
// would be. A frequency of zero means FreeDV does not know where the radio
// is, and nothing is permitted until it does.
inline std::string usDataTransmitRestriction(uint64_t dialHz)
{
    if (dialHz == 0)
    {
        return "The operating frequency is not known, so text chat will not transmit. "
               "Enter it in the main window, or enable rig control.";
    }

    if (usDataTransmitPermitted(dialHz)) return "";

    const UsDataSegment* nearest = nullptr;
    uint64_t nearestDistance = UINT64_MAX;
    for (const UsDataSegment& segment : US_DATA_SEGMENTS)
    {
        uint64_t distance = dialHz < segment.lowHz    ? segment.lowHz - dialHz
                            : dialHz > segment.highHz ? dialHz - segment.highHz
                                                      : 0;
        if (distance < nearestDistance)
        {
            nearest = &segment;
            nearestDistance = distance;
        }
    }

    char reason[240];
    snprintf(reason, sizeof(reason),
             "Text chat will not transmit on %.3f MHz: US rules permit data only in the amateur "
             "data segments (47 CFR 97.305), with 3 kHz to spare. The nearest is %.3f-%.3f MHz "
             "(%s).",
             dialHz / 1e6, nearest->lowHz / 1e6, nearest->highHz / 1e6, nearest->band);
    return reason;
}

} // namespace TextMessaging

#endif // TEXT_MESSAGING__US_DATA_SEGMENTS_H
