//=========================================================================
// Name:            UsDataSegmentsTest.cpp
// Purpose:         Pins where US rules let text chat transmit.
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

#include <cstdio>
#include <cstdio>
#include <string>

#include "../UsDataSegments.h"

using namespace TextMessaging;

namespace
{

int failures = 0;

void check(bool condition, const char* what, int line)
{
    if (!condition)
    {
        failures++;
        fprintf(stderr, "FAIL (line %d): %s\n", line, what);
    }
}

#define CHECK(cond) check((cond), #cond, __LINE__)

bool permitted(uint64_t dialHz)
{
    bool allowed = usDataTransmitPermitted(dialHz);
    CHECK(allowed == usDataTransmitRestriction(dialHz).empty());
    return allowed;
}

// Where FreeDV voice is worked: phone segments, where data is not authorised.
void testPhoneSegmentsAreRefused()
{
    CHECK(!permitted(14236000)); // 20 m FreeDV
    CHECK(!permitted(7177000));  // 40 m FreeDV
    CHECK(!permitted(3625000));  // 75 m
    CHECK(!permitted(18118000)); // 17 m
    CHECK(!permitted(21313000)); // 15 m
    CHECK(!permitted(24933000)); // 12 m
    CHECK(!permitted(28330000)); // 10 m
    CHECK(!permitted(29600000)); // 10 m FM, still phone

    // The reason names the frequency, the rule, and where to go instead.
    std::string reason = usDataTransmitRestriction(14236000);
    CHECK(reason.find("14.236") != std::string::npos);
    CHECK(reason.find("97.305") != std::string::npos);
    CHECK(reason.find("14.000-14.150") != std::string::npos);
}

// Outside every amateur band is refused: the guard lists where data is
// permitted, and anything it does not list is not. It once listed only the
// phone segments, and 12.312 MHz passed.
void testOutsideTheAmateurBandsIsRefused()
{
    CHECK(!permitted(12312000));
    CHECK(!permitted(13000000));
    CHECK(!permitted(1500000));
    CHECK(!permitted(30000000));
    CHECK(!permitted(100000000));
    CHECK(!permitted(2400000000ULL));

    // Bands left out on purpose.
    CHECK(!permitted(137000));    // 2200 m: prior notice required
    CHECK(!permitted(475000));    // 630 m: prior notice required
    CHECK(!permitted(219500000)); // 1.25 m, 219-220 MHz: message forwarding only

    std::string reason = usDataTransmitRestriction(12312000);
    CHECK(reason.find("12.312") != std::string::npos);
    CHECK(reason.find("14.000-14.150") != std::string::npos); // nearer than 30 m
}

// The data segments, and the bands that permit data throughout.
void testDataSegmentsArePermitted()
{
    CHECK(permitted(1840000));    // 160 m
    CHECK(permitted(3573000));    // 80 m
    CHECK(permitted(5357000));    // 60 m segment
    CHECK(permitted(7074000));    // 40 m
    CHECK(permitted(7110000));    // 40 m, 7.100-7.125
    CHECK(permitted(10136000));   // 30 m
    CHECK(permitted(14074000));   // 20 m
    CHECK(permitted(18100000));   // 17 m
    CHECK(permitted(21074000));   // 15 m
    CHECK(permitted(24915000));   // 12 m
    CHECK(permitted(28074000));   // 10 m
    CHECK(permitted(50313000));   // 6 m
    CHECK(permitted(144174000));  // 2 m
    CHECK(permitted(223500000));  // 1.25 m
    CHECK(permitted(432100000));  // 70 cm
    CHECK(permitted(903100000));  // 33 cm
    CHECK(permitted(1296100000)); // 23 cm
}

// The dial is not where the signal is: the dial has to be the margin inside
// a segment, at both ends, for the signal to be inside it.
void testTheMarginInsideEachSegment()
{
    for (const UsDataSegment& segment : US_DATA_SEGMENTS)
    {
        CHECK(permitted(segment.lowHz + DATA_SEGMENT_MARGIN_HZ));
        CHECK(permitted(segment.highHz - DATA_SEGMENT_MARGIN_HZ));
        CHECK(!permitted(segment.lowHz + DATA_SEGMENT_MARGIN_HZ - 1));
        CHECK(!permitted(segment.highHz - DATA_SEGMENT_MARGIN_HZ + 1));
        CHECK(!permitted(segment.lowHz));
        CHECK(!permitted(segment.highHz));
    }

    // 14.148 MHz USB puts the signal around 14.1495, right against the edge.
    CHECK(!permitted(14148000));
    CHECK(permitted(14147000));
}

// The four 60 m channels are narrower than the margin, so each is permitted
// only at its standard USB dial, 1.5 kHz below its centre, give or take a
// little.
void testSixtyMetreChannels()
{
    for (uint64_t centre : US_60M_DATA_CHANNEL_CENTRES_HZ)
    {
        uint64_t dial = centre - CHANNEL_DIAL_BELOW_CENTRE_HZ;
        CHECK(permitted(dial));
        CHECK(permitted(dial - CHANNEL_DIAL_TOLERANCE_HZ));
        CHECK(permitted(dial + CHANNEL_DIAL_TOLERANCE_HZ));
        CHECK(!permitted(dial - CHANNEL_DIAL_TOLERANCE_HZ - 1));
        CHECK(!permitted(dial + CHANNEL_DIAL_TOLERANCE_HZ + 1));
        CHECK(!permitted(centre)); // the dial on the centre puts the signal off it
    }

    CHECK(permitted(5330500));  // 5.332 MHz channel
    CHECK(!permitted(5340000)); // between channels
}

// Not knowing the frequency is not permission.
void testUnknownFrequencyIsRefused()
{
    CHECK(!permitted(0));
    CHECK(usDataTransmitRestriction(0).find("not known") != std::string::npos);
}

} // namespace

int main()
{
    testPhoneSegmentsAreRefused();
    testOutsideTheAmateurBandsIsRefused();
    testDataSegmentsArePermitted();
    testTheMarginInsideEachSegment();
    testSixtyMetreChannels();
    testUnknownFrequencyIsRefused();

    if (failures > 0)
    {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }

    printf("all US data segment checks passed\n");
    return 0;
}
