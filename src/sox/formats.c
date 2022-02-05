/* Implements the public API for using libSoX file formats.
 * All public functions & data are prefixed with sox_ .
 *
 * (c) 2005-8 Chris Bagwell and SoX contributors
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define _GNU_SOURCE
#include "sox_i.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_IO_H
  #include <io.h>
#endif

#if HAVE_MAGIC
  #include <magic.h>
#endif

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#define PIPE_AUTO_DETECT_SIZE 256 /* Only as much as we can rewind a pipe */
#define AUTO_DETECT_SIZE 4096     /* For seekable file, so no restriction */

void sox_format_quit(void) /* Cleanup things.  */
{
#ifdef HAVE_LIBLTDL
  int ret;
  if (plugins_initted && (ret = lt_dlexit()) != 0)
    lsx_fail("lt_dlexit failed with %d error(s): %s", ret, lt_dlerror());
  plugins_initted = sox_false;
  nformats = NSTATIC_FORMATS;
#endif
}

unsigned sox_precision(sox_encoding_t encoding, unsigned bits_per_sample)
{
  switch (encoding) {
    case SOX_ENCODING_DWVW:       return bits_per_sample;
    case SOX_ENCODING_DWVWN:      return !bits_per_sample? 16: 0; /* ? */
    case SOX_ENCODING_HCOM:       return !(bits_per_sample & 7) && (bits_per_sample >> 3) - 1 < 1? bits_per_sample: 0;
    case SOX_ENCODING_WAVPACK:
    case SOX_ENCODING_FLAC:       return !(bits_per_sample & 7) && (bits_per_sample >> 3) - 1 < 4? bits_per_sample: 0;
    case SOX_ENCODING_SIGN2:      return bits_per_sample <= 32? bits_per_sample : 0;
    case SOX_ENCODING_UNSIGNED:   return !(bits_per_sample & 7) && (bits_per_sample >> 3) - 1 < 4? bits_per_sample: 0;

    case SOX_ENCODING_ALAW:       return bits_per_sample == 8? 13: 0;
    case SOX_ENCODING_ULAW:       return bits_per_sample == 8? 14: 0;

    case SOX_ENCODING_CL_ADPCM:   return bits_per_sample? 8: 0;
    case SOX_ENCODING_CL_ADPCM16: return bits_per_sample == 4? 13: 0;
    case SOX_ENCODING_MS_ADPCM:   return bits_per_sample == 4? 14: 0;
    case SOX_ENCODING_IMA_ADPCM:  return bits_per_sample == 4? 13: 0;
    case SOX_ENCODING_OKI_ADPCM:  return bits_per_sample == 4? 12: 0;
    case SOX_ENCODING_G721:       return bits_per_sample == 4? 12: 0;
    case SOX_ENCODING_G723:       return bits_per_sample == 3? 8:
                                         bits_per_sample == 5? 14: 0;
    case SOX_ENCODING_CVSD:       return bits_per_sample == 1? 16: 0;
    case SOX_ENCODING_DPCM:       return bits_per_sample; /* ? */

    case SOX_ENCODING_MP3:        return 0; /* Accept the precision returned by the format. */

    case SOX_ENCODING_GSM:
    case SOX_ENCODING_VORBIS:
    case SOX_ENCODING_OPUS:
    case SOX_ENCODING_AMR_WB:
    case SOX_ENCODING_AMR_NB:
    case SOX_ENCODING_LPC10:      return !bits_per_sample? 16: 0;

    case SOX_ENCODING_WAVPACKF:
    case SOX_ENCODING_FLOAT:      return bits_per_sample == 32 ? 25: bits_per_sample == 64 ? 54: 0;
    case SOX_ENCODING_FLOAT_TEXT: return !bits_per_sample? 54: 0;

    case SOX_ENCODINGS:
    case SOX_ENCODING_UNKNOWN:    break;
  }
  return 0;
}
