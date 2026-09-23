//=========================================================================
// Name:            TextMessagingProtocol.cpp
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

#include "TextMessagingProtocol.h"

#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>

#include "HeardStationList.h"
#include "MessageStore.h"

namespace TextMessaging
{

namespace
{

// A message that completed within this window and arrives again is a
// retransmission whose acknowledgement we lost, not a new message.
constexpr uint64_t DUPLICATE_WINDOW_MS = 10 * 60 * 1000;

// Message IDs start somewhere random rather than at one. Receivers remember
// (sender, ID) for DUPLICATE_WINDOW_MS, so a station that restarted and began
// again at one would have its first messages taken for retransmissions of the
// last ones: acknowledged, and never shown.
uint16_t randomAirId()
{
    std::random_device device;
    return (uint16_t)(device() & 0xFFFFu);
}

std::string trim(const std::string& text)
{
    size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

// SNR travels in one byte in half dB steps, which covers every SNR the modem
// can report with room to spare.
uint8_t encodeSnr(float snr)
{
    float scaled = snr * 2.0f;
    if (scaled > 127.0f) scaled = 127.0f;
    if (scaled < -128.0f) scaled = -128.0f;
    return (uint8_t)(int8_t)std::lround(scaled);
}

float decodeSnr(uint8_t encoded)
{
    return (float)(int8_t)encoded / 2.0f;
}

std::string formatSnr(float snr)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1f", (double)snr);
    return buffer;
}

} // namespace

TextMessagingProtocol::TextMessagingProtocol(MessageStore& store, HeardStationList& stations)
    : store_(store)
    , stations_(stations)
    , transport_(nullptr)
    , observer_(nullptr)
    , myCallsignCrc_(0)
    , autoReplyEnabled_(true)
    , nextAirId_(randomAirId())
    , quietUntilMs_(0)
    , jitterState_(1)
    , channelBusy_(false)
    , channelBusySinceMs_(0)
    , channelReservedUntilMs_(0)
    , lastTickMs_(0)
    , monotonicMs_([]() {
        return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    })
    , wallClock_([]() { return std::time(nullptr); })
{
    // empty
}

TextMessagingProtocol::~TextMessagingProtocol()
{
    // empty
}

void TextMessagingProtocol::setTransport(ITextMessagingTransport* transport)
{
    std::lock_guard<std::mutex> lock(mutex_);
    transport_ = transport;
}

void TextMessagingProtocol::setObserver(ITextMessagingObserver* observer)
{
    std::lock_guard<std::mutex> lock(mutex_);
    observer_ = observer;
}

void TextMessagingProtocol::setMyCallsign(const std::string& callsign)
{
    std::lock_guard<std::mutex> lock(mutex_);
    myCallsign_ = FrameCodec::normalizeCallsign(callsign);
    myCallsignCrc_ = FrameCodec::callsignCrc24(myCallsign_);

    // Seeding the backoff from our own callsign keeps it reproducible for a
    // given station while making any two stations back off differently.
    jitterState_ = myCallsignCrc_ | 1u;
}

std::string TextMessagingProtocol::myCallsign() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return myCallsign_;
}

void TextMessagingProtocol::setAutoReplyEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(mutex_);
    autoReplyEnabled_ = enabled;
}

bool TextMessagingProtocol::autoReplyEnabled() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return autoReplyEnabled_;
}

void TextMessagingProtocol::setClocks(std::function<uint64_t()> monotonicMs,
                                      std::function<std::time_t()> wallClock)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (monotonicMs) monotonicMs_ = monotonicMs;
    if (wallClock) wallClock_ = wallClock;
}

size_t TextMessagingProtocol::pendingCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return outbox_.size();
}

uint16_t TextMessagingProtocol::nextAirIdLocked()
{
    // Zero is reserved so that an all zero header cannot look like a valid ID.
    if (nextAirId_ == 0) nextAirId_ = 1;
    return nextAirId_++;
}

Frame TextMessagingProtocol::makeFrameLocked(FrameType type, const std::string& destination,
                                             uint16_t airId, uint8_t fragmentIndex,
                                             uint8_t fragmentCount,
                                             const std::vector<uint8_t>& payload) const
{
    Frame frame;
    frame.type = type;
    frame.destinationCrc = destination.empty() ? 0 : FrameCodec::callsignCrc24(destination);
    frame.originCallsign = myCallsign_;
    frame.airId = airId;
    frame.fragmentIndex = fragmentIndex;
    frame.fragmentCount = fragmentCount;
    frame.payload = payload;
    return frame;
}

