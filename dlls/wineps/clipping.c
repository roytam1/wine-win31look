/*
 *	PostScript clipping functions
 *
 *	Copyright 1999  Luc Tourangau
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

#include "psdrv.h"
#include "wine/debug.h"
#include "winbase.h"

WINE_DEFAULT_DEBUG_CHANNEL(psdrv);

/***********************************************************************
 *           PSDRV_SetDeviceClipping
 */
void PSDRV_SetDeviceClipping( PSDRV_PDEVICE *physDev, HRGN vis_rgn, HRGN clip_rgn )
{
    /* We could set a dirty flag here to speed up PSDRV_SetClip */
    return;
}

/***********************************************************************
 *           PSDRV_SetClip
 *
 * The idea here is that every graphics operation should bracket
 * output in PSDRV_SetClip/ResetClip calls.  The clip path outside
 * these calls will be empty; the reason for this is that it is
 * impossible in PostScript to cleanly make the clip path larger than
 * the current one.  Also Photoshop assumes that despite having set a
 * small clip area in the printer dc that it can still write raw
 * PostScript to the driver and expect this code not to be clipped.
 */
void PSDRV_SetClip( PSDRV_PDEVICE *physDev )
{
    CHAR szArrayName[] = "clippath";
    DWORD size;
    RGNDATA *rgndata = NULL;
    HRGN hrgn = CreateRectRgn(0,0,0,0);
    BOOL empty;

    TRACE("hdc=%p\n", physDev->hdc);

    empty = !GetClipRgn(physDev->hdc, hrgn);

    if(!empty) {
        size = GetRegionData(hrgn, 0, NULL);
        if(!size) {
            ERR("Invalid region\n");
            goto end;
        }

        rgndata = HeapAlloc( GetProcessHeap(), 0, size );
        if(!rgndata) {
            ERR("Can't allocate buffer\n");
            goto end;
        }

        GetRegionData(hrgn, size, rgndata);

        PSDRV_WriteGSave(physDev);

        /* check for NULL region */
        if (rgndata->rdh.nCount == 0)
        {
            /* set an empty clip path. */
            PSDRV_WriteRectClip(physDev, 0, 0, 0, 0);
        }
        /* optimize when it is a simple region */
        else if (rgndata->rdh.nCount == 1)
        {
            RECT *pRect = (RECT *)rgndata->Buffer;

            PSDRV_WriteRectClip(physDev, pRect->left, pRect->top,
                                pRect->right - pRect->left,
                                pRect->bottom - pRect->top);
        }
        else
        {
            INT i;
            RECT *pRect = (RECT *)rgndata->Buffer;

            PSDRV_WriteArrayDef(physDev, szArrayName, rgndata->rdh.nCount * 4);

            for (i = 0; i < rgndata->rdh.nCount; i++, pRect++)
            {
                PSDRV_WriteArrayPut(physDev, szArrayName, i * 4,
                                    pRect->left);
                PSDRV_WriteArrayPut(physDev, szArrayName, i * 4 + 1,
                                    pRect->top);
                PSDRV_WriteArrayPut(physDev, szArrayName, i * 4 + 2,
                                    pRect->right - pRect->left);
                PSDRV_WriteArrayPut(physDev, szArrayName, i * 4 + 3,
                                    pRect->bottom - pRect->top);
            }
            PSDRV_WriteRectClip2(physDev, szArrayName);
        }
    }
end:
    if(rgndata) HeapFree( GetProcessHeap(), 0, rgndata );
    DeleteObject(hrgn);
}


/***********************************************************************
 *           PSDRV_ResetClip
 */
void PSDRV_ResetClip( PSDRV_PDEVICE *physDev )
{
    HRGN hrgn = CreateRectRgn(0,0,0,0);
    BOOL empty;

     empty = !GetClipRgn(physDev->hdc, hrgn);
     if(!empty)
         PSDRV_WriteGRestore(physDev);
    DeleteObject(hrgn);
}
