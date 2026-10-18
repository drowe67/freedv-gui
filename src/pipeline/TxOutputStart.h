//=========================================================================
// Name:            TxOutputStart.h
// Purpose:         Decides when the radio's sound card starts being given
//                  a transmission, and whether there's a block ready for it.
//
// Authors:         Mooneer Salem
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
//  for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses>.
//
//=========================================================================

#ifndef TX_OUTPUT_START_H
#define TX_OUTPUT_START_H

#include <algorithm>
#include <atomic>

#include "freedv_sanitizers.h"

// How much of a transmission is made ready before the radio's sound card is
// given any of it.
//
// The sound card takes what it's given in real time from the moment it's
// given anything, in blocks of its own size, and gets silence (a hole in
// the signal) if it asks for more than has been made. What's made comes a
// modem frame at a time (120 ms for RADE) and only as fast as the microphone
// delivers, so without something in hand that happens straight away. This
// is two RADE modem frames: one to cover how it's made, and one to spare.
constexpr int TX_OUTPUT_START_MS = 240;

// How full the FIFO has to be for the radio's sound card to be given a whole
// block every time it asks: TX_OUTPUT_START_MS worth of samples, or half the
// FIFO if that's more than it can hold.
//
// The thread making the transmission holds the FIFO at this level while the
// microphone is falling behind (see txProcessing_() in TxRxThread.cpp), which
// is how a transmission that has started is kept going: a sound card given
// nothing because nothing was ready is a carrier that's gone down, and that
// costs the other station the signal altogether rather than a word or two.
inline int txOutputStartLevel(int capacity, int sampleRate) FREEDV_NONBLOCKING
{
    return std::min((int)((long)TX_OUTPUT_START_MS * sampleRate / 1000), capacity / 2);
}

// The most the radio's sound card can be given in one go. A card that asks
// for more than the transmitter can be relied on to have ready is given that
// many and silence for the rest of its block: it can never be given more --
// the FIFO is filled a modem frame at a time and never right to the top --
// and being given nothing every time it asks is a transmission that never
// starts.
inline int txOutputWanted(int capacity, int sampleRate, int asked) FREEDV_NONBLOCKING
{
    return std::min(asked, txOutputStartLevel(capacity, sampleRate));
}

enum class TxOutput
{
    Waiting,    // the transmission hasn't started going out yet: silence
    Short,      // it has, and there's less than a block ready: silence
    Ready,      // there's a block to send
};

// What the radio's sound card gets when it asks for `wanted` samples with
// `used` of them ready, in a FIFO that holds `capacity`.
//
// `started` is cleared by whoever empties the FIFO at the start of a
// transmission, and set here once enough is ready or the transmission is
// already ending (when what there is, is all there's going to be).
inline TxOutput txOutputFor(int used, int capacity, int wanted, int sampleRate, bool ending, std::atomic<bool>& started) FREEDV_NONBLOCKING
{
    if (!started.load(std::memory_order_acquire))
    {
        int threshold = txOutputStartLevel(capacity, sampleRate);
        if (used < threshold && !ending)
        {
            return TxOutput::Waiting;
        }
        started.store(true, std::memory_order_release);
    }

    return (used >= wanted) ? TxOutput::Ready : TxOutput::Short;
}

#endif // TX_OUTPUT_START_H
