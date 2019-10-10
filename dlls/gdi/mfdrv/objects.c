/*
 * GDI objects
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "wownt32.h"
#include "mfdrv/metafiledrv.h"
#include "gdi.h"
#include "gdi_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(metafile);

/******************************************************************
 *         MFDRV_AddHandle
 */
UINT MFDRV_AddHandle( PHYSDEV dev, HGDIOBJ obj )
{
    METAFILEDRV_PDEVICE *physDev = (METAFILEDRV_PDEVICE *)dev;
    INT16 index;

    for(index = 0; index < physDev->handles_size; index++)
        if(physDev->handles[index] == 0) break; 
    if(index == physDev->handles_size) {
        physDev->handles_size += HANDLE_LIST_INC;
        physDev->handles = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                       physDev->handles,
                                       physDev->handles_size * sizeof(physDev->handles[0]));
    }
    physDev->handles[index] = obj;

    physDev->cur_handles++; 
    if(physDev->cur_handles > physDev->mh->mtNoObjects)
        physDev->mh->mtNoObjects++;

    return index ; /* index 0 is not reserved for metafiles */
}

/******************************************************************
 *         MFDRV_FindObject
 */
static INT16 MFDRV_FindObject( PHYSDEV dev, HGDIOBJ obj )
{
    METAFILEDRV_PDEVICE *physDev = (METAFILEDRV_PDEVICE *)dev;
    INT16 index;

    for(index = 0; index < physDev->handles_size; index++)
        if(physDev->handles[index] == obj) break;

    if(index == physDev->handles_size) return -1;

    return index ;
}


/******************************************************************
 *         MFDRV_DeleteObject
 */
BOOL MFDRV_DeleteObject( PHYSDEV dev, HGDIOBJ obj )
{   
    METARECORD mr;
    METAFILEDRV_PDEVICE *physDev = (METAFILEDRV_PDEVICE *)dev;
    INT16 index;
    BOOL ret = TRUE;

    index = MFDRV_FindObject(dev, obj);
    if( index < 0 )
        return 0;

    mr.rdSize = sizeof mr / 2;
    mr.rdFunction = META_DELETEOBJECT;
    mr.rdParm[0] = index;

    if(!MFDRV_WriteRecord( dev, &mr, mr.rdSize*2 ))
        ret = FALSE;

    physDev->handles[index] = 0;
    physDev->cur_handles--;
    return ret;
}


/***********************************************************************
 *           MFDRV_SelectObject
 */
static BOOL MFDRV_SelectObject( PHYSDEV dev, INT16 index)
{
    METARECORD mr;

    mr.rdSize = sizeof mr / 2;
    mr.rdFunction = META_SELECTOBJECT;
    mr.rdParm[0] = index;

    return MFDRV_WriteRecord( dev, &mr, mr.rdSize*2 );
}


/***********************************************************************
 *           MFDRV_SelectBitmap
 */
HBITMAP MFDRV_SelectBitmap( PHYSDEV dev, HBITMAP hbitmap )
{
    return 0;
}


/******************************************************************
 *         MFDRV_CreateBrushIndirect
 */

