//=========================================================================
// Name:            MessageStore.h
// Purpose:         SQLite backed storage for chat messages and heard stations.
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

#ifndef TEXT_MESSAGING__MESSAGE_STORE_H
#define TEXT_MESSAGING__MESSAGE_STORE_H

#include <mutex>
#include <string>
#include <vector>

#include "TextMessagingTypes.h"

struct sqlite3;

namespace TextMessaging
{

// Chat history that survives a restart. Every method is safe to call from the
// GUI thread and the protocol thread at the same time; failures are reported
// by return value and described by lastError(), because this module is built
// without FreeDV's logging umbrella so that it can be unit tested on its own.
class MessageStore
{
public:
    MessageStore();
    ~MessageStore();

    MessageStore(const MessageStore&) = delete;
    MessageStore& operator=(const MessageStore&) = delete;

    // Opens (creating if needed) the database at the given path and applies
    // the schema. Pass ":memory:" for a throwaway database.
    bool open(const std::string& path);
    void close();
    bool isOpen() const;

    // Assigns message.id on success.
    bool addMessage(TextMessage& message);

    bool updateMessageStatus(int64_t id, MessageStatus status, int retryCount);

    // Most recent messages first in the database, returned oldest first so the
    // chat window can append them in order.
    std::vector<TextMessage> recentMessages(int limit);

    // Deletes messages older than the given age. Called at startup so a busy
    // station's history does not grow without bound.
    bool pruneMessagesOlderThan(std::time_t cutoff);

    bool upsertHeardStation(const HeardStation& station);
    std::vector<HeardStation> heardStations();
    bool pruneHeardStationsOlderThan(std::time_t cutoff);

    std::string lastError() const;

private:
    bool execute(const std::string& sql);
    bool applySchema();
    void setError(const std::string& context);

    mutable std::mutex mutex_;
    sqlite3* db_;
    std::string lastError_;
};

} // namespace TextMessaging

#endif // TEXT_MESSAGING__MESSAGE_STORE_H
