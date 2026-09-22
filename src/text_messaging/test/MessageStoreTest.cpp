//=========================================================================
// Name:            MessageStoreTest.cpp
// Purpose:         Exercises chat persistence and the heard station list.
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

#include "../HeardStationList.h"
#include "../MessageStore.h"

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

constexpr std::time_t NOW = 1750000000;

TextMessage makeSentMessage(const std::string& text, std::time_t when)
{
    TextMessage message;
    message.airId = 0x1234;
    message.originCallsign = "W1AW";
    message.destCallsign = "VK3ABC";
    message.text = text;
    message.timestamp = when;
    message.direction = MessageDirection::Sent;
    message.status = MessageStatus::Queued;
    return message;
}

void testMessageRoundTrip()
{
    MessageStore store;
    CHECK(store.open(":memory:"));
    CHECK(store.isOpen());

    TextMessage sent = makeSentMessage("Good morning from the chat window", NOW);
    CHECK(store.addMessage(sent));
    CHECK(sent.id > 0);

    TextMessage received;
    received.originCallsign = "VK3ABC";
    received.destCallsign = "W1AW";
    received.text = "Morning!";
    received.timestamp = NOW + 30;
    received.direction = MessageDirection::Received;
    received.status = MessageStatus::Received;
    received.snr = -3.5f;
    CHECK(store.addMessage(received));
    CHECK(received.id != sent.id);

    std::vector<TextMessage> messages = store.recentMessages(50);
    CHECK(messages.size() == 2);

    // Oldest first, so the chat window can append in order.
    CHECK(messages[0].id == sent.id);
    CHECK(messages[1].id == received.id);

    CHECK(messages[0].text == "Good morning from the chat window");
    CHECK(messages[0].originCallsign == "W1AW");
    CHECK(messages[0].destCallsign == "VK3ABC");
    CHECK(messages[0].airId == 0x1234);
    CHECK(messages[0].direction == MessageDirection::Sent);
    CHECK(messages[0].status == MessageStatus::Queued);
    CHECK(!messages[0].broadcast);

    CHECK(messages[1].direction == MessageDirection::Received);
    CHECK(messages[1].status == MessageStatus::Received);
    CHECK(messages[1].snr < -3.0f && messages[1].snr > -4.0f);

    // Status changes are what the retry chips in the chat window read.
    CHECK(store.updateMessageStatus(sent.id, MessageStatus::Retrying, 2));
    messages = store.recentMessages(50);
    CHECK(messages[0].status == MessageStatus::Retrying);
    CHECK(messages[0].retryCount == 2);

    CHECK(store.updateMessageStatus(sent.id, MessageStatus::Acknowledged, 2));
    messages = store.recentMessages(50);
    CHECK(messages[0].status == MessageStatus::Acknowledged);
}

void testMessageLimitAndPrune()
{
    MessageStore store;
    CHECK(store.open(":memory:"));

    for (int i = 0; i < 10; i++)
    {
        TextMessage message = makeSentMessage("message " + std::to_string(i), NOW + i);
        CHECK(store.addMessage(message));
    }

    // The limit has to keep the newest messages, not the first ones written.
    std::vector<TextMessage> messages = store.recentMessages(3);
    CHECK(messages.size() == 3);
    CHECK(messages[0].text == "message 7");
    CHECK(messages[2].text == "message 9");

    CHECK(store.pruneMessagesOlderThan(NOW + 5));
    messages = store.recentMessages(50);
    CHECK(messages.size() == 5);
    CHECK(messages[0].text == "message 5");
}

void testHeardStationPersistence()
{
    MessageStore store;
    CHECK(store.open(":memory:"));

    HeardStation station;
    station.callsign = "VK3ABC";
    station.snr = 4.0f;
    station.lastHeard = NOW;
    CHECK(store.upsertHeardStation(station));

    // Hearing the same station again updates it rather than duplicating it.
    station.snr = 7.5f;
    station.lastHeard = NOW + 60;
    CHECK(store.upsertHeardStation(station));

    HeardStation other;
    other.callsign = "W1AW";
    other.snr = 1.0f;
    other.lastHeard = NOW + 120;
    CHECK(store.upsertHeardStation(other));

    std::vector<HeardStation> stations = store.heardStations();
    CHECK(stations.size() == 2);
    CHECK(stations[0].callsign == "W1AW"); // most recently heard first
    CHECK(stations[1].callsign == "VK3ABC");
    CHECK(stations[1].snr > 7.0f);

    CHECK(store.pruneHeardStationsOlderThan(NOW + 100));
    stations = store.heardStations();
    CHECK(stations.size() == 1);
    CHECK(stations[0].callsign == "W1AW");
}

void testClosedStoreFails()
{
    MessageStore store;
    TextMessage message = makeSentMessage("nowhere to go", NOW);

    CHECK(!store.isOpen());
    CHECK(!store.addMessage(message));
    CHECK(!store.lastError().empty());
    CHECK(store.recentMessages(10).empty());
}

