/*
 * Memory Allocator and Media Sample Implementation
 *
 * Copyright 2003 Robert Shearman
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

#include <assert.h>
#include <limits.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "vfwmsgs.h"

#include "quartz_private.h"
#include "wine/list.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(quartz);

void dump_AM_SAMPLE2_PROPERTIES(AM_SAMPLE2_PROPERTIES * pProps)
{
    if (!pProps)
    {
        TRACE("AM_SAMPLE2_PROPERTIES: (null)\n");
        return;
    }
    TRACE("\tcbData: %ld\n", pProps->cbData);
    TRACE("\tdwTypeSpecificFlags: 0x%8lx\n", pProps->dwTypeSpecificFlags);
    TRACE("\tdwSampleFlags: 0x%8lx\n", pProps->dwSampleFlags);
    TRACE("\tlActual: %ld\n", pProps->lActual);
    TRACE("\ttStart: %lx%08lx%s\n", (LONG)(pProps->tStart >> 32), (LONG)pProps->tStart, pProps->dwSampleFlags & AM_SAMPLE_TIMEVALID ? "" : " (not valid)");
    TRACE("\ttStop: %lx%08lx%s\n", (LONG)(pProps->tStop >> 32), (LONG)pProps->tStop, pProps->dwSampleFlags & AM_SAMPLE_STOPVALID ? "" : " (not valid)");
    TRACE("\tdwStreamId: 0x%lx\n", pProps->dwStreamId);
    TRACE("\tpMediaType: %p\n", pProps->pMediaType);
    TRACE("\tpbBuffer: %p\n", pProps->pbBuffer);
    TRACE("\tcbBuffer: %ld\n", pProps->cbBuffer);
}

typedef struct BaseMemAllocator
{
    const IMemAllocatorVtbl * lpVtbl;

    ULONG ref;
    ALLOCATOR_PROPERTIES * pProps;
    CRITICAL_SECTION csState;
    HRESULT (* fnAlloc) (IMemAllocator *);
    HRESULT (* fnFree)(IMemAllocator *);
    HANDLE hSemWaiting;
    BOOL bDecommitQueued;
    BOOL bCommitted;
    LONG lWaiting;
    struct list free_list;
    struct list used_list;
} BaseMemAllocator;

typedef struct StdMediaSample2
{
    const IMediaSample2Vtbl * lpvtbl;

    ULONG ref;
    AM_SAMPLE2_PROPERTIES props;
    IMemAllocator * pParent;
    struct list listentry;
    LONGLONG tMediaStart;
    LONGLONG tMediaEnd;
} StdMediaSample2;

static const struct IMemAllocatorVtbl BaseMemAllocator_VTable;
static const struct IMediaSample2Vtbl StdMediaSample2_VTable;

#define AM_SAMPLE2_PROP_SIZE_WRITABLE (unsigned int)(&((AM_SAMPLE2_PROPERTIES *)0)->pbBuffer)

#ifdef _I64_MAX
#define INVALID_MEDIA_TIME _I64_MAX
#else
#define INVALID_MEDIA_TIME ((long long)(~0ull >> 1))
#endif

static HRESULT BaseMemAllocator_Init(HRESULT (* fnAlloc)(IMemAllocator *), HRESULT (* fnFree)(IMemAllocator *), BaseMemAllocator * pMemAlloc)
{
    assert(fnAlloc && fnFree);

    pMemAlloc->lpVtbl = &BaseMemAllocator_VTable;

    pMemAlloc->ref = 0;
    pMemAlloc->pProps = NULL;
    list_init(&pMemAlloc->free_list);
    list_init(&pMemAlloc->used_list);
    pMemAlloc->fnAlloc = fnAlloc;
    pMemAlloc->fnFree = fnFree;
    pMemAlloc->bDecommitQueued = FALSE;
    pMemAlloc->bCommitted = FALSE;
    pMemAlloc->hSemWaiting = NULL;
    pMemAlloc->lWaiting = 0;

    InitializeCriticalSection(&pMemAlloc->csState);

    return S_OK;
}

static HRESULT WINAPI BaseMemAllocator_QueryInterface(IMemAllocator * iface, REFIID riid, LPVOID * ppv)
{
    ICOM_THIS(BaseMemAllocator, iface);
    TRACE("(%s, %p)\n", qzdebugstr_guid(riid), ppv);

    *ppv = NULL;

    if (IsEqualIID(riid, &IID_IUnknown))
        *ppv = (LPVOID)This;
    else if (IsEqualIID(riid, &IID_IMemAllocator))
        *ppv = (LPVOID)This;

    if (*ppv)
    {
        IUnknown_AddRef((IUnknown *)(*ppv));
        return S_OK;
    }

    FIXME("No interface for %s!\n", qzdebugstr_guid(riid));

    return E_NOINTERFACE;
}

static ULONG WINAPI BaseMemAllocator_AddRef(IMemAllocator * iface)
{
    ICOM_THIS(BaseMemAllocator, iface);

    TRACE("()\n");

    return InterlockedIncrement(&This->ref);
}

static ULONG WINAPI BaseMemAllocator_Release(IMemAllocator * iface)
{
    ICOM_THIS(BaseMemAllocator, iface);

    TRACE("()\n");

    if (!InterlockedDecrement(&This->ref))
    {
        CloseHandle(This->hSemWaiting);
        if (This->bCommitted)
            This->fnFree(iface);
        if (This->pProps)
            HeapFree(GetProcessHeap(), 0, This->pProps);
        CoTaskMemFree(This);
        return 0;
    }
    return This->ref;
}

static HRESULT WINAPI BaseMemAllocator_SetProperties(IMemAllocator * iface, ALLOCATOR_PROPERTIES *pRequest, ALLOCATOR_PROPERTIES *pActual)
{
    ICOM_THIS(BaseMemAllocator, iface);
    HRESULT hr;

    TRACE("(%p, %p)\n", pRequest, pActual);

    EnterCriticalSection(&This->csState);
    {
        if (!list_empty(&This->used_list))
            hr = VFW_E_BUFFERS_OUTSTANDING;
        else if (This->bCommitted)
            hr = VFW_E_ALREADY_COMMITTED;
        else
        {
            if (!This->pProps)
                This->pProps = HeapAlloc(GetProcessHeap(), 0, sizeof(*This->pProps));

            if (!This->pProps)
                hr = E_OUTOFMEMORY;
            else
            {
                memcpy(This->pProps, pRequest, sizeof(*This->pProps));
                    
                memcpy(pActual, pRequest, sizeof(*pActual));

                hr = S_OK;
            }
        }
    }
    LeaveCriticalSection(&This->csState);

    return hr;
}

static HRESULT WINAPI BaseMemAllocator_GetProperties(IMemAllocator * iface, ALLOCATOR_PROPERTIES *pProps)
{
    ICOM_THIS(BaseMemAllocator, iface);
    HRESULT hr = S_OK;

    TRACE("(%p)\n", pProps);

    EnterCriticalSection(&This->csState);
    {
        /* NOTE: this is different from the native version.
         * It would silently succeed if the properties had
         * not been set, but would fail further on down the
         * line with some obscure error like having an
         * invalid alignment. Whether or not our version
         * will cause any problems remains to be seen */
        if (!This->pProps)
            hr = VFW_E_SIZENOTSET;
        else
            memcpy(pProps, This->pProps, sizeof(*pProps));
    }
    LeaveCriticalSection(&This->csState);

    return hr;
}

static HRESULT WINAPI BaseMemAllocator_Commit(IMemAllocator * iface)
{
    ICOM_THIS(BaseMemAllocator, iface);
    HRESULT hr;

    TRACE("()\n");

    EnterCriticalSection(&This->csState);
    {
        if (!This->pProps)
            hr = VFW_E_SIZENOTSET;
        else if (This->bCommitted)
            hr = VFW_E_ALREADY_COMMITTED;
        else if (This->bDecommitQueued)
        {
            This->bDecommitQueued = FALSE;
            hr = S_OK;
        }
        else
        {
            if (!(This->hSemWaiting = CreateSemaphoreW(NULL, This->pProps->cBuffers, This->pProps->cBuffers, NULL)))
            {
                ERR("Couldn't create semaphore (error was %ld)\n", GetLastError());
                hr = HRESULT_FROM_WIN32(GetLastError());
            }
            else
            {
                hr = This->fnAlloc(iface);
                if (SUCCEEDED(hr))
                    This->bCommitted = TRUE;
                else
                    ERR("fnAlloc failed with error 0x%lx\n", hr);
            }
        }
    }
    LeaveCriticalSection(&This->csState);

    return hr;
}

static HRESULT WINAPI BaseMemAllocator_Decommit(IMemAllocator * iface)
{
    ICOM_THIS(BaseMemAllocator, iface);
    HRESULT hr;

    TRACE("()\n");

    EnterCriticalSection(&This->csState);
    {
        if (!This->bCommitted)
            hr = VFW_E_NOT_COMMITTED;
        else
        {
            if (!list_empty(&This->used_list))
            {
                This->bDecommitQueued = TRUE;
                /* notify ALL waiting threads that they cannot be allocated a buffer any more */
                ReleaseSemaphore(This->hSemWaiting, This->lWaiting, NULL);
                
                hr = S_OK;
            }
            else
            {
                assert(This->lWaiting == 0);

                This->bCommitted = FALSE;
                CloseHandle(This->hSemWaiting);
                This->hSemWaiting = NULL;

                hr = This->fnFree(iface);
                if (FAILED(hr))
                    ERR("fnFree failed with error 0x%lx\n", hr);
            }
        }
    }
    LeaveCriticalSection(&This->csState);

    return hr;
}

