//=========================================================================
// Name:            FrameCodec.cpp
// Purpose:         Packs and parses text messaging frames for the modem.
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

#include "FrameCodec.h"

#include <cctype>
#include <cstring>

namespace TextMessaging
{

namespace
{

// Air alphabet. Index zero is the pad character, so a short callsign packs to
// the same value whichever end does the packing.
const char* const BASE40_ALPHABET = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-/.";
constexpr int BASE40_SIZE = 40;

// CRC-24/OPENPGP, the same parameters FreeDATA uses for its callsign CRCs.
constexpr uint32_t CRC24_INIT = 0xB704CEu;
constexpr uint32_t CRC24_POLY = 0x864CFBu;

int base40Index(char c)
{
    for (int i = 0; i < BASE40_SIZE; i++)
    {
        if (BASE40_ALPHABET[i] == c) return i;
    }
    return -1;
}

// Offsets of each header field, in the order documented in
// TextMessagingTypes.h. Everything up to and including the message ID is
// common to both frame layouts; the fragment fields exist only in text frames,
// and the payload length byte is the last header byte either way.
constexpr int OFFSET_TYPE = 0;
constexpr int OFFSET_DEST_CRC = 1;
constexpr int OFFSET_ORIGIN_CALLSIGN = 4;
constexpr int OFFSET_AIR_ID = 10;
constexpr int OFFSET_FRAGMENT_INDEX = 12;  // text frames only
constexpr int OFFSET_FRAGMENT_COUNT = 13;  // text frames only
constexpr int OFFSET_SIGNALLING_PAYLOAD_LENGTH = 12;
constexpr int OFFSET_TEXT_PAYLOAD_LENGTH = 14;

// Bursts still to come in the keying. Fragment numbers never reach 16, so a
// text frame's fragment index byte has a spare high nibble for the count.
// A signalling frame has no spare byte, but frame type values stay below
// 0x80, which leaves the top bit of the type byte to say "more follows".
constexpr uint8_t TYPE_MORE_FOLLOWS = 0x80;
constexpr uint8_t TYPE_VALUE_MASK = 0x7F;
constexpr uint8_t FRAGMENT_INDEX_MASK = 0x0F;
constexpr int BURSTS_FOLLOWING_SHIFT = 4;

static_assert(MAX_FRAGMENTS_PER_MESSAGE <= FRAGMENT_INDEX_MASK + 1,
              "fragment index no longer leaves room for the bursts following count");
static_assert(MAX_TEXT_BURSTS_FOLLOWING == (0xFF >> BURSTS_FOLLOWING_SHIFT),
              "bursts following count and its nibble disagree");
static_assert(((uint8_t)FrameType::Ping & TYPE_MORE_FOLLOWS) == 0 &&
                  ((uint8_t)FrameType::PingAck & TYPE_MORE_FOLLOWS) == 0 &&
                  ((uint8_t)FrameType::Message & TYPE_MORE_FOLLOWS) == 0 &&
                  ((uint8_t)FrameType::MessageAck & TYPE_MORE_FOLLOWS) == 0 &&
                  ((uint8_t)FrameType::Broadcast & TYPE_MORE_FOLLOWS) == 0 &&
                  ((uint8_t)FrameType::MessagePartialAck & TYPE_MORE_FOLLOWS) == 0,
              "a frame type value collides with the more-follows bit");

static_assert(OFFSET_AIR_ID == OFFSET_ORIGIN_CALLSIGN + PACKED_CALLSIGN_BYTES,
              "packed callsign does not fit its header field");
static_assert(OFFSET_SIGNALLING_PAYLOAD_LENGTH + 1 == SIGNALLING_HEADER_BYTES,
              "signalling header field offsets and SIGNALLING_HEADER_BYTES disagree");
static_assert(OFFSET_TEXT_PAYLOAD_LENGTH + 1 == TEXT_HEADER_BYTES,
              "text header field offsets and TEXT_HEADER_BYTES disagree");
static_assert(OFFSET_FRAGMENT_COUNT + 1 == OFFSET_TEXT_PAYLOAD_LENGTH,
              "fragment fields must sit between the message ID and the length byte");

// The offset of the payload length byte, which is the only header field whose
// position depends on the layout.
int payloadLengthOffset(FrameType type)
{
    return FrameCodec::isSignallingFrameType(type) ? OFFSET_SIGNALLING_PAYLOAD_LENGTH
                                                   : OFFSET_TEXT_PAYLOAD_LENGTH;
}

} // namespace

std::string FrameCodec::normalizeCallsign(const std::string& callsign)
{
    std::string result;
    result.reserve(callsign.size());

    for (char c : callsign)
    {
        char upper = (char)std::toupper((unsigned char)c);
        if (upper == ' ') continue;
        if (base40Index(upper) > 0) result += upper;
    }

    return result;
}

uint32_t FrameCodec::callsignCrc24(const std::string& callsign)
{
    std::string normalized = normalizeCallsign(callsign);
    uint32_t crc = CRC24_INIT;

    for (char c : normalized)
    {
        crc ^= ((uint32_t)(unsigned char)c) << 16;
        for (int bit = 0; bit < 8; bit++)
        {
            crc <<= 1;
            if (crc & 0x1000000u) crc ^= CRC24_POLY;
        }
    }

    return crc & 0xFFFFFFu;
}

void FrameCodec::packCallsign(const std::string& callsign, uint8_t* out)
{
    std::string normalized = normalizeCallsign(callsign);
    if ((int)normalized.size() > MAX_PACKED_CALLSIGN_CHARS)
    {
        normalized.resize(MAX_PACKED_CALLSIGN_CHARS);
    }

    // Left aligned and padded with the zero symbol, so unpacking gives the
    // callsign back with trailing pad characters that are easy to strip.
    uint64_t packed = 0;
    for (int i = 0; i < MAX_PACKED_CALLSIGN_CHARS; i++)
    {
        int index = i < (int)normalized.size() ? base40Index(normalized[i]) : 0;
        if (index < 0) index = 0;
        packed = packed * BASE40_SIZE + (uint64_t)index;
    }

    for (int i = 0; i < PACKED_CALLSIGN_BYTES; i++)
    {
        out[i] = (uint8_t)(packed >> (8 * (PACKED_CALLSIGN_BYTES - 1 - i)));
    }
}

std::string FrameCodec::unpackCallsign(const uint8_t* in)
{
    uint64_t packed = 0;
    for (int i = 0; i < PACKED_CALLSIGN_BYTES; i++)
    {
        packed = (packed << 8) | (uint64_t)in[i];
    }

    // Six bytes can hold values the nine symbol alphabet cannot produce, so
    // anything above the alphabet's range is corruption, not a callsign.
    uint64_t maxPacked = 1;
    for (int i = 0; i < MAX_PACKED_CALLSIGN_CHARS; i++) maxPacked *= BASE40_SIZE;
    if (packed >= maxPacked) return "";

    char chars[MAX_PACKED_CALLSIGN_CHARS + 1];
    chars[MAX_PACKED_CALLSIGN_CHARS] = '\0';
    for (int i = MAX_PACKED_CALLSIGN_CHARS - 1; i >= 0; i--)
    {
        chars[i] = BASE40_ALPHABET[packed % BASE40_SIZE];
        packed /= BASE40_SIZE;
    }

    std::string result(chars);
    size_t end = result.find_last_not_of(' ');
    if (end == std::string::npos) return "";
    result.resize(end + 1);

    // A packed value larger than the alphabet can represent (corruption that
    // slipped past the modem CRC) decodes to pad characters in the middle;
    // those are not callsigns, so refuse them rather than show garbage.
    if (result.find(' ') != std::string::npos) return "";

    return result;
}

bool FrameCodec::isKnownFrameType(uint8_t type)
{
    switch ((FrameType)type)
    {
        case FrameType::Ping:
        case FrameType::PingAck:
        case FrameType::Message:
        case FrameType::MessageAck:
        case FrameType::Broadcast:
        case FrameType::MessagePartialAck:
            return true;
        default:
            return false;
    }
}

bool FrameCodec::isSignallingFrameType(FrameType type)
{
    switch (type)
    {
        case FrameType::Ping:
        case FrameType::PingAck:
        case FrameType::MessageAck:
        case FrameType::MessagePartialAck:
            return true;
        case FrameType::Message:
        case FrameType::Broadcast:
            return false;
    }

    return false;
}

int FrameCodec::headerBytes(FrameType type)
{
    return isSignallingFrameType(type) ? SIGNALLING_HEADER_BYTES : TEXT_HEADER_BYTES;
}

std::vector<uint8_t> FrameCodec::encode(const Frame& frame, int frameBytes)
{
    const int header = headerBytes(frame.type);
    const bool signalling = isSignallingFrameType(frame.type);

    if (frameBytes < header) return {};
    if (frame.payload.size() > 255) return {};
    if ((int)frame.payload.size() > frameBytes - header) return {};
    if (normalizeCallsign(frame.originCallsign).empty()) return {};

    // A signalling frame has nowhere to put the fragment fields, so it may
    // only ever describe a single fragment.
    if (signalling)
    {
        if (frame.fragmentCount != 1 || frame.fragmentIndex != 0) return {};
    }
    else
    {
        if (frame.fragmentCount == 0) return {};
        if (frame.fragmentIndex >= frame.fragmentCount) return {};
        if (frame.fragmentIndex > FRAGMENT_INDEX_MASK) return {};
        if (frame.burstsFollowing > MAX_TEXT_BURSTS_FOLLOWING) return {};
    }

    std::vector<uint8_t> out(frameBytes, 0);

    out[OFFSET_TYPE] = (uint8_t)frame.type;
    if (signalling && frame.burstsFollowing > 0) out[OFFSET_TYPE] |= TYPE_MORE_FOLLOWS;
    out[OFFSET_DEST_CRC] = (uint8_t)(frame.destinationCrc >> 16);
    out[OFFSET_DEST_CRC + 1] = (uint8_t)(frame.destinationCrc >> 8);
    out[OFFSET_DEST_CRC + 2] = (uint8_t)frame.destinationCrc;
    packCallsign(frame.originCallsign, &out[OFFSET_ORIGIN_CALLSIGN]);
    out[OFFSET_AIR_ID] = (uint8_t)(frame.airId >> 8);
    out[OFFSET_AIR_ID + 1] = (uint8_t)frame.airId;

    if (!signalling)
    {
        out[OFFSET_FRAGMENT_INDEX] =
            (uint8_t)((frame.burstsFollowing << BURSTS_FOLLOWING_SHIFT) | frame.fragmentIndex);
        out[OFFSET_FRAGMENT_COUNT] = frame.fragmentCount;
    }

    out[payloadLengthOffset(frame.type)] = (uint8_t)frame.payload.size();

    if (!frame.payload.empty())
    {
        std::memcpy(&out[header], frame.payload.data(), frame.payload.size());
    }

    return out;
}

bool FrameCodec::decode(const uint8_t* data, int length, Frame& frameOut)
{
    // The type byte decides which layout the rest of the frame is in, so it
    // has to be checked before anything is read at a layout dependent offset.
    if (data == nullptr || length < 1) return false;

    const bool moreFollows = (data[OFFSET_TYPE] & TYPE_MORE_FOLLOWS) != 0;
    const uint8_t typeValue = data[OFFSET_TYPE] & TYPE_VALUE_MASK;
    if (!isKnownFrameType(typeValue)) return false;

    FrameType type = (FrameType)typeValue;
    const int header = headerBytes(type);
    const bool signalling = isSignallingFrameType(type);
    if (length < header) return false;

    int payloadLength = data[payloadLengthOffset(type)];
    if (payloadLength > length - header) return false;

    uint8_t fragmentIndex = 0;
    uint8_t fragmentCount = 1;
    uint8_t burstsFollowing = moreFollows ? 1 : 0;
    if (!signalling)
    {
        // A text frame says how many follow in its own header; the type bit
        // is not something we send on one.
        if (moreFollows) return false;

        fragmentIndex = data[OFFSET_FRAGMENT_INDEX] & FRAGMENT_INDEX_MASK;
        burstsFollowing = data[OFFSET_FRAGMENT_INDEX] >> BURSTS_FOLLOWING_SHIFT;
        fragmentCount = data[OFFSET_FRAGMENT_COUNT];
        if (fragmentCount == 0 || fragmentIndex >= fragmentCount) return false;
        if (fragmentCount > MAX_FRAGMENTS_PER_MESSAGE) return false;
    }

    std::string originCallsign = unpackCallsign(&data[OFFSET_ORIGIN_CALLSIGN]);
    if (originCallsign.empty()) return false;

    frameOut.type = type;
    frameOut.destinationCrc =
        ((uint32_t)data[OFFSET_DEST_CRC] << 16) |
        ((uint32_t)data[OFFSET_DEST_CRC + 1] << 8) |
        (uint32_t)data[OFFSET_DEST_CRC + 2];
    frameOut.originCallsign = originCallsign;
    frameOut.airId = (uint16_t)(((uint16_t)data[OFFSET_AIR_ID] << 8) | data[OFFSET_AIR_ID + 1]);
    frameOut.fragmentIndex = fragmentIndex;
    frameOut.fragmentCount = fragmentCount;
    frameOut.burstsFollowing = burstsFollowing;
    frameOut.payload.assign(&data[header], &data[header] + payloadLength);

    return true;
}

} // namespace TextMessaging
