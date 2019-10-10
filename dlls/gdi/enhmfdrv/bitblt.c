/*
 * Enhanced MetaFile driver BitBlt functions
 *
 * Copyright 2002 Huw D M Davies for CodeWeavers
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
#include "enhmetafiledrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(enhmetafile);

BOOL EMFDRV_PatBlt( PHYSDEV dev, INT left, INT top,
                    INT width, INT height, DWORD rop )
{
    EMRBITBLT emr;
    BOOL ret;

    emr.emr.iType = EMR_BITBLT;
    emr.emr.nSize = sizeof(emr);
    emr.rclBounds.left = left;
    emr.rclBounds.top = top;
    emr.rclBounds.right = left + width - 1;
    emr.rclBounds.bottom = top + height - 1;
    emr.xDest = left;
    emr.yDest = top;
    emr.cxDest = width;
    emr.cyDest = height;
    emr.dwRop = rop;
    emr.xSrc = 0;
    emr.ySrc = 0;
    emr.xformSrc.eM11 = 1.0;
    emr.xformSrc.eM12 = 0.0;
    emr.xformSrc.eM21 = 0.0;
    emr.xformSrc.eM22 = 1.0;
    emr.xformSrc.eDx = 0.0;
    emr.xformSrc.eDy = 0.0;
    emr.crBkColorSrc = 0;
    emr.iUsageSrc = 0;
    emr.offBmiSrc = 0;
    emr.cbBmiSrc = 0;
    emr.offBitsSrc = 0;
    emr.cbBitsSrc = 0;

    ret = EMFDRV_WriteRecord( dev, &emr.emr );
    if(ret)
        EMFDRV_UpdateBBox( dev, &emr.rclBounds );
    return ret;
}

/* Utilitarian function used by EMFDRV_BitBlt and EMFDRV_StretchBlt */

