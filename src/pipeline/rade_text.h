//==========================================================================
// Name:            rade_text.h
//
// Purpose:         Handles reliable text (e.g. text with FEC).
// Created:         December 7, 2024
// Authors:         Mooneer Salem
//
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================

#ifndef RADE_TEXT_H
#define RADE_TEXT_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/* Hide internals of rade_text_t. */
typedef void* rade_text_t;

/* Function type for callback (when full reliable text has been received). */
typedef void (*on_text_rx_t)(rade_text_t rt, const char* txt_ptr,
                             int length, void* state);

/* Allocate rade_text object. */
rade_text_t rade_text_create();

/* Destroy rade_text object. */
void rade_text_destroy(rade_text_t ptr);

/* Generates float array for use with RADE EOO functions. */
void rade_text_generate_tx_string(
    rade_text_t ptr, const char* str, int strlength,
    float* syms);

/* Set text RX callback. */
void rade_text_set_rx_callback(rade_text_t ptr, on_text_rx_t text_rx_fn, void* state);

/* Decode received symbols from RADE decoder. */
void rade_text_rx(rade_text_t ptr, float* syms);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // RADE_TEXT_H