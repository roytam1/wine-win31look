/*
 * Metafile GDI mapping mode functions
 *
 * Copyright 1996 Alexandre Julliard
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
#include "gdi_private.h"
#include "mfdrv/metafiledrv.h"


/***********************************************************************
 *           MFDRV_SetMapMode
 */
INT MFDRV_SetMapMode( PHYSDEV dev, INT mode )
{
    if(!MFDRV_MetaParam1( dev, META_SETMAPMODE, mode ))
        return FALSE;
    return GDI_NO_MORE_WORK;
}


/***********************************************************************
 *           MFDRV_SetViewportExt
 */
INT MFDRV_SetViewportExt( PHYSDEV dev, INT x, INT y )
{
    if(!MFDRV_MetaParam2( dev, META_SETVIEWPORTEXT, x, y ))
        return FALSE;
    return GDI_NO_MORE_WORK;
}


/***********************************************************************
 *           MFDRV_SetViewportOrg
 */
INT MFDRV_SetViewportOrg( PHYSDEV dev, INT x, INT y )
{
    if(!MFDRV_MetaParam2( dev, META_SETVIEWPORTORG, x, y ))
        return FALSE;
    return GDI_NO_MORE_WORK;
}


/***********************************************************************
 *           MFDRV_SetWindowExt
 */
INT MFDRV_SetWindowExt( PHYSDEV dev, INT x, INT y )
{
    if(!MFDRV_MetaParam2( dev, META_SETWINDOWEXT, x, y ))
        return FALSE;
    return GDI_NO_MORE_WORK;
}


/***********************************************************************
 *           MFDRV_SetWindowOrg
 */
INT MFDRV_SetWindowOrg( PHYSDEV dev, INT x, INT y )
{
    if(!MFDRV_MetaParam2( dev, META_SETWINDOWORG, x, y ))
        return FALSE;
    return GDI_NO_MORE_WORK;
}


/***********************************************************************
 *           MFDRV_OffsetViewportOrg
 */
INT MFDRV_OffsetViewportOrg( PHYSDEV dev, INT x, INT y )
{
    if(!MFDRV_MetaParam2( dev, META_OFFSETVIEWPORTORG, x, y ))
        return FALSE;
    return GDI_NO_MORE_WORK;
}


/***********************************************************************
 *           MFDRV_OffsetWindowOrg
 */
INT MFDRV_OffsetWindowOrg( PHYSDEV dev, INT x, INT y )
{
    if(!MFDRV_MetaParam2( dev, META_OFFSETWINDOWORG, x, y ))
        return FALSE;
    return GDI_NO_MORE_WORK;
}


/***********************************************************************
 *           MFDRV_ScaleViewportExt
 */
INT MFDRV_ScaleViewportExt( PHYSDEV dev, INT xNum, INT xDenom, INT yNum, INT yDenom )
{
    if(!MFDRV_MetaParam4( dev, META_SCALEVIEWPORTEXT, xNum, xDenom, yNum, yDenom ))
        return FALSE;
    return GDI_NO_MORE_WORK;
}


/***********************************************************************
 *           MFDRV_ScaleWindowExt
 */
INT MFDRV_ScaleWindowExt( PHYSDEV dev, INT xNum, INT xDenom, INT yNum, INT yDenom )
{
    if(!MFDRV_MetaParam4( dev, META_SCALEWINDOWEXT, xNum, xDenom, yNum, yDenom ))
        return FALSE;
    return GDI_NO_MORE_WORK;
}
