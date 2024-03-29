/* DirectMusic Buffer Format
 *
 * Copyright (C) 2003-2004 Rok Mandeljc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __WINE_DMUSIC_BUFFER_H
#define __WINE_DMUSIC_BUFFER_H

/*****************************************************************************
 * Misc. definitions
 */
#define QWORD_ALIGN(x) (((x) + 7) & ~7)
#define DMUS_EVENT_SIZE(cb) QWORD_ALIGN(sizeof(DMUS_EVENTHEADER) + cb)

/*****************************************************************************
 * Flags
 */
#define DMUS_EVENT_STRUCTURED   0x1

/*****************************************************************************
 * Structures
 */
/* typedef definitions */
typedef struct _DMUS_EVENTHEADER DMUS_EVENTHEADER, *LPDMUS_EVENTHEADER;

/* actual structure*/ 
#include <pshpack4.h>
struct _DMUS_EVENTHEADER {
    DWORD           cbEvent;
    DWORD           dwChannelGroup;
    REFERENCE_TIME  rtDelta;
    DWORD           dwFlags;
};
#include <poppack.h>

#endif /* __WINE_DMUSIC_BUFFER_H */
