/*
 * Copyright 1999 Marcus Meissner
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

/* TODO:
 *  - IAVIStreaming interface is missing for the IAVIStreamImpl
 *  - IAVIStream_fnFindSample: FIND_INDEX isn't supported.
 *  - IAVIStream_fnReadFormat: formatchanges aren't read in.
 *  - IAVIStream_fnDelete: a stub.
 *  - IAVIStream_fnSetInfo: a stub.
 *  - make thread safe
 *
 * KNOWN Bugs:
 *  - native version can hangup when reading a file generated with this DLL.
 *    When index is missing it works, but index seems to be okay.
 */

#define COM_NO_WINDOWS_H
#include <assert.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winnls.h"
#include "winerror.h"
#include "windowsx.h"
#include "mmsystem.h"
#include "vfw.h"

#include "avifile_private.h"
#include "extrachunk.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(avifile);

#ifndef IDX_PER_BLOCK
#define IDX_PER_BLOCK 2730
#endif

/***********************************************************************/

static HRESULT WINAPI IAVIFile_fnQueryInterface(IAVIFile* iface,REFIID refiid,LPVOID *obj);
static ULONG   WINAPI IAVIFile_fnAddRef(IAVIFile* iface);
static ULONG   WINAPI IAVIFile_fnRelease(IAVIFile* iface);
static HRESULT WINAPI IAVIFile_fnInfo(IAVIFile*iface,AVIFILEINFOW*afi,LONG size);
static HRESULT WINAPI IAVIFile_fnGetStream(IAVIFile*iface,PAVISTREAM*avis,DWORD fccType,LONG lParam);
static HRESULT WINAPI IAVIFile_fnCreateStream(IAVIFile*iface,PAVISTREAM*avis,AVISTREAMINFOW*asi);
static HRESULT WINAPI IAVIFile_fnWriteData(IAVIFile*iface,DWORD ckid,LPVOID lpData,LONG size);
static HRESULT WINAPI IAVIFile_fnReadData(IAVIFile*iface,DWORD ckid,LPVOID lpData,LONG *size);
static HRESULT WINAPI IAVIFile_fnEndRecord(IAVIFile*iface);
static HRESULT WINAPI IAVIFile_fnDeleteStream(IAVIFile*iface,DWORD fccType,LONG lParam);

struct ICOM_VTABLE(IAVIFile) iavift = {
  ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
  IAVIFile_fnQueryInterface,
  IAVIFile_fnAddRef,
  IAVIFile_fnRelease,
  IAVIFile_fnInfo,
  IAVIFile_fnGetStream,
  IAVIFile_fnCreateStream,
  IAVIFile_fnWriteData,
  IAVIFile_fnReadData,
  IAVIFile_fnEndRecord,
  IAVIFile_fnDeleteStream
};

static HRESULT WINAPI IPersistFile_fnQueryInterface(IPersistFile*iface,REFIID refiid,LPVOID*obj);
static ULONG   WINAPI IPersistFile_fnAddRef(IPersistFile*iface);
static ULONG   WINAPI IPersistFile_fnRelease(IPersistFile*iface);
static HRESULT WINAPI IPersistFile_fnGetClassID(IPersistFile*iface,CLSID*pClassID);
static HRESULT WINAPI IPersistFile_fnIsDirty(IPersistFile*iface);
static HRESULT WINAPI IPersistFile_fnLoad(IPersistFile*iface,LPCOLESTR pszFileName,DWORD dwMode);
static HRESULT WINAPI IPersistFile_fnSave(IPersistFile*iface,LPCOLESTR pszFileName,BOOL fRemember);
static HRESULT WINAPI IPersistFile_fnSaveCompleted(IPersistFile*iface,LPCOLESTR pszFileName);
static HRESULT WINAPI IPersistFile_fnGetCurFile(IPersistFile*iface,LPOLESTR*ppszFileName);

struct ICOM_VTABLE(IPersistFile) ipersistft = {
  ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
  IPersistFile_fnQueryInterface,
  IPersistFile_fnAddRef,
  IPersistFile_fnRelease,
  IPersistFile_fnGetClassID,
  IPersistFile_fnIsDirty,
  IPersistFile_fnLoad,
  IPersistFile_fnSave,
  IPersistFile_fnSaveCompleted,
  IPersistFile_fnGetCurFile
};

static HRESULT WINAPI IAVIStream_fnQueryInterface(IAVIStream*iface,REFIID refiid,LPVOID *obj);
static ULONG   WINAPI IAVIStream_fnAddRef(IAVIStream*iface);
static ULONG   WINAPI IAVIStream_fnRelease(IAVIStream* iface);
static HRESULT WINAPI IAVIStream_fnCreate(IAVIStream*iface,LPARAM lParam1,LPARAM lParam2);
static HRESULT WINAPI IAVIStream_fnInfo(IAVIStream*iface,AVISTREAMINFOW *psi,LONG size);
static LONG    WINAPI IAVIStream_fnFindSample(IAVIStream*iface,LONG pos,LONG flags);
static HRESULT WINAPI IAVIStream_fnReadFormat(IAVIStream*iface,LONG pos,LPVOID format,LONG *formatsize);
static HRESULT WINAPI IAVIStream_fnSetFormat(IAVIStream*iface,LONG pos,LPVOID format,LONG formatsize);
static HRESULT WINAPI IAVIStream_fnRead(IAVIStream*iface,LONG start,LONG samples,LPVOID buffer,LONG buffersize,LONG *bytesread,LONG *samplesread);
static HRESULT WINAPI IAVIStream_fnWrite(IAVIStream*iface,LONG start,LONG samples,LPVOID buffer,LONG buffersize,DWORD flags,LONG *sampwritten,LONG *byteswritten);
static HRESULT WINAPI IAVIStream_fnDelete(IAVIStream*iface,LONG start,LONG samples);
static HRESULT WINAPI IAVIStream_fnReadData(IAVIStream*iface,DWORD fcc,LPVOID lp,LONG *lpread);
static HRESULT WINAPI IAVIStream_fnWriteData(IAVIStream*iface,DWORD fcc,LPVOID lp,LONG size);
static HRESULT WINAPI IAVIStream_fnSetInfo(IAVIStream*iface,AVISTREAMINFOW*info,LONG infolen);

struct ICOM_VTABLE(IAVIStream) iavist = {
  ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
  IAVIStream_fnQueryInterface,
  IAVIStream_fnAddRef,
  IAVIStream_fnRelease,
  IAVIStream_fnCreate,
  IAVIStream_fnInfo,
  IAVIStream_fnFindSample,
  IAVIStream_fnReadFormat,
  IAVIStream_fnSetFormat,
  IAVIStream_fnRead,
  IAVIStream_fnWrite,
  IAVIStream_fnDelete,
  IAVIStream_fnReadData,
  IAVIStream_fnWriteData,
  IAVIStream_fnSetInfo
};

typedef struct _IAVIFileImpl IAVIFileImpl;

typedef struct _IPersistFileImpl {
  /* IUnknown stuff */
  ICOM_VFIELD(IPersistFile);

  /* IPersistFile stuff */
  IAVIFileImpl     *paf;
} IPersistFileImpl;

typedef struct _IAVIStreamImpl {
  /* IUnknown stuff */
  ICOM_VFIELD(IAVIStream);
  DWORD		    ref;

  /* IAVIStream stuff */
  IAVIFileImpl     *paf;
  DWORD             nStream;       /* the n-th stream in file */
  AVISTREAMINFOW    sInfo;

  LPVOID            lpFormat;
  DWORD             cbFormat;

  LPVOID            lpHandlerData;
  DWORD             cbHandlerData;

  EXTRACHUNKS       extra;

  LPDWORD           lpBuffer;
  DWORD             cbBuffer;       /* size of lpBuffer */
  DWORD             dwCurrentFrame; /* frame/block currently in lpBuffer */

  LONG              lLastFrame;    /* last correct index in idxFrames */
  AVIINDEXENTRY    *idxFrames;
  DWORD             nIdxFrames;     /* upper index limit of idxFrames */
  AVIINDEXENTRY    *idxFmtChanges;
  DWORD             nIdxFmtChanges; /* upper index limit of idxFmtChanges */
} IAVIStreamImpl;

struct _IAVIFileImpl {
  /* IUnknown stuff */
  ICOM_VFIELD(IAVIFile);
  DWORD		    ref;

  /* IAVIFile stuff... */
  IPersistFileImpl  iPersistFile;

  AVIFILEINFOW      fInfo;
  IAVIStreamImpl   *ppStreams[MAX_AVISTREAMS];

  EXTRACHUNKS       fileextra;

  DWORD             dwMoviChunkPos;  /* some stuff for saving ... */
  DWORD             dwIdxChunkPos;
  DWORD             dwNextFramePos;
  DWORD             dwInitialFrames;

  MMCKINFO          ckLastRecord;
  AVIINDEXENTRY    *idxRecords;      /* won't be updated while loading */
  DWORD             nIdxRecords;     /* current fill level */
  DWORD             cbIdxRecords;    /* size of idxRecords */

  /* IPersistFile stuff ... */
  HMMIO             hmmio;
  LPWSTR            szFileName;
  UINT              uMode;
  BOOL              fDirty;
};

/***********************************************************************/

static HRESULT AVIFILE_AddFrame(IAVIStreamImpl *This, DWORD ckid, DWORD size,
				DWORD offset, DWORD flags);
static HRESULT AVIFILE_AddRecord(IAVIFileImpl *This);
static DWORD   AVIFILE_ComputeMoviStart(IAVIFileImpl *This);
static void    AVIFILE_ConstructAVIStream(IAVIFileImpl *paf, DWORD nr,
					  LPAVISTREAMINFOW asi);
static void    AVIFILE_DestructAVIStream(IAVIStreamImpl *This);
static HRESULT AVIFILE_LoadFile(IAVIFileImpl *This);
static HRESULT AVIFILE_LoadIndex(IAVIFileImpl *This, DWORD size, DWORD offset);
static HRESULT AVIFILE_ParseIndex(IAVIFileImpl *This, AVIINDEXENTRY *lp,
				  LONG count, DWORD pos, BOOL *bAbsolute);
static HRESULT AVIFILE_ReadBlock(IAVIStreamImpl *This, DWORD start,
				 LPVOID buffer, LONG size);
static void    AVIFILE_SamplesToBlock(IAVIStreamImpl *This, LPLONG pos,
				      LPLONG offset);
static HRESULT AVIFILE_SaveFile(IAVIFileImpl *This);
static HRESULT AVIFILE_SaveIndex(IAVIFileImpl *This);
static ULONG   AVIFILE_SearchStream(IAVIFileImpl *This, DWORD fccType,
				    LONG lSkip);
static void    AVIFILE_UpdateInfo(IAVIFileImpl *This);
static HRESULT AVIFILE_WriteBlock(IAVIStreamImpl *This, DWORD block,
				  FOURCC ckid, DWORD flags, LPVOID buffer,
				  LONG size);

HRESULT AVIFILE_CreateAVIFile(REFIID riid, LPVOID *ppv)
{
  IAVIFileImpl *pfile;
  HRESULT       hr;

  assert(riid != NULL && ppv != NULL);

  *ppv = NULL;

  pfile = (IAVIFileImpl*)LocalAlloc(LPTR, sizeof(IAVIFileImpl));
  if (pfile == NULL)
    return AVIERR_MEMORY;

  pfile->lpVtbl = &iavift;
  pfile->ref = 0;
  pfile->iPersistFile.lpVtbl = &ipersistft;
  pfile->iPersistFile.paf = pfile;

  hr = IUnknown_QueryInterface((IUnknown*)pfile, riid, ppv);
  if (FAILED(hr))
    LocalFree((HLOCAL)pfile);

  return hr;
}

static HRESULT WINAPI IAVIFile_fnQueryInterface(IAVIFile *iface, REFIID refiid,
						LPVOID *obj)
{
  ICOM_THIS(IAVIFileImpl,iface);

  TRACE("(%p,%s,%p)\n", This, debugstr_guid(refiid), obj);

  if (IsEqualGUID(&IID_IUnknown, refiid) ||
      IsEqualGUID(&IID_IAVIFile, refiid)) {
    *obj = iface;
    IAVIFile_AddRef(iface);

    return S_OK;
  } else if (IsEqualGUID(&IID_IPersistFile, refiid)) {
    *obj = &This->iPersistFile;
    IAVIFile_AddRef(iface);

    return S_OK;
  }

  return OLE_E_ENUM_NOMORE;
}

static ULONG WINAPI IAVIFile_fnAddRef(IAVIFile *iface)
{
  ICOM_THIS(IAVIFileImpl,iface);

  TRACE("(%p) -> %ld\n", iface, This->ref + 1);
  return ++(This->ref);
}

static ULONG WINAPI IAVIFile_fnRelease(IAVIFile *iface)
{
  ICOM_THIS(IAVIFileImpl,iface);
  UINT i;

  TRACE("(%p) -> %ld\n", iface, This->ref - 1);

  if (!--(This->ref)) {
    if (This->fDirty) {
      /* need to write headers to file */
      AVIFILE_SaveFile(This);
    }

    for (i = 0; i < This->fInfo.dwStreams; i++) {
      if (This->ppStreams[i] != NULL) {
	if (This->ppStreams[i]->ref != 0) {
	  ERR(": someone has still %lu reference to stream %u (%p)!\n",
	       This->ppStreams[i]->ref, i, This->ppStreams[i]);
	}
	AVIFILE_DestructAVIStream(This->ppStreams[i]);
	LocalFree((HLOCAL)This->ppStreams[i]);
	This->ppStreams[i] = NULL;
      }
    }

    if (This->idxRecords != NULL) {
      GlobalFreePtr(This->idxRecords);
      This->idxRecords  = NULL;
      This->nIdxRecords = 0;
    }

    if (This->fileextra.lp != NULL) {
      GlobalFreePtr(This->fileextra.lp);
      This->fileextra.lp = NULL;
      This->fileextra.cb = 0;
    }

    if (This->szFileName != NULL) {
      LocalFree((HLOCAL)This->szFileName);
      This->szFileName = NULL;
    }
    if (This->hmmio != NULL) {
      mmioClose(This->hmmio, 0);
      This->hmmio = NULL;
    }

    LocalFree((HLOCAL)This);
    return 0;
  }
  return This->ref;
}

static HRESULT WINAPI IAVIFile_fnInfo(IAVIFile *iface, LPAVIFILEINFOW afi,
				      LONG size)
{
  ICOM_THIS(IAVIFileImpl,iface);

  TRACE("(%p,%p,%ld)\n",iface,afi,size);

  if (afi == NULL)
    return AVIERR_BADPARAM;
  if (size < 0)
    return AVIERR_BADSIZE;

  AVIFILE_UpdateInfo(This);

  memcpy(afi, &This->fInfo, min((DWORD)size, sizeof(This->fInfo)));

  if ((DWORD)size < sizeof(This->fInfo))
    return AVIERR_BUFFERTOOSMALL;
  return AVIERR_OK;
}

static HRESULT WINAPI IAVIFile_fnGetStream(IAVIFile *iface, PAVISTREAM *avis,
					   DWORD fccType, LONG lParam)
{
  ICOM_THIS(IAVIFileImpl,iface);

  ULONG nStream;

  TRACE("(%p,%p,0x%08lX,%ld)\n", iface, avis, fccType, lParam);

  if (avis == NULL || lParam < 0)
    return AVIERR_BADPARAM;

  nStream = AVIFILE_SearchStream(This, fccType, lParam);

  /* Does the requested stream exist? */
  if (nStream < This->fInfo.dwStreams &&
      This->ppStreams[nStream] != NULL) {
    *avis = (PAVISTREAM)This->ppStreams[nStream];
    IAVIStream_AddRef(*avis);

    return AVIERR_OK;
  }

  /* Sorry, but the specified stream doesn't exist */
  return AVIERR_NODATA;
}

static HRESULT WINAPI IAVIFile_fnCreateStream(IAVIFile *iface,PAVISTREAM *avis,
					      LPAVISTREAMINFOW asi)
{
  ICOM_THIS(IAVIFileImpl,iface);

  DWORD n;

  TRACE("(%p,%p,%p)\n", iface, avis, asi);

  /* check parameters */
  if (avis == NULL || asi == NULL)
    return AVIERR_BADPARAM;

  *avis = NULL;

  /* Does the user have write permission? */
  if ((This->uMode & MMIO_RWMODE) == 0)
    return AVIERR_READONLY;

  /* Can we add another stream? */
  n = This->fInfo.dwStreams;
  if (n >= MAX_AVISTREAMS || This->dwMoviChunkPos != 0) {
    /* already reached max nr of streams
     * or have already written frames to disk */
    return AVIERR_UNSUPPORTED;
  }

  /* check AVISTREAMINFO for some really needed things */
  if (asi->fccType == 0 || asi->dwScale == 0 || asi->dwRate == 0)
    return AVIERR_BADFORMAT;

  /* now it seems to be save to add the stream */
  assert(This->ppStreams[n] == NULL);
  This->ppStreams[n] = (IAVIStreamImpl*)LocalAlloc(LPTR,
						   sizeof(IAVIStreamImpl));
  if (This->ppStreams[n] == NULL)
    return AVIERR_MEMORY;

  /* initialize the new allocated stream */
  AVIFILE_ConstructAVIStream(This, n, asi);

  This->fInfo.dwStreams++;
  This->fDirty = TRUE;

  /* update our AVIFILEINFO structure */
  AVIFILE_UpdateInfo(This);

  /* return it */
  *avis = (PAVISTREAM)This->ppStreams[n];
  IAVIStream_AddRef(*avis);

  return AVIERR_OK;
}

static HRESULT WINAPI IAVIFile_fnWriteData(IAVIFile *iface, DWORD ckid,
					   LPVOID lpData, LONG size)
{
  ICOM_THIS(IAVIFileImpl,iface);

  TRACE("(%p,0x%08lX,%p,%ld)\n", iface, ckid, lpData, size);

  /* check parameters */
  if (lpData == NULL)
    return AVIERR_BADPARAM;
  if (size < 0)
    return AVIERR_BADSIZE;

  /* Do we have write permission? */
  if ((This->uMode & MMIO_RWMODE) == 0)
    return AVIERR_READONLY;

  This->fDirty = TRUE;

  return WriteExtraChunk(&This->fileextra, ckid, lpData, size);
}

static HRESULT WINAPI IAVIFile_fnReadData(IAVIFile *iface, DWORD ckid,
					  LPVOID lpData, LONG *size)
{
  ICOM_THIS(IAVIFileImpl,iface);

  TRACE("(%p,0x%08lX,%p,%p)\n", iface, ckid, lpData, size);

  return ReadExtraChunk(&This->fileextra, ckid, lpData, size);
}

static HRESULT WINAPI IAVIFile_fnEndRecord(IAVIFile *iface)
{
  ICOM_THIS(IAVIFileImpl,iface);

  TRACE("(%p)\n",iface);

  if ((This->uMode & MMIO_RWMODE) == 0)
    return AVIERR_READONLY;

  This->fDirty = TRUE;

  /* no frames written to any stream? -- compute start of 'movi'-chunk */
  if (This->dwMoviChunkPos == 0)
    AVIFILE_ComputeMoviStart(This);

  This->fInfo.dwFlags  |= AVIFILEINFO_ISINTERLEAVED;

  /* already written frames to any stream, ... */
  if (This->ckLastRecord.dwFlags & MMIO_DIRTY) {
    /* close last record */
    if (mmioAscend(This->hmmio, &This->ckLastRecord, 0) != 0)
      return AVIERR_FILEWRITE;

    AVIFILE_AddRecord(This);

    if (This->fInfo.dwSuggestedBufferSize < This->ckLastRecord.cksize + 3 * sizeof(DWORD))
      This->fInfo.dwSuggestedBufferSize = This->ckLastRecord.cksize + 3 * sizeof(DWORD);
  }

  /* write out a new record into file, but don't close it */
  This->ckLastRecord.cksize  = 0;
  This->ckLastRecord.fccType = listtypeAVIRECORD;
  if (mmioSeek(This->hmmio, This->dwNextFramePos, SEEK_SET) == -1)
    return AVIERR_FILEWRITE;
  if (mmioCreateChunk(This->hmmio, &This->ckLastRecord, MMIO_CREATELIST) != 0)
    return AVIERR_FILEWRITE;
  This->dwNextFramePos += 3 * sizeof(DWORD);

  return AVIERR_OK;
}

static HRESULT WINAPI IAVIFile_fnDeleteStream(IAVIFile *iface, DWORD fccType,
					      LONG lParam)
{
  ICOM_THIS(IAVIFileImpl,iface);

  ULONG nStream;

  TRACE("(%p,0x%08lX,%ld)\n", iface, fccType, lParam);

  /* check parameter */
  if (lParam < 0)
    return AVIERR_BADPARAM;

  /* Have user write permissions? */
  if ((This->uMode & MMIO_RWMODE) == 0)
    return AVIERR_READONLY;

  nStream = AVIFILE_SearchStream(This, fccType, lParam);

  /* Does the requested stream exist? */
  if (nStream < This->fInfo.dwStreams &&
      This->ppStreams[nStream] != NULL) {
    /* ... so delete it now */
    LocalFree((HLOCAL)This->ppStreams[nStream]);

    if (This->fInfo.dwStreams - nStream > 0)
      memcpy(This->ppStreams + nStream, This->ppStreams + nStream + 1,
	     (This->fInfo.dwStreams - nStream) * sizeof(IAVIStreamImpl*));

    This->ppStreams[This->fInfo.dwStreams] = NULL;
    This->fInfo.dwStreams--;
    This->fDirty = TRUE;

    /* This->fInfo will be updated further when asked for */
    return AVIERR_OK;
  } else
    return AVIERR_NODATA;
}

/***********************************************************************/

static HRESULT WINAPI IPersistFile_fnQueryInterface(IPersistFile *iface,
						    REFIID refiid, LPVOID *obj)
{
  ICOM_THIS(IPersistFileImpl,iface);

  assert(This->paf != NULL);

  return IAVIFile_QueryInterface((PAVIFILE)This->paf, refiid, obj);
}

static ULONG   WINAPI IPersistFile_fnAddRef(IPersistFile *iface)
{
  ICOM_THIS(IPersistFileImpl,iface);

  assert(This->paf != NULL);

  return IAVIFile_AddRef((PAVIFILE)This->paf);
}

static ULONG   WINAPI IPersistFile_fnRelease(IPersistFile *iface)
{
  ICOM_THIS(IPersistFileImpl,iface);

  assert(This->paf != NULL);

  return IAVIFile_Release((PAVIFILE)This->paf);
}

static HRESULT WINAPI IPersistFile_fnGetClassID(IPersistFile *iface,
						LPCLSID pClassID)
{
  TRACE("(%p,%p)\n", iface, pClassID);

  if (pClassID == NULL)
    return AVIERR_BADPARAM;

  memcpy(pClassID, &CLSID_AVIFile, sizeof(CLSID_AVIFile));

  return AVIERR_OK;
}

static HRESULT WINAPI IPersistFile_fnIsDirty(IPersistFile *iface)
{
  ICOM_THIS(IPersistFileImpl,iface);

  TRACE("(%p)\n", iface);

  assert(This->paf != NULL);

  return (This->paf->fDirty ? S_OK : S_FALSE);
}

static HRESULT WINAPI IPersistFile_fnLoad(IPersistFile *iface,
					  LPCOLESTR pszFileName, DWORD dwMode)
{
  ICOM_THIS(IPersistFileImpl,iface);

  int len;

  TRACE("(%p,%s,0x%08lX)\n", iface, debugstr_w(pszFileName), dwMode);

  /* check parameter */
  if (pszFileName == NULL)
    return AVIERR_BADPARAM;

  assert(This->paf != NULL);
  if (This->paf->hmmio != NULL)
    return AVIERR_ERROR; /* No reuse of this object for another file! */

  /* remeber mode and name */
  This->paf->uMode = dwMode;

  len = lstrlenW(pszFileName) + 1;
  This->paf->szFileName = (LPWSTR)LocalAlloc(LPTR, len * sizeof(WCHAR));
  if (This->paf->szFileName == NULL)
    return AVIERR_MEMORY;
  lstrcpyW(This->paf->szFileName, pszFileName);

  /* try to open the file */
  This->paf->hmmio = mmioOpenW(This->paf->szFileName, NULL,
			       MMIO_ALLOCBUF | dwMode);
  if (This->paf->hmmio == NULL) {
    /* mmioOpenW not in native DLLs of Win9x -- try mmioOpenA */
    LPSTR szFileName = LocalAlloc(LPTR, len * sizeof(CHAR));
    if (szFileName == NULL)
      return AVIERR_MEMORY;

    WideCharToMultiByte(CP_ACP, 0, This->paf->szFileName, -1, szFileName,
			len, NULL, NULL);

    This->paf->hmmio = mmioOpenA(szFileName, NULL, MMIO_ALLOCBUF | dwMode);
    LocalFree((HLOCAL)szFileName);
    if (This->paf->hmmio == NULL)
      return AVIERR_FILEOPEN;
  }

  /* should we create a new file? */
  if (dwMode & OF_CREATE) {
    memset(& This->paf->fInfo, 0, sizeof(This->paf->fInfo));
    This->paf->fInfo.dwFlags = AVIFILEINFO_HASINDEX | AVIFILEINFO_TRUSTCKTYPE;

    return AVIERR_OK;
  } else
    return AVIFILE_LoadFile(This->paf);
}

static HRESULT WINAPI IPersistFile_fnSave(IPersistFile *iface,
					  LPCOLESTR pszFileName,BOOL fRemember)
{
  TRACE("(%p,%s,%d)\n", iface, debugstr_w(pszFileName), fRemember);

  /* We write directly to disk, so nothing to do. */

  return AVIERR_OK;
}

static HRESULT WINAPI IPersistFile_fnSaveCompleted(IPersistFile *iface,
						   LPCOLESTR pszFileName)
{
  TRACE("(%p,%s)\n", iface, debugstr_w(pszFileName));

  /* We write directly to disk, so nothing to do. */

  return AVIERR_OK;
}

static HRESULT WINAPI IPersistFile_fnGetCurFile(IPersistFile *iface,
						LPOLESTR *ppszFileName)
{
  ICOM_THIS(IPersistFileImpl,iface);

  TRACE("(%p,%p)\n", iface, ppszFileName);

  if (ppszFileName == NULL)
    return AVIERR_BADPARAM;

  *ppszFileName = NULL;

  assert(This->paf != NULL);

  if (This->paf->szFileName != NULL) {
    int len = lstrlenW(This->paf->szFileName);

    *ppszFileName = (LPOLESTR)GlobalAllocPtr(GHND, len * sizeof(WCHAR));
    if (*ppszFileName == NULL)
      return AVIERR_MEMORY;

    memcpy(*ppszFileName, This->paf->szFileName, len * sizeof(WCHAR));
  }

  return AVIERR_OK;
}

/***********************************************************************/

static HRESULT WINAPI IAVIStream_fnQueryInterface(IAVIStream *iface,
						  REFIID refiid, LPVOID *obj)
{
  ICOM_THIS(IAVIStreamImpl,iface);

  TRACE("(%p,%s,%p)\n", iface, debugstr_guid(refiid), obj);

  if (IsEqualGUID(&IID_IUnknown, refiid) ||
      IsEqualGUID(&IID_IAVIStream, refiid)) {
    *obj = This;
    IAVIStream_AddRef(iface);

    return S_OK;
  }
  /* FIXME: IAVIStreaming interface */

  return OLE_E_ENUM_NOMORE;
}

static ULONG WINAPI IAVIStream_fnAddRef(IAVIStream *iface)
{
  ICOM_THIS(IAVIStreamImpl,iface);

  TRACE("(%p) -> %ld\n", iface, This->ref + 1);

  /* also add ref to parent, so that it doesn't kill us */
  if (This->paf != NULL)
    IAVIFile_AddRef((PAVIFILE)This->paf);

  return ++(This->ref);
}

static ULONG WINAPI IAVIStream_fnRelease(IAVIStream* iface)
{
  ICOM_THIS(IAVIStreamImpl,iface);

  TRACE("(%p) -> %ld\n", iface, This->ref - 1);

  /* we belong to the AVIFile, which must free us! */
  if (This->ref == 0) {
    ERR(": already released!\n");
    return 0;
  }

  This->ref--;

  if (This->paf != NULL)
    IAVIFile_Release((PAVIFILE)This->paf);

  return This->ref;
}

static HRESULT WINAPI IAVIStream_fnCreate(IAVIStream *iface, LPARAM lParam1,
					  LPARAM lParam2)
{
  TRACE("(%p,0x%08lX,0x%08lX)\n", iface, lParam1, lParam2);

  /* This IAVIStream interface needs an AVIFile */
  return AVIERR_UNSUPPORTED;
}

static HRESULT WINAPI IAVIStream_fnInfo(IAVIStream *iface,LPAVISTREAMINFOW psi,
					LONG size)
{
  ICOM_THIS(IAVIStreamImpl,iface);

  TRACE("(%p,%p,%ld)\n", iface, psi, size);

  if (psi == NULL)
    return AVIERR_BADPARAM;
  if (size < 0)
    return AVIERR_BADSIZE;

  memcpy(psi, &This->sInfo, min((DWORD)size, sizeof(This->sInfo)));

  if ((DWORD)size < sizeof(This->sInfo))
    return AVIERR_BUFFERTOOSMALL;
  return AVIERR_OK;
}

static LONG WINAPI IAVIStream_fnFindSample(IAVIStream *iface, LONG pos,
					   LONG flags)
{
  ICOM_THIS(IAVIStreamImpl,iface);

  LONG offset = 0;

  TRACE("(%p,%ld,0x%08lX)\n",iface,pos,flags);

  if (flags & FIND_FROM_START) {
    pos = This->sInfo.dwStart;
    flags &= ~(FIND_FROM_START|FIND_PREV);
    flags |= FIND_NEXT;
  }

  if (This->sInfo.dwSampleSize != 0) {
    /* convert samples into block number with offset */
    AVIFILE_SamplesToBlock(This, &pos, &offset);
  }

  if (flags & FIND_TYPE) {
    if (flags & FIND_KEY) {
      while (0 <= pos && pos <= This->lLastFrame) {
	if (This->idxFrames[pos].dwFlags & AVIIF_KEYFRAME)
	  goto RETURN_FOUND;

	if (flags & FIND_NEXT)
	  pos++;
	else
	  pos--;
      };
    } else if (flags & FIND_ANY) {
      while (0 <= pos && pos <= This->lLastFrame) {
	if (This->idxFrames[pos].dwChunkLength > 0)
	  goto RETURN_FOUND;

	if (flags & FIND_NEXT)
	  pos++;
	else
	  pos--;

      };
    } else if ((flags & FIND_FORMAT) && This->idxFmtChanges != NULL &&
	       This->sInfo.fccType == streamtypeVIDEO) {
      if (flags & FIND_NEXT) {
	ULONG n;

	for (n = 0; n < This->sInfo.dwFormatChangeCount; n++)
	  if (This->idxFmtChanges[n].ckid >= pos) {
            pos = This->idxFmtChanges[n].ckid;
	    goto RETURN_FOUND;
          }
      } else {
	LONG n;

	for (n = (LONG)This->sInfo.dwFormatChangeCount; n >= 0; n--) {
	  if (This->idxFmtChanges[n].ckid <= pos) {
            pos = This->idxFmtChanges[n].ckid;
	    goto RETURN_FOUND;
          }
	}

	if (pos > (LONG)This->sInfo.dwStart)
	  return 0; /* format changes always for first frame */
      }
    }

    return -1;
  }

 RETURN_FOUND:
  if (pos < (LONG)This->sInfo.dwStart)
    return -1;

  switch (flags & FIND_RET) {
  case FIND_LENGTH:
    /* physical size */
    pos = This->idxFrames[pos].dwChunkLength;
    break;
  case FIND_OFFSET:
    /* physical position */
    pos = This->idxFrames[pos].dwChunkOffset + 2 * sizeof(DWORD)
      + offset * This->sInfo.dwSampleSize;
    break;
  case FIND_SIZE:
    /* logical size */
    if (This->sInfo.dwSampleSize)
      pos = This->sInfo.dwSampleSize;
    else
      pos = 1;
    break;
  case FIND_INDEX:
    FIXME(": FIND_INDEX flag is not supported!\n");
    /* This is an index in the index-table on disc. */
    break;
  }; /* else logical position */

  return pos;
}

static HRESULT WINAPI IAVIStream_fnReadFormat(IAVIStream *iface, LONG pos,
					      LPVOID format, LONG *formatsize)
{
  ICOM_THIS(IAVIStreamImpl,iface);

  TRACE("(%p,%ld,%p,%p)\n", iface, pos, format, formatsize);

  if (formatsize == NULL)
    return AVIERR_BADPARAM;

  /* only interested in needed buffersize? */
  if (format == NULL || *formatsize <= 0) {
    *formatsize = This->cbFormat;

    return AVIERR_OK;
  }

  /* copy initial format (only as much as will fit) */
  memcpy(format, This->lpFormat, min(*(DWORD*)formatsize, This->cbFormat));
  if (*(DWORD*)formatsize < This->cbFormat) {
    *formatsize = This->cbFormat;
    return AVIERR_BUFFERTOOSMALL;
  }

  /* Could format change? When yes will it change? */
  if ((This->sInfo.dwFlags & AVISTREAMINFO_FORMATCHANGES) &&
      pos > This->sInfo.dwStart) {
    LONG lLastFmt;

    lLastFmt = IAVIStream_fnFindSample(iface, pos, FIND_FORMAT|FIND_PREV);
    if (lLastFmt > 0) {
      FIXME(": need to read formatchange for %ld -- unimplemented!\n",lLastFmt);
    }
  }

  *formatsize = This->cbFormat;
  return AVIERR_OK;
}

static HRESULT WINAPI IAVIStream_fnSetFormat(IAVIStream *iface, LONG pos,
					     LPVOID format, LONG formatsize)
{
  ICOM_THIS(IAVIStreamImpl,iface);

  LPBITMAPINFOHEADER lpbiNew = (LPBITMAPINFOHEADER)format;

  TRACE("(%p,%ld,%p,%ld)\n", iface, pos, format, formatsize);

  /* check parameters */
  if (format == NULL || formatsize <= 0)
    return AVIERR_BADPARAM;

  /* Do we have write permission? */
  if ((This->paf->uMode & MMIO_RWMODE) == 0)
    return AVIERR_READONLY;

  /* can only set format before frame is written! */
  if (This->lLastFrame > pos)
    return AVIERR_UNSUPPORTED;

  /* initial format or a formatchange? */
  if (This->lpFormat == NULL) {
    /* initial format */
    if (This->paf->dwMoviChunkPos != 0)
      return AVIERR_ERROR; /* user has used API in wrong sequnece! */

    This->lpFormat = GlobalAllocPtr(GMEM_MOVEABLE, formatsize);
    if (This->lpFormat == NULL)
      return AVIERR_MEMORY;
    This->cbFormat = formatsize;

    memcpy(This->lpFormat, format, formatsize);

    /* update some infos about stream */
    if (This->sInfo.fccType == streamtypeVIDEO) {
      LONG lDim;

      lDim = This->sInfo.rcFrame.right - This->sInfo.rcFrame.left;
      if (lDim < lpbiNew->biWidth)
	This->sInfo.rcFrame.right = This->sInfo.rcFrame.left + lpbiNew->biWidth;
      lDim = This->sInfo.rcFrame.bottom - This->sInfo.rcFrame.top;
      if (lDim < lpbiNew->biHeight)
	This->sInfo.rcFrame.bottom = This->sInfo.rcFrame.top + lpbiNew->biHeight;
    } else if (This->sInfo.fccType == streamtypeAUDIO)
      This->sInfo.dwSampleSize = ((LPWAVEFORMATEX)This->lpFormat)->nBlockAlign;

    return AVIERR_OK;
  } else {
    MMCKINFO           ck;
    LPBITMAPINFOHEADER lpbiOld = (LPBITMAPINFOHEADER)This->lpFormat;
    RGBQUAD           *rgbNew  = (RGBQUAD*)((LPBYTE)lpbiNew + lpbiNew->biSize);
    AVIPALCHANGE      *lppc = NULL;
    INT                n;

    /* pherhaps formatchange, check it ... */
    if (This->cbFormat != formatsize)
      return AVIERR_UNSUPPORTED;

    /* no formatchange, only the initial one */
    if (memcmp(This->lpFormat, format, formatsize) == 0)
      return AVIERR_OK;

    /* check that's only the palette, which changes */
    if (lpbiOld->biSize        != lpbiNew->biSize ||
	lpbiOld->biWidth       != lpbiNew->biWidth ||
	lpbiOld->biHeight      != lpbiNew->biHeight ||
	lpbiOld->biPlanes      != lpbiNew->biPlanes ||
	lpbiOld->biBitCount    != lpbiNew->biBitCount ||
	lpbiOld->biCompression != lpbiNew->biCompression ||
	lpbiOld->biClrUsed     != lpbiNew->biClrUsed)
      return AVIERR_UNSUPPORTED;

    This->sInfo.dwFlags |= AVISTREAMINFO_FORMATCHANGES;

    /* simply say all colors have changed */
    ck.ckid   = MAKEAVICKID(cktypePALchange, This->nStream);
    ck.cksize = 2 * sizeof(WORD) + lpbiOld->biClrUsed * sizeof(PALETTEENTRY);
    lppc = (AVIPALCHANGE*)GlobalAllocPtr(GMEM_MOVEABLE, ck.cksize);
    if (lppc == NULL)
      return AVIERR_MEMORY;

    lppc->bFirstEntry = 0;
    lppc->bNumEntries = (lpbiOld->biClrUsed < 256 ? lpbiOld->biClrUsed : 0);
    lppc->wFlags      = 0;
    for (n = 0; n < lpbiOld->biClrUsed; n++) {
      lppc->peNew[n].peRed   = rgbNew[n].rgbRed;
      lppc->peNew[n].peGreen = rgbNew[n].rgbGreen;
      lppc->peNew[n].peBlue  = rgbNew[n].rgbBlue;
      lppc->peNew[n].peFlags = 0;
    }

    if (mmioSeek(This->paf->hmmio, This->paf->dwNextFramePos, SEEK_SET) == -1)
      return AVIERR_FILEWRITE;
    if (mmioCreateChunk(This->paf->hmmio, &ck, 0) != S_OK)
      return AVIERR_FILEWRITE;
    if (mmioWrite(This->paf->hmmio, (HPSTR)lppc, ck.cksize) != ck.cksize)
      return AVIERR_FILEWRITE;
    if (mmioAscend(This->paf->hmmio, &ck, 0) != S_OK)
      return AVIERR_FILEWRITE;
    This->paf->dwNextFramePos += ck.cksize + 2 * sizeof(DWORD);

    GlobalFreePtr(lppc);

    return AVIFILE_AddFrame(This, cktypePALchange, n, ck.dwDataOffset, 0);
  }
}

