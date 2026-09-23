//=========================================================================
// Name:            TextMessagingProtocolTest.cpp
// Purpose:         Exercises the chat state machine against a fake radio.
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
#include <algorithm>
#include <string>
#include <vector>

#include "../DeliveryChip.h"
#include "../FrameCodec.h"
#include "../HeardStationList.h"
#include "../MessageStore.h"
#include "../TextMessagingProtocol.h"

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

// A radio that records what it was asked to send and finishes transmitting
// only when the test says so.
class FakeTransport : public ITextMessagingTransport
{
public:
    bool transmit(const std::vector<OutgoingBurst>& bursts) override
    {
        if (refuse) return false;

        transmissions.emplace_back();
        modes.emplace_back();
        for (const OutgoingBurst& burst : bursts)
        {
            transmissions.back().push_back(burst.frame);
            modes.back().push_back(burst.mode);
        }
        transmitting = true;
        return true;
    }

    bool isTransmitting() const override { return transmitting || voiceActive; }
    bool isChannelBusy() const override { return channelBusy; }

    std::vector<std::vector<std::vector<uint8_t>>> transmissions; // frames, per keying
    std::vector<std::vector<BurstMode>> modes;                    // and their modes
    bool transmitting = false;
    bool voiceActive = false;
    bool channelBusy = false;
    bool refuse = false;
};

class RecordingObserver : public ITextMessagingObserver
{
public:
    void onMessageAdded(const TextMessage& message) override { added.push_back(message); }
    void onMessageUpdated(const TextMessage& message) override { updated.push_back(message); }
    void onStationsChanged() override { stationsChanged++; }

    const TextMessage* lastUpdateFor(int64_t id) const
    {
        for (auto it = updated.rbegin(); it != updated.rend(); ++it)
        {
            if (it->id == id) return &(*it);
        }
        return nullptr;
    }

    std::vector<TextMessage> added;
    std::vector<TextMessage> updated;
    int stationsChanged = 0;
};

// Wraps everything a test station needs, with clocks the test drives.
struct Station
{
    explicit Station(const std::string& callsign)
        : protocol(store, stations)
    {
        check(store.open(":memory:"), "store.open", __LINE__);
        protocol.setTransport(&transport);
        protocol.setObserver(&observer);
        protocol.setMyCallsign(callsign);
        protocol.setClocks([this]() { return nowMs; }, [this]() { return (std::time_t)1750000000; });
    }

    // Runs the protocol far enough to put a queued transmission on the air and
    // see it through to the end of the over. The clock skips past any
    // turnaround or retry backoff first, since a real station would simply
    // have waited.
    void completeOneTransmission()
    {
        nowMs += std::max(MAX_TURNAROUND_MILLISECONDS, MAX_RETRY_BACKOFF_MILLISECONDS) + 1;
        protocol.tick(); // hands the burst to the transport
        transport.transmitting = false;
        protocol.tick(); // the over has finished
    }

    // Decodes every frame of the most recent transmission into this station.
    void receiveFrom(FakeTransport& other, float snr = 5.0f)
    {
        CHECK(!other.transmissions.empty());
        for (const std::vector<uint8_t>& raw : other.transmissions.back())
        {
            Frame frame;
            CHECK(FrameCodec::decode(raw.data(), (int)raw.size(), frame));
            protocol.onFrameReceived(frame, snr);
        }
    }

    MessageStore store;
    HeardStationList stations;
    FakeTransport transport;
    RecordingObserver observer;
    TextMessagingProtocol protocol;
    uint64_t nowMs = 1000;
};

void testAddressedMessageIsAcknowledged()
{
    Station sender("W1AW");
    Station receiver("VK3ABC");

    std::string error;
    CHECK(sender.protocol.sendMessage("Hello over there", "VK3ABC", error));
    CHECK(error.empty());
    CHECK(sender.observer.added.size() == 1);
    CHECK(sender.observer.added[0].status == MessageStatus::Queued);
    CHECK(sender.observer.added[0].destCallsign == "VK3ABC");
    CHECK(!sender.observer.added[0].broadcast);

    sender.completeOneTransmission();
    CHECK(sender.transport.transmissions.size() == 1);
    CHECK(sender.transport.transmissions[0].size() == 1); // short message, one fragment
    CHECK(sender.transport.modes[0][0] == BurstMode::Text);          // text goes in the wider mode

    int64_t messageId = sender.observer.added[0].id;
    const TextMessage* update = sender.observer.lastUpdateFor(messageId);
    CHECK(update != nullptr && update->status == MessageStatus::AwaitingAck);

    // The receiver decodes it, shows it, and queues an acknowledgement.
    receiver.receiveFrom(sender.transport, 6.5f);
    CHECK(receiver.observer.added.size() == 1);
    CHECK(receiver.observer.added[0].text == "Hello over there");
    CHECK(receiver.observer.added[0].originCallsign == "W1AW");
    CHECK(receiver.observer.added[0].direction == MessageDirection::Received);
    CHECK(receiver.observer.stationsChanged > 0);
    CHECK(receiver.stations.contains("W1AW"));
    CHECK(receiver.protocol.pendingCount() == 1);

    receiver.completeOneTransmission();
    CHECK(receiver.transport.modes[0][0] == BurstMode::Signalling); // acknowledgements are signalling
    CHECK(receiver.protocol.pendingCount() == 0);

    sender.receiveFrom(receiver.transport);
    update = sender.observer.lastUpdateFor(messageId);
    CHECK(update != nullptr && update->status == MessageStatus::Acknowledged);
    CHECK(sender.protocol.pendingCount() == 0);
}

void testRetriesThenFails()
{
    Station sender("W1AW");

    std::string error;
    CHECK(sender.protocol.sendMessage("Anybody there", "VK3ABC", error));
    int64_t messageId = sender.observer.added[0].id;

    for (int attempt = 0; attempt <= MAX_MESSAGE_RETRIES; attempt++)
    {
        sender.completeOneTransmission();
        CHECK((int)sender.transport.transmissions.size() == attempt + 1);

        // Nothing comes back before the timer expires.
        sender.nowMs += ACK_TIMEOUT_MILLISECONDS + 1;
        sender.protocol.tick();

        const TextMessage* update = sender.observer.lastUpdateFor(messageId);
        CHECK(update != nullptr);
        if (attempt < MAX_MESSAGE_RETRIES)
        {
            CHECK(update->status == MessageStatus::Retrying);
            CHECK(update->retryCount == attempt + 1);
        }
        else
        {
            CHECK(update->status == MessageStatus::Failed);
        }
    }

    CHECK((int)sender.transport.transmissions.size() == MAX_MESSAGE_RETRIES + 1);
    CHECK(sender.protocol.pendingCount() == 0);
}

void testBroadcastIsNotAcknowledged()
{
    Station sender("W1AW");
    Station receiver("VK3ABC");

    std::string error;
    CHECK(sender.protocol.sendMessage("CQ CQ from the chat window", "", error));
    CHECK(sender.observer.added[0].broadcast);

    sender.completeOneTransmission();

    int64_t messageId = sender.observer.added[0].id;
    const TextMessage* update = sender.observer.lastUpdateFor(messageId);
    CHECK(update != nullptr && update->status == MessageStatus::Sent);
    CHECK(sender.protocol.pendingCount() == 0);

    // Any station decodes a broadcast, and none of them answer it.
    receiver.receiveFrom(sender.transport);
    CHECK(receiver.observer.added.size() == 1);
    CHECK(receiver.observer.added[0].broadcast);
    CHECK(receiver.protocol.pendingCount() == 0);
}