static HRESULT WINAPI BaseMemAllocator_GetBuffer(IMemAllocator * iface, IMediaSample ** pSample, REFERENCE_TIME *pStartTime, REFERENCE_TIME *pEndTime, DWORD dwFlags)
{
    ICOM_THIS(BaseMemAllocator, iface);
    HRESULT hr = S_OK;

    /* NOTE: The pStartTime and pEndTime parameters are not applied to the sample. 
     * The allocator might use these values to determine which buffer it retrieves */
    
    TRACE("(%p, %p, %p, %lx)\n", pSample, pStartTime, pEndTime, dwFlags);

    *pSample = NULL;

    if (!This->bCommitted)
        return VFW_E_NOT_COMMITTED;

    This->lWaiting++;
    if (WaitForSingleObject(This->hSemWaiting, (dwFlags & AM_GBF_NOWAIT) ? 0 : INFINITE) != WAIT_OBJECT_0)
    {
        This->lWaiting--;
        return VFW_E_TIMEOUT;
    }
    This->lWaiting--;

    EnterCriticalSection(&This->csState);
    {
        if (!This->bCommitted)
            hr = VFW_E_NOT_COMMITTED;
        else if (This->bDecommitQueued)
            hr = VFW_E_TIMEOUT;
        else
        {
            struct list * free = list_head(&This->free_list);
            list_remove(free);
            list_add_head(&This->used_list, free);

            *pSample = (IMediaSample *)LIST_ENTRY(free, StdMediaSample2, listentry);

            assert(((StdMediaSample2 *)*pSample)->ref == 0);

            IMediaSample_AddRef(*pSample);
        }
    }
    LeaveCriticalSection(&This->csState);

    return hr;
}

