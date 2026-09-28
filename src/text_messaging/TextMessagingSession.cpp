//=========================================================================
// Name:            TextMessagingSession.cpp
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

#include "TextMessagingSession.h"

#include <chrono>

namespace TextMessaging
{

namespace
{

// Fast enough that an acknowledgement is never held up noticeably, slow
// enough to be invisible in a profile.
constexpr int TICK_INTERVAL_MS = 100;

} // namespace

TextMessagingSession::TextMessagingSession()
    : protocol_(store_, stations_)
    , transport_(nullptr)
    , running_(false)
{
    // empty
}

TextMessagingSession::~TextMessagingSession()
{
    stop();
}

TextMessagingSession& TextMessagingSession::instance()
{
    static TextMessagingSession session;
    return session;
}

bool TextMessagingSession::isStarted() const
{
    return running_.load(std::memory_order_acquire);
}

bool TextMessagingSession::start(const std::string& databasePath,
                                 ITextMessagingTransport* transport)
{
    if (running_.load(std::memory_order_acquire)) return true;

    if (!store_.open(databasePath))
    {
        lastError_ = store_.lastError();
        return false;
    }

    std::time_t now = std::time(nullptr);
    store_.pruneMessagesOlderThan(now - HISTORY_RETENTION_SECONDS);
    store_.pruneHeardStationsOlderThan(now - HeardStationList::DEFAULT_MAX_AGE_SECONDS);
    stations_.restore(store_.heardStations(), now);

    transport_ = transport;
    protocol_.setTransport(transport);

    running_.store(true, std::memory_order_release);
    tickThread_ = std::thread([this]() { tickLoop(); });

    return true;
}

void TextMessagingSession::stop()
{
    if (!running_.exchange(false, std::memory_order_acq_rel)) return;

    if (tickThread_.joinable()) tickThread_.join();

    protocol_.setTransport(nullptr);
    protocol_.setObserver(nullptr);
    transport_ = nullptr;
    store_.close();
}

void TextMessagingSession::tickLoop()
{
    while (running_.load(std::memory_order_acquire))
    {
        if (transport_ != nullptr) transport_->poll();
        protocol_.tick();

        std::this_thread::sleep_for(std::chrono::milliseconds(TICK_INTERVAL_MS));
    }
}

} // namespace TextMessaging