static HRESULT WINAPI IAVIStream_fnRead(IAVIStream *iface, LONG start,
					LONG samples, LPVOID buffer,
					LONG buffersize, LPLONG bytesread,
					LPLONG samplesread)
{
  ICOM_THIS(IAVIStreamImpl,iface);

  DWORD    size;
  HRESULT  hr;

  TRACE("(%p,%ld,%ld,%p,%ld,%p,%p)\n", iface, start, samples, buffer,
 	buffersize, bytesread, samplesread);

  /* clear return parameters if given */
  if (bytesread != NULL)
    *bytesread = 0;
  if (samplesread != NULL)
    *samplesread = 0;

  /* check parameters */
  if ((LONG)This->sInfo.dwStart > start)
    return AVIERR_NODATA; /* couldn't read before start of stream */
  if (This->sInfo.dwStart + This->sInfo.dwLength < (DWORD)start)
    return AVIERR_NODATA; /* start is past end of stream */

  /* should we read as much as possible? */
  if (samples == -1) {
    /* User should know how much we have read */
    if (bytesread == NULL && samplesread == NULL)
      return AVIERR_BADPARAM;

    if (This->sInfo.dwSampleSize != 0)
      samples = buffersize / This->sInfo.dwSampleSize;
    else
      samples = 1;
  }

  /* limit to end of stream */
  if ((LONG)This->sInfo.dwLength < samples)
    samples = This->sInfo.dwLength;
  if ((start - This->sInfo.dwStart) > (This->sInfo.dwLength - samples))
    samples = This->sInfo.dwLength - (start - This->sInfo.dwStart);

  /* nothing to read? Then leave ... */
  if (samples == 0)
    return AVIERR_OK;

  if (This->sInfo.dwSampleSize != 0) {
    /* fixed samplesize -- we can read over frame/block boundaries */
    LONG block  = start;
    LONG offset = 0;

    /* convert start sample to block,offset pair */
    AVIFILE_SamplesToBlock(This, &block, &offset);

    /* convert samples to bytes */
    samples *= This->sInfo.dwSampleSize;

    while (samples > 0 && buffersize > 0) {
      if (block != This->dwCurrentFrame) {
	hr = AVIFILE_ReadBlock(This, block, NULL, 0);
	if (FAILED(hr))
	  return hr;
      }

      size = min((DWORD)samples, (DWORD)buffersize);
      size = min(size, This->cbBuffer - offset);
      memcpy(buffer, ((BYTE*)&This->lpBuffer[2]) + offset, size);

      block++;
      offset = 0;
      ((BYTE*)buffer) += size;
      samples    -= size;
      buffersize -= size;

      /* fill out return parameters if given */
      if (bytesread != NULL)
	*bytesread   += size;
      if (samplesread != NULL)
	*samplesread += size / This->sInfo.dwSampleSize;
    }

    if (samples == 0)
      return AVIERR_OK;
    else
      return AVIERR_BUFFERTOOSMALL;
  } else {
    /* variable samplesize -- we can only read one full frame/block */
    if (samples > 1)
      samples = 1;

    assert(start <= This->lLastFrame);
    size = This->idxFrames[start].dwChunkLength;
    if (buffer != NULL && buffersize >= size) {
      hr = AVIFILE_ReadBlock(This, start, buffer, size);
      if (FAILED(hr))
	return hr;
    } else if (buffer != NULL)
      return AVIERR_BUFFERTOOSMALL;

    /* fill out return parameters if given */
    if (bytesread != NULL)
      *bytesread = size;
    if (samplesread != NULL)
      *samplesread = samples;

    return AVIERR_OK;
  }
}

static HRESULT WINAPI IAVIStream_fnWrite(IAVIStream *iface, LONG start,
					 LONG samples, LPVOID buffer,
					 LONG buffersize, DWORD flags,
					 LPLONG sampwritten,
					 LPLONG byteswritten)
{
  ICOM_THIS(IAVIStreamImpl,iface);

  FOURCC  ckid;
  HRESULT hr;

  TRACE("(%p,%ld,%ld,%p,%ld,0x%08lX,%p,%p)\n", iface, start, samples,
	buffer, buffersize, flags, sampwritten, byteswritten);

  /* clear return parameters if given */
  if (sampwritten != NULL)
    *sampwritten = 0;
  if (byteswritten != NULL)
    *byteswritten = 0;

  /* check parameters */
  if (buffer == NULL && (buffersize > 0 || samples > 0))
    return AVIERR_BADPARAM;

  /* Have we write permission? */
  if ((This->paf->uMode & MMIO_RWMODE) == 0)
    return AVIERR_READONLY;

  switch (This->sInfo.fccType) {
  case streamtypeAUDIO:
    ckid = MAKEAVICKID(cktypeWAVEbytes, This->nStream);
    break;
  default:
    if ((flags & AVIIF_KEYFRAME) && buffersize != 0)
      ckid = MAKEAVICKID(cktypeDIBbits, This->nStream);
    else
      ckid = MAKEAVICKID(cktypeDIBcompressed, This->nStream);
    break;
  };

  /* append to end of stream? */
  if (start == -1) {
    if (This->lLastFrame == -1)
      start = This->sInfo.dwStart;
    else
      start = This->sInfo.dwLength;
  } else if (This->lLastFrame == -1)
    This->sInfo.dwStart = start;

  if (This->sInfo.dwSampleSize != 0) {
    /* fixed sample size -- audio like */
    if (samples * This->sInfo.dwSampleSize != buffersize)
      return AVIERR_BADPARAM;

    /* Couldn't skip audio-like data -- User must supply appropriate silence */
    if (This->sInfo.dwLength != start)
      return AVIERR_UNSUPPORTED;

    /* Convert position to frame/block */
    start = This->lLastFrame + 1;

    if ((This->paf->fInfo.dwFlags & AVIFILEINFO_ISINTERLEAVED) == 0) {
      FIXME(": not interleaved, could collect audio data!\n");
    }
  } else {
    /* variable sample size -- video like */
    if (samples > 1)
      return AVIERR_UNSUPPORTED;

    /* must we fill up with empty frames? */
    if (This->lLastFrame != -1) {
      FOURCC ckid2 = MAKEAVICKID(cktypeDIBcompressed, This->nStream);

      while (start > This->lLastFrame + 1) {
	hr = AVIFILE_WriteBlock(This, This->lLastFrame + 1, ckid2, 0, NULL, 0);
	if (FAILED(hr))
	  return hr;
      }
    }
  }

  /* write the block now */
  hr = AVIFILE_WriteBlock(This, start, ckid, flags, buffer, buffersize);
  if (SUCCEEDED(hr)) {
    /* fill out return parameters if given */
    if (sampwritten != NULL)
      *sampwritten = samples;
    if (byteswritten != NULL)
      *byteswritten = buffersize;
  }

  return hr;
}

static HRESULT WINAPI IAVIStream_fnDelete(IAVIStream *iface, LONG start,
					  LONG samples)
{
  ICOM_THIS(IAVIStreamImpl,iface);

  FIXME("(%p,%ld,%ld): stub\n", iface, start, samples);

  /* check parameters */
  if (start < 0 || samples < 0)
    return AVIERR_BADPARAM;

  /* Delete before start of stream? */
  if (start + samples < This->sInfo.dwStart)
    return AVIERR_OK;

  /* Delete after end of stream? */
  if (start > This->sInfo.dwLength)
    return AVIERR_OK;

  /* For the rest we need write permissions */
  if ((This->paf->uMode & MMIO_RWMODE) == 0)
    return AVIERR_READONLY;

  /* 1. overwrite the data with JUNK
   *
   * if ISINTERLEAVED {
   *   2. concat all neighboured JUNK-blocks in this record to one
   *   3. if this record only contains JUNK and is at end set dwNextFramePos
   *      to start of this record, repeat this.
   * } else {
   *   2. concat all neighboured JUNK-blocks.
   *   3. if the JUNK block is at the end, then set dwNextFramePos to
   *      start of this block.
   * }
   */

  return AVIERR_UNSUPPORTED;
}

static HRESULT WINAPI IAVIStream_fnReadData(IAVIStream *iface, DWORD fcc,
					    LPVOID lp, LPLONG lpread)
{
  ICOM_THIS(IAVIStreamImpl,iface);

  TRACE("(%p,0x%08lX,%p,%p)\n", iface, fcc, lp, lpread);

  if (fcc == ckidSTREAMHANDLERDATA) {
    if (This->lpHandlerData != NULL && This->cbHandlerData > 0) {
      if (lp == NULL || *lpread <= 0) {
	*lpread = This->cbHandlerData;
	return AVIERR_OK;
      }

      memcpy(lp, This->lpHandlerData, min(This->cbHandlerData, *lpread));
      if (*lpread < This->cbHandlerData)
	return AVIERR_BUFFERTOOSMALL;
      return AVIERR_OK;
    } else
      return AVIERR_NODATA;
  } else
    return ReadExtraChunk(&This->extra, fcc, lp, lpread);
}

static HRESULT WINAPI IAVIStream_fnWriteData(IAVIStream *iface, DWORD fcc,
					     LPVOID lp, LONG size)
{
  ICOM_THIS(IAVIStreamImpl,iface);

  TRACE("(%p,0x%08lx,%p,%ld)\n", iface, fcc, lp, size);

  /* check parameters */
  if (lp == NULL)
    return AVIERR_BADPARAM;
  if (size <= 0)
    return AVIERR_BADSIZE;

  /* need write permission */
  if ((This->paf->uMode & MMIO_RWMODE) == 0)
    return AVIERR_READONLY;

  /* already written something to this file? */
  if (This->paf->dwMoviChunkPos != 0) {
    /* the data will be inserted before the 'movi' chunk, so check for
     * enough space */
    DWORD dwPos = AVIFILE_ComputeMoviStart(This->paf);

    /* ckid,size => 2 * sizeof(DWORD) */
    dwPos += 2 * sizeof(DWORD) + size;
    if (size >= This->paf->dwMoviChunkPos - 2 * sizeof(DWORD))
      return AVIERR_UNSUPPORTED; /* not enough space left */
  }

  This->paf->fDirty = TRUE;

  if (fcc == ckidSTREAMHANDLERDATA) {
    if (This->lpHandlerData != NULL) {
      FIXME(": handler data already set -- overwirte?\n");
      return AVIERR_UNSUPPORTED;
    }

    This->lpHandlerData = GlobalAllocPtr(GMEM_MOVEABLE, size);
    if (This->lpHandlerData == NULL)
      return AVIERR_MEMORY;
    This->cbHandlerData = size;
    memcpy(This->lpHandlerData, lp, size);

    return AVIERR_OK;
  } else
    return WriteExtraChunk(&This->extra, fcc, lp, size);
}

static HRESULT WINAPI IAVIStream_fnSetInfo(IAVIStream *iface,
					   LPAVISTREAMINFOW info, LONG infolen)
{
  FIXME("(%p,%p,%ld): stub\n", iface, info, infolen);

  return E_FAIL;
}

/***********************************************************************/

static HRESULT AVIFILE_AddFrame(IAVIStreamImpl *This, DWORD ckid, DWORD size, DWORD offset, DWORD flags)
{
  /* pre-conditions */
  assert(This != NULL);

  switch (TWOCCFromFOURCC(ckid)) {
  case cktypeDIBbits:
    if (This->paf->fInfo.dwFlags & AVIFILEINFO_TRUSTCKTYPE)
      flags |= AVIIF_KEYFRAME;
    break;
  case cktypeDIBcompressed:
    if (This->paf->fInfo.dwFlags & AVIFILEINFO_TRUSTCKTYPE)
      flags &= ~AVIIF_KEYFRAME;
    break;
  case cktypePALchange:
    if (This->sInfo.fccType != streamtypeVIDEO) {
      ERR(": found palette change in non-video stream!\n");
      return AVIERR_BADFORMAT;
    }
    This->sInfo.dwFlags |= AVISTREAMINFO_FORMATCHANGES;
    This->sInfo.dwFormatChangeCount++;

    if (This->idxFmtChanges == NULL || This->sInfo.dwFormatChangeCount < This->nIdxFmtChanges) {
      UINT n = This->sInfo.dwFormatChangeCount;

      This->nIdxFmtChanges += 16;
      if (This->idxFmtChanges == NULL)
	This->idxFmtChanges =
	  GlobalAllocPtr(GHND, This->nIdxFmtChanges * sizeof(AVIINDEXENTRY));
      else
	This->idxFmtChanges =
	  GlobalReAllocPtr(This->idxFmtChanges,
			   This->nIdxFmtChanges * sizeof(AVIINDEXENTRY), GHND);
      if (This->idxFmtChanges == NULL)
	return AVIERR_MEMORY;

      This->idxFmtChanges[n].ckid          = This->lLastFrame;
      This->idxFmtChanges[n].dwFlags       = 0;
      This->idxFmtChanges[n].dwChunkOffset = offset;
      This->idxFmtChanges[n].dwChunkLength = size;

      return AVIERR_OK;
    }
    break;
  case cktypeWAVEbytes:
    if (This->paf->fInfo.dwFlags & AVIFILEINFO_TRUSTCKTYPE)
      flags |= AVIIF_KEYFRAME;
    break;
  default:
    WARN(": unknown TWOCC 0x%04X found\n", TWOCCFromFOURCC(ckid));
    break;
  };

  /* first frame is alwasy a keyframe */
  if (This->lLastFrame == -1)
    flags |= AVIIF_KEYFRAME;

  if (This->sInfo.dwSuggestedBufferSize < size)
    This->sInfo.dwSuggestedBufferSize = size;

  /* get memory for index */
  if (This->idxFrames == NULL || This->lLastFrame + 1 >= This->nIdxFrames) {
    This->nIdxFrames += 512;
    if (This->idxFrames == NULL)
      This->idxFrames =
	GlobalAllocPtr(GHND, This->nIdxFrames * sizeof(AVIINDEXENTRY));
      else
	This->idxFrames =
	  GlobalReAllocPtr(This->idxFrames,
			   This->nIdxFrames * sizeof(AVIINDEXENTRY), GHND);
    if (This->idxFrames == NULL)
      return AVIERR_MEMORY;
  }

  This->lLastFrame++;
  This->idxFrames[This->lLastFrame].ckid          = ckid;
  This->idxFrames[This->lLastFrame].dwFlags       = flags;
  This->idxFrames[This->lLastFrame].dwChunkOffset = offset;
  This->idxFrames[This->lLastFrame].dwChunkLength = size;

  /* update AVISTREAMINFO structure if necessary */
  if (This->sInfo.dwLength <= This->lLastFrame)
    This->sInfo.dwLength = This->lLastFrame + 1;

  return AVIERR_OK;
}

