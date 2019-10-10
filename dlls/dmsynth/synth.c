/* IDirectMusicSynth8 Implementation
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

#include "dmsynth_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmsynth);

/* IDirectMusicSynth8Impl IUnknown part: */
HRESULT WINAPI IDirectMusicSynth8Impl_QueryInterface (LPDIRECTMUSICSYNTH8 iface, REFIID riid, LPVOID *ppobj) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	TRACE("(%p, %s, %p)\n", This, debugstr_dmguid(riid), ppobj);

	if (IsEqualIID (riid, &IID_IUnknown) || 
	    IsEqualIID (riid, &IID_IDirectMusicSynth) ||
	    IsEqualIID (riid, &IID_IDirectMusicSynth8)) {
		IDirectMusicSynth8Impl_AddRef(iface);
		*ppobj = This;
		return S_OK;
	}
	WARN("(%p, %s, %p): not found\n", This, debugstr_dmguid(riid), ppobj);
	return E_NOINTERFACE;
}

ULONG WINAPI IDirectMusicSynth8Impl_AddRef (LPDIRECTMUSICSYNTH8 iface) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	TRACE("(%p): AddRef from %ld\n", This, This->ref);
	return ++(This->ref);
}

ULONG WINAPI IDirectMusicSynth8Impl_Release (LPDIRECTMUSICSYNTH8 iface) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	ULONG ref = --This->ref;
	TRACE("(%p): ReleaseRef to %ld\n", This, This->ref);
	if (ref == 0) {
		HeapFree(GetProcessHeap(), 0, This);
	}
	return ref;
}

/* IDirectMusicSynth8Impl IDirectMusicSynth part: */
HRESULT WINAPI IDirectMusicSynth8Impl_Open (LPDIRECTMUSICSYNTH8 iface, LPDMUS_PORTPARAMS pPortParams) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %p): stub\n", This, pPortParams);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_Close (LPDIRECTMUSICSYNTH8 iface) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p): stub\n", This);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_SetNumChannelGroups (LPDIRECTMUSICSYNTH8 iface, DWORD dwGroups) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %ld): stub\n", This, dwGroups);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_Download (LPDIRECTMUSICSYNTH8 iface, LPHANDLE phDownload, LPVOID pvData, LPBOOL pbFree) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %p, %p, %p): stub\n", This, phDownload, pvData, pbFree);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_Unload (LPDIRECTMUSICSYNTH8 iface, HANDLE hDownload, HRESULT (CALLBACK* lpFreeHandle)(HANDLE,HANDLE), HANDLE hUserData) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %p, %p): stub\n", This, hDownload, hUserData);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_PlayBuffer (LPDIRECTMUSICSYNTH8 iface, REFERENCE_TIME rt, LPBYTE pbBuffer, DWORD cbBuffer) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %lli, %p, %ld): stub\n", This, rt, pbBuffer, cbBuffer);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_GetRunningStats (LPDIRECTMUSICSYNTH8 iface, LPDMUS_SYNTHSTATS pStats) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %p): stub\n", This, pStats);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_GetPortCaps (LPDIRECTMUSICSYNTH8 iface, LPDMUS_PORTCAPS pCaps) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	TRACE("(%p, %p)\n", This, pCaps);
	*pCaps = This->pCaps;
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_SetMasterClock (LPDIRECTMUSICSYNTH8 iface, IReferenceClock* pClock) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %p): stub\n", This, pClock);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_GetLatencyClock (LPDIRECTMUSICSYNTH8 iface, IReferenceClock** ppClock) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	TRACE("(%p, %p)\n", This, ppClock);
	*ppClock = This->pLatencyClock;
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_Activate (LPDIRECTMUSICSYNTH8 iface, BOOL fEnable) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	TRACE("(%p, %d)\n", This, fEnable);
	This->fActive = fEnable;
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_SetSynthSink (LPDIRECTMUSICSYNTH8 iface, IDirectMusicSynthSink* pSynthSink) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	TRACE("(%p, %p)\n", This, pSynthSink);
	This->pSynthSink = (IDirectMusicSynthSinkImpl*)pSynthSink;
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_Render (LPDIRECTMUSICSYNTH8 iface, short* pBuffer, DWORD dwLength, LONGLONG llPosition) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %p, %ld, %lli): stub\n", This, pBuffer, dwLength, llPosition);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_SetChannelPriority (LPDIRECTMUSICSYNTH8 iface, DWORD dwChannelGroup, DWORD dwChannel, DWORD dwPriority) {
	/*ICOM_THIS(IDirectMusicSynth8Impl,iface); */
	/* silenced because of too many messages - 1000 groups * 16 channels ;=) */
	/*FIXME("(%p, %ld, %ld, %ld): stub\n", This, dwChannelGroup, dwChannel, dwPriority); */
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_GetChannelPriority (LPDIRECTMUSICSYNTH8 iface, DWORD dwChannelGroup, DWORD dwChannel, LPDWORD pdwPriority) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %ld, %ld, %p): stub\n", This, dwChannelGroup, dwChannel, pdwPriority);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_GetFormat (LPDIRECTMUSICSYNTH8 iface, LPWAVEFORMATEX pWaveFormatEx, LPDWORD pdwWaveFormatExSiz) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %p, %p): stub\n", This, pWaveFormatEx, pdwWaveFormatExSiz);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_GetAppend (LPDIRECTMUSICSYNTH8 iface, DWORD* pdwAppend) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %p): stub\n", This, pdwAppend);
	return S_OK;
}

