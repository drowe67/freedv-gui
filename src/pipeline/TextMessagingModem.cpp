//=========================================================================
// Name:            TextMessagingModem.cpp
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

#include "TextMessagingModem.h"

#include <algorithm>
#include <cassert>
#include <cstring>

#include "freedv_api.h"
#include "util/logging/ulog.h"

using namespace TextMessaging;

namespace
{

// Gap left between the bursts that make up one message, so the receiving
// modem has time to drop sync and look for the next preamble.
constexpr int INTER_BURST_GAP_MS = 100;
constexpr int MODEM_SAMPLE_RATE = 8000;

// Enough audio to hold several bursts if the tap thread falls behind. Beyond
// this the oldest samples are dropped: they are older than any burst we could
// still decode.
constexpr int MAX_BUFFERED_SAMPLES = MODEM_SAMPLE_RATE * 4;

} // namespace

TextMessagingModem::TextMessagingModem()
    : signallingTx_(nullptr)
    , textTx_(nullptr)
    , open_(false)
{
    // empty
}

TextMessagingModem::~TextMessagingModem()
{
    close();
}

bool TextMessagingModem::open()
{
    if (open_) return true;

    signallingRx_.modem = freedv_open(FREEDV_MODE_DATAC13);
    textRx_.modem = freedv_open(FREEDV_MODE_DATAC4);
    signallingTx_ = freedv_open(FREEDV_MODE_DATAC13);
    textTx_ = freedv_open(FREEDV_MODE_DATAC4);

    if (signallingRx_.modem == nullptr || textRx_.modem == nullptr ||
        signallingTx_ == nullptr || textTx_ == nullptr)
    {
        log_warn("Could not open the text messaging data modems");
        close();
        return false;
    }

    for (Demodulator* demodulator : {&signallingRx_, &textRx_})
    {
        // One frame per burst: each frame is preceded by its own preamble, so
        // the modem should look for a new burst after every one.
        freedv_set_frames_per_burst(demodulator->modem, 1);
        freedv_set_verbose(demodulator->modem, 0);

        assert(freedv_get_modem_sample_rate(demodulator->modem) == MODEM_SAMPLE_RATE);

        int bytesPerModemFrame = freedv_get_bits_per_modem_frame(demodulator->modem) / 8;
        demodulator->payloadBytes = bytesPerModemFrame - 2; // the modem's own CRC
        demodulator->bytes.resize(bytesPerModemFrame);
        demodulator->buffer.clear();
        demodulator->buffer.reserve(freedv_get_n_max_modem_samples(demodulator->modem) * 2);
    }

    // The payload sizes the frame codec was written against have to match what
    // this build of codec2 actually gives us, or frames would silently be
    // truncated on the air.
    if (signallingRx_.payloadBytes != SIGNALLING_FRAME_BYTES ||
        textRx_.payloadBytes != TEXT_FRAME_BYTES)
    {
        log_warn("Unexpected codec2 data mode payload sizes (%d/%d, expected %d/%d)",
                 signallingRx_.payloadBytes, textRx_.payloadBytes, SIGNALLING_FRAME_BYTES,
                 TEXT_FRAME_BYTES);
        close();
        return false;
    }

    freedv_set_verbose(signallingTx_, 0);
    freedv_set_verbose(textTx_, 0);

    open_ = true;
    return true;
}

void TextMessagingModem::close()
{
    open_ = false;

    for (struct freedv** modem : {&signallingRx_.modem, &textRx_.modem, &signallingTx_, &textTx_})
    {
        if (*modem != nullptr)
        {
            freedv_close(*modem);
            *modem = nullptr;
        }
    }

    signallingRx_.buffer.clear();
    textRx_.buffer.clear();
}

bool TextMessagingModem::isOpen() const
{
    return open_;
}

void TextMessagingModem::setFrameCallback(FrameCallback callback)
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    frameCallback_ = std::move(callback);
}

