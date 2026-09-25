//=========================================================================
// Name:            FrameCodecTest.cpp
// Purpose:         Pins the text messaging on air frame format.
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
#include <string>
#include <vector>

#include "../FrameCodec.h"

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

Frame makeMessageFrame()
{
    Frame frame;
    frame.type = FrameType::Message;
    frame.destinationCrc = FrameCodec::callsignCrc24("VK3ABC");
    frame.originCallsign = "W1AW";
    frame.airId = 0xBEEF;
    frame.fragmentIndex = 1;
    frame.fragmentCount = 3;
    const std::string text = "Hello from the chat window";
    frame.payload.assign(text.begin(), text.end());
    return frame;
}

Frame makePingFrame()
{
    Frame frame;
    frame.type = FrameType::Ping;
    frame.destinationCrc = FrameCodec::callsignCrc24("VK3ABC");
    frame.originCallsign = "W1AW";
    frame.airId = 0x1234;
    return frame;
}

// The wire format is a compatibility surface between stations running
// different builds, so the CRC and packing vectors below are pinned rather
// than recomputed by the test.
void testCallsignEncoding()
{
    CHECK(FrameCodec::callsignCrc24("W1AW") == 0xFC3980u);
    CHECK(FrameCodec::callsignCrc24("VK3ABC") == 0xFD1092u);
    CHECK(FrameCodec::callsignCrc24("CQ") == 0x4A8CF3u);

    // Normalization has to agree on both ends or the CRCs will not match.
    CHECK(FrameCodec::callsignCrc24("w1aw") == 0xFC3980u);
    CHECK(FrameCodec::callsignCrc24(" W1AW ") == 0xFC3980u);
    CHECK(FrameCodec::normalizeCallsign("vk3abc/p") == "VK3ABC/P");
    CHECK(FrameCodec::normalizeCallsign("!@#$") == "");

    const uint8_t expectedW1AW[PACKED_CALLSIGN_BYTES] = {0xC5, 0x09, 0x93, 0x36, 0x80, 0x00};
    const uint8_t expectedPortable[PACKED_CALLSIGN_BYTES] = {0xC1, 0xE1, 0x4B, 0xB5, 0xE3, 0x90};

    uint8_t packed[PACKED_CALLSIGN_BYTES];
    FrameCodec::packCallsign("W1AW", packed);
    for (int i = 0; i < PACKED_CALLSIGN_BYTES; i++) CHECK(packed[i] == expectedW1AW[i]);
    CHECK(FrameCodec::unpackCallsign(packed) == "W1AW");

    FrameCodec::packCallsign("VK3ABC/P", packed);
    for (int i = 0; i < PACKED_CALLSIGN_BYTES; i++) CHECK(packed[i] == expectedPortable[i]);
    CHECK(FrameCodec::unpackCallsign(packed) == "VK3ABC/P");

    // Longer than the air alphabet can carry: truncated, not corrupted.
    FrameCodec::packCallsign("ABCDEFGHIJKL", packed);
    CHECK(FrameCodec::unpackCallsign(packed) == "ABCDEFGHI");

    // A value above 40^9 cannot have come from packCallsign.
    const uint8_t outOfRange[PACKED_CALLSIGN_BYTES] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    CHECK(FrameCodec::unpackCallsign(outOfRange) == "");
}

