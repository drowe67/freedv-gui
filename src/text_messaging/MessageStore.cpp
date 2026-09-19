//=========================================================================
// Name:            MessageStore.cpp
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

#include "MessageStore.h"

#include <algorithm>

#include "sqlite3.h"

namespace TextMessaging
{

namespace
{

// Bumped whenever the schema changes; a database from a newer FreeDV is left
// alone rather than being written to with the wrong shape.
constexpr int SCHEMA_VERSION = 1;

const char* const SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS messages ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  air_id INTEGER NOT NULL,"
    "  origin TEXT NOT NULL,"
    "  destination TEXT NOT NULL,"
    "  broadcast INTEGER NOT NULL,"
    "  body TEXT NOT NULL,"
    "  timestamp INTEGER NOT NULL,"
    "  direction INTEGER NOT NULL,"
    "  status INTEGER NOT NULL,"
    "  retry_count INTEGER NOT NULL,"
    "  snr REAL NOT NULL);"
    "CREATE INDEX IF NOT EXISTS messages_timestamp ON messages(timestamp);"
    "CREATE TABLE IF NOT EXISTS heard_stations ("
    "  callsign TEXT PRIMARY KEY,"
    "  snr REAL NOT NULL,"
    "  last_heard INTEGER NOT NULL);";

// Statuses are persisted as integers, so the mapping cannot follow the enum's
// declaration order; it has to be explicit and stable.
int statusToInt(MessageStatus status)
{
    switch (status)
    {
        case MessageStatus::Queued: return 0;
        case MessageStatus::Transmitting: return 1;
        case MessageStatus::AwaitingAck: return 2;
        case MessageStatus::Retrying: return 3;
        case MessageStatus::Acknowledged: return 4;
        case MessageStatus::Failed: return 5;
        case MessageStatus::Sent: return 6;
        case MessageStatus::Received: return 7;
    }
    return 0;
}

MessageStatus intToStatus(int value)
{
    switch (value)
    {
        case 0: return MessageStatus::Queued;
        case 1: return MessageStatus::Transmitting;
        case 2: return MessageStatus::AwaitingAck;
        case 3: return MessageStatus::Retrying;
        case 4: return MessageStatus::Acknowledged;
        case 5: return MessageStatus::Failed;
        case 6: return MessageStatus::Sent;
        case 7: return MessageStatus::Received;
        default: return MessageStatus::Failed;
    }
}

std::string columnText(sqlite3_stmt* statement, int column)
{
    const unsigned char* text = sqlite3_column_text(statement, column);
    if (text == nullptr) return "";
    return std::string(reinterpret_cast<const char*>(text));
}

} // namespace

MessageStore::MessageStore()
    : db_(nullptr)
{
    // empty
}

MessageStore::~MessageStore()
{
    close();
}

bool MessageStore::open(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (db_ != nullptr)
    {
        lastError_ = "database is already open";
        return false;
    }

    int result = sqlite3_open_v2(
        path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
        nullptr);
    if (result != SQLITE_OK)
    {
        setError("opening " + path);

        // sqlite3_open_v2 hands back a handle even on failure so that the
        // error can be read from it; it still has to be closed.
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    if (!applySchema())
    {
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    return true;
}

void MessageStore::close()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (db_ != nullptr)
    {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool MessageStore::isOpen() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return db_ != nullptr;
}

bool MessageStore::applySchema()
{
    // Chat writes are small and infrequent; WAL keeps a reader in the GUI
    // thread from blocking the protocol thread's writes.
    if (!execute("PRAGMA journal_mode=WAL;")) return false;

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db_, "PRAGMA user_version;", -1, &statement, nullptr) != SQLITE_OK)
    {
        setError("reading schema version");
        return false;
    }

    int existingVersion = 0;
    if (sqlite3_step(statement) == SQLITE_ROW) existingVersion = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);

    // A database written by a newer FreeDV may have columns we would not fill
    // in; refuse it rather than corrupt the user's history.
    if (existingVersion > SCHEMA_VERSION)
    {
        lastError_ = "message store was written by a newer version of FreeDV";
        return false;
    }

    if (!execute(SCHEMA_SQL)) return false;

    return execute("PRAGMA user_version=" + std::to_string(SCHEMA_VERSION) + ";");
}

bool MessageStore::execute(const std::string& sql)
{
    char* errorText = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errorText) != SQLITE_OK)
    {
        lastError_ = errorText != nullptr ? errorText : "unknown SQLite error";
        sqlite3_free(errorText);
        return false;
    }

    return true;
}

void MessageStore::setError(const std::string& context)
{
    const char* message = db_ != nullptr ? sqlite3_errmsg(db_) : "database is not open";
    lastError_ = context + ": " + (message != nullptr ? message : "unknown SQLite error");
}

bool MessageStore::addMessage(TextMessage& message)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (db_ == nullptr)
    {
        lastError_ = "database is not open";
        return false;
    }

    const char* sql =
        "INSERT INTO messages (air_id, origin, destination, broadcast, body, timestamp,"
        " direction, status, retry_count, snr) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        setError("preparing message insert");
        return false;
    }

    sqlite3_bind_int(statement, 1, message.airId);
    sqlite3_bind_text(statement, 2, message.originCallsign.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement, 3, message.destCallsign.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(statement, 4, message.broadcast ? 1 : 0);
    sqlite3_bind_text(statement, 5, message.text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement, 6, (sqlite3_int64)message.timestamp);
    sqlite3_bind_int(statement, 7, message.direction == MessageDirection::Sent ? 0 : 1);
    sqlite3_bind_int(statement, 8, statusToInt(message.status));
    sqlite3_bind_int(statement, 9, message.retryCount);
    sqlite3_bind_double(statement, 10, message.snr);

    bool ok = sqlite3_step(statement) == SQLITE_DONE;
    if (!ok) setError("inserting message");
    sqlite3_finalize(statement);

    if (ok) message.id = (int64_t)sqlite3_last_insert_rowid(db_);

    return ok;
}