bool TextMessagingProtocol::isAddressedToMeLocked(const Frame& frame) const
{
    // Addressing is by CRC, so a collision could in principle hand us somebody
    // else's frame; the origin callsign in the frame is what gets displayed,
    // so the worst case is a stray line in the chat window.
    return !myCallsign_.empty() && frame.destinationCrc == myCallsignCrc_;
}

bool TextMessagingProtocol::sendMessage(const std::string& text, const std::string& destination,
                                        std::string& errorOut)
{
    std::vector<PendingEvent> events;
    bool ok = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        ok = queueMessageLocked(text, destination, errorOut, events);
    }

    deliver(events);
    return ok;
}

bool TextMessagingProtocol::queueMessageLocked(const std::string& text,
                                               const std::string& destination,
                                               std::string& errorOut,
                                               std::vector<PendingEvent>& events)
{
    if (myCallsign_.empty())
    {
        errorOut = "Set your callsign in Tools/Options before sending messages.";
        return false;
    }

    if (transport_ == nullptr)
    {
        errorOut = "FreeDV is not running, so there is nothing to transmit with.";
        return false;
    }

    std::string body = trim(text);
    if (body.empty())
    {
        errorOut = "There is nothing to send.";
        return false;
    }

    if ((int)body.size() > MAX_MESSAGE_TEXT_BYTES)
    {
        errorOut = "Message is too long; the limit is " +
                   std::to_string(MAX_MESSAGE_TEXT_BYTES) + " characters.";
        return false;
    }

    std::string normalizedDestination = FrameCodec::normalizeCallsign(destination);
    bool broadcast = normalizedDestination.empty();
    uint16_t airId = nextAirIdLocked();

    // Encode before storing, so a message that cannot go on the air never
    // appears in the chat window as something that was sent.
    PendingTransmission pending;
    pending.signalling = false;
    pending.expectsAck = !broadcast;
    pending.isPing = false;
    pending.destination = normalizedDestination;
    pending.state = TransmissionState::Queued;

    size_t fragmentCount = (body.size() + TEXT_BYTES_PER_FRAGMENT - 1) / TEXT_BYTES_PER_FRAGMENT;
    for (size_t index = 0; index < fragmentCount; index++)
    {
        std::string chunk = body.substr(index * TEXT_BYTES_PER_FRAGMENT, TEXT_BYTES_PER_FRAGMENT);
        std::vector<uint8_t> payload(chunk.begin(), chunk.end());

        Frame frame = makeFrameLocked(broadcast ? FrameType::Broadcast : FrameType::Message,
                                      normalizedDestination, airId, (uint8_t)index,
                                      (uint8_t)fragmentCount, payload);

        // Checked now, as the whole message would be sent, so that a keying
        // built later from any subset of these fragments cannot fail to encode.
        frame.burstsFollowing = (uint8_t)(fragmentCount - 1 - index);
        if (FrameCodec::encode(frame, TEXT_FRAME_BYTES).empty())
        {
            errorOut = "Could not encode the message for transmission.";
            return false;
        }

        pending.fragments.push_back(frame);
    }

    TextMessage message;
    message.kind = MessageKind::Chat;
    message.airId = airId;
    message.originCallsign = myCallsign_;
    message.destCallsign = normalizedDestination;
    message.broadcast = broadcast;
    message.text = body;
    message.timestamp = wallClock_();
    message.direction = MessageDirection::Sent;
    message.status = MessageStatus::Queued;

    if (!store_.addMessage(message))
    {
        errorOut = "Could not save the message: " + store_.lastError();
        return false;
    }

    pending.message = message;
    outbox_.push_back(pending);

    PendingEvent event;
    event.type = PendingEvent::Type::MessageAdded;
    event.message = message;
    events.push_back(event);

    return true;
}

bool TextMessagingProtocol::sendPing(const std::string& destination, std::string& errorOut)
{
    std::vector<PendingEvent> events;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (myCallsign_.empty())
        {
            errorOut = "Set your callsign in Tools/Options before pinging.";
            return false;
        }

        if (transport_ == nullptr)
        {
            errorOut = "FreeDV is not running, so there is nothing to transmit with.";
            return false;
        }

        std::string normalizedDestination = FrameCodec::normalizeCallsign(destination);
        if (normalizedDestination.empty())
        {
            errorOut = "Select a station to ping.";
            return false;
        }

        uint16_t airId = nextAirIdLocked();
        Frame frame = makeFrameLocked(FrameType::Ping, normalizedDestination, airId, 0, 1, {});
        std::vector<uint8_t> encoded = FrameCodec::encode(frame, SIGNALLING_FRAME_BYTES);
        if (encoded.empty())
        {
            errorOut = "Could not encode the ping for transmission.";
            return false;
        }

        TextMessage message;
        message.kind = MessageKind::System;
        message.airId = airId;
        message.originCallsign = myCallsign_;
        message.destCallsign = normalizedDestination;
        message.text = myCallsign_ + " >> " + normalizedDestination + " : PING!";
        message.timestamp = wallClock_();
        message.direction = MessageDirection::Sent;
        message.status = MessageStatus::Queued;

        if (!store_.addMessage(message))
        {
            errorOut = "Could not save the ping: " + store_.lastError();
            return false;
        }

        PendingTransmission pending;
        pending.message = message;
        pending.frames.push_back(encoded);
        pending.signalling = true;
        pending.expectsAck = true;
        pending.isPing = true;
        pending.destination = normalizedDestination;
        pending.state = TransmissionState::Queued;
        outbox_.push_back(pending);

        PendingEvent event;
        event.type = PendingEvent::Type::MessageAdded;
        event.message = message;
        events.push_back(event);
    }

    deliver(events);
    return true;
}

