//=========================================================================
// Name:            TextMessagingProtocol.h
// Purpose:         Message, acknowledgement and ping state machine.
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

#ifndef TEXT_MESSAGING__TEXT_MESSAGING_PROTOCOL_H
#define TEXT_MESSAGING__TEXT_MESSAGING_PROTOCOL_H

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "FrameCodec.h"
#include "TextMessagingTypes.h"

namespace TextMessaging
{

class HeardStationList;
class MessageStore;

// Implemented by the audio pipeline. A transmission is one keying of the
// transmitter carrying every frame handed to transmit(); once transmit()
// returns true, isTransmitting() must keep returning true until the last
// sample of that burst has been sent, and must also be true while the
// operator is transmitting voice. That contract is what lets the protocol
// know when its acknowledgement timer should start.
class ITextMessagingTransport
{
public:
    virtual ~ITextMessagingTransport() = default;

    virtual bool transmit(const std::vector<std::vector<uint8_t>>& frames, bool signalling) = 0;
    virtual bool isTransmitting() const = 0;

    // Called from the same loop that drives tick(), so a transport can finish
    // a burst (unkey the transmitter) without a timer of its own.
    virtual void poll() { /* nothing to do by default */ }

    // True while the receiver is locked onto somebody else's burst. The
    // protocol freezes on it: nothing starts, and no acknowledgement timer
    // runs down, until the channel is clear again.
    virtual bool isChannelBusy() const { return false; }
};

// Implemented by the dialog. Callbacks arrive on whichever thread drove the
// change and never with the protocol's lock held.
class ITextMessagingObserver
{
public:
    virtual ~ITextMessagingObserver() = default;

    virtual void onMessageAdded(const TextMessage& message) = 0;
    virtual void onMessageUpdated(const TextMessage& message) = 0;
    virtual void onStationsChanged() = 0;
};

// Drives chat over the air: fragments outgoing messages, retries the ones that
// are not acknowledged, reassembles incoming ones, answers pings, and keeps
// the message store and heard station list up to date.
//
// Only one transmission is outstanding at a time, which is all a half duplex
// channel can honestly support; anything else queued waits its turn.
class TextMessagingProtocol
{
public:
    TextMessagingProtocol(MessageStore& store, HeardStationList& stations);
    ~TextMessagingProtocol();

    TextMessagingProtocol(const TextMessagingProtocol&) = delete;
    TextMessagingProtocol& operator=(const TextMessagingProtocol&) = delete;

    void setTransport(ITextMessagingTransport* transport);
    void setObserver(ITextMessagingObserver* observer);

    void setMyCallsign(const std::string& callsign);
    std::string myCallsign() const;

    // Automatic acknowledgements and pongs. On by default; the dialog exposes
    // it so that an operator who must not transmit unattended can turn the
    // station into a receive only chat client.
    void setAutoReplyEnabled(bool enabled);
    bool autoReplyEnabled() const;

    // Replaces the clocks the protocol reads. Milliseconds must be monotonic
    // (timeouts) and the wall clock is what the chat window timestamps with.
    void setClocks(std::function<uint64_t()> monotonicMs, std::function<std::time_t()> wallClock);

    // Queues a message. Pass an empty destination to broadcast. Returns false
    // with a reason in errorOut if the message cannot be sent at all: no
    // callsign configured, empty or oversized text, or no transport.
    bool sendMessage(const std::string& text, const std::string& destination,
                     std::string& errorOut);

    // Queues a ping to a single station.
    bool sendPing(const std::string& destination, std::string& errorOut);

    // Called by the receive step for every frame the modem decodes.
    void onFrameReceived(const Frame& frame, float snr);

    // Drives transmission, retries and timeouts. Call it a few times a second
    // from the GUI timer; it does no work of its own when nothing is pending.
    void tick();

    // Number of transmissions waiting, including the one on the air. Used by
    // the dialog to show that something is still queued.
    size_t pendingCount() const;

    // What we are waiting to hear back, if anything. Reported for the whole
    // acknowledgement cycle including the retries, so the status line does not
    // flicker as the message goes back on the air.
    AckWait ackWait() const;

    // True while a burst is actually on the air, ours or the voice keyer's.
    // The chat window disables sending on it, so nothing is queued behind a
    // keyed transmitter.
    bool isTransmitting() const;

private:
    enum class TransmissionState
    {
        Queued,        // waiting for a clear transmitter
        Transmitting,  // handed to the transport, burst in progress
        AwaitingAck,   // burst finished, acknowledgement timer running
    };

