//=========================================================================
// Name:            TextMessagingSession.h
// Purpose:         Keeps chat running whether or not its window is open.
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

#ifndef TEXT_MESSAGING__TEXT_MESSAGING_SESSION_H
#define TEXT_MESSAGING__TEXT_MESSAGING_SESSION_H

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "HeardStationList.h"
#include "MessageStore.h"
#include "TextMessagingProtocol.h"

namespace TextMessaging
{

// Owns the chat state for the life of the application. The dialog attaches to
// it as an observer when it opens and detaches when it closes, so retries and
// incoming messages keep working with the window shut.
class TextMessagingSession
{
public:
    // How long chat history is kept; anything older is dropped at startup.
    static constexpr std::time_t HISTORY_RETENTION_SECONDS = 30 * 24 * 60 * 60;

    // How many messages the dialog loads when it opens.
    static constexpr int MESSAGES_TO_RESTORE = 200;

    static TextMessagingSession& instance();

    // Opens the database, restores heard stations and starts the tick thread.
    // Safe to call again; only the first call does anything.
    bool start(const std::string& databasePath, ITextMessagingTransport* transport);
    void stop();
    bool isStarted() const;

    TextMessagingProtocol& protocol() { return protocol_; }
    MessageStore& store() { return store_; }
    HeardStationList& stations() { return stations_; }

    std::string lastError() const { return lastError_; }

private:
    TextMessagingSession();
    ~TextMessagingSession();

    TextMessagingSession(const TextMessagingSession&) = delete;
    TextMessagingSession& operator=(const TextMessagingSession&) = delete;

    void tickLoop();

    MessageStore store_;
    HeardStationList stations_;
    TextMessagingProtocol protocol_;
    ITextMessagingTransport* transport_;

    std::atomic<bool> running_;
    std::thread tickThread_;
    std::string lastError_;
};

} // namespace TextMessaging

#endif // TEXT_MESSAGING__TEXT_MESSAGING_SESSION_H