static HRESULT WINAPI BaseMemAllocator_ReleaseBuffer(IMemAllocator * iface, IMediaSample * pSample)
{
    ICOM_THIS(BaseMemAllocator, iface);
    StdMediaSample2 * pStdSample = (StdMediaSample2 *)pSample;
    HRESULT hr = S_OK;
    
    TRACE("(%p)\n", pSample);

    /* FIXME: make sure that sample is currently on the used list */

    /* FIXME: we should probably check the ref count on the sample before freeing
     * it to make sure that it is not still in use */
    EnterCriticalSection(&This->csState);
    {
        if (!This->bCommitted)
            ERR("Releasing a buffer when the allocator is not committed?!?\n");

		/* remove from used_list */
        list_remove(&pStdSample->listentry);

        list_add_head(&This->free_list, &pStdSample->listentry);

        if (list_empty(&This->used_list) && This->bDecommitQueued && This->bCommitted)
        {
            HRESULT hrfree;

            assert(This->lWaiting == 0);

            This->bCommitted = FALSE;
            This->bDecommitQueued = FALSE;

            CloseHandle(This->hSemWaiting);
            This->hSemWaiting = NULL;
            
            if (FAILED(hrfree = This->fnFree(iface)))
                ERR("fnFree failed with error 0x%lx\n", hrfree);
        }
    }
    LeaveCriticalSection(&This->csState);

    /* notify a waiting thread that there is now a free buffer */
    if (!ReleaseSemaphore(This->hSemWaiting, 1, NULL))
    {
        ERR("ReleaseSemaphore failed with error %ld\n", GetLastError());
        hr = HRESULT_FROM_WIN32(GetLastError());
    }

    return hr;
}

static const IMemAllocatorVtbl BaseMemAllocator_VTable = 
{
    ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
    BaseMemAllocator_QueryInterface,
    BaseMemAllocator_AddRef,
    BaseMemAllocator_Release,
    BaseMemAllocator_SetProperties,
    BaseMemAllocator_GetProperties,
    BaseMemAllocator_Commit,
    BaseMemAllocator_Decommit,
    BaseMemAllocator_GetBuffer,
    BaseMemAllocator_ReleaseBuffer
};

static HRESULT StdMediaSample2_Construct(BYTE * pbBuffer, LONG cbBuffer, IMemAllocator * pParent, StdMediaSample2 ** ppSample)
{
    assert(pbBuffer && pParent && (cbBuffer > 0));

    if (!(*ppSample = CoTaskMemAlloc(sizeof(StdMediaSample2))))
        return E_OUTOFMEMORY;

    (*ppSample)->lpvtbl = &StdMediaSample2_VTable;
    (*ppSample)->ref = 0;
    ZeroMemory(&(*ppSample)->props, sizeof((*ppSample)->props));

    /* NOTE: no need to AddRef as the parent is guaranteed to be around
     * at least as long as us and we don't want to create circular
     * dependencies on the ref count */
    (*ppSample)->pParent = pParent;
    (*ppSample)->props.cbData = sizeof(AM_SAMPLE2_PROPERTIES);
    (*ppSample)->props.cbBuffer = (*ppSample)->props.lActual = cbBuffer;
    (*ppSample)->props.pbBuffer = pbBuffer;
    (*ppSample)->tMediaStart = INVALID_MEDIA_TIME;
    (*ppSample)->tMediaEnd = 0;

    return S_OK;
}

static void StdMediaSample2_Delete(StdMediaSample2 * This)
{
    /* NOTE: does not remove itself from the list it belongs to */
    CoTaskMemFree(This);
}

static HRESULT WINAPI StdMediaSample2_QueryInterface(IMediaSample2 * iface, REFIID riid, LPVOID * ppv)
{
    ICOM_THIS(StdMediaSample2, iface);
    TRACE("(%s, %p)\n", qzdebugstr_guid(riid), ppv);

    *ppv = NULL;

    if (IsEqualIID(riid, &IID_IUnknown))
        *ppv = (LPVOID)This;
    else if (IsEqualIID(riid, &IID_IMediaSample))
        *ppv = (LPVOID)This;
    else if (IsEqualIID(riid, &IID_IMediaSample2))
        *ppv = (LPVOID)This;

    if (*ppv)
    {
        IUnknown_AddRef((IUnknown *)(*ppv));
        return S_OK;
    }

    FIXME("No interface for %s!\n", qzdebugstr_guid(riid));

    return E_NOINTERFACE;
}