static HRESULT AVIFILE_AddRecord(IAVIFileImpl *This)
{
  /* pre-conditions */
  assert(This != NULL && This->ppStreams[0] != NULL);

  if (This->idxRecords == NULL || This->cbIdxRecords == 0) {
    This->cbIdxRecords += 1024 * sizeof(AVIINDEXENTRY);
    This->idxRecords = GlobalAllocPtr(GHND, This->cbIdxRecords);
    if (This->idxRecords == NULL)
      return AVIERR_MEMORY;
  }

  assert(This->nIdxRecords < This->cbIdxRecords/sizeof(AVIINDEXENTRY));

  This->idxRecords[This->nIdxRecords].ckid          = listtypeAVIRECORD;
  This->idxRecords[This->nIdxRecords].dwFlags       = AVIIF_LIST;
  This->idxRecords[This->nIdxRecords].dwChunkOffset =
    This->ckLastRecord.dwDataOffset - 2 * sizeof(DWORD);
  This->idxRecords[This->nIdxRecords].dwChunkLength =
    This->ckLastRecord.cksize;
  This->nIdxRecords++;

  return AVIERR_OK;
}

static DWORD   AVIFILE_ComputeMoviStart(IAVIFileImpl *This)
{
  DWORD dwPos;
  DWORD nStream;

  /* RIFF,hdrl,movi,avih => (3 * 3 + 2) * sizeof(DWORD) = 11 * sizeof(DWORD) */
  dwPos = 11 * sizeof(DWORD) + sizeof(MainAVIHeader);

  for (nStream = 0; nStream < This->fInfo.dwStreams; nStream++) {
    IAVIStreamImpl *pStream = This->ppStreams[nStream];

    /* strl,strh,strf => (3 + 2 * 2) * sizeof(DWORD) = 7 * sizeof(DWORD) */
    dwPos += 7 * sizeof(DWORD) + sizeof(AVIStreamHeader);
    dwPos += ((pStream->cbFormat + 1) & ~1U);
    if (pStream->lpHandlerData != NULL && pStream->cbHandlerData > 0)
      dwPos += 2 * sizeof(DWORD) + ((pStream->cbHandlerData + 1) & ~1U);
    if (lstrlenW(pStream->sInfo.szName) > 0)
      dwPos += 2 * sizeof(DWORD) + ((lstrlenW(pStream->sInfo.szName) + 1) & ~1U);
  }

  if (This->dwMoviChunkPos == 0) {
    This->dwNextFramePos = dwPos;

    /* pad to multiple of AVI_HEADERSIZE only if we are more than 8 bytes away from it */
    if (((dwPos + AVI_HEADERSIZE) & ~(AVI_HEADERSIZE - 1)) - dwPos > 2 * sizeof(DWORD))
      This->dwNextFramePos = (dwPos + AVI_HEADERSIZE) & ~(AVI_HEADERSIZE - 1);

    This->dwMoviChunkPos = This->dwNextFramePos - sizeof(DWORD);
  }

  return dwPos;
}

static void    AVIFILE_ConstructAVIStream(IAVIFileImpl *paf, DWORD nr, LPAVISTREAMINFOW asi)
{
  IAVIStreamImpl *pstream;

  /* pre-conditions */
  assert(paf != NULL);
  assert(nr < MAX_AVISTREAMS);
  assert(paf->ppStreams[nr] != NULL);

  pstream = paf->ppStreams[nr];

  pstream->lpVtbl         = &iavist;
  pstream->ref            = 0;
  pstream->paf            = paf;
  pstream->nStream        = nr;
  pstream->dwCurrentFrame = (DWORD)-1;
  pstream->lLastFrame    = -1;

  if (asi != NULL) {
    memcpy(&pstream->sInfo, asi, sizeof(pstream->sInfo));

    if (asi->dwLength > 0) {
      /* pre-allocate mem for frame-index structure */
      pstream->idxFrames =
	(AVIINDEXENTRY*)GlobalAllocPtr(GHND, asi->dwLength * sizeof(AVIINDEXENTRY));
      if (pstream->idxFrames != NULL)
	pstream->nIdxFrames = asi->dwLength;
    }
    if (asi->dwFormatChangeCount > 0) {
      /* pre-allocate mem for formatchange-index structure */
      pstream->idxFmtChanges =
	(AVIINDEXENTRY*)GlobalAllocPtr(GHND, asi->dwFormatChangeCount * sizeof(AVIINDEXENTRY));
      if (pstream->idxFmtChanges != NULL)
	pstream->nIdxFmtChanges = asi->dwFormatChangeCount;
    }

    /* These values will be computed */
    pstream->sInfo.dwLength              = 0;
    pstream->sInfo.dwSuggestedBufferSize = 0;
    pstream->sInfo.dwFormatChangeCount   = 0;
    pstream->sInfo.dwEditCount           = 1;
    if (pstream->sInfo.dwSampleSize > 0)
      SetRectEmpty(&pstream->sInfo.rcFrame);
  }

  pstream->sInfo.dwCaps = AVIFILECAPS_CANREAD|AVIFILECAPS_CANWRITE;
}

static void    AVIFILE_DestructAVIStream(IAVIStreamImpl *This)
{
  /* pre-conditions */
  assert(This != NULL);

  This->dwCurrentFrame = (DWORD)-1;
  This->lLastFrame    = -1;
  This->paf = NULL;
  if (This->idxFrames != NULL) {
    GlobalFreePtr(This->idxFrames);
    This->idxFrames  = NULL;
    This->nIdxFrames = 0;
  }
  if (This->idxFmtChanges != NULL) {
    GlobalFreePtr(This->idxFmtChanges);
    This->idxFmtChanges = NULL;
  }
  if (This->lpBuffer != NULL) {
    GlobalFreePtr(This->lpBuffer);
    This->lpBuffer = NULL;
    This->cbBuffer = 0;
  }
  if (This->lpHandlerData != NULL) {
    GlobalFreePtr(This->lpHandlerData);
    This->lpHandlerData = NULL;
    This->cbHandlerData = 0;
  }
  if (This->extra.lp != NULL) {
    GlobalFreePtr(This->extra.lp);
    This->extra.lp = NULL;
    This->extra.cb = 0;
  }
  if (This->lpFormat != NULL) {
    GlobalFreePtr(This->lpFormat);
    This->lpFormat = NULL;
    This->cbFormat = 0;
  }
}