void TextMessagingProtocol::queueAckLocked(const std::string& destination, uint16_t airId)
{
    // The whole message is in, so a request for part of it still waiting to
    // go out has been overtaken.
    for (auto it = outbox_.begin(); it != outbox_.end();)
    {
        bool overtaken = it->partialAck && it->state == TransmissionState::Queued &&
                         it->destination == destination && it->message.airId == airId;
        it = overtaken ? outbox_.erase(it) : std::next(it);
    }

    // A retransmission of a message we already have arrives one fragment at a
    // time, and each duplicate fragment asks for the acknowledgement again.
    // One queued acknowledgement answers all of them; eight would hold the
    // channel for eight bursts saying the same thing.
    for (const PendingTransmission& queued : outbox_)
    {
        if (queued.ack && queued.state == TransmissionState::Queued &&
            queued.destination == destination && queued.message.airId == airId)
        {
            return;
        }
    }

    Frame frame = makeFrameLocked(FrameType::MessageAck, destination, airId, 0, 1, {});
    std::vector<uint8_t> encoded = FrameCodec::encode(frame, SIGNALLING_FRAME_BYTES);
    if (encoded.empty()) return;

    PendingTransmission pending;
    pending.frames.push_back(encoded);
    pending.signalling = true;
    pending.expectsAck = false;
    pending.reply = true;
    pending.ack = true;
    pending.message.airId = airId;
    pending.destination = destination;
    pending.state = TransmissionState::Queued;

    // Acknowledgements go to the front: the sender is sitting on a timer.
    outbox_.push_front(pending);
}

// Tells the sender which fragments arrived, so that it resends only the rest.
// A newer report for the same message replaces one still waiting to go out.
void TextMessagingProtocol::queuePartialAckLocked(const std::string& destination, uint16_t airId,
                                                  uint32_t received)
{
    std::vector<uint8_t> payload{(uint8_t)received};
    Frame frame = makeFrameLocked(FrameType::MessagePartialAck, destination, airId, 0, 1, payload);
    std::vector<uint8_t> encoded = FrameCodec::encode(frame, SIGNALLING_FRAME_BYTES);
    if (encoded.empty()) return;

    for (PendingTransmission& queued : outbox_)
    {
        if (queued.partialAck && queued.state == TransmissionState::Queued &&
            queued.destination == destination && queued.message.airId == airId)
        {
            queued.frames.assign(1, encoded);
            return;
        }
    }

    PendingTransmission pending;
    pending.frames.push_back(encoded);
    pending.signalling = true;
    pending.expectsAck = false;
    pending.reply = true;
    pending.partialAck = true;
    pending.message.airId = airId;
    pending.destination = destination;
    pending.state = TransmissionState::Queued;

    // Like an acknowledgement: the sender is sitting on a timer.
    outbox_.push_front(pending);
}

// A message addressed to us, heard in part during a keying that is now over,
// asks the sender for the fragments still missing. One heard nothing in the
// keying says nothing: the sender's timer covers that, as it always has.
void TextMessagingProtocol::requestMissingFragmentsLocked(uint64_t nowMs)
{
    for (auto& entry : inbox_)
    {
        Reassembly& reassembly = entry.second;
        if (reassembly.broadcast || !reassembly.heardThisKeying) continue;
        if (nowMs < reassembly.keyingEndsMs) continue;

        reassembly.heardThisKeying = false;
        if (autoReplyEnabled_)
        {
            queuePartialAckLocked(entry.first.first, entry.first.second, reassembly.receivedMask);
        }
    }
}

void TextMessagingProtocol::queuePongLocked(const std::string& destination, float snr)
{
    std::vector<uint8_t> payload{encodeSnr(snr)};
    Frame frame = makeFrameLocked(FrameType::PingAck, destination, nextAirIdLocked(), 0, 1, payload);
    std::vector<uint8_t> encoded = FrameCodec::encode(frame, SIGNALLING_FRAME_BYTES);
    if (encoded.empty()) return;

    PendingTransmission pending;
    pending.frames.push_back(encoded);
    pending.signalling = true;
    pending.expectsAck = false;
    pending.reply = true;
    pending.destination = destination;
    pending.state = TransmissionState::Queued;
    outbox_.push_front(pending);
}