/* IDirectMusicSynth8Impl IDirectMusicSynth8 part: */
HRESULT WINAPI IDirectMusicSynth8Impl_PlayVoice (LPDIRECTMUSICSYNTH8 iface, REFERENCE_TIME rt, DWORD dwVoiceId, DWORD dwChannelGroup, DWORD dwChannel, DWORD dwDLId, long prPitch, long vrVolume, SAMPLE_TIME stVoiceStart, SAMPLE_TIME stLoopStart, SAMPLE_TIME stLoopEnd) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %lli, %ld, %ld, %ld, %ld, %li, %li,%lli, %lli, %lli): stub\n", This, rt, dwVoiceId, dwChannelGroup, dwChannel, dwDLId, prPitch, vrVolume, stVoiceStart, stLoopStart, stLoopEnd);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_StopVoice (LPDIRECTMUSICSYNTH8 iface, REFERENCE_TIME rt, DWORD dwVoiceId) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %lli, %ld): stub\n", This, rt, dwVoiceId);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_GetVoiceState (LPDIRECTMUSICSYNTH8 iface, DWORD dwVoice[], DWORD cbVoice, DMUS_VOICE_STATE dwVoiceState[]) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %p, %ld, %p): stub\n", This, dwVoice, cbVoice, dwVoiceState);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_Refresh (LPDIRECTMUSICSYNTH8 iface, DWORD dwDownloadID, DWORD dwFlags) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %ld, %ld): stub\n", This, dwDownloadID, dwFlags);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynth8Impl_AssignChannelToBuses (LPDIRECTMUSICSYNTH8 iface, DWORD dwChannelGroup, DWORD dwChannel, LPDWORD pdwBuses, DWORD cBuses) {
	ICOM_THIS(IDirectMusicSynth8Impl,iface);
	FIXME("(%p, %ld, %ld, %p, %ld): stub\n", This, dwChannelGroup, dwChannel, pdwBuses, cBuses);
	return S_OK;
}

ICOM_VTABLE(IDirectMusicSynth8) DirectMusicSynth8_Vtbl = {
    ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
	IDirectMusicSynth8Impl_QueryInterface,
	IDirectMusicSynth8Impl_AddRef,
	IDirectMusicSynth8Impl_Release,
	IDirectMusicSynth8Impl_Open,
	IDirectMusicSynth8Impl_Close,
	IDirectMusicSynth8Impl_SetNumChannelGroups,
	IDirectMusicSynth8Impl_Download,
	IDirectMusicSynth8Impl_Unload,
	IDirectMusicSynth8Impl_PlayBuffer,
	IDirectMusicSynth8Impl_GetRunningStats,
	IDirectMusicSynth8Impl_GetPortCaps,
	IDirectMusicSynth8Impl_SetMasterClock,
	IDirectMusicSynth8Impl_GetLatencyClock,
	IDirectMusicSynth8Impl_Activate,
	IDirectMusicSynth8Impl_SetSynthSink,
	IDirectMusicSynth8Impl_Render,
	IDirectMusicSynth8Impl_SetChannelPriority,
	IDirectMusicSynth8Impl_GetChannelPriority,
	IDirectMusicSynth8Impl_GetFormat,
	IDirectMusicSynth8Impl_GetAppend,
	IDirectMusicSynth8Impl_PlayVoice,
	IDirectMusicSynth8Impl_StopVoice,
	IDirectMusicSynth8Impl_GetVoiceState,
	IDirectMusicSynth8Impl_Refresh,
	IDirectMusicSynth8Impl_AssignChannelToBuses
};

/* for ClassFactory */
HRESULT WINAPI DMUSIC_CreateDirectMusicSynthImpl (LPCGUID lpcGUID, LPVOID* ppobj, LPUNKNOWN pUnkOuter) {
	IDirectMusicSynth8Impl *obj;
	
	TRACE("(%p,%p,%p)\n", lpcGUID, ppobj, pUnkOuter);
	obj = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectMusicSynth8Impl));
	if (NULL == obj) {
		*ppobj = (LPDIRECTMUSICSYNTH8) NULL;
		return E_OUTOFMEMORY;
	}
	obj->lpVtbl = &DirectMusicSynth8_Vtbl;
	obj->ref = 0;
	/* fill in caps */
	obj->pCaps.dwSize = sizeof(DMUS_PORTCAPS);
	obj->pCaps.dwFlags = DMUS_PC_DLS | DMUS_PC_SOFTWARESYNTH | DMUS_PC_DIRECTSOUND | DMUS_PC_DLS2 | DMUS_PC_AUDIOPATH | DMUS_PC_WAVE;
	obj->pCaps.guidPort = CLSID_DirectMusicSynth;
	obj->pCaps.dwClass = DMUS_PC_OUTPUTCLASS;
	obj->pCaps.dwType = DMUS_PORT_WINMM_DRIVER;
	obj->pCaps.dwMemorySize = DMUS_PC_SYSTEMMEMORY;
	obj->pCaps.dwMaxChannelGroups = 1000;
	obj->pCaps.dwMaxVoices = 1000;
	obj->pCaps.dwMaxAudioChannels = -1;
	obj->pCaps.dwEffectFlags = DMUS_EFFECT_REVERB | DMUS_EFFECT_CHORUS | DMUS_EFFECT_DELAY;
	MultiByteToWideChar (CP_ACP, 0, "Microsotf Synthesizer", -1, obj->pCaps.wszDescription, sizeof(obj->pCaps.wszDescription)/sizeof(WCHAR));
	/* assign latency clock */
	/*DMUSIC_CreateReferenceClockImpl (&IID_IReferenceClock, (LPVOID*)&This->pLatencyClock, NULL); */

	return IDirectMusicSynth8Impl_QueryInterface ((LPDIRECTMUSICSYNTH8)obj, lpcGUID, ppobj);
}
