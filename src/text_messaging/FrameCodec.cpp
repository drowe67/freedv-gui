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
// TextMessagingTypes.h.
constexpr int OFFSET_TYPE = 0;
constexpr int OFFSET_DEST_CRC = 1;
constexpr int OFFSET_ORIGIN_CRC = 4;
constexpr int OFFSET_ORIGIN_CALLSIGN = 7;
constexpr int OFFSET_AIR_ID = 13;
constexpr int OFFSET_FRAGMENT_INDEX = 15;
constexpr int OFFSET_FRAGMENT_COUNT = 16;
constexpr int OFFSET_PAYLOAD_LENGTH = 17;

static_assert(OFFSET_PAYLOAD_LENGTH + 1 == FRAME_HEADER_BYTES,
              "header field offsets and FRAME_HEADER_BYTES disagree");
static_assert(OFFSET_AIR_ID == OFFSET_ORIGIN_CALLSIGN + PACKED_CALLSIGN_BYTES,
              "packed callsign does not fit its header field");

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
            return true;
        default:
            return false;
    }
}

std::vector<uint8_t> FrameCodec::encode(const Frame& frame, int frameBytes)
{
    if (frameBytes < FRAME_HEADER_BYTES) return {};
    if (frame.payload.size() > 255) return {};
    if ((int)frame.payload.size() > frameBytes - FRAME_HEADER_BYTES) return {};
    if (frame.fragmentCount == 0) return {};
    if (frame.fragmentIndex >= frame.fragmentCount) return {};
    if (normalizeCallsign(frame.originCallsign).empty()) return {};

    std::vector<uint8_t> out(frameBytes, 0);

    out[OFFSET_TYPE] = (uint8_t)frame.type;
    out[OFFSET_DEST_CRC] = (uint8_t)(frame.destinationCrc >> 16);
    out[OFFSET_DEST_CRC + 1] = (uint8_t)(frame.destinationCrc >> 8);
    out[OFFSET_DEST_CRC + 2] = (uint8_t)frame.destinationCrc;
    out[OFFSET_ORIGIN_CRC] = (uint8_t)(frame.originCrc >> 16);
    out[OFFSET_ORIGIN_CRC + 1] = (uint8_t)(frame.originCrc >> 8);
    out[OFFSET_ORIGIN_CRC + 2] = (uint8_t)frame.originCrc;
    packCallsign(frame.originCallsign, &out[OFFSET_ORIGIN_CALLSIGN]);
    out[OFFSET_AIR_ID] = (uint8_t)(frame.airId >> 8);
    out[OFFSET_AIR_ID + 1] = (uint8_t)frame.airId;
    out[OFFSET_FRAGMENT_INDEX] = frame.fragmentIndex;
    out[OFFSET_FRAGMENT_COUNT] = frame.fragmentCount;
    out[OFFSET_PAYLOAD_LENGTH] = (uint8_t)frame.payload.size();

    if (!frame.payload.empty())
    {
        std::memcpy(&out[FRAME_HEADER_BYTES], frame.payload.data(), frame.payload.size());
    }

    return out;
}

bool FrameCodec::decode(const uint8_t* data, int length, Frame& frameOut)
{
    if (data == nullptr || length < FRAME_HEADER_BYTES) return false;
    if (!isKnownFrameType(data[OFFSET_TYPE])) return false;

    int payloadLength = data[OFFSET_PAYLOAD_LENGTH];
    if (payloadLength > length - FRAME_HEADER_BYTES) return false;

    uint8_t fragmentIndex = data[OFFSET_FRAGMENT_INDEX];
    uint8_t fragmentCount = data[OFFSET_FRAGMENT_COUNT];
    if (fragmentCount == 0 || fragmentIndex >= fragmentCount) return false;
    if (fragmentCount > MAX_FRAGMENTS_PER_MESSAGE) return false;

    std::string originCallsign = unpackCallsign(&data[OFFSET_ORIGIN_CALLSIGN]);
    if (originCallsign.empty()) return false;

    frameOut.type = (FrameType)data[OFFSET_TYPE];
    frameOut.destinationCrc =
        ((uint32_t)data[OFFSET_DEST_CRC] << 16) |
        ((uint32_t)data[OFFSET_DEST_CRC + 1] << 8) |
        (uint32_t)data[OFFSET_DEST_CRC + 2];
    frameOut.originCrc =
        ((uint32_t)data[OFFSET_ORIGIN_CRC] << 16) |
        ((uint32_t)data[OFFSET_ORIGIN_CRC + 1] << 8) |
        (uint32_t)data[OFFSET_ORIGIN_CRC + 2];
    frameOut.originCallsign = originCallsign;
    frameOut.airId = (uint16_t)(((uint16_t)data[OFFSET_AIR_ID] << 8) | data[OFFSET_AIR_ID + 1]);
    frameOut.fragmentIndex = fragmentIndex;
    frameOut.fragmentCount = fragmentCount;
    frameOut.payload.assign(&data[FRAME_HEADER_BYTES], &data[FRAME_HEADER_BYTES] + payloadLength);

    return true;
}

} // namespace TextMessaging