bool MessageStore::updateMessageStatus(int64_t id, MessageStatus status, int retryCount)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (db_ == nullptr)
    {
        lastError_ = "database is not open";
        return false;
    }

    const char* sql = "UPDATE messages SET status = ?, retry_count = ? WHERE id = ?;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        setError("preparing status update");
        return false;
    }

    sqlite3_bind_int(statement, 1, statusToInt(status));
    sqlite3_bind_int(statement, 2, retryCount);
    sqlite3_bind_int64(statement, 3, (sqlite3_int64)id);

    bool ok = sqlite3_step(statement) == SQLITE_DONE;
    if (!ok) setError("updating message status");
    sqlite3_finalize(statement);

    return ok;
}

std::vector<TextMessage> MessageStore::recentMessages(int limit)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<TextMessage> messages;
    if (db_ == nullptr)
    {
        lastError_ = "database is not open";
        return messages;
    }

    const char* sql =
        "SELECT id, air_id, origin, destination, broadcast, body, timestamp, direction,"
        " status, retry_count, snr FROM messages ORDER BY id DESC LIMIT ?;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        setError("preparing message query");
        return messages;
    }

    sqlite3_bind_int(statement, 1, limit);

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        TextMessage message;
        message.id = (int64_t)sqlite3_column_int64(statement, 0);
        message.airId = (uint16_t)sqlite3_column_int(statement, 1);
        message.originCallsign = columnText(statement, 2);
        message.destCallsign = columnText(statement, 3);
        message.broadcast = sqlite3_column_int(statement, 4) != 0;
        message.text = columnText(statement, 5);
        message.timestamp = (std::time_t)sqlite3_column_int64(statement, 6);
        message.direction = sqlite3_column_int(statement, 7) == 0
            ? MessageDirection::Sent
            : MessageDirection::Received;
        message.status = intToStatus(sqlite3_column_int(statement, 8));
        message.retryCount = sqlite3_column_int(statement, 9);
        message.snr = (float)sqlite3_column_double(statement, 10);
        messages.push_back(message);
    }

    sqlite3_finalize(statement);

    // Queried newest first so that LIMIT keeps the newest; the chat window
    // wants them in the order they happened.
    std::reverse(messages.begin(), messages.end());

    return messages;
}

bool MessageStore::pruneMessagesOlderThan(std::time_t cutoff)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (db_ == nullptr)
    {
        lastError_ = "database is not open";
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM messages WHERE timestamp < ?;", -1, &statement,
                           nullptr) != SQLITE_OK)
    {
        setError("preparing message prune");
        return false;
    }

    sqlite3_bind_int64(statement, 1, (sqlite3_int64)cutoff);

    bool ok = sqlite3_step(statement) == SQLITE_DONE;
    if (!ok) setError("pruning messages");
    sqlite3_finalize(statement);

    return ok;
}

bool MessageStore::upsertHeardStation(const HeardStation& station)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (db_ == nullptr)
    {
        lastError_ = "database is not open";
        return false;
    }

    const char* sql =
        "INSERT INTO heard_stations (callsign, snr, last_heard) VALUES (?, ?, ?)"
        " ON CONFLICT(callsign) DO UPDATE SET snr = excluded.snr,"
        " last_heard = excluded.last_heard;";

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &statement, nullptr) != SQLITE_OK)
    {
        setError("preparing heard station upsert");
        return false;
    }

    sqlite3_bind_text(statement, 1, station.callsign.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(statement, 2, station.snr);
    sqlite3_bind_int64(statement, 3, (sqlite3_int64)station.lastHeard);

    bool ok = sqlite3_step(statement) == SQLITE_DONE;
    if (!ok) setError("upserting heard station");
    sqlite3_finalize(statement);

    return ok;
}

std::vector<HeardStation> MessageStore::heardStations()
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<HeardStation> stations;
    if (db_ == nullptr)
    {
        lastError_ = "database is not open";
        return stations;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db_,
                           "SELECT callsign, snr, last_heard FROM heard_stations"
                           " ORDER BY last_heard DESC;",
                           -1, &statement, nullptr) != SQLITE_OK)
    {
        setError("preparing heard station query");
        return stations;
    }

    while (sqlite3_step(statement) == SQLITE_ROW)
    {
        HeardStation station;
        station.callsign = columnText(statement, 0);
        station.snr = (float)sqlite3_column_double(statement, 1);
        station.lastHeard = (std::time_t)sqlite3_column_int64(statement, 2);
        stations.push_back(station);
    }

    sqlite3_finalize(statement);

    return stations;
}

bool MessageStore::pruneHeardStationsOlderThan(std::time_t cutoff)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (db_ == nullptr)
    {
        lastError_ = "database is not open";
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM heard_stations WHERE last_heard < ?;", -1,
                           &statement, nullptr) != SQLITE_OK)
    {
        setError("preparing heard station prune");
        return false;
    }

    sqlite3_bind_int64(statement, 1, (sqlite3_int64)cutoff);

    bool ok = sqlite3_step(statement) == SQLITE_DONE;
    if (!ok) setError("pruning heard stations");
    sqlite3_finalize(statement);

    return ok;
}

std::string MessageStore::lastError() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

} // namespace TextMessaging
