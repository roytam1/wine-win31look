/*
 * X11 graphics driver graphics functions
 *
 * Copyright 1993,1994 Alexandre Julliard
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

/*
 * FIXME: only some of these functions obey the GM_ADVANCED
 * graphics mode
 */

#include "config.h"

#include <math.h>
#ifdef HAVE_FLOAT_H
# include <float.h>
#endif
#include <stdlib.h>
#ifndef PI
#define PI M_PI
#endif
#include <string.h>
#include <X11/Intrinsic.h>

#include "x11drv.h"
#include "x11font.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(graphics);

#define ABS(x)    ((x)<0?(-(x)):(x))

  /* ROP code to GC function conversion */
const int X11DRV_XROPfunction[16] =
{
    GXclear,        /* R2_BLACK */
    GXnor,          /* R2_NOTMERGEPEN */
    GXandInverted,  /* R2_MASKNOTPEN */
    GXcopyInverted, /* R2_NOTCOPYPEN */
    GXandReverse,   /* R2_MASKPENNOT */
    GXinvert,       /* R2_NOT */
    GXxor,          /* R2_XORPEN */
    GXnand,         /* R2_NOTMASKPEN */
    GXand,          /* R2_MASKPEN */
    GXequiv,        /* R2_NOTXORPEN */
    GXnoop,         /* R2_NOP */
    GXorInverted,   /* R2_MERGENOTPEN */
    GXcopy,         /* R2_COPYPEN */
    GXorReverse,    /* R2_MERGEPENNOT */
    GXor,           /* R2_MERGEPEN */
    GXset           /* R2_WHITE */
};


/***********************************************************************
 *           X11DRV_SetupGCForPatBlt
 *
 * Setup the GC for a PatBlt operation using current brush.
 * If fMapColors is TRUE, X pixels are mapped to Windows colors.
 * Return FALSE if brush is BS_NULL, TRUE otherwise.
 */
BOOL X11DRV_SetupGCForPatBlt( X11DRV_PDEVICE *physDev, GC gc, BOOL fMapColors )
{
    XGCValues val;
    unsigned long mask;
    Pixmap pixmap = 0;
    POINT pt;

    if (physDev->brush.style == BS_NULL) return FALSE;
    if (physDev->brush.pixel == -1)
    {
	/* Special case used for monochrome pattern brushes.
	 * We need to swap foreground and background because
	 * Windows does it the wrong way...
	 */
	val.foreground = physDev->backgroundPixel;
	val.background = physDev->textPixel;
    }
    else
    {
	val.foreground = physDev->brush.pixel;
	val.background = physDev->backgroundPixel;
    }
    if (fMapColors && X11DRV_PALETTE_XPixelToPalette)
    {
        val.foreground = X11DRV_PALETTE_XPixelToPalette[val.foreground];
        val.background = X11DRV_PALETTE_XPixelToPalette[val.background];
    }

    val.function = X11DRV_XROPfunction[GetROP2(physDev->hdc)-1];
    /*
    ** Let's replace GXinvert by GXxor with (black xor white)
    ** This solves the selection color and leak problems in excel
    ** FIXME : Let's do that only if we work with X-pixels, not with Win-pixels
    */
    if (val.function == GXinvert)
    {
        val.foreground = (WhitePixel( gdi_display, DefaultScreen(gdi_display) ) ^
                          BlackPixel( gdi_display, DefaultScreen(gdi_display) ));
	val.function = GXxor;
    }
    val.fill_style = physDev->brush.fillStyle;
    switch(val.fill_style)
    {
    case FillStippled:
    case FillOpaqueStippled:
	if (GetBkMode(physDev->hdc)==OPAQUE) val.fill_style = FillOpaqueStippled;
	val.stipple = physDev->brush.pixmap;
	mask = GCStipple;
        break;

    case FillTiled:
        if (fMapColors && X11DRV_PALETTE_XPixelToPalette)
        {
            register int x, y;
            XImage *image;
            wine_tsx11_lock();
            pixmap = XCreatePixmap( gdi_display, root_window, 8, 8, screen_depth );
            image = XGetImage( gdi_display, physDev->brush.pixmap, 0, 0, 8, 8,
                               AllPlanes, ZPixmap );
            for (y = 0; y < 8; y++)
                for (x = 0; x < 8; x++)
                    XPutPixel( image, x, y,
                               X11DRV_PALETTE_XPixelToPalette[XGetPixel( image, x, y)] );
            XPutImage( gdi_display, pixmap, gc, image, 0, 0, 0, 0, 8, 8 );
            XDestroyImage( image );
            wine_tsx11_unlock();
            val.tile = pixmap;
        }
        else val.tile = physDev->brush.pixmap;
	mask = GCTile;
        break;

    default:
        mask = 0;
        break;
    }
    GetBrushOrgEx( physDev->hdc, &pt );
    val.ts_x_origin = physDev->org.x + pt.x;
    val.ts_y_origin = physDev->org.y + pt.y;
    val.fill_rule = (GetPolyFillMode(physDev->hdc) == WINDING) ? WindingRule : EvenOddRule;
    wine_tsx11_lock();
    XChangeGC( gdi_display, gc,
	       GCFunction | GCForeground | GCBackground | GCFillStyle |
	       GCFillRule | GCTileStipXOrigin | GCTileStipYOrigin | mask,
	       &val );
    if (pixmap) XFreePixmap( gdi_display, pixmap );
    wine_tsx11_unlock();
    return TRUE;
}


/***********************************************************************
 *           X11DRV_SetupGCForBrush
 *
 * Setup physDev->gc for drawing operations using current brush.
 * Return FALSE if brush is BS_NULL, TRUE otherwise.
 */
BOOL X11DRV_SetupGCForBrush( X11DRV_PDEVICE *physDev )
{
    return X11DRV_SetupGCForPatBlt( physDev, physDev->gc, FALSE );
}


/***********************************************************************
 *           X11DRV_SetupGCForPen
 *
 * Setup physDev->gc for drawing operations using current pen.
 * Return FALSE if pen is PS_NULL, TRUE otherwise.
 */