void testMessageNotForUsIsIgnored()
{
    Station sender("W1AW");
    Station bystander("DJ2LS");

    std::string error;
    CHECK(sender.protocol.sendMessage("Private note", "VK3ABC", error));
    sender.completeOneTransmission();

    bystander.receiveFrom(sender.transport);

    // The station is still worth listing as heard, but the text is not ours.
    CHECK(bystander.stations.contains("W1AW"));
    CHECK(bystander.observer.added.empty());
    CHECK(bystander.protocol.pendingCount() == 0);
}

void testLongMessageIsFragmentedAndReassembled()
{
    Station sender("W1AW");
    Station receiver("VK3ABC");

    std::string body(TEXT_BYTES_PER_FRAGMENT * 2 + 10, 'A');
    std::string error;
    CHECK(sender.protocol.sendMessage(body, "VK3ABC", error));
    sender.completeOneTransmission();
    CHECK(sender.transport.transmissions[0].size() == 3);

    receiver.receiveFrom(sender.transport);
    CHECK(receiver.observer.added.size() == 1);
    CHECK(receiver.observer.added[0].text == body);

    std::string tooLong(MAX_MESSAGE_TEXT_BYTES + 1, 'B');
    CHECK(!sender.protocol.sendMessage(tooLong, "VK3ABC", error));
    CHECK(!error.empty());
}

void testRetransmissionIsNotShownTwice()
{
    Station sender("W1AW");
    Station receiver("VK3ABC");

    std::string error;
    CHECK(sender.protocol.sendMessage("Say again", "VK3ABC", error));
    sender.completeOneTransmission();

    receiver.receiveFrom(sender.transport);
    CHECK(receiver.observer.added.size() == 1);
    receiver.completeOneTransmission(); // first acknowledgement

    // The acknowledgement was lost, so the sender transmits the same message
    // again: the receiver must re-acknowledge without duplicating the line.
    sender.nowMs += ACK_TIMEOUT_MILLISECONDS + 1;
    sender.protocol.tick();
    sender.completeOneTransmission();

    receiver.receiveFrom(sender.transport);
    CHECK(receiver.observer.added.size() == 1);
    CHECK(receiver.protocol.pendingCount() == 1);
}

void testPingAndPong()
{
    Station sender("W1AW");
    Station receiver("VK3ABC");

    std::string error;
    CHECK(sender.protocol.sendPing("VK3ABC", error));
    CHECK(sender.observer.added.size() == 1);
    CHECK(sender.observer.added[0].kind == MessageKind::System);
    CHECK(sender.observer.added[0].text == "W1AW >> VK3ABC : PING!");

    sender.completeOneTransmission();
    CHECK(sender.transport.modes[0][0] == BurstMode::Signalling);

    receiver.receiveFrom(sender.transport, 8.0f);
    CHECK(receiver.observer.added.size() == 1);
    CHECK(receiver.observer.added[0].text == "W1AW >> VK3ABC : PING!");
    CHECK(receiver.protocol.pendingCount() == 1); // the pong

    receiver.completeOneTransmission();
    sender.receiveFrom(receiver.transport, 4.0f);

    CHECK(sender.observer.added.size() == 2);
    const TextMessage& pong = sender.observer.added[1];
    CHECK(pong.kind == MessageKind::System);
    CHECK(pong.text.find("VK3ABC >> W1AW : PONG!") == 0);
    CHECK(pong.text.find("heard you at 8.0 dB") != std::string::npos);
    CHECK(sender.protocol.pendingCount() == 0);
}

void testPingTimesOut()
{
    Station sender("W1AW");

    std::string error;
    CHECK(sender.protocol.sendPing("VK3ABC", error));
    sender.completeOneTransmission();

    sender.nowMs += PING_TIMEOUT_MILLISECONDS + 1;
    sender.protocol.tick();

    // One transmission only: a ping is not worth retrying behind the
    // operator's back.
    CHECK(sender.transport.transmissions.size() == 1);
    CHECK(sender.observer.added.size() == 2);
    CHECK(sender.observer.added[1].text.find("no response to PING") != std::string::npos);
    CHECK(sender.protocol.pendingCount() == 0);
}

void testAutoReplyCanBeDisabled()
{
    Station sender("W1AW");
    Station receiver("VK3ABC");
    receiver.protocol.setAutoReplyEnabled(false);
    CHECK(!receiver.protocol.autoReplyEnabled());

    std::string error;
    CHECK(sender.protocol.sendMessage("Are you listening", "VK3ABC", error));
    sender.completeOneTransmission();

    receiver.receiveFrom(sender.transport);
    CHECK(receiver.observer.added.size() == 1); // the message still shows
    CHECK(receiver.protocol.pendingCount() == 0); // but nothing is transmitted

    CHECK(sender.protocol.sendPing("VK3ABC", error));
    sender.completeOneTransmission();
    receiver.receiveFrom(sender.transport);
    CHECK(receiver.observer.added.size() == 2);
    CHECK(receiver.protocol.pendingCount() == 0);
}

// A transmission parked on its acknowledgement timer leaves the transmitter
// idle, so the traffic queued behind it has to keep moving. Before this was
// fixed, one unanswered message stalled the whole outbox for the entire retry
// cycle, and the operator's next message or ping simply never went out.
void testAckWaitDoesNotBlockTheQueue()
{
    Station sender("W1AW");

    std::string error;
    CHECK(sender.protocol.sendMessage("First", "VK3ABC", error));
    sender.completeOneTransmission();

    int64_t firstId = sender.observer.added[0].id;
    const TextMessage* update = sender.observer.lastUpdateFor(firstId);
    CHECK(update != nullptr && update->status == MessageStatus::AwaitingAck);

    // Queued while the first message is still waiting to be acknowledged.
    CHECK(sender.protocol.sendMessage("Second", "VK3ABC", error));
    sender.completeOneTransmission();
    CHECK(sender.transport.transmissions.size() == 2);
    CHECK(sender.protocol.pendingCount() == 2);

    int64_t secondId = sender.observer.added[1].id;
    uint16_t secondAirId = sender.observer.added[1].airId;
    update = sender.observer.lastUpdateFor(secondId);
    CHECK(update != nullptr && update->status == MessageStatus::AwaitingAck);

    // The second message is acknowledged first. It sits behind the first one
    // in the queue, and the acknowledgement has to find it there and leave the
    // other message's retry timer running.
    Frame ack;
    ack.type = FrameType::MessageAck;
    ack.originCallsign = "VK3ABC";
    ack.destinationCrc = FrameCodec::callsignCrc24("W1AW");
    ack.airId = secondAirId;
    sender.protocol.onFrameReceived(ack, 5.0f);

    update = sender.observer.lastUpdateFor(secondId);
    CHECK(update != nullptr && update->status == MessageStatus::Acknowledged);
    CHECK(sender.protocol.pendingCount() == 1);

    update = sender.observer.lastUpdateFor(firstId);
    CHECK(update != nullptr && update->status == MessageStatus::AwaitingAck);

    // The unacknowledged one still retries on its own timer.
    sender.nowMs += ACK_TIMEOUT_MILLISECONDS + 1;
    sender.protocol.tick();
    update = sender.observer.lastUpdateFor(firstId);
    CHECK(update != nullptr && update->status == MessageStatus::Retrying);
}

void testVoiceTransmissionDefersChat()
{
    Station sender("W1AW");
    sender.transport.voiceActive = true;

    std::string error;
    CHECK(sender.protocol.sendMessage("Wait your turn", "VK3ABC", error));

    sender.protocol.tick();
    CHECK(sender.transport.transmissions.empty());

    sender.transport.voiceActive = false;
    sender.completeOneTransmission();
    CHECK(sender.transport.transmissions.size() == 1);
}