INT16 MFDRV_CreateBrushIndirect(PHYSDEV dev, HBRUSH hBrush )
{
    DWORD size;
    METARECORD *mr;
    LOGBRUSH logbrush;
    METAFILEDRV_PDEVICE *physDev = (METAFILEDRV_PDEVICE *)dev;
    BOOL r;

    if (!GetObjectA( hBrush, sizeof(logbrush), &logbrush )) return -1;

    switch(logbrush.lbStyle)
    {
    case BS_SOLID:
    case BS_NULL:
    case BS_HATCHED:
        {
	    LOGBRUSH16 lb16;

	    lb16.lbStyle = logbrush.lbStyle;
	    lb16.lbColor = logbrush.lbColor;
	    lb16.lbHatch = logbrush.lbHatch;
	    size = sizeof(METARECORD) + sizeof(LOGBRUSH16) - 2;
	    mr = HeapAlloc( GetProcessHeap(), 0, size );
	    mr->rdSize = size / 2;
	    mr->rdFunction = META_CREATEBRUSHINDIRECT;
	    memcpy( mr->rdParm, &lb16, sizeof(LOGBRUSH16));
	    break;
	}
    case BS_PATTERN:
        {
	    BITMAP bm;
	    BYTE *bits;
	    BITMAPINFO *info;
	    DWORD bmSize;

	    GetObjectA((HANDLE)logbrush.lbHatch, sizeof(bm), &bm);
	    if(bm.bmBitsPixel != 1 || bm.bmPlanes != 1) {
	        FIXME("Trying to store a colour pattern brush\n");
		goto done;
	    }

	    bmSize = DIB_GetDIBImageBytes(bm.bmWidth, bm.bmHeight, 1);

	    size = sizeof(METARECORD) + sizeof(WORD) + sizeof(BITMAPINFO) +
	      sizeof(RGBQUAD) + bmSize;

	    mr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
	    if(!mr) goto done;
	    mr->rdFunction = META_DIBCREATEPATTERNBRUSH;
	    mr->rdSize = size / 2;
	    mr->rdParm[0] = BS_PATTERN;
	    mr->rdParm[1] = DIB_RGB_COLORS;
	    info = (BITMAPINFO *)(mr->rdParm + 2);

	    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	    info->bmiHeader.biWidth = bm.bmWidth;
	    info->bmiHeader.biHeight = bm.bmHeight;
	    info->bmiHeader.biPlanes = 1;
	    info->bmiHeader.biBitCount = 1;
	    bits = ((BYTE *)info) + sizeof(BITMAPINFO) + sizeof(RGBQUAD);

	    GetDIBits(physDev->hdc, (HANDLE)logbrush.lbHatch, 0, bm.bmHeight,
		      bits, info, DIB_RGB_COLORS);
	    *(DWORD *)info->bmiColors = 0;
	    *(DWORD *)(info->bmiColors + 1) = 0xffffff;
	    break;
	}

    case BS_DIBPATTERN:
        {
	      BITMAPINFO *info;
	      DWORD bmSize, biSize;

	      info = GlobalLock16((HGLOBAL16)logbrush.lbHatch);
	      if (info->bmiHeader.biCompression)
		  bmSize = info->bmiHeader.biSizeImage;
	      else
		  bmSize = DIB_GetDIBImageBytes(info->bmiHeader.biWidth,
						info->bmiHeader.biHeight,
						info->bmiHeader.biBitCount);
	      biSize = DIB_BitmapInfoSize(info, LOWORD(logbrush.lbColor));
	      size = sizeof(METARECORD) + biSize + bmSize + 2;
	      mr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
	      if(!mr) goto done;
	      mr->rdFunction = META_DIBCREATEPATTERNBRUSH;
	      mr->rdSize = size / 2;
	      *(mr->rdParm) = logbrush.lbStyle;
	      *(mr->rdParm + 1) = LOWORD(logbrush.lbColor);
	      memcpy(mr->rdParm + 2, info, biSize + bmSize);
	      break;
	}
	default:
	    FIXME("Unkonwn brush style %x\n", logbrush.lbStyle);
	    return 0;
    }
    r = MFDRV_WriteRecord( dev, mr, mr->rdSize * 2);
    HeapFree(GetProcessHeap(), 0, mr);
    if( !r )
        return -1;
done:
    return MFDRV_AddHandle( dev, hBrush );
}


/***********************************************************************
 *           MFDRV_SelectBrush
 */
HBRUSH MFDRV_SelectBrush( PHYSDEV dev, HBRUSH hbrush )
{
    METAFILEDRV_PDEVICE *physDev = (METAFILEDRV_PDEVICE *)dev;
    INT16 index;

    index = MFDRV_FindObject(dev, hbrush);
    if( index < 0 )
    {
        index = MFDRV_CreateBrushIndirect( dev, hbrush );
        if( index < 0 )
            return 0;
        GDI_hdc_using_object(hbrush, physDev->hdc);
    }
    return MFDRV_SelectObject( dev, index ) ? hbrush : HGDI_ERROR;
}

/******************************************************************
 *         MFDRV_CreateFontIndirect
 */

static UINT16 MFDRV_CreateFontIndirect(PHYSDEV dev, HFONT hFont, LOGFONT16 *logfont)
{
    char buffer[sizeof(METARECORD) - 2 + sizeof(LOGFONT16)];
    METARECORD *mr = (METARECORD *)&buffer;

    mr->rdSize = (sizeof(METARECORD) + sizeof(LOGFONT16) - 2) / 2;
    mr->rdFunction = META_CREATEFONTINDIRECT;
    memcpy(&(mr->rdParm), logfont, sizeof(LOGFONT16));
    if (!(MFDRV_WriteRecord( dev, mr, mr->rdSize * 2)))
        return 0;
    return MFDRV_AddHandle( dev, hFont );
}


/***********************************************************************
 *           MFDRV_SelectFont
 */
HFONT MFDRV_SelectFont( PHYSDEV dev, HFONT hfont, HANDLE gdiFont )
{
    METAFILEDRV_PDEVICE *physDev = (METAFILEDRV_PDEVICE *)dev;
    LOGFONT16 lf16;
    INT16 index;

    index = MFDRV_FindObject(dev, hfont);
    if( index < 0 )
    {
        if (!GetObject16( HFONT_16(hfont), sizeof(lf16), &lf16 ))
            return HGDI_ERROR;
        index = MFDRV_CreateFontIndirect(dev, hfont, &lf16);
        if( index < 0 )
            return HGDI_ERROR;
        GDI_hdc_using_object(hfont, physDev->hdc);
    }
    return MFDRV_SelectObject( dev, index ) ? hfont : HGDI_ERROR;
}

/******************************************************************
 *         MFDRV_CreatePenIndirect
 */
