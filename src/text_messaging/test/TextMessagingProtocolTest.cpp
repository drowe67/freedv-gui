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
#include <string>
#include <vector>

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
    bool transmit(const std::vector<std::vector<uint8_t>>& frames, bool signalling) override
    {
        if (refuse) return false;

        transmissions.push_back(frames);
        signallingFlags.push_back(signalling);
        transmitting = true;
        return true;
    }

    bool isTransmitting() const override { return transmitting || voiceActive; }

    std::vector<std::vector<std::vector<uint8_t>>> transmissions;
    std::vector<bool> signallingFlags;
    bool transmitting = false;
    bool voiceActive = false;
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
    // turnaround first, since a real station would simply have waited.
    void completeOneTransmission()
    {
        nowMs += MAX_TURNAROUND_MILLISECONDS + 1;
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
    CHECK(!sender.transport.signallingFlags[0]);          // text goes in the wider mode

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
    CHECK(receiver.transport.signallingFlags[0]); // acknowledgements are signalling
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
    CHECK(sender.transport.signallingFlags[0]);

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
    testPingAndPong();
    testPingTimesOut();
    testAutoReplyCanBeDisabled();
    testAckWaitDoesNotBlockTheQueue();
    testTurnaroundKeepsStationsOffEachOther();
    testNextBurstWaitsForTheFarEndToAnswer();
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