// Two half duplex stations that key the moment the other stops cannot hear
// each other. On the loopback bench both stations keyed within the same second
// and each missed the other's reply, so a reply waits for the sender's receiver
// to come back, and the next burst waits for the far end to have its turn.
void testTurnaroundKeepsStationsOffEachOther()
{
    Station sender("W1AW");
    Station receiver("VK3ABC");

    std::string error;
    CHECK(sender.protocol.sendMessage("Hello", "VK3ABC", error));
    sender.completeOneTransmission();

    // The acknowledgement is queued, but keying now would land on top of a
    // sender that is still turning its receiver back on.
    receiver.receiveFrom(sender.transport);
    CHECK(receiver.protocol.pendingCount() == 1);
    receiver.protocol.tick();
    CHECK(receiver.transport.transmissions.empty());

    receiver.nowMs += TURNAROUND_AFTER_RX_MILLISECONDS + 1;
    receiver.protocol.tick();
    CHECK(receiver.transport.transmissions.size() == 1);
}

// The other half of the same problem: having just transmitted, we owe the far
// end room to answer before starting whatever else is queued.
void testNextBurstWaitsForTheFarEndToAnswer()
{
    Station sender("W1AW");

    std::string error;
    CHECK(sender.protocol.sendMessage("first", "VK3ABC", error));
    sender.completeOneTransmission();
    CHECK(sender.transport.transmissions.size() == 1);

    // Queued while the far end is presumably composing its acknowledgement.
    CHECK(sender.protocol.sendMessage("second", "VK3ABC", error));
    sender.protocol.tick();
    CHECK(sender.transport.transmissions.size() == 1);

    sender.nowMs += MAX_TURNAROUND_MILLISECONDS + 1;
    sender.protocol.tick();
    CHECK(sender.transport.transmissions.size() == 2);
}

// The status line asks what we are waiting for, and it has to keep saying so
// for the whole acknowledgement cycle. Reporting only while the timer runs
// would blink the notice off every time the message went back on the air.
void testAckWaitCoversTheWholeCycle()
{
    Station sender("W1AW");
    CHECK(sender.protocol.ackWait() == AckWait::Nothing);

    std::string error;
    // Queued but never sent: waiting its turn, not waiting on a reply.
    CHECK(sender.protocol.sendMessage("Anybody there", "VK3ABC", error));
    CHECK(sender.protocol.ackWait() == AckWait::Nothing);

    sender.completeOneTransmission();
    CHECK(sender.protocol.ackWait() == AckWait::Message);

    // Still a message we are waiting on, part way through the retries.
    sender.nowMs += ACK_TIMEOUT_MILLISECONDS + 1;
    sender.protocol.tick();
    CHECK(sender.protocol.ackWait() == AckWait::Message);

    for (int attempt = 1; attempt <= MAX_MESSAGE_RETRIES; attempt++)
    {
        sender.completeOneTransmission();
        sender.nowMs += ACK_TIMEOUT_MILLISECONDS + 1;
        sender.protocol.tick();
    }

    // Given up on: nothing is outstanding, so the status line goes quiet.
    CHECK(sender.protocol.ackWait() == AckWait::Nothing);

    // A ping is distinguishable, because the window names it separately.
    CHECK(sender.protocol.sendPing("VK3ABC", error));
    sender.completeOneTransmission();
    CHECK(sender.protocol.ackWait() == AckWait::Ping);

    // A broadcast expects nothing back and must not claim otherwise.
    Station broadcaster("W1AW");
    CHECK(broadcaster.protocol.sendMessage("CQ", "", error));
    broadcaster.completeOneTransmission();
    CHECK(broadcaster.protocol.ackWait() == AckWait::Nothing);
}

// The collision the bench caught: a station finished a message at 14:29:23,
// the far end began acknowledging it at 14:29:24, and the station keyed again
// at 14:29:25 and never heard the reply. A burst that asked for an
// acknowledgement has to leave room for a whole burst coming back, not just
// for our own changeover.
void testAWaitedReplyOutlastsThePlainTurnaround()
{
    Station sender("W1AW");

    std::string error;
    CHECK(sender.protocol.sendMessage("needs an ack", "VK3ABC", error));
    sender.completeOneTransmission();
    CHECK(sender.transport.transmissions.size() == 1);

    // Something else the operator queued while the reply is outstanding.
    CHECK(sender.protocol.sendPing("VK3ABC", error));

    // Past the wait a reply-free burst would have earned, and this is exactly
    // where the far end is mid-acknowledgement.
    sender.nowMs += TURNAROUND_AFTER_TX_MILLISECONDS + TURNAROUND_JITTER_MILLISECONDS + 1;
    sender.protocol.tick();
    CHECK(sender.transport.transmissions.size() == 1);

    // Once the reply has had its chance, the queue moves again.
    sender.nowMs += REPLY_WINDOW_MILLISECONDS;
    sender.protocol.tick();
    CHECK(sender.transport.transmissions.size() == 2);
}

// The window is room for a reply, not a fixed delay: once the acknowledgement
// is in, the transmitter is free as soon as the ordinary turnaround is up.
void testTheWindowEndsWhenTheReplyArrives()
{
    Station sender("W1AW");
    Station receiver("VK3ABC");

    std::string error;
    CHECK(sender.protocol.sendMessage("needs an ack", "VK3ABC", error));
    sender.completeOneTransmission();

    receiver.receiveFrom(sender.transport);
    receiver.completeOneTransmission();
    sender.receiveFrom(receiver.transport);

    CHECK(sender.protocol.pendingCount() == 0);

    // Nothing is outstanding now, so the ordinary turnaround is all that
    // stands between us and the next burst. Were the window still running it
    // would take REPLY_WINDOW_MILLISECONDS, which is far longer than this.
    CHECK(sender.protocol.sendPing("VK3ABC", error));
    sender.nowMs += TURNAROUND_AFTER_TX_MILLISECONDS + TURNAROUND_JITTER_MILLISECONDS + 1;
    sender.protocol.tick();
    CHECK(sender.transport.transmissions.size() == 2);
}

// A reply we owe is not held for our own reply window. On the bench a station
// waiting on an acknowledgement decoded a ping, held the pong five seconds for
// its own window, and keyed it one second before the pinging station, whose
// identical window had just expired, keyed its next message over the top.
void testReplyIsNotHeldForOurOwnReplyWindow()
{
    Station sender("W1AW");
    Station other("DJ2LS");

    std::string error;
    CHECK(sender.protocol.sendMessage("needs an ack", "VK3ABC", error));
    sender.completeOneTransmission();
    CHECK(sender.transport.transmissions.size() == 1);

    // Somebody else pings us while VK3ABC's reply is still owed.
    CHECK(other.protocol.sendPing("W1AW", error));
    other.completeOneTransmission();
    sender.receiveFrom(other.transport);

    // Past the turnarounds but well inside the reply window, which is where
    // testAWaitedReplyOutlastsThePlainTurnaround shows a message still waits.
    CHECK(TURNAROUND_AFTER_TX_MILLISECONDS + TURNAROUND_JITTER_MILLISECONDS + 1 <
          REPLY_WINDOW_MILLISECONDS);
    sender.nowMs += TURNAROUND_AFTER_TX_MILLISECONDS + TURNAROUND_JITTER_MILLISECONDS + 1;
    sender.protocol.tick();
    CHECK(sender.transport.transmissions.size() == 2);
    CHECK(sender.transport.modes.back()[0] == BurstMode::Signalling); // the pong
}