BOOL X11DRV_SetupGCForPen( X11DRV_PDEVICE *physDev )
{
    XGCValues val;
    UINT rop2 = GetROP2(physDev->hdc);

    if (physDev->pen.style == PS_NULL) return FALSE;

    switch (rop2)
    {
    case R2_BLACK :
        val.foreground = BlackPixel( gdi_display, DefaultScreen(gdi_display) );
	val.function = GXcopy;
	break;
    case R2_WHITE :
        val.foreground = WhitePixel( gdi_display, DefaultScreen(gdi_display) );
	val.function = GXcopy;
	break;
    case R2_XORPEN :
	val.foreground = physDev->pen.pixel;
	/* It is very unlikely someone wants to XOR with 0 */
	/* This fixes the rubber-drawings in paintbrush */
	if (val.foreground == 0)
            val.foreground = (WhitePixel( gdi_display, DefaultScreen(gdi_display) ) ^
                              BlackPixel( gdi_display, DefaultScreen(gdi_display) ));
	val.function = GXxor;
	break;
    default :
	val.foreground = physDev->pen.pixel;
	val.function   = X11DRV_XROPfunction[rop2-1];
    }
    val.background = physDev->backgroundPixel;
    val.fill_style = FillSolid;
    val.line_width = physDev->pen.width;
    if (val.line_width <= 1) {
	val.cap_style = CapNotLast;
    } else {
	switch (physDev->pen.endcap)
	{
	case PS_ENDCAP_SQUARE:
	    val.cap_style = CapProjecting;
	    break;
	case PS_ENDCAP_FLAT:
	    val.cap_style = CapButt;
	    break;
	case PS_ENDCAP_ROUND:
	default:
	    val.cap_style = CapRound;
	}
    }
    switch (physDev->pen.linejoin)
    {
    case PS_JOIN_BEVEL:
	val.join_style = JoinBevel;
        break;
    case PS_JOIN_MITER:
	val.join_style = JoinMiter;
        break;
    case PS_JOIN_ROUND:
    default:
	val.join_style = JoinRound;
    }
    wine_tsx11_lock();
    if ((physDev->pen.width <= 1) &&
        (physDev->pen.style != PS_SOLID) &&
        (physDev->pen.style != PS_INSIDEFRAME))
    {
        XSetDashes( gdi_display, physDev->gc, 0, physDev->pen.dashes, physDev->pen.dash_len );
        val.line_style = (GetBkMode(physDev->hdc) == OPAQUE) ? LineDoubleDash : LineOnOffDash;
    }
    else val.line_style = LineSolid;

    XChangeGC( gdi_display, physDev->gc,
	       GCFunction | GCForeground | GCBackground | GCLineWidth |
	       GCLineStyle | GCCapStyle | GCJoinStyle | GCFillStyle, &val );
    wine_tsx11_unlock();
    return TRUE;
}


/***********************************************************************
 *           X11DRV_SetupGCForText
 *
 * Setup physDev->gc for text drawing operations.
 * Return FALSE if the font is null, TRUE otherwise.
 */
BOOL X11DRV_SetupGCForText( X11DRV_PDEVICE *physDev )
{
    XFontStruct* xfs = XFONT_GetFontStruct( physDev->font );

    if( xfs )
    {
	XGCValues val;

	val.function   = GXcopy;  /* Text is always GXcopy */
	val.foreground = physDev->textPixel;
	val.background = physDev->backgroundPixel;
	val.fill_style = FillSolid;
	val.font       = xfs->fid;

        wine_tsx11_lock();
        XChangeGC( gdi_display, physDev->gc,
		   GCFunction | GCForeground | GCBackground | GCFillStyle |
		   GCFont, &val );
        wine_tsx11_unlock();
	return TRUE;
    }
    WARN("Physical font failure\n" );
    return FALSE;
}

/***********************************************************************
 *           X11DRV_XWStoDS
 *
 * Performs a world-to-viewport transformation on the specified width.
 */
INT X11DRV_XWStoDS( X11DRV_PDEVICE *physDev, INT width )
{
    POINT pt[2];

    pt[0].x = 0;
    pt[0].y = 0;
    pt[1].x = width;
    pt[1].y = 0;
    LPtoDP( physDev->hdc, pt, 2 );
    return pt[1].x - pt[0].x;
}

/***********************************************************************
 *           X11DRV_YWStoDS
 *
 * Performs a world-to-viewport transformation on the specified height.
 */
INT X11DRV_YWStoDS( X11DRV_PDEVICE *physDev, INT height )
{
    POINT pt[2];

    pt[0].x = 0;
    pt[0].y = 0;
    pt[1].x = 0;
    pt[1].y = height;
    LPtoDP( physDev->hdc, pt, 2 );
    return pt[1].y - pt[0].y;
}

/***********************************************************************
 *           X11DRV_LineTo
 */
BOOL
X11DRV_LineTo( X11DRV_PDEVICE *physDev, INT x, INT y )
{
    POINT pt[2];

    if (X11DRV_SetupGCForPen( physDev )) {
	/* Update the pixmap from the DIB section */
	X11DRV_LockDIBSection(physDev, DIB_Status_GdiMod, FALSE);

        GetCurrentPositionEx( physDev->hdc, &pt[0] );
        pt[1].x = x;
        pt[1].y = y;
        LPtoDP( physDev->hdc, pt, 2 );

        wine_tsx11_lock();
        XDrawLine(gdi_display, physDev->drawable, physDev->gc,
                  physDev->org.x + pt[0].x, physDev->org.y + pt[0].y,
                  physDev->org.x + pt[1].x, physDev->org.y + pt[1].y );
        wine_tsx11_unlock();

	/* Update the DIBSection from the pixmap */
	X11DRV_UnlockDIBSection(physDev, TRUE);
    }
    return TRUE;
}



/***********************************************************************
 *           X11DRV_DrawArc
 *
 * Helper functions for Arc(), Chord() and Pie().
 * 'lines' is the number of lines to draw: 0 for Arc, 1 for Chord, 2 for Pie.
 *
 */
