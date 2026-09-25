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

#include <algorithm>
#include <chrono>
#include <cstdlib>

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

// How long after a burst's audio should have finished we are willing to wait
// for the transmit thread to confirm it. Covers the output FIFO's depth, which
// is where the samples sit once our own queue has drained.
constexpr uint64_t PLAYOUT_MARGIN_MS = 1000;

uint64_t monotonicMs()
{
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Set FREEDV_TEXT_CHAT_TX_LOG to have each burst report how long its audio
// was and how long the transmitter was actually held, with the milestones in
// between. A burst that keys for far longer than its audio is the thing to
// look for: the far end has to wait all of it out before it may answer.
bool txLogEnabled()
{
    static const bool enabled = std::getenv("FREEDV_TEXT_CHAT_TX_LOG") != nullptr;
    return enabled;
}

} // namespace

TextMessagingTransport::TextMessagingTransport(TextMessagingModem* modem)
    : modem_(modem)
    , keyed_(false)
    , keyedAtMs_(0)
    , keyDeadlineMs_(0)
    , burstMs_(0)
    , sawTransmitting_(false)
    , sawEmpty_(false)
    , loggedChannelBusy_(false)
    , channelBusySinceMs_(0)
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

void TextMessagingTransport::setTransmitAllowedCheck(VoiceTransmitCheck transmitAllowedCheck)
{
    std::lock_guard<std::mutex> lock(mutex_);
    transmitAllowedCheck_ = std::move(transmitAllowedCheck);
}

bool TextMessagingTransport::transmit(const std::vector<TextMessaging::OutgoingBurst>& bursts)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (modem_ == nullptr || !modem_->isOpen()) return false;
    if (pttFunction_ == nullptr) return false;
    if (keyed_.load(std::memory_order_acquire)) return false;

    // Voice always wins the transmitter.
    if (voiceTransmitCheck_ != nullptr && voiceTransmitCheck_()) return false;

    if (transmitAllowedCheck_ != nullptr && !transmitAllowedCheck_()) return false;

    if (!modem_->modulate(bursts, samples_)) return false;

    auto& queue = textMessagingTxQueue();
    if (!queue.enqueue(samples_.data(), (int)samples_.size()))
    {
        log_warn("Text messaging burst did not fit in the transmit queue");
        return false;
    }

    queue.setOwnsTransmitter(true);

    uint64_t burstMs = (uint64_t)samples_.size() * 1000 / MODEM_SAMPLE_RATE;
    keyedAtMs_ = monotonicMs();
    keyDeadlineMs_ = keyedAtMs_ + burstMs + KEY_TIMEOUT_MARGIN_MS;
    keyed_.store(true, std::memory_order_release);

    burstMs_ = burstMs;
    sawTransmitting_ = false;
    sawEmpty_ = false;

    if (txLogEnabled())
    {
        int signallingBursts = (int)std::count_if(
            bursts.begin(), bursts.end(),
            [](const TextMessaging::OutgoingBurst& burst)
            { return burst.mode == TextMessaging::BurstMode::Signalling; });
        log_info("TX: keying for %d signalling and %d text frame(s), %d samples, %llu ms of audio",
                 signallingBursts, (int)bursts.size() - signallingBursts,
                 (int)samples_.size(), (unsigned long long)burstMs);
    }

    pttFunction_(true);

    return true;
}

bool TextMessagingTransport::isTransmitting() const
{
    if (keyed_.load(std::memory_order_acquire)) return true;
    if (textMessagingTxQueue().isTransmitting()) return true;

    return pttHeld();
}

bool TextMessagingTransport::pttHeld() const
{
    VoiceTransmitCheck check;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        check = voiceTransmitCheck_;
    }

    return check != nullptr && check();
}

bool TextMessagingTransport::isChannelBusy() const
{
    return modem_ != nullptr && modem_->isReceiving();
}