static BOOL EMFDRV_BitBlockTransfer( 
    PHYSDEV devDst, INT xDst, INT yDst, INT widthDst, INT heightDst,  
    PHYSDEV devSrc, INT xSrc, INT ySrc, INT widthSrc, INT heightSrc, DWORD rop,
    DWORD emrType)
{
    BOOL ret;
    PEMRBITBLT pEMR;
    UINT emrSize;
    UINT bmiSize;
    UINT bitsSize;
    UINT size;
    BITMAP  BM;
    WORD nBPP;
    LPBITMAPINFOHEADER lpBmiH;
    EMFDRV_PDEVICE* physDevSrc = (EMFDRV_PDEVICE*)devSrc;
    HBITMAP hBitmap = GetCurrentObject(physDevSrc->hdc, OBJ_BITMAP);

    if (emrType == EMR_BITBLT)
        emrSize = sizeof(EMRBITBLT);
    else if (emrType == EMR_STRETCHBLT)
        emrSize = sizeof(EMRSTRETCHBLT);
    else
        return FALSE;

    GetObjectW(hBitmap, sizeof(BITMAP), &BM);

    nBPP = BM.bmPlanes * BM.bmBitsPixel;
    if(nBPP > 8) nBPP = 24; /* FIXME Can't get 16bpp to work for some reason */

    bitsSize = DIB_GetDIBWidthBytes(BM.bmWidth, nBPP) * BM.bmHeight;
    bmiSize = sizeof(BITMAPINFOHEADER) + 
        (nBPP <= 8 ? 1 << nBPP : 0) * sizeof(RGBQUAD);
    size = emrSize + bmiSize + bitsSize;

    pEMR = HeapAlloc(GetProcessHeap(), 0, size);
    if (!pEMR) return FALSE;

    /* Initialize EMR */
    pEMR->emr.iType = emrType;
    pEMR->emr.nSize = size;
    pEMR->rclBounds.left = xDst;
    pEMR->rclBounds.top = yDst;
    pEMR->rclBounds.right = xDst + widthDst - 1;
    pEMR->rclBounds.bottom = yDst + heightDst - 1;
    pEMR->xDest = xDst;
    pEMR->yDest = yDst;
    pEMR->cxDest = widthDst;
    pEMR->cyDest = heightDst;
    pEMR->dwRop = rop;
    pEMR->xSrc = xSrc;
    pEMR->ySrc = ySrc;
    pEMR->xformSrc.eM11 = 1.0;  /** FIXME:           */
    pEMR->xformSrc.eM12 = 0.0;  /** Setting default  */
    pEMR->xformSrc.eM21 = 0.0;  /** value.           */
    pEMR->xformSrc.eM22 = 1.0;  /** Where should we  */
    pEMR->xformSrc.eDx = 0.0;   /** get that info    */
    pEMR->xformSrc.eDy = 0.0;   /** ????             */
    pEMR->crBkColorSrc = GetBkColor(physDevSrc->hdc);
    pEMR->iUsageSrc = DIB_RGB_COLORS;
    pEMR->offBmiSrc = emrSize;
    pEMR->cbBmiSrc = bmiSize;
    pEMR->offBitsSrc = emrSize + bmiSize;
    pEMR->cbBitsSrc = bitsSize;
    if (emrType == EMR_STRETCHBLT) 
    {
        PEMRSTRETCHBLT pEMRStretch = (PEMRSTRETCHBLT)pEMR;
        pEMRStretch->cxSrc = widthSrc;
        pEMRStretch->cySrc = heightSrc;
    }

    /* Initialize BITMAPINFO structure */
    lpBmiH = (LPBITMAPINFOHEADER)((BYTE*)pEMR + pEMR->offBmiSrc);

    lpBmiH->biSize = sizeof(BITMAPINFOHEADER); 
    lpBmiH->biWidth =  BM.bmWidth;
    lpBmiH->biHeight = BM.bmHeight;
    lpBmiH->biPlanes = BM.bmPlanes;
    lpBmiH->biBitCount = nBPP;
    /* Assume the bitmap isn't compressed and set the BI_RGB flag. */
    lpBmiH->biCompression = BI_RGB;
    lpBmiH->biSizeImage = bitsSize;
    lpBmiH->biYPelsPerMeter = /* 1 meter  = 39.37 inch */
        MulDiv(GetDeviceCaps(physDevSrc->hdc,LOGPIXELSX),3937,100);
    lpBmiH->biXPelsPerMeter = 
        MulDiv(GetDeviceCaps(physDevSrc->hdc,LOGPIXELSY),3937,100);
    lpBmiH->biClrUsed   = nBPP <= 8 ? 1 << nBPP : 0;
    /* Set biClrImportant to 0, indicating that all of the 
       device colors are important. */
    lpBmiH->biClrImportant = 0; 

    /* Initiliaze bitmap bits */
    if (GetDIBits(physDevSrc->hdc, hBitmap, 0, (UINT)lpBmiH->biHeight,
                  (BYTE*)pEMR + pEMR->offBitsSrc,
                  (LPBITMAPINFO)lpBmiH, DIB_RGB_COLORS))
    {
        ret = EMFDRV_WriteRecord(devDst, (EMR*)pEMR);
        if (ret) EMFDRV_UpdateBBox(devDst, &(pEMR->rclBounds));
    } 
    else
        ret = FALSE;

    HeapFree( GetProcessHeap(), 0, pEMR);
    return ret;
}

BOOL EMFDRV_BitBlt( 
    PHYSDEV devDst, INT xDst, INT yDst, INT width, INT height,
    PHYSDEV devSrc, INT xSrc, INT ySrc, DWORD rop)
{
    return EMFDRV_BitBlockTransfer( devDst, xDst, yDst, width, height,  
                                    devSrc, xSrc, ySrc, width, height, 
                                    rop, EMR_BITBLT );
}

BOOL EMFDRV_StretchBlt( 
    PHYSDEV devDst, INT xDst, INT yDst, INT widthDst, INT heightDst,  
    PHYSDEV devSrc, INT xSrc, INT ySrc, INT widthSrc, INT heightSrc, DWORD rop )
{
    return EMFDRV_BitBlockTransfer( devDst, xDst, yDst, widthDst, heightDst,  
                                    devSrc, xSrc, ySrc, widthSrc, heightSrc, 
                                    rop, EMR_STRETCHBLT );
}

