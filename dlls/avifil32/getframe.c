/*
 * Copyright 2002-2003 Michael G�nnewig
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

#define COM_NO_WINDOWS_H
#include <assert.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "windowsx.h"
#include "wingdi.h"
#include "winuser.h"
#include "vfw.h"

#include "avifile_private.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(avifile);

#ifndef DIBPTR
#define DIBPTR(lp)      ((LPBYTE)(lp) + (lp)->biSize + \
                         (lp)->biClrUsed * sizeof(RGBQUAD))
#endif

/***********************************************************************/

static HRESULT WINAPI IGetFrame_fnQueryInterface(IGetFrame *iface,
						 REFIID refiid, LPVOID *obj);
static ULONG   WINAPI IGetFrame_fnAddRef(IGetFrame *iface);
static ULONG   WINAPI IGetFrame_fnRelease(IGetFrame *iface);
static LPVOID  WINAPI IGetFrame_fnGetFrame(IGetFrame *iface, LONG lPos);
static HRESULT WINAPI IGetFrame_fnBegin(IGetFrame *iface, LONG lStart,
					LONG lEnd, LONG lRate);
static HRESULT WINAPI IGetFrame_fnEnd(IGetFrame *iface);
static HRESULT WINAPI IGetFrame_fnSetFormat(IGetFrame *iface,
					    LPBITMAPINFOHEADER lpbi,
					    LPVOID lpBits, INT x, INT y,
					    INT dx, INT dy);

struct ICOM_VTABLE(IGetFrame) igetframeVtbl = {
  ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
  IGetFrame_fnQueryInterface,
  IGetFrame_fnAddRef,
  IGetFrame_fnRelease,
  IGetFrame_fnGetFrame,
  IGetFrame_fnBegin,
  IGetFrame_fnEnd,
  IGetFrame_fnSetFormat
};

typedef struct _IGetFrameImpl {
  /* IUnknown stuff */
  ICOM_VFIELD(IGetFrame);
  DWORD              ref;

  /* IGetFrame stuff */
  BOOL               bFixedStream;
  PAVISTREAM         pStream;

  LPVOID             lpInBuffer;
  LONG               cbInBuffer;
  LPBITMAPINFOHEADER lpInFormat;
  LONG               cbInFormat;

  LONG               lCurrentFrame;
  LPBITMAPINFOHEADER lpOutFormat;
  LPVOID             lpOutBuffer;

  HIC                hic;
  BOOL               bResize;
  DWORD              x;
  DWORD              y;
  DWORD              dx;
  DWORD              dy;

  BOOL               bFormatChanges;
  DWORD              dwFormatChangeCount;
  DWORD              dwEditCount;
} IGetFrameImpl;

/***********************************************************************/

static void AVIFILE_CloseCompressor(IGetFrameImpl *This)
{
  if (This->lpOutFormat != NULL && This->lpInFormat != This->lpOutFormat) {
    GlobalFreePtr(This->lpOutFormat);
    This->lpOutFormat = NULL;
  }
  if (This->lpInFormat != NULL) {
    GlobalFreePtr(This->lpInFormat);
    This->lpInFormat = NULL;
  }
  if (This->hic != NULL) {
    if (This->bResize)
      ICDecompressExEnd(This->hic);
    else
      ICDecompressEnd(This->hic);
    ICClose(This->hic);
    This->hic = NULL;
  }
}

PGETFRAME AVIFILE_CreateGetFrame(PAVISTREAM pStream)
{
  IGetFrameImpl *pg;

  /* check parameter */
  if (pStream == NULL)
    return NULL;

  pg = (IGetFrameImpl*)LocalAlloc(LPTR, sizeof(IGetFrameImpl));
  if (pg != NULL) {
    pg->lpVtbl        = &igetframeVtbl;
    pg->ref           = 1;
    pg->lCurrentFrame = -1;
    pg->pStream       = pStream;
    IAVIStream_AddRef(pStream);
  }

  return (PGETFRAME)pg;
}

static HRESULT WINAPI IGetFrame_fnQueryInterface(IGetFrame *iface,
						 REFIID refiid, LPVOID *obj)
{
  ICOM_THIS(IGetFrameImpl,iface);

  TRACE("(%p,%s,%p)\n", This, debugstr_guid(refiid), obj);

  if (IsEqualGUID(&IID_IUnknown, refiid) ||
      IsEqualGUID(&IID_IGetFrame, refiid)) {
    *obj = iface;
    return S_OK;
  }

  return OLE_E_ENUM_NOMORE;
}

static ULONG   WINAPI IGetFrame_fnAddRef(IGetFrame *iface)
{
  ICOM_THIS(IGetFrameImpl,iface);

  TRACE("(%p)\n", iface);

  return ++(This->ref);
}