static BOOL
X11DRV_DrawArc( X11DRV_PDEVICE *physDev, INT left, INT top, INT right,
                INT bottom, INT xstart, INT ystart,
                INT xend, INT yend, INT lines )
{
    INT xcenter, ycenter, istart_angle, idiff_angle;
    INT width, oldwidth;
    double start_angle, end_angle;
    XPoint points[4];
    BOOL update = FALSE;
    POINT start, end;
    RECT rc;

    SetRect(&rc, left, top, right, bottom);
    start.x = xstart;
    start.y = ystart;
    end.x = xend;
    end.y = yend;
    LPtoDP(physDev->hdc, (POINT*)&rc, 2);
    LPtoDP(physDev->hdc, &start, 1);
    LPtoDP(physDev->hdc, &end, 1);

    if (rc.right < rc.left) { INT tmp = rc.right; rc.right = rc.left; rc.left = tmp; }
    if (rc.bottom < rc.top) { INT tmp = rc.bottom; rc.bottom = rc.top; rc.top = tmp; }
    if ((rc.left == rc.right) || (rc.top == rc.bottom)
            ||(lines && ((rc.right-rc.left==1)||(rc.bottom-rc.top==1)))) return TRUE;

    if (GetArcDirection( physDev->hdc ) == AD_CLOCKWISE)
      { POINT tmp = start; start = end; end = tmp; }

    oldwidth = width = physDev->pen.width;
    if (!width) width = 1;
    if(physDev->pen.style == PS_NULL) width = 0;

    if ((physDev->pen.style == PS_INSIDEFRAME))
    {
        if (2*width > (rc.right-rc.left)) width=(rc.right-rc.left + 1)/2;
        if (2*width > (rc.bottom-rc.top)) width=(rc.bottom-rc.top + 1)/2;
        rc.left   += width / 2;
        rc.right  -= (width - 1) / 2;
        rc.top    += width / 2;
        rc.bottom -= (width - 1) / 2;
    }
    if(width == 0) width = 1; /* more accurate */
    physDev->pen.width = width;

    xcenter = (rc.right + rc.left) / 2;
    ycenter = (rc.bottom + rc.top) / 2;
    start_angle = atan2( (double)(ycenter-start.y)*(rc.right-rc.left),
			 (double)(start.x-xcenter)*(rc.bottom-rc.top) );
    end_angle   = atan2( (double)(ycenter-end.y)*(rc.right-rc.left),
			 (double)(end.x-xcenter)*(rc.bottom-rc.top) );
    if ((start.x==end.x)&&(start.y==end.y))
      { /* A lazy program delivers xstart=xend=ystart=yend=0) */
	start_angle = 0;
	end_angle = 2* PI;
      }
    else /* notorious cases */
      if ((start_angle == PI)&&( end_angle <0))
	start_angle = - PI;
    else
      if ((end_angle == PI)&&( start_angle <0))
	end_angle = - PI;
    istart_angle = (INT)(start_angle * 180 * 64 / PI + 0.5);
    idiff_angle  = (INT)((end_angle - start_angle) * 180 * 64 / PI + 0.5);
    if (idiff_angle <= 0) idiff_angle += 360 * 64;

    /* Update the pixmap from the DIB section */
    X11DRV_LockDIBSection(physDev, DIB_Status_GdiMod, FALSE);

      /* Fill arc with brush if Chord() or Pie() */

    if ((lines > 0) && X11DRV_SetupGCForBrush( physDev )) {
        wine_tsx11_lock();
        XSetArcMode( gdi_display, physDev->gc, (lines==1) ? ArcChord : ArcPieSlice);
        XFillArc( gdi_display, physDev->drawable, physDev->gc,
                  physDev->org.x + rc.left, physDev->org.y + rc.top,
                  rc.right-rc.left-1, rc.bottom-rc.top-1, istart_angle, idiff_angle );
        wine_tsx11_unlock();
	update = TRUE;
    }

      /* Draw arc and lines */

    if (X11DRV_SetupGCForPen( physDev ))
    {
        wine_tsx11_lock();
        XDrawArc( gdi_display, physDev->drawable, physDev->gc,
                  physDev->org.x + rc.left, physDev->org.y + rc.top,
                  rc.right-rc.left-1, rc.bottom-rc.top-1, istart_angle, idiff_angle );
        if (lines) {
            /* use the truncated values */
            start_angle=(double)istart_angle*PI/64./180.;
            end_angle=(double)(istart_angle+idiff_angle)*PI/64./180.;
            /* calculate the endpoints and round correctly */
            points[0].x = (int) floor(physDev->org.x + (rc.right+rc.left)/2.0 +
                    cos(start_angle) * (rc.right-rc.left-width*2+2) / 2. + 0.5);
            points[0].y = (int) floor(physDev->org.y + (rc.top+rc.bottom)/2.0 -
                    sin(start_angle) * (rc.bottom-rc.top-width*2+2) / 2. + 0.5);
            points[1].x = (int) floor(physDev->org.x + (rc.right+rc.left)/2.0 +
                    cos(end_angle) * (rc.right-rc.left-width*2+2) / 2. + 0.5);
            points[1].y = (int) floor(physDev->org.y + (rc.top+rc.bottom)/2.0 -
                    sin(end_angle) * (rc.bottom-rc.top-width*2+2) / 2. + 0.5);

            /* OK, this stuff is optimized for Xfree86
             * which is probably the server most used by
             * wine users. Other X servers will not
             * display correctly. (eXceed for instance)
             * so if you feel you must make changes, make sure that
             * you either use Xfree86 or separate your changes
             * from these (compile switch or whatever)
             */
            if (lines == 2) {
                INT dx1,dy1;
                points[3] = points[1];
                points[1].x = physDev->org.x + xcenter;
                points[1].y = physDev->org.y + ycenter;
                points[2] = points[1];
                dx1=points[1].x-points[0].x;
                dy1=points[1].y-points[0].y;
                if(((rc.top-rc.bottom) | -2) == -2)
                    if(dy1>0) points[1].y--;
                if(dx1<0) {
                    if (((-dx1)*64)<=ABS(dy1)*37) points[0].x--;
                    if(((-dx1*9))<(dy1*16)) points[0].y--;
                    if( dy1<0 && ((dx1*9)) < (dy1*16)) points[0].y--;
                } else {
                    if(dy1 < 0)  points[0].y--;
                    if(((rc.right-rc.left) | -2) == -2) points[1].x--;
                }
                dx1=points[3].x-points[2].x;
                dy1=points[3].y-points[2].y;
                if(((rc.top-rc.bottom) | -2 ) == -2)
                    if(dy1 < 0) points[2].y--;
                if( dx1<0){
                    if( dy1>0) points[3].y--;
                    if(((rc.right-rc.left) | -2) == -2 ) points[2].x--;
                }else {
                    points[3].y--;
                    if( dx1 * 64 < dy1 * -37 ) points[3].x--;
                }
                lines++;
	    }
            XDrawLines( gdi_display, physDev->drawable, physDev->gc,
                        points, lines+1, CoordModeOrigin );
        }
        wine_tsx11_unlock();
	update = TRUE;
    }

    /* Update the DIBSection of the pixmap */
    X11DRV_UnlockDIBSection(physDev, update);

    physDev->pen.width = oldwidth;
    return TRUE;
}


/***********************************************************************
 *           X11DRV_Arc
 */
BOOL
X11DRV_Arc( X11DRV_PDEVICE *physDev, INT left, INT top, INT right, INT bottom,
            INT xstart, INT ystart, INT xend, INT yend )
{
    return X11DRV_DrawArc( physDev, left, top, right, bottom,
			   xstart, ystart, xend, yend, 0 );
}