    struct PendingTransmission
    {
        TextMessage message;                        // the chat line it belongs to
        std::vector<std::vector<uint8_t>> frames;
        bool signalling = false;                    // DATAC13 rather than DATAC4
        bool expectsAck = false;
        bool isPing = false;
        bool reply = false;      // an acknowledgement or pong we owe somebody
        std::string destination;
        int retries = 0;
        uint64_t deadlineMs = 0;
        uint64_t sentAtMs = 0;   // end of our burst, for the reply window
        uint64_t notBeforeMs = 0; // retry backoff; nothing to do with the far end
        TransmissionState state = TransmissionState::Queued;
    };

    struct Reassembly
    {
        std::vector<std::string> fragments;
        uint8_t fragmentCount = 0;
        uint32_t receivedMask = 0;
        uint64_t firstSeenMs = 0;
        float snr = 0.0f;
        bool broadcast = false;
    };

    using ReassemblyKey = std::pair<std::string, uint16_t>;

    // Events queued while the lock is held and delivered once it is released,
    // so an observer is free to call back into the protocol.
    struct PendingEvent
    {
        enum class Type { MessageAdded, MessageUpdated, StationsChanged } type;
        TextMessage message;
    };

    // All of these assume mutex_ is held.
    bool queueMessageLocked(const std::string& text, const std::string& destination,
                            std::string& errorOut, std::vector<PendingEvent>& events);
    void queueAckLocked(const std::string& destination, uint16_t airId);
    void queuePongLocked(const std::string& destination, float snr);
    void handleIncomingFragmentLocked(const Frame& frame, float snr,
                                      std::vector<PendingEvent>& events);
    void handleAckLocked(const Frame& frame, std::vector<PendingEvent>& events);
    void handlePingLocked(const Frame& frame, float snr, std::vector<PendingEvent>& events);
    void handlePongLocked(const Frame& frame, float snr, std::vector<PendingEvent>& events);
    void addSystemMessageLocked(const std::string& text, const std::string& destination,
                                std::vector<PendingEvent>& events);
    void updateStatusLocked(PendingTransmission& pending, MessageStatus status,
                            std::vector<PendingEvent>& events);
    void purgeStaleReassembliesLocked(uint64_t nowMs);

    // Holds the transmitter off until the far end has had its turn. Never
    // shortens a wait that is already running.
    void deferTransmissionLocked(uint64_t nowMs, int baseMs, int jitterMs);
    uint64_t quietUntilLocked(bool forReply) const;
    bool channelFrozenLocked(uint64_t nowMs);
    void holdTimersLocked(uint64_t pausedMs);
    uint32_t randomDelayLocked(int maxMs);
    void serviceOutboxLocked(uint64_t nowMs, bool frozen, std::vector<PendingEvent>& events);
    uint16_t nextAirIdLocked();
    Frame makeFrameLocked(FrameType type, const std::string& destination, uint16_t airId,
                          uint8_t fragmentIndex, uint8_t fragmentCount,
                          const std::vector<uint8_t>& payload) const;
    bool isAddressedToMeLocked(const Frame& frame) const;

    void deliver(const std::vector<PendingEvent>& events);

    mutable std::mutex mutex_;
    MessageStore& store_;
    HeardStationList& stations_;
    ITextMessagingTransport* transport_;
    ITextMessagingObserver* observer_;

    std::string myCallsign_;
    uint32_t myCallsignCrc_;
    bool autoReplyEnabled_;
    uint16_t nextAirId_;

    uint64_t quietUntilMs_;
    uint32_t jitterState_;

    // Carrier sense bookkeeping, updated every tick whether or not anything
    // is queued, so a busy spell is measured from when it really began.
    bool channelBusy_;
    uint64_t channelBusySinceMs_;
    uint64_t lastTickMs_;

    std::deque<PendingTransmission> outbox_;
    std::map<ReassemblyKey, Reassembly> inbox_;
    std::map<ReassemblyKey, uint64_t> recentlyCompleted_;

    std::function<uint64_t()> monotonicMs_;
    std::function<std::time_t()> wallClock_;
};

} // namespace TextMessaging

#endif // TEXT_MESSAGING__TEXT_MESSAGING_PROTOCOL_H