static ULONG   WINAPI IGetFrame_fnRelease(IGetFrame *iface)
{
  ICOM_THIS(IGetFrameImpl,iface);

  TRACE("(%p)\n", iface);

  if (!--(This->ref)) {
    AVIFILE_CloseCompressor(This);
    if (This->pStream != NULL) {
      IAVIStream_Release(This->pStream);
      This->pStream = NULL;
    }

    LocalFree((HLOCAL)iface);
    return 0;
  }

  return This->ref;
}

static LPVOID  WINAPI IGetFrame_fnGetFrame(IGetFrame *iface, LONG lPos)
{
  ICOM_THIS(IGetFrameImpl,iface);

  LONG readBytes;
  LONG readSamples;

  TRACE("(%p,%ld)\n", iface, lPos);

  /* We don't want negative start values! -- marks invalid buffer content */
  if (lPos < 0)
    return NULL;

  /* check state */
  if (This->pStream == NULL)
    return NULL;
  if (This->lpInFormat == NULL)
    return NULL;

  /* Could stream have changed? */
  if (! This->bFixedStream) {
    AVISTREAMINFOW sInfo;

    IAVIStream_Info(This->pStream, &sInfo, sizeof(sInfo));

    if (sInfo.dwEditCount != This->dwEditCount) {
      This->dwEditCount   = sInfo.dwEditCount;
      This->lCurrentFrame = -1;
    }

    if (sInfo.dwFormatChangeCount != This->dwFormatChangeCount) {
      /* stream has changed */
      if (This->lpOutFormat != NULL) {
	BITMAPINFOHEADER bi;

	memcpy(&bi, This->lpOutFormat, sizeof(bi));
	AVIFILE_CloseCompressor(This);

	if (FAILED(IGetFrame_SetFormat(iface, &bi, NULL, 0, 0, -1, -1))) {
	  if (FAILED(IGetFrame_SetFormat(iface, NULL, NULL, 0, 0, -1, -1)))
	    return NULL;
	}
      } else if (FAILED(IGetFrame_SetFormat(iface, NULL, NULL, 0, 0, -1, -1)))
	return NULL;
    }
  }

  if (lPos != This->lCurrentFrame) {
    LONG lNext = IAVIStream_FindSample(This->pStream,lPos,FIND_KEY|FIND_PREV);

    if (lNext == -1)
      return NULL; /* frame doesn't exist */
    if (lNext <= This->lCurrentFrame && This->lCurrentFrame < lPos)
      lNext = This->lCurrentFrame + 1;

    for (; lNext <= lPos; lNext++) {
      /* new format for this frame? */
      if (This->bFormatChanges) {
	IAVIStream_ReadFormat(This->pStream, lNext,
			      This->lpInFormat, &This->cbInFormat);
	if (This->lpOutFormat != NULL) {
	  if (This->lpOutFormat->biBitCount <= 8)
	    ICDecompressGetPalette(This->hic, This->lpInFormat,
				   This->lpOutFormat);
	}
      }

      /* read input frame */
      while (FAILED(AVIStreamRead(This->pStream, lNext, 1, This->lpInBuffer,
				  This->cbInBuffer, &readBytes, &readSamples))) {
	/* not enough memory for input buffer? */
	readBytes = 0;
	if (FAILED(AVIStreamSampleSize(This->pStream, lNext, &readBytes)))
	  return NULL; /* bad thing, but bad things will happen */
	if (readBytes <= 0) {
	  ERR(": IAVIStream::REad doesn't return needed bytes!\n");
	  return NULL;
	}

	/* IAVIStream::Read failed because of other reasons not buffersize? */
	if (This->cbInBuffer >= readBytes)
	  break;
	This->cbInBuffer = This->cbInFormat + readBytes;
	This->lpInFormat = GlobalReAllocPtr(This->lpInFormat, This->cbInBuffer, 0);
	if (This->lpInFormat == NULL)
	  return NULL; /* out of memory */
	This->lpInBuffer = (BYTE*)This->lpInFormat + This->cbInFormat;
      }

      if (readSamples != 1) {
	ERR(": no frames read\n");
	return NULL;
      }
      if (readBytes != 0) {
	This->lpInFormat->biSizeImage = readBytes;

	/* nothing to decompress? */
	if (This->hic == NULL) {
	  This->lCurrentFrame = lPos;
	  return This->lpInFormat;
	}

	if (This->bResize) {
	  ICDecompressEx(This->hic,0,This->lpInFormat,This->lpInBuffer,0,0,
			 This->lpInFormat->biWidth,This->lpInFormat->biHeight,
			 This->lpOutFormat,This->lpOutBuffer,This->x,This->y,
			 This->dx,This->dy);
	} else {
	  ICDecompress(This->hic, 0, This->lpInFormat, This->lpInBuffer,
		       This->lpOutFormat, This->lpOutBuffer);
	}
      }
    } /* for (lNext < lPos) */
  } /* if (This->lCurrentFrame != lPos) */

  return (This->hic == NULL ? This->lpInFormat : This->lpOutFormat);
}