static HRESULT AVIFILE_LoadFile(IAVIFileImpl *This)
{
  MainAVIHeader   MainAVIHdr;
  MMCKINFO        ckRIFF;
  MMCKINFO        ckLIST1;
  MMCKINFO        ckLIST2;
  MMCKINFO        ck;
  IAVIStreamImpl *pStream;
  DWORD           nStream;
  HRESULT         hr;

  if (This->hmmio == NULL)
    return AVIERR_FILEOPEN;

  /* initialize stream ptr's */
  memset(This->ppStreams, 0, sizeof(This->ppStreams));

  /* try to get "RIFF" chunk -- must not be at beginning of file! */
  ckRIFF.fccType = formtypeAVI;
  if (mmioDescend(This->hmmio, &ckRIFF, NULL, MMIO_FINDRIFF) != S_OK) {
    ERR(": not an AVI!\n");
    return AVIERR_FILEREAD;
  }

  /* get "LIST" "hdrl" */
  ckLIST1.fccType = listtypeAVIHEADER;
  hr = FindChunkAndKeepExtras(&This->fileextra, This->hmmio, &ckLIST1, &ckRIFF, MMIO_FINDLIST);
  if (FAILED(hr))
    return hr;

  /* get "avih" chunk */
  ck.ckid = ckidAVIMAINHDR;
  hr = FindChunkAndKeepExtras(&This->fileextra, This->hmmio, &ck, &ckLIST1, MMIO_FINDCHUNK);
  if (FAILED(hr))
    return hr;

  if (ck.cksize != sizeof(MainAVIHdr)) {
    ERR(": invalid size of %ld for MainAVIHeader!\n", ck.cksize);
    return AVIERR_BADFORMAT;
  }
  if (mmioRead(This->hmmio, (HPSTR)&MainAVIHdr, ck.cksize) != ck.cksize)
    return AVIERR_FILEREAD;

  /* check for MAX_AVISTREAMS limit */
  if (MainAVIHdr.dwStreams > MAX_AVISTREAMS) {
    WARN("file contains %lu streams, but only supports %d -- change MAX_AVISTREAMS!\n", MainAVIHdr.dwStreams, MAX_AVISTREAMS);
    return AVIERR_UNSUPPORTED;
  }

  /* adjust permissions if copyrighted material in file */
  if (MainAVIHdr.dwFlags & AVIFILEINFO_COPYRIGHTED) {
    This->uMode &= ~MMIO_RWMODE;
    This->uMode |= MMIO_READ;
  }

  /* convert MainAVIHeader into AVIFILINFOW */
  memset(&This->fInfo, 0, sizeof(This->fInfo));
  This->fInfo.dwRate                = MainAVIHdr.dwMicroSecPerFrame;
  This->fInfo.dwScale               = 1000000;
  This->fInfo.dwMaxBytesPerSec      = MainAVIHdr.dwMaxBytesPerSec;
  This->fInfo.dwFlags               = MainAVIHdr.dwFlags;
  This->fInfo.dwCaps                = AVIFILECAPS_CANREAD|AVIFILECAPS_CANWRITE;
  This->fInfo.dwLength              = MainAVIHdr.dwTotalFrames;
  This->fInfo.dwStreams             = MainAVIHdr.dwStreams;
  This->fInfo.dwSuggestedBufferSize = MainAVIHdr.dwSuggestedBufferSize;
  This->fInfo.dwWidth               = MainAVIHdr.dwWidth;
  This->fInfo.dwHeight              = MainAVIHdr.dwHeight;
  LoadStringW(AVIFILE_hModule, IDS_AVIFILETYPE, This->fInfo.szFileType,
	      sizeof(This->fInfo.szFileType));

  /* go back to into header list */
  if (mmioAscend(This->hmmio, &ck, 0) != S_OK)
    return AVIERR_FILEREAD;

  /* foreach stream exists a "LIST","strl" chunk */
  for (nStream = 0; nStream < This->fInfo.dwStreams; nStream++) {
    /* get next nested chunk in this "LIST","strl" */
    if (mmioDescend(This->hmmio, &ckLIST2, &ckLIST1, 0) != S_OK)
      return AVIERR_FILEREAD;

    /* nested chunk must be of type "LIST","strl" -- when not normally JUNK */
    if (ckLIST2.ckid == FOURCC_LIST &&
	ckLIST2.fccType == listtypeSTREAMHEADER) {
      pStream = This->ppStreams[nStream] =
	(IAVIStreamImpl*)LocalAlloc(LPTR, sizeof(IAVIStreamImpl));
      if (pStream == NULL)
	return AVIERR_MEMORY;
      AVIFILE_ConstructAVIStream(This, nStream, NULL);

      ck.ckid = 0;
      while (mmioDescend(This->hmmio, &ck, &ckLIST2, 0) == S_OK) {
	switch (ck.ckid) {
	case ckidSTREAMHANDLERDATA:
	  if (pStream->lpHandlerData != NULL)
	    return AVIERR_BADFORMAT;
	  pStream->lpHandlerData = GlobalAllocPtr(GMEM_DDESHARE|GMEM_MOVEABLE,
						  ck.cksize);
	  if (pStream->lpHandlerData == NULL)
	    return AVIERR_MEMORY;
	  pStream->cbHandlerData = ck.cksize;

	  if (mmioRead(This->hmmio, (HPSTR)pStream->lpHandlerData, ck.cksize) != ck.cksize)
	    return AVIERR_FILEREAD;
	  break;
	case ckidSTREAMFORMAT:
	  if (pStream->lpFormat != NULL)
	    return AVIERR_BADFORMAT;
          if (ck.cksize == 0)
            break;

	  pStream->lpFormat = GlobalAllocPtr(GMEM_DDESHARE|GMEM_MOVEABLE,
					     ck.cksize);
	  if (pStream->lpFormat == NULL)
	    return AVIERR_MEMORY;
	  pStream->cbFormat = ck.cksize;

	  if (mmioRead(This->hmmio, (HPSTR)pStream->lpFormat, ck.cksize) != ck.cksize)
	    return AVIERR_FILEREAD;

	  if (pStream->sInfo.fccType == streamtypeVIDEO) {
	    LPBITMAPINFOHEADER lpbi = (LPBITMAPINFOHEADER)pStream->lpFormat;

	    /* some corrections to the video format */
	    if (lpbi->biClrUsed == 0 && lpbi->biBitCount <= 8)
	      lpbi->biClrUsed = 1u << lpbi->biBitCount;
	    if (lpbi->biCompression == BI_RGB && lpbi->biSizeImage == 0)
	      lpbi->biSizeImage = DIBWIDTHBYTES(*lpbi) * lpbi->biHeight;
	    if (lpbi->biCompression != BI_RGB && lpbi->biBitCount == 8) {
	      if (pStream->sInfo.fccHandler == mmioFOURCC('R','L','E','0') ||
		  pStream->sInfo.fccHandler == mmioFOURCC('R','L','E',' '))
		lpbi->biCompression = BI_RLE8;
	    }
	    if (lpbi->biCompression == BI_RGB &&
		(pStream->sInfo.fccHandler == 0 ||
		 pStream->sInfo.fccHandler == mmioFOURCC('N','O','N','E')))
	      pStream->sInfo.fccHandler = comptypeDIB;

	    /* init rcFrame if it's empty */
	    if (IsRectEmpty(&pStream->sInfo.rcFrame))
	      SetRect(&pStream->sInfo.rcFrame, 0, 0, lpbi->biWidth, lpbi->biHeight);
	  }
	  break;
	case ckidSTREAMHEADER:
	  {
	    static const WCHAR streamTypeFmt[] = {'%','4','.','4','h','s',0};

	    AVIStreamHeader streamHdr;
	    WCHAR           szType[25];
	    WCHAR           streamNameFmt[25];
	    UINT            count;
	    LONG            n = ck.cksize;

	    if (ck.cksize > sizeof(streamHdr))
	      n = sizeof(streamHdr);

	    if (mmioRead(This->hmmio, (HPSTR)&streamHdr, n) != n)
	      return AVIERR_FILEREAD;

	    pStream->sInfo.fccType               = streamHdr.fccType;
	    pStream->sInfo.fccHandler            = streamHdr.fccHandler;
	    pStream->sInfo.dwFlags               = streamHdr.dwFlags;
	    pStream->sInfo.wPriority             = streamHdr.wPriority;
	    pStream->sInfo.wLanguage             = streamHdr.wLanguage;
	    pStream->sInfo.dwInitialFrames       = streamHdr.dwInitialFrames;
	    pStream->sInfo.dwScale               = streamHdr.dwScale;
	    pStream->sInfo.dwRate                = streamHdr.dwRate;
	    pStream->sInfo.dwStart               = streamHdr.dwStart;
	    pStream->sInfo.dwLength              = streamHdr.dwLength;
	    pStream->sInfo.dwSuggestedBufferSize =
	      streamHdr.dwSuggestedBufferSize;
	    pStream->sInfo.dwQuality             = streamHdr.dwQuality;
	    pStream->sInfo.dwSampleSize          = streamHdr.dwSampleSize;
	    pStream->sInfo.rcFrame.left          = streamHdr.rcFrame.left;
	    pStream->sInfo.rcFrame.top           = streamHdr.rcFrame.top;
	    pStream->sInfo.rcFrame.right         = streamHdr.rcFrame.right;
	    pStream->sInfo.rcFrame.bottom        = streamHdr.rcFrame.bottom;
	    pStream->sInfo.dwEditCount           = 0;
	    pStream->sInfo.dwFormatChangeCount   = 0;

	    /* generate description for stream like "filename.avi Type #n" */
	    if (streamHdr.fccType == streamtypeVIDEO)
	      LoadStringW(AVIFILE_hModule, IDS_VIDEO, szType, sizeof(szType));
	    else if (streamHdr.fccType == streamtypeAUDIO)
	      LoadStringW(AVIFILE_hModule, IDS_AUDIO, szType, sizeof(szType));
	    else
	      wsprintfW(szType, streamTypeFmt, (char*)&streamHdr.fccType);

	    /* get count of this streamtype up to this stream */
	    count = 0;
	    for (n = nStream; 0 <= n; n--) {
	      if (This->ppStreams[n]->sInfo.fccHandler == streamHdr.fccType)
		count++;
	    }

	    memset(pStream->sInfo.szName, 0, sizeof(pStream->sInfo.szName));

	    LoadStringW(AVIFILE_hModule, IDS_AVISTREAMFORMAT, streamNameFmt, sizeof(streamNameFmt));

	    /* FIXME: avoid overflow -- better use wsnprintfW, which doesn't exists ! */
	    wsprintfW(pStream->sInfo.szName, streamNameFmt,
		      AVIFILE_BasenameW(This->szFileName), szType, count);
	  }
	  break;
	case ckidSTREAMNAME:
	  { /* streamname will be saved as ASCII string */
	    LPSTR str = (LPSTR)LocalAlloc(LMEM_FIXED, ck.cksize);
	    if (str == NULL)
	      return AVIERR_MEMORY;

	    if (mmioRead(This->hmmio, (HPSTR)str, ck.cksize) != ck.cksize)
	      return AVIERR_FILEREAD;

	    MultiByteToWideChar(CP_ACP, 0, str, -1, pStream->sInfo.szName,
				sizeof(pStream->sInfo.szName)/sizeof(pStream->sInfo.szName[0]));

	    LocalFree((HLOCAL)str);
	  }
	  break;
	case ckidAVIPADDING:
	case mmioFOURCC('p','a','d','d'):
	  break;
	default:
	  WARN(": found extra chunk 0x%08lX\n", ck.ckid);
	  hr = ReadChunkIntoExtra(&pStream->extra, This->hmmio, &ck);
	  if (FAILED(hr))
	    return hr;
	};

	if (mmioAscend(This->hmmio, &ck, 0) != S_OK)
	  return AVIERR_FILEREAD;
      }
    } else {
      /* nested chunks in "LIST","hdrl" which are not of type "LIST","strl" */
      hr = ReadChunkIntoExtra(&This->fileextra, This->hmmio, &ckLIST2);
      if (FAILED(hr))
	return hr;
    }
    if (mmioAscend(This->hmmio, &ckLIST2, 0) != S_OK)
      return AVIERR_FILEREAD;
  }

  /* read any extra headers in "LIST","hdrl" */
  FindChunkAndKeepExtras(&This->fileextra, This->hmmio, &ck, &ckLIST1, 0);
  if (mmioAscend(This->hmmio, &ckLIST1, 0) != S_OK)
    return AVIERR_FILEREAD;

  /* search "LIST","movi" chunk in "RIFF","AVI " */
  ckLIST1.fccType = listtypeAVIMOVIE;
  hr = FindChunkAndKeepExtras(&This->fileextra, This->hmmio, &ckLIST1, &ckRIFF,
			      MMIO_FINDLIST);
  if (FAILED(hr))
    return hr;

  This->dwMoviChunkPos = ckLIST1.dwDataOffset;
  This->dwIdxChunkPos  = ckLIST1.cksize + ckLIST1.dwDataOffset;
  if (mmioAscend(This->hmmio, &ckLIST1, 0) != S_OK)
    return AVIERR_FILEREAD;

  /* try to find an index */
  ck.ckid = ckidAVINEWINDEX;
  hr = FindChunkAndKeepExtras(&This->fileextra, This->hmmio,
			      &ck, &ckRIFF, MMIO_FINDCHUNK);
  if (SUCCEEDED(hr) && ck.cksize > 0) {
    if (FAILED(AVIFILE_LoadIndex(This, ck.cksize, ckLIST1.dwDataOffset)))
      This->fInfo.dwFlags &= ~AVIFILEINFO_HASINDEX;
  }

  /* when we haven't found an index or it's bad, then build one
   * by parsing 'movi' chunk */
  if ((This->fInfo.dwFlags & AVIFILEINFO_HASINDEX) == 0) {
    for (nStream = 0; nStream < This->fInfo.dwStreams; nStream++)
      This->ppStreams[nStream]->lLastFrame = -1;

    if (mmioSeek(This->hmmio, ckLIST1.dwDataOffset + sizeof(DWORD), SEEK_SET) == -1) {
      ERR(": Oops, can't seek back to 'movi' chunk!\n");
      return AVIERR_FILEREAD;
    }

    /* seek through the 'movi' list until end */
    while (mmioDescend(This->hmmio, &ck, &ckLIST1, 0) == S_OK) {
      if (ck.ckid != FOURCC_LIST) {
	if (mmioAscend(This->hmmio, &ck, 0) == S_OK) {
	  nStream = StreamFromFOURCC(ck.ckid);

	  if (nStream > This->fInfo.dwStreams)
	    return AVIERR_BADFORMAT;

	  AVIFILE_AddFrame(This->ppStreams[nStream], ck.ckid, ck.cksize,
			   ck.dwDataOffset - 2 * sizeof(DWORD), 0);
	} else {
	  nStream = StreamFromFOURCC(ck.ckid);
	  WARN(": file seems to be truncated!\n");
	  if (nStream <= This->fInfo.dwStreams &&
	      This->ppStreams[nStream]->sInfo.dwSampleSize > 0) {
	    ck.cksize = mmioSeek(This->hmmio, 0, SEEK_END);
	    if (ck.cksize != -1) {
	      ck.cksize -= ck.dwDataOffset;
	      ck.cksize &= ~(This->ppStreams[nStream]->sInfo.dwSampleSize - 1);

	      AVIFILE_AddFrame(This->ppStreams[nStream], ck.ckid, ck.cksize,
			       ck.dwDataOffset - 2 * sizeof(DWORD), 0);
	    }
	  }
	}
      }
    }
  }

  /* find other chunks */
  FindChunkAndKeepExtras(&This->fileextra, This->hmmio, &ck, &ckRIFF, 0);

  return AVIERR_OK;
}

static HRESULT AVIFILE_LoadIndex(IAVIFileImpl *This, DWORD size, DWORD offset)
{
  AVIINDEXENTRY *lp;
  DWORD          pos, n;
  HRESULT        hr = AVIERR_OK;
  BOOL           bAbsolute = TRUE;

  lp = (AVIINDEXENTRY*)GlobalAllocPtr(GMEM_MOVEABLE,
				      IDX_PER_BLOCK * sizeof(AVIINDEXENTRY));
  if (lp == NULL)
    return AVIERR_MEMORY;

  /* adjust limits for index tables, so that inserting will be faster */
  for (n = 0; n < This->fInfo.dwStreams; n++) {
    IAVIStreamImpl *pStream = This->ppStreams[n];

    pStream->lLastFrame = -1;

    if (pStream->idxFrames != NULL) {
      GlobalFreePtr(pStream->idxFrames);
      pStream->idxFrames  = NULL;
      pStream->nIdxFrames = 0;
    }

    if (pStream->sInfo.dwSampleSize != 0) {
      if (n > 0 && This->fInfo.dwFlags & AVIFILEINFO_ISINTERLEAVED) {
	pStream->nIdxFrames = This->ppStreams[0]->nIdxFrames;
      } else if (pStream->sInfo.dwSuggestedBufferSize) {
	pStream->nIdxFrames =
	  pStream->sInfo.dwLength / pStream->sInfo.dwSuggestedBufferSize;
      }
    } else
      pStream->nIdxFrames = pStream->sInfo.dwLength;

    pStream->idxFrames =
      (AVIINDEXENTRY*)GlobalAllocPtr(GHND, pStream->nIdxFrames * sizeof(AVIINDEXENTRY));
    if (pStream->idxFrames == NULL && pStream->nIdxFrames > 0) {
      pStream->nIdxFrames = 0;
      return AVIERR_MEMORY;
    }
  }

  pos = (DWORD)-1;
  while (size != 0) {
    LONG read = min(IDX_PER_BLOCK * sizeof(AVIINDEXENTRY), size);

    if (mmioRead(This->hmmio, (HPSTR)lp, read) != read) {
      hr = AVIERR_FILEREAD;
      break;
    }
    size -= read;

    if (pos == (DWORD)-1)
      pos = offset - lp->dwChunkOffset + sizeof(DWORD);

    AVIFILE_ParseIndex(This, lp, read / sizeof(AVIINDEXENTRY),
		       pos, &bAbsolute);
  }

  if (lp != NULL)
    GlobalFreePtr(lp);

  /* checking ... */
  for (n = 0; n < This->fInfo.dwStreams; n++) {
    IAVIStreamImpl *pStream = This->ppStreams[n];

    if (pStream->sInfo.dwSampleSize == 0 &&
	pStream->sInfo.dwLength != pStream->lLastFrame+1)
      ERR("stream %lu length mismatch: dwLength=%lu found=%ld\n",
	   n, pStream->sInfo.dwLength, pStream->lLastFrame);
  }

  return hr;
}