void TextMessagingProtocol::addSystemMessageLocked(const std::string& text,
                                                   const std::string& destination,
                                                   std::vector<PendingEvent>& events)
{
    TextMessage message;
    message.kind = MessageKind::System;
    message.originCallsign = myCallsign_;
    message.destCallsign = destination;
    message.text = text;
    message.timestamp = wallClock_();
    message.direction = MessageDirection::Received;
    message.status = MessageStatus::Received;

    if (!store_.addMessage(message)) return;

    PendingEvent event;
    event.type = PendingEvent::Type::MessageAdded;
    event.message = message;
    events.push_back(event);
}

void TextMessagingProtocol::updateStatusLocked(PendingTransmission& pending, MessageStatus status,
                                               std::vector<PendingEvent>& events)
{
    // Acknowledgements and pongs have no chat line of their own.
    if (pending.message.id == 0) return;

    pending.message.status = status;
    pending.message.retryCount = pending.retries;
    pending.message.fragmentCount = (int)pending.fragments.size();
    pending.message.fragmentsConfirmed = std::popcount(pending.confirmed);
    store_.updateMessageStatus(pending.message.id, status, pending.retries);

    PendingEvent event;
    event.type = PendingEvent::Type::MessageUpdated;
    event.message = pending.message;
    events.push_back(event);
}

void TextMessagingProtocol::onFrameReceived(const Frame& frame, float snr)
{
    std::vector<PendingEvent> events;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Our own transmission looped back through the radio's monitor path.
        if (!myCallsign_.empty() && frame.originCallsign == myCallsign_) return;

        // The station we just heard is turning its receiver back on; keying
        // straight away talks over it.
        deferTransmissionLocked(monotonicMs_(), TURNAROUND_AFTER_RX_MILLISECONDS, 0);

        std::time_t now = wallClock_();
        stations_.heard(frame.originCallsign, snr, now);

        HeardStation station;
        station.callsign = frame.originCallsign;
        station.snr = snr;
        station.lastHeard = now;
        store_.upsertHeardStation(station);

        PendingEvent stationsChanged;
        stationsChanged.type = PendingEvent::Type::StationsChanged;
        events.push_back(stationsChanged);

        // Whoever the frame is for, its sender holds the channel for the rest
        // of its keying.
        reserveChannelForKeyingLocked(frame, monotonicMs_());

        switch (frame.type)
        {
            case FrameType::Broadcast:
                handleIncomingFragmentLocked(frame, snr, events);
                break;
            case FrameType::Message:
                if (isAddressedToMeLocked(frame)) handleIncomingFragmentLocked(frame, snr, events);
                break;
            case FrameType::MessageAck:
                if (isAddressedToMeLocked(frame)) handleAckLocked(frame, events);
                break;
            case FrameType::MessagePartialAck:
                if (isAddressedToMeLocked(frame)) handlePartialAckLocked(frame, events);
                break;
            case FrameType::Ping:
                if (isAddressedToMeLocked(frame)) handlePingLocked(frame, snr, events);
                break;
            case FrameType::PingAck:
                if (isAddressedToMeLocked(frame)) handlePongLocked(frame, snr, events);
                break;
        }
    }

    deliver(events);
}

// Every frame says how many bursts its sender still has to send in this
// keying; see TEXT_FRAGMENT_AIR_MILLISECONDS. A text frame gives the count,
// so it sets the reservation exactly, and the last burst of a keying releases
// it. That also covers a keying that is not a whole message, such as a resend
// of the fragments a receiver was missing, which "fragment k of n" could not.
void TextMessagingProtocol::reserveChannelForKeyingLocked(const Frame& frame, uint64_t nowMs)
{
    if (FrameCodec::isSignallingFrameType(frame.type))
    {
        // Only "more follows", and only ever lengthens a reservation.
        if (frame.burstsFollowing == 0) return;

        uint64_t until = nowMs + (uint64_t)SIGNALLING_FOLLOWED_RESERVATION_MILLISECONDS;
        if (until > channelReservedUntilMs_) channelReservedUntilMs_ = until;
        return;
    }

    channelReservedUntilMs_ =
        frame.burstsFollowing == 0
            ? 0
            : nowMs + (uint64_t)frame.burstsFollowing * (uint64_t)TEXT_FRAGMENT_AIR_MILLISECONDS;
}

