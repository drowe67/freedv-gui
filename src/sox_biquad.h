//==========================================================================
// Name:            sox_biquad.h
// Purpose:         Interface into Sox Biquad filters 
// Created:         Dec 1, 2012
// Authors:         David Rowe
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

#ifndef __SOX_BIQUAD__
#define __SOX_BIQUAD__

#ifdef __cplusplus
extern "C" {

#endif

void sox_biquad_start(void);
void sox_biquad_finish(void);
void *sox_biquad_create(int argc, const char *argv[]);
void sox_biquad_destroy(void *sbq);
void sox_biquad_filter(void *sbq, short out[], short in[], int n);

#ifdef __cplusplus
}
#endif

#endif