// A retry backs off by a random amount before keying again, bounded by the
// attempt number, and different stations draw differently: on the bench two
// stations' retry timers expired in the same second and they keyed together,
// too close for either to sense the other's carrier.
void testRetryBacksOffBeforeKeyingAgain()
{
    const char* callsigns[] = {"W1AW", "VK3ABC", "DJ2LS", "G0ABC", "K1ABC", "TEST1/P"};
    std::vector<uint64_t> firstBackoffs;

    for (const char* callsign : callsigns)
    {
        Station sender(callsign);

        std::string error;
        CHECK(sender.protocol.sendMessage("Anybody there", "VK3XYZ", error));

        for (int attempt = 1; attempt <= MAX_MESSAGE_RETRIES; attempt++)
        {
            sender.completeOneTransmission();
            size_t sentSoFar = sender.transport.transmissions.size();

            // The timer expires; the retry is queued but must not key at once
            // unless its backoff happened to be zero.
            sender.nowMs += ACK_TIMEOUT_MILLISECONDS + 1;
            uint64_t expiredAt = sender.nowMs;
            uint64_t bound = (uint64_t)RETRY_BACKOFF_MILLISECONDS * attempt;

            uint64_t keyedAfter = 0;
            for (;;)
            {
                sender.protocol.tick();
                if (sender.transport.transmissions.size() > sentSoFar)
                {
                    keyedAfter = sender.nowMs - expiredAt;
                    break;
                }
                CHECK(sender.nowMs - expiredAt <= bound); // never later than the bound
                if (sender.nowMs - expiredAt > bound) break;
                sender.nowMs += 100;
            }

            CHECK(keyedAfter <= bound);
            if (attempt == 1) firstBackoffs.push_back(keyedAfter);
        }
    }

    // Six stations with identical traffic must not all key at the same moment,
    // and the backoff has to be real, not a bound that is never used.
    bool allEqual = true;
    uint64_t largest = 0;
    for (uint64_t backoff : firstBackoffs)
    {
        if (backoff != firstBackoffs[0]) allEqual = false;
        if (backoff > largest) largest = backoff;
    }
    CHECK(!allEqual);
    CHECK(largest > 0);
}

// Having answered a station, we give it the channel for a whole reply window
// before starting traffic of our own. On the bench the acknowledged station
// keyed two seconds after our acknowledgement ended, exactly when the plain
// turnaround let us key, and the two collided four times in one run.
void testAReplyGivesTheOtherStationTheChannel()
{
    Station sender("W1AW");
    Station receiver("VK3ABC");

    std::string error;
    CHECK(sender.protocol.sendMessage("first", "VK3ABC", error));
    sender.completeOneTransmission();
    receiver.receiveFrom(sender.transport); // queues our acknowledgement

    CHECK(receiver.protocol.sendMessage("mine", "W1AW", error));
    receiver.completeOneTransmission();
    CHECK(receiver.transport.transmissions.size() == 1);
    CHECK(receiver.transport.modes.back()[0] == BurstMode::Signalling); // the acknowledgement went first

    // Past the plain turnaround: still theirs.
    receiver.nowMs += TURNAROUND_AFTER_TX_MILLISECONDS + TURNAROUND_JITTER_MILLISECONDS + 1;
    receiver.protocol.tick();
    CHECK(receiver.transport.transmissions.size() == 1);

    // Past the window: ours.
    receiver.nowMs += REPLY_WINDOW_MILLISECONDS;
    receiver.protocol.tick();
    CHECK(receiver.transport.transmissions.size() == 2);
}

// A fragment says how many more bursts its sender holds the channel for,
// whatever the demodulator says in between: on the bench it read the channel
// clear for two seconds in the middle of a four fragment message.
void testFragmentsStillToComeReserveTheChannel()
{
    Station sender("W1AW");

    std::string error;
    std::string body(TEXT_BYTES_PER_FRAGMENT * 2 + 10, 'A'); // three fragments
    CHECK(sender.protocol.sendMessage(body, "VK3ABC", error));
    sender.completeOneTransmission();
    const auto& frames = sender.transport.transmissions[0];
    CHECK(frames.size() == 3);

    auto decodeInto = [&](Station& station, size_t index)
    {
        Frame frame;
        CHECK(FrameCodec::decode(frames[index].data(), (int)frames[index].size(), frame));
        station.protocol.onFrameReceived(frame, 5.0f);
    };

    // Only the first fragment has been heard: two more bursts are coming.
    Station partial("VK3ABC");
    CHECK(partial.protocol.sendMessage("waiting", "W1AW", error));
    uint64_t heardAt = partial.nowMs;
    decodeInto(partial, 0);

    partial.nowMs = heardAt + MAX_TURNAROUND_MILLISECONDS + 1;
    partial.protocol.tick();
    CHECK(partial.transport.transmissions.empty());

    // The reservation lapses, then the random pause every release carries.
    partial.nowMs = heardAt + 2 * TEXT_FRAGMENT_AIR_MILLISECONDS + 1;
    partial.protocol.tick();
    partial.nowMs += TURNAROUND_JITTER_MILLISECONDS + 1;
    partial.protocol.tick();
    CHECK(partial.transport.transmissions.size() == 1);

    // A message for somebody else holds the channel just the same: on the
    // bench a third station's eight fragment message ran forty four seconds.
    Station bystander("DJ2LS");
    CHECK(bystander.protocol.sendMessage("waiting", "W1AW", error));
    heardAt = bystander.nowMs;
    decodeInto(bystander, 0);
    bystander.nowMs = heardAt + MAX_TURNAROUND_MILLISECONDS + 1;
    bystander.protocol.tick();
    CHECK(bystander.transport.transmissions.empty());

    // The last fragment ends the sender's keying, even with one lost before it.
    Station lossy("VK3ABC");
    CHECK(lossy.protocol.sendMessage("waiting", "W1AW", error));
    decodeInto(lossy, 0);
    decodeInto(lossy, 2);
    lossy.nowMs += MAX_TURNAROUND_MILLISECONDS + 1;
    lossy.protocol.tick();
    CHECK(lossy.transport.transmissions.size() == 1);
}

// Everybody who heard a burst comes unfrozen at the same instant, so the
// release carries a random pause: stations with identical traffic must not
// all key together the moment the channel clears.
void testClearingChannelReleasesStationsAtDifferentMoments()
{
    const char* callsigns[] = {"W1AW", "VK3ABC", "DJ2LS", "G0ABC", "K1ABC", "TEST1/P"};
    std::vector<uint64_t> delays;

    for (const char* callsign : callsigns)
    {
        Station station(callsign);
        station.transport.channelBusy = true;

        std::string error;
        CHECK(station.protocol.sendMessage("Hold it", "VK3XYZ", error));
        station.nowMs += 2000;
        station.protocol.tick();
        CHECK(station.transport.transmissions.empty());

        station.transport.channelBusy = false;
        uint64_t clearedAt = station.nowMs;
        for (;;)
        {
            station.protocol.tick();
            if (!station.transport.transmissions.empty()) break;
            CHECK(station.nowMs - clearedAt <= TURNAROUND_JITTER_MILLISECONDS);
            if (station.nowMs - clearedAt > TURNAROUND_JITTER_MILLISECONDS) break;
            station.nowMs += 100;
        }
        delays.push_back(station.nowMs - clearedAt);
    }

    bool allEqual = true;
    for (uint64_t delay : delays)
    {
        if (delay != delays[0]) allEqual = false;
    }
    CHECK(!allEqual);
}