void testRoundTrip()
{
    Frame original = makeMessageFrame();
    std::vector<uint8_t> encoded = FrameCodec::encode(original, TEXT_FRAME_BYTES);
    CHECK((int)encoded.size() == TEXT_FRAME_BYTES);

    Frame decoded;
    CHECK(FrameCodec::decode(encoded.data(), (int)encoded.size(), decoded));
    CHECK(decoded.type == original.type);
    CHECK(decoded.destinationCrc == original.destinationCrc);
    CHECK(decoded.originCallsign == "W1AW");
    CHECK(decoded.airId == 0xBEEF);
    CHECK(decoded.fragmentIndex == 1);
    CHECK(decoded.fragmentCount == 3);
    CHECK(decoded.payload == original.payload);

    // Signalling frames carry no text but must survive the same path, and a
    // ping acknowledgement's SNR byte is the only payload it has.
    Frame ping;
    ping.type = FrameType::Ping;
    ping.destinationCrc = FrameCodec::callsignCrc24("VK3ABC");
    ping.originCallsign = "W1AW";

    encoded = FrameCodec::encode(ping, SIGNALLING_FRAME_BYTES);
    CHECK((int)encoded.size() == SIGNALLING_FRAME_BYTES);
    CHECK(FrameCodec::decode(encoded.data(), (int)encoded.size(), decoded));
    CHECK(decoded.type == FrameType::Ping);
    CHECK(decoded.payload.empty());
    CHECK(decoded.fragmentCount == 1);

    // The SNR byte is the whole of a signalling frame's payload budget, so it
    // has to survive a frame that is otherwise full of header.
    Frame pong;
    pong.type = FrameType::PingAck;
    pong.destinationCrc = FrameCodec::callsignCrc24("VK3ABC");
    pong.originCallsign = "W1AW";
    pong.airId = 0x1234;
    pong.payload.assign(1, 0x2A);

    encoded = FrameCodec::encode(pong, SIGNALLING_FRAME_BYTES);
    CHECK((int)encoded.size() == SIGNALLING_FRAME_BYTES);
    CHECK(FrameCodec::decode(encoded.data(), (int)encoded.size(), decoded));
    CHECK(decoded.type == FrameType::PingAck);
    CHECK(decoded.airId == 0x1234);
    CHECK(decoded.originCallsign == "W1AW");
    CHECK(decoded.payload.size() == 1 && decoded.payload[0] == 0x2A);

    // A full fragment has to fit the modem frame exactly.
    Frame full = makeMessageFrame();
    full.payload.assign(TEXT_BYTES_PER_FRAGMENT, 'X');
    encoded = FrameCodec::encode(full, TEXT_FRAME_BYTES);
    CHECK((int)encoded.size() == TEXT_FRAME_BYTES);
    CHECK(FrameCodec::decode(encoded.data(), (int)encoded.size(), decoded));
    CHECK((int)decoded.payload.size() == TEXT_BYTES_PER_FRAGMENT);

    // Padding after a short payload must not leak into the text.
    Frame shortFrame = makeMessageFrame();
    shortFrame.payload.assign(1, 'Y');
    encoded = FrameCodec::encode(shortFrame, TEXT_FRAME_BYTES);
    CHECK(FrameCodec::decode(encoded.data(), (int)encoded.size(), decoded));
    CHECK(decoded.payload.size() == 1);
}

