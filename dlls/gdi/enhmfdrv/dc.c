/*
 * Enhanced MetaFile driver dc value functions
 *
 * Copyright 1999 Huw D M Davies
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

#include "enhmfdrv/enhmetafiledrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(enhmetafile);

INT EMFDRV_SaveDC( PHYSDEV dev )
{
    EMRSAVEDC emr;
    emr.emr.iType = EMR_SAVEDC;
    emr.emr.nSize = sizeof(emr);
    return EMFDRV_WriteRecord( dev, &emr.emr );
}

BOOL EMFDRV_RestoreDC( PHYSDEV dev, INT level )
{
    EMFDRV_PDEVICE* physDev = (EMFDRV_PDEVICE*)dev;
    EMRRESTOREDC emr;

    emr.emr.iType = EMR_RESTOREDC;
    emr.emr.nSize = sizeof(emr);
    emr.iRelative = -1;
    if (level == -1) 
        return EMFDRV_WriteRecord( dev, &emr.emr );
    else if (level > 0 && level <= physDev->dc->saveLevel) 
    {
        while (level >= physDev->dc->saveLevel) 
        {
            EMFDRV_WriteRecord( dev, &emr.emr );
            level--;
        }
        return TRUE;
    }
    return FALSE;
}

UINT EMFDRV_SetTextAlign( PHYSDEV dev, UINT align )
{
    EMRSETTEXTALIGN emr;
    emr.emr.iType = EMR_SETTEXTALIGN;
    emr.emr.nSize = sizeof(emr);
    emr.iMode = align;
    return EMFDRV_WriteRecord( dev, &emr.emr );
}

BOOL EMFDRV_SetTextJustification(PHYSDEV dev, INT nBreakExtra, INT nBreakCount)
{
    EMRSETTEXTJUSTIFICATION emr;
    emr.emr.iType = EMR_SETTEXTJUSTIFICATION;
    emr.emr.nSize = sizeof(emr);
    emr.nBreakExtra = nBreakExtra;
    emr.nBreakCount = nBreakCount;
    return EMFDRV_WriteRecord(dev, &emr.emr);
}

INT EMFDRV_SetBkMode( PHYSDEV dev, INT mode )
{
    EMRSETBKMODE emr;
    emr.emr.iType = EMR_SETBKMODE;
    emr.emr.nSize = sizeof(emr);
    emr.iMode = mode;
    return EMFDRV_WriteRecord( dev, &emr.emr );
}

INT EMFDRV_SetROP2( PHYSDEV dev, INT rop )
{
    EMRSETROP2 emr;
    emr.emr.iType = EMR_SETROP2;
    emr.emr.nSize = sizeof(emr);
    emr.iMode = rop;
    return EMFDRV_WriteRecord( dev, &emr.emr );
}

INT EMFDRV_SetPolyFillMode( PHYSDEV dev, INT mode )
{
    EMRSETPOLYFILLMODE emr;
    emr.emr.iType = EMR_SETPOLYFILLMODE;
    emr.emr.nSize = sizeof(emr);
    emr.iMode = mode;
    return EMFDRV_WriteRecord( dev, &emr.emr );
}

INT EMFDRV_SetStretchBltMode( PHYSDEV dev, INT mode )
{
    EMRSETSTRETCHBLTMODE emr;
    emr.emr.iType = EMR_SETSTRETCHBLTMODE;
    emr.emr.nSize = sizeof(emr);
    emr.iMode = mode;
    return EMFDRV_WriteRecord( dev, &emr.emr );
}

INT EMFDRV_ExcludeClipRect( PHYSDEV dev, INT left, INT top, INT right, INT bottom )
{
    EMREXCLUDECLIPRECT emr;
    emr.emr.iType      = EMR_EXCLUDECLIPRECT;
    emr.emr.nSize      = sizeof(emr);
    emr.rclClip.left   = left;
    emr.rclClip.top    = top;
    emr.rclClip.right  = right;
    emr.rclClip.bottom = bottom;
    return EMFDRV_WriteRecord( dev, &emr.emr );
}

INT EMFDRV_IntersectClipRect( PHYSDEV dev, INT left, INT top, INT right, INT bottom)
{
    EMRINTERSECTCLIPRECT emr;
    emr.emr.iType      = EMR_INTERSECTCLIPRECT;
    emr.emr.nSize      = sizeof(emr);
    emr.rclClip.left   = left;
    emr.rclClip.top    = top;
    emr.rclClip.right  = right;
    emr.rclClip.bottom = bottom;
    return EMFDRV_WriteRecord( dev, &emr.emr );
}

INT EMFDRV_OffsetClipRgn( PHYSDEV dev, INT x, INT y )
{
    EMROFFSETCLIPRGN emr;
    emr.emr.iType   = EMR_OFFSETCLIPRGN;
    emr.emr.nSize   = sizeof(emr);
    emr.ptlOffset.x = x;
    emr.ptlOffset.y = y;
    return EMFDRV_WriteRecord( dev, &emr.emr );
}

INT EMFDRV_ExtSelectClipRgn( PHYSDEV dev, HRGN hrgn, INT mode )
{
    EMREXTSELECTCLIPRGN *emr;
    DWORD size, rgnsize;
    BOOL ret;

    if (!hrgn)
    {
        if (mode != RGN_COPY) return ERROR;
        rgnsize = 0;
    }
    else rgnsize = GetRegionData( hrgn, 0, NULL );

    size = rgnsize + offsetof(EMREXTSELECTCLIPRGN,RgnData);
    emr = HeapAlloc( GetProcessHeap(), 0, size );
    if (rgnsize) GetRegionData( hrgn, rgnsize, (RGNDATA *)&emr->RgnData );

    emr->emr.iType = EMR_EXTSELECTCLIPRGN;
    emr->emr.nSize = size;
    emr->cbRgnData = rgnsize;
    emr->iMode     = mode;

    ret = EMFDRV_WriteRecord( dev, &emr->emr );
    HeapFree( GetProcessHeap(), 0, emr );
    return ret ? SIMPLEREGION : ERROR;
}

DWORD EMFDRV_SetMapperFlags( PHYSDEV dev, DWORD flags )
{
    EMRSETMAPPERFLAGS emr;

    emr.emr.iType = EMR_SETMAPPERFLAGS;
    emr.emr.nSize = sizeof(emr);
    emr.dwFlags   = flags;

    return EMFDRV_WriteRecord( dev, &emr.emr );
}

BOOL EMFDRV_AbortPath( PHYSDEV dev )
{
    EMRABORTPATH emr;

    emr.emr.iType = EMR_ABORTPATH;
    emr.emr.nSize = sizeof(emr);

    return EMFDRV_WriteRecord( dev, &emr.emr );
}

BOOL EMFDRV_BeginPath( PHYSDEV dev )
{
    EMRBEGINPATH emr;

    emr.emr.iType = EMR_BEGINPATH;
    emr.emr.nSize = sizeof(emr);

    return EMFDRV_WriteRecord( dev, &emr.emr );
}

BOOL EMFDRV_CloseFigure( PHYSDEV dev )
{
    EMRCLOSEFIGURE emr;

    emr.emr.iType = EMR_CLOSEFIGURE;
    emr.emr.nSize = sizeof(emr);

    return EMFDRV_WriteRecord( dev, &emr.emr );
}

BOOL EMFDRV_EndPath( PHYSDEV dev )
{
    EMRENDPATH emr;

    emr.emr.iType = EMR_ENDPATH;
    emr.emr.nSize = sizeof(emr);

    return EMFDRV_WriteRecord( dev, &emr.emr );
}

BOOL EMFDRV_FillPath( PHYSDEV dev )
{
    EMRFILLPATH emr;

    emr.emr.iType = EMR_FILLPATH;
    emr.emr.nSize = sizeof(emr);
    FIXME("Bounds\n");
    emr.rclBounds.left = 0;
    emr.rclBounds.top = 0;
    emr.rclBounds.right = 0;
    emr.rclBounds.bottom = 0;
    return EMFDRV_WriteRecord( dev, &emr.emr );
}

BOOL EMFDRV_FlattenPath( PHYSDEV dev )
{
    EMRFLATTENPATH emr;

    emr.emr.iType = EMR_FLATTENPATH;
    emr.emr.nSize = sizeof(emr);

    return EMFDRV_WriteRecord( dev, &emr.emr );
}

BOOL EMFDRV_SelectClipPath( PHYSDEV dev, INT iMode )
{
    EMRSELECTCLIPPATH emr;

    emr.emr.iType = EMR_SELECTCLIPPATH;
    emr.emr.nSize = sizeof(emr);
    emr.iMode = iMode;

    return EMFDRV_WriteRecord( dev, &emr.emr );
}

BOOL EMFDRV_StrokeAndFillPath( PHYSDEV dev )
{
    EMRSTROKEANDFILLPATH emr;

    emr.emr.iType = EMR_STROKEANDFILLPATH;
    emr.emr.nSize = sizeof(emr);
    FIXME("Bounds\n");
    emr.rclBounds.left = 0;
    emr.rclBounds.top = 0;
    emr.rclBounds.right = 0;
    emr.rclBounds.bottom = 0;
    return EMFDRV_WriteRecord( dev, &emr.emr );
}

BOOL EMFDRV_StrokePath( PHYSDEV dev )
{
    EMRSTROKEPATH emr;

    emr.emr.iType = EMR_STROKEPATH;
    emr.emr.nSize = sizeof(emr);
    FIXME("Bounds\n");
    emr.rclBounds.left = 0;
    emr.rclBounds.top = 0;
    emr.rclBounds.right = 0;
    emr.rclBounds.bottom = 0;
    return EMFDRV_WriteRecord( dev, &emr.emr );
}

BOOL EMFDRV_WidenPath( PHYSDEV dev )
{
    EMRWIDENPATH emr;

    emr.emr.iType = EMR_WIDENPATH;
    emr.emr.nSize = sizeof(emr);

    return EMFDRV_WriteRecord( dev, &emr.emr );
}

INT EMFDRV_GetDeviceCaps(PHYSDEV dev, INT cap)
{
    EMFDRV_PDEVICE *physDev = (EMFDRV_PDEVICE*) dev;

    switch(cap) {

    case HORZRES:
        return physDev->horzres;
    case VERTRES:
        return physDev->vertres;
    case LOGPIXELSX:
        return physDev->logpixelsx;
    case LOGPIXELSY:
        return physDev->logpixelsy;
    case HORZSIZE:
        return physDev->horzsize;
    case VERTSIZE:
        return physDev->vertsize;
    case BITSPIXEL:
        return physDev->bitspixel;
    case TEXTCAPS:
        return physDev->textcaps;
    case RASTERCAPS:
        return physDev->rastercaps;
    case TECHNOLOGY:
        return physDev->technology;
    case PLANES:
        return physDev->planes;

    default:
        FIXME("Unimplemented cap %d\n", cap);
	return 0;

    }
}
