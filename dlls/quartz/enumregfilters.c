/*
 * Implementation of IEnumRegFilters Interface
 *
 * Copyright 2004 Christian Costa
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

#include "quartz_private.h"

#include "wine/unicode.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(quartz);

typedef struct IEnumRegFiltersImpl
{
    const IEnumRegFiltersVtbl * lpVtbl;
    ULONG refCount;
    ULONG size;
    REGFILTER* RegFilters;
    ULONG uIndex;
} IEnumRegFiltersImpl;

static const struct IEnumRegFiltersVtbl IEnumRegFiltersImpl_Vtbl;

HRESULT IEnumRegFiltersImpl_Construct(REGFILTER* pInRegFilters, const ULONG size, IEnumRegFilters ** ppEnum)
{
    IEnumRegFiltersImpl* pEnumRegFilters;
    REGFILTER* pRegFilters = NULL;
    int i;

    TRACE("(%p, %ld, %p)\n", pInRegFilters, size, ppEnum);

    pEnumRegFilters = CoTaskMemAlloc(sizeof(IEnumRegFiltersImpl));
    if (!pEnumRegFilters)
    {
        *ppEnum = NULL;
        return E_OUTOFMEMORY;
    }

    /* Accept size of 0 */
    if (size)
    {
        pRegFilters = CoTaskMemAlloc(sizeof(REGFILTER)*size);
        if (!pRegFilters)
	{
            CoTaskMemFree(pEnumRegFilters);
            *ppEnum = NULL;
           return E_OUTOFMEMORY;
        }
    }

    for(i = 0; i < size; i++)
    {
        pRegFilters[i].Clsid = pInRegFilters[i].Clsid;
        pRegFilters[i].Name = (WCHAR*)CoTaskMemAlloc((strlenW(pInRegFilters[i].Name)+1)*sizeof(WCHAR));
        if (!pRegFilters[i].Name)
        {
            while(i)
                CoTaskMemFree(pRegFilters[--i].Name);
            CoTaskMemFree(pRegFilters);
            CoTaskMemFree(pEnumRegFilters);
            return E_OUTOFMEMORY;
        }
        CopyMemory(pRegFilters[i].Name, pInRegFilters[i].Name, (strlenW(pInRegFilters[i].Name)+1)*sizeof(WCHAR));
    }

    pEnumRegFilters->lpVtbl = &IEnumRegFiltersImpl_Vtbl;
    pEnumRegFilters->refCount = 1;
    pEnumRegFilters->uIndex = 0;
    pEnumRegFilters->RegFilters = pRegFilters;
    pEnumRegFilters->size = size;

    *ppEnum = (IEnumRegFilters *)(&pEnumRegFilters->lpVtbl);

    return S_OK;
}

static HRESULT WINAPI IEnumRegFiltersImpl_QueryInterface(IEnumRegFilters * iface, REFIID riid, LPVOID * ppv)
{
    TRACE("(%p)->(%s, %p)\n", iface, qzdebugstr_guid(riid), ppv);

    *ppv = NULL;

    if (IsEqualIID(riid, &IID_IUnknown))
        *ppv = (LPVOID)iface;
    else if (IsEqualIID(riid, &IID_IEnumRegFilters))
        *ppv = (LPVOID)iface;

    if (*ppv)
    {
        IUnknown_AddRef((IUnknown *)(*ppv));
        return S_OK;
    }

    FIXME("No interface for %s!\n", qzdebugstr_guid(riid));

    return E_NOINTERFACE;
}

static ULONG WINAPI IEnumRegFiltersImpl_AddRef(IEnumRegFilters * iface)
{
    ICOM_THIS(IEnumRegFiltersImpl, iface);

    TRACE("(%p)\n", iface);

    return ++This->refCount;
}

static ULONG WINAPI IEnumRegFiltersImpl_Release(IEnumRegFilters * iface)
{
    ICOM_THIS(IEnumRegFiltersImpl, iface);

    TRACE("(%p)\n", iface);

    if (!--This->refCount)
    {
        CoTaskMemFree(This);
        return 0;
    } else
        return This->refCount;
}

static HRESULT WINAPI IEnumRegFiltersImpl_Next(IEnumRegFilters * iface, ULONG cFilters, REGFILTER ** ppRegFilter, ULONG * pcFetched)
{
    ULONG cFetched; 
    ICOM_THIS(IEnumRegFiltersImpl, iface);
    int i;

    cFetched = min(This->size, This->uIndex + cFilters) - This->uIndex;

    TRACE("(%p)->(%lu, %p, %p)\n", iface, cFilters, ppRegFilter, pcFetched);

    if (cFetched > 0)
    {
        for(i = 0; i < cFetched; i++)
        {
            /* The string in the REGFILTER structure must be allocated in the same block as the REGFILTER structure itself */
            ppRegFilter[i] = (REGFILTER*)CoTaskMemAlloc(sizeof(REGFILTER)+(strlenW(This->RegFilters[i].Name)+1)*sizeof(WCHAR));
            if (!ppRegFilter[i])
            {
                while(i)
                {
                    CoTaskMemFree(ppRegFilter[--i]);
                    ppRegFilter[i] = NULL;
                }
                return E_OUTOFMEMORY;
        }
            ppRegFilter[i]->Clsid = This->RegFilters[i].Clsid;
            ppRegFilter[i]->Name = (WCHAR*)((char*)ppRegFilter[i]+sizeof(REGFILTER));
            CopyMemory(ppRegFilter[i]->Name, This->RegFilters[i].Name, (strlenW(This->RegFilters[i].Name)+1)*sizeof(WCHAR));
        }

        This->uIndex += cFetched;
        if (pcFetched)
            *pcFetched = cFetched;
        return S_OK;
    }

    return S_FALSE;
}

static HRESULT WINAPI IEnumRegFiltersImpl_Skip(IEnumRegFilters * iface, ULONG n)
{
    TRACE("(%p)->(%lu)\n", iface, n);

    return E_NOTIMPL;
}

static HRESULT WINAPI IEnumRegFiltersImpl_Reset(IEnumRegFilters * iface)
{
    ICOM_THIS(IEnumRegFiltersImpl, iface);

    TRACE("(%p)\n", iface);

    This->uIndex = 0;
    return S_OK;
}

static HRESULT WINAPI IEnumRegFiltersImpl_Clone(IEnumRegFilters * iface, IEnumRegFilters ** ppEnum)
{
    TRACE("(%p)->(%p)\n", iface, ppEnum);

    return E_NOTIMPL;
}

static const IEnumRegFiltersVtbl IEnumRegFiltersImpl_Vtbl =
{
    ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
    IEnumRegFiltersImpl_QueryInterface,
    IEnumRegFiltersImpl_AddRef,
    IEnumRegFiltersImpl_Release,
    IEnumRegFiltersImpl_Next,
    IEnumRegFiltersImpl_Skip,
    IEnumRegFiltersImpl_Reset,
    IEnumRegFiltersImpl_Clone
};