// How many bursts follow in the keying is how listeners know how long to
// leave the channel alone, and it is a wire format change, so the bytes are
// pinned: the high nibble of a text frame's fragment index byte, and the top
// bit of a signalling frame's type byte.
void testBurstsFollowing()
{
    Frame text = makeMessageFrame(); // fragment index 1
    text.burstsFollowing = 5;
    std::vector<uint8_t> encoded = FrameCodec::encode(text, TEXT_FRAME_BYTES);
    CHECK((int)encoded.size() == TEXT_FRAME_BYTES);
    CHECK(encoded[0] == 0x20);  // Message, no flag: text frames carry the count
    CHECK(encoded[12] == 0x51); // five following, fragment index 1

    Frame decoded;
    CHECK(FrameCodec::decode(encoded.data(), (int)encoded.size(), decoded));
    CHECK(decoded.fragmentIndex == 1);
    CHECK(decoded.fragmentCount == 3);
    CHECK(decoded.burstsFollowing == 5);

    // The last burst of a keying says none follow, and encodes exactly as a
    // frame did before the field existed.
    text.burstsFollowing = 0;
    encoded = FrameCodec::encode(text, TEXT_FRAME_BYTES);
    CHECK(encoded[12] == 0x01);
    CHECK(FrameCodec::decode(encoded.data(), (int)encoded.size(), decoded));
    CHECK(decoded.burstsFollowing == 0);

    // A signalling frame can only say whether any follow.
    Frame ping = makePingFrame();
    ping.burstsFollowing = 3;
    encoded = FrameCodec::encode(ping, SIGNALLING_FRAME_BYTES);
    CHECK(encoded[0] == 0x90);
    CHECK(FrameCodec::decode(encoded.data(), (int)encoded.size(), decoded));
    CHECK(decoded.type == FrameType::Ping);
    CHECK(decoded.burstsFollowing == 1);

    ping.burstsFollowing = 0;
    encoded = FrameCodec::encode(ping, SIGNALLING_FRAME_BYTES);
    CHECK(encoded[0] == 0x10);
    CHECK(FrameCodec::decode(encoded.data(), (int)encoded.size(), decoded));
    CHECK(decoded.burstsFollowing == 0);

    // More than the nibble holds cannot be sent.
    text.burstsFollowing = MAX_TEXT_BURSTS_FOLLOWING + 1;
    CHECK(FrameCodec::encode(text, TEXT_FRAME_BYTES).empty());

    // The type bit on a text frame is nothing we send, so it is corruption.
    text.burstsFollowing = 2;
    encoded = FrameCodec::encode(text, TEXT_FRAME_BYTES);
    encoded[0] |= 0x80;
    CHECK(!FrameCodec::decode(encoded.data(), (int)encoded.size(), decoded));
}

// A partial acknowledgement is a new frame type rather than a payload on the
// old one, so a build that predates it drops it instead of reading "message
// delivered". Its one payload byte is a bit per fragment received.
void testPartialAcknowledgement()
{
    Frame partial;
    partial.type = FrameType::MessagePartialAck;
    partial.destinationCrc = FrameCodec::callsignCrc24("W1AW");
    partial.originCallsign = "VK3ABC";
    partial.airId = 0xBEEF;
    partial.payload.assign(1, 0x05); // fragments 1 and 3 of three arrived

    std::vector<uint8_t> encoded = FrameCodec::encode(partial, SIGNALLING_FRAME_BYTES);
    CHECK((int)encoded.size() == SIGNALLING_FRAME_BYTES);
    CHECK(encoded[0] == 0x23);
    CHECK(encoded[12] == 1);    // payload length
    CHECK(encoded[13] == 0x05); // the mask

    Frame decoded;
    CHECK(FrameCodec::decode(encoded.data(), (int)encoded.size(), decoded));
    CHECK(decoded.type == FrameType::MessagePartialAck);
    CHECK(FrameCodec::isSignallingFrameType(decoded.type));
    CHECK(decoded.airId == 0xBEEF);
    CHECK(decoded.originCallsign == "VK3ABC");
    CHECK(decoded.payload.size() == 1 && decoded.payload[0] == 0x05);
}