static ULONG WINAPI StdMediaSample2_AddRef(IMediaSample2 * iface)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("()\n");

    return InterlockedIncrement(&This->ref);
}

static ULONG WINAPI StdMediaSample2_Release(IMediaSample2 * iface)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("()\n");

    if (!InterlockedDecrement(&This->ref))
    {
        IMemAllocator_ReleaseBuffer(This->pParent, (IMediaSample *)iface);
        return 0;
    }
    return This->ref;
}

static HRESULT WINAPI StdMediaSample2_GetPointer(IMediaSample2 * iface, BYTE ** ppBuffer)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("(%p)\n", ppBuffer);

    *ppBuffer = This->props.pbBuffer;

    return S_OK;
}

static long WINAPI StdMediaSample2_GetSize(IMediaSample2 * iface)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("StdMediaSample2_GetSize()\n");

    return This->props.cbBuffer;
}

static HRESULT WINAPI StdMediaSample2_GetTime(IMediaSample2 * iface, REFERENCE_TIME * pStart, REFERENCE_TIME * pEnd)
{
    HRESULT hr;
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("(%p, %p)\n", pStart, pEnd);

    if (!(This->props.dwSampleFlags & AM_SAMPLE_TIMEVALID))
        hr = VFW_E_SAMPLE_TIME_NOT_SET;
    else if (!(This->props.dwSampleFlags & AM_SAMPLE_STOPVALID))
    {
        *pStart = This->props.tStart;
        *pEnd = This->props.tStart + 1;
        
        hr = VFW_S_NO_STOP_TIME;
    }
    else
    {
        *pStart = This->props.tStart;
        *pEnd = This->props.tStop;

        hr = S_OK;
    }

    return S_OK;
}

static HRESULT WINAPI StdMediaSample2_SetTime(IMediaSample2 * iface, REFERENCE_TIME * pStart, REFERENCE_TIME * pEnd)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("(%p, %p)\n", pStart, pEnd);

    if (pStart)
    {
        This->props.tStart = *pStart;
        This->props.dwSampleFlags |= AM_SAMPLE_TIMEVALID;
    }
    else
        This->props.dwSampleFlags &= ~AM_SAMPLE_TIMEVALID;

    if (pEnd)
    {
        This->props.tStop = *pEnd;
        This->props.dwSampleFlags |= AM_SAMPLE_STOPVALID;
    }
    else
        This->props.dwSampleFlags &= ~AM_SAMPLE_STOPVALID;

    return S_OK;
}

static HRESULT WINAPI StdMediaSample2_IsSyncPoint(IMediaSample2 * iface)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("()\n");

    return (This->props.dwSampleFlags & AM_SAMPLE_SPLICEPOINT) ? S_OK : S_FALSE;
}

static HRESULT WINAPI StdMediaSample2_SetSyncPoint(IMediaSample2 * iface, BOOL bIsSyncPoint)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("(%s)\n", bIsSyncPoint ? "TRUE" : "FALSE");

    This->props.dwSampleFlags = (This->props.dwSampleFlags & ~AM_SAMPLE_SPLICEPOINT) | bIsSyncPoint ? AM_SAMPLE_SPLICEPOINT : 0;

    return S_OK;
}

static HRESULT WINAPI StdMediaSample2_IsPreroll(IMediaSample2 * iface)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("()\n");

    return (This->props.dwSampleFlags & AM_SAMPLE_PREROLL) ? S_OK : S_FALSE;
}

static HRESULT WINAPI StdMediaSample2_SetPreroll(IMediaSample2 * iface, BOOL bIsPreroll)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("(%s)\n", bIsPreroll ? "TRUE" : "FALSE");

    This->props.dwSampleFlags = (This->props.dwSampleFlags & ~AM_SAMPLE_PREROLL) | bIsPreroll ? AM_SAMPLE_PREROLL : 0;

    return S_OK;
}

