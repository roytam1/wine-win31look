/*
 * Unicode routines for use inside the server
 *
 * Copyright (C) 1999 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __WINE_SERVER_UNICODE_H
#define __WINE_SERVER_UNICODE_H

#include <stdio.h>

#include "windef.h"
#include "wine/unicode.h"
#include "object.h"

static inline WCHAR *strdupW( const WCHAR *str )
{
    size_t len = (strlenW(str) + 1) * sizeof(WCHAR);
    return memdup( str, len );
}

extern int dump_strW( const WCHAR *str, size_t len, FILE *f, const char escape[2] );

#endif  /* __WINE_SERVER_UNICODE_H */
