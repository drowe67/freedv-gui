//=========================================================================
// Name:            TextMessagingTransport.cpp
// Purpose:         Puts text messaging bursts on the air.
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

#include "TextMessagingTransport.h"

#include <chrono>

#include "TextMessagingModem.h"
#include "TextMessagingTxQueue.h"
#include "util/logging/ulog.h"

namespace
{

constexpr int MODEM_SAMPLE_RATE = 8000;

// If the transmit thread never drains the queue -- no transmit sound device,
// or audio stopped mid burst -- the radio must not stay keyed. The burst's own
// length plus this margin is the longest we will hold PTT.
constexpr uint64_t KEY_TIMEOUT_MARGIN_MS = 10000;

uint64_t monotonicMs()
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

} // namespace

TextMessagingTransport::TextMessagingTransport(TextMessagingModem* modem)
    : modem_(modem)
    , keyed_(false)
    , keyedAtMs_(0)
    , keyDeadlineMs_(0)
{
    // empty
}

void TextMessagingTransport::setPttFunction(PttFunction pttFunction)
{
    std::lock_guard<std::mutex> lock(mutex_);
    pttFunction_ = std::move(pttFunction);
}

void TextMessagingTransport::setVoiceTransmitCheck(VoiceTransmitCheck voiceTransmitCheck)
{
    std::lock_guard<std::mutex> lock(mutex_);
    voiceTransmitCheck_ = std::move(voiceTransmitCheck);
}

bool TextMessagingTransport::transmit(const std::vector<std::vector<uint8_t>>& frames,
                                      bool signalling)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (modem_ == nullptr || !modem_->isOpen()) return false;
    if (pttFunction_ == nullptr) return false;
    if (keyed_.load(std::memory_order_acquire)) return false;

    // Voice always wins the transmitter.
    if (voiceTransmitCheck_ != nullptr && voiceTransmitCheck_()) return false;

    if (!modem_->modulate(frames, signalling, samples_)) return false;

    auto& queue = textMessagingTxQueue();
    if (!queue.enqueue(samples_.data(), (int)samples_.size()))
    {
        log_warn("Text messaging burst did not fit in the transmit queue");
        return false;
    }

    uint64_t burstMs = (uint64_t)samples_.size() * 1000 / MODEM_SAMPLE_RATE;
    keyedAtMs_ = monotonicMs();
    keyDeadlineMs_ = keyedAtMs_ + burstMs + KEY_TIMEOUT_MARGIN_MS;
    keyed_.store(true, std::memory_order_release);

    pttFunction_(true);

    return true;
}

bool TextMessagingTransport::isTransmitting() const
{
    if (keyed_.load(std::memory_order_acquire)) return true;
    if (textMessagingTxQueue().isTransmitting()) return true;

    VoiceTransmitCheck check;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        check = voiceTransmitCheck_;
    }

    return check != nullptr && check();
}

void TextMessagingTransport::poll()
{
    if (!keyed_.load(std::memory_order_acquire)) return;

    auto& queue = textMessagingTxQueue();
    uint64_t now = monotonicMs();

    // The transmit thread sets the transmitting flag when it starts sending
    // and clears it once the sound card has played the last sample out.
    bool burstFinished = queue.isEmpty() && !queue.isTransmitting();

    // Give the transmit thread a moment to pick the burst up before deciding
    // that an empty queue means the burst is over.
    bool startedYet = queue.isTransmitting() || now - keyedAtMs_ > 500;

    if (burstFinished && startedYet)
    {
        unkey();
        return;
    }

    if (now > keyDeadlineMs_)
    {
        log_warn("Text messaging burst did not finish in time; unkeying");
        queue.clear();
        queue.setTransmitting(false);
        unkey();
    }
}

void TextMessagingTransport::abort()
{
    auto& queue = textMessagingTxQueue();
    queue.clear();
    queue.setTransmitting(false);

    if (keyed_.load(std::memory_order_acquire)) unkey();
}

void TextMessagingTransport::unkey()
{
    PttFunction pttFunction;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pttFunction = pttFunction_;
    }

    keyed_.store(false, std::memory_order_release);
    if (pttFunction != nullptr) pttFunction(false);
}