// A retransmitted message we already have arrives one fragment at a time, and
// every duplicate fragment asks for the acknowledgement again. One queued
// acknowledgement answers them all.
void testRetransmittedFragmentsQueueOneAcknowledgement()
{
    Station sender("W1AW");
    Station receiver("VK3ABC");

    std::string error;
    std::string body(TEXT_BYTES_PER_FRAGMENT * 2 + 10, 'A'); // three fragments
    CHECK(sender.protocol.sendMessage(body, "VK3ABC", error));
    sender.completeOneTransmission();
    receiver.receiveFrom(sender.transport);
    CHECK(receiver.observer.added.size() == 1);
    receiver.completeOneTransmission(); // the acknowledgement, lost on the way

    sender.nowMs += ACK_TIMEOUT_MILLISECONDS + 1;
    sender.protocol.tick();
    sender.completeOneTransmission();
    CHECK(sender.transport.transmissions.back().size() == 3);

    receiver.receiveFrom(sender.transport);
    CHECK(receiver.observer.added.size() == 1);
    CHECK(receiver.protocol.pendingCount() == 1);
}

// A sender that restarts must not have its new message taken for a
// retransmission of an old one that happened to carry the same ID: the
// receiver would acknowledge it and never show it, and the sender would see
// OK. The ID is forced to collide here, so the content check is what counts.
void testRestartedSenderIsNotMistakenForARetransmission()
{
    Station receiver("VK3ABC");
    std::string error;

    Station before("W1AW");
    CHECK(before.protocol.sendMessage("before the restart", "VK3ABC", error));
    before.completeOneTransmission();
    receiver.receiveFrom(before.transport);
    CHECK(receiver.observer.added.size() == 1);
    uint16_t reusedId = before.observer.added[0].airId;

    Station after("W1AW");
    CHECK(after.protocol.sendMessage("after the restart", "VK3ABC", error));
    after.completeOneTransmission();
    for (const std::vector<uint8_t>& raw : after.transport.transmissions.back())
    {
        Frame frame;
        CHECK(FrameCodec::decode(raw.data(), (int)raw.size(), frame));
        frame.airId = reusedId;
        receiver.protocol.onFrameReceived(frame, 5.0f);
    }

    CHECK(receiver.observer.added.size() == 2);
    CHECK(receiver.observer.added.back().text == "after the restart");

    // And a true retransmission of that new message is still recognised.
    for (const std::vector<uint8_t>& raw : after.transport.transmissions.back())
    {
        Frame frame;
        CHECK(FrameCodec::decode(raw.data(), (int)raw.size(), frame));
        frame.airId = reusedId;
        receiver.protocol.onFrameReceived(frame, 5.0f);
    }
    CHECK(receiver.observer.added.size() == 2);
}

// Message IDs start at a random point, so stations that restart do not all
// begin again at the same ID.
void testMessageIdsStartAtRandom()
{
    std::vector<uint16_t> firstIds;
    for (int i = 0; i < 4; i++)
    {
        Station sender("W1AW");
        std::string error;
        CHECK(sender.protocol.sendMessage("hello", "VK3ABC", error));
        firstIds.push_back(sender.observer.added[0].airId);
    }

    bool allEqual = true;
    for (uint16_t id : firstIds)
    {
        if (id != firstIds[0]) allEqual = false;
    }
    CHECK(!allEqual);
}

// Retries resend the same fragments, so a message can be pieced together from
// several attempts, each of which lost some of it to a fade. The partial copy
// has to last as long as fragments keep coming: timed from the first fragment
// it was dropped before a long message's second retry arrived.
void testRetriesFillInAMessageOverTime()
{
    Station sender("W1AW");
    std::string error;
    std::string body(TEXT_BYTES_PER_FRAGMENT * 2 + 10, 'A'); // three fragments
    CHECK(sender.protocol.sendMessage(body, "VK3ABC", error));
    sender.completeOneTransmission();
    const auto& frames = sender.transport.transmissions[0];
    CHECK(frames.size() == 3);

    auto decodeInto = [&](Station& station, size_t index)
    {
        Frame frame;
        CHECK(FrameCodec::decode(frames[index].data(), (int)frames[index].size(), frame));
        station.protocol.onFrameReceived(frame, 5.0f);
    };

    // One fragment per attempt, each attempt well inside the timeout of the
    // last but the whole span well beyond it.
    uint64_t step = REASSEMBLY_TIMEOUT_MILLISECONDS - 20000;
    Station receiver("VK3ABC");
    decodeInto(receiver, 0);
    receiver.nowMs += step;
    receiver.protocol.tick();
    decodeInto(receiver, 1);
    receiver.nowMs += step;
    receiver.protocol.tick();
    decodeInto(receiver, 2);
    CHECK(receiver.observer.added.size() == 1);
    CHECK(!receiver.observer.added.empty() && receiver.observer.added[0].text == body);

    // Silence for longer than the timeout still drops what was held.
    Station quiet("VK3ABC");
    decodeInto(quiet, 0);
    quiet.nowMs += REASSEMBLY_TIMEOUT_MILLISECONDS + 1;
    quiet.protocol.tick();
    decodeInto(quiet, 1);
    decodeInto(quiet, 2);
    CHECK(quiet.observer.added.empty());
}

// The reservation comes from what the frame says is still to come in its
// keying, not from its fragment number. A resend of fragments 2 and 6 of 8
// ends after fragment 6; "fragment k of n" would hold the channel for five
// more bursts after fragment 2 that are never coming.
void testReservationFollowsTheBurstsStillToCome()
{
    auto keyedAfter = [](Station& station, uint64_t heardAt, uint64_t limit) -> uint64_t
    {
        for (station.nowMs = heardAt; station.nowMs <= heardAt + limit; station.nowMs += 100)
        {
            station.protocol.tick();
            if (!station.transport.transmissions.empty()) return station.nowMs - heardAt;
        }
        return UINT64_MAX;
    };

    std::string error;
    Frame resend;
    resend.type = FrameType::Message;
    resend.destinationCrc = FrameCodec::callsignCrc24("K1ABC");
    resend.originCallsign = "DJ2LS";
    resend.airId = 0x4242;
    resend.fragmentIndex = 1;
    resend.fragmentCount = 8;
    resend.burstsFollowing = 1; // fragment 6 comes next, then the keying ends
    resend.payload.assign(1, 'x');

    Station bystander("VK3ABC");
    CHECK(bystander.protocol.sendMessage("waiting", "W1AW", error));
    uint64_t heardAt = bystander.nowMs;
    bystander.protocol.onFrameReceived(resend, 5.0f);
    uint64_t waited = keyedAfter(bystander, heardAt, 8 * TEXT_FRAGMENT_AIR_MILLISECONDS);
    CHECK(waited >= (uint64_t)TEXT_FRAGMENT_AIR_MILLISECONDS);
    CHECK(waited <= (uint64_t)(TEXT_FRAGMENT_AIR_MILLISECONDS + TURNAROUND_JITTER_MILLISECONDS + 100));

    // A signalling frame that says more follows holds the channel for the
    // text that comes after it, allowing for the first fragment being lost.
    Frame ack;
    ack.type = FrameType::MessageAck;
    ack.destinationCrc = FrameCodec::callsignCrc24("K1ABC");
    ack.originCallsign = "DJ2LS";
    ack.airId = 0x4243;
    ack.burstsFollowing = 1;

    Station listener("VK3ABC");
    CHECK(listener.protocol.sendMessage("waiting", "W1AW", error));
    heardAt = listener.nowMs;
    listener.protocol.onFrameReceived(ack, 5.0f);
    waited = keyedAfter(listener, heardAt, 4 * TEXT_FRAGMENT_AIR_MILLISECONDS);
    CHECK(waited >= (uint64_t)SIGNALLING_FOLLOWED_RESERVATION_MILLISECONDS);
    CHECK(waited <= (uint64_t)(SIGNALLING_FOLLOWED_RESERVATION_MILLISECONDS +
                               TURNAROUND_JITTER_MILLISECONDS + 100));

    // And a signalling frame with nothing after it reserves nothing.
    ack.burstsFollowing = 0;
    Station plain("VK3ABC");
    CHECK(plain.protocol.sendMessage("waiting", "W1AW", error));
    heardAt = plain.nowMs;
    plain.protocol.onFrameReceived(ack, 5.0f);
    waited = keyedAfter(plain, heardAt, 4 * TEXT_FRAGMENT_AIR_MILLISECONDS);
    CHECK(waited <= (uint64_t)MAX_TURNAROUND_MILLISECONDS);
}