/***********************************************************************
 *           X11DRV_Pie
 */
BOOL
X11DRV_Pie( X11DRV_PDEVICE *physDev, INT left, INT top, INT right, INT bottom,
            INT xstart, INT ystart, INT xend, INT yend )
{
    return X11DRV_DrawArc( physDev, left, top, right, bottom,
			   xstart, ystart, xend, yend, 2 );
}

/***********************************************************************
 *           X11DRV_Chord
 */
BOOL
X11DRV_Chord( X11DRV_PDEVICE *physDev, INT left, INT top, INT right, INT bottom,
              INT xstart, INT ystart, INT xend, INT yend )
{
    return X11DRV_DrawArc( physDev, left, top, right, bottom,
		  	   xstart, ystart, xend, yend, 1 );
}


/***********************************************************************
 *           X11DRV_Ellipse
 */
BOOL
X11DRV_Ellipse( X11DRV_PDEVICE *physDev, INT left, INT top, INT right, INT bottom )
{
    INT width, oldwidth;
    BOOL update = FALSE;
    RECT rc;

    SetRect(&rc, left, top, right, bottom);
    LPtoDP(physDev->hdc, (POINT*)&rc, 2);

    if ((rc.left == rc.right) || (rc.top == rc.bottom)) return TRUE;

    if (rc.right < rc.left) { INT tmp = rc.right; rc.right = rc.left; rc.left = tmp; }
    if (rc.bottom < rc.top) { INT tmp = rc.bottom; rc.bottom = rc.top; rc.top = tmp; }

    oldwidth = width = physDev->pen.width;
    if (!width) width = 1;
    if(physDev->pen.style == PS_NULL) width = 0;

    if ((physDev->pen.style == PS_INSIDEFRAME))
    {
        if (2*width > (rc.right-rc.left)) width=(rc.right-rc.left + 1)/2;
        if (2*width > (rc.bottom-rc.top)) width=(rc.bottom-rc.top + 1)/2;
        rc.left   += width / 2;
        rc.right  -= (width - 1) / 2;
        rc.top    += width / 2;
        rc.bottom -= (width - 1) / 2;
    }
    if(width == 0) width = 1; /* more accurate */
    physDev->pen.width = width;

    /* Update the pixmap from the DIB section */
    X11DRV_LockDIBSection(physDev, DIB_Status_GdiMod, FALSE);

    if (X11DRV_SetupGCForBrush( physDev ))
    {
        wine_tsx11_lock();
        XFillArc( gdi_display, physDev->drawable, physDev->gc,
                  physDev->org.x + rc.left, physDev->org.y + rc.top,
                  rc.right-rc.left-1, rc.bottom-rc.top-1, 0, 360*64 );
        wine_tsx11_unlock();
	update = TRUE;
    }
    if (X11DRV_SetupGCForPen( physDev ))
    {
        wine_tsx11_lock();
        XDrawArc( gdi_display, physDev->drawable, physDev->gc,
                  physDev->org.x + rc.left, physDev->org.y + rc.top,
                  rc.right-rc.left-1, rc.bottom-rc.top-1, 0, 360*64 );
        wine_tsx11_unlock();
	update = TRUE;
    }

    /* Update the DIBSection from the pixmap */
    X11DRV_UnlockDIBSection(physDev, update);

    physDev->pen.width = oldwidth;
    return TRUE;
}


/***********************************************************************
 *           X11DRV_Rectangle
 */
BOOL
X11DRV_Rectangle(X11DRV_PDEVICE *physDev, INT left, INT top, INT right, INT bottom)
{
    INT width, oldwidth, oldjoinstyle;
    BOOL update = FALSE;
    RECT rc;

    TRACE("(%d %d %d %d)\n", left, top, right, bottom);

    SetRect(&rc, left, top, right, bottom);
    LPtoDP(physDev->hdc, (POINT*)&rc, 2);

    if ((rc.left == rc.right) || (rc.top == rc.bottom)) return TRUE;

    if (rc.right < rc.left) { INT tmp = rc.right; rc.right = rc.left; rc.left = tmp; }
    if (rc.bottom < rc.top) { INT tmp = rc.bottom; rc.bottom = rc.top; rc.top = tmp; }

    oldwidth = width = physDev->pen.width;
    if (!width) width = 1;
    if(physDev->pen.style == PS_NULL) width = 0;

    if ((physDev->pen.style == PS_INSIDEFRAME))
    {
        if (2*width > (rc.right-rc.left)) width=(rc.right-rc.left + 1)/2;
        if (2*width > (rc.bottom-rc.top)) width=(rc.bottom-rc.top + 1)/2;
        rc.left   += width / 2;
        rc.right  -= (width - 1) / 2;
        rc.top    += width / 2;
        rc.bottom -= (width - 1) / 2;
    }
    if(width == 1) width = 0;
    physDev->pen.width = width;
    oldjoinstyle = physDev->pen.linejoin;
    if(physDev->pen.type != PS_GEOMETRIC)
        physDev->pen.linejoin = PS_JOIN_MITER;

    /* Update the pixmap from the DIB section */
    X11DRV_LockDIBSection(physDev, DIB_Status_GdiMod, FALSE);

    if ((rc.right > rc.left + width) && (rc.bottom > rc.top + width))
    {
        if (X11DRV_SetupGCForBrush( physDev ))
	{
            wine_tsx11_lock();
            XFillRectangle( gdi_display, physDev->drawable, physDev->gc,
                            physDev->org.x + rc.left + (width + 1) / 2,
                            physDev->org.y + rc.top + (width + 1) / 2,
                            rc.right-rc.left-width-1, rc.bottom-rc.top-width-1);
            wine_tsx11_unlock();
	    update = TRUE;
	}
    }
    if (X11DRV_SetupGCForPen( physDev ))
    {
        wine_tsx11_lock();
        XDrawRectangle( gdi_display, physDev->drawable, physDev->gc,
                        physDev->org.x + rc.left, physDev->org.y + rc.top,
                        rc.right-rc.left-1, rc.bottom-rc.top-1 );
        wine_tsx11_unlock();
	update = TRUE;
    }

    /* Update the DIBSection from the pixmap */
    X11DRV_UnlockDIBSection(physDev, update);

    physDev->pen.width = oldwidth;
    physDev->pen.linejoin = oldjoinstyle;
    return TRUE;
}

/***********************************************************************
 *           X11DRV_RoundRect
 */
