/*
 * Enhanced MetaFile driver initialisation functions
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

#include <stdarg.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "gdi.h"
#include "gdi_private.h"
#include "enhmfdrv/enhmetafiledrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(enhmetafile);

static const DC_FUNCTIONS EMFDRV_Funcs =
{
    NULL,                            /* pAbortDoc */
    EMFDRV_AbortPath,                /* pAbortPath */
    NULL,                            /* pAngleArc */
    EMFDRV_Arc,                      /* pArc */
    NULL,                            /* pArcTo */
    EMFDRV_BeginPath,                /* pBeginPath */
    EMFDRV_BitBlt,                   /* pBitBlt */
    NULL,                            /* pChoosePixelFormat */
    EMFDRV_Chord,                    /* pChord */
    EMFDRV_CloseFigure,              /* pCloseFigure */
    NULL,                            /* pCreateBitmap */
    NULL,                            /* pCreateDC */
    NULL,                            /* pCreateDIBSection */
    NULL,                            /* pDeleteBitmap */
    NULL,                            /* pDeleteDC */
    EMFDRV_DeleteObject,             /* pDeleteObject */
    NULL,                            /* pDescribePixelFormat */
    NULL,                            /* pDeviceCapabilities */
    EMFDRV_Ellipse,                  /* pEllipse */
    NULL,                            /* pEndDoc */
    NULL,                            /* pEndPage */
    EMFDRV_EndPath,                  /* pEndPath */
    NULL,                            /* pEnumDeviceFonts */
    EMFDRV_ExcludeClipRect,          /* pExcludeClipRect */
    NULL,                            /* pExtDeviceMode */
    NULL,                            /* pExtEscape */
    EMFDRV_ExtFloodFill,             /* pExtFloodFill */
    EMFDRV_ExtSelectClipRgn,         /* pExtSelectClipRgn */
    EMFDRV_ExtTextOut,               /* pExtTextOut */
    EMFDRV_FillPath,                 /* pFillPath */
    EMFDRV_FillRgn,                  /* pFillRgn */
    EMFDRV_FlattenPath,              /* pFlattenPath */
    EMFDRV_FrameRgn,                 /* pFrameRgn */
    EMFDRV_GdiComment,               /* pGdiComment */
    NULL,                            /* pGetBitmapBits */
    NULL,                            /* pGetCharWidth */
    NULL,                            /* pGetDCOrgEx */
    NULL,                            /* pGetDIBColorTable */
    NULL,                            /* pGetDIBits */
    EMFDRV_GetDeviceCaps,            /* pGetDeviceCaps */
    NULL,                            /* pGetDeviceGammaRamp */
    NULL,                            /* pGetNearestColor */
    NULL,                            /* pGetPixel */
    NULL,                            /* pGetPixelFormat */
    NULL,                            /* pGetSystemPaletteEntries */
    NULL,                            /* pGetTextExtentPoint */
    NULL,                            /* pGetTextMetrics */
    EMFDRV_IntersectClipRect,        /* pIntersectClipRect */
    EMFDRV_InvertRgn,                /* pInvertRgn */
    EMFDRV_LineTo,                   /* pLineTo */
    EMFDRV_ModifyWorldTransform,     /* pModifyWorldTransform */
    EMFDRV_MoveTo,                   /* pMoveTo */
    EMFDRV_OffsetClipRgn,            /* pOffsetClipRgn */
    EMFDRV_OffsetViewportOrg,        /* pOffsetViewportOrg */
    EMFDRV_OffsetWindowOrg,          /* pOffsetWindowOrg */
    EMFDRV_PaintRgn,                 /* pPaintRgn */
    EMFDRV_PatBlt,                   /* pPatBlt */
    EMFDRV_Pie,                      /* pPie */
    NULL,                            /* pPolyBezier */
    NULL,                            /* pPolyBezierTo */
    NULL,                            /* pPolyDraw */
    EMFDRV_PolyPolygon,              /* pPolyPolygon */
    EMFDRV_PolyPolyline,             /* pPolyPolyline */
    EMFDRV_Polygon,                  /* pPolygon */
    EMFDRV_Polyline,                 /* pPolyline */
    NULL,                            /* pPolylineTo */
    NULL,                            /* pRealizeDefaultPalette */
    NULL,                            /* pRealizePalette */
    EMFDRV_Rectangle,                /* pRectangle */
    NULL,                            /* pResetDC */
    EMFDRV_RestoreDC,                /* pRestoreDC */
    EMFDRV_RoundRect,                /* pRoundRect */
    EMFDRV_SaveDC,                   /* pSaveDC */
    EMFDRV_ScaleViewportExt,         /* pScaleViewportExt */
    EMFDRV_ScaleWindowExt,           /* pScaleWindowExt */
    EMFDRV_SelectBitmap,             /* pSelectBitmap */
    EMFDRV_SelectBrush,              /* pSelectBrush */
    EMFDRV_SelectClipPath,           /* pSelectClipPath */
    EMFDRV_SelectFont,               /* pSelectFont */
    NULL,                            /* pSelectPalette */
    EMFDRV_SelectPen,                /* pSelectPen */
    EMFDRV_SetArcDirection,          /* pSetArcDirection */
    NULL,                            /* pSetBitmapBits */
    EMFDRV_SetBkColor,               /* pSetBkColor */
    EMFDRV_SetBkMode,                /* pSetBkMode */
    NULL,                            /* pSetDCBrushColor */
    NULL,                            /* pSetDCOrg */
    NULL,                            /* pSetDCPenColor */
    NULL,                            /* pSetDIBColorTable */
    NULL,                            /* pSetDIBits */
    EMFDRV_SetDIBitsToDevice,        /* pSetDIBitsToDevice */
    NULL,                            /* pSetDeviceClipping */
    NULL,                            /* pSetDeviceGammaRamp */
    EMFDRV_SetMapMode,               /* pSetMapMode */
    EMFDRV_SetMapperFlags,           /* pSetMapperFlags */
    NULL,                            /* pSetPixel */
    NULL,                            /* pSetPixelFormat */
    EMFDRV_SetPolyFillMode,          /* pSetPolyFillMode */
    EMFDRV_SetROP2,                  /* pSetROP2 */
    NULL,                            /* pSetRelAbs */
    EMFDRV_SetStretchBltMode,        /* pSetStretchBltMode */
    EMFDRV_SetTextAlign,             /* pSetTextAlign */
    NULL,                            /* pSetTextCharacterExtra */
    EMFDRV_SetTextColor,             /* pSetTextColor */
    EMFDRV_SetTextJustification,     /* pSetTextJustification */ 
    EMFDRV_SetViewportExt,           /* pSetViewportExt */
    EMFDRV_SetViewportOrg,           /* pSetViewportOrg */
    EMFDRV_SetWindowExt,             /* pSetWindowExt */
    EMFDRV_SetWindowOrg,             /* pSetWindowOrg */
    EMFDRV_SetWorldTransform,        /* pSetWorldTransform */
    NULL,                            /* pStartDoc */
    NULL,                            /* pStartPage */
    EMFDRV_StretchBlt,               /* pStretchBlt */
    EMFDRV_StretchDIBits,            /* pStretchDIBits */
    EMFDRV_StrokeAndFillPath,        /* pStrokeAndFillPath */
    EMFDRV_StrokePath,               /* pStrokePath */
    NULL,                            /* pSwapBuffers */
    EMFDRV_WidenPath                 /* pWidenPath */
};


