//=========================================================================
// Name:            TextMessagingTxQueue.cpp
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

#include "TextMessagingTxQueue.h"

TextMessagingTxQueue::TextMessagingTxQueue()
    : fifo_(CAPACITY_SAMPLES)
    , transmitting_(false)
    , ownsTransmitter_(false)
{
    // empty
}

bool TextMessagingTxQueue::enqueue(const short* samples, int numSamples)
{
    if (samples == nullptr || numSamples <= 0) return false;
    if (fifo_.numFree() < numSamples) return false;

    return fifo_.write(const_cast<short*>(samples), numSamples) == 0;
}

int TextMessagingTxQueue::read(short* samples, int numSamples) FREEDV_NONBLOCKING
{
    int available = fifo_.numUsed();
    int toRead = numSamples < available ? numSamples : available;
    if (toRead <= 0) return 0;

    // GenericFIFO reads all or nothing, and we just checked that this much is
    // there; a concurrent writer can only add more.
    if (fifo_.read(samples, toRead) != 0) return 0;

    return toRead;
}

int TextMessagingTxQueue::numUsed() const FREEDV_NONBLOCKING
{
    return fifo_.numUsed();
}

bool TextMessagingTxQueue::isEmpty() const FREEDV_NONBLOCKING
{
    return fifo_.numUsed() == 0;
}

bool TextMessagingTxQueue::isTransmitting() const FREEDV_NONBLOCKING
{
    return transmitting_.load(std::memory_order_acquire);
}

void TextMessagingTxQueue::setTransmitting(bool transmitting) FREEDV_NONBLOCKING
{
    transmitting_.store(transmitting, std::memory_order_release);
}

bool TextMessagingTxQueue::ownsTransmitter() const FREEDV_NONBLOCKING
{
    return ownsTransmitter_.load(std::memory_order_acquire);
}

void TextMessagingTxQueue::setOwnsTransmitter(bool owns) FREEDV_NONBLOCKING
{
    ownsTransmitter_.store(owns, std::memory_order_release);
}

void TextMessagingTxQueue::clear()
{
    fifo_.reset();
}

TextMessagingTxQueue& textMessagingTxQueue()
{
    static TextMessagingTxQueue queue;
    return queue;
}
