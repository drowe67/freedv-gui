//=========================================================================
// Name:            TextMessagingTypes.h
// Purpose:         Shared vocabulary for the FreeDV text messaging feature.
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

#ifndef TEXT_MESSAGING__TEXT_MESSAGING_TYPES_H
#define TEXT_MESSAGING__TEXT_MESSAGING_TYPES_H

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace TextMessaging
{

// Over the air frame types. The protocol is inspired by FreeDATA but is not
// wire compatible with it, so the values are our own; they are persisted in
// nothing and may be renumbered until the first release.
enum class FrameType : uint8_t
{
    Ping = 0x10,       // "are you there" addressed to a single station
    PingAck = 0x11,    // reply to Ping, carries the SNR the ping was heard at
    Message = 0x20,    // addressed message fragment, acknowledgement requested
    MessageAck = 0x21, // acknowledgement of a complete addressed message
    Broadcast = 0x22,  // unaddressed message fragment, no acknowledgement
};

// Modem frame payload sizes (codec2 1.2.0, data bits per modem frame divided
// by 8, less the two byte CRC that the raw data API appends). DATAC13 carries
// signalling, DATAC4 carries message text; both are the most robust option in
// their size class, which matters more than throughput for chat.
constexpr int SIGNALLING_FRAME_BYTES = 30; // DATAC13: 256 data bits - CRC16
constexpr int TEXT_FRAME_BYTES = 126;      // DATAC4: 1024 data bits - CRC16

// On air header: type (1) + destination CRC (3) + origin CRC (3) +
// packed origin callsign (6) + message ID (2) + fragment index (1) +
// fragment count (1) + payload length (1). The length byte is what lets the
// decoder tell payload from the zero padding out to the modem frame size.
constexpr int FRAME_HEADER_BYTES = 18;
constexpr int TEXT_BYTES_PER_FRAGMENT = TEXT_FRAME_BYTES - FRAME_HEADER_BYTES;

// A message is sent as one keying of the transmitter, so its length is bounded
// by how long we are willing to hold the channel: eight fragments is roughly
// 860 characters and around 30 seconds of DATAC4.
constexpr int MAX_FRAGMENTS_PER_MESSAGE = 8;
constexpr int MAX_MESSAGE_TEXT_BYTES = TEXT_BYTES_PER_FRAGMENT * MAX_FRAGMENTS_PER_MESSAGE;

// Callsigns are packed nine characters into six bytes (base 40), which covers
// portable suffixes such as VK3ABC/P. Anything longer is truncated for the
// air, never for the display.
constexpr int MAX_PACKED_CALLSIGN_CHARS = 9;
constexpr int PACKED_CALLSIGN_BYTES = 6;

// Retry policy for addressed messages. Timeout is measured from the end of our
// transmission to the arrival of the acknowledgement.
constexpr int MAX_MESSAGE_RETRIES = 3;
constexpr int ACK_TIMEOUT_MILLISECONDS = 15000;
constexpr int PING_TIMEOUT_MILLISECONDS = 15000;

enum class MessageDirection
{
    Sent,
    Received,
};

// Lifecycle of a message in the chat window. Sent messages walk Queued ->
// Transmitting -> AwaitingAck -> Acknowledged, falling into Retrying on each
// timeout and Failed once the retries are used up. Broadcasts stop at Sent
// because nothing acknowledges them. Received messages are created Received.
enum class MessageStatus
{
    Queued,
    Transmitting,
    AwaitingAck,
    Retrying,
    Acknowledged,
    Failed,
    Sent,
    Received,
};

// One line in the chat window, and one row in the message store.
struct TextMessage
{
    int64_t id = 0;                 // message store row ID, 0 until stored
    uint16_t airId = 0;             // ID carried on air, used to match ACKs
    std::string originCallsign;     // who sent it (us, for Sent messages)
    std::string destCallsign;       // empty for broadcasts
    bool broadcast = false;
    std::string text;
    std::time_t timestamp = 0;      // when queued (Sent) or decoded (Received)
    MessageDirection direction = MessageDirection::Sent;
    MessageStatus status = MessageStatus::Queued;
    int retryCount = 0;
    float snr = 0.0f;               // SNR the message was received at
};

// A station we have decoded something from, shown in the heard stations list.
struct HeardStation
{
    std::string callsign;
    float snr = 0.0f;
    std::time_t lastHeard = 0;
};

} // namespace TextMessaging

#endif // TEXT_MESSAGING__TEXT_MESSAGING_TYPES_H