BOOL
X11DRV_RoundRect( X11DRV_PDEVICE *physDev, INT left, INT top, INT right,
                  INT bottom, INT ell_width, INT ell_height )
{
    INT width, oldwidth, oldendcap;
    BOOL update = FALSE;
    RECT rc;
    POINT pts[2];

    TRACE("(%d %d %d %d  %d %d\n",
    	left, top, right, bottom, ell_width, ell_height);

    SetRect(&rc, left, top, right, bottom);
    LPtoDP(physDev->hdc, (POINT*)&rc, 2);

    if ((rc.left == rc.right) || (rc.top == rc.bottom))
	return TRUE;

    /* Make sure ell_width and ell_height are >= 1 otherwise XDrawArc gets
       called with width/height < 0 */
    pts[0].x = pts[0].y = 0;
    pts[1].x = ell_width;
    pts[1].y = ell_height;
    LPtoDP(physDev->hdc, pts, 2);
    ell_width  = max(abs( pts[1].x - pts[0].x ), 1);
    ell_height = max(abs( pts[1].y - pts[0].y ), 1);

    /* Fix the coordinates */

    if (rc.right < rc.left) { INT tmp = rc.right; rc.right = rc.left; rc.left = tmp; }
    if (rc.bottom < rc.top) { INT tmp = rc.bottom; rc.bottom = rc.top; rc.top = tmp; }

    oldwidth = width = physDev->pen.width;
    oldendcap = physDev->pen.endcap;
    if (!width) width = 1;
    if(physDev->pen.style == PS_NULL) width = 0;

    if ((physDev->pen.style == PS_INSIDEFRAME))
    {
        if (2*width > (rc.right-rc.left)) width=(rc.right-rc.left + 1)/2;
        if (2*width > (rc.bottom-rc.top)) width=(rc.bottom-rc.top + 1)/2;
        rc.left   += width / 2;
        rc.right  -= (width - 1) / 2;
        rc.top    += width / 2;
        rc.bottom -= (width - 1) / 2;
    }
    if(width == 0) width = 1;
    physDev->pen.width = width;
    physDev->pen.endcap = PS_ENDCAP_SQUARE;

    /* Update the pixmap from the DIB section */
    X11DRV_LockDIBSection(physDev, DIB_Status_GdiMod, FALSE);

    wine_tsx11_lock();
    if (X11DRV_SetupGCForBrush( physDev ))
    {
        if (ell_width > (rc.right-rc.left) )
            if (ell_height > (rc.bottom-rc.top) )
                XFillArc( gdi_display, physDev->drawable, physDev->gc,
                          physDev->org.x + rc.left, physDev->org.y + rc.top,
                          rc.right - rc.left - 1, rc.bottom - rc.top - 1,
                          0, 360 * 64 );
            else{
                XFillArc( gdi_display, physDev->drawable, physDev->gc,
                          physDev->org.x + rc.left, physDev->org.y + rc.top,
                          rc.right - rc.left - 1, ell_height, 0, 180 * 64 );
                XFillArc( gdi_display, physDev->drawable, physDev->gc,
                          physDev->org.x + rc.left,
                          physDev->org.y + rc.bottom - ell_height - 1,
                          rc.right - rc.left - 1, ell_height, 180 * 64,
                          180 * 64 );
            }
	else if (ell_height > (rc.bottom-rc.top) ){
            XFillArc( gdi_display, physDev->drawable, physDev->gc,
                      physDev->org.x + rc.left, physDev->org.y + rc.top,
                      ell_width, rc.bottom - rc.top - 1, 90 * 64, 180 * 64 );
            XFillArc( gdi_display, physDev->drawable, physDev->gc,
                      physDev->org.x + rc.right - ell_width - 1, physDev->org.y + rc.top,
                      ell_width, rc.bottom - rc.top - 1, 270 * 64, 180 * 64 );
        }else{
            XFillArc( gdi_display, physDev->drawable, physDev->gc,
                      physDev->org.x + rc.left, physDev->org.y + rc.top,
                      ell_width, ell_height, 90 * 64, 90 * 64 );
            XFillArc( gdi_display, physDev->drawable, physDev->gc,
                      physDev->org.x + rc.left,
                      physDev->org.y + rc.bottom - ell_height - 1,
                      ell_width, ell_height, 180 * 64, 90 * 64 );
            XFillArc( gdi_display, physDev->drawable, physDev->gc,
                      physDev->org.x + rc.right - ell_width - 1,
                      physDev->org.y + rc.bottom - ell_height - 1,
                      ell_width, ell_height, 270 * 64, 90 * 64 );
            XFillArc( gdi_display, physDev->drawable, physDev->gc,
                      physDev->org.x + rc.right - ell_width - 1,
                      physDev->org.y + rc.top,
                      ell_width, ell_height, 0, 90 * 64 );
        }
        if (ell_width < rc.right - rc.left)
        {
            XFillRectangle( gdi_display, physDev->drawable, physDev->gc,
                            physDev->org.x + rc.left + (ell_width + 1) / 2,
                            physDev->org.y + rc.top + 1,
                            rc.right - rc.left - ell_width - 1,
                            (ell_height + 1) / 2 - 1);
            XFillRectangle( gdi_display, physDev->drawable, physDev->gc,
                            physDev->org.x + rc.left + (ell_width + 1) / 2,
                            physDev->org.y + rc.bottom - (ell_height) / 2 - 1,
                            rc.right - rc.left - ell_width - 1,
                            (ell_height) / 2 );
        }
        if  (ell_height < rc.bottom - rc.top)
        {
            XFillRectangle( gdi_display, physDev->drawable, physDev->gc,
                            physDev->org.x + rc.left + 1,
                            physDev->org.y + rc.top + (ell_height + 1) / 2,
                            rc.right - rc.left - 2,
                            rc.bottom - rc.top - ell_height - 1);
        }
	update = TRUE;
    }
    /* FIXME: this could be done with on X call
     * more efficient and probably more correct
     * on any X server: XDrawArcs will draw
     * straight horizontal and vertical lines
     * if width or height are zero.
     *
     * BTW this stuff is optimized for an Xfree86 server
     * read the comments inside the X11DRV_DrawArc function
     */
    if (X11DRV_SetupGCForPen( physDev ))
    {
        if (ell_width > (rc.right-rc.left) )
            if (ell_height > (rc.bottom-rc.top) )
                XDrawArc( gdi_display, physDev->drawable, physDev->gc,
                          physDev->org.x + rc.left, physDev->org.y + rc.top,
                          rc.right - rc.left - 1, rc.bottom - rc.top - 1, 0 , 360 * 64 );
            else{
                XDrawArc( gdi_display, physDev->drawable, physDev->gc,
                          physDev->org.x + rc.left, physDev->org.y + rc.top,
                          rc.right - rc.left - 1, ell_height - 1, 0 , 180 * 64 );
                XDrawArc( gdi_display, physDev->drawable, physDev->gc,
                          physDev->org.x + rc.left,
                          physDev->org.y + rc.bottom - ell_height,
                          rc.right - rc.left - 1, ell_height - 1, 180 * 64 , 180 * 64 );
            }
	else if (ell_height > (rc.bottom-rc.top) ){
            XDrawArc( gdi_display, physDev->drawable, physDev->gc,
                      physDev->org.x + rc.left, physDev->org.y + rc.top,
                      ell_width - 1 , rc.bottom - rc.top - 1, 90 * 64 , 180 * 64 );
            XDrawArc( gdi_display, physDev->drawable, physDev->gc,
                      physDev->org.x + rc.right - ell_width,
                      physDev->org.y + rc.top,
                      ell_width - 1 , rc.bottom - rc.top - 1, 270 * 64 , 180 * 64 );
	}else{
            XDrawArc( gdi_display, physDev->drawable, physDev->gc,
                      physDev->org.x + rc.left, physDev->org.y + rc.top,
                      ell_width - 1, ell_height - 1, 90 * 64, 90 * 64 );
            XDrawArc( gdi_display, physDev->drawable, physDev->gc,
                      physDev->org.x + rc.left, physDev->org.y + rc.bottom - ell_height,
                      ell_width - 1, ell_height - 1, 180 * 64, 90 * 64 );
            XDrawArc( gdi_display, physDev->drawable, physDev->gc,
                      physDev->org.x + rc.right - ell_width,
                      physDev->org.y + rc.bottom - ell_height,
                      ell_width - 1, ell_height - 1, 270 * 64, 90 * 64 );
            XDrawArc( gdi_display, physDev->drawable, physDev->gc,
                      physDev->org.x + rc.right - ell_width, physDev->org.y + rc.top,
                      ell_width - 1, ell_height - 1, 0, 90 * 64 );
	}
	if (ell_width < rc.right - rc.left)
	{
            XDrawLine( gdi_display, physDev->drawable, physDev->gc,
                       physDev->org.x + rc.left + ell_width / 2,
                       physDev->org.y + rc.top,
                       physDev->org.x + rc.right - (ell_width+1) / 2,
                       physDev->org.y + rc.top);
            XDrawLine( gdi_display, physDev->drawable, physDev->gc,
                       physDev->org.x + rc.left + ell_width / 2 ,
                       physDev->org.y + rc.bottom - 1,
                       physDev->org.x + rc.right - (ell_width+1)/ 2,
                       physDev->org.y + rc.bottom - 1);
	}
	if (ell_height < rc.bottom - rc.top)
	{
            XDrawLine( gdi_display, physDev->drawable, physDev->gc,
                       physDev->org.x + rc.right - 1,
                       physDev->org.y + rc.top + ell_height / 2,
                       physDev->org.x + rc.right - 1,
                       physDev->org.y + rc.bottom - (ell_height+1) / 2);
            XDrawLine( gdi_display, physDev->drawable, physDev->gc,
                       physDev->org.x + rc.left,
                       physDev->org.y + rc.top + ell_height / 2,
                       physDev->org.x + rc.left,
                       physDev->org.y + rc.bottom - (ell_height+1) / 2);
	}
	update = TRUE;
    }
    wine_tsx11_unlock();
    /* Update the DIBSection from the pixmap */
    X11DRV_UnlockDIBSection(physDev, update);

    physDev->pen.width = oldwidth;
    physDev->pen.endcap = oldendcap;
    return TRUE;
}