static HRESULT AVIFILE_ParseIndex(IAVIFileImpl *This, AVIINDEXENTRY *lp,
				  LONG count, DWORD pos, BOOL *bAbsolute)
{
  if (lp == NULL)
    return AVIERR_BADPARAM;

  for (; count > 0; count--, lp++) {
    WORD nStream = StreamFromFOURCC(lp->ckid);

    if (lp->ckid == listtypeAVIRECORD || nStream == 0x7F)
      continue; /* skip these */

    if (nStream > This->fInfo.dwStreams)
      return AVIERR_BADFORMAT;

    if (*bAbsolute == TRUE && lp->dwChunkOffset < This->dwMoviChunkPos)
      *bAbsolute = FALSE;

    if (*bAbsolute)
      lp->dwChunkOffset += sizeof(DWORD);
    else
      lp->dwChunkOffset += pos;

    if (FAILED(AVIFILE_AddFrame(This->ppStreams[nStream], lp->ckid, lp->dwChunkLength, lp->dwChunkOffset, lp->dwFlags)))
      return AVIERR_MEMORY;
  }

  return AVIERR_OK;
}

static HRESULT AVIFILE_ReadBlock(IAVIStreamImpl *This, DWORD pos,
				 LPVOID buffer, LONG size)
{
  /* pre-conditions */
  assert(This != NULL);
  assert(This->paf != NULL);
  assert(This->paf->hmmio != NULL);
  assert(This->sInfo.dwStart <= pos && pos < This->sInfo.dwLength);
  assert(pos <= This->lLastFrame);

  /* should we read as much as block gives us? */
  if (size == 0 || size > This->idxFrames[pos].dwChunkLength)
    size = This->idxFrames[pos].dwChunkLength;

  /* read into out own buffer or given one? */
  if (buffer == NULL) {
    /* we also read the chunk */
    size += 2 * sizeof(DWORD);

    /* check that buffer is big enough -- don't trust dwSuggestedBufferSize */
    if (This->lpBuffer == NULL || size < This->cbBuffer) {
      DWORD maxSize = max(size, This->sInfo.dwSuggestedBufferSize);

      if (This->lpBuffer == NULL)
	This->lpBuffer = (LPDWORD)GlobalAllocPtr(GMEM_MOVEABLE, maxSize);
      else
	This->lpBuffer =
	  (LPDWORD)GlobalReAllocPtr(This->lpBuffer, maxSize, GMEM_MOVEABLE);
      if (This->lpBuffer == NULL)
	return AVIERR_MEMORY;
      This->cbBuffer = max(size, This->sInfo.dwSuggestedBufferSize);
    }

    /* now read the complete chunk into our buffer */
    if (mmioSeek(This->paf->hmmio, This->idxFrames[pos].dwChunkOffset, SEEK_SET) == -1)
      return AVIERR_FILEREAD;
    if (mmioRead(This->paf->hmmio, (HPSTR)This->lpBuffer, size) != size)
      return AVIERR_FILEREAD;

    /* check if it was the correct block which we have read */
    if (This->lpBuffer[0] != This->idxFrames[pos].ckid ||
	This->lpBuffer[1] != This->idxFrames[pos].dwChunkLength) {
      ERR(": block %ld not found at 0x%08lX\n", pos, This->idxFrames[pos].dwChunkOffset);
      ERR(": Index says: '%4.4s'(0x%08lX) size 0x%08lX\n",
	  (char*)&This->idxFrames[pos].ckid, This->idxFrames[pos].ckid,
	  This->idxFrames[pos].dwChunkLength);
      ERR(": Data  says: '%4.4s'(0x%08lX) size 0x%08lX\n",
	  (char*)&This->lpBuffer[0], This->lpBuffer[0], This->lpBuffer[1]);
      return AVIERR_FILEREAD;
    }
  } else {
    if (mmioSeek(This->paf->hmmio, This->idxFrames[pos].dwChunkOffset + 2 * sizeof(DWORD), SEEK_SET) == -1)
      return AVIERR_FILEREAD;
    if (mmioRead(This->paf->hmmio, (HPSTR)buffer, size) != size)
      return AVIERR_FILEREAD;
  }

  return AVIERR_OK;
}

static void    AVIFILE_SamplesToBlock(IAVIStreamImpl *This, LPLONG pos,
				      LPLONG offset)
{
  DWORD block;

  /* pre-conditions */
  assert(This != NULL);
  assert(pos != NULL);
  assert(offset != NULL);
  assert(This->sInfo.dwSampleSize != 0);
  assert(*pos >= This->sInfo.dwStart);

  /* convert start sample to start bytes */
  (*offset)  = (*pos) - This->sInfo.dwStart;
  (*offset) *= This->sInfo.dwSampleSize;

  /* convert bytes to block number */
  for (block = 0; block <= This->lLastFrame; block++) {
    if (This->idxFrames[block].dwChunkLength <= *offset)
      (*offset) -= This->idxFrames[block].dwChunkLength;
    else
      break;
  }

  *pos = block;
}

static HRESULT AVIFILE_SaveFile(IAVIFileImpl *This)
{
  MainAVIHeader   MainAVIHdr;
  IAVIStreamImpl* pStream;
  MMCKINFO        ckRIFF;
  MMCKINFO        ckLIST1;
  MMCKINFO        ckLIST2;
  MMCKINFO        ck;
  DWORD           nStream;
  DWORD           dwPos;
  HRESULT         hr;

  /* initialize some things */
  if (This->dwMoviChunkPos == 0)
    AVIFILE_ComputeMoviStart(This);

  /* written one record to much? */
  if (This->ckLastRecord.dwFlags & MMIO_DIRTY) {
    This->dwNextFramePos -= 3 * sizeof(DWORD);
    if (This->nIdxRecords > 0)
      This->nIdxRecords--;
  }

  AVIFILE_UpdateInfo(This);

  assert(This->fInfo.dwScale != 0);

  memset(&MainAVIHdr, 0, sizeof(MainAVIHdr));
  MainAVIHdr.dwMicroSecPerFrame    = MulDiv(This->fInfo.dwRate, 1000000,
					    This->fInfo.dwScale);
  MainAVIHdr.dwMaxBytesPerSec      = This->fInfo.dwMaxBytesPerSec;
  MainAVIHdr.dwPaddingGranularity  = AVI_HEADERSIZE;
  MainAVIHdr.dwFlags               = This->fInfo.dwFlags;
  MainAVIHdr.dwTotalFrames         = This->fInfo.dwLength;
  MainAVIHdr.dwInitialFrames       = 0;
  MainAVIHdr.dwStreams             = This->fInfo.dwStreams;
  MainAVIHdr.dwSuggestedBufferSize = This->fInfo.dwSuggestedBufferSize;
  MainAVIHdr.dwWidth               = This->fInfo.dwWidth;
  MainAVIHdr.dwHeight              = This->fInfo.dwHeight;
  MainAVIHdr.dwInitialFrames       = This->dwInitialFrames;

  /* now begin writing ... */
  mmioSeek(This->hmmio, 0, SEEK_SET);

  /* RIFF chunk */
  ckRIFF.cksize  = 0;
  ckRIFF.fccType = formtypeAVI;
  if (mmioCreateChunk(This->hmmio, &ckRIFF, MMIO_CREATERIFF) != S_OK)
    return AVIERR_FILEWRITE;

  /* AVI headerlist */
  ckLIST1.cksize  = 0;
  ckLIST1.fccType = listtypeAVIHEADER;
  if (mmioCreateChunk(This->hmmio, &ckLIST1, MMIO_CREATELIST) != S_OK)
    return AVIERR_FILEWRITE;

  /* MainAVIHeader */
  ck.ckid    = ckidAVIMAINHDR;
  ck.cksize  = sizeof(MainAVIHdr);
  ck.fccType = 0;
  if (mmioCreateChunk(This->hmmio, &ck, 0) != S_OK)
    return AVIERR_FILEWRITE;
  if (mmioWrite(This->hmmio, (HPSTR)&MainAVIHdr, ck.cksize) != ck.cksize)
    return AVIERR_FILEWRITE;
  if (mmioAscend(This->hmmio, &ck, 0) != S_OK)
    return AVIERR_FILEWRITE;

  /* write the headers of each stream into a separate streamheader list */
  for (nStream = 0; nStream < This->fInfo.dwStreams; nStream++) {
    AVIStreamHeader strHdr;

    pStream = This->ppStreams[nStream];

    /* begin the new streamheader list */
    ckLIST2.cksize  = 0;
    ckLIST2.fccType = listtypeSTREAMHEADER;
    if (mmioCreateChunk(This->hmmio, &ckLIST2, MMIO_CREATELIST) != S_OK)
      return AVIERR_FILEWRITE;

    /* create an AVIStreamHeader from the AVSTREAMINFO */
    strHdr.fccType               = pStream->sInfo.fccType;
    strHdr.fccHandler            = pStream->sInfo.fccHandler;
    strHdr.dwFlags               = pStream->sInfo.dwFlags;
    strHdr.wPriority             = pStream->sInfo.wPriority;
    strHdr.wLanguage             = pStream->sInfo.wLanguage;
    strHdr.dwInitialFrames       = pStream->sInfo.dwInitialFrames;
    strHdr.dwScale               = pStream->sInfo.dwScale;
    strHdr.dwRate                = pStream->sInfo.dwRate;
    strHdr.dwStart               = pStream->sInfo.dwStart;
    strHdr.dwLength              = pStream->sInfo.dwLength;
    strHdr.dwSuggestedBufferSize = pStream->sInfo.dwSuggestedBufferSize;
    strHdr.dwQuality             = pStream->sInfo.dwQuality;
    strHdr.dwSampleSize          = pStream->sInfo.dwSampleSize;
    strHdr.rcFrame.left          = pStream->sInfo.rcFrame.left;
    strHdr.rcFrame.top           = pStream->sInfo.rcFrame.top;
    strHdr.rcFrame.right         = pStream->sInfo.rcFrame.right;
    strHdr.rcFrame.bottom        = pStream->sInfo.rcFrame.bottom;

    /* now write the AVIStreamHeader */
    ck.ckid   = ckidSTREAMHEADER;
    ck.cksize = sizeof(strHdr);
    if (mmioCreateChunk(This->hmmio, &ck, 0) != S_OK)
      return AVIERR_FILEWRITE;
    if (mmioWrite(This->hmmio, (HPSTR)&strHdr, ck.cksize) != ck.cksize)
      return AVIERR_FILEWRITE;
    if (mmioAscend(This->hmmio, &ck, 0) != S_OK)
      return AVIERR_FILEWRITE;

    /* ... the hopefully ever present streamformat ... */
    ck.ckid   = ckidSTREAMFORMAT;
    ck.cksize = pStream->cbFormat;
    if (mmioCreateChunk(This->hmmio, &ck, 0) != S_OK)
      return AVIERR_FILEWRITE;
    if (pStream->lpFormat != NULL && ck.cksize > 0) {
      if (mmioWrite(This->hmmio, (HPSTR)pStream->lpFormat, ck.cksize) != ck.cksize)
	return AVIERR_FILEWRITE;
    }
    if (mmioAscend(This->hmmio, &ck, 0) != S_OK)
      return AVIERR_FILEWRITE;

    /* ... some optional existing handler data ... */
    if (pStream->lpHandlerData != NULL && pStream->cbHandlerData > 0) {
      ck.ckid   = ckidSTREAMHANDLERDATA;
      ck.cksize = pStream->cbHandlerData;
      if (mmioCreateChunk(This->hmmio, &ck, 0) != S_OK)
	return AVIERR_FILEWRITE;
      if (mmioWrite(This->hmmio, (HPSTR)pStream->lpHandlerData, ck.cksize) != ck.cksize)
	return AVIERR_FILEWRITE;
      if (mmioAscend(This->hmmio, &ck, 0) != S_OK)
	return AVIERR_FILEWRITE;
    }

    /* ... some optional additional extra chunk for this stream ... */
    if (pStream->extra.lp != NULL && pStream->extra.cb > 0) {
      /* the chunk header(s) are already in the strucuture */
      if (mmioWrite(This->hmmio, (HPSTR)pStream->extra.lp, pStream->extra.cb) != pStream->extra.cb)
	return AVIERR_FILEWRITE;
    }

    /* ... an optional name for this stream ... */
    if (lstrlenW(pStream->sInfo.szName) > 0) {
      LPSTR str;

      ck.ckid   = ckidSTREAMNAME;
      ck.cksize = lstrlenW(pStream->sInfo.szName) + 1;
      if (ck.cksize & 1) /* align */
	ck.cksize++;
      if (mmioCreateChunk(This->hmmio, &ck, 0) != S_OK)
	return AVIERR_FILEWRITE;

      /* the streamname must be saved in ASCII not Unicode */
      str = (LPSTR)LocalAlloc(LPTR, ck.cksize);
      if (str == NULL)
	return AVIERR_MEMORY;
      WideCharToMultiByte(CP_ACP, 0, pStream->sInfo.szName, -1, str,
			  ck.cksize, NULL, NULL);

      if (mmioWrite(This->hmmio, (HPSTR)str, ck.cksize) != ck.cksize) {
	LocalFree((HLOCAL)str);	
	return AVIERR_FILEWRITE;
      }

      LocalFree((HLOCAL)str);
      if (mmioAscend(This->hmmio, &ck, 0) != S_OK)
	return AVIERR_FILEWRITE;
    }

    /* close streamheader list for this stream */
    if (mmioAscend(This->hmmio, &ckLIST2, 0) != S_OK)
      return AVIERR_FILEWRITE;
  } /* for (0 <= nStream < MainAVIHdr.dwStreams) */

  /* close the aviheader list */
  if (mmioAscend(This->hmmio, &ckLIST1, 0) != S_OK)
    return AVIERR_FILEWRITE;

  /* check for padding to pre-guessed 'movi'-chunk position */
  dwPos = ckLIST1.dwDataOffset + ckLIST1.cksize;
  if (This->dwMoviChunkPos - 2 * sizeof(DWORD) > dwPos) {
    ck.ckid   = ckidAVIPADDING;
    ck.cksize = This->dwMoviChunkPos - dwPos - 4 * sizeof(DWORD);
    assert((LONG)ck.cksize >= 0);

    if (mmioCreateChunk(This->hmmio, &ck, 0) != S_OK)
      return AVIERR_FILEWRITE;
    if (mmioSeek(This->hmmio, ck.cksize, SEEK_CUR) == -1)
      return AVIERR_FILEWRITE;
    if (mmioAscend(This->hmmio, &ck, 0) != S_OK)
      return AVIERR_FILEWRITE;
  }

  /* now write the 'movi' chunk */
  mmioSeek(This->hmmio, This->dwMoviChunkPos - 2 * sizeof(DWORD), SEEK_SET);
  ckLIST1.cksize  = 0;
  ckLIST1.fccType = listtypeAVIMOVIE;
  if (mmioCreateChunk(This->hmmio, &ckLIST1, MMIO_CREATELIST) != S_OK)
    return AVIERR_FILEWRITE;
  if (mmioSeek(This->hmmio, This->dwNextFramePos, SEEK_SET) == -1)
    return AVIERR_FILEWRITE;
  if (mmioAscend(This->hmmio, &ckLIST1, 0) != S_OK)
    return AVIERR_FILEWRITE;

  /* write 'idx1' chunk */
  hr = AVIFILE_SaveIndex(This);
  if (FAILED(hr))
    return hr;

  /* write optional extra file chunks */
  if (This->fileextra.lp != NULL && This->fileextra.cb > 0) {
    /* as for the streams, are the chunk header(s) in the structure */
    if (mmioWrite(This->hmmio, (HPSTR)This->fileextra.lp, This->fileextra.cb) != This->fileextra.cb)
      return AVIERR_FILEWRITE;
  }

  /* close RIFF chunk */
  if (mmioAscend(This->hmmio, &ckRIFF, 0) != S_OK)
    return AVIERR_FILEWRITE;

  /* add some JUNK at end for bad parsers */
  memset(&ckRIFF, 0, sizeof(ckRIFF));
  mmioWrite(This->hmmio, (HPSTR)&ckRIFF, sizeof(ckRIFF));
  mmioFlush(This->hmmio, 0);

  return AVIERR_OK;
}

