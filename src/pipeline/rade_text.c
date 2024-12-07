//==========================================================================
// Name:            rade_text.c
//
// Purpose:         Handles reliable text (e.g. text with FEC).
// Created:         August 15, 2021
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

#include "rade_text.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gp_interleaver.h"
#include "ldpc_codes.h"
#include "ofdm_internal.h"
#include "util/logging/ulog.h"

#define LDPC_TOTAL_SIZE_BITS (112)

#define RADE_TEXT_MAX_LENGTH (8)
#define RADE_TEXT_CRC_LENGTH (1)
#define RADE_TEXT_MAX_RAW_LENGTH \
  (RADE_TEXT_MAX_LENGTH + RADE_TEXT_CRC_LENGTH)

/* Two bytes of text/CRC equal four bytes of LDPC(112,56). */
#define RADE_TEXT_BYTES_PER_ENCODED_SEGMENT (8)

/* Internal definition of rade_text_t. */
typedef struct {
  on_text_rx_t text_rx_callback;
  void* callback_state;

  char tx_text[LDPC_TOTAL_SIZE_BITS];
  int tx_text_index;
  int tx_text_length;

  _Complex float inbound_pending_syms[LDPC_TOTAL_SIZE_BITS / 2];
  float inbound_pending_amps[LDPC_TOTAL_SIZE_BITS / 2];

  struct LDPC ldpc;
} rade_text_impl_t;

// 6 bit character set for text field use:
// 0: ASCII null
// 1-9: ASCII 38-47
// 10-19: ASCII '0'-'9'
// 20-46: ASCII 'A'-'Z'
// 47: ASCII ' '
static void convert_callsign_to_ota_string_(const char* input, char* output,
                                            int maxLength) {
  assert(input != NULL);
  assert(output != NULL);
  assert(maxLength >= 0);

  int outidx = 0;
  for (size_t index = 0; index < maxLength; index++) {
    if (input[index] == 0) break;

    if (input[index] >= 38 && input[index] <= 47) {
      output[outidx++] = input[index] - 37;
    } else if (input[index] >= '0' && input[index] <= '9') {
      output[outidx++] = input[index] - '0' + 10;
    } else if (input[index] >= 'A' && input[index] <= 'Z') {
      output[outidx++] = input[index] - 'A' + 20;
    } else if (input[index] >= 'a' && input[index] <= 'z') {
      output[outidx++] = toupper(input[index]) - 'A' + 20;
    }
  }
  output[outidx] = 0;
}

static void convert_ota_string_to_callsign_(const char* input, char* output,
                                            int maxLength) {
  assert(input != NULL);
  assert(output != NULL);
  assert(maxLength >= 0);

  int outidx = 0;
  for (size_t index = 0; index < maxLength; index++) {
    if (input[index] == 0) break;

    if (input[index] >= 1 && input[index] <= 9) {
      output[outidx++] = input[index] + 37;
    } else if (input[index] >= 10 && input[index] <= 19) {
      output[outidx++] = input[index] - 10 + '0';
    } else if (input[index] >= 20 && input[index] <= 46) {
      output[outidx++] = input[index] - 20 + 'A';
    }
  }
  output[outidx] = 0;
}