static LONG WINAPI StdMediaSample2_GetActualDataLength(IMediaSample2 * iface)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("()\n");

    return This->props.lActual;
}

static HRESULT WINAPI StdMediaSample2_SetActualDataLength(IMediaSample2 * iface, LONG len)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("(%ld)\n", len);

    if ((len > This->props.cbBuffer) || (len < 0))
        return VFW_E_BUFFER_OVERFLOW;
    else
    {
        This->props.lActual = len;
        return S_OK;
    }
}

static HRESULT WINAPI StdMediaSample2_GetMediaType(IMediaSample2 * iface, AM_MEDIA_TYPE ** ppMediaType)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("(%p)\n", ppMediaType);

    if (!This->props.pMediaType)
        return S_FALSE;

    if (!(*ppMediaType = CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE))))
        return E_OUTOFMEMORY;

    return CopyMediaType(*ppMediaType, This->props.pMediaType);
}

static HRESULT WINAPI StdMediaSample2_SetMediaType(IMediaSample2 * iface, AM_MEDIA_TYPE * pMediaType)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("(%p)\n", pMediaType);

    if (This->props.pMediaType)
        DeleteMediaType(This->props.pMediaType);
    else if (!(This->props.pMediaType = CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE))))
        return E_OUTOFMEMORY;

    return CopyMediaType(This->props.pMediaType, pMediaType);
}

static HRESULT WINAPI StdMediaSample2_IsDiscontinuity(IMediaSample2 * iface)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("()\n");

    return (This->props.dwSampleFlags & AM_SAMPLE_DATADISCONTINUITY) ? S_OK : S_FALSE;
}

static HRESULT WINAPI StdMediaSample2_SetDiscontinuity(IMediaSample2 * iface, BOOL bIsDiscontinuity)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("(%s)\n", bIsDiscontinuity ? "TRUE" : "FALSE");

    This->props.dwSampleFlags = (This->props.dwSampleFlags & ~AM_SAMPLE_DATADISCONTINUITY) | bIsDiscontinuity ? AM_SAMPLE_DATADISCONTINUITY : 0;

    return S_OK;
}

static HRESULT WINAPI StdMediaSample2_GetMediaTime(IMediaSample2 * iface, LONGLONG * pStart, LONGLONG * pEnd)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("(%p, %p)\n", pStart, pEnd);

    if (This->tMediaStart == INVALID_MEDIA_TIME)
        return VFW_E_MEDIA_TIME_NOT_SET;

    *pStart = This->tMediaStart;
    *pEnd = This->tMediaEnd;

    return E_NOTIMPL;
}

static HRESULT WINAPI StdMediaSample2_SetMediaTime(IMediaSample2 * iface, LONGLONG * pStart, LONGLONG * pEnd)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("(%p, %p)\n", pStart, pEnd);

    if (pStart)
        This->tMediaStart = *pStart;
    else
        This->tMediaStart = INVALID_MEDIA_TIME;

    if (pEnd)
        This->tMediaEnd = *pEnd;
    else
        This->tMediaEnd = 0;

    return S_OK;
}

static HRESULT WINAPI StdMediaSample2_GetProperties(IMediaSample2 * iface, DWORD cbProperties, BYTE * pbProperties)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("(%ld, %p)\n", cbProperties, pbProperties);

    memcpy(pbProperties, &This->props, min(cbProperties, sizeof(This->props)));

    return S_OK;
}

static HRESULT WINAPI StdMediaSample2_SetProperties(IMediaSample2 * iface, DWORD cbProperties, const BYTE * pbProperties)
{
    ICOM_THIS(StdMediaSample2, iface);

    TRACE("(%ld, %p)\n", cbProperties, pbProperties);

    /* NOTE: pbBuffer and cbBuffer are read-only */
    memcpy(&This->props, pbProperties, min(cbProperties, AM_SAMPLE2_PROP_SIZE_WRITABLE));

    return S_OK;
}