bool TextMessagingModem::modulateFrame(struct freedv* modem, const std::vector<uint8_t>& frame,
                                       std::vector<short>& samplesOut)
{
    int bytesPerModemFrame = freedv_get_bits_per_modem_frame(modem) / 8;
    int payloadBytes = bytesPerModemFrame - 2;
    if ((int)frame.size() != payloadBytes) return false;

    std::vector<uint8_t> bytes(bytesPerModemFrame, 0);
    std::memcpy(bytes.data(), frame.data(), frame.size());

    // The raw data modes require the CRC in the last two bytes.
    uint16_t crc = freedv_gen_crc16(bytes.data(), payloadBytes);
    bytes[bytesPerModemFrame - 2] = (uint8_t)(crc >> 8);
    bytes[bytesPerModemFrame - 1] = (uint8_t)(crc & 0xFF);

    int preambleSamples = freedv_get_n_tx_preamble_modem_samples(modem);
    int frameSamples = freedv_get_n_tx_modem_samples(modem);
    int postambleSamples = freedv_get_n_tx_postamble_modem_samples(modem);
    int gapSamples = MODEM_SAMPLE_RATE * INTER_BURST_GAP_MS / 1000;

    // The preamble and postamble calls report how many samples they actually
    // produced, which can be fewer than the maximum the getters report, so
    // each section is trimmed to its real length before the next one starts.
    size_t offset = samplesOut.size();
    samplesOut.resize(offset + preambleSamples);
    samplesOut.resize(offset + freedv_rawdatapreambletx(modem, &samplesOut[offset]));

    offset = samplesOut.size();
    samplesOut.resize(offset + frameSamples);
    freedv_rawdatatx(modem, &samplesOut[offset], bytes.data());

    offset = samplesOut.size();
    samplesOut.resize(offset + postambleSamples);
    samplesOut.resize(offset + freedv_rawdatapostambletx(modem, &samplesOut[offset]));

    // Silence between bursts, so the far end can drop sync and start looking
    // for the next preamble.
    samplesOut.resize(samplesOut.size() + gapSamples, 0);

    return true;
}

bool TextMessagingModem::modulate(const std::vector<std::vector<uint8_t>>& frames, bool signalling,
                                  std::vector<short>& samplesOut)
{
    if (!open_) return false;

    struct freedv* modem = signalling ? signallingTx_ : textTx_;
    samplesOut.clear();

    for (const std::vector<uint8_t>& frame : frames)
    {
        if (!modulateFrame(modem, frame, samplesOut))
        {
            samplesOut.clear();
            return false;
        }
    }

    return !samplesOut.empty();
}

void TextMessagingModem::demodulateOne(Demodulator& demodulator, const short* samples,
                                       int numSamples)
{
    std::vector<short>& buffer = demodulator.buffer;
    buffer.insert(buffer.end(), samples, samples + numSamples);

    if ((int)buffer.size() > MAX_BUFFERED_SAMPLES)
    {
        buffer.erase(buffer.begin(), buffer.begin() + (buffer.size() - MAX_BUFFERED_SAMPLES));
    }

    int nin = freedv_nin(demodulator.modem);
    while ((int)buffer.size() >= nin)
    {
        int bytesOut = freedv_rawdatarx(demodulator.modem, demodulator.bytes.data(), buffer.data());
        buffer.erase(buffer.begin(), buffer.begin() + nin);
        nin = freedv_nin(demodulator.modem);

        if (bytesOut <= 0) continue;

        Frame frame;
        if (!FrameCodec::decode(demodulator.bytes.data(), bytesOut, frame)) continue;

        int sync = 0;
        float snr = 0.0f;
        freedv_get_modem_stats(demodulator.modem, &sync, &snr);

        FrameCallback callback;
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            callback = frameCallback_;
        }

        if (callback) callback(frame, snr);
    }
}

void TextMessagingModem::demodulate(const short* samples, int numSamples)
{
    if (!open_ || samples == nullptr || numSamples <= 0) return;

    demodulateOne(signallingRx_, samples, numSamples);
    demodulateOne(textRx_, samples, numSamples);
}

TextMessagingModem& textMessagingModem()
{
    static TextMessagingModem modem;
    return modem;
}