Frame decodeOne(const std::vector<uint8_t>& raw)
{
    Frame frame;
    CHECK(FrameCodec::decode(raw.data(), (int)raw.size(), frame));
    return frame;
}

bool everUpdatedTo(const RecordingObserver& observer, int64_t id, MessageStatus status)
{
    for (const TextMessage& update : observer.updated)
    {
        if (update.id == id && update.status == status) return true;
    }
    return false;
}

// A receiver that heard part of a message tells the sender which fragments
// arrived once the sender's keying is over, and the sender resends just the
// rest, at once, without counting a retry.
void testMissingFragmentsAreAskedForAndResent()
{
    Station sender("W1AW");
    Station receiver("VK3ABC");

    std::string error;
    std::string body(TEXT_BYTES_PER_FRAGMENT * 2 + 10, 'A'); // three fragments
    CHECK(sender.protocol.sendMessage(body, "VK3ABC", error));
    int64_t id = sender.observer.added[0].id;
    sender.completeOneTransmission();
    std::vector<std::vector<uint8_t>> first = sender.transport.transmissions.back();
    CHECK(first.size() == 3);

    // Fragment two is lost to a fade; fragment three says the keying is over.
    receiver.protocol.onFrameReceived(decodeOne(first[0]), 5.0f);
    receiver.protocol.onFrameReceived(decodeOne(first[2]), 5.0f);
    CHECK(receiver.observer.added.empty());

    receiver.completeOneTransmission();
    CHECK(receiver.transport.transmissions.size() == 1);
    CHECK(receiver.transport.transmissions.back().size() == 1);
    Frame partial = decodeOne(receiver.transport.transmissions.back()[0]);
    CHECK(partial.type == FrameType::MessagePartialAck);
    CHECK(partial.airId == decodeOne(first[0]).airId);
    CHECK(partial.payload.size() == 1 && partial.payload[0] == 0x05);

    // The window learns how far the message got the moment the report arrives.
    sender.receiveFrom(receiver.transport);
    const TextMessage* progress = sender.observer.lastUpdateFor(id);
    CHECK(progress != nullptr && progress->fragmentsConfirmed == 2);
    CHECK(progress != nullptr && progress->fragmentCount == 3);
    CHECK(progress != nullptr && deliveryChipState(*progress).kind == DeliveryChipKind::Resend);

    sender.completeOneTransmission();
    CHECK(sender.transport.transmissions.size() == 2);
    CHECK(sender.transport.transmissions.back().size() == 1);
    Frame resent = decodeOne(sender.transport.transmissions.back()[0]);
    CHECK(resent.fragmentIndex == 1);
    CHECK(resent.fragmentCount == 3);
    CHECK(resent.burstsFollowing == 0);
    CHECK(!everUpdatedTo(sender.observer, id, MessageStatus::Retrying));

    // The resend completes the message; one plain acknowledgement goes back.
    receiver.receiveFrom(sender.transport);
    CHECK(receiver.observer.added.size() == 1);
    CHECK(!receiver.observer.added.empty() && receiver.observer.added[0].text == body);
    CHECK(receiver.protocol.pendingCount() == 1);
    receiver.completeOneTransmission();
    CHECK(decodeOne(receiver.transport.transmissions.back()[0]).type == FrameType::MessageAck);

    sender.receiveFrom(receiver.transport);
    const TextMessage* update = sender.observer.lastUpdateFor(id);
    CHECK(update != nullptr && update->status == MessageStatus::Acknowledged);
    CHECK(update != nullptr && update->retryCount == 0);
}

// With the last fragment lost there is nothing saying the keying ended, so
// the receiver waits for as long as the last fragment it heard said the rest
// would take, and then asks. A broadcast asks for nothing, and neither does a
// station that may not transmit on its own.
void testLostLastFragmentStillAsksForTheRest()
{
    Station sender("W1AW");
    std::string error;
    std::string body(TEXT_BYTES_PER_FRAGMENT * 2 + 10, 'A');
    CHECK(sender.protocol.sendMessage(body, "VK3ABC", error));
    CHECK(sender.protocol.sendMessage(body, "", error)); // and as a broadcast
    sender.completeOneTransmission();
    std::vector<std::vector<uint8_t>> addressed = sender.transport.transmissions.back();
    sender.completeOneTransmission();
    std::vector<std::vector<uint8_t>> broadcast = sender.transport.transmissions.back();
    CHECK(decodeOne(broadcast[0]).type == FrameType::Broadcast);

    Station receiver("VK3ABC");
    uint64_t heardAt = receiver.nowMs;
    receiver.protocol.onFrameReceived(decodeOne(addressed[0]), 5.0f); // two still to come
    receiver.nowMs = heardAt + 2 * TEXT_FRAGMENT_AIR_MILLISECONDS - 1;
    receiver.protocol.tick();
    CHECK(receiver.protocol.pendingCount() == 0);
    receiver.nowMs = heardAt + 2 * TEXT_FRAGMENT_AIR_MILLISECONDS;
    receiver.protocol.tick();
    CHECK(receiver.protocol.pendingCount() == 1);
    receiver.completeOneTransmission();
    CHECK(receiver.transport.transmissions.size() == 1);
    Frame partial = decodeOne(receiver.transport.transmissions.back()[0]);
    CHECK(partial.type == FrameType::MessagePartialAck);
    CHECK(partial.payload.size() == 1 && partial.payload[0] == 0x01);

    Station listener("VK3ABC");
    listener.protocol.onFrameReceived(decodeOne(broadcast[0]), 5.0f);
    listener.nowMs += 3 * TEXT_FRAGMENT_AIR_MILLISECONDS;
    listener.protocol.tick();
    CHECK(listener.protocol.pendingCount() == 0);

    Station unattended("VK3ABC");
    unattended.protocol.setAutoReplyEnabled(false);
    unattended.protocol.onFrameReceived(decodeOne(addressed[0]), 5.0f);
    unattended.nowMs += 3 * TEXT_FRAGMENT_AIR_MILLISECONDS;
    unattended.protocol.tick();
    CHECK(unattended.protocol.pendingCount() == 0);
}

