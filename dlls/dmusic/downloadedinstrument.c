/* IDirectMusicDownloadedInstrument Implementation
 *
 * Copyright (C) 2003-2004 Rok Mandeljc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dmusic_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmusic);

/* IDirectMusicDownloadedInstrumentImpl IUnknown part: */
HRESULT WINAPI IDirectMusicDownloadedInstrumentImpl_QueryInterface (LPDIRECTMUSICDOWNLOADEDINSTRUMENT iface, REFIID riid, LPVOID *ppobj) {
	ICOM_THIS(IDirectMusicDownloadedInstrumentImpl,iface);
	TRACE("(%p, %s, %p)\n", This, debugstr_dmguid(riid), ppobj);

	if (IsEqualIID (riid, &IID_IUnknown)
		|| IsEqualIID (riid, &IID_IDirectMusicDownloadedInstrument)) {
		IDirectMusicDownloadedInstrumentImpl_AddRef(iface);
		*ppobj = This;
		return S_OK;
	}
	WARN("(%p, %s, %p): not found\n", This, debugstr_dmguid(riid), ppobj);
	return E_NOINTERFACE;
}

ULONG WINAPI IDirectMusicDownloadedInstrumentImpl_AddRef (LPDIRECTMUSICDOWNLOADEDINSTRUMENT iface) {
	ICOM_THIS(IDirectMusicDownloadedInstrumentImpl,iface);
	TRACE("(%p): AddRef from %ld\n", This, This->ref);
	return ++(This->ref);
}

ULONG WINAPI IDirectMusicDownloadedInstrumentImpl_Release (LPDIRECTMUSICDOWNLOADEDINSTRUMENT iface) {
	ICOM_THIS(IDirectMusicDownloadedInstrumentImpl,iface);
	ULONG ref = --This->ref;
	TRACE("(%p): ReleaseRef to %ld\n", This, This->ref);
	if (ref == 0) {
		HeapFree(GetProcessHeap(), 0, This);
	}
	return ref;
}

/* IDirectMusicDownloadedInstrumentImpl IDirectMusicDownloadedInstrument part: */
/* none at this time */

ICOM_VTABLE(IDirectMusicDownloadedInstrument) DirectMusicDownloadedInstrument_Vtbl = {
    ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	IDirectMusicDownloadedInstrumentImpl_QueryInterface,
	IDirectMusicDownloadedInstrumentImpl_AddRef,
	IDirectMusicDownloadedInstrumentImpl_Release
};

/* for ClassFactory */
HRESULT WINAPI DMUSIC_CreateDirectMusicDownloadedInstrumentImpl (LPCGUID lpcGUID, LPVOID* ppobj, LPUNKNOWN pUnkOuter) {
	IDirectMusicDownloadedInstrumentImpl* dmdlinst;
	
	dmdlinst = HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectMusicDownloadedInstrumentImpl));
	if (NULL == dmdlinst) {
		*ppobj = (LPVOID) NULL;
		return E_OUTOFMEMORY;
	}
	dmdlinst->lpVtbl = &DirectMusicDownloadedInstrument_Vtbl;
	dmdlinst->ref = 0; /* will be inited by QueryInterface */
	
	return IDirectMusicDownloadedInstrumentImpl_QueryInterface ((LPDIRECTMUSICDOWNLOADEDINSTRUMENT)dmdlinst, lpcGUID, ppobj);	
}