/***********************************************************************
 *           X11DRV_SetPixel
 */
COLORREF
X11DRV_SetPixel( X11DRV_PDEVICE *physDev, INT x, INT y, COLORREF color )
{
    Pixel pixel;
    POINT pt;

    pt.x = x;
    pt.y = y;
    LPtoDP( physDev->hdc, &pt, 1 );
    pixel = X11DRV_PALETTE_ToPhysical( physDev, color );

    /* Update the pixmap from the DIB section */
    X11DRV_LockDIBSection(physDev, DIB_Status_GdiMod, FALSE);

    /* inefficient but simple... */
    wine_tsx11_lock();
    XSetForeground( gdi_display, physDev->gc, pixel );
    XSetFunction( gdi_display, physDev->gc, GXcopy );
    XDrawPoint( gdi_display, physDev->drawable, physDev->gc,
                physDev->org.x + pt.x, physDev->org.y + pt.y );
    wine_tsx11_unlock();

    /* Update the DIBSection from the pixmap */
    X11DRV_UnlockDIBSection(physDev, TRUE);

    return X11DRV_PALETTE_ToLogical(pixel);
}


/***********************************************************************
 *           X11DRV_GetPixel
 */
COLORREF
X11DRV_GetPixel( X11DRV_PDEVICE *physDev, INT x, INT y )
{
    static Pixmap pixmap = 0;
    XImage * image;
    int pixel;
    POINT pt;
    BOOL memdc = (GetObjectType(physDev->hdc) == OBJ_MEMDC);

    pt.x = x;
    pt.y = y;
    LPtoDP( physDev->hdc, &pt, 1 );

    /* Update the pixmap from the DIB section */
    X11DRV_LockDIBSection(physDev, DIB_Status_GdiMod, FALSE);

    wine_tsx11_lock();
    if (memdc)
    {
        image = XGetImage( gdi_display, physDev->drawable,
                           physDev->org.x + pt.x, physDev->org.y + pt.y,
                           1, 1, AllPlanes, ZPixmap );
    }
    else
    {
        /* If we are reading from the screen, use a temporary copy */
        /* to avoid a BadMatch error */
        if (!pixmap) pixmap = XCreatePixmap( gdi_display, root_window,
                                             1, 1, physDev->depth );
        XCopyArea( gdi_display, physDev->drawable, pixmap, BITMAP_colorGC,
                   physDev->org.x + pt.x, physDev->org.y + pt.y, 1, 1, 0, 0 );
        image = XGetImage( gdi_display, pixmap, 0, 0, 1, 1, AllPlanes, ZPixmap );
    }
    pixel = XGetPixel( image, 0, 0 );
    XDestroyImage( image );
    wine_tsx11_unlock();

    /* Update the DIBSection from the pixmap */
    X11DRV_UnlockDIBSection(physDev, FALSE);

    return X11DRV_PALETTE_ToLogical(pixel);
}


