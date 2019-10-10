/*
 * GDI definitions
 *
 * Copyright 1993 Alexandre Julliard
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

#ifndef __WINE_GDI_H
#define __WINE_GDI_H

#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <wingdi.h>
#include <wine/wingdi16.h>
#include <math.h>

  /* GDI objects magic numbers */
#define FIRST_MAGIC           0x4f47
#define PEN_MAGIC             0x4f47
#define BRUSH_MAGIC           0x4f48
#define FONT_MAGIC            0x4f49
#define PALETTE_MAGIC         0x4f4a
#define BITMAP_MAGIC          0x4f4b
#define REGION_MAGIC          0x4f4c
#define DC_MAGIC              0x4f4d
#define DISABLED_DC_MAGIC     0x4f4e
#define META_DC_MAGIC         0x4f4f
#define METAFILE_MAGIC        0x4f50
#define METAFILE_DC_MAGIC     0x4f51
#define ENHMETAFILE_MAGIC     0x4f52
#define ENHMETAFILE_DC_MAGIC  0x4f53
#define MEMORY_DC_MAGIC       0x4f54
#define LAST_MAGIC            0x4f54

#define MAGIC_DONTCARE	      0xffff

/* GDI constants for making objects private/system (naming undoc. !) */
#define OBJECT_PRIVATE        0x2000
#define OBJECT_NOSYSTEM       0x8000

#define GDIMAGIC(magic) ((magic) & ~(OBJECT_PRIVATE|OBJECT_NOSYSTEM))

struct gdi_obj_funcs;
struct hdc_list;

typedef struct tagGDIOBJHDR
{
    HANDLE16    hNext;
    WORD        wMagic;
    DWORD       dwCount;
    const struct gdi_obj_funcs *funcs;
    struct hdc_list *hdcs;
} GDIOBJHDR;

/* extra stock object: default 1x1 bitmap for memory DCs */
#define DEFAULT_BITMAP (STOCK_LAST+1)

/* bitmap object */

typedef struct tagBITMAPOBJ
{
    GDIOBJHDR   header;
    BITMAP      bitmap;
    SIZE        size;   /* For SetBitmapDimension() */
    const struct tagDC_FUNCS *funcs; /* DC function table */
    void	*physBitmap; /* ptr to device specific data */
    /* For device-independent bitmaps: */
    DIBSECTION *dib;
    SEGPTR      segptr_bits;  /* segptr to DIB bits */
} BITMAPOBJ;

/* palette object */

#define NB_RESERVED_COLORS     20   /* number of fixed colors in system palette */

#define PC_SYS_USED            0x80 /* palentry is used (both system and logical) */
#define PC_SYS_RESERVED        0x40 /* system palentry is not to be mapped to */
#define PC_SYS_MAPPED          0x10 /* logical palentry is a direct alias for system palentry */

typedef struct tagPALETTEOBJ
{
    GDIOBJHDR                    header;
    int                          *mapping;
    LOGPALETTE                   logpalette; /* _MUST_ be the last field */
} PALETTEOBJ;

  /* GDI local heap */

extern void *GDI_GetObjPtr( HGDIOBJ, WORD );
extern void GDI_ReleaseObj( HGDIOBJ );

#define WINE_GGO_GRAY16_BITMAP 0x7f

#endif  /* __WINE_GDI_H */