/**********************************************************************
 *	     EMFDRV_DeleteDC
 */
static BOOL EMFDRV_DeleteDC( PHYSDEV dev )
{
    EMFDRV_PDEVICE *physDev = (EMFDRV_PDEVICE *)dev;
    DC *dc = physDev->dc;
    UINT index;

    if (physDev->emh) HeapFree( GetProcessHeap(), 0, physDev->emh );
    for(index = 0; index < physDev->handles_size; index++)
        if(physDev->handles[index])
	    GDI_hdc_not_using_object(physDev->handles[index], physDev->hdc);
    HeapFree( GetProcessHeap(), 0, physDev->handles );
    HeapFree( GetProcessHeap(), 0, physDev );
    dc->physDev = NULL;
    GDI_FreeObject( dc->hSelf, dc );
    return TRUE;
}


/******************************************************************
 *         EMFDRV_WriteRecord
 *
 * Warning: this function can change the pointer to the metafile header.
 */
BOOL EMFDRV_WriteRecord( PHYSDEV dev, EMR *emr )
{
    DWORD len;
    ENHMETAHEADER *emh;
    EMFDRV_PDEVICE *physDev = (EMFDRV_PDEVICE *)dev;

    physDev->emh->nBytes += emr->nSize;
    physDev->emh->nRecords++;

    if(physDev->hFile) {
        TRACE("Writing record to disk\n");
	if (!WriteFile(physDev->hFile, (char *)emr, emr->nSize, NULL, NULL))
	    return FALSE;
    } else {
        DWORD nEmfSize = HeapSize(GetProcessHeap(), 0, physDev->emh);
        len = physDev->emh->nBytes;
        if (len > nEmfSize) {
            nEmfSize += (nEmfSize / 2) + emr->nSize;
            emh = HeapReAlloc(GetProcessHeap(), 0, physDev->emh, nEmfSize);
            if (!emh) return FALSE;
            physDev->emh = emh;
        }
        memcpy((CHAR *)physDev->emh + physDev->emh->nBytes - emr->nSize, emr,
               emr->nSize);
    }
    return TRUE;
}