/***********************************************************************
 *           X11DRV_PaintRgn
 */
BOOL
X11DRV_PaintRgn( X11DRV_PDEVICE *physDev, HRGN hrgn )
{
    if (X11DRV_SetupGCForBrush( physDev ))
    {
        int i;
        XRectangle *rect;
        RGNDATA *data = X11DRV_GetRegionData( hrgn, physDev->hdc );

        if (!data) return FALSE;
        rect = (XRectangle *)data->Buffer;
        for (i = 0; i < data->rdh.nCount; i++)
        {
            rect[i].x += physDev->org.x;
            rect[i].y += physDev->org.y;
        }

        X11DRV_LockDIBSection(physDev, DIB_Status_GdiMod, FALSE);
        wine_tsx11_lock();
        XFillRectangles( gdi_display, physDev->drawable, physDev->gc, rect, data->rdh.nCount );
        wine_tsx11_unlock();
        X11DRV_UnlockDIBSection(physDev, TRUE);
        HeapFree( GetProcessHeap(), 0, data );
    }
    return TRUE;
}

/**********************************************************************
 *          X11DRV_Polyline
 */
BOOL
X11DRV_Polyline( X11DRV_PDEVICE *physDev, const POINT* pt, INT count )
{
    INT oldwidth;
    register int i;
    XPoint *points;

    if((oldwidth = physDev->pen.width) == 0) physDev->pen.width = 1;

    if (!(points = HeapAlloc( GetProcessHeap(), 0, sizeof(XPoint) * count )))
    {
        WARN("No memory to convert POINTs to XPoints!\n");
        return FALSE;
    }
    for (i = 0; i < count; i++)
    {
        POINT tmp = pt[i];
        LPtoDP(physDev->hdc, &tmp, 1);
        points[i].x = physDev->org.x + tmp.x;
        points[i].y = physDev->org.y + tmp.y;
    }

    if (X11DRV_SetupGCForPen ( physDev ))
    {
        X11DRV_LockDIBSection(physDev, DIB_Status_GdiMod, FALSE);
        wine_tsx11_lock();
        XDrawLines( gdi_display, physDev->drawable, physDev->gc,
                    points, count, CoordModeOrigin );
        wine_tsx11_unlock();
        X11DRV_UnlockDIBSection(physDev, TRUE);
    }

    HeapFree( GetProcessHeap(), 0, points );
    physDev->pen.width = oldwidth;
    return TRUE;
}


/**********************************************************************
 *          X11DRV_Polygon
 */
BOOL
X11DRV_Polygon( X11DRV_PDEVICE *physDev, const POINT* pt, INT count )
{
    register int i;
    XPoint *points;
    BOOL update = FALSE;

    if (!(points = HeapAlloc( GetProcessHeap(), 0, sizeof(XPoint) * (count+1) )))
    {
        WARN("No memory to convert POINTs to XPoints!\n");
        return FALSE;
    }
    for (i = 0; i < count; i++)
    {
        POINT tmp = pt[i];
        LPtoDP(physDev->hdc, &tmp, 1);
        points[i].x = physDev->org.x + tmp.x;
        points[i].y = physDev->org.y + tmp.y;
    }
    points[count] = points[0];

    /* Update the pixmap from the DIB section */
    X11DRV_LockDIBSection(physDev, DIB_Status_GdiMod, FALSE);

    if (X11DRV_SetupGCForBrush( physDev ))
    {
        wine_tsx11_lock();
        XFillPolygon( gdi_display, physDev->drawable, physDev->gc,
                      points, count+1, Complex, CoordModeOrigin);
        wine_tsx11_unlock();
	update = TRUE;
    }
    if (X11DRV_SetupGCForPen ( physDev ))
    {
        wine_tsx11_lock();
        XDrawLines( gdi_display, physDev->drawable, physDev->gc,
                    points, count+1, CoordModeOrigin );
        wine_tsx11_unlock();
	update = TRUE;
    }

    /* Update the DIBSection from the pixmap */
    X11DRV_UnlockDIBSection(physDev, update);

    HeapFree( GetProcessHeap(), 0, points );
    return TRUE;
}


/**********************************************************************
 *          X11DRV_PolyPolygon
 */
BOOL
X11DRV_PolyPolygon( X11DRV_PDEVICE *physDev, const POINT* pt, const INT* counts, UINT polygons)
{
    HRGN hrgn;

    /* FIXME: The points should be converted to device coords before */
    /* creating the region. */

    hrgn = CreatePolyPolygonRgn( pt, counts, polygons, GetPolyFillMode( physDev->hdc ) );
    X11DRV_PaintRgn( physDev, hrgn );
    DeleteObject( hrgn );

      /* Draw the outline of the polygons */

    if (X11DRV_SetupGCForPen ( physDev ))
    {
	int i, j, max = 0;
	XPoint *points;

	/* Update the pixmap from the DIB section */
	X11DRV_LockDIBSection(physDev, DIB_Status_GdiMod, FALSE);

	for (i = 0; i < polygons; i++) if (counts[i] > max) max = counts[i];
        if (!(points = HeapAlloc( GetProcessHeap(), 0, sizeof(XPoint) * (max+1) )))
        {
            WARN("No memory to convert POINTs to XPoints!\n");
            return FALSE;
        }
	for (i = 0; i < polygons; i++)
	{
	    for (j = 0; j < counts[i]; j++)
	    {
                POINT tmp = *pt;
                LPtoDP(physDev->hdc, &tmp, 1);
                points[j].x = physDev->org.x + tmp.x;
                points[j].y = physDev->org.y + tmp.y;
		pt++;
	    }
	    points[j] = points[0];
            wine_tsx11_lock();
            XDrawLines( gdi_display, physDev->drawable, physDev->gc,
                        points, j + 1, CoordModeOrigin );
            wine_tsx11_unlock();
	}

	/* Update the DIBSection of the dc's bitmap */
	X11DRV_UnlockDIBSection(physDev, TRUE);

	HeapFree( GetProcessHeap(), 0, points );
    }
    return TRUE;
}


/**********************************************************************
 *          X11DRV_PolyPolyline
 */
