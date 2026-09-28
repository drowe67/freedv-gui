//=========================================================================
// Name:            TextMessagingTxQueue.h
// Purpose:         Holds modulated text messaging audio awaiting transmission.
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

#ifndef AUDIO_PIPELINE__TEXT_MESSAGING_TX_QUEUE_H
#define AUDIO_PIPELINE__TEXT_MESSAGING_TX_QUEUE_H

#include <atomic>

#include "freedv_sanitizers.h"
#include "util/GenericFIFO.h"

// Modulated 8 kHz audio waiting for the transmit thread to send it. The
// session fills it before keying the transmitter and watches it empty to know
// when the burst is over; the transmit thread drains it in place of
// microphone audio.
class TextMessagingTxQueue
{
public:
    // Room for a minute of audio, well past the longest message the protocol
    // will send in one keying.
    static constexpr int CAPACITY_SAMPLES = 8000 * 60;

    TextMessagingTxQueue();

    // Returns false if the burst would not fit, in which case nothing is
    // queued: a partial burst on the air is worse than none.
    bool enqueue(const short* samples, int numSamples);

    // Returns the number of samples actually read.
    int read(short* samples, int numSamples) FREEDV_NONBLOCKING;

    int numUsed() const FREEDV_NONBLOCKING;

    // True from the moment the transmit thread starts sending a burst until
    // the last sample of it has left the output FIFO. The session watches
    // this to know when it can unkey the transmitter.
    bool isTransmitting() const FREEDV_NONBLOCKING;
    void setTransmitting(bool transmitting) FREEDV_NONBLOCKING;

    // True from the moment the session keys the radio for a burst until the
    // transmitter has been released again. While it is set the transmit
    // thread must not modulate microphone audio, or the tail of the keying
    // would go out as voice.
    bool ownsTransmitter() const FREEDV_NONBLOCKING;
    void setOwnsTransmitter(bool owns) FREEDV_NONBLOCKING;
    bool isEmpty() const FREEDV_NONBLOCKING;
    void clear();

private:
    GenericFIFO<short> fifo_;
    std::atomic<bool> transmitting_;
    std::atomic<bool> ownsTransmitter_;
};

TextMessagingTxQueue& textMessagingTxQueue();

#endif // AUDIO_PIPELINE__TEXT_MESSAGING_TX_QUEUE_H