// A report still waiting to go out must say what is true when it goes: a
// later keying that brings more fragments updates it rather than queuing a
// second, and one that completes the message replaces it with a plain
// acknowledgement.
void testQueuedReportsStayCurrent()
{
    Station sender("W1AW");
    std::string error;
    std::string body(TEXT_BYTES_PER_FRAGMENT * 2 + 10, 'D'); // three fragments
    CHECK(sender.protocol.sendMessage(body, "VK3ABC", error));
    sender.completeOneTransmission();
    std::vector<std::vector<uint8_t>> frames = sender.transport.transmissions.back();

    // Fragment one, then the keying ends with nothing more heard. Somebody is
    // still on the channel, so the report waits.
    Station receiver("VK3ABC");
    Frame first = decodeOne(frames[0]);
    receiver.protocol.onFrameReceived(first, 5.0f);
    receiver.transport.channelBusy = true;
    receiver.nowMs += (uint64_t)first.burstsFollowing * TEXT_FRAGMENT_AIR_MILLISECONDS;
    receiver.protocol.tick();
    CHECK(receiver.protocol.pendingCount() == 1);
    CHECK(receiver.transport.transmissions.empty());

    // Before the report can go out, another keying brings fragment three.
    receiver.protocol.onFrameReceived(decodeOne(frames[2]), 5.0f);
    receiver.protocol.tick();
    CHECK(receiver.protocol.pendingCount() == 1);
    receiver.transport.channelBusy = false;
    receiver.protocol.tick(); // the channel clears, and its random pause begins
    receiver.completeOneTransmission();
    CHECK(receiver.transport.transmissions.size() == 1);
    Frame report = decodeOne(receiver.transport.transmissions.back()[0]);
    CHECK(report.type == FrameType::MessagePartialAck);
    CHECK(report.payload.size() == 1 && report.payload[0] == 0x05);

    // Fragments one and three again, and the report is queued; then fragment
    // two completes the message before it is sent.
    Station overtaken("VK3ABC");
    overtaken.protocol.onFrameReceived(decodeOne(frames[0]), 5.0f);
    overtaken.protocol.onFrameReceived(decodeOne(frames[2]), 5.0f);
    overtaken.protocol.tick();
    CHECK(overtaken.protocol.pendingCount() == 1);
    CHECK(overtaken.transport.transmissions.empty()); // still in the turnaround
    overtaken.protocol.onFrameReceived(decodeOne(frames[1]), 5.0f);
    CHECK(overtaken.observer.added.size() == 1);
    CHECK(overtaken.protocol.pendingCount() == 1);
    overtaken.completeOneTransmission();
    CHECK(overtaken.transport.transmissions.size() == 1);
    CHECK(decodeOne(overtaken.transport.transmissions.back()[0]).type == FrameType::MessageAck);
}

// A message that gets one new fragment through per keying needs more keyings
// than it has retries. Each of those made progress, so none is a retry, and
// the message is finished rather than failed.
void testProgressDoesNotUseUpRetries()
{
    Station sender("W1AW");
    Station receiver("VK3ABC");

    std::string error;
    std::string body(TEXT_BYTES_PER_FRAGMENT * 4 + 10, 'B'); // five fragments
    CHECK(sender.protocol.sendMessage(body, "VK3ABC", error));
    int64_t id = sender.observer.added[0].id;
    CHECK(5 > MAX_MESSAGE_RETRIES + 1);

    for (int keying = 0; keying < 5; keying++)
    {
        sender.completeOneTransmission();
        const std::vector<std::vector<uint8_t>>& frames = sender.transport.transmissions.back();
        CHECK((int)frames.size() == 5 - keying);

        // Only the first burst of each keying gets through.
        Frame heard = decodeOne(frames[0]);
        CHECK(heard.fragmentIndex == keying);
        receiver.protocol.onFrameReceived(heard, 5.0f);
        if (keying == 4) break;

        receiver.nowMs += (uint64_t)heard.burstsFollowing * TEXT_FRAGMENT_AIR_MILLISECONDS;
        receiver.protocol.tick();
        receiver.completeOneTransmission();
        sender.receiveFrom(receiver.transport);
    }

    CHECK(receiver.observer.added.size() == 1);
    CHECK(!receiver.observer.added.empty() && receiver.observer.added[0].text == body);
    receiver.completeOneTransmission();
    sender.receiveFrom(receiver.transport);

    const TextMessage* update = sender.observer.lastUpdateFor(id);
    CHECK(update != nullptr && update->status == MessageStatus::Acknowledged);
    CHECK(!everUpdatedTo(sender.observer, id, MessageStatus::Retrying));
    CHECK(!everUpdatedTo(sender.observer, id, MessageStatus::Failed));
}

// A report that confirms nothing new means the last keying got nothing new
// through: that is a retry, backed off like one, and it resends only what the
// far end has still not confirmed.
void testPartialAckWithNoNewsCountsAsARetry()
{
    Station sender("W1AW");
    std::string error;
    std::string body(TEXT_BYTES_PER_FRAGMENT * 2 + 10, 'C'); // three fragments
    CHECK(sender.protocol.sendMessage(body, "VK3ABC", error));
    int64_t id = sender.observer.added[0].id;
    sender.completeOneTransmission();

    Frame partial;
    partial.type = FrameType::MessagePartialAck;
    partial.destinationCrc = FrameCodec::callsignCrc24("W1AW");
    partial.originCallsign = "VK3ABC";
    partial.airId = sender.observer.added[0].airId;
    partial.payload.assign(1, 0x01);

    sender.protocol.onFrameReceived(partial, 5.0f); // news: fragment one arrived
    sender.completeOneTransmission();
    CHECK(sender.transport.transmissions.back().size() == 2);
    CHECK(!everUpdatedTo(sender.observer, id, MessageStatus::Retrying));

    sender.protocol.onFrameReceived(partial, 5.0f); // the same again: no news
    const TextMessage* update = sender.observer.lastUpdateFor(id);
    CHECK(update != nullptr && update->status == MessageStatus::Retrying);
    CHECK(update != nullptr && update->retryCount == 1);

    sender.completeOneTransmission();
    CHECK(sender.transport.transmissions.size() == 3);
    CHECK(sender.transport.transmissions.back().size() == 2);
    CHECK(decodeOne(sender.transport.transmissions.back()[0]).fragmentIndex == 1);

    // A partial acknowledgement from anyone but the addressee changes nothing.
    Frame stranger = partial;
    stranger.originCallsign = "DJ2LS";
    stranger.payload.assign(1, 0x07);
    sender.protocol.onFrameReceived(stranger, 5.0f);
    update = sender.observer.lastUpdateFor(id);
    CHECK(update != nullptr && update->status != MessageStatus::Acknowledged);
}

// Which delivery chip a sent message shows. SENDING and SENT belong to the
// first attempt alone; a retry keeps its number through the whole attempt so
// the chip never appears to go backwards; a message the far end holds part
// of says so, and how much.
void testDeliveryChip()
{
    auto kindOf = [](MessageStatus status, int retry, int confirmed, int count)
    {
        TextMessage message;
        message.status = status;
        message.retryCount = retry;
        message.fragmentsConfirmed = confirmed;
        message.fragmentCount = count;
        return deliveryChipState(message);
    };

    CHECK(kindOf(MessageStatus::Queued, 0, 0, 3).kind == DeliveryChipKind::Queued);
    CHECK(kindOf(MessageStatus::Transmitting, 0, 0, 3).kind == DeliveryChipKind::Sending);
    CHECK(kindOf(MessageStatus::AwaitingAck, 0, 0, 3).kind == DeliveryChipKind::Sent);
    CHECK(kindOf(MessageStatus::Sent, 0, 0, 3).kind == DeliveryChipKind::Sent);
    CHECK(kindOf(MessageStatus::Received, 0, 0, 0).kind == DeliveryChipKind::None);
    CHECK(kindOf(MessageStatus::Acknowledged, 2, 3, 3).kind == DeliveryChipKind::Acknowledged);

    for (MessageStatus status : {MessageStatus::Retrying, MessageStatus::Transmitting,
                                 MessageStatus::AwaitingAck})
    {
        DeliveryChipState retry = kindOf(status, 2, 0, 3);
        CHECK(retry.kind == DeliveryChipKind::Retry);
        CHECK(retry.retry == 2);
        CHECK(!retry.showsProgress());

        // Part delivered, no failed attempt: a resend, with how far it got.
        DeliveryChipState resend = kindOf(status, 0, 5, 8);
        CHECK(resend.kind == DeliveryChipKind::Resend);
        CHECK(resend.showsProgress());
        CHECK(resend.fragmentsConfirmed == 5 && resend.fragmentCount == 8);

        // A failed attempt after progress keeps both.
        DeliveryChipState both = kindOf(status, 1, 5, 8);
        CHECK(both.kind == DeliveryChipKind::Retry);
        CHECK(both.showsProgress());
    }

    // Out of retries part way through still says how far it got.
    DeliveryChipState failed = kindOf(MessageStatus::Failed, 3, 5, 8);
    CHECK(failed.kind == DeliveryChipKind::NotAcknowledged);
    CHECK(failed.showsProgress());

    // Nothing or everything confirmed is not progress worth showing.
    CHECK(!kindOf(MessageStatus::AwaitingAck, 0, 0, 8).showsProgress());
    CHECK(!kindOf(MessageStatus::Acknowledged, 0, 8, 8).showsProgress());
}

