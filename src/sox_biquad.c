//==========================================================================
// Name:            sox_biquad.c
// Purpose:         Interface into Sox Biquad filters 
// Created:         Dec 1, 2012
// Authors:         David Rowe
// 
// To test:
/*
          $ gcc sox_biquad.c sox/effects_i.c sox/effects.c sox/formats_i.c \
            sox/biquad.c sox/biquads.c sox/xmalloc.c sox/libsox.c \
            -o sox_biquad -DSOX_BIQUAD_UNITTEST -D__FREEDV__ \
            -Wall -lm -lsndfile -g
          $ ./sox_biquad
*/
//
// License:
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License version 2.1,
//  as published by the Free Software Foundation.  This program is
//  distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
//  License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, see <http://www.gnu.org/licenses/>.
//
//==========================================================================

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sox/sox.h"

#include "sox_biquad.h"

#include "util/logging/ulog.h"


int lsx_biquad_flow(sox_effect_t * effp, const sox_sample_t *ibuf,
                    sox_sample_t *obuf, size_t *isamp, size_t *osamp);

void sox_biquad_start(void)
{
    int r = sox_init();
    assert(r == SOX_SUCCESS);
}

void sox_biquad_finish(void)
{
    sox_quit();
}

/*
  Effect must be implemented by biquads.c in sox, arguments are just
  like sox command line, for example:

  char *argv[10];
  argv[0] = "highpass"; argv[1]="1000"; argc=1;
*/

void *sox_biquad_create(int argc, const char *argv[])
{
    int ret;
    sox_effect_t *e;
    int (*start)(sox_effect_t *); /* function pointer to effect start func */
    long sampleRate = 8000;

    if (strcmp(argv[0], "equalizer") == 0 || strcmp(argv[0], "vol") == 0) {
        if (argc == 4)
        {
            sampleRate = atol(argv[4]);
            argc--;
        }
    }
    else {
        if (argc == 3)
        {
            sampleRate = atol(argv[3]);
            argc--;
        }
    }

    e = sox_create_effect(sox_find_effect(argv[0])); assert(e != NULL);

    ret = sox_effect_options(e, argc, (char * const*)&argv[1]);
    assert(ret == SOX_SUCCESS);

    start = e->handler.start;

    e->in_signal.rate = (double)sampleRate;
    
    // Force flows to 1. This is needed to ensure that sox_delete_effect() deallocates
    // all memory used by the effect.
    e->flows = 1;
    
    ret = start(e);
    if (ret != SOX_SUCCESS && ret != SOX_EFF_NULL)
    {
        log_error("sox_biquad ret = %d (%s)", ret, sox_strerror(ret)); 
        assert(0);
    }
    else if (ret == SOX_EFF_NULL)
    {
        // no-op effect, no need to have it
        sox_biquad_destroy(e);
        e = NULL;
    }
    
    return (void *)e;
}

void sox_biquad_destroy(void *sbq) {
    sox_effect_t *e = (sox_effect_t *)sbq;
    sox_delete_effect(e);
}

void sox_biquad_filter(void *sbq, short out[], short in[], int n) FREEDV_NONBLOCKING_EXCEPT
{
    sox_effect_t *e = (sox_effect_t *)sbq;
    sox_sample_t ibuf[n];
    sox_sample_t obuf[n];
    size_t isamp, osamp;
    unsigned int clips;
    SOX_SAMPLE_LOCALS; 
    int i;

    clips = 0;
    for(i=0; i<n; i++)
        ibuf[i] = SOX_SIGNED_16BIT_TO_SAMPLE(in[i], clips);
    isamp = osamp = (unsigned int)n;
    e->handler.flow(e, ibuf, obuf, &isamp, &osamp);
    for(i=0; i<n; i++)
        out[i] = SOX_SAMPLE_TO_SIGNED_16BIT(obuf[i], clips); 
}


#ifdef SOX_BIQUAD_UNITTEST
#define N 20
int main(void) {
    void *sbq;
    const char *argv[] = {"highpass", "1000"};
    short in[N];
    short out[N];
    int   i, argc;;

    for(i=0; i<N; i++)
        in[i] = 0;
    in[0] = 8000;

    sox_biquad_start();
    //argv[0] = "highpass"; argv[1]="1000"; 
    argc=1;
    sbq = sox_biquad_create(argc, argv);

    sox_biquad_filter(sbq, out, in, N);
    for(i=0; i<N; i++)
        printf("%d\n", out[i]);
   
    sox_biquad_destroy(sbq);
    sox_biquad_finish();

    return 0;
}
#endif