void testHeardStationList()
{
    HeardStationList list(600);

    CHECK(list.heard("vk3abc", 3.0f, NOW));           // normalized on the way in
    CHECK(!list.heard("VK3ABC", 5.0f, NOW + 10));     // same station, not new
    CHECK(list.heard("W1AW", -2.0f, NOW + 20));
    CHECK(!list.heard("!!!", 0.0f, NOW));             // nothing usable in it

    CHECK(list.contains("VK3ABC"));
    CHECK(list.contains("w1aw"));
    CHECK(!list.contains("DJ2LS"));

    std::vector<HeardStation> stations = list.stations();
    CHECK(stations.size() == 2);
    CHECK(stations[0].callsign == "W1AW");
    CHECK(stations[1].callsign == "VK3ABC");
    CHECK(stations[1].snr > 4.9f); // latest decode's SNR won

    // A stale decode must not drag the last heard time backwards.
    CHECK(!list.heard("W1AW", 99.0f, NOW - 500));
    stations = list.stations();
    CHECK(stations[0].callsign == "W1AW");
    CHECK(stations[0].lastHeard == NOW + 20);

    CHECK(list.prune(NOW + 100) == 0);
    CHECK(list.prune(NOW + 615) == 1); // VK3ABC last heard at NOW + 10
    CHECK(!list.contains("VK3ABC"));
    CHECK(list.contains("W1AW"));

    std::vector<HeardStation> restored;
    HeardStation fresh;
    fresh.callsign = "DJ2LS";
    fresh.snr = 6.0f;
    fresh.lastHeard = NOW + 900;
    restored.push_back(fresh);

    HeardStation stale;
    stale.callsign = "G0ABC";
    stale.snr = 6.0f;
    stale.lastHeard = NOW; // older than the 600 second window
    restored.push_back(stale);

    list.restore(restored, NOW + 1000);
    CHECK(list.stations().size() == 1);
    CHECK(list.contains("DJ2LS"));

    list.clear();
    CHECK(list.stations().empty());
}

void testPinnedStations()
{
    HeardStationList list(600);
    HeardStation station;

    CHECK(!list.pin("   "));                          // nothing usable in it
    CHECK(list.pin("dj2ls"));
    CHECK(list.contains("DJ2LS"));
    CHECK(list.find("DJ2LS", station));
    CHECK(station.callsign == "DJ2LS");
    CHECK(station.pinned);
    CHECK(station.lastHeard == 0);                    // never heard

    // Pinned by hand, so time does not remove it.
    CHECK(list.prune(NOW + 100000) == 0);
    CHECK(list.contains("DJ2LS"));

    // Hearing it fills in the decode without unpinning it.
    CHECK(!list.heard("DJ2LS", 4.0f, NOW));           // already listed
    CHECK(list.find("DJ2LS", station));
    CHECK(station.pinned);
    CHECK(station.snr > 3.9f);
    CHECK(station.lastHeard == NOW);
    CHECK(list.prune(NOW + 100000) == 0);

    // Pinning a station already heard keeps its decode.
    CHECK(list.heard("W1AW", -1.0f, NOW + 20));
    CHECK(list.pin("W1AW"));
    CHECK(list.find("W1AW", station));
    CHECK(station.pinned);
    CHECK(station.lastHeard == NOW + 20);

    // A never heard station sorts after everything that has been.
    CHECK(list.pin("G0ABC"));
    std::vector<HeardStation> stations = list.stations();
    CHECK(stations.size() == 3);
    CHECK(stations[0].callsign == "W1AW");
    CHECK(stations[1].callsign == "DJ2LS");
    CHECK(stations[2].callsign == "G0ABC");

    // A restore replaces the heard entries and leaves the pinned ones, filling
    // in the decode of a pinned station the store knew about.
    std::vector<HeardStation> restored;
    HeardStation heardPinned;
    heardPinned.callsign = "W1AW";
    heardPinned.snr = 7.0f;
    heardPinned.lastHeard = NOW + 50;
    restored.push_back(heardPinned);

    HeardStation fresh;
    fresh.callsign = "VK3ABC";
    fresh.snr = 6.0f;
    fresh.lastHeard = NOW + 40;
    restored.push_back(fresh);

    CHECK(list.heard("K1ABC", 1.0f, NOW + 30));       // heard only, so replaced
    list.restore(restored, NOW + 60);
    CHECK(list.stations().size() == 4);
    CHECK(list.contains("DJ2LS"));
    CHECK(list.contains("G0ABC"));
    CHECK(list.contains("VK3ABC"));
    CHECK(!list.contains("K1ABC"));
    CHECK(list.find("W1AW", station));
    CHECK(station.pinned);
    CHECK(station.lastHeard == NOW + 50);

    // Removal is the only way out, and works on heard entries too.
    CHECK(list.remove("dj2ls"));
    CHECK(!list.remove("DJ2LS"));                     // already gone
    CHECK(!list.contains("DJ2LS"));
    CHECK(!list.find("DJ2LS", station));
    CHECK(list.remove("VK3ABC"));
    CHECK(!list.contains("VK3ABC"));
    CHECK(list.prune(NOW + 100000) == 0);             // nothing unpinned is left
    CHECK(list.stations().size() == 2);
}

} // namespace

int main()
{
    testMessageRoundTrip();
    testMessageLimitAndPrune();
    testHeardStationPersistence();
    testClosedStoreFails();
    testHeardStationList();
    testPinnedStations();

    if (failures > 0)
    {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }

    printf("all text messaging store checks passed\n");
    return 0;
}
