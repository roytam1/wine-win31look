/*
 * Copyright 2002 Michael G�nnewig
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
#include "wingdi.h"
#include "winuser.h"
#include "winnls.h"
#include "winerror.h"

#include "ole2.h"
#include "vfw.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(avifile);

#include "initguid.h"
#include "avifile_private.h"

HMODULE AVIFILE_hModule   = NULL;

BOOL    AVIFILE_bLocked   = FALSE;
UINT    AVIFILE_uUseCount = 0;

static HRESULT WINAPI IClassFactory_fnQueryInterface(LPCLASSFACTORY iface,REFIID riid,LPVOID *ppobj);
static ULONG   WINAPI IClassFactory_fnAddRef(LPCLASSFACTORY iface);
static ULONG   WINAPI IClassFactory_fnRelease(LPCLASSFACTORY iface);
static HRESULT WINAPI IClassFactory_fnCreateInstance(LPCLASSFACTORY iface,LPUNKNOWN pOuter,REFIID riid,LPVOID *ppobj);
static HRESULT WINAPI IClassFactory_fnLockServer(LPCLASSFACTORY iface,BOOL dolock);

static ICOM_VTABLE(IClassFactory) iclassfact = {
  ICOM_MSVTABLE_COMPAT_DummyRTTIVALUE
  IClassFactory_fnQueryInterface,
  IClassFactory_fnAddRef,
  IClassFactory_fnRelease,
  IClassFactory_fnCreateInstance,
  IClassFactory_fnLockServer
};

typedef struct
{
  /* IUnknown fields */
  ICOM_VFIELD(IClassFactory);
  DWORD	 dwRef;

  CLSID  clsid;
} IClassFactoryImpl;

static HRESULT AVIFILE_CreateClassFactory(const CLSID *pclsid, const IID *riid,
					  LPVOID *ppv)
{
  IClassFactoryImpl *pClassFactory = NULL;
  HRESULT            hr;

  *ppv = NULL;

  pClassFactory = (IClassFactoryImpl*)LocalAlloc(LPTR, sizeof(*pClassFactory));
  if (pClassFactory == NULL)
    return E_OUTOFMEMORY;

  pClassFactory->lpVtbl    = &iclassfact;
  pClassFactory->dwRef     = 0;
  memcpy(&pClassFactory->clsid, pclsid, sizeof(pClassFactory->clsid));

  hr = IUnknown_QueryInterface((IUnknown*)pClassFactory, riid, ppv);
  if (FAILED(hr)) {
    LocalFree((HLOCAL)pClassFactory);
    *ppv = NULL;
  }

  return hr;
}

static HRESULT WINAPI IClassFactory_fnQueryInterface(LPCLASSFACTORY iface,
						     REFIID riid,LPVOID *ppobj)
{
  TRACE("(%p,%p,%p)\n", iface, riid, ppobj);

  if ((IsEqualGUID(&IID_IUnknown, riid)) ||
      (IsEqualGUID(&IID_IClassFactory, riid))) {
    *ppobj = iface;
    IClassFactory_AddRef(iface);
    return S_OK;
  }

  return E_NOINTERFACE;
}

static ULONG WINAPI IClassFactory_fnAddRef(LPCLASSFACTORY iface)
{
  ICOM_THIS(IClassFactoryImpl,iface);

  TRACE("(%p)\n", iface);

  return ++(This->dwRef);
}

static ULONG WINAPI IClassFactory_fnRelease(LPCLASSFACTORY iface)
{
  ICOM_THIS(IClassFactoryImpl,iface);

  TRACE("(%p)\n", iface);
  if ((--(This->dwRef)) > 0)
    return This->dwRef;

  return 0;
}

static HRESULT WINAPI IClassFactory_fnCreateInstance(LPCLASSFACTORY iface,
						     LPUNKNOWN pOuter,
						     REFIID riid,LPVOID *ppobj)
{
  ICOM_THIS(IClassFactoryImpl,iface);

  TRACE("(%p,%p,%s,%p)\n", iface, pOuter, debugstr_guid(riid),
	ppobj);

  if (ppobj == NULL || pOuter != NULL)
    return E_FAIL;
  *ppobj = NULL;

  if (IsEqualGUID(&CLSID_AVIFile, &This->clsid))
    return AVIFILE_CreateAVIFile(riid,ppobj);
  if (IsEqualGUID(&CLSID_ICMStream, &This->clsid))
    return AVIFILE_CreateICMStream(riid,ppobj);
  if (IsEqualGUID(&CLSID_WAVFile, &This->clsid))
    return AVIFILE_CreateWAVFile(riid,ppobj);
  if (IsEqualGUID(&CLSID_ACMStream, &This->clsid))
    return AVIFILE_CreateACMStream(riid,ppobj);

  return E_NOINTERFACE;
}

static HRESULT WINAPI IClassFactory_fnLockServer(LPCLASSFACTORY iface,BOOL dolock)
{
  TRACE("(%p,%d)\n",iface,dolock);

  AVIFILE_bLocked = dolock;

  return S_OK;
}

LPCWSTR AVIFILE_BasenameW(LPCWSTR szPath)
{
#define SLASH(w) ((w) == '/' || (w) == '\\')

  LPCWSTR szCur;

  for (szCur = szPath + lstrlenW(szPath);
       szCur > szPath && !SLASH(*szCur) && *szCur != ':';)
    szCur--;

  if (szCur == szPath)
    return szCur;
  else
    return szCur + 1;

#undef SLASH
}

/***********************************************************************
 *		DllGetClassObject (AVIFIL32.@)
 */
HRESULT WINAPI AVIFILE_DllGetClassObject(const CLSID* pclsid,REFIID piid,LPVOID *ppv)
{
  TRACE("(%s,%s,%p)\n", debugstr_guid(pclsid), debugstr_guid(piid), ppv);

  if (pclsid == NULL || piid == NULL || ppv == NULL)
    return E_FAIL;

  return AVIFILE_CreateClassFactory(pclsid,piid,ppv);
}

/*****************************************************************************
 *		DllCanUnloadNow		(AVIFIL32.@)
 */
DWORD WINAPI AVIFILE_DllCanUnloadNow(void)
{
  return ((AVIFILE_bLocked || AVIFILE_uUseCount) ? S_FALSE : S_OK);
}

/*****************************************************************************
 *		DllMain		[AVIFIL32.init]
 */
BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD fdwReason, LPVOID lpvReserved)
{
  TRACE("(%p,%lu,%p)\n", hInstDll, fdwReason, lpvReserved);

  switch (fdwReason) {
  case DLL_PROCESS_ATTACH:
    DisableThreadLibraryCalls(hInstDll);
    AVIFILE_hModule = (HMODULE)hInstDll;
    break;
  case DLL_PROCESS_DETACH:
    break;
  };

  return TRUE;
}
