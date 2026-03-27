//=========================================================================
// Name:            flex_defines.h
// Purpose:         Configuration for the Flex waveform.
//
// Authors:         Mooneer Salem
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

#ifndef FLEX_DEFINES_H
#define FLEX_DEFINES_H

#define FLEX_TCP_PORT (4992)
#define FLEX_SAMPLE_RATE (24000)
#define MAX_VITA_PACKETS (200)
#define MAX_VITA_SAMPLES (180) /* 7.5ms/block @ 24000 Hz */
#define VITA_SAMPLES_TO_SEND MAX_VITA_SAMPLES
#define MIN_VITA_PACKETS_TO_SEND (1)
#define MAX_VITA_PACKETS_TO_SEND (10)
#define US_OF_AUDIO_PER_VITA_PACKET (5250)
#define VITA_IO_TIME_INTERVAL_US (US_OF_AUDIO_PER_VITA_PACKET * MIN_VITA_PACKETS_TO_SEND) /* Time interval between subsequent sends or receives */
#define MAX_JITTER_US (500) /* Corresponds to +/- the maximum amount VITA_IO_TIME_INTERVAL_US should vary by. */
#define US_TO_MS (1000)
#define FIFO_SIZE_SAMPLES (FIFO_SIZE * FLEX_SAMPLE_RATE / 1000)

#endif // FLEX_DEFINES_H
