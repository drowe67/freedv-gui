//=========================================================================
// Name:            FrameCodec.h
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

#ifndef TEXT_MESSAGING__FRAME_CODEC_H
#define TEXT_MESSAGING__FRAME_CODEC_H

#include <cstdint>
#include <string>
#include <vector>

#include "TextMessagingTypes.h"

namespace TextMessaging
{

// One decoded (or about to be encoded) frame. Frames are carried by the codec2
// raw data API, which appends and verifies its own CRC over each modem frame,
// so nothing here repeats that check; a frame handed to decode() has already
// been vouched for by the modem.
struct Frame
{
    FrameType type = FrameType::Ping;
    uint32_t destinationCrc = 0;    // 24 bits; zero means "broadcast"
    std::string originCallsign;     // as decoded, may be truncated to 9 chars
    uint16_t airId = 0;             // matches a message to its acknowledgement
    uint8_t fragmentIndex = 0;      // zero based
    uint8_t fragmentCount = 1;
    std::vector<uint8_t> payload;
};

class FrameCodec
{
public:
    // Uppercases and strips characters the air alphabet cannot carry, so that
    // the CRC both ends compute over a callsign agrees. Returns an empty
    // string if nothing usable is left.
    static std::string normalizeCallsign(const std::string& callsign);

    // CRC-24/OPENPGP over the normalized callsign. Used to address frames
    // without spending six bytes on the destination.
    static uint32_t callsignCrc24(const std::string& callsign);

    // Base 40 packing of up to MAX_PACKED_CALLSIGN_CHARS characters into
    // PACKED_CALLSIGN_BYTES bytes, big endian. Longer callsigns are truncated.
    static void packCallsign(const std::string& callsign, uint8_t* out);
    static std::string unpackCallsign(const uint8_t* in);

    // Serializes a frame, zero padded out to frameBytes (SIGNALLING_FRAME_BYTES
    // or TEXT_FRAME_BYTES). Returns an empty vector if the frame does not fit
    // the header its type requires, if the payload does not fit behind that
    // header, or if it carries a callsign that cannot be packed.
    static std::vector<uint8_t> encode(const Frame& frame, int frameBytes);

    // Parses a frame received from the modem. Returns false when the frame is
    // too short, the type is not one of ours, or the declared payload length
    // runs past the end of the data.
    static bool decode(const uint8_t* data, int length, Frame& frameOut);

    // True for the frame types this build knows how to handle. Kept separate
    // so the receive path can drop unknown types without parsing them.
    static bool isKnownFrameType(uint8_t type);

    // Pings and acknowledgements ride DATAC13, which is too small for the
    // fragment fields, so they carry a shorter header and are always a single
    // fragment. Message text rides DATAC4 and carries the full header.
    static bool isSignallingFrameType(FrameType type);
    static int headerBytes(FrameType type);
};

} // namespace TextMessaging

#endif // TEXT_MESSAGING__FRAME_CODEC_H