void TextMessagingTransport::poll()
{
    // The protocol freezes on the channel being busy but has no logging of its
    // own, so the spells are reported here.
    if (txLogEnabled())
    {
        bool busy = isChannelBusy();
        if (busy != loggedChannelBusy_)
        {
            uint64_t busyNow = monotonicMs();
            if (busy)
            {
                channelBusySinceMs_ = busyNow;
                log_info("RX: channel busy, demodulator in sync");
            }
            else
            {
                log_info("RX: channel clear after %llu ms",
                         (unsigned long long)(busyNow - channelBusySinceMs_));
            }
            loggedChannelBusy_ = busy;
        }
    }

    if (!keyed_.load(std::memory_order_acquire)) return;

    auto& queue = textMessagingTxQueue();
    uint64_t now = monotonicMs();

    // The transmit thread sets the transmitting flag when it starts sending
    // and clears it once the sound card has played the last sample out.
    bool transmitting = queue.isTransmitting();
    bool empty = queue.isEmpty();

    bool startedNow = transmitting && !sawTransmitting_;
    bool emptyNow = empty && !sawEmpty_;
    if (startedNow) sawTransmitting_ = true;
    if (emptyNow) sawEmpty_ = true;

    if (txLogEnabled())
    {
        if (startedNow)
        {
            log_info("TX: +%llu ms transmit thread picked the burst up",
                     (unsigned long long)(now - keyedAtMs_));
        }
        if (emptyNow)
        {
            log_info("TX: +%llu ms queue drained", (unsigned long long)(now - keyedAtMs_));
        }
    }

    // The operator can unkey from the main window while a burst is playing.
    // With the transmitter gone the rest of the burst goes nowhere, and left
    // alone the burst would stand as "Transmitting" until the watchdog fired:
    // on the bench, twenty seconds after XMIT was pressed. The transmit thread
    // does not pick a burst up until the radio is keyed, so having seen it
    // start and now finding the radio unkeyed means somebody else unkeyed it.
    if (sawTransmitting_ && !pttHeld())
    {
        log_warn("Text messaging burst was unkeyed from the main window %llu ms in; dropping it",
                 (unsigned long long)(now - keyedAtMs_));
        queue.clear();
        queue.setTransmitting(false);
        unkey();
        return;
    }

    // The transmit thread clearing the transmitting flag means the sound card
    // has played the burst out, which is the accurate signal and the normal
    // path. But it has been seen to miss the flag in both directions: never
    // setting it, in which case an empty queue is no proof the audio has been
    // played and unkeying here clips the end of the burst off the air; and
    // never clearing it, in which case we sat on the transmitter until the
    // watchdog fired ten seconds later and the far end could not answer.
    // Neither is trusted on its own.
    bool playedOut = now - keyedAtMs_ >= burstMs_ + PLAYOUT_MARGIN_MS;
    bool burstFinished = empty && ((sawTransmitting_ && !transmitting) || playedOut);

    if (burstFinished)
    {
        // The transmit thread confirms a burst by clearing this flag. Having
        // concluded without it, it is not going to, and the flag is what
        // isTransmitting() reports to the protocol: leaving it set strands the
        // station silent for ever, unable to acknowledge anything.
        if (transmitting)
        {
            log_warn("Text messaging burst was never confirmed by the transmit thread "
                     "(%llu ms of audio, held %llu ms); releasing the transmitter",
                     (unsigned long long)burstMs_,
                     (unsigned long long)(now - keyedAtMs_));
            queue.clear();
            queue.setTransmitting(false);
        }

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

    if (txLogEnabled())
    {
        uint64_t held = monotonicMs() - keyedAtMs_;
        log_info("TX: +%llu ms unkey requested (audio was %llu ms, %lld ms of it silence)",
                 (unsigned long long)held, (unsigned long long)burstMs_,
                 (long long)held - (long long)burstMs_);
    }

    // The receivers have been frozen since we keyed; whatever they were
    // doing then is no guide to what is on the channel now.
    if (modem_ != nullptr) modem_->resetReceivers();

    // Ownership of the transmitter is released by whoever performs the PTT
    // change, once the changeover has actually finished: the transmit thread
    // must keep microphone audio off the air until then.
    if (pttFunction != nullptr) pttFunction(false);
}