BOOL
X11DRV_PolyPolyline( X11DRV_PDEVICE *physDev, const POINT* pt, const DWORD* counts, DWORD polylines )
{
    if (X11DRV_SetupGCForPen ( physDev ))
    {
        int i, j, max = 0;
        XPoint *points;

	/* Update the pixmap from the DIB section */
    	X11DRV_LockDIBSection(physDev, DIB_Status_GdiMod, FALSE);

        for (i = 0; i < polylines; i++) if (counts[i] > max) max = counts[i];
        if (!(points = HeapAlloc( GetProcessHeap(), 0, sizeof(XPoint) * max )))
        {
            WARN("No memory to convert POINTs to XPoints!\n");
            return FALSE;
        }
        for (i = 0; i < polylines; i++)
        {
            for (j = 0; j < counts[i]; j++)
            {
                POINT tmp = *pt;
                LPtoDP(physDev->hdc, &tmp, 1);
                points[j].x = physDev->org.x + tmp.x;
                points[j].y = physDev->org.y + tmp.y;
                pt++;
            }
            wine_tsx11_lock();
            XDrawLines( gdi_display, physDev->drawable, physDev->gc,
                        points, j, CoordModeOrigin );
            wine_tsx11_unlock();
        }

	/* Update the DIBSection of the dc's bitmap */
    	X11DRV_UnlockDIBSection(physDev, TRUE);

	HeapFree( GetProcessHeap(), 0, points );
    }
    return TRUE;
}


/**********************************************************************
 *          X11DRV_InternalFloodFill
 *
 * Internal helper function for flood fill.
 * (xorg,yorg) is the origin of the X image relative to the drawable.
 * (x,y) is relative to the origin of the X image.
 */
static void X11DRV_InternalFloodFill(XImage *image, X11DRV_PDEVICE *physDev,
                                     int x, int y,
                                     int xOrg, int yOrg,
                                     Pixel pixel, WORD fillType )
{
    int left, right;

#define TO_FLOOD(x,y)  ((fillType == FLOODFILLBORDER) ? \
                        (XGetPixel(image,x,y) != pixel) : \
                        (XGetPixel(image,x,y) == pixel))

    if (!TO_FLOOD(x,y)) return;

      /* Find left and right boundaries */

    left = right = x;
    while ((left > 0) && TO_FLOOD( left-1, y )) left--;
    while ((right < image->width) && TO_FLOOD( right, y )) right++;
    XFillRectangle( gdi_display, physDev->drawable, physDev->gc,
                    xOrg + left, yOrg + y, right-left, 1 );

      /* Set the pixels of this line so we don't fill it again */

    for (x = left; x < right; x++)
    {
        if (fillType == FLOODFILLBORDER) XPutPixel( image, x, y, pixel );
        else XPutPixel( image, x, y, ~pixel );
    }

      /* Fill the line above */

    if (--y >= 0)
    {
        x = left;
        while (x < right)
        {
            while ((x < right) && !TO_FLOOD(x,y)) x++;
            if (x >= right) break;
            while ((x < right) && TO_FLOOD(x,y)) x++;
            X11DRV_InternalFloodFill(image, physDev, x-1, y,
                                     xOrg, yOrg, pixel, fillType );
        }
    }

      /* Fill the line below */

    if ((y += 2) < image->height)
    {
        x = left;
        while (x < right)
        {
            while ((x < right) && !TO_FLOOD(x,y)) x++;
            if (x >= right) break;
            while ((x < right) && TO_FLOOD(x,y)) x++;
            X11DRV_InternalFloodFill(image, physDev, x-1, y,
                                     xOrg, yOrg, pixel, fillType );
        }
    }
#undef TO_FLOOD
}


/**********************************************************************
 *          X11DRV_ExtFloodFill
 */
BOOL
X11DRV_ExtFloodFill( X11DRV_PDEVICE *physDev, INT x, INT y, COLORREF color,
                     UINT fillType )
{
    XImage *image;
    RECT rect;
    POINT pt;

    TRACE("X11DRV_ExtFloodFill %d,%d %06lx %d\n", x, y, color, fillType );

    pt.x = x;
    pt.y = y;
    LPtoDP( physDev->hdc, &pt, 1 );
    if (!PtInRegion( physDev->region, pt.x, pt.y )) return FALSE;
    GetRgnBox( physDev->region, &rect );

    wine_tsx11_lock();
    image = XGetImage( gdi_display, physDev->drawable,
                       physDev->org.x + rect.left, physDev->org.y + rect.top,
                       rect.right - rect.left, rect.bottom - rect.top,
                       AllPlanes, ZPixmap );
    wine_tsx11_unlock();
    if (!image) return FALSE;

    if (X11DRV_SetupGCForBrush( physDev ))
    {
	/* Update the pixmap from the DIB section */
	X11DRV_LockDIBSection(physDev, DIB_Status_GdiMod, FALSE);

          /* ROP mode is always GXcopy for flood-fill */
        wine_tsx11_lock();
        XSetFunction( gdi_display, physDev->gc, GXcopy );
        X11DRV_InternalFloodFill(image, physDev,
                                 physDev->org.x + pt.x - rect.left,
                                 physDev->org.y + pt.y - rect.top,
                                 rect.left, rect.top,
                                 X11DRV_PALETTE_ToPhysical( physDev, color ),
                                 fillType );
        wine_tsx11_unlock();
        /* Update the DIBSection of the dc's bitmap */
        X11DRV_UnlockDIBSection(physDev, TRUE);
    }

    wine_tsx11_lock();
    XDestroyImage( image );
    wine_tsx11_unlock();
    return TRUE;
}

/**********************************************************************
 *          X11DRV_SetBkColor
 */
COLORREF
X11DRV_SetBkColor( X11DRV_PDEVICE *physDev, COLORREF color )
{
    physDev->backgroundPixel = X11DRV_PALETTE_ToPhysical( physDev, color );
    return color;
}

/**********************************************************************
 *          X11DRV_SetTextColor
 */
COLORREF
X11DRV_SetTextColor( X11DRV_PDEVICE *physDev, COLORREF color )
{
    physDev->textPixel = X11DRV_PALETTE_ToPhysical( physDev, color );
    return color;
}

/***********************************************************************
 *           GetDCOrgEx   (X11DRV.@)
 */
BOOL X11DRV_GetDCOrgEx( X11DRV_PDEVICE *physDev, LPPOINT lpp )
{
    lpp->x = physDev->org.x + physDev->drawable_org.x;
    lpp->y = physDev->org.y + physDev->drawable_org.y;
    return TRUE;
}


/***********************************************************************
 *           SetDCOrg   (X11DRV.@)
 */
DWORD X11DRV_SetDCOrg( X11DRV_PDEVICE *physDev, INT x, INT y )
{
    DWORD ret = MAKELONG( physDev->org.x + physDev->drawable_org.x,
                          physDev->org.y + physDev->drawable_org.y );
    physDev->org.x = x - physDev->drawable_org.x;
    physDev->org.y = y - physDev->drawable_org.y;
    return ret;
}
