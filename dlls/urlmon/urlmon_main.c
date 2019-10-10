/*
 * UrlMon
 *
 * Copyright (c) 2000 Patrik Stridvall
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

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "wtypes.h"

#include "wine/debug.h"

#include "urlmon_main.h"

WINE_DEFAULT_DEBUG_CHANNEL(urlmon);

HINSTANCE URLMON_hInstance = 0;

/***********************************************************************
 *		DllMain (URLMON.init)
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID fImpLoad)
{
    TRACE("%p 0x%lx %p\n", hinstDLL, fdwReason, fImpLoad);

    switch(fdwReason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hinstDLL);
        URLMON_hInstance = hinstDLL;
	break;

    case DLL_PROCESS_DETACH:
        URLMON_hInstance = 0;
	break;
    }
    return TRUE;
}


/***********************************************************************
 *		DllInstall (URLMON.@)
 */
HRESULT WINAPI URLMON_DllInstall(BOOL bInstall, LPCWSTR cmdline)
{
  FIXME("(%s, %s): stub\n", bInstall?"TRUE":"FALSE",
	debugstr_w(cmdline));

  return S_OK;
}

/***********************************************************************
 *		DllCanUnloadNow (URLMON.@)
 */
HRESULT WINAPI URLMON_DllCanUnloadNow(void)
{
    FIXME("(void): stub\n");

    return S_FALSE;
}

/***********************************************************************
 *		DllGetClassObject (URLMON.@)
 */
HRESULT WINAPI URLMON_DllGetClassObject(REFCLSID rclsid, REFIID riid,
					LPVOID *ppv)
{
    FIXME("(%p, %p, %p): stub\n", debugstr_guid(rclsid),
	  debugstr_guid(riid), ppv);

    return CLASS_E_CLASSNOTAVAILABLE;
}

/***********************************************************************
 *		DllRegisterServer (URLMON.@)
 */
HRESULT WINAPI URLMON_DllRegisterServer(void)
{
    FIXME("(void): stub\n");

    return S_OK;
}

/***********************************************************************
 *		DllRegisterServerEx (URLMON.@)
 */
HRESULT WINAPI URLMON_DllRegisterServerEx(void)
{
    FIXME("(void): stub\n");

    return E_FAIL;
}

/***********************************************************************
 *		DllUnregisterServer (URLMON.@)
 */
HRESULT WINAPI URLMON_DllUnregisterServer(void)
{
    FIXME("(void): stub\n");

    return S_OK;
}

/**************************************************************************
 *                 UrlMkSetSessionOption (URLMON.@)
 */
 HRESULT WINAPI UrlMkSetSessionOption(long lost, LPVOID *splat, long time,
 					long nosee)
{
    FIXME("(%#lx, %p, %#lx, %#lx): stub\n", lost, splat, time, nosee);

    return S_OK;
}

/**************************************************************************
 *                 ObtainUserAgentString (URLMON.@)
 */
HRESULT WINAPI ObtainUserAgentString(DWORD dwOption, LPCSTR pcszUAOut, DWORD *cbSize)
{
    FIXME("(%ld, %p, %p): stub\n", dwOption, pcszUAOut, cbSize);

    if(dwOption) {
      ERR("dwOption: %ld, must be zero\n", dwOption);
    }

    return S_OK;
}