static HRESULT AVIFILE_SaveIndex(IAVIFileImpl *This)
{
  IAVIStreamImpl *pStream;
  AVIINDEXENTRY   idx;
  MMCKINFO        ck;
  DWORD           nStream;
  LONG            n;

  ck.ckid   = ckidAVINEWINDEX;
  ck.cksize = 0;
  if (mmioCreateChunk(This->hmmio, &ck, 0) != S_OK)
    return AVIERR_FILEWRITE;

  if (This->fInfo.dwFlags & AVIFILEINFO_ISINTERLEAVED) {
    /* is interleaved -- write block of coresponding frames */
    LONG lInitialFrames = 0;
    LONG stepsize;
    LONG i;

    if (This->ppStreams[0]->sInfo.dwSampleSize == 0)
      stepsize = 1;
    else
      stepsize = AVIStreamTimeToSample((PAVISTREAM)This->ppStreams[0], 1000000);

    assert(stepsize > 0);

    for (nStream = 0; nStream < This->fInfo.dwStreams; nStream++) {
      if (lInitialFrames < This->ppStreams[nStream]->sInfo.dwInitialFrames)
	lInitialFrames = This->ppStreams[nStream]->sInfo.dwInitialFrames;
    }

    for (i = -lInitialFrames; i < (LONG)This->fInfo.dwLength - lInitialFrames;
	 i += stepsize) {
      DWORD nFrame = lInitialFrames + i;

      assert(nFrame < This->nIdxRecords);

      idx.ckid          = listtypeAVIRECORD;
      idx.dwFlags       = AVIIF_LIST;
      idx.dwChunkLength = This->idxRecords[nFrame].dwChunkLength;
      idx.dwChunkOffset = This->idxRecords[nFrame].dwChunkOffset
	- This->dwMoviChunkPos;
      if (mmioWrite(This->hmmio, (HPSTR)&idx, sizeof(idx)) != sizeof(idx))
	return AVIERR_FILEWRITE;

      for (nStream = 0; nStream < This->fInfo.dwStreams; nStream++) {
	pStream = This->ppStreams[nStream];

	/* heave we reached start of this stream? */
	if (-(LONG)pStream->sInfo.dwInitialFrames > i)
	  continue;

	if (pStream->sInfo.dwInitialFrames < lInitialFrames)
	  nFrame -= (lInitialFrames - pStream->sInfo.dwInitialFrames);

	/* reached end of this stream? */
	if (pStream->lLastFrame <= nFrame)
	  continue;

	if ((pStream->sInfo.dwFlags & AVISTREAMINFO_FORMATCHANGES) &&
	    pStream->sInfo.dwFormatChangeCount != 0 &&
	    pStream->idxFmtChanges != NULL) {
	  DWORD pos;

	  for (pos = 0; pos < pStream->sInfo.dwFormatChangeCount; pos++) {
	    if (pStream->idxFmtChanges[pos].ckid == nFrame) {
	      idx.dwFlags = AVIIF_NOTIME;
	      idx.ckid    = MAKEAVICKID(cktypePALchange, pStream->nStream);
	      idx.dwChunkLength = pStream->idxFmtChanges[pos].dwChunkLength;
	      idx.dwChunkOffset = pStream->idxFmtChanges[pos].dwChunkOffset
		- This->dwMoviChunkPos;

	      if (mmioWrite(This->hmmio, (HPSTR)&idx, sizeof(idx)) != sizeof(idx))
		return AVIERR_FILEWRITE;
	      break;
	    }
	  }
	} /* if have formatchanges */

	idx.ckid          = pStream->idxFrames[nFrame].ckid;
	idx.dwFlags       = pStream->idxFrames[nFrame].dwFlags;
	idx.dwChunkLength = pStream->idxFrames[nFrame].dwChunkLength;
	idx.dwChunkOffset = pStream->idxFrames[nFrame].dwChunkOffset
	  - This->dwMoviChunkPos;
	if (mmioWrite(This->hmmio, (HPSTR)&idx, sizeof(idx)) != sizeof(idx))
	  return AVIERR_FILEWRITE;
      }
    }
  } else {
    /* not interleaved -- write index for each stream at once */
    for (nStream = 0; nStream < This->fInfo.dwStreams; nStream++) {
      pStream = This->ppStreams[nStream];

      for (n = 0; n <= pStream->lLastFrame; n++) {
	if ((pStream->sInfo.dwFlags & AVISTREAMINFO_FORMATCHANGES) &&
	    (pStream->sInfo.dwFormatChangeCount != 0)) {
	  DWORD pos;

	  for (pos = 0; pos < pStream->sInfo.dwFormatChangeCount; pos++) {
	    if (pStream->idxFmtChanges[pos].ckid == n) {
	      idx.dwFlags = AVIIF_NOTIME;
	      idx.ckid    = MAKEAVICKID(cktypePALchange, pStream->nStream);
	      idx.dwChunkLength = pStream->idxFmtChanges[pos].dwChunkLength;
	      idx.dwChunkOffset =
		pStream->idxFmtChanges[pos].dwChunkOffset - This->dwMoviChunkPos;
	      if (mmioWrite(This->hmmio, (HPSTR)&idx, sizeof(idx)) != sizeof(idx))
		return AVIERR_FILEWRITE;
	      break;
	    }
	  }
	} /* if have formatchanges */

	idx.ckid          = pStream->idxFrames[n].ckid;
	idx.dwFlags       = pStream->idxFrames[n].dwFlags;
	idx.dwChunkLength = pStream->idxFrames[n].dwChunkLength;
	idx.dwChunkOffset = pStream->idxFrames[n].dwChunkOffset
	  - This->dwMoviChunkPos;

	if (mmioWrite(This->hmmio, (HPSTR)&idx, sizeof(idx)) != sizeof(idx))
	  return AVIERR_FILEWRITE;
      }
    }
  } /* if not interleaved */

  if (mmioAscend(This->hmmio, &ck, 0) != S_OK)
    return AVIERR_FILEWRITE;

  return AVIERR_OK;
}

static ULONG  AVIFILE_SearchStream(IAVIFileImpl *This, DWORD fcc, LONG lSkip)
{
  UINT i;
  UINT nStream;

  /* pre-condition */
  assert(lSkip >= 0);

  if (fcc != 0) {
    /* search the number of the specified stream */
    nStream = (ULONG)-1;
    for (i = 0; i < This->fInfo.dwStreams; i++) {
      assert(This->ppStreams[i] != NULL);

      if (This->ppStreams[i]->sInfo.fccType == fcc) {
	if (lSkip == 0) {
	  nStream = i;
	  break;
	} else
	  lSkip--;
      }
    }
  } else
    nStream = lSkip;

  return nStream;
}

static void    AVIFILE_UpdateInfo(IAVIFileImpl *This)
{
  UINT i;

  /* pre-conditions */
  assert(This != NULL);

  This->fInfo.dwMaxBytesPerSec      = 0;
  This->fInfo.dwCaps                = AVIFILECAPS_CANREAD|AVIFILECAPS_CANWRITE;
  This->fInfo.dwSuggestedBufferSize = 0;
  This->fInfo.dwWidth               = 0;
  This->fInfo.dwHeight              = 0;
  This->fInfo.dwScale               = 0;
  This->fInfo.dwRate                = 0;
  This->fInfo.dwLength              = 0;
  This->dwInitialFrames             = 0;

  for (i = 0; i < This->fInfo.dwStreams; i++) {
    AVISTREAMINFOW *psi;
    DWORD           n;

    /* pre-conditions */
    assert(This->ppStreams[i] != NULL);

    psi = &This->ppStreams[i]->sInfo;
    assert(psi->dwScale != 0);
    assert(psi->dwRate != 0);

    if (i == 0) {
      /* use first stream timings as base */
      This->fInfo.dwScale  = psi->dwScale;
      This->fInfo.dwRate   = psi->dwRate;
      This->fInfo.dwLength = psi->dwLength;
    } else {
      n = AVIStreamSampleToSample((PAVISTREAM)This->ppStreams[0],
				  (PAVISTREAM)This->ppStreams[i],psi->dwLength);
      if (This->fInfo.dwLength < n)
	This->fInfo.dwLength = n;
    }

    if (This->dwInitialFrames < psi->dwInitialFrames)
      This->dwInitialFrames = psi->dwInitialFrames;

    if (This->fInfo.dwSuggestedBufferSize < psi->dwSuggestedBufferSize)
      This->fInfo.dwSuggestedBufferSize = psi->dwSuggestedBufferSize;

    if (psi->dwSampleSize != 0) {
      /* fixed sample size -- exact computation */
      This->fInfo.dwMaxBytesPerSec += MulDiv(psi->dwSampleSize, psi->dwRate,
					     psi->dwScale);
    } else {
      /* variable sample size -- only upper limit */
      This->fInfo.dwMaxBytesPerSec += MulDiv(psi->dwSuggestedBufferSize,
					     psi->dwRate, psi->dwScale);

      /* update dimensions */
      n = psi->rcFrame.right - psi->rcFrame.left;
      if (This->fInfo.dwWidth < n)
	This->fInfo.dwWidth = n;
      n = psi->rcFrame.bottom - psi->rcFrame.top;
      if (This->fInfo.dwHeight < n)
	This->fInfo.dwHeight = n;
    }
  }
}

static HRESULT AVIFILE_WriteBlock(IAVIStreamImpl *This, DWORD block,
				  FOURCC ckid, DWORD flags, LPVOID buffer,
				  LONG size)
{
  MMCKINFO ck;

  ck.ckid    = ckid;
  ck.cksize  = size;
  ck.fccType = 0;

  /* if no frame/block is already written, we must compute start of movi chunk */
  if (This->paf->dwMoviChunkPos == 0)
    AVIFILE_ComputeMoviStart(This->paf);

  if (mmioSeek(This->paf->hmmio, This->paf->dwNextFramePos, SEEK_SET) == -1)
    return AVIERR_FILEWRITE;

  if (mmioCreateChunk(This->paf->hmmio, &ck, 0) != S_OK)
    return AVIERR_FILEWRITE;
  if (buffer != NULL && size > 0) {
    if (mmioWrite(This->paf->hmmio, (HPSTR)buffer, size) != size)
      return AVIERR_FILEWRITE;
  }
  if (mmioAscend(This->paf->hmmio, &ck, 0) != S_OK)
    return AVIERR_FILEWRITE;

  This->paf->fDirty         = TRUE;
  This->paf->dwNextFramePos = mmioSeek(This->paf->hmmio, 0, SEEK_CUR);

  return AVIFILE_AddFrame(This, ckid, size,
			  ck.dwDataOffset - 2 * sizeof(DWORD), flags);
}