void TextMessagingProtocol::handleIncomingFragmentLocked(const Frame& frame, float snr,
                                                         std::vector<PendingEvent>& events)
{
    bool broadcast = frame.type == FrameType::Broadcast;
    ReassemblyKey key(frame.originCallsign, frame.airId);
    uint64_t nowMs = monotonicMs_();

    // The sender is retransmitting a message we already have, which means our
    // acknowledgement did not reach them. Send it again rather than showing
    // the message twice. A retransmission carries the same fragments; a
    // fragment that differs under the same ID is a new message from a sender
    // whose IDs started over, and is taken in as one.
    auto completed = recentlyCompleted_.find(key);
    if (completed != recentlyCompleted_.end())
    {
        const std::vector<std::string>& fragments = completed->second.fragments;
        bool sameMessage =
            fragments.size() == frame.fragmentCount &&
            fragments[frame.fragmentIndex] ==
                std::string(frame.payload.begin(), frame.payload.end());

        if (sameMessage)
        {
            if (!broadcast && autoReplyEnabled_) queueAckLocked(frame.originCallsign, frame.airId);
            return;
        }

        recentlyCompleted_.erase(completed);
    }

    Reassembly& reassembly = inbox_[key];
    if (reassembly.fragmentCount != frame.fragmentCount)
    {
        reassembly.fragments.assign(frame.fragmentCount, "");
        reassembly.fragmentCount = frame.fragmentCount;
        reassembly.receivedMask = 0;
        reassembly.snr = snr;
        reassembly.broadcast = broadcast;
    }

    reassembly.fragments[frame.fragmentIndex].assign(frame.payload.begin(), frame.payload.end());
    reassembly.receivedMask |= (1u << frame.fragmentIndex);
    reassembly.lastHeardMs = nowMs;
    reassembly.heardThisKeying = true;
    reassembly.keyingEndsMs =
        nowMs + (uint64_t)frame.burstsFollowing * (uint64_t)TEXT_FRAGMENT_AIR_MILLISECONDS;
    reassembly.snr = (reassembly.snr + snr) / 2.0f;

    uint32_t completeMask = (1u << frame.fragmentCount) - 1u;
    if (reassembly.receivedMask != completeMask) return;

    TextMessage message;
    message.kind = MessageKind::Chat;
    message.airId = frame.airId;
    message.originCallsign = frame.originCallsign;
    message.destCallsign = broadcast ? "" : myCallsign_;
    message.broadcast = broadcast;
    for (const std::string& fragment : reassembly.fragments) message.text += fragment;
    message.timestamp = wallClock_();
    message.direction = MessageDirection::Received;
    message.status = MessageStatus::Received;
    message.snr = reassembly.snr;

    Completed& done = recentlyCompleted_[key];
    done.atMs = nowMs;
    done.fragments = reassembly.fragments;
    inbox_.erase(key);

    if (store_.addMessage(message))
    {
        PendingEvent event;
        event.type = PendingEvent::Type::MessageAdded;
        event.message = message;
        events.push_back(event);
    }

    if (!broadcast && autoReplyEnabled_) queueAckLocked(frame.originCallsign, frame.airId);
}

void TextMessagingProtocol::handleAckLocked(const Frame& frame, std::vector<PendingEvent>& events)
{
    // The acknowledged message is not necessarily at the head: an incoming
    // message's own acknowledgement jumps the queue ahead of it.
    for (auto it = outbox_.begin(); it != outbox_.end(); ++it)
    {
        if (it->isPing || !it->expectsAck) continue;
        if (it->message.airId != frame.airId) continue;
        if (it->destination != frame.originCallsign) continue;

        updateStatusLocked(*it, MessageStatus::Acknowledged, events);
        outbox_.erase(it);
        return;
    }
}

// The far end has part of a message of ours. Whatever it confirms is never
// sent again. A report that brings news resends the rest at once and costs no
// retry: a message getting through piece by piece is worth finishing. One
// that brings none means the last keying delivered nothing new, and counts
// as a retry just as a timeout does.
void TextMessagingProtocol::handlePartialAckLocked(const Frame& frame,
                                                   std::vector<PendingEvent>& events)
{
    if (frame.payload.size() != 1) return;

    for (size_t i = 0; i < outbox_.size(); i++)
    {
        PendingTransmission& pending = outbox_[i];
        if (pending.isPing || !pending.expectsAck || pending.fragments.empty()) continue;
        if (pending.message.airId != frame.airId) continue;
        if (pending.destination != frame.originCallsign) continue;

        uint32_t all = (1u << pending.fragments.size()) - 1u;
        uint32_t received = frame.payload[0] & all;
        bool progress = (received & ~pending.confirmed) != 0;
        pending.confirmed |= received;

        if (pending.confirmed == all)
        {
            updateStatusLocked(pending, MessageStatus::Acknowledged, events);
            outbox_.erase(outbox_.begin() + (std::ptrdiff_t)i);
            return;
        }

        // Mid keying the report can only have been meant for an earlier one;
        // what it confirms is recorded and the keying carries on.
        if (pending.state == TransmissionState::Transmitting) return;

        if (progress)
        {
            pending.state = TransmissionState::Queued;
            pending.notBeforeMs = 0;

            // No new status: the message is still waiting on the far end. But
            // the window shows how far it got, and that has changed.
            updateStatusLocked(pending, pending.message.status, events);
            return;
        }

        if (pending.state == TransmissionState::AwaitingAck)
        {
            retryOrFailLocked(i, monotonicMs_(), events);
        }
        return;
    }
}

