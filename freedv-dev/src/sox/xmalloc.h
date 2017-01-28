/* libSoX Memory allocation functions
 *
 * Copyright (c) 2005-2006 Reuben Thomas.  All rights reserved.
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

#ifndef LSX_MALLOC_H
#define LSX_MALLOC_H

#include <stddef.h>
#include <string.h>

#define lsx_malloc(size) lsx_realloc(NULL, (size))
#define lsx_calloc(n,s) (((n)*(s))? memset(lsx_malloc((n)*(s)),0,(n)*(s)) : NULL)
#define lsx_Calloc(v,n)  v = lsx_calloc(n,sizeof(*(v)))
#define lsx_strdup(p) ((p)? strcpy((char *)lsx_malloc(strlen(p) + 1), p) : NULL)
#define lsx_memdup(p,s) ((p)? memcpy(lsx_malloc(s), p, s) : NULL)
#define lsx_valloc(v,n)  v = lsx_malloc((n)*sizeof(*(v)))
#define lsx_revalloc(v,n)  v = lsx_realloc(v, (n)*sizeof(*(v)))

#endif