INT EMFDRV_StretchDIBits( PHYSDEV dev, INT xDst, INT yDst, INT widthDst,
                                      INT heightDst, INT xSrc, INT ySrc,
                                      INT widthSrc, INT heightSrc,
                                      const void *bits, const BITMAPINFO *info,
                                      UINT wUsage, DWORD dwRop )
{
    EMRSTRETCHDIBITS *emr;
    BOOL ret;
    UINT bmi_size=0, bits_size, emr_size;
    
    bits_size = DIB_GetDIBImageBytes(info->bmiHeader.biWidth,
                                     info->bmiHeader.biHeight,
                                     info->bmiHeader.biBitCount);

    /* calculate the size of the colour table */
    bmi_size = DIB_BitmapInfoSize(info, wUsage);

    emr_size = sizeof (EMRSTRETCHDIBITS) + bmi_size + bits_size;
    emr = HeapAlloc(GetProcessHeap(), 0, emr_size );
    if (!emr) return 0;

    /* write a bitmap info header (with colours) to the record */
    memcpy( &emr[1], info, bmi_size);

    /* write bitmap bits to the record */
    memcpy ( ( (BYTE *) (&emr[1]) ) + bmi_size, bits, bits_size);

    /* fill in the EMR header at the front of our piece of memory */
    emr->emr.iType = EMR_STRETCHDIBITS;
    emr->emr.nSize = emr_size;

    emr->xDest     = xDst;
    emr->yDest     = yDst;
    emr->cxDest    = widthDst;
    emr->cyDest    = heightDst;
    emr->dwRop     = dwRop;
    emr->xSrc      = xSrc; /* FIXME: only save the piece of the bitmap needed */
    emr->ySrc      = ySrc;

    emr->iUsageSrc    = wUsage;
    emr->offBmiSrc    = sizeof (EMRSTRETCHDIBITS);
    emr->cbBmiSrc     = bmi_size;
    emr->offBitsSrc   = emr->offBmiSrc + bmi_size; 
    emr->cbBitsSrc    = bits_size;

    emr->cxSrc = widthSrc;
    emr->cySrc = heightSrc;

    emr->rclBounds.left   = xDst;
    emr->rclBounds.top    = yDst;
    emr->rclBounds.right  = xDst + widthDst;
    emr->rclBounds.bottom = yDst + heightDst;

    /* save the record we just created */
    ret = EMFDRV_WriteRecord( dev, &emr->emr );
    if(ret)
        EMFDRV_UpdateBBox( dev, &emr->rclBounds );

    HeapFree(GetProcessHeap(), 0, emr);

    return ret;
}

INT EMFDRV_SetDIBitsToDevice( 
    PHYSDEV dev, INT xDst, INT yDst, DWORD width, DWORD height,
    INT xSrc, INT ySrc, UINT startscan, UINT lines,
    LPCVOID bits, const BITMAPINFO *info, UINT wUsage )
{
    EMRSETDIBITSTODEVICE* pEMR;
    DWORD size, bmiSize, bitsSize;

    bmiSize = DIB_BitmapInfoSize(info, wUsage);
    bitsSize = DIB_GetDIBImageBytes( info->bmiHeader.biWidth,
                                     info->bmiHeader.biHeight,
                                     info->bmiHeader.biBitCount );
    size = sizeof(EMRSETDIBITSTODEVICE) + bmiSize + bitsSize;

    pEMR = HeapAlloc(GetProcessHeap(), 0, size);
    if (!pEMR) return 0;

    pEMR->emr.iType = EMR_SETDIBITSTODEVICE;
    pEMR->emr.nSize = size;
    pEMR->rclBounds.left = xDst;
    pEMR->rclBounds.top = yDst;
    pEMR->rclBounds.right = xDst + width - 1;
    pEMR->rclBounds.bottom = yDst + height - 1;
    pEMR->xDest = xDst;
    pEMR->yDest = yDst;
    pEMR->xSrc = xSrc;
    pEMR->ySrc = ySrc;
    pEMR->cxSrc = width;
    pEMR->cySrc = height;
    pEMR->offBmiSrc = sizeof(EMRSETDIBITSTODEVICE);
    pEMR->cbBmiSrc = bmiSize;
    pEMR->offBitsSrc = sizeof(EMRSETDIBITSTODEVICE) + bmiSize;
    pEMR->cbBitsSrc = bitsSize;
    pEMR->iUsageSrc = wUsage;
    pEMR->iStartScan = startscan;
    pEMR->cScans = lines;
    memcpy((BYTE*)pEMR + pEMR->offBmiSrc, info, bmiSize);
    memcpy((BYTE*)pEMR + pEMR->offBitsSrc, bits, bitsSize);

    if (EMFDRV_WriteRecord(dev, (EMR*)pEMR))
        EMFDRV_UpdateBBox(dev, &(pEMR->rclBounds));

    HeapFree( GetProcessHeap(), 0, pEMR);
    return lines;
}