static HRESULT WINAPI IGetFrame_fnBegin(IGetFrame *iface, LONG lStart,
					LONG lEnd, LONG lRate)
{
  ICOM_THIS(IGetFrameImpl,iface);

  TRACE("(%p,%ld,%ld,%ld)\n", iface, lStart, lEnd, lRate);

  This->bFixedStream = TRUE;

  return (IGetFrame_GetFrame(iface, lStart) ? AVIERR_OK : AVIERR_ERROR);
}

static HRESULT WINAPI IGetFrame_fnEnd(IGetFrame *iface)
{
  ICOM_THIS(IGetFrameImpl,iface);

  TRACE("(%p)\n", iface);

  This->bFixedStream = FALSE;

  return AVIERR_OK;
}

static HRESULT WINAPI IGetFrame_fnSetFormat(IGetFrame *iface,
					    LPBITMAPINFOHEADER lpbiWanted,
					    LPVOID lpBits, INT x, INT y,
					    INT dx, INT dy)
{
  ICOM_THIS(IGetFrameImpl,iface);

  AVISTREAMINFOW     sInfo;
  LPBITMAPINFOHEADER lpbi         = lpbiWanted;
  BOOL               bBestDisplay = FALSE;

  TRACE("(%p,%p,%p,%d,%d,%d,%d)\n", iface, lpbiWanted, lpBits,
	x, y, dx, dy);

  if (This->pStream == NULL)
    return AVIERR_ERROR;

  if ((LONG)lpbiWanted == AVIGETFRAMEF_BESTDISPLAYFMT) {
    lpbi = NULL;
    bBestDisplay = TRUE;
  }

  IAVIStream_Info(This->pStream, &sInfo, sizeof(sInfo));
  if (sInfo.fccType != streamtypeVIDEO)
    return AVIERR_UNSUPPORTED;

  This->bFormatChanges =
    (sInfo.dwFlags & AVISTREAMINFO_FORMATCHANGES ? TRUE : FALSE );
  This->dwFormatChangeCount = sInfo.dwFormatChangeCount;
  This->dwEditCount         = sInfo.dwEditCount;
  This->lCurrentFrame       = -1;

  /* get input format from stream */
  if (This->lpInFormat == NULL) {
    HRESULT hr;

    This->cbInBuffer = (LONG)sInfo.dwSuggestedBufferSize;
    if (This->cbInBuffer == 0)
      This->cbInBuffer = 1024;

    IAVIStream_ReadFormat(This->pStream, sInfo.dwStart,
			  NULL, &This->cbInFormat);

    This->lpInFormat =
      (LPBITMAPINFOHEADER)GlobalAllocPtr(GHND, This->cbInFormat + This->cbInBuffer);
    if (This->lpInFormat == NULL) {
      AVIFILE_CloseCompressor(This);
      return AVIERR_MEMORY;
    }

    hr = IAVIStream_ReadFormat(This->pStream, sInfo.dwStart, This->lpInFormat, &This->cbInFormat);
    if (FAILED(hr)) {
      AVIFILE_CloseCompressor(This);
      return hr;
    }

    This->lpInBuffer = ((LPBYTE)This->lpInFormat) + This->cbInFormat;
  }

  /* check input format */
  if (This->lpInFormat->biClrUsed == 0 && This->lpInFormat->biBitCount <= 8)
    This->lpInFormat->biClrUsed = 1u << This->lpInFormat->biBitCount;
  if (This->lpInFormat->biSizeImage == 0 &&
      This->lpInFormat->biCompression == BI_RGB) {
    This->lpInFormat->biSizeImage =
      DIBWIDTHBYTES(*This->lpInFormat) * This->lpInFormat->biHeight;
  }

  /* only to pass through? */
  if (This->lpInFormat->biCompression == BI_RGB && lpBits == NULL) {
    if (lpbi == NULL || 
	(lpbi->biCompression == BI_RGB &&
	 lpbi->biWidth == This->lpInFormat->biWidth &&
	 lpbi->biHeight == This->lpInFormat->biHeight &&
	 lpbi->biBitCount == This->lpInFormat->biBitCount)) {
      This->lpOutFormat = This->lpInFormat;
      This->lpOutBuffer = DIBPTR(This->lpInFormat);
      return AVIERR_OK;
    }
  }

  /* need memory for output format? */
  if (This->lpOutFormat == NULL) {
    This->lpOutFormat =
      (LPBITMAPINFOHEADER)GlobalAllocPtr(GHND, sizeof(BITMAPINFOHEADER)
					 + 256 * sizeof(RGBQUAD));
    if (This->lpOutFormat == NULL) {
      AVIFILE_CloseCompressor(This);
      return AVIERR_MEMORY;
    }
  }

  /* need handle to video compressor */
  if (This->hic == NULL) {
    FOURCC fccHandler;

    if (This->lpInFormat->biCompression == BI_RGB)
      fccHandler = comptypeDIB;
    else if (This->lpInFormat->biCompression == BI_RLE8)
      fccHandler = mmioFOURCC('R','L','E',' ');
    else
      fccHandler = sInfo.fccHandler;

    if (lpbi != NULL) {
      if (lpbi->biWidth == 0)
	lpbi->biWidth = This->lpInFormat->biWidth;
      if (lpbi->biHeight == 0)
	lpbi->biHeight = This->lpInFormat->biHeight;
    }

    This->hic = ICLocate(ICTYPE_VIDEO, fccHandler, This->lpInFormat, lpbi, ICMODE_DECOMPRESS);
    if (This->hic == NULL) {
      AVIFILE_CloseCompressor(This);
      return AVIERR_NOCOMPRESSOR;
    }
  }

  /* output format given? */
  if (lpbi != NULL) {
    /* check the given output format ... */
    if (lpbi->biClrUsed == 0 && lpbi->biBitCount <= 8)
      lpbi->biClrUsed = 1u << lpbi->biBitCount;

    /* ... and remember it */
    memcpy(This->lpOutFormat, lpbi,
	   lpbi->biSize + lpbi->biClrUsed * sizeof(RGBQUAD));
    if (lpbi->biBitCount <= 8)
      ICDecompressGetPalette(This->hic, This->lpInFormat, This->lpOutFormat);

    return AVIERR_OK;
  } else {
    if (bBestDisplay) {
      ICGetDisplayFormat(This->hic, This->lpInFormat,
			 This->lpOutFormat, 0, dx, dy);
    } else if (ICDecompressGetFormat(This->hic, This->lpInFormat,
				     This->lpOutFormat) < 0) {
      AVIFILE_CloseCompressor(This);
      return AVIERR_NOCOMPRESSOR;
    }

    /* check output format */
    if (This->lpOutFormat->biClrUsed == 0 &&
	This->lpOutFormat->biBitCount <= 8)
      This->lpOutFormat->biClrUsed = 1u << This->lpOutFormat->biBitCount;
    if (This->lpOutFormat->biSizeImage == 0 &&
	This->lpOutFormat->biCompression == BI_RGB) {
      This->lpOutFormat->biSizeImage =
	DIBWIDTHBYTES(*This->lpOutFormat) * This->lpOutFormat->biHeight;
    }

    if (lpBits == NULL) {
      register DWORD size = This->lpOutFormat->biClrUsed * sizeof(RGBQUAD);

      size += This->lpOutFormat->biSize + This->lpOutFormat->biSizeImage;
      This->lpOutFormat =
	(LPBITMAPINFOHEADER)GlobalReAllocPtr(This->lpOutFormat, size, GMEM_MOVEABLE);
      if (This->lpOutFormat == NULL) {
	AVIFILE_CloseCompressor(This);
	return AVIERR_MEMORY;
      }
      This->lpOutBuffer = DIBPTR(This->lpOutFormat);
    } else
      This->lpOutBuffer = lpBits;

    /* for user size was irrelevant */
    if (dx == -1)
      dx = This->lpOutFormat->biWidth;
    if (dy == -1)
      dy = This->lpOutFormat->biHeight;

    /* need to resize? */
    if (x != 0 || y != 0) {
      if (dy == This->lpOutFormat->biHeight &&
	  dx == This->lpOutFormat->biWidth)
	This->bResize = FALSE;
      else
	This->bResize = TRUE;
    }

    if (This->bResize) {
      This->x  = x;
      This->y  = y;
      This->dx = dx;
      This->dy = dy;

      if (ICDecompressExBegin(This->hic,0,This->lpInFormat,This->lpInBuffer,0,
			      0,This->lpInFormat->biWidth,
			      This->lpInFormat->biHeight,This->lpOutFormat,
			      This->lpOutBuffer, x, y, dx, dy) == ICERR_OK)
	return AVIERR_OK;
    } else if (ICDecompressBegin(This->hic, This->lpInFormat,
				 This->lpOutFormat) == ICERR_OK)
      return AVIERR_OK;

    AVIFILE_CloseCompressor(This);

    return AVIERR_COMPRESSOR;
  }
}

/***********************************************************************/
