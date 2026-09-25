//=========================================================================
// Name:            TextMessagingModem.h
// Purpose:         Carries text messaging frames over codec2 data modes.
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

#ifndef AUDIO_PIPELINE__TEXT_MESSAGING_MODEM_H
#define AUDIO_PIPELINE__TEXT_MESSAGING_MODEM_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

#include "FrameCodec.h"

// Forward declaration of the struct implemented by Codec2.
extern "C"
{
    struct freedv;
}

// Text messaging rides on the codec2 raw data modes rather than on RADE:
// DATAC13 for the short signalling frames and DATAC4 for message text, both
// the most robust choice in their size class. Each frame goes out as its own
// burst (preamble, frame, postamble) inside a single keying of the
// transmitter, so the receiving modem can acquire every frame independently
// instead of having to hold sync across the whole message.
//
// Threading: open() and close() must run on the same thread (the GUI thread),
// because codec2's realtime allocator is thread local and memory taken from
// one thread's pool cannot be returned on another. modulate() is called from
// the GUI thread and demodulate() only from the audio tap thread; the two use
// separate modem instances and so never contend.
class TextMessagingModem
{
public:
    using FrameCallback = std::function<void(const TextMessaging::Frame& frame, float snr)>;

    TextMessagingModem();
    ~TextMessagingModem();

    TextMessagingModem(const TextMessagingModem&) = delete;
    TextMessagingModem& operator=(const TextMessagingModem&) = delete;

    bool open();
    void close();
    bool isOpen() const;

    // Called before audio starts. The callback runs on the tap thread.
    void setFrameCallback(FrameCallback callback);

    // Turns a keying's bursts into 8 kHz samples ready for the transmitter,
    // each burst in its own mode and complete with its own preamble, so a
    // keying may mix the two. Returns false if the modem is not open or a
    // frame is the wrong size for the mode it is to be sent in.
    bool modulate(const std::vector<TextMessaging::OutgoingBurst>& bursts,
                  std::vector<short>& samplesOut);

    // Feeds received 8 kHz audio to both demodulators, invoking the frame
    // callback for every frame that passes the modem's CRC.
    void demodulate(const short* samples, int numSamples);

    // True while either demodulator is locked onto a burst, or was within
    // CHANNEL_BUSY_HOLD_MILLISECONDS: somebody else has the channel. Safe to
    // call from any thread.
    bool isReceiving() const;

    // Puts both demodulators back to searching for a preamble and forgets
    // any sync they reported. Called at the end of our own transmission: the
    // receive path is not run while we are keyed, so a demodulator that was
    // locked onto a burst when we keyed is still "locked" when we unkey,
    // reports the channel busy on silence, and, expecting payload rather
    // than a preamble, misses the first burst that actually arrives. The
    // bench lost a frame that way. Safe to call from any thread.
    void resetReceivers();

private:
    struct Demodulator
    {
        struct freedv* modem = nullptr;
        const char* name = "";          // the mode, for FREEDV_TEXT_CHAT_RX_LOG
        std::vector<short> buffer;      // samples not yet consumed by the modem
        std::vector<uint8_t> bytes;     // one modem frame of decoded payload
        int payloadBytes = 0;
    };

    void closeLocked();
    void demodulateOne(Demodulator& demodulator, const short* samples, int numSamples);
    bool modulateFrame(struct freedv* modem, const std::vector<uint8_t>& frame,
                       std::vector<short>& samplesOut);

    Demodulator signallingRx_;
    Demodulator textRx_;
    struct freedv* signallingTx_;
    struct freedv* textTx_;
    std::atomic<bool> open_;

    // When a demodulator last reported sync, on the steady clock; zero for
    // never. Written by the tap thread, read by the protocol's.
    std::atomic<uint64_t> lastSyncMs_;

    // Held by demodulate() and by open()/close(), so the receive tap can never
    // be inside the modem while it is being torn down at shutdown.
    std::mutex rxMutex_;
    std::mutex callbackMutex_;
    FrameCallback frameCallback_;
};

// The application wide modem. Opened and closed by MainFrame on the GUI
// thread; the receive step and the text messaging session both talk to it.
TextMessagingModem& textMessagingModem();

#endif // AUDIO_PIPELINE__TEXT_MESSAGING_MODEM_H