void testEncodeRejections()
{
    Frame oversized = makeMessageFrame();
    oversized.payload.assign(TEXT_BYTES_PER_FRAGMENT + 1, 'X');
    CHECK(FrameCodec::encode(oversized, TEXT_FRAME_BYTES).empty());

    // A text frame's header alone is larger than a DATAC13 frame.
    CHECK(FrameCodec::encode(makeMessageFrame(), SIGNALLING_FRAME_BYTES).empty());

    // One byte is all a signalling frame has left after its header.
    Frame fatSignalling = makePingFrame();
    fatSignalling.payload.assign(SIGNALLING_PAYLOAD_BYTES + 1, 0x11);
    CHECK(FrameCodec::encode(fatSignalling, SIGNALLING_FRAME_BYTES).empty());

    // There is nowhere to record a fragment number in a signalling frame, so
    // one that claims to be fragmented must be refused rather than silently
    // sent as a single fragment.
    Frame fragmentedSignalling = makePingFrame();
    fragmentedSignalling.fragmentCount = 2;
    CHECK(FrameCodec::encode(fragmentedSignalling, SIGNALLING_FRAME_BYTES).empty());

    Frame noCallsign = makeMessageFrame();
    noCallsign.originCallsign = "";
    CHECK(FrameCodec::encode(noCallsign, TEXT_FRAME_BYTES).empty());

    Frame badFragments = makeMessageFrame();
    badFragments.fragmentIndex = 3;
    badFragments.fragmentCount = 3;
    CHECK(FrameCodec::encode(badFragments, TEXT_FRAME_BYTES).empty());

    Frame zeroCount = makeMessageFrame();
    zeroCount.fragmentCount = 0;
    CHECK(FrameCodec::encode(zeroCount, TEXT_FRAME_BYTES).empty());
}

void testDecodeRejections()
{
    std::vector<uint8_t> encoded = FrameCodec::encode(makeMessageFrame(), TEXT_FRAME_BYTES);
    Frame decoded;

    CHECK(!FrameCodec::decode(nullptr, TEXT_FRAME_BYTES, decoded));
    CHECK(!FrameCodec::decode(encoded.data(), TEXT_HEADER_BYTES - 1, decoded));

    std::vector<uint8_t> corrupted = encoded;
    corrupted[0] = 0x99; // not one of our frame types
    CHECK(!FrameCodec::decode(corrupted.data(), (int)corrupted.size(), decoded));

    corrupted = encoded;
    corrupted[TEXT_HEADER_BYTES - 1] = 0xFF; // payload length past the frame
    CHECK(!FrameCodec::decode(corrupted.data(), (int)corrupted.size(), decoded));

    corrupted = encoded;
    corrupted[13] = 0; // zero fragment count
    CHECK(!FrameCodec::decode(corrupted.data(), (int)corrupted.size(), decoded));

    corrupted = encoded;
    corrupted[12] = 9; // fragment index past the count
    CHECK(!FrameCodec::decode(corrupted.data(), (int)corrupted.size(), decoded));

    corrupted = encoded;
    corrupted[13] = MAX_FRAGMENTS_PER_MESSAGE + 1; // more fragments than we send
    CHECK(!FrameCodec::decode(corrupted.data(), (int)corrupted.size(), decoded));

    corrupted = encoded;
    for (int i = 0; i < PACKED_CALLSIGN_BYTES; i++) corrupted[4 + i] = 0xFF; // unpackable callsign
    CHECK(!FrameCodec::decode(corrupted.data(), (int)corrupted.size(), decoded));

    // A signalling frame is a byte shorter than its own header, so a DATAC13
    // frame truncated by one must not be read as if the length byte were there.
    std::vector<uint8_t> ping = FrameCodec::encode(makePingFrame(), SIGNALLING_FRAME_BYTES);
    CHECK(!ping.empty());
    CHECK(!FrameCodec::decode(ping.data(), SIGNALLING_HEADER_BYTES - 1, decoded));

    // Byte 13 is a text frame's fragment count, where zero is a reject, but in
    // a signalling frame it is payload. Reading the two layouts with the same
    // offsets would throw this ping away.
    corrupted = ping;
    corrupted[13] = 0;
    CHECK(FrameCodec::decode(corrupted.data(), (int)corrupted.size(), decoded));
    CHECK(decoded.type == FrameType::Ping);
    CHECK(decoded.fragmentCount == 1);
    CHECK(decoded.fragmentIndex == 0);
}

} // namespace

int main()
{
    testCallsignEncoding();
    testRoundTrip();
    testBurstsFollowing();
    testPartialAcknowledgement();
    testEncodeRejections();
    testDecodeRejections();

    if (failures > 0)
    {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }

    printf("all text messaging frame codec checks passed\n");
    return 0;
}