static UINT16 MFDRV_CreatePenIndirect(PHYSDEV dev, HPEN hPen, LOGPEN16 *logpen)
{
    char buffer[sizeof(METARECORD) - 2 + sizeof(*logpen)];
    METARECORD *mr = (METARECORD *)&buffer;

    mr->rdSize = (sizeof(METARECORD) + sizeof(*logpen) - 2) / 2;
    mr->rdFunction = META_CREATEPENINDIRECT;
    memcpy(&(mr->rdParm), logpen, sizeof(*logpen));
    if (!(MFDRV_WriteRecord( dev, mr, mr->rdSize * 2)))
        return 0;
    return MFDRV_AddHandle( dev, hPen );
}


/***********************************************************************
 *           MFDRV_SelectPen
 */
HPEN MFDRV_SelectPen( PHYSDEV dev, HPEN hpen )
{
    METAFILEDRV_PDEVICE *physDev = (METAFILEDRV_PDEVICE *)dev;
    LOGPEN16 logpen;
    INT16 index;

    index = MFDRV_FindObject(dev, hpen);
    if( index < 0 )
    {
        if (!GetObject16( HPEN_16(hpen), sizeof(logpen), &logpen ))
            return 0;
        index = MFDRV_CreatePenIndirect( dev, hpen, &logpen );
        if( index < 0 )
            return 0;
        GDI_hdc_using_object(hpen, physDev->hdc);
    }
    return MFDRV_SelectObject( dev, index ) ? hpen : HGDI_ERROR;
}


/******************************************************************
 *         MFDRV_CreatePalette
 */
static BOOL MFDRV_CreatePalette(PHYSDEV dev, HPALETTE hPalette, LOGPALETTE* logPalette, int sizeofPalette)
{
    int index;
    BOOL ret;
    METARECORD *mr;

    mr = HeapAlloc( GetProcessHeap(), 0, sizeof(METARECORD) + sizeofPalette - sizeof(WORD) );
    mr->rdSize = (sizeof(METARECORD) + sizeofPalette - sizeof(WORD)) / sizeof(WORD);
    mr->rdFunction = META_CREATEPALETTE;
    memcpy(&(mr->rdParm), logPalette, sizeofPalette);
    if (!(MFDRV_WriteRecord( dev, mr, mr->rdSize * sizeof(WORD))))
    {
        HeapFree(GetProcessHeap(), 0, mr);
        return FALSE;
    }

    mr->rdSize = sizeof(METARECORD) / sizeof(WORD);
    mr->rdFunction = META_SELECTPALETTE;

    if ((index = MFDRV_AddHandle( dev, hPalette )) == -1) ret = FALSE;
    else
    {
        *(mr->rdParm) = index;
        ret = MFDRV_WriteRecord( dev, mr, mr->rdSize * sizeof(WORD));
    }
    HeapFree(GetProcessHeap(), 0, mr);
    return ret;
}


/***********************************************************************
 *           MFDRV_SelectPalette
 */
HPALETTE MFDRV_SelectPalette( PHYSDEV dev, HPALETTE hPalette, BOOL bForceBackground )
{
#define PALVERSION 0x0300

    PLOGPALETTE logPalette;
    WORD        wNumEntries = 0;
    BOOL        creationSucceed;
    int         sizeofPalette;

    GetObjectA(hPalette, sizeof(WORD), (LPSTR) &wNumEntries);

    if (wNumEntries == 0) return 0;

    sizeofPalette = sizeof(LOGPALETTE) + ((wNumEntries-1) * sizeof(PALETTEENTRY));
    logPalette = HeapAlloc( GetProcessHeap(), 0, sizeofPalette );

    if (logPalette == NULL) return 0;

    logPalette->palVersion = PALVERSION;
    logPalette->palNumEntries = wNumEntries;

    GetPaletteEntries(hPalette, 0, wNumEntries, logPalette->palPalEntry);

    creationSucceed = MFDRV_CreatePalette( dev, hPalette, logPalette, sizeofPalette );

    HeapFree( GetProcessHeap(), 0, logPalette );

    if (creationSucceed)
        return hPalette;

    return 0;
}

/***********************************************************************
 *           MFDRV_RealizePalette
 */
UINT MFDRV_RealizePalette(PHYSDEV dev, HPALETTE hPalette, BOOL dummy)
{
    char buffer[sizeof(METARECORD) - sizeof(WORD)];
    METARECORD *mr = (METARECORD *)&buffer;

    mr->rdSize = (sizeof(METARECORD) - sizeof(WORD)) / sizeof(WORD);
    mr->rdFunction = META_REALIZEPALETTE;

    if (!(MFDRV_WriteRecord( dev, mr, mr->rdSize * sizeof(WORD)))) return 0;

    /* The return value is suppose to be the number of entries
       in the logical palette mapped to the system palette or 0
       if the function failed. Since it's not trivial here to
       get that kind of information and since it's of little
       use in the case of metafiles, we'll always return 1. */
    return 1;
}
