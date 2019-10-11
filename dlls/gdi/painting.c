/*
 * GDI drawing functions.
 *
 * Copyright 1993, 1994 Alexandre Julliard
 * Copyright 1997 Bertho A. Stultiens
 *           1999 Huw D M Davies
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
#include "wine/port.h"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winerror.h"
#include "gdi.h"
#include "gdi_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(gdi);


/***********************************************************************
 *           LineTo    (GDI32.@)
 */
BOOL WINAPI LineTo( HDC hdc, INT x, INT y )
{
    DC * dc = DC_GetDCUpdate( hdc );
    BOOL ret;

    if(!dc) return FALSE;

    if(PATH_IsPathOpen(dc->path))
        ret = PATH_LineTo(dc, x, y);
    else
        ret = dc->funcs->pLineTo && dc->funcs->pLineTo(dc->physDev,x,y);
    if(ret) {
        dc->CursPosX = x;
        dc->CursPosY = y;
    }
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           MoveToEx    (GDI32.@)
 */
BOOL WINAPI MoveToEx( HDC hdc, INT x, INT y, LPPOINT pt )
{
    BOOL ret = TRUE;
    DC * dc = DC_GetDCPtr( hdc );

    if(!dc) return FALSE;

    if(pt) {
        pt->x = dc->CursPosX;
        pt->y = dc->CursPosY;
    }
    dc->CursPosX = x;
    dc->CursPosY = y;

    if(PATH_IsPathOpen(dc->path)) ret = PATH_MoveTo(dc);
    else if (dc->funcs->pMoveTo) ret = dc->funcs->pMoveTo(dc->physDev,x,y);
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           Arc    (GDI32.@)
 */
BOOL WINAPI Arc( HDC hdc, INT left, INT top, INT right,
                     INT bottom, INT xstart, INT ystart,
                     INT xend, INT yend )
{
    BOOL ret = FALSE;
    DC * dc = DC_GetDCUpdate( hdc );
    if (dc)
    {
    if(PATH_IsPathOpen(dc->path))
            ret = PATH_Arc(dc, left, top, right, bottom, xstart, ystart, xend, yend,0);
        else if (dc->funcs->pArc)
            ret = dc->funcs->pArc(dc->physDev,left,top,right,bottom,xstart,ystart,xend,yend);
        GDI_ReleaseObj( hdc );
    }
    return ret;
}

/***********************************************************************
 *           ArcTo    (GDI32.@)
 */
BOOL WINAPI ArcTo( HDC hdc,
                     INT left,   INT top,
                     INT right,  INT bottom,
                     INT xstart, INT ystart,
                     INT xend,   INT yend )
{
    BOOL result;
    DC * dc = DC_GetDCUpdate( hdc );
    if(!dc) return FALSE;

    if(dc->funcs->pArcTo)
    {
        result = dc->funcs->pArcTo( dc->physDev, left, top, right, bottom,
				  xstart, ystart, xend, yend );
        GDI_ReleaseObj( hdc );
        return result;
    }
    GDI_ReleaseObj( hdc );
    /*
     * Else emulate it.
     * According to the documentation, a line is drawn from the current
     * position to the starting point of the arc.
     */
    LineTo(hdc, xstart, ystart);
    /*
     * Then the arc is drawn.
     */
    result = Arc(hdc, left, top, right, bottom, xstart, ystart, xend, yend);
    /*
     * If no error occurred, the current position is moved to the ending
     * point of the arc.
     */
    if (result) MoveToEx(hdc, xend, yend, NULL);
    return result;
}


/***********************************************************************
 *           Pie   (GDI32.@)
 */
BOOL WINAPI Pie( HDC hdc, INT left, INT top,
                     INT right, INT bottom, INT xstart, INT ystart,
                     INT xend, INT yend )
{
    BOOL ret = FALSE;
    DC * dc = DC_GetDCUpdate( hdc );
    if (!dc) return FALSE;

    if(PATH_IsPathOpen(dc->path))
        ret = PATH_Arc(dc,left,top,right,bottom,xstart,ystart,xend,yend,2);
    else if(dc->funcs->pPie)
        ret = dc->funcs->pPie(dc->physDev,left,top,right,bottom,xstart,ystart,xend,yend);

    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           Chord    (GDI32.@)
 */
BOOL WINAPI Chord( HDC hdc, INT left, INT top,
                       INT right, INT bottom, INT xstart, INT ystart,
                       INT xend, INT yend )
{
    BOOL ret = FALSE;
    DC * dc = DC_GetDCUpdate( hdc );
    if (!dc) return FALSE;

    if(PATH_IsPathOpen(dc->path))
	ret = PATH_Arc(dc,left,top,right,bottom,xstart,ystart,xend,yend,1);
    else if(dc->funcs->pChord)
        ret = dc->funcs->pChord(dc->physDev,left,top,right,bottom,xstart,ystart,xend,yend);

    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           Ellipse    (GDI32.@)
 */
BOOL WINAPI Ellipse( HDC hdc, INT left, INT top,
                         INT right, INT bottom )
{
    BOOL ret = FALSE;
    DC * dc = DC_GetDCUpdate( hdc );
    if (!dc) return FALSE;

    if(PATH_IsPathOpen(dc->path))
	ret = PATH_Ellipse(dc,left,top,right,bottom);
    else if (dc->funcs->pEllipse)
        ret = dc->funcs->pEllipse(dc->physDev,left,top,right,bottom);

    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           Rectangle    (GDI32.@)
 */
BOOL WINAPI Rectangle( HDC hdc, INT left, INT top,
                           INT right, INT bottom )
{
    BOOL ret = FALSE;
    DC * dc = DC_GetDCUpdate( hdc );
    if (dc)
    {
    if(PATH_IsPathOpen(dc->path))
            ret = PATH_Rectangle(dc, left, top, right, bottom);
        else if (dc->funcs->pRectangle)
            ret = dc->funcs->pRectangle(dc->physDev,left,top,right,bottom);
        GDI_ReleaseObj( hdc );
    }
    return ret;
}


/***********************************************************************
 *           RoundRect    (GDI32.@)
 */
BOOL WINAPI RoundRect( HDC hdc, INT left, INT top, INT right,
                           INT bottom, INT ell_width, INT ell_height )
{
    BOOL ret = FALSE;
    DC *dc = DC_GetDCUpdate( hdc );

    if (dc)
    {
        if(PATH_IsPathOpen(dc->path))
	    ret = PATH_RoundRect(dc,left,top,right,bottom,ell_width,ell_height);
        else if (dc->funcs->pRoundRect)
            ret = dc->funcs->pRoundRect(dc->physDev,left,top,right,bottom,ell_width,ell_height);
        GDI_ReleaseObj( hdc );
    }
    return ret;
}

/***********************************************************************
 *           SetPixel    (GDI32.@)
 */
COLORREF WINAPI SetPixel( HDC hdc, INT x, INT y, COLORREF color )
{
    COLORREF ret = 0;
    DC * dc = DC_GetDCUpdate( hdc );
    if (dc)
    {
        if (dc->funcs->pSetPixel) ret = dc->funcs->pSetPixel(dc->physDev,x,y,color);
        GDI_ReleaseObj( hdc );
    }
    return ret;
}

/***********************************************************************
 *           SetPixelV    (GDI32.@)
 */
BOOL WINAPI SetPixelV( HDC hdc, INT x, INT y, COLORREF color )
{
    BOOL ret = FALSE;
    DC * dc = DC_GetDCUpdate( hdc );
    if (dc)
    {
        if (dc->funcs->pSetPixel)
        {
            dc->funcs->pSetPixel(dc->physDev,x,y,color);
            ret = TRUE;
        }
        GDI_ReleaseObj( hdc );
    }
    return ret;
}

/***********************************************************************
 *           GetPixel    (GDI32.@)
 */
COLORREF WINAPI GetPixel( HDC hdc, INT x, INT y )
{
    COLORREF ret = CLR_INVALID;
    DC * dc = DC_GetDCUpdate( hdc );

    if (dc)
    {
    /* FIXME: should this be in the graphics driver? */
        if (PtVisible( hdc, x, y ))
        {
            if (dc->funcs->pGetPixel) ret = dc->funcs->pGetPixel(dc->physDev,x,y);
        }
        GDI_ReleaseObj( hdc );
    }
    return ret;
}


/******************************************************************************
 * ChoosePixelFormat [GDI32.@]
 * Matches a pixel format to given format
 *
 * PARAMS
 *    hdc  [I] Device context to search for best pixel match
 *    ppfd [I] Pixel format for which a match is sought
 *
 * RETURNS
 *    Success: Pixel format index closest to given format
 *    Failure: 0
 */
INT WINAPI ChoosePixelFormat( HDC hdc, const LPPIXELFORMATDESCRIPTOR ppfd )
{
    INT ret = 0;
    DC * dc = DC_GetDCPtr( hdc );

    TRACE("(%p,%p)\n",hdc,ppfd);

    if (!dc) return 0;

    if (!dc->funcs->pChoosePixelFormat) FIXME(" :stub\n");
    else ret = dc->funcs->pChoosePixelFormat(dc->physDev,ppfd);

    GDI_ReleaseObj( hdc );
    return ret;
}


/******************************************************************************
 * SetPixelFormat [GDI32.@]
 * Sets pixel format of device context
 *
 * PARAMS
 *    hdc          [I] Device context to search for best pixel match
 *    iPixelFormat [I] Pixel format index
 *    ppfd         [I] Pixel format for which a match is sought
 *
 * RETURNS STD
 */
BOOL WINAPI SetPixelFormat( HDC hdc, INT iPixelFormat,
                            const PIXELFORMATDESCRIPTOR *ppfd)
{
    INT bRet = FALSE;
    DC * dc = DC_GetDCPtr( hdc );

    TRACE("(%p,%d,%p)\n",hdc,iPixelFormat,ppfd);

    if (!dc) return 0;

    if (!dc->funcs->pSetPixelFormat) FIXME(" :stub\n");
    else bRet = dc->funcs->pSetPixelFormat(dc->physDev,iPixelFormat,ppfd);

    GDI_ReleaseObj( hdc );
    return bRet;
}


/******************************************************************************
 * GetPixelFormat [GDI32.@]
 * Gets index of pixel format of DC
 *
 * PARAMETERS
 *    hdc [I] Device context whose pixel format index is sought
 *
 * RETURNS
 *    Success: Currently selected pixel format
 *    Failure: 0
 */
INT WINAPI GetPixelFormat( HDC hdc )
{
    INT ret = 0;
    DC * dc = DC_GetDCPtr( hdc );

    TRACE("(%p)\n",hdc);

    if (!dc) return 0;

    if (!dc->funcs->pGetPixelFormat) FIXME(" :stub\n");
    else ret = dc->funcs->pGetPixelFormat(dc->physDev);

    GDI_ReleaseObj( hdc );
    return ret;
}


/******************************************************************************
 * DescribePixelFormat [GDI32.@]
 * Gets info about pixel format from DC
 *
 * PARAMS
 *    hdc          [I] Device context
 *    iPixelFormat [I] Pixel format selector
 *    nBytes       [I] Size of buffer
 *    ppfd         [O] Pointer to structure to receive pixel format data
 *
 * RETURNS
 *    Success: Maximum pixel format index of the device context
 *    Failure: 0
 */
INT WINAPI DescribePixelFormat( HDC hdc, INT iPixelFormat, UINT nBytes,
                                LPPIXELFORMATDESCRIPTOR ppfd )
{
    INT ret = 0;
    DC * dc = DC_GetDCPtr( hdc );

    TRACE("(%p,%d,%d,%p): stub\n",hdc,iPixelFormat,nBytes,ppfd);

    if (!dc) return 0;

    if (!dc->funcs->pDescribePixelFormat)
    {
        FIXME(" :stub\n");
        ppfd->nSize = nBytes;
        ppfd->nVersion = 1;
	ret = 3;
    }
    else ret = dc->funcs->pDescribePixelFormat(dc->physDev,iPixelFormat,nBytes,ppfd);

    GDI_ReleaseObj( hdc );
    return ret;
}


/******************************************************************************
 * SwapBuffers [GDI32.@]
 * Exchanges front and back buffers of window
 *
 * PARAMS
 *    hdc [I] Device context whose buffers get swapped
 *
 * RETURNS STD
 */
BOOL WINAPI SwapBuffers( HDC hdc )
{
    INT bRet = FALSE;
    DC * dc = DC_GetDCPtr( hdc );

    TRACE("(%p)\n",hdc);

    if (!dc) return TRUE;

    if (!dc->funcs->pSwapBuffers)
    {
        FIXME(" :stub\n");
	bRet = TRUE;
    }
    else bRet = dc->funcs->pSwapBuffers(dc->physDev);

    GDI_ReleaseObj( hdc );
    return bRet;
}


/***********************************************************************
 *           PaintRgn    (GDI32.@)
 */
BOOL WINAPI PaintRgn( HDC hdc, HRGN hrgn )
{
    BOOL ret = FALSE;
    DC * dc = DC_GetDCUpdate( hdc );
    if (dc)
    {
        if (dc->funcs->pPaintRgn) ret = dc->funcs->pPaintRgn(dc->physDev,hrgn);
        GDI_ReleaseObj( hdc );
    }
    return ret;
}


/***********************************************************************
 *           FillRgn    (GDI32.@)
 */
BOOL WINAPI FillRgn( HDC hdc, HRGN hrgn, HBRUSH hbrush )
{
    BOOL retval = FALSE;
    HBRUSH prevBrush;
    DC * dc = DC_GetDCUpdate( hdc );

    if (!dc) return FALSE;
    if(dc->funcs->pFillRgn)
        retval = dc->funcs->pFillRgn(dc->physDev, hrgn, hbrush);
    else if ((prevBrush = SelectObject( hdc, hbrush )))
    {
    retval = PaintRgn( hdc, hrgn );
    SelectObject( hdc, prevBrush );
    }
    GDI_ReleaseObj( hdc );
    return retval;
}


/***********************************************************************
 *           FrameRgn     (GDI32.@)
 */
BOOL WINAPI FrameRgn( HDC hdc, HRGN hrgn, HBRUSH hbrush,
                          INT nWidth, INT nHeight )
{
    BOOL ret = FALSE;
    DC *dc = DC_GetDCUpdate( hdc );

    if (!dc) return FALSE;
    if(dc->funcs->pFrameRgn)
        ret = dc->funcs->pFrameRgn( dc->physDev, hrgn, hbrush, nWidth, nHeight );
    else
    {
        HRGN tmp = CreateRectRgn( 0, 0, 0, 0 );
        if (tmp)
        {
            if (REGION_FrameRgn( tmp, hrgn, nWidth, nHeight ))
            {
                FillRgn( hdc, tmp, hbrush );
                ret = TRUE;
            }
            DeleteObject( tmp );
        }
    }
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           InvertRgn    (GDI32.@)
 */
BOOL WINAPI InvertRgn( HDC hdc, HRGN hrgn )
{
    HBRUSH prevBrush;
    INT prevROP;
    BOOL retval;
    DC *dc = DC_GetDCUpdate( hdc );
    if (!dc) return FALSE;

    if(dc->funcs->pInvertRgn)
        retval = dc->funcs->pInvertRgn( dc->physDev, hrgn );
    else
    {
    prevBrush = SelectObject( hdc, GetStockObject(BLACK_BRUSH) );
    prevROP = SetROP2( hdc, R2_NOT );
    retval = PaintRgn( hdc, hrgn );
    SelectObject( hdc, prevBrush );
    SetROP2( hdc, prevROP );
    }
    GDI_ReleaseObj( hdc );
    return retval;
}


/**********************************************************************
 *          Polyline   (GDI32.@)
 */
BOOL WINAPI Polyline( HDC hdc, const POINT* pt, INT count )
{
    BOOL ret = FALSE;
    DC * dc = DC_GetDCUpdate( hdc );
    if (dc)
    {
        if (PATH_IsPathOpen(dc->path)) ret = PATH_Polyline(dc, pt, count);
        else if (dc->funcs->pPolyline) ret = dc->funcs->pPolyline(dc->physDev,pt,count);
        GDI_ReleaseObj( hdc );
    }
    return ret;
}

/**********************************************************************
 *          PolylineTo   (GDI32.@)
 */
BOOL WINAPI PolylineTo( HDC hdc, const POINT* pt, DWORD cCount )
{
    DC * dc = DC_GetDCUpdate( hdc );
    BOOL ret = FALSE;

    if(!dc) return FALSE;

    if(PATH_IsPathOpen(dc->path))
        ret = PATH_PolylineTo(dc, pt, cCount);

    else if(dc->funcs->pPolylineTo)
        ret = dc->funcs->pPolylineTo(dc->physDev, pt, cCount);

    else { /* do it using Polyline */
        POINT *pts = HeapAlloc( GetProcessHeap(), 0,
				sizeof(POINT) * (cCount + 1) );
	if (pts)
        {
	pts[0].x = dc->CursPosX;
	pts[0].y = dc->CursPosY;
	memcpy( pts + 1, pt, sizeof(POINT) * cCount );
	ret = Polyline( hdc, pts, cCount + 1 );
	HeapFree( GetProcessHeap(), 0, pts );
    }
    }
    if(ret) {
        dc->CursPosX = pt[cCount-1].x;
	dc->CursPosY = pt[cCount-1].y;
    }
    GDI_ReleaseObj( hdc );
    return ret;
}


/**********************************************************************
 *          Polygon  (GDI32.@)
 */
BOOL WINAPI Polygon( HDC hdc, const POINT* pt, INT count )
{
    BOOL ret = FALSE;
    DC * dc = DC_GetDCUpdate( hdc );
    if (dc)
    {
        if (PATH_IsPathOpen(dc->path)) ret = PATH_Polygon(dc, pt, count);
        else if (dc->funcs->pPolygon) ret = dc->funcs->pPolygon(dc->physDev,pt,count);
        GDI_ReleaseObj( hdc );
    }
    return ret;
}


/**********************************************************************
 *          PolyPolygon  (GDI32.@)
 */
BOOL WINAPI PolyPolygon( HDC hdc, const POINT* pt, const INT* counts,
                             UINT polygons )
{
    BOOL ret = FALSE;
    DC * dc = DC_GetDCUpdate( hdc );
    if (dc)
    {
        if (PATH_IsPathOpen(dc->path)) ret = PATH_PolyPolygon(dc, pt, counts, polygons);
        else if (dc->funcs->pPolyPolygon) ret = dc->funcs->pPolyPolygon(dc->physDev,pt,counts,polygons);
        GDI_ReleaseObj( hdc );
    }
    return ret;
}

/**********************************************************************
 *          PolyPolyline  (GDI32.@)
 */
BOOL WINAPI PolyPolyline( HDC hdc, const POINT* pt, const DWORD* counts,
                            DWORD polylines )
{
    BOOL ret = FALSE;
    DC * dc = DC_GetDCUpdate( hdc );
    if (dc)
    {
        if (PATH_IsPathOpen(dc->path)) ret = PATH_PolyPolyline(dc, pt, counts, polylines);
        else if (dc->funcs->pPolyPolyline) ret = dc->funcs->pPolyPolyline(dc->physDev,pt,counts,polylines);
        GDI_ReleaseObj( hdc );
    }
    return ret;
}

/**********************************************************************
 *          ExtFloodFill   (GDI32.@)
 */
BOOL WINAPI ExtFloodFill( HDC hdc, INT x, INT y, COLORREF color,
                              UINT fillType )
{
    BOOL ret = FALSE;
    DC * dc = DC_GetDCUpdate( hdc );
    if (dc)
    {
        if (dc->funcs->pExtFloodFill) ret = dc->funcs->pExtFloodFill(dc->physDev,x,y,color,fillType);
        GDI_ReleaseObj( hdc );
    }
    return ret;
}


/**********************************************************************
 *          FloodFill   (GDI32.@)
 */
BOOL WINAPI FloodFill( HDC hdc, INT x, INT y, COLORREF color )
{
    return ExtFloodFill( hdc, x, y, color, FLOODFILLBORDER );
}


/******************************************************************************
 * PolyBezier [GDI32.@]
 * Draws one or more Bezier curves
 *
 * PARAMS
 *    hDc     [I] Handle to device context
 *    lppt    [I] Pointer to endpoints and control points
 *    cPoints [I] Count of endpoints and control points
 *
 * RETURNS STD
 */
BOOL WINAPI PolyBezier( HDC hdc, const POINT* lppt, DWORD cPoints )
{
    BOOL ret = FALSE;
    DC * dc;

    /* cPoints must be 3 * n + 1 (where n>=1) */
    if (cPoints == 1 || (cPoints % 3) != 1) return FALSE;

    dc = DC_GetDCUpdate( hdc );
    if(!dc) return FALSE;

    if(PATH_IsPathOpen(dc->path))
	ret = PATH_PolyBezier(dc, lppt, cPoints);
    else if (dc->funcs->pPolyBezier)
        ret = dc->funcs->pPolyBezier(dc->physDev, lppt, cPoints);
    else  /* We'll convert it into line segments and draw them using Polyline */
    {
        POINT *Pts;
	INT nOut;

	if ((Pts = GDI_Bezier( lppt, cPoints, &nOut )))
        {
	    TRACE("Pts = %p, no = %d\n", Pts, nOut);
	    ret = Polyline( dc->hSelf, Pts, nOut );
	    HeapFree( GetProcessHeap(), 0, Pts );
	}
    }

    GDI_ReleaseObj( hdc );
    return ret;
}

/******************************************************************************
 * PolyBezierTo [GDI32.@]
 * Draws one or more Bezier curves
 *
 * PARAMS
 *    hDc     [I] Handle to device context
 *    lppt    [I] Pointer to endpoints and control points
 *    cPoints [I] Count of endpoints and control points
 *
 * RETURNS STD
 */
BOOL WINAPI PolyBezierTo( HDC hdc, const POINT* lppt, DWORD cPoints )
{
    DC * dc = DC_GetDCUpdate( hdc );
    BOOL ret;

    if(!dc) return FALSE;

    if(PATH_IsPathOpen(dc->path))
        ret = PATH_PolyBezierTo(dc, lppt, cPoints);
    else if(dc->funcs->pPolyBezierTo)
        ret = dc->funcs->pPolyBezierTo(dc->physDev, lppt, cPoints);
    else { /* We'll do it using PolyBezier */
        POINT *pt;
	pt = HeapAlloc( GetProcessHeap(), 0, sizeof(POINT) * (cPoints + 1) );
	if(!pt) return FALSE;
	pt[0].x = dc->CursPosX;
	pt[0].y = dc->CursPosY;
	memcpy(pt + 1, lppt, sizeof(POINT) * cPoints);
	ret = PolyBezier(dc->hSelf, pt, cPoints+1);
	HeapFree( GetProcessHeap(), 0, pt );
    }
    if(ret) {
        dc->CursPosX = lppt[cPoints-1].x;
        dc->CursPosY = lppt[cPoints-1].y;
    }
    GDI_ReleaseObj( hdc );
    return ret;
}

/***********************************************************************
 *      AngleArc (GDI32.@)
 */
BOOL WINAPI AngleArc(HDC hdc, INT x, INT y, DWORD dwRadius, FLOAT eStartAngle, FLOAT eSweepAngle)
{
    INT x1,y1,x2,y2, arcdir;
    BOOL result;
    DC *dc;

    if( (signed int)dwRadius < 0 )
	return FALSE;

    dc = DC_GetDCUpdate( hdc );
    if(!dc) return FALSE;

    if(dc->funcs->pAngleArc)
    {
        result = dc->funcs->pAngleArc( dc->physDev, x, y, dwRadius, eStartAngle, eSweepAngle );

        GDI_ReleaseObj( hdc );
        return result;
    }
    GDI_ReleaseObj( hdc );

    /* AngleArc always works counterclockwise */
    arcdir = GetArcDirection( hdc );
    SetArcDirection( hdc, AD_COUNTERCLOCKWISE );

    x1 = x + cos(eStartAngle*M_PI/180) * dwRadius;
    y1 = y - sin(eStartAngle*M_PI/180) * dwRadius;
    x2 = x + cos((eStartAngle+eSweepAngle)*M_PI/180) * dwRadius;
    y2 = x - sin((eStartAngle+eSweepAngle)*M_PI/180) * dwRadius;

    LineTo( hdc, x1, y1 );
    if( eSweepAngle >= 0 )
        result = Arc( hdc, x-dwRadius, y-dwRadius, x+dwRadius, y+dwRadius,
		      x1, y1, x2, y2 );
    else
	result = Arc( hdc, x-dwRadius, y-dwRadius, x+dwRadius, y+dwRadius,
		      x2, y2, x1, y1 );

    if( result ) MoveToEx( hdc, x2, y2, NULL );
    SetArcDirection( hdc, arcdir );
    return result;
}

/***********************************************************************
 *      PolyDraw (GDI32.@)
 */
BOOL WINAPI PolyDraw(HDC hdc, const POINT *lppt, const BYTE *lpbTypes,
                       DWORD cCount)
{
    DC *dc;
    BOOL result;
    POINT lastmove;
    int i;

    dc = DC_GetDCUpdate( hdc );
    if(!dc) return FALSE;

    if(dc->funcs->pPolyDraw)
    {
        result = dc->funcs->pPolyDraw( dc->physDev, lppt, lpbTypes, cCount );
        GDI_ReleaseObj( hdc );
        return result;
    }
    GDI_ReleaseObj( hdc );

    /* check for each bezierto if there are two more points */
    for( i = 0; i < cCount; i++ )
	if( lpbTypes[i] != PT_MOVETO &&
	    lpbTypes[i] & PT_BEZIERTO )
	{
	    if( cCount < i+3 )
		return FALSE;
	    else
		i += 2;
	}

    /* if no moveto occurs, we will close the figure here */
    lastmove.x = dc->CursPosX;
    lastmove.y = dc->CursPosY;

    /* now let's draw */
    for( i = 0; i < cCount; i++ )
    {
	if( lpbTypes[i] == PT_MOVETO )
	{
	    MoveToEx( hdc, lppt[i].x, lppt[i].y, NULL );
	    lastmove.x = dc->CursPosX;
	    lastmove.y = dc->CursPosY;
	}
	else if( lpbTypes[i] & PT_LINETO )
	    LineTo( hdc, lppt[i].x, lppt[i].y );
	else if( lpbTypes[i] & PT_BEZIERTO )
	{
	    PolyBezierTo( hdc, &lppt[i], 3 );
	    i += 2;
	}
	else
	    return FALSE;

	if( lpbTypes[i] & PT_CLOSEFIGURE )
	{
	    if( PATH_IsPathOpen( dc->path ) )
		CloseFigure( hdc );
	    else
		LineTo( hdc, lastmove.x, lastmove.y );
	}
    }

    return TRUE;
}

/******************************************************************
 *
 *   *Very* simple bezier drawing code,
 *
 *   It uses a recursive algorithm to divide the curve in a series
 *   of straight line segements. Not ideal but for me sufficient.
 *   If you are in need for something better look for some incremental
 *   algorithm.
 *
 *   7 July 1998 Rein Klazes
 */

 /*
  * some macro definitions for bezier drawing
  *
  * to avoid truncation errors the coordinates are
  * shifted upwards. When used in drawing they are
  * shifted down again, including correct rounding
  * and avoiding floating point arithmetic
  * 4 bits should allow 27 bits coordinates which I saw
  * somewhere in the win32 doc's
  *
  */

#define BEZIERSHIFTBITS 4
#define BEZIERSHIFTUP(x)    ((x)<<BEZIERSHIFTBITS)
#define BEZIERPIXEL        BEZIERSHIFTUP(1)
#define BEZIERSHIFTDOWN(x)  (((x)+(1<<(BEZIERSHIFTBITS-1)))>>BEZIERSHIFTBITS)
/* maximum depth of recursion */
#define BEZIERMAXDEPTH  8

/* size of array to store points on */
/* enough for one curve */
#define BEZIER_INITBUFSIZE    (150)

/* calculate Bezier average, in this case the middle
 * correctly rounded...
 * */

#define BEZIERMIDDLE(Mid, P1, P2) \
    (Mid).x=((P1).x+(P2).x + 1)/2;\
    (Mid).y=((P1).y+(P2).y + 1)/2;

/**********************************************************
* BezierCheck helper function to check
* that recursion can be terminated
*       Points[0] and Points[3] are begin and endpoint
*       Points[1] and Points[2] are control points
*       level is the recursion depth
*       returns true if the recusion can be terminated
*/
static BOOL BezierCheck( int level, POINT *Points)
{
    INT dx, dy;
    dx=Points[3].x-Points[0].x;
    dy=Points[3].y-Points[0].y;
    if(abs(dy)<=abs(dx)){/* shallow line */
        /* check that control points are between begin and end */
        if(Points[1].x < Points[0].x){
            if(Points[1].x < Points[3].x)
                return FALSE;
        }else
            if(Points[1].x > Points[3].x)
                return FALSE;
        if(Points[2].x < Points[0].x){
            if(Points[2].x < Points[3].x)
                return FALSE;
        }else
            if(Points[2].x > Points[3].x)
                return FALSE;
        dx=BEZIERSHIFTDOWN(dx);
        if(!dx) return TRUE;
        if(abs(Points[1].y-Points[0].y-(dy/dx)*
                BEZIERSHIFTDOWN(Points[1].x-Points[0].x)) > BEZIERPIXEL ||
           abs(Points[2].y-Points[0].y-(dy/dx)*
                   BEZIERSHIFTDOWN(Points[2].x-Points[0].x)) > BEZIERPIXEL )
            return FALSE;
        else
            return TRUE;
    }else{ /* steep line */
        /* check that control points are between begin and end */
        if(Points[1].y < Points[0].y){
            if(Points[1].y < Points[3].y)
                return FALSE;
        }else
            if(Points[1].y > Points[3].y)
                return FALSE;
        if(Points[2].y < Points[0].y){
            if(Points[2].y < Points[3].y)
                return FALSE;
        }else
            if(Points[2].y > Points[3].y)
                return FALSE;
        dy=BEZIERSHIFTDOWN(dy);
        if(!dy) return TRUE;
        if(abs(Points[1].x-Points[0].x-(dx/dy)*
                BEZIERSHIFTDOWN(Points[1].y-Points[0].y)) > BEZIERPIXEL ||
           abs(Points[2].x-Points[0].x-(dx/dy)*
                   BEZIERSHIFTDOWN(Points[2].y-Points[0].y)) > BEZIERPIXEL )
            return FALSE;
        else
            return TRUE;
    }
}

/* Helper for GDI_Bezier.
 * Just handles one Bezier, so Points should point to four POINTs
 */
static void GDI_InternalBezier( POINT *Points, POINT **PtsOut, INT *dwOut,
				INT *nPtsOut, INT level )
{
    if(*nPtsOut == *dwOut) {
        *dwOut *= 2;
	*PtsOut = HeapReAlloc( GetProcessHeap(), 0, *PtsOut,
			       *dwOut * sizeof(POINT) );
    }

    if(!level || BezierCheck(level, Points)) {
        if(*nPtsOut == 0) {
            (*PtsOut)[0].x = BEZIERSHIFTDOWN(Points[0].x);
            (*PtsOut)[0].y = BEZIERSHIFTDOWN(Points[0].y);
            *nPtsOut = 1;
        }
	(*PtsOut)[*nPtsOut].x = BEZIERSHIFTDOWN(Points[3].x);
        (*PtsOut)[*nPtsOut].y = BEZIERSHIFTDOWN(Points[3].y);
        (*nPtsOut) ++;
    } else {
        POINT Points2[4]; /* for the second recursive call */
        Points2[3]=Points[3];
        BEZIERMIDDLE(Points2[2], Points[2], Points[3]);
        BEZIERMIDDLE(Points2[0], Points[1], Points[2]);
        BEZIERMIDDLE(Points2[1],Points2[0],Points2[2]);

        BEZIERMIDDLE(Points[1], Points[0],  Points[1]);
        BEZIERMIDDLE(Points[2], Points[1], Points2[0]);
        BEZIERMIDDLE(Points[3], Points[2], Points2[1]);

        Points2[0]=Points[3];

        /* do the two halves */
        GDI_InternalBezier(Points, PtsOut, dwOut, nPtsOut, level-1);
        GDI_InternalBezier(Points2, PtsOut, dwOut, nPtsOut, level-1);
    }
}



/***********************************************************************
 *           GDI_Bezier   [INTERNAL]
 *   Calculate line segments that approximate -what microsoft calls- a bezier
 *   curve.
 *   The routine recursively divides the curve in two parts until a straight
 *   line can be drawn
 *
 *  PARAMS
 *
 *  Points  [I] Ptr to count POINTs which are the end and control points
 *              of the set of Bezier curves to flatten.
 *  count   [I] Number of Points.  Must be 3n+1.
 *  nPtsOut [O] Will contain no of points that have been produced (i.e. no. of
 *              lines+1).
 *
 *  RETURNS
 *
 *  Ptr to an array of POINTs that contain the lines that approximinate the
 *  Beziers.  The array is allocated on the process heap and it is the caller's
 *  responsibility to HeapFree it. [this is not a particularly nice interface
 *  but since we can't know in advance how many points will generate, the
 *  alternative would be to call the function twice, once to determine the size
 *  and a second time to do the work - I decided this was too much of a pain].
 */
POINT *GDI_Bezier( const POINT *Points, INT count, INT *nPtsOut )
{
    POINT *out;
    INT Bezier, dwOut = BEZIER_INITBUFSIZE, i;

    if((count - 1) % 3 != 0) {
        ERR("Invalid no. of points\n");
	return NULL;
    }
    *nPtsOut = 0;
    out = HeapAlloc( GetProcessHeap(), 0, dwOut * sizeof(POINT));
    for(Bezier = 0; Bezier < (count-1)/3; Bezier++) {
	POINT ptBuf[4];
	memcpy(ptBuf, Points + Bezier * 3, sizeof(POINT) * 4);
	for(i = 0; i < 4; i++) {
	    ptBuf[i].x = BEZIERSHIFTUP(ptBuf[i].x);
	    ptBuf[i].y = BEZIERSHIFTUP(ptBuf[i].y);
	}
        GDI_InternalBezier( ptBuf, &out, &dwOut, nPtsOut, BEZIERMAXDEPTH );
    }
    TRACE("Produced %d points\n", *nPtsOut);
    return out;
}

/******************************************************************************
 *           GradientFill   (GDI32.@)
 *
 *  FIXME: we don't support the Alpha channel properly
 */
BOOL WINAPI GdiGradientFill( HDC hdc, TRIVERTEX *vert_array, ULONG nvert,
                          void * grad_array, ULONG ngrad, ULONG mode )
{
  int i;

  TRACE("vert_array:0x%08lx nvert:%ld grad_array:0x%08lx ngrad:%ld\n",
        (long)vert_array, nvert, (long)grad_array, ngrad);

  switch(mode) 
    {
    case GRADIENT_FILL_RECT_H:
      for(i = 0; i < ngrad; i++) 
        {
          GRADIENT_RECT *rect = ((GRADIENT_RECT *)grad_array) + i;
          TRIVERTEX *v1 = vert_array + rect->UpperLeft;
          TRIVERTEX *v2 = vert_array + rect->LowerRight;
          int y1 = v1->y < v2->y ? v1->y : v2->y;
          int y2 = v2->y > v1->y ? v2->y : v1->y;
          int x, dx;
          if (v1->x > v2->x)
            {
              TRIVERTEX *t = v2;
              v2 = v1;
              v1 = t;
            }
          dx = v2->x - v1->x;
          for (x = 0; x < dx; x++)
            {
              POINT pts[2];
              HPEN hPen, hOldPen;
              
              hPen = CreatePen( PS_SOLID, 1, RGB(
                  (v1->Red   * (dx - x) + v2->Red   * x) / dx >> 8,
                  (v1->Green * (dx - x) + v2->Green * x) / dx >> 8,
                  (v1->Blue  * (dx - x) + v2->Blue  * x) / dx >> 8));
              hOldPen = SelectObject( hdc, hPen );
              pts[0].x = v1->x + x;
              pts[0].y = y1;
              pts[1].x = v1->x + x;
              pts[1].y = y2;
              Polyline( hdc, &pts[0], 2 );
              DeleteObject( SelectObject(hdc, hOldPen ) );
            }
        }
      break;
    case GRADIENT_FILL_RECT_V:
      for(i = 0; i < ngrad; i++) 
        {
          GRADIENT_RECT *rect = ((GRADIENT_RECT *)grad_array) + i;
          TRIVERTEX *v1 = vert_array + rect->UpperLeft;
          TRIVERTEX *v2 = vert_array + rect->LowerRight;
          int x1 = v1->x < v2->x ? v1->x : v2->x;
          int x2 = v2->x > v1->x ? v2->x : v1->x;
          int y, dy;
          if (v1->y > v2->y)
            {
              TRIVERTEX *t = v2;
              v2 = v1;
              v1 = t;
            }
          dy = v2->y - v1->y;
          for (y = 0; y < dy; y++)
            {
              POINT pts[2];
              HPEN hPen, hOldPen;
              
              hPen = CreatePen( PS_SOLID, 1, RGB(
                  (v1->Red   * (dy - y) + v2->Red   * y) / dy >> 8,
                  (v1->Green * (dy - y) + v2->Green * y) / dy >> 8,
                  (v1->Blue  * (dy - y) + v2->Blue  * y) / dy >> 8));
              hOldPen = SelectObject( hdc, hPen );
              pts[0].x = x1;
              pts[0].y = v1->y + y;
              pts[1].x = x2;
              pts[1].y = v1->y + y;
              Polyline( hdc, &pts[0], 2 );
              DeleteObject( SelectObject(hdc, hOldPen ) );
            }
        }
      break;
    case GRADIENT_FILL_TRIANGLE:
      for (i = 0; i < ngrad; i++)  
        {
          GRADIENT_TRIANGLE *tri = ((GRADIENT_TRIANGLE *)grad_array) + i;
          TRIVERTEX *v1 = vert_array + tri->Vertex1;
          TRIVERTEX *v2 = vert_array + tri->Vertex2;
          TRIVERTEX *v3 = vert_array + tri->Vertex3;
          int y, dy;
          
          if (v1->y > v2->y)
            { TRIVERTEX *t = v1; v1 = v2; v2 = t; }
          if (v2->y > v3->y)
            {
              TRIVERTEX *t = v2; v2 = v3; v3 = t;
              if (v1->y > v2->y)
                { t = v1; v1 = v2; v2 = t; }
            }
          /* v1->y <= v2->y <= v3->y */

          dy = v3->y - v1->y;
          for (y = 0; y < dy; y++)
            {
              /* v1->y <= y < v3->y */
              TRIVERTEX *v = y < (v2->y - v1->y) ? v1 : v3;
              /* (v->y <= y < v2->y) || (v2->y <= y < v->y) */
              int dy2 = v2->y - v->y;
              int y2 = y + v1->y - v->y;

              int x1 = (v3->x     * y  + v1->x     * (dy  - y )) / dy;
              int x2 = (v2->x     * y2 + v-> x     * (dy2 - y2)) / dy2;
              int r1 = (v3->Red   * y  + v1->Red   * (dy  - y )) / dy;
              int r2 = (v2->Red   * y2 + v-> Red   * (dy2 - y2)) / dy2;
              int g1 = (v3->Green * y  + v1->Green * (dy  - y )) / dy;
              int g2 = (v2->Green * y2 + v-> Green * (dy2 - y2)) / dy2;
              int b1 = (v3->Blue  * y  + v1->Blue  * (dy  - y )) / dy;
              int b2 = (v2->Blue  * y2 + v-> Blue  * (dy2 - y2)) / dy2;
               
              int x;
	      if (x1 < x2)
                {
                  int dx = x2 - x1;
                  for (x = 0; x < dx; x++)
                    SetPixel (hdc, x + x1, y + v1->y, RGB(
                      (r1 * (dx - x) + r2 * x) / dx >> 8,
                      (g1 * (dx - x) + g2 * x) / dx >> 8,
                      (b1 * (dx - x) + b2 * x) / dx >> 8));
                }
              else
                {
                  int dx = x1 - x2;
                  for (x = 0; x < dx; x++)
                    SetPixel (hdc, x + x2, y + v1->y, RGB(
                      (r2 * (dx - x) + r1 * x) / dx >> 8,
                      (g2 * (dx - x) + g1 * x) / dx >> 8,
                      (b2 * (dx - x) + b1 * x) / dx >> 8));
                }
            }
        }
      break;
    default:
      return FALSE;
  }

  return TRUE;
}