void TextMessagingProtocol::handlePingLocked(const Frame& frame, float snr,
                                             std::vector<PendingEvent>& events)
{
    addSystemMessageLocked(frame.originCallsign + " >> " + myCallsign_ + " : PING!",
                           frame.originCallsign, events);

    if (autoReplyEnabled_) queuePongLocked(frame.originCallsign, snr);
}

void TextMessagingProtocol::handlePongLocked(const Frame& frame, float snr,
                                             std::vector<PendingEvent>& events)
{
    std::string heardBy;
    if (!frame.payload.empty())
    {
        heardBy = ", heard you at " + formatSnr(decodeSnr(frame.payload[0])) + " dB";
    }

    addSystemMessageLocked(frame.originCallsign + " >> " + myCallsign_ + " : PONG! (" +
                               formatSnr(snr) + " dB" + heardBy + ")",
                           frame.originCallsign, events);

    for (auto it = outbox_.begin(); it != outbox_.end(); ++it)
    {
        if (!it->isPing) continue;
        if (it->destination != frame.originCallsign) continue;

        updateStatusLocked(*it, MessageStatus::Acknowledged, events);
        outbox_.erase(it);
        return;
    }
}

void TextMessagingProtocol::purgeStaleReassembliesLocked(uint64_t nowMs)
{
    for (auto it = inbox_.begin(); it != inbox_.end();)
    {
        if (nowMs - it->second.lastHeardMs > (uint64_t)REASSEMBLY_TIMEOUT_MILLISECONDS)
        {
            it = inbox_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (auto it = recentlyCompleted_.begin(); it != recentlyCompleted_.end();)
    {
        if (nowMs - it->second.atMs > DUPLICATE_WINDOW_MS)
        {
            it = recentlyCompleted_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

AckWait TextMessagingProtocol::ackWait() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (const PendingTransmission& pending : outbox_)
    {
        if (!pending.expectsAck) continue;

        // Nothing is outstanding until it has actually been sent once. A
        // message still waiting its turn is queued, not awaited, and saying
        // otherwise would overwrite the notice that says so.
        if (pending.state != TransmissionState::AwaitingAck && pending.retries == 0 &&
            pending.confirmed == 0)
        {
            continue;
        }

        return pending.isPing ? AckWait::Ping : AckWait::Message;
    }

    return AckWait::Nothing;
}

bool TextMessagingProtocol::isTransmitting() const
{
    ITextMessagingTransport* transport = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        transport = transport_;
    }

    // Asked outside the lock: the transport reaches into the audio pipeline,
    // which has no business waiting on the protocol's mutex.
    return transport != nullptr && transport->isTransmitting();
}

// When the transmitter may next be used. Beyond the plain turnaround, a
// message we have sent and not yet had answered buys the far end room to
// answer it: it waits out its own turnaround first and then sends a whole
// burst, none of which we can hear while keyed. The window disappears by
// itself when the acknowledgement arrives, because the entry goes with it.
//
// A reply we owe -- an acknowledgement or a pong -- waits out the turnarounds
// but not that window. The station we are answering has just finished a burst
// and is waiting on a window of exactly the same length before it keys again,
// so a reply held for our own window goes out at the very moment that
// station's window expires, and the two collide. The bench showed it: a pong
// held five seconds keyed one second before the far end's next message. The
// cost is that a reply to one station can key while a slow acknowledgement
// from another is just starting; carrier sense covers that once the other
// burst is more than a second old.
uint64_t TextMessagingProtocol::quietUntilLocked(bool forReply) const
{
    uint64_t quietUntil = quietUntilMs_;
    if (forReply) return quietUntil;

    for (const PendingTransmission& pending : outbox_)
    {
        if (pending.state != TransmissionState::AwaitingAck) continue;

        uint64_t replyBy = pending.sentAtMs + (uint64_t)REPLY_WINDOW_MILLISECONDS;
        if (replyBy > quietUntil) quietUntil = replyBy;
    }

    return quietUntil;
}

// Whether the queue is frozen because somebody else has the channel: the
// receiver says so, or a fragmented message we are part way through hearing
// has more bursts to come. A spell that outlasts any real transmission is a
// receiver false triggering, not traffic, and is ignored until it clears so
// it cannot silence us for good.
bool TextMessagingProtocol::channelFrozenLocked(uint64_t nowMs)
{
    bool busy = (transport_ != nullptr && transport_->isChannelBusy()) ||
                nowMs < channelReservedUntilMs_;

    if (!busy)
    {
        // Everybody who heard that burst comes unfrozen at the same instant,
        // and two of them keying together cannot sense each other. A random
        // moment's pause spreads them out.
        if (channelBusy_) deferTransmissionLocked(nowMs, 0, TURNAROUND_JITTER_MILLISECONDS);
        channelBusy_ = false;
        return false;
    }

    if (!channelBusy_)
    {
        channelBusy_ = true;
        channelBusySinceMs_ = nowMs;
    }

    return nowMs - channelBusySinceMs_ < (uint64_t)MAX_CHANNEL_BUSY_MILLISECONDS;
}

// Time spent frozen does not count against anything outstanding. The
// acknowledgement deadline moves back so nothing retries or gives up while the
// reply could not have reached us, and so does the start of the reply window:
// when a third station's burst ends, both ends of our exchange come unfrozen
// together, and the far end is owed its turn to answer before we key again.
void TextMessagingProtocol::holdTimersLocked(uint64_t pausedMs)
{
    for (PendingTransmission& pending : outbox_)
    {
        if (pending.state != TransmissionState::AwaitingAck) continue;

        pending.deadlineMs += pausedMs;
        pending.sentAtMs += pausedMs;
    }
}

void TextMessagingProtocol::deferTransmissionLocked(uint64_t nowMs, int baseMs, int jitterMs)
{
    uint64_t until = nowMs + (uint64_t)baseMs + randomDelayLocked(jitterMs);
    if (until > quietUntilMs_) quietUntilMs_ = until;
}

// A delay in [0, maxMs] from the station's own sequence, so any two stations
// draw differently and no draw repeats the last.
uint32_t TextMessagingProtocol::randomDelayLocked(int maxMs)
{
    if (maxMs <= 0) return 0;

    jitterState_ = jitterState_ * 1103515245u + 12345u;
    return (jitterState_ >> 16) % (uint32_t)(maxMs + 1);
}

void TextMessagingProtocol::tick()
{
    std::vector<PendingEvent> events;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        uint64_t nowMs = monotonicMs_();
        purgeStaleReassembliesLocked(nowMs);
        requestMissingFragmentsLocked(nowMs);

        // While the channel is busy everything waits, timers included: the far
        // end cannot answer us through somebody else's burst, and it may be the
        // answer itself that is coming in.
        bool frozen = channelFrozenLocked(nowMs);
        if (frozen && lastTickMs_ != 0 && nowMs > lastTickMs_) holdTimersLocked(nowMs - lastTickMs_);
        lastTickMs_ = nowMs;

        if (transport_ != nullptr && !outbox_.empty()) serviceOutboxLocked(nowMs, frozen, events);
    }

    deliver(events);
}

void TextMessagingProtocol::serviceOutboxLocked(uint64_t nowMs, bool frozen,
                                                std::vector<PendingEvent>& events)
{
    // Voice always wins the transmitter, and only one burst is on the air at a
    // time. A transmission parked on its acknowledgement timer does not count:
    // the transmitter is idle for the whole of that wait, so the traffic queued
    // behind it keeps moving, and handleAckLocked matches an acknowledgement to
    // any entry rather than just the first.
    if (!transport_->isTransmitting())
    {
        // The burst we handed to the transport has finished. Acknowledgements
        // push to the front, so it is not necessarily the first entry.
        for (size_t i = 0; i < outbox_.size(); i++)
        {
            PendingTransmission& sent = outbox_[i];
            if (sent.state != TransmissionState::Transmitting) continue;

            // Having answered somebody, give them the channel: see the note
            // on REPLY_WINDOW_MILLISECONDS.
            deferTransmissionLocked(nowMs,
                                    sent.reply ? REPLY_WINDOW_MILLISECONDS
                                               : TURNAROUND_AFTER_TX_MILLISECONDS,
                                    TURNAROUND_JITTER_MILLISECONDS);

            sent.sentAtMs = nowMs;

            if (sent.expectsAck)
            {
                sent.state = TransmissionState::AwaitingAck;
                sent.deadlineMs = nowMs + (uint64_t)(sent.isPing ? PING_TIMEOUT_MILLISECONDS
                                                                 : ACK_TIMEOUT_MILLISECONDS);
                updateStatusLocked(sent, MessageStatus::AwaitingAck, events);
            }
            else
            {
                updateStatusLocked(sent, MessageStatus::Sent, events);
                outbox_.erase(outbox_.begin() + (std::ptrdiff_t)i);
            }
            break;
        }

        // The turnaround is the far end's turn; anything queued waits it out.
        // Only the start of a burst is held back -- the acknowledgement timers
        // below keep running, so a retry is never late because of it. An entry
        // still backing off from a retry steps aside for whatever is behind it.
        if (!frozen)
        {
            for (size_t i = 0; i < outbox_.size(); i++)
            {
                PendingTransmission& next = outbox_[i];
                if (next.state != TransmissionState::Queued) continue;
                if (nowMs < next.notBeforeMs) continue;
                if (nowMs < quietUntilLocked(next.reply)) continue;

                std::vector<OutgoingBurst> keying = keyingBurstsLocked(next);
                if (!keying.empty() && transport_->transmit(keying))
                {
                    next.state = TransmissionState::Transmitting;
                    updateStatusLocked(next, MessageStatus::Transmitting, events);
                }
                break;
            }
        }
    }

    // Expired acknowledgement timers, checked even while keying so that a
    // message gets no extra grace just because something else is on the air.
    for (size_t i = 0; i < outbox_.size();)
    {
        PendingTransmission& waiting = outbox_[i];
        if (waiting.state != TransmissionState::AwaitingAck || nowMs < waiting.deadlineMs)
        {
            i++;
            continue;
        }

        if (!retryOrFailLocked(i, nowMs, events)) i++;
    }
}

// An attempt that got nothing through. A ping gets one chance; a message gets
// the retries the operator can see counting up in the chat window, and goes
// back to the queue to wait its turn like any other transmission. Returns true
// if the entry was removed from the outbox.
bool TextMessagingProtocol::retryOrFailLocked(size_t index, uint64_t nowMs,
                                              std::vector<PendingEvent>& events)
{
    PendingTransmission& waiting = outbox_[index];

    if (!waiting.isPing && waiting.retries < MAX_MESSAGE_RETRIES)
    {
        waiting.retries++;
        waiting.state = TransmissionState::Queued;
        waiting.notBeforeMs = nowMs + randomDelayLocked(RETRY_BACKOFF_MILLISECONDS * waiting.retries);
        updateStatusLocked(waiting, MessageStatus::Retrying, events);
        return false;
    }

    if (waiting.isPing)
    {
        addSystemMessageLocked(waiting.destination + " : no response to PING",
                               waiting.destination, events);
    }

    updateStatusLocked(waiting, MessageStatus::Failed, events);
    outbox_.erase(outbox_.begin() + (std::ptrdiff_t)index);
    return true;
}

// The bursts for one keying. A message sends the fragments not yet confirmed,
// in order, each saying how many of them are still to come; everything else is
// the single burst it was queued as.
std::vector<OutgoingBurst> TextMessagingProtocol::keyingBurstsLocked(
    const PendingTransmission& pending) const
{
    std::vector<OutgoingBurst> keying;

    if (pending.fragments.empty())
    {
        BurstMode mode = pending.signalling ? BurstMode::Signalling : BurstMode::Text;
        for (const std::vector<uint8_t>& frame : pending.frames) keying.push_back({mode, frame});
        return keying;
    }

    std::vector<const Frame*> outstanding;
    for (size_t index = 0; index < pending.fragments.size(); index++)
    {
        if ((pending.confirmed & (1u << index)) == 0) outstanding.push_back(&pending.fragments[index]);
    }

    for (size_t position = 0; position < outstanding.size(); position++)
    {
        Frame frame = *outstanding[position];
        frame.burstsFollowing = (uint8_t)(outstanding.size() - 1 - position);

        // Every fragment encoded when the message was queued, with at least as
        // many following as it can have here, so this does not fail.
        keying.push_back({BurstMode::Text, FrameCodec::encode(frame, TEXT_FRAME_BYTES)});
        if (keying.back().frame.empty()) return {};
    }

    return keying;
}

void TextMessagingProtocol::deliver(const std::vector<PendingEvent>& events)
{
    ITextMessagingObserver* observer = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        observer = observer_;
    }

    if (observer == nullptr) return;

    for (const PendingEvent& event : events)
    {
        switch (event.type)
        {
            case PendingEvent::Type::MessageAdded:
                observer->onMessageAdded(event.message);
                break;
            case PendingEvent::Type::MessageUpdated:
                observer->onMessageUpdated(event.message);
                break;
            case PendingEvent::Type::StationsChanged:
                observer->onStationsChanged();
                break;
        }
    }
}

} // namespace TextMessaging