/******************************************************************
 *         EMFDRV_UpdateBBox
 */
void EMFDRV_UpdateBBox( PHYSDEV dev, RECTL *rect )
{
    EMFDRV_PDEVICE *physDev = (EMFDRV_PDEVICE *)dev;
    RECTL *bounds = &physDev->emh->rclBounds;
    RECTL vportRect = *rect;

    LPtoDP(physDev->hdc, (LPPOINT)&vportRect, 2);

    if(bounds->left > bounds->right) {/* first rect */
	*bounds = vportRect;
	return;
    }
    bounds->left   = min(bounds->left,   vportRect.left);
    bounds->top    = min(bounds->top,    vportRect.top);
    bounds->right  = max(bounds->right,  vportRect.right);
    bounds->bottom = max(bounds->bottom, vportRect.bottom);
    return;
}

/**********************************************************************
 *          CreateEnhMetaFileA   (GDI32.@)
 */
HDC WINAPI CreateEnhMetaFileA(
    HDC hdc,           /* [in] optional reference DC */
    LPCSTR filename,   /* [in] optional filename for disk metafiles */
    const RECT *rect,  /* [in] optional bounding rectangle */
    LPCSTR description /* [in] optional description */
    )
{
    LPWSTR filenameW = NULL;
    LPWSTR descriptionW = NULL;
    HDC hReturnDC;
    DWORD len1, len2, total;

    if(filename)
    {
        total = MultiByteToWideChar( CP_ACP, 0, filename, -1, NULL, 0 );
        filenameW = HeapAlloc( GetProcessHeap(), 0, total * sizeof(WCHAR) );
        MultiByteToWideChar( CP_ACP, 0, filename, -1, filenameW, total );
    }
    if(description) {
        len1 = strlen(description);
	len2 = strlen(description + len1 + 1);
        total = MultiByteToWideChar( CP_ACP, 0, description, len1 + len2 + 3, NULL, 0 );
	descriptionW = HeapAlloc( GetProcessHeap(), 0, total * sizeof(WCHAR) );
        MultiByteToWideChar( CP_ACP, 0, description, len1 + len2 + 3, descriptionW, total );
    }

    hReturnDC = CreateEnhMetaFileW(hdc, filenameW, rect, descriptionW);

    if(filenameW)
        HeapFree( GetProcessHeap(), 0, filenameW );
    if(descriptionW)
        HeapFree( GetProcessHeap(), 0, descriptionW );

    return hReturnDC;
}

/**********************************************************************
 *          CreateEnhMetaFileW   (GDI32.@)
 */