static char calculateCRC8_(char* input, int length) {
  assert(input != NULL);
  assert(length >= 0);

  unsigned char generator = 0x1D;
  unsigned char crc = 0x00; /* start with 0 so first byte can be 'xored' in */

  while (length > 0) {
    unsigned char ch = *input++;
    length--;

    // Break out if we see a null.
    if (ch == 0) break;

    crc ^= ch; /* XOR-in the next input byte */

    for (int i = 0; i < 8; i++) {
      if ((crc & 0x80) != 0) {
        crc = (unsigned char)((crc << 1) ^ generator);
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}

static int rade_text_ldpc_decode(rade_text_impl_t* obj, char* dest) {
  assert(obj != NULL);
  assert(dest != NULL);

  _Complex float deinterleavedSyms[LDPC_TOTAL_SIZE_BITS / 2];
  float deinterleavedAmps[LDPC_TOTAL_SIZE_BITS / 2];
  float llr[LDPC_TOTAL_SIZE_BITS];
  unsigned char output[LDPC_TOTAL_SIZE_BITS];
  int parityCheckCount = 0;

  // Use soft decision for the LDPC decoder.
  int Npayloadsymsperpacket = LDPC_TOTAL_SIZE_BITS / 2;

  // Deinterleave symbols
  /*gp_deinterleave_comp(
      (COMP*)deinterleavedSyms,
      (COMP*)&obj->inbound_pending_syms[0],
      Npayloadsymsperpacket);
  gp_deinterleave_float(
      deinterleavedAmps,
      &obj->inbound_pending_amps[0],
      Npayloadsymsperpacket);*/
  memcpy(deinterleavedSyms, obj->inbound_pending_syms, Npayloadsymsperpacket * sizeof(COMP));
  memcpy(deinterleavedAmps, obj->inbound_pending_amps, Npayloadsymsperpacket * sizeof(float));

  // Generate amplitudes based on deinterleaved symbols
  float meanAmplitude = 0;
  for (int index = 0; index < Npayloadsymsperpacket; index++)
  {
    //float* sym = (float*)&deinterleavedSyms[index];
    //deinterleavedAmps[index] = sqrt(pow(sym[0], 2) + pow(sym[1], 2));
    meanAmplitude += deinterleavedAmps[index];
  }

  float EsNo = 3.0;  // note: constant from freedv_700.c
  meanAmplitude /= Npayloadsymsperpacket;

  log_info("mean amplitude: %f", meanAmplitude);

  symbols_to_llrs(llr, (COMP*)deinterleavedSyms, deinterleavedAmps, EsNo,
                  meanAmplitude, Npayloadsymsperpacket);
  run_ldpc_decoder(&obj->ldpc, output, llr, &parityCheckCount);

  // Data is valid if BER < 0.2
  float ber_est = (float)(obj->ldpc.NumberParityBits - parityCheckCount) /
                  obj->ldpc.NumberParityBits;
  int result = (ber_est < 0.2);

  log_info("BER est: %f", ber_est);
  if (result) {
    memset(dest, 0, RADE_TEXT_BYTES_PER_ENCODED_SEGMENT);

    for (int bitIndex = 0; bitIndex < 8; bitIndex++) {
      if (output[bitIndex]) dest[0] |= 1 << bitIndex;
    }
    for (int bitIndex = 8; bitIndex < (LDPC_TOTAL_SIZE_BITS / 2); bitIndex++) {
      int bitsSinceCrc = bitIndex - 8;
      if (output[bitIndex])
        dest[1 + (bitsSinceCrc / 6)] |= (1 << (bitsSinceCrc % 6));
    }
  }

  return result;
}

/* Decode received symbols from RADE decoder. */
void rade_text_rx(rade_text_t ptr, float* syms)
{
    rade_text_impl_t* obj = (rade_text_impl_t*)ptr;
    assert(obj != NULL);

    // Copy over symbols prior to decode.
    for (int index = 0; index < LDPC_TOTAL_SIZE_BITS / 2; index++)
    {
        complex float symbol = CMPLXF(syms[2 * index], syms[2 * index + 1]);
        *(complex float*)&obj->inbound_pending_syms[index] = symbol * cmplx(ROT45);

        float* sym = (float*)&obj->inbound_pending_syms[index];
        obj->inbound_pending_amps[index] = sqrt(pow(sym[0], 2) + pow(sym[1], 2));
    }
    
    // We have all the bits we need, so we're ready to decode.
    char decodedStr[RADE_TEXT_MAX_RAW_LENGTH + 1];
    char rawStr[RADE_TEXT_MAX_RAW_LENGTH + 1];
    memset(rawStr, 0, RADE_TEXT_MAX_RAW_LENGTH + 1);
    memset(decodedStr, 0, RADE_TEXT_MAX_RAW_LENGTH + 1);

    if (rade_text_ldpc_decode(obj, rawStr) != 0) {
        // BER is under limits.
        convert_ota_string_to_callsign_(&rawStr[RADE_TEXT_CRC_LENGTH],
                                        &decodedStr[RADE_TEXT_CRC_LENGTH],
                                        RADE_TEXT_MAX_LENGTH);
        decodedStr[0] = rawStr[0];  // CRC

        // Get expected and actual CRC.
        unsigned char receivedCRC = decodedStr[0];
        unsigned char calcCRC = calculateCRC8_(
            &rawStr[RADE_TEXT_CRC_LENGTH], RADE_TEXT_MAX_LENGTH);

        log_info("rxCRC: %d, calcCRC: %d, decodedStr: %s",
           receivedCRC, calcCRC, &decodedStr[RADE_TEXT_CRC_LENGTH]);

        if (receivedCRC == calcCRC) {
          // We got a valid string. Call assigned callback.
          obj->text_rx_callback(obj, &decodedStr[RADE_TEXT_CRC_LENGTH],
                                strlen(&decodedStr[RADE_TEXT_CRC_LENGTH]),
                                obj->callback_state);
        }
  }
}

rade_text_t rade_text_create() {
  rade_text_impl_t* ret = calloc(1, sizeof(rade_text_impl_t));
  assert(ret != NULL);

  // Load LDPC code into memory.
  int code_index = ldpc_codes_find("HRA_56_56");
  memcpy(&ret->ldpc, &ldpc_codes[code_index], sizeof(struct LDPC));

  return (rade_text_t)ret;
}

void rade_text_destroy(rade_text_t ptr) {
  assert(ptr != NULL);
  free(ptr);
}

void rade_text_generate_tx_string(
    rade_text_t ptr, const char* str, int strlength,
    float* syms)
{
  rade_text_impl_t* impl = (rade_text_impl_t*)ptr;
  assert(impl != NULL);

  char tmp[RADE_TEXT_MAX_RAW_LENGTH + 1];
  memset(tmp, 0, RADE_TEXT_MAX_RAW_LENGTH + 1);

  convert_callsign_to_ota_string_(str, &tmp[RADE_TEXT_CRC_LENGTH],
                                  strlength < RADE_TEXT_MAX_LENGTH
                                      ? strlength
                                      : RADE_TEXT_MAX_LENGTH);

  int txt_length = strlen(&tmp[RADE_TEXT_CRC_LENGTH]);
  if (txt_length >= RADE_TEXT_MAX_LENGTH) {
    txt_length = RADE_TEXT_MAX_LENGTH;
  }
  impl->tx_text_length = LDPC_TOTAL_SIZE_BITS;
  impl->tx_text_index = 0;
  unsigned char crc =
      calculateCRC8_(&tmp[RADE_TEXT_CRC_LENGTH], txt_length);
  tmp[0] = crc;

  // Encode block of text using LDPC(112,56).
  unsigned char ibits[LDPC_TOTAL_SIZE_BITS / 2];
  unsigned char pbits[LDPC_TOTAL_SIZE_BITS / 2];
  memset(ibits, 0, LDPC_TOTAL_SIZE_BITS / 2);
  memset(pbits, 0, LDPC_TOTAL_SIZE_BITS / 2);
  for (int index = 0; index < 8; index++) {
    if (tmp[0] & (1 << index)) ibits[index] = 1;
  }

  // Pack 6 bit characters into single LDPC block.
  for (int ibitsBitIndex = 8; ibitsBitIndex < (LDPC_TOTAL_SIZE_BITS / 2);
       ibitsBitIndex++) {
    int bitsFromCrc = ibitsBitIndex - 8;
    unsigned int byte = tmp[RADE_TEXT_CRC_LENGTH + bitsFromCrc / 6];
    unsigned int bitToCheck = bitsFromCrc % 6;
    // fprintf(stderr, "bit index: %d, byte: %x, bit to check: %d, result:
    // %d\n", ibitsBitIndex, byte, bitToCheck, (byte & (1 << bitToCheck)) != 0);

    if (byte & (1 << bitToCheck)) {
      ibits[ibitsBitIndex] = 1;
    }
  }

  encode(&impl->ldpc, ibits, pbits);

  // Split LDPC encoded bits into individual bits, with the first
  // RADE_TEXT_UW_LENGTH_BITS being UW.
  char tmpbits[LDPC_TOTAL_SIZE_BITS];

  memset(impl->tx_text, 0, LDPC_TOTAL_SIZE_BITS);
  memcpy(&tmpbits[0], &ibits[0], LDPC_TOTAL_SIZE_BITS / 2);
  memcpy(&tmpbits[LDPC_TOTAL_SIZE_BITS / 2], &pbits[0],
         LDPC_TOTAL_SIZE_BITS / 2);

  // Interleave the bits together to enhance fading performance.
  memcpy(impl->tx_text, tmpbits, LDPC_TOTAL_SIZE_BITS);
  /*gp_interleave_bits(&impl->tx_text[0], tmpbits,
                     LDPC_TOTAL_SIZE_BITS / 2);*/

  // Generate floats based on the bits.
  for (int index = 0; index < LDPC_TOTAL_SIZE_BITS; index++)
  {
    syms[index] = 2.0*impl->tx_text[index] - 1.0;
  }
}

void rade_text_set_rx_callback(rade_text_t ptr, on_text_rx_t text_rx_fn, void* state)
{
    rade_text_impl_t* impl = (rade_text_impl_t*)ptr;
    assert(impl != NULL);

    impl->text_rx_callback = text_rx_fn;
    impl->callback_state = state;
}