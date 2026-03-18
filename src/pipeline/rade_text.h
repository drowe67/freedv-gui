//==========================================================================
// Name:            rade_text.h
//
// Purpose:         Handles reliable text (e.g. text with FEC).
// Created:         December 7, 2024
// Authors:         Mooneer Salem
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
//==========================================================================

#ifndef RADE_TEXT_H
#define RADE_TEXT_H

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

    /* Hide internals of rade_text_t. */
    typedef void *rade_text_t;

    /* Function type for callback (when full reliable text has been received). */
    typedef void (*on_text_rx_t)(rade_text_t rt, const char *txt_ptr, int length, void *state);

    /* Allocate rade_text object. */
    rade_text_t rade_text_create();

    /* Destroy rade_text object. */
    void rade_text_destroy(rade_text_t ptr);

    /* Generates float array for use with RADE EOO functions. */
    void rade_text_generate_tx_string(rade_text_t ptr, const char *str, int strlength, float *syms, int symSize);

    /* Set text RX callback. */
    void rade_text_set_rx_callback(rade_text_t ptr, on_text_rx_t text_rx_fn, void *state);

    /* Decode received symbols from RADE decoder. */
    void rade_text_rx(rade_text_t ptr, float *syms, int symSize);

    /* Whether to enable output of stats (i.e. BER). */
    void rade_text_enable_stats_output(rade_text_t ptr, int enable);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // RADE_TEXT_H