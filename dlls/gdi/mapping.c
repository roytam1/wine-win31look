/*
 * GDI mapping mode functions
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

#include "gdi.h"
#include "wownt32.h"
#include "gdi_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(gdi);


/***********************************************************************
 *           MAPPING_FixIsotropic
 *
 * Fix viewport extensions for isotropic mode.
 */
void MAPPING_FixIsotropic( DC * dc )
{
    double xdim = (double)dc->vportExtX * GetDeviceCaps( dc->hSelf, HORZSIZE ) /
                  (GetDeviceCaps( dc->hSelf, HORZRES ) * dc->wndExtX);
    double ydim = (double)dc->vportExtY * GetDeviceCaps( dc->hSelf, VERTSIZE ) /
                  (GetDeviceCaps( dc->hSelf, VERTRES ) * dc->wndExtY);
    if (xdim > ydim)
    {
	dc->vportExtX = floor(dc->vportExtX * fabs( ydim / xdim ) + 0.5);
	if (!dc->vportExtX) dc->vportExtX = 1;
    }
    else
    {
	dc->vportExtY = floor(dc->vportExtY * fabs( xdim / ydim ) + 0.5);
	if (!dc->vportExtY) dc->vportExtY = 1;
    }
}


/***********************************************************************
 *           DPtoLP    (GDI.67)
 */
BOOL16 WINAPI DPtoLP16( HDC16 hdc, LPPOINT16 points, INT16 count )
{
    DC * dc = DC_GetDCPtr( HDC_32(hdc) );
    if (!dc) return FALSE;

    while (count--)
    {
        points->x = MulDiv( points->x - dc->vportOrgX, dc->wndExtX, dc->vportExtX ) + dc->wndOrgX;
        points->y = MulDiv( points->y - dc->vportOrgY, dc->wndExtY, dc->vportExtY ) + dc->wndOrgY;
        points++;
    }
    GDI_ReleaseObj( HDC_32(hdc) );
    return TRUE;
}


/***********************************************************************
 *           DPtoLP    (GDI32.@)
 */
BOOL WINAPI DPtoLP( HDC hdc, LPPOINT points, INT count )
{
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return FALSE;

    if (dc->vport2WorldValid)
    {
        while (count--)
        {
            FLOAT x = points->x;
            FLOAT y = points->y;
            points->x = floor( x * dc->xformVport2World.eM11 +
                               y * dc->xformVport2World.eM21 +
                               dc->xformVport2World.eDx + 0.5 );
            points->y = floor( x * dc->xformVport2World.eM12 +
                               y * dc->xformVport2World.eM22 +
                               dc->xformVport2World.eDy + 0.5 );
            points++;
        }
    }
    GDI_ReleaseObj( hdc );
    return (count < 0);
}


/***********************************************************************
 *           LPtoDP    (GDI.99)
 */
BOOL16 WINAPI LPtoDP16( HDC16 hdc, LPPOINT16 points, INT16 count )
{
    DC * dc = DC_GetDCPtr( HDC_32(hdc) );
    if (!dc) return FALSE;

    while (count--)
    {
        points->x = MulDiv( points->x - dc->wndOrgX, dc->vportExtX, dc->wndExtX ) + dc->vportOrgX;
        points->y = MulDiv( points->y - dc->wndOrgY, dc->vportExtY, dc->wndExtY ) + dc->vportOrgY;
        points++;
    }
    GDI_ReleaseObj( HDC_32(hdc) );
    return TRUE;
}


/***********************************************************************
 *           LPtoDP    (GDI32.@)
 */
BOOL WINAPI LPtoDP( HDC hdc, LPPOINT points, INT count )
{
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return FALSE;

    while (count--)
    {
        FLOAT x = points->x;
        FLOAT y = points->y;
        points->x = floor( x * dc->xformWorld2Vport.eM11 +
                           y * dc->xformWorld2Vport.eM21 +
                           dc->xformWorld2Vport.eDx + 0.5 );
        points->y = floor( x * dc->xformWorld2Vport.eM12 +
                           y * dc->xformWorld2Vport.eM22 +
                           dc->xformWorld2Vport.eDy + 0.5 );
        points++;
    }
    GDI_ReleaseObj( hdc );
    return TRUE;
}


/***********************************************************************
 *           SetMapMode    (GDI32.@)
 */
INT WINAPI SetMapMode( HDC hdc, INT mode )
{
    INT ret;
    INT horzSize, vertSize, horzRes, vertRes;

    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return 0;
    if (dc->funcs->pSetMapMode)
    {
        if((ret = dc->funcs->pSetMapMode( dc->physDev, mode )) != TRUE)
	{
	    if(ret == GDI_NO_MORE_WORK)
	        ret = TRUE;
	    goto done;
	}
    }

    TRACE("%p %d\n", hdc, mode );

    ret = dc->MapMode;

    if (mode == dc->MapMode && (mode == MM_ISOTROPIC || mode == MM_ANISOTROPIC))
        goto done;

    horzSize = GetDeviceCaps( hdc, HORZSIZE );
    vertSize = GetDeviceCaps( hdc, VERTSIZE );
    horzRes  = GetDeviceCaps( hdc, HORZRES );
    vertRes  = GetDeviceCaps( hdc, VERTRES );
    switch(mode)
    {
    case MM_TEXT:
        dc->wndExtX   = 1;
        dc->wndExtY   = 1;
        dc->vportExtX = 1;
        dc->vportExtY = 1;
        break;
    case MM_LOMETRIC:
    case MM_ISOTROPIC:
        dc->wndExtX   = horzSize * 10;
        dc->wndExtY   = vertSize * 10;
        dc->vportExtX = horzRes;
        dc->vportExtY = -vertRes;
        break;
    case MM_HIMETRIC:
        dc->wndExtX   = horzSize * 100;
        dc->wndExtY   = vertSize * 100;
        dc->vportExtX = horzRes;
        dc->vportExtY = -vertRes;
        break;
    case MM_LOENGLISH:
        dc->wndExtX   = MulDiv(1000, horzSize, 254);
        dc->wndExtY   = MulDiv(1000, vertSize, 254);
        dc->vportExtX = horzRes;
        dc->vportExtY = -vertRes;
        break;
    case MM_HIENGLISH:
        dc->wndExtX   = MulDiv(10000, horzSize, 254);
        dc->wndExtY   = MulDiv(10000, vertSize, 254);
        dc->vportExtX = horzRes;
        dc->vportExtY = -vertRes;
        break;
    case MM_TWIPS:
        dc->wndExtX   = MulDiv(14400, horzSize, 254);
        dc->wndExtY   = MulDiv(14400, vertSize, 254);
        dc->vportExtX = horzRes;
        dc->vportExtY = -vertRes;
        break;
    case MM_ANISOTROPIC:
        break;
    default:
        goto done;
    }
    dc->MapMode = mode;
    DC_UpdateXforms( dc );
 done:
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           SetViewportExtEx    (GDI32.@)
 */
BOOL WINAPI SetViewportExtEx( HDC hdc, INT x, INT y, LPSIZE size )
{
    INT ret = TRUE;
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return FALSE;
    if (dc->funcs->pSetViewportExt)
    {
        if((ret = dc->funcs->pSetViewportExt( dc->physDev, x, y )) != TRUE)
	{
	    if(ret == GDI_NO_MORE_WORK)
	        ret = TRUE;
	    goto done;
	}
    }
    if (size)
    {
	size->cx = dc->vportExtX;
	size->cy = dc->vportExtY;
    }
    if ((dc->MapMode != MM_ISOTROPIC) && (dc->MapMode != MM_ANISOTROPIC))
	goto done;
    if (!x || !y)
    {
        ret = FALSE;
        goto done;
    }
    dc->vportExtX = x;
    dc->vportExtY = y;
    if (dc->MapMode == MM_ISOTROPIC) MAPPING_FixIsotropic( dc );
    DC_UpdateXforms( dc );
 done:
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           SetViewportOrgEx    (GDI32.@)
 */
BOOL WINAPI SetViewportOrgEx( HDC hdc, INT x, INT y, LPPOINT pt )
{
    INT ret = TRUE;
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return FALSE;
    if (dc->funcs->pSetViewportOrg)
    {
        if((ret = dc->funcs->pSetViewportOrg( dc->physDev, x, y )) != TRUE)
	{
	    if(ret == GDI_NO_MORE_WORK)
	        ret = TRUE;
	    goto done;
	}
    }
    if (pt)
    {
        pt->x = dc->vportOrgX;
	pt->y = dc->vportOrgY;
    }
    dc->vportOrgX = x;
    dc->vportOrgY = y;
    DC_UpdateXforms( dc );

 done:
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           SetWindowExtEx    (GDI32.@)
 */
BOOL WINAPI SetWindowExtEx( HDC hdc, INT x, INT y, LPSIZE size )
{
    INT ret = TRUE;
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return FALSE;
    if (dc->funcs->pSetWindowExt)
    {
        if((ret = dc->funcs->pSetWindowExt( dc->physDev, x, y )) != TRUE)
	{
	    if(ret == GDI_NO_MORE_WORK)
	        ret = TRUE;
	    goto done;
	}
    }
    if (size)
    {
	size->cx = dc->wndExtX;
	size->cy = dc->wndExtY;
    }
    if ((dc->MapMode != MM_ISOTROPIC) && (dc->MapMode != MM_ANISOTROPIC))
	goto done;
    if (!x || !y)
    {
        ret = FALSE;
        goto done;
    }
    dc->wndExtX = x;
    dc->wndExtY = y;
    /* Windows fixes MM_ISOTROPIC mode only in SetViewportExtEx() */
    DC_UpdateXforms( dc );
 done:
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           SetWindowOrgEx    (GDI32.@)
 */
BOOL WINAPI SetWindowOrgEx( HDC hdc, INT x, INT y, LPPOINT pt )
{
    INT ret = TRUE;
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return FALSE;
    if (dc->funcs->pSetWindowOrg)
    {
        if((ret = dc->funcs->pSetWindowOrg( dc->physDev, x, y )) != TRUE)
	{
	    if(ret == GDI_NO_MORE_WORK)
	        ret = TRUE;
	    goto done;
	}
    }
    if (pt)
    {
        pt->x = dc->wndOrgX;
	pt->y = dc->wndOrgY;
    }
    dc->wndOrgX = x;
    dc->wndOrgY = y;
    DC_UpdateXforms( dc );
 done:
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           OffsetViewportOrgEx    (GDI32.@)
 */
BOOL WINAPI OffsetViewportOrgEx( HDC hdc, INT x, INT y, LPPOINT pt)
{
    INT ret = TRUE;
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return FALSE;
    if (dc->funcs->pOffsetViewportOrg)
    {
        if((ret = dc->funcs->pOffsetViewportOrg( dc->physDev, x, y )) != TRUE)
	{
	    if(ret == GDI_NO_MORE_WORK)
	        ret = TRUE;
	    goto done;
	}
    }
    if (pt)
    {
        pt->x = dc->vportOrgX;
	pt->y = dc->vportOrgY;
    }
    dc->vportOrgX += x;
    dc->vportOrgY += y;
    DC_UpdateXforms( dc );
 done:
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           OffsetWindowOrgEx    (GDI32.@)
 */
BOOL WINAPI OffsetWindowOrgEx( HDC hdc, INT x, INT y, LPPOINT pt )
{
    INT ret = TRUE;
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return FALSE;
    if (dc->funcs->pOffsetWindowOrg)
    {
        if((ret = dc->funcs->pOffsetWindowOrg( dc->physDev, x, y )) != TRUE)
	{
	    if(ret == GDI_NO_MORE_WORK)
	        ret = TRUE;
	    goto done;
	}
    }
    if (pt)
    {
        pt->x = dc->wndOrgX;
	pt->y = dc->wndOrgY;
    }
    dc->wndOrgX += x;
    dc->wndOrgY += y;
    DC_UpdateXforms( dc );
 done:
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           ScaleViewportExtEx    (GDI32.@)
 */
BOOL WINAPI ScaleViewportExtEx( HDC hdc, INT xNum, INT xDenom,
                                    INT yNum, INT yDenom, LPSIZE size )
{
    INT ret = TRUE;
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return FALSE;
    if (dc->funcs->pScaleViewportExt)
    {
        if((ret = dc->funcs->pScaleViewportExt( dc->physDev, xNum, xDenom, yNum, yDenom )) != TRUE)
	{
	    if(ret == GDI_NO_MORE_WORK)
	        ret = TRUE;
	    goto done;
	}
    }
    if (size)
    {
	size->cx = dc->vportExtX;
	size->cy = dc->vportExtY;
    }
    if ((dc->MapMode != MM_ISOTROPIC) && (dc->MapMode != MM_ANISOTROPIC))
	goto done;
    if (!xNum || !xDenom || !xNum || !yDenom)
    {
        ret = FALSE;
        goto done;
    }
    dc->vportExtX = (dc->vportExtX * xNum) / xDenom;
    dc->vportExtY = (dc->vportExtY * yNum) / yDenom;
    if (dc->vportExtX == 0) dc->vportExtX = 1;
    if (dc->vportExtY == 0) dc->vportExtY = 1;
    if (dc->MapMode == MM_ISOTROPIC) MAPPING_FixIsotropic( dc );
    DC_UpdateXforms( dc );
 done:
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           ScaleWindowExtEx    (GDI32.@)
 */
BOOL WINAPI ScaleWindowExtEx( HDC hdc, INT xNum, INT xDenom,
                                  INT yNum, INT yDenom, LPSIZE size )
{
    INT ret = TRUE;
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return FALSE;
    if (dc->funcs->pScaleWindowExt)
    {
        if((ret = dc->funcs->pScaleWindowExt( dc->physDev, xNum, xDenom, yNum, yDenom )) != TRUE)
	{
	    if(ret == GDI_NO_MORE_WORK)
	        ret = TRUE;
	    goto done;
	}
    }
    if (size)
    {
	size->cx = dc->wndExtX;
	size->cy = dc->wndExtY;
    }
    if ((dc->MapMode != MM_ISOTROPIC) && (dc->MapMode != MM_ANISOTROPIC))
	goto done;
    if (!xNum || !xDenom || !xNum || !yDenom)
    {
        ret = FALSE;
        goto done;
    }
    dc->wndExtX = (dc->wndExtX * xNum) / xDenom;
    dc->wndExtY = (dc->wndExtY * yNum) / yDenom;
    if (dc->wndExtX == 0) dc->wndExtX = 1;
    if (dc->wndExtY == 0) dc->wndExtY = 1;
    if (dc->MapMode == MM_ISOTROPIC) MAPPING_FixIsotropic( dc );
    DC_UpdateXforms( dc );
 done:
    GDI_ReleaseObj( hdc );
    return ret;
}
