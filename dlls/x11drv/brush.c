/*
 * X11DRV brush objects
 *
 * Copyright 1993, 1994  Alexandre Julliard
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

#include "config.h"

#include <stdlib.h>

#include "gdi.h"
#include "x11drv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(gdi);

#define NB_HATCH_STYLES  (HS_DIAGCROSS+1)

static const char HatchBrushes[NB_HATCH_STYLES + 1][8] =
{
    { 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00 }, /* HS_HORIZONTAL */
    { 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08 }, /* HS_VERTICAL   */
    { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 }, /* HS_FDIAGONAL  */
    { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 }, /* HS_BDIAGONAL  */
    { 0x08, 0x08, 0x08, 0xff, 0x08, 0x08, 0x08, 0x08 }, /* HS_CROSS      */
    { 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81 }, /* HS_DIAGCROSS  */
    { 0xee, 0xbb, 0xee, 0xbb, 0xee, 0xbb, 0xee, 0xbb }  /* Hack for DKGRAY */
};

  /* Levels of each primary for dithering */
#define PRIMARY_LEVELS  3
#define TOTAL_LEVELS    (PRIMARY_LEVELS*PRIMARY_LEVELS*PRIMARY_LEVELS)

 /* Dithering matrix size  */
#define MATRIX_SIZE     8
#define MATRIX_SIZE_2   (MATRIX_SIZE*MATRIX_SIZE)

  /* Total number of possible levels for a dithered primary color */
#define DITHER_LEVELS   (MATRIX_SIZE_2 * (PRIMARY_LEVELS-1) + 1)

  /* Dithering matrix */
static const int dither_matrix[MATRIX_SIZE_2] =
{
     0, 32,  8, 40,  2, 34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,
    12, 44,  4, 36, 14, 46,  6, 38,
    60, 28, 52, 20, 62, 30, 54, 22,
     3, 35, 11, 43,  1, 33,  9, 41,
    51, 19, 59, 27, 49, 17, 57, 25,
    15, 47,  7, 39, 13, 45,  5, 37,
    63, 31, 55, 23, 61, 29, 53, 21
};

  /* Mapping between (R,G,B) triples and EGA colors */
static const int EGAmapping[TOTAL_LEVELS] =
{
    0,  /* 000000 -> 000000 */
    4,  /* 00007f -> 000080 */
    12, /* 0000ff -> 0000ff */
    2,  /* 007f00 -> 008000 */
    6,  /* 007f7f -> 008080 */
    6,  /* 007fff -> 008080 */
    10, /* 00ff00 -> 00ff00 */
    6,  /* 00ff7f -> 008080 */
    14, /* 00ffff -> 00ffff */
    1,  /* 7f0000 -> 800000 */
    5,  /* 7f007f -> 800080 */
    5,  /* 7f00ff -> 800080 */
    3,  /* 7f7f00 -> 808000 */
    8,  /* 7f7f7f -> 808080 */
    7,  /* 7f7fff -> c0c0c0 */
    3,  /* 7fff00 -> 808000 */
    7,  /* 7fff7f -> c0c0c0 */
    7,  /* 7fffff -> c0c0c0 */
    9,  /* ff0000 -> ff0000 */
    5,  /* ff007f -> 800080 */
    13, /* ff00ff -> ff00ff */
    3,  /* ff7f00 -> 808000 */
    7,  /* ff7f7f -> c0c0c0 */
    7,  /* ff7fff -> c0c0c0 */
    11, /* ffff00 -> ffff00 */
    7,  /* ffff7f -> c0c0c0 */
    15  /* ffffff -> ffffff */
};

#define PIXEL_VALUE(r,g,b) \
    X11DRV_PALETTE_mapEGAPixel[EGAmapping[((r)*PRIMARY_LEVELS+(g))*PRIMARY_LEVELS+(b)]]

  /* X image for building dithered pixmap */
static XImage *ditherImage = NULL;


/***********************************************************************
 *           BRUSH_DitherColor
 */
static Pixmap BRUSH_DitherColor( COLORREF color )
{
    static COLORREF prevColor = 0xffffffff;
    unsigned int x, y;
    Pixmap pixmap;

    if (!ditherImage)
    {
        ditherImage = X11DRV_DIB_CreateXImage( MATRIX_SIZE, MATRIX_SIZE, screen_depth );
        if (!ditherImage) return 0;
    }

    wine_tsx11_lock();
    if (color != prevColor)
    {
	int r = GetRValue( color ) * DITHER_LEVELS;
	int g = GetGValue( color ) * DITHER_LEVELS;
	int b = GetBValue( color ) * DITHER_LEVELS;
	const int *pmatrix = dither_matrix;

	for (y = 0; y < MATRIX_SIZE; y++)
	{
	    for (x = 0; x < MATRIX_SIZE; x++)
	    {
		int d  = *pmatrix++ * 256;
		int dr = ((r + d) / MATRIX_SIZE_2) / 256;
		int dg = ((g + d) / MATRIX_SIZE_2) / 256;
		int db = ((b + d) / MATRIX_SIZE_2) / 256;
		XPutPixel( ditherImage, x, y, PIXEL_VALUE(dr,dg,db) );
	    }
	}
	prevColor = color;
    }

    pixmap = XCreatePixmap( gdi_display, root_window, MATRIX_SIZE, MATRIX_SIZE, screen_depth );
    XPutImage( gdi_display, pixmap, BITMAP_colorGC, ditherImage, 0, 0,
	       0, 0, MATRIX_SIZE, MATRIX_SIZE );
    wine_tsx11_unlock();
    return pixmap;
}


/***********************************************************************
 *           BRUSH_SelectSolidBrush
 */
