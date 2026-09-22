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

// Modem frame payload sizes: what codec2 hands us per modem frame, less the
// two byte CRC the raw data API appends. DATAC13 carries signalling, DATAC4
// carries message text; both are the most robust option in their size class,
// which matters more than throughput for chat. TextMessagingModem checks these
// against the modem at open time and refuses to run if they have drifted.
constexpr int SIGNALLING_FRAME_BYTES = 14; // DATAC13: 128 bits - CRC16
constexpr int TEXT_FRAME_BYTES = 54;       // DATAC4: 448 bits - CRC16

// On air header. Every frame starts with type (1) + destination CRC (3) +
// packed origin callsign (6) + message ID (2). The origin CRC is not sent: it
// is the CRC of the callsign already in the frame, and three bytes is a tenth
// of a DATAC13 frame.
//
// Text frames then add fragment index (1) + fragment count (1); signalling
// frames are always a single fragment and spend those two bytes on payload
// instead, which is what lets a ping fit in DATAC13 at all. Both end with a
// payload length byte, which is how the decoder tells payload from the zero
// padding out to the modem frame size.
constexpr int SIGNALLING_HEADER_BYTES = 13;
constexpr int TEXT_HEADER_BYTES = 15;
constexpr int TEXT_BYTES_PER_FRAGMENT = TEXT_FRAME_BYTES - TEXT_HEADER_BYTES;
constexpr int SIGNALLING_PAYLOAD_BYTES = SIGNALLING_FRAME_BYTES - SIGNALLING_HEADER_BYTES;

static_assert(SIGNALLING_HEADER_BYTES < SIGNALLING_FRAME_BYTES,
              "a signalling frame must have room for its header and a payload byte");
static_assert(TEXT_HEADER_BYTES < TEXT_FRAME_BYTES,
              "a text frame must have room for its header and some text");

// A message is sent as one keying of the transmitter, so its length is bounded
// by how long we are willing to hold the channel: eight DATAC4 fragments is
// around 30 seconds.
constexpr int MAX_FRAGMENTS_PER_MESSAGE = 8;
constexpr int MAX_MESSAGE_TEXT_BYTES = TEXT_BYTES_PER_FRAGMENT * MAX_FRAGMENTS_PER_MESSAGE;

// Callsigns are packed nine characters into six bytes (base 40), which covers
// portable suffixes such as VK3ABC/P. Anything longer is truncated for the
// air, never for the display.
constexpr int MAX_PACKED_CALLSIGN_CHARS = 9;
constexpr int PACKED_CALLSIGN_BYTES = 6;

// A half duplex station hears nothing while it is keyed, and its receiver
// needs a moment to settle after it unkeys. So replying the instant a burst
// decodes talks over a station that is still turning around, and starting the
// next queued burst straight after our own talks over the reply we just asked
// for. The loopback bench caught both: two stations keyed at the same instant
// and each missed what the other sent.
//
// Every transmission therefore waits out a turnaround, measured from the last
// thing we heard and from the end of our own last burst.
// Measured from the last frame we decoded, which is not the same as the far
// end going quiet: its postamble, end of over and audio drain all follow the
// last frame we could read. This has to outlast that tail.
constexpr int TURNAROUND_AFTER_RX_MILLISECONDS = 1500;

// For a burst nobody is expected to answer.
constexpr int TURNAROUND_AFTER_TX_MILLISECONDS = 1500;

// For a burst that asked for an acknowledgement. The far end waits out its own
// turnaround and then sends a burst of its own, so a wait sized for our
// changeover alone is far too short: on the bench a station finished a message
// at 14:29:23, the far end began acknowledging it at 14:29:24, and we keyed
// over the top of it at 14:29:25 and lost the reply. The window ends early
// anyway when the acknowledgement arrives, because the entry waiting on it is
// removed from the outbox.
constexpr int REPLY_WINDOW_MILLISECONDS = 5000;

// Two stations that back off by exactly the same amount collide again on the
// retry, so the wait after our own burst carries jitter. It is drawn from a
// sequence seeded by the station's own callsign, which keeps it reproducible
// per station while decorrelating any two of them.
constexpr int TURNAROUND_JITTER_MILLISECONDS = 1000;
constexpr int MAX_TURNAROUND_MILLISECONDS =
    REPLY_WINDOW_MILLISECONDS + TURNAROUND_JITTER_MILLISECONDS;

// Carrier sense. While the demodulator is locked onto a burst -- from the
// moment it correlates a preamble until the packet is in -- someone else has
// the channel, and keying would talk over them. Every fragment of a message is
// a burst of its own, so sync genuinely drops for the postamble, the gap and
// the next preamble; the hold bridges that, or the channel would read as free
// between fragments of the same message.
constexpr int CHANNEL_BUSY_HOLD_MILLISECONDS = 1000;

// A demodulator that keeps false triggering on band noise must not be able to
// silence the station for good. The longest real traffic is eight DATAC4
// fragments at about five and a half seconds each, so anything continuously
// busy for longer than this is not a transmission and is ignored.
constexpr int MAX_CHANNEL_BUSY_MILLISECONDS = 60000;

// Retry policy for addressed messages. Timeout is measured from the end of our
// transmission to the arrival of the acknowledgement.
constexpr int MAX_MESSAGE_RETRIES = 3;
constexpr int ACK_TIMEOUT_MILLISECONDS = 15000;
constexpr int PING_TIMEOUT_MILLISECONDS = 15000;

// A retry does not key the moment its timer expires. Two stations whose timers
// expire in the same second cannot see each other by carrier sense, which
// needs about a second of preamble, and the bench caught exactly that: one
// station's second retry and the other's first keyed together at 21:24:21.
// Each retry therefore waits a random backoff first, drawn from a range that
// grows with the attempt, so two stations that collided once are unlikely to
// collide the same way again.
constexpr int RETRY_BACKOFF_MILLISECONDS = 3000;
constexpr int MAX_RETRY_BACKOFF_MILLISECONDS = RETRY_BACKOFF_MILLISECONDS * MAX_MESSAGE_RETRIES;

// What the station is currently waiting to hear back, which is what the chat
// window's status line reports while an acknowledgement cycle is running.
enum class AckWait
{
    Nothing,
    Message,
    Ping,
};

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

// Chat lines are what the operator typed; system lines are the small PING and
// PONG notices the protocol writes into the same window.
enum class MessageKind
{
    Chat,
    System,
};

// One line in the chat window, and one row in the message store.
struct TextMessage
{
    MessageKind kind = MessageKind::Chat;
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
// One the operator typed in by hand is pinned: it may never have been heard
// (lastHeard stays 0), it does not age out, and only the operator removes it.
struct HeardStation
{
    std::string callsign;
    float snr = 0.0f;
    std::time_t lastHeard = 0;
    bool pinned = false;
};

} // namespace TextMessaging

#endif // TEXT_MESSAGING__TEXT_MESSAGING_TYPES_H