static const IMediaSample2Vtbl StdMediaSample2_VTable = 
{
    ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
    StdMediaSample2_QueryInterface,
    StdMediaSample2_AddRef,
    StdMediaSample2_Release,
    StdMediaSample2_GetPointer,
    StdMediaSample2_GetSize,
    StdMediaSample2_GetTime,
    StdMediaSample2_SetTime,
    StdMediaSample2_IsSyncPoint,
    StdMediaSample2_SetSyncPoint,
    StdMediaSample2_IsPreroll,
    StdMediaSample2_SetPreroll,
    StdMediaSample2_GetActualDataLength,
    StdMediaSample2_SetActualDataLength,
    StdMediaSample2_GetMediaType,
    StdMediaSample2_SetMediaType,
    StdMediaSample2_IsDiscontinuity,
    StdMediaSample2_SetDiscontinuity,
    StdMediaSample2_GetMediaTime,
    StdMediaSample2_SetMediaTime,
    StdMediaSample2_GetProperties,
    StdMediaSample2_SetProperties
};

typedef struct StdMemAllocator
{
    BaseMemAllocator base;
    LPVOID pMemory;
} StdMemAllocator;

static HRESULT StdMemAllocator_Alloc(IMemAllocator * iface)
{
    ICOM_THIS(StdMemAllocator, iface);
    StdMediaSample2 * pSample = NULL;
    SYSTEM_INFO si;
    long i;

    assert(list_empty(&This->base.free_list));

    /* check alignment */
    GetSystemInfo(&si);

    /* we do not allow a courser alignment than the OS page size */
    if ((si.dwPageSize % This->base.pProps->cbAlign) != 0)
        return VFW_E_BADALIGN;

    /* FIXME: each sample has to have its buffer start on the right alignment.
     * We don't do this at the moment */

    /* allocate memory */
    This->pMemory = VirtualAlloc(NULL, (This->base.pProps->cbBuffer + This->base.pProps->cbPrefix) * This->base.pProps->cBuffers, MEM_COMMIT, PAGE_READWRITE);

    for (i = This->base.pProps->cBuffers - 1; i >= 0; i--)
    {
        /* pbBuffer does not start at the base address, it starts at base + cbPrefix */
        BYTE * pbBuffer = (BYTE *)This->pMemory + i * (This->base.pProps->cbBuffer + This->base.pProps->cbPrefix) + This->base.pProps->cbPrefix;
        
        StdMediaSample2_Construct(pbBuffer, This->base.pProps->cbBuffer, iface, &pSample);

        list_add_head(&This->base.free_list, &pSample->listentry);
    }

    return S_OK;
}

static HRESULT StdMemAllocator_Free(IMemAllocator * iface)
{
    ICOM_THIS(StdMemAllocator, iface);
    struct list * cursor;

    assert(list_empty(&This->base.used_list));

    while ((cursor = list_head(&This->base.free_list)) != NULL)
    {
        list_remove(cursor);
        StdMediaSample2_Delete(LIST_ENTRY(cursor, StdMediaSample2, listentry));
    }
    
    /* free memory */
    if (!VirtualFree(This->pMemory, 0, MEM_RELEASE))
    {
        ERR("Couldn't free memory. Error: %ld\n", GetLastError());
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}

HRESULT StdMemAllocator_create(LPUNKNOWN lpUnkOuter, LPVOID * ppv)
{
    StdMemAllocator * pMemAlloc;
    HRESULT hr;

    *ppv = NULL;
    
    if (lpUnkOuter)
        return CLASS_E_NOAGGREGATION;

    if (!(pMemAlloc = CoTaskMemAlloc(sizeof(*pMemAlloc))))
        return E_OUTOFMEMORY;

    pMemAlloc->pMemory = NULL;

    if (SUCCEEDED(hr = BaseMemAllocator_Init(StdMemAllocator_Alloc, StdMemAllocator_Free, &pMemAlloc->base)))
        *ppv = (LPVOID)pMemAlloc;
    else
        CoTaskMemFree(pMemAlloc);

    return hr;
}