static void BRUSH_SelectSolidBrush( X11DRV_PDEVICE *physDev, COLORREF color )
{
    if ((physDev->depth > 1) && (screen_depth <= 8) && !X11DRV_IsSolidColor( color ))
    {
	  /* Dithered brush */
	physDev->brush.pixmap = BRUSH_DitherColor( color );
	physDev->brush.fillStyle = FillTiled;
	physDev->brush.pixel = 0;
    }
    else
    {
	  /* Solid brush */
	physDev->brush.pixel = X11DRV_PALETTE_ToPhysical( physDev, color );
	physDev->brush.fillStyle = FillSolid;
    }
}


/***********************************************************************
 *           BRUSH_SelectPatternBrush
 */
static BOOL BRUSH_SelectPatternBrush( X11DRV_PDEVICE *physDev, HBITMAP hbitmap )
{
    BOOL ret = FALSE;
    BITMAPOBJ * bmp = (BITMAPOBJ *) GDI_GetObjPtr( hbitmap, BITMAP_MAGIC );
    if (!bmp) return FALSE;

    if(!bmp->physBitmap) goto done;

    wine_tsx11_lock();
    if ((physDev->depth == 1) && (bmp->bitmap.bmBitsPixel != 1))
    {
        /* Special case: a color pattern on a monochrome DC */
        physDev->brush.pixmap = XCreatePixmap( gdi_display, root_window, 8, 8, 1);
        /* FIXME: should probably convert to monochrome instead */
        XCopyPlane( gdi_display, (Pixmap)bmp->physBitmap, physDev->brush.pixmap,
                    BITMAP_monoGC, 0, 0, 8, 8, 0, 0, 1 );
    }
    else
    {
        physDev->brush.pixmap = XCreatePixmap( gdi_display, root_window,
                                               8, 8, bmp->bitmap.bmBitsPixel );
        XCopyArea( gdi_display, (Pixmap)bmp->physBitmap, physDev->brush.pixmap,
                   BITMAP_GC(bmp), 0, 0, 8, 8, 0, 0 );
    }
    wine_tsx11_unlock();

    if (bmp->bitmap.bmBitsPixel > 1)
    {
	physDev->brush.fillStyle = FillTiled;
	physDev->brush.pixel = 0;  /* Ignored */
    }
    else
    {
	physDev->brush.fillStyle = FillOpaqueStippled;
	physDev->brush.pixel = -1;  /* Special case (see DC_SetupGCForBrush) */
    }
    ret = TRUE;
 done:
    GDI_ReleaseObj( hbitmap );
    return ret;
}


/***********************************************************************
 *           SelectBrush   (X11DRV.@)
 */
HBRUSH X11DRV_SelectBrush( X11DRV_PDEVICE *physDev, HBRUSH hbrush )
{
    LOGBRUSH logbrush;
    HBITMAP hBitmap;
    BITMAPINFO * bmpInfo;

    if (!GetObjectA( hbrush, sizeof(logbrush), &logbrush )) return 0;

    TRACE("hdc=%p hbrush=%p\n", physDev->hdc,hbrush);

    if (physDev->brush.pixmap)
    {
        wine_tsx11_lock();
        XFreePixmap( gdi_display, physDev->brush.pixmap );
        wine_tsx11_unlock();
        physDev->brush.pixmap = 0;
    }
    physDev->brush.style = logbrush.lbStyle;
    if (hbrush == GetStockObject( DC_BRUSH ))
        logbrush.lbColor = GetDCBrushColor( physDev->hdc );

    switch(logbrush.lbStyle)
    {
      case BS_NULL:
	TRACE("BS_NULL\n" );
	break;

      case BS_SOLID:
        TRACE("BS_SOLID\n" );
	BRUSH_SelectSolidBrush( physDev, logbrush.lbColor );
	break;

      case BS_HATCHED:
	TRACE("BS_HATCHED\n" );
	physDev->brush.pixel = X11DRV_PALETTE_ToPhysical( physDev, logbrush.lbColor );
        wine_tsx11_lock();
        physDev->brush.pixmap = XCreateBitmapFromData( gdi_display, root_window,
                                                       HatchBrushes[logbrush.lbHatch], 8, 8 );
        wine_tsx11_unlock();
	physDev->brush.fillStyle = FillStippled;
	break;

      case BS_PATTERN:
	TRACE("BS_PATTERN\n");
	if (!BRUSH_SelectPatternBrush( physDev, (HBITMAP)logbrush.lbHatch )) return 0;
	break;

      case BS_DIBPATTERN:
	TRACE("BS_DIBPATTERN\n");
	if ((bmpInfo = (BITMAPINFO *) GlobalLock16( (HGLOBAL16)logbrush.lbHatch )))
	{
	    int size = X11DRV_DIB_BitmapInfoSize( bmpInfo, logbrush.lbColor );
	    hBitmap = CreateDIBitmap( physDev->hdc, &bmpInfo->bmiHeader,
                                        CBM_INIT, ((char *)bmpInfo) + size,
                                        bmpInfo,
                                        (WORD)logbrush.lbColor );
	    BRUSH_SelectPatternBrush( physDev, hBitmap );
	    DeleteObject( hBitmap );
	    GlobalUnlock16( (HGLOBAL16)logbrush.lbHatch );
	}

	break;
    }
    return hbrush;
}


/***********************************************************************
 *           SetDCBrushColor (X11DRV.@)
 */
COLORREF X11DRV_SetDCBrushColor( X11DRV_PDEVICE *physDev, COLORREF crColor )
{
    if (GetCurrentObject(physDev->hdc, OBJ_BRUSH) == GetStockObject( DC_BRUSH ))
        BRUSH_SelectSolidBrush( physDev, crColor );

    return crColor;
}