HDC WINAPI CreateEnhMetaFileW(
    HDC           hdc,        /* [in] optional reference DC */
    LPCWSTR       filename,   /* [in] optional filename for disk metafiles */
    const RECT*   rect,       /* [in] optional bounding rectangle */
    LPCWSTR       description /* [in] optional description */
    )
{
    static const WCHAR displayW[] = {'D','I','S','P','L','A','Y',0};
    HDC ret;
    DC *dc;
    HDC hRefDC = hdc ? hdc : CreateDCW(displayW,NULL,NULL,NULL); 
        /* If no ref, use current display */
    EMFDRV_PDEVICE *physDev;
    HANDLE hFile;
    DWORD size = 0, length = 0;

    TRACE("%s\n", debugstr_w(filename) );

    if (!(dc = DC_AllocDC( &EMFDRV_Funcs, ENHMETAFILE_DC_MAGIC ))) return 0;

    physDev = (EMFDRV_PDEVICE *)HeapAlloc(GetProcessHeap(),0,sizeof(*physDev));
    if (!physDev) {
        GDI_FreeObject( dc->hSelf, dc );
        return 0;
    }
    dc->physDev = (PHYSDEV)physDev;
    physDev->hdc = dc->hSelf;
    physDev->dc = dc;

    if(description) { /* App name\0Title\0\0 */
        length = lstrlenW(description);
	length += lstrlenW(description + length + 1);
	length += 3;
	length *= 2;
    }
    size = sizeof(ENHMETAHEADER) + (length + 3) / 4 * 4;

    if (!(physDev->emh = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, size))) {
        HeapFree( GetProcessHeap(), 0, physDev );
        GDI_FreeObject( dc->hSelf, dc );
        return 0;
    }

    physDev->handles = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, HANDLE_LIST_INC * sizeof(physDev->handles[0]));
    physDev->handles_size = HANDLE_LIST_INC;
    physDev->cur_handles = 1;
    physDev->hFile = 0;

    physDev->horzres = GetDeviceCaps(hRefDC, HORZRES);
    physDev->vertres = GetDeviceCaps(hRefDC, VERTRES);
    physDev->logpixelsx = GetDeviceCaps(hRefDC, LOGPIXELSX);
    physDev->logpixelsy = GetDeviceCaps(hRefDC, LOGPIXELSY);
    physDev->horzsize = GetDeviceCaps(hRefDC, HORZSIZE);
    physDev->vertsize = GetDeviceCaps(hRefDC, VERTSIZE);
    physDev->bitspixel = GetDeviceCaps(hRefDC, BITSPIXEL);
    physDev->textcaps = GetDeviceCaps(hRefDC, TEXTCAPS);
    physDev->rastercaps = GetDeviceCaps(hRefDC, RASTERCAPS);
    physDev->technology = GetDeviceCaps(hRefDC, TECHNOLOGY);
    physDev->planes = GetDeviceCaps(hRefDC, PLANES);

    physDev->emh->iType = EMR_HEADER;
    physDev->emh->nSize = size;

    physDev->emh->rclBounds.left = physDev->emh->rclBounds.top = 0;
    physDev->emh->rclBounds.right = physDev->emh->rclBounds.bottom = -1;

    if(rect) {
        physDev->emh->rclFrame.left   = rect->left;
	physDev->emh->rclFrame.top    = rect->top;
	physDev->emh->rclFrame.right  = rect->right;
	physDev->emh->rclFrame.bottom = rect->bottom;
    } else {  /* Set this to {0,0 - -1,-1} and update it at the end */
        physDev->emh->rclFrame.left = physDev->emh->rclFrame.top = 0;
	physDev->emh->rclFrame.right = physDev->emh->rclFrame.bottom = -1;
    }

    physDev->emh->dSignature = ENHMETA_SIGNATURE;
    physDev->emh->nVersion = 0x10000;
    physDev->emh->nBytes = physDev->emh->nSize;
    physDev->emh->nRecords = 1;
    physDev->emh->nHandles = 1;

    physDev->emh->sReserved = 0; /* According to docs, this is reserved and must be 0 */
    physDev->emh->nDescription = length / 2;

    physDev->emh->offDescription = length ? sizeof(ENHMETAHEADER) : 0;

    physDev->emh->nPalEntries = 0; /* I guess this should start at 0 */

    /* Size in pixels */
    physDev->emh->szlDevice.cx = physDev->horzres;
    physDev->emh->szlDevice.cy = physDev->vertres;

    /* Size in millimeters */
    physDev->emh->szlMillimeters.cx = physDev->horzsize;
    physDev->emh->szlMillimeters.cy = physDev->vertsize;

    /* Size in micrometers */
    physDev->emh->szlMicrometers.cx = physDev->horzsize * 1000;
    physDev->emh->szlMicrometers.cy = physDev->vertsize * 1000;

    memcpy((char *)physDev->emh + sizeof(ENHMETAHEADER), description, length);

    if (filename)  /* disk based metafile */
    {
        if ((hFile = CreateFileW(filename, GENERIC_WRITE | GENERIC_READ, 0,
				 NULL, CREATE_ALWAYS, 0, 0)) == INVALID_HANDLE_VALUE) {
            EMFDRV_DeleteDC( dc->physDev );
            return 0;
        }
        if (!WriteFile( hFile, (LPSTR)physDev->emh, size, NULL, NULL )) {
            EMFDRV_DeleteDC( dc->physDev );
            return 0;
	}
	physDev->hFile = hFile;
    }

    TRACE("returning %p\n", dc->hSelf);
    ret = dc->hSelf;
    GDI_ReleaseObj( dc->hSelf );

    if( !hdc )
      DeleteDC( hRefDC );

    return ret;
}