// Carrier sense: while the receiver is locked onto somebody else's burst,
// nothing we have queued may start, however long it has been waiting. Once
// it clears, the queue moves again within the random pause a release carries.
void testBusyChannelFreezesTheQueue()
{
    Station sender("W1AW");
    sender.transport.channelBusy = true;

    std::string error;
    CHECK(sender.protocol.sendMessage("Hold it", "VK3ABC", error));

    for (int i = 0; i < 10; i++)
    {
        sender.nowMs += 1000;
        sender.protocol.tick();
    }
    CHECK(sender.transport.transmissions.empty());

    sender.transport.channelBusy = false;
    sender.protocol.tick();
    sender.nowMs += TURNAROUND_JITTER_MILLISECONDS + 1;
    sender.protocol.tick();
    CHECK(sender.transport.transmissions.size() == 1);
}

// The far end cannot answer us through somebody else's burst, so time spent
// frozen must not count against the acknowledgement: nothing retries during
// it, and the timer resumes rather than expiring the moment the channel clears.
void testBusyChannelHoldsTheAcknowledgementTimer()
{
    Station sender("W1AW");

    std::string error;
    CHECK(sender.protocol.sendMessage("Anybody there", "VK3ABC", error));
    sender.completeOneTransmission();
    int64_t id = sender.observer.added[0].id;

    // Busy for longer than the whole acknowledgement timeout.
    sender.transport.channelBusy = true;
    for (int i = 0; i < 20; i++)
    {
        sender.nowMs += 1000;
        sender.protocol.tick();
    }

    const TextMessage* update = sender.observer.lastUpdateFor(id);
    CHECK(update != nullptr && update->status == MessageStatus::AwaitingAck);
    CHECK(update != nullptr && update->retryCount == 0);

    sender.transport.channelBusy = false;
    sender.protocol.tick();
    update = sender.observer.lastUpdateFor(id);
    CHECK(update != nullptr && update->status == MessageStatus::AwaitingAck);

    sender.nowMs += ACK_TIMEOUT_MILLISECONDS + 1;
    sender.protocol.tick();
    update = sender.observer.lastUpdateFor(id);
    CHECK(update != nullptr && update->status == MessageStatus::Retrying);
}

// A third station's burst freezes both ends of our exchange, and both come
// unfrozen at the same moment. The far end is owed its turn to answer first,
// so the reply window has to be held through the busy spell as well; were it
// not, it would already have lapsed and we would key over the late reply.
void testBusyChannelHoldsTheReplyWindow()
{
    Station sender("W1AW");

    std::string error;
    CHECK(sender.protocol.sendMessage("needs an ack", "VK3ABC", error));
    sender.completeOneTransmission();

    sender.transport.channelBusy = true;
    for (int i = 0; i < 10; i++)
    {
        sender.nowMs += 1000;
        sender.protocol.tick();
    }
    sender.transport.channelBusy = false;

    CHECK(sender.protocol.sendPing("VK3ABC", error));
    sender.nowMs += TURNAROUND_AFTER_TX_MILLISECONDS + TURNAROUND_JITTER_MILLISECONDS + 1;
    sender.protocol.tick();
    CHECK(sender.transport.transmissions.size() == 1);

    sender.nowMs += REPLY_WINDOW_MILLISECONDS;
    sender.protocol.tick();
    CHECK(sender.transport.transmissions.size() == 2);
}

// A receiver that never drops sync is false triggering on noise, not hearing a
// transmission. It must not be able to silence the station for good.
void testChannelThatNeverClearsIsEventuallyIgnored()
{
    Station sender("W1AW");
    sender.transport.channelBusy = true;

    std::string error;
    CHECK(sender.protocol.sendMessage("Still here", "VK3ABC", error));

    sender.protocol.tick();
    sender.nowMs += MAX_CHANNEL_BUSY_MILLISECONDS - 1000;
    sender.protocol.tick();
    CHECK(sender.transport.transmissions.empty());

    sender.nowMs += 2000;
    sender.protocol.tick();
    CHECK(sender.transport.transmissions.size() == 1);
}

void testSendRequiresCallsign()
{
    MessageStore store;
    CHECK(store.open(":memory:"));
    HeardStationList stations;
    TextMessagingProtocol protocol(store, stations);
    FakeTransport transport;
    protocol.setTransport(&transport);

    std::string error;
    CHECK(!protocol.sendMessage("Hello", "VK3ABC", error));
    CHECK(!error.empty());

    protocol.setMyCallsign("W1AW");
    error.clear();
    CHECK(!protocol.sendMessage("   ", "VK3ABC", error));
    CHECK(!error.empty());
    CHECK(!protocol.sendPing("", error));
}

} // namespace

int main()
{
    testAddressedMessageIsAcknowledged();
    testRetriesThenFails();
    testBroadcastIsNotAcknowledged();
    testMessageNotForUsIsIgnored();
    testLongMessageIsFragmentedAndReassembled();
    testRetransmissionIsNotShownTwice();
    testRetransmittedFragmentsQueueOneAcknowledgement();
    testRestartedSenderIsNotMistakenForARetransmission();
    testMessageIdsStartAtRandom();
    testRetriesFillInAMessageOverTime();
    testMissingFragmentsAreAskedForAndResent();
    testLostLastFragmentStillAsksForTheRest();
    testQueuedReportsStayCurrent();
    testProgressDoesNotUseUpRetries();
    testPartialAckWithNoNewsCountsAsARetry();
    testDeliveryChip();
    testPingAndPong();
    testPingTimesOut();
    testAutoReplyCanBeDisabled();
    testAckWaitDoesNotBlockTheQueue();
    testTurnaroundKeepsStationsOffEachOther();
    testNextBurstWaitsForTheFarEndToAnswer();
    testAWaitedReplyOutlastsThePlainTurnaround();
    testTheWindowEndsWhenTheReplyArrives();
    testReplyIsNotHeldForOurOwnReplyWindow();
    testRetryBacksOffBeforeKeyingAgain();
    testAReplyGivesTheOtherStationTheChannel();
    testFragmentsStillToComeReserveTheChannel();
    testReservationFollowsTheBurstsStillToCome();
    testClearingChannelReleasesStationsAtDifferentMoments();
    testAckWaitCoversTheWholeCycle();
    testBusyChannelFreezesTheQueue();
    testBusyChannelHoldsTheAcknowledgementTimer();
    testBusyChannelHoldsTheReplyWindow();
    testChannelThatNeverClearsIsEventuallyIgnored();
    testVoiceTransmissionDefersChat();
    testSendRequiresCallsign();

    if (failures > 0)
    {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }

    printf("all text messaging protocol checks passed\n");
    return 0;
}