/******************************************************************
 *             CloseEnhMetaFile (GDI32.@)
 */
HENHMETAFILE WINAPI CloseEnhMetaFile(HDC hdc) /* [in] metafile DC */
{
    HENHMETAFILE hmf;
    EMFDRV_PDEVICE *physDev;
    DC *dc;
    EMREOF emr;
    HANDLE hMapping = 0;

    TRACE("(%p)\n", hdc );

    if (!(dc = (DC *) GDI_GetObjPtr( hdc, ENHMETAFILE_DC_MAGIC ))) return 0;
    physDev = (EMFDRV_PDEVICE *)dc->physDev;

    emr.emr.iType = EMR_EOF;
    emr.emr.nSize = sizeof(emr);
    emr.nPalEntries = 0;
    emr.offPalEntries = 0;
    emr.nSizeLast = emr.emr.nSize;
    EMFDRV_WriteRecord( dc->physDev, &emr.emr );

    /* Update rclFrame if not initialized in CreateEnhMetaFile */
    if(physDev->emh->rclFrame.left > physDev->emh->rclFrame.right) {
        physDev->emh->rclFrame.left = physDev->emh->rclBounds.left *
	  physDev->emh->szlMillimeters.cx * 100 / physDev->emh->szlDevice.cx;
        physDev->emh->rclFrame.top = physDev->emh->rclBounds.top *
	  physDev->emh->szlMillimeters.cy * 100 / physDev->emh->szlDevice.cy;
        physDev->emh->rclFrame.right = physDev->emh->rclBounds.right *
	  physDev->emh->szlMillimeters.cx * 100 / physDev->emh->szlDevice.cx;
        physDev->emh->rclFrame.bottom = physDev->emh->rclBounds.bottom *
	  physDev->emh->szlMillimeters.cy * 100 / physDev->emh->szlDevice.cy;
    }

    if (physDev->hFile)  /* disk based metafile */
    {
        if (SetFilePointer(physDev->hFile, 0, NULL, FILE_BEGIN) != 0)
        {
            CloseHandle( physDev->hFile );
            EMFDRV_DeleteDC( dc->physDev );
            return 0;
        }

        if (!WriteFile(physDev->hFile, (LPSTR)physDev->emh,
                       sizeof(*physDev->emh), NULL, NULL))
        {
            CloseHandle( physDev->hFile );
            EMFDRV_DeleteDC( dc->physDev );
            return 0;
        }
	HeapFree( GetProcessHeap(), 0, physDev->emh );
        hMapping = CreateFileMappingA(physDev->hFile, NULL, PAGE_READONLY, 0,
				      0, NULL);
	TRACE("hMapping = %p\n", hMapping );
	physDev->emh = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
	TRACE("view = %p\n", physDev->emh );
        CloseHandle( hMapping );
        CloseHandle( physDev->hFile );
    }

    hmf = EMF_Create_HENHMETAFILE( physDev->emh, (physDev->hFile != 0) );
    physDev->emh = NULL;  /* So it won't be deleted */
    EMFDRV_DeleteDC( dc->physDev );
    return hmf;
}
