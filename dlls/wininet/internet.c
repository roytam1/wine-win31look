/*
 * Wininet
 *
 * Copyright 1999 Corel Corporation
 * Copyright 2002 CodeWeavers Inc.
 * Copyright 2002 Jaco Greeff
 * Copyright 2002 TransGaming Technologies Inc.
 * Copyright 2004 Mike McCormack for CodeWeavers
 *
 * Ulrich Czekalla
 * Aric Stewart
 * David Hammerton
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

#include "config.h"
#include "wine/port.h"

#define MAXHOSTNAME 100 /* from http.c */

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <stdlib.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "ntstatus.h"
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "wininet.h"
#include "winnls.h"
#include "wine/debug.h"
#include "winerror.h"
#define NO_SHLWAPI_STREAM
#include "shlwapi.h"

#include "wine/exception.h"
#include "excpt.h"

#include "internet.h"

#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(wininet);

#define MAX_IDLE_WORKER 1000*60*1
#define MAX_WORKER_THREADS 10
#define RESPONSE_TIMEOUT        30

#define GET_HWININET_FROM_LPWININETFINDNEXT(lpwh) \
(LPWININETAPPINFOW)(((LPWININETFTPSESSIONA)(lpwh->hdr.lpwhparent))->hdr.lpwhparent)


typedef struct
{
    DWORD  dwError;
    CHAR   response[MAX_REPLY_LEN];
} WITHREADERROR, *LPWITHREADERROR;

BOOL WINAPI INTERNET_FindNextFileA(HINTERNET hFind, LPVOID lpvFindData);
HINTERNET WINAPI INTERNET_InternetOpenUrlW(HINTERNET hInternet, LPCWSTR lpszUrl,
					   LPCWSTR lpszHeaders, DWORD dwHeadersLength, DWORD dwFlags, DWORD dwContext);
VOID INTERNET_ExecuteWork();

DWORD g_dwTlsErrIndex = TLS_OUT_OF_INDEXES;
DWORD dwNumThreads;
DWORD dwNumIdleThreads;
DWORD dwNumJobs;
HANDLE hEventArray[2];
#define hQuitEvent hEventArray[0]
#define hWorkEvent hEventArray[1]
CRITICAL_SECTION csQueue;
LPWORKREQUEST lpHeadWorkQueue;
LPWORKREQUEST lpWorkQueueTail;

extern void URLCacheContainers_CreateDefaults();
extern void URLCacheContainers_DeleteAll();

#define HANDLE_CHUNK_SIZE 0x10

static CRITICAL_SECTION WININET_cs;
static CRITICAL_SECTION_DEBUG WININET_cs_debug = 
{
    0, 0, &WININET_cs,
    { &WININET_cs_debug.ProcessLocksList, &WININET_cs_debug.ProcessLocksList },
      0, 0, { 0, (DWORD)(__FILE__ ": WININET_cs") }
};
static CRITICAL_SECTION WININET_cs = { &WININET_cs_debug, -1, 0, 0, 0, 0 };

static LPWININETHANDLEHEADER *WININET_Handles;
static UINT WININET_dwNextHandle;
static UINT WININET_dwMaxHandles;

HINTERNET WININET_AllocHandle( LPWININETHANDLEHEADER info )
{
    LPWININETHANDLEHEADER *p;
    UINT handle = 0, num;

    EnterCriticalSection( &WININET_cs );
    if( !WININET_dwMaxHandles )
    {
        num = HANDLE_CHUNK_SIZE;
        p = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 
                   sizeof (UINT)* num);
        if( !p )
            goto end;
        WININET_Handles = p;
        WININET_dwMaxHandles = num;
    }
    if( WININET_dwMaxHandles == WININET_dwNextHandle )
    {
        num = WININET_dwMaxHandles + HANDLE_CHUNK_SIZE;
        p = HeapReAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
                   WININET_Handles, sizeof (UINT)* num);
        if( !p )
            goto end;
        WININET_Handles = p;
        WININET_dwMaxHandles = num;
    }

    handle = WININET_dwNextHandle;
    if( WININET_Handles[handle] )
        ERR("handle isn't free but should be\n");
    WININET_Handles[handle] = info;

    while( WININET_Handles[WININET_dwNextHandle] && 
           (WININET_dwNextHandle < WININET_dwMaxHandles ) )
        WININET_dwNextHandle++;
    
end:
    LeaveCriticalSection( &WININET_cs );

    return (HINTERNET) (handle+1);
}

HINTERNET WININET_FindHandle( LPWININETHANDLEHEADER info )
{
    UINT i, handle = 0;

    EnterCriticalSection( &WININET_cs );
    for( i=0; i<WININET_dwMaxHandles; i++ )
    {
        if( info == WININET_Handles[i] )
        {
            handle = i+1;
            break;
        }
    }
    LeaveCriticalSection( &WININET_cs );

    return (HINTERNET) handle;
}

LPWININETHANDLEHEADER WININET_GetObject( HINTERNET hinternet )
{
    LPWININETHANDLEHEADER info = NULL;
    UINT handle = (UINT) hinternet;

    EnterCriticalSection( &WININET_cs );

    if( (handle > 0) && ( handle <= WININET_dwNextHandle ) )
        info = WININET_Handles[handle-1];

    LeaveCriticalSection( &WININET_cs );

    TRACE("handle %d -> %p\n", handle, info);

    return info;
}

BOOL WININET_FreeHandle( HINTERNET hinternet )
{
    BOOL ret = FALSE;
    UINT handle = (UINT) hinternet;

    EnterCriticalSection( &WININET_cs );

    if( (handle > 1) && ( handle < WININET_dwNextHandle ) )
    {
        handle--;
        if( WININET_Handles[handle] )
        {
            WININET_Handles[handle] = NULL;
            ret = TRUE;
            if( WININET_dwNextHandle > handle )
                WININET_dwNextHandle = handle;
        }
    }

    LeaveCriticalSection( &WININET_cs );

    return ret;
}

/***********************************************************************
 * DllMain [Internal] Initializes the internal 'WININET.DLL'.
 *
 * PARAMS
 *     hinstDLL    [I] handle to the DLL's instance
 *     fdwReason   [I]
 *     lpvReserved [I] reserved, must be NULL
 *
 * RETURNS
 *     Success: TRUE
 *     Failure: FALSE
 */

BOOL WINAPI DllMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("%p,%lx,%p\n", hinstDLL, fdwReason, lpvReserved);

    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:

            g_dwTlsErrIndex = TlsAlloc();

	    if (g_dwTlsErrIndex == TLS_OUT_OF_INDEXES)
		return FALSE;

	    hQuitEvent = CreateEventA(0, TRUE, FALSE, NULL);
	    hWorkEvent = CreateEventA(0, FALSE, FALSE, NULL);
	    InitializeCriticalSection(&csQueue);

            URLCacheContainers_CreateDefaults();

            dwNumThreads = 0;
            dwNumIdleThreads = 0;
	    dwNumJobs = 0;

        case DLL_THREAD_ATTACH:
	    {
                LPWITHREADERROR lpwite = HeapAlloc(GetProcessHeap(), 0, sizeof(WITHREADERROR));
		if (NULL == lpwite)
                    return FALSE;

                TlsSetValue(g_dwTlsErrIndex, (LPVOID)lpwite);
	    }
	    break;

        case DLL_THREAD_DETACH:
	    if (g_dwTlsErrIndex != TLS_OUT_OF_INDEXES)
			{
				LPVOID lpwite = TlsGetValue(g_dwTlsErrIndex);
				if (lpwite)
                   HeapFree(GetProcessHeap(), 0, lpwite);
			}
	    break;

        case DLL_PROCESS_DETACH:

	    URLCacheContainers_DeleteAll();

	    if (g_dwTlsErrIndex != TLS_OUT_OF_INDEXES)
	    {
	        HeapFree(GetProcessHeap(), 0, TlsGetValue(g_dwTlsErrIndex));
	        TlsFree(g_dwTlsErrIndex);
	    }

	    SetEvent(hQuitEvent);

	    CloseHandle(hQuitEvent);
	    CloseHandle(hWorkEvent);
	    DeleteCriticalSection(&csQueue);
            break;
    }

    return TRUE;
}


/***********************************************************************
 *           InternetInitializeAutoProxyDll   (WININET.@)
 *
 * Setup the internal proxy
 *
 * PARAMETERS
 *     dwReserved
 *
 * RETURNS
 *     FALSE on failure
 *
 */
BOOL WINAPI InternetInitializeAutoProxyDll(DWORD dwReserved)
{
    FIXME("STUB\n");
    INTERNET_SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

/***********************************************************************
 *           DetectAutoProxyUrl   (WININET.@)
 *
 * Auto detect the proxy url
 *
 * RETURNS
 *     FALSE on failure
 *
 */
BOOL WINAPI DetectAutoProxyUrl(LPSTR lpszAutoProxyUrl,
	DWORD dwAutoProxyUrlLength, DWORD dwDetectFlags)
{
    FIXME("STUB\n");
    INTERNET_SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}


/***********************************************************************
 *           INTERNET_ConfigureProxyFromReg
 *
 * FIXME:
 * The proxy may be specified in the form 'http=proxy.my.org'
 * Presumably that means there can be ftp=ftpproxy.my.org too.
 */
static BOOL INTERNET_ConfigureProxyFromReg( LPWININETAPPINFOW lpwai )
{
    HKEY key;
    DWORD r, keytype, len, enabled;
    LPSTR lpszInternetSettings =
        "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";
    WCHAR szProxyServer[] = { 'P','r','o','x','y','S','e','r','v','e','r', 0 };

    r = RegOpenKeyA(HKEY_CURRENT_USER, lpszInternetSettings, &key);
    if ( r != ERROR_SUCCESS )
        return FALSE;

    len = sizeof enabled;
    r = RegQueryValueExA( key, "ProxyEnable", NULL, &keytype,
                          (BYTE*)&enabled, &len);
    if( (r == ERROR_SUCCESS) && enabled )
    {
        TRACE("Proxy is enabled.\n");

        /* figure out how much memory the proxy setting takes */
        r = RegQueryValueExW( key, szProxyServer, NULL, &keytype, 
                              NULL, &len);
        if( (r == ERROR_SUCCESS) && len && (keytype == REG_SZ) )
        {
            LPWSTR szProxy, p;
            WCHAR szHttp[] = {'h','t','t','p','=',0};

            szProxy=HeapAlloc( GetProcessHeap(), 0, len );
            RegQueryValueExW( key, szProxyServer, NULL, &keytype,
                              (BYTE*)szProxy, &len);

            /* find the http proxy, and strip away everything else */
            p = strstrW( szProxy, szHttp );
            if( p )
            {
                 p += lstrlenW(szHttp);
                 lstrcpyW( szProxy, p );
            }
            p = strchrW( szProxy, ' ' );
            if( p )
                *p = 0;

            lpwai->dwAccessType = INTERNET_OPEN_TYPE_PROXY;
            lpwai->lpszProxy = szProxy;

            TRACE("http proxy = %s\n", debugstr_w(lpwai->lpszProxy));
        }
        else
            ERR("Couldn't read proxy server settings.\n");
    }
    else
        TRACE("Proxy is not enabled.\n");
    RegCloseKey(key);

    return enabled;
}

/***********************************************************************
 *           InternetOpenA   (WININET.@)
 *
 * Per-application initialization of wininet
 *
 * RETURNS
 *    HINTERNET on success
 *    NULL on failure
 *
 */
HINTERNET WINAPI InternetOpenW(LPCWSTR lpszAgent, DWORD dwAccessType,
    LPCWSTR lpszProxy, LPCWSTR lpszProxyBypass, DWORD dwFlags)
{
    LPWININETAPPINFOW lpwai = NULL;
    HINTERNET handle = NULL;

    if (TRACE_ON(wininet)) {
#define FE(x) { x, #x }
	static const wininet_flag_info access_type[] = {
	    FE(INTERNET_OPEN_TYPE_PRECONFIG),
	    FE(INTERNET_OPEN_TYPE_DIRECT),
	    FE(INTERNET_OPEN_TYPE_PROXY),
	    FE(INTERNET_OPEN_TYPE_PRECONFIG_WITH_NO_AUTOPROXY)
	};
	static const wininet_flag_info flag[] = {
	    FE(INTERNET_FLAG_ASYNC),
	    FE(INTERNET_FLAG_FROM_CACHE),
	    FE(INTERNET_FLAG_OFFLINE)
	};
#undef FE
	int i;
	const char *access_type_str = "Unknown";
	DWORD flag_val = dwFlags;
	
	TRACE("(%s, %li, %s, %s, %li)\n", debugstr_w(lpszAgent), dwAccessType,
	      debugstr_w(lpszProxy), debugstr_w(lpszProxyBypass), dwFlags);
	for (i = 0; i < (sizeof(access_type) / sizeof(access_type[0])); i++) {
	    if (access_type[i].val == dwAccessType) {
		access_type_str = access_type[i].name;
		break;
	    }
	}
	TRACE("  access type : %s\n", access_type_str);
	TRACE("  flags       :");
	for (i = 0; i < (sizeof(flag) / sizeof(flag[0])); i++) {
	    if (flag[i].val & flag_val) {
		DPRINTF(" %s", flag[i].name);
		flag_val &= ~flag[i].val;
	    }
	}	
	if (flag_val) DPRINTF(" Unknown flags (%08lx)", flag_val);
	DPRINTF("\n");
    }

    /* Clear any error information */
    INTERNET_SetLastError(0);

    lpwai = HeapAlloc(GetProcessHeap(), 0, sizeof(WININETAPPINFOW));
    if (NULL == lpwai)
    {
        INTERNET_SetLastError(ERROR_OUTOFMEMORY);
	goto lend;
    }
 
    memset(lpwai, 0, sizeof(WININETAPPINFOW));
    lpwai->hdr.htype = WH_HINIT;
    lpwai->hdr.lpwhparent = NULL;
    lpwai->hdr.dwFlags = dwFlags;
    lpwai->dwAccessType = dwAccessType;
    lpwai->lpszProxyUsername = NULL;
    lpwai->lpszProxyPassword = NULL;

    handle = WININET_AllocHandle( &lpwai->hdr );
    if( !handle )
    {
        HeapFree( GetProcessHeap(), 0, lpwai );
        INTERNET_SetLastError(ERROR_OUTOFMEMORY);
	goto lend;
    }

    if (NULL != lpszAgent)
    {
        lpwai->lpszAgent = HeapAlloc( GetProcessHeap(),0,
                                      (strlenW(lpszAgent)+1)*sizeof(WCHAR));
        if (lpwai->lpszAgent)
            lstrcpyW( lpwai->lpszAgent, lpszAgent );
    }
    if(dwAccessType == INTERNET_OPEN_TYPE_PRECONFIG)
        INTERNET_ConfigureProxyFromReg( lpwai );
    else if (NULL != lpszProxy)
    {
        lpwai->lpszProxy = HeapAlloc( GetProcessHeap(), 0,
                                      (strlenW(lpszProxy)+1)*sizeof(WCHAR));
        if (lpwai->lpszProxy)
            lstrcpyW( lpwai->lpszProxy, lpszProxy );
    }

    if (NULL != lpszProxyBypass)
    {
        lpwai->lpszProxyBypass = HeapAlloc( GetProcessHeap(), 0,
                                     (strlenW(lpszProxyBypass)+1)*sizeof(WCHAR));
        if (lpwai->lpszProxyBypass)
            lstrcpyW( lpwai->lpszProxyBypass, lpszProxyBypass );
    }

 lend:
    TRACE("returning %p\n", (HINTERNET)lpwai);

    return handle;
}


/***********************************************************************
 *           InternetOpenW   (WININET.@)
 *
 * Per-application initialization of wininet
 *
 * RETURNS
 *    HINTERNET on success
 *    NULL on failure
 *
 */
HINTERNET WINAPI InternetOpenA(LPCSTR lpszAgent, DWORD dwAccessType,
    LPCSTR lpszProxy, LPCSTR lpszProxyBypass, DWORD dwFlags)
{
    HINTERNET rc = (HINTERNET)NULL;
    INT len;
    WCHAR *szAgent = NULL, *szProxy = NULL, *szBypass = NULL;

    TRACE("(%s, 0x%08lx, %s, %s, 0x%08lx)\n", debugstr_a(lpszAgent),
       dwAccessType, debugstr_a(lpszProxy), debugstr_a(lpszProxyBypass), dwFlags);

    if( lpszAgent )
    {
        len = MultiByteToWideChar(CP_ACP, 0, lpszAgent, -1, NULL, 0);
        szAgent = HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, lpszAgent, -1, szAgent, len);
    }

    if( lpszProxy )
    {
        len = MultiByteToWideChar(CP_ACP, 0, lpszProxy, -1, NULL, 0);
        szProxy = HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, lpszProxy, -1, szProxy, len);
    }

    if( lpszProxyBypass )
    {
        len = MultiByteToWideChar(CP_ACP, 0, lpszProxyBypass, -1, NULL, 0);
        szBypass = HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, lpszProxyBypass, -1, szBypass, len);
    }

    rc = InternetOpenW(szAgent, dwAccessType, szProxy, szBypass, dwFlags);

    if( szAgent )
        HeapFree(GetProcessHeap(), 0, szAgent);
    if( szProxy )
        HeapFree(GetProcessHeap(), 0, szProxy);
    if( szBypass )
        HeapFree(GetProcessHeap(), 0, szBypass);

    return rc;
}

/***********************************************************************
 *           InternetGetLastResponseInfoA (WININET.@)
 *
 * Return last wininet error description on the calling thread
 *
 * RETURNS
 *    TRUE on success of writing to buffer
 *    FALSE on failure
 *
 */
BOOL WINAPI InternetGetLastResponseInfoA(LPDWORD lpdwError,
    LPSTR lpszBuffer, LPDWORD lpdwBufferLength)
{
    LPWITHREADERROR lpwite = (LPWITHREADERROR)TlsGetValue(g_dwTlsErrIndex);

    TRACE("\n");

    *lpdwError = lpwite->dwError;
    if (lpwite->dwError)
    {
        strncpy(lpszBuffer, lpwite->response, *lpdwBufferLength);
	*lpdwBufferLength = strlen(lpszBuffer);
    }
    else
        *lpdwBufferLength = 0;

    return TRUE;
}


/***********************************************************************
 *           InternetGetConnectedState (WININET.@)
 *
 * Return connected state
 *
 * RETURNS
 *    TRUE if connected
 *    if lpdwStatus is not null, return the status (off line,
 *    modem, lan...) in it.
 *    FALSE if not connected
 */
BOOL WINAPI InternetGetConnectedState(LPDWORD lpdwStatus, DWORD dwReserved)
{
    TRACE("(%p, 0x%08lx)\n", lpdwStatus, dwReserved);

    if (lpdwStatus) {
	FIXME("always returning LAN connection.\n");
	*lpdwStatus = INTERNET_CONNECTION_LAN;
    }
    return TRUE;
}

/***********************************************************************
 *           InternetGetConnectedStateEx (WININET.@)
 *
 * Return connected state
 *
 * RETURNS
 *    TRUE if connected
 *    if lpdwStatus is not null, return the status (off line,
 *    modem, lan...) in it.
 *    FALSE if not connected
 */
BOOL WINAPI InternetGetConnectedStateExW(LPDWORD lpdwStatus, LPWSTR lpszConnectionName,
                                         DWORD dwNameLen, DWORD dwReserved)
{
    TRACE("(%p, %s, %ld, 0x%08lx)\n", lpdwStatus, debugstr_w(lpszConnectionName), dwNameLen, dwReserved);

    /* Must be zero */
    if(dwReserved)
	return FALSE;

    if (lpdwStatus) {
        FIXME("always returning LAN connection.\n");
        *lpdwStatus = INTERNET_CONNECTION_LAN;
    }
    return TRUE;
}

/***********************************************************************
 *           InternetConnectA (WININET.@)
 *
 * Open a ftp, gopher or http session
 *
 * RETURNS
 *    HINTERNET a session handle on success
 *    NULL on failure
 *
 */
HINTERNET WINAPI InternetConnectW(HINTERNET hInternet,
    LPCWSTR lpszServerName, INTERNET_PORT nServerPort,
    LPCWSTR lpszUserName, LPCWSTR lpszPassword,
    DWORD dwService, DWORD dwFlags, DWORD dwContext)
{
    HINTERNET rc = (HINTERNET) NULL;

    TRACE("(%p, %s, %i, %s, %s, %li, %li, %li)\n", hInternet, debugstr_w(lpszServerName),
	  nServerPort, debugstr_w(lpszUserName), debugstr_w(lpszPassword),
	  dwService, dwFlags, dwContext);

    /* Clear any error information */
    INTERNET_SetLastError(0);

    switch (dwService)
    {
        case INTERNET_SERVICE_FTP:
            rc = FTP_Connect(hInternet, lpszServerName, nServerPort,
            lpszUserName, lpszPassword, dwFlags, dwContext);
            break;

        case INTERNET_SERVICE_HTTP:
	    rc = HTTP_Connect(hInternet, lpszServerName, nServerPort,
            lpszUserName, lpszPassword, dwFlags, dwContext);
            break;

        case INTERNET_SERVICE_GOPHER:
        default:
            break;
    }

    TRACE("returning %p\n", rc);
    return rc;
}


/***********************************************************************
 *           InternetConnectA (WININET.@)
 *
 * Open a ftp, gopher or http session
 *
 * RETURNS
 *    HINTERNET a session handle on success
 *    NULL on failure
 *
 */
HINTERNET WINAPI InternetConnectA(HINTERNET hInternet,
    LPCSTR lpszServerName, INTERNET_PORT nServerPort,
    LPCSTR lpszUserName, LPCSTR lpszPassword,
    DWORD dwService, DWORD dwFlags, DWORD dwContext)
{
    HINTERNET rc = (HINTERNET)NULL;
    INT len = 0;
    LPWSTR szServerName = NULL;
    LPWSTR szUserName = NULL;
    LPWSTR szPassword = NULL;

    if (lpszServerName)
    {
	len = MultiByteToWideChar(CP_ACP, 0, lpszServerName, -1, NULL, 0);
        szServerName = HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, lpszServerName, -1, szServerName, len);
    }
    if (lpszUserName)
    {
	len = MultiByteToWideChar(CP_ACP, 0, lpszUserName, -1, NULL, 0);
        szUserName = HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, lpszUserName, -1, szUserName, len);
    }
    if (lpszPassword)
    {
	len = MultiByteToWideChar(CP_ACP, 0, lpszPassword, -1, NULL, 0);
        szPassword = HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, lpszPassword, -1, szPassword, len);
    }


    rc = InternetConnectW(hInternet, szServerName, nServerPort,
        szUserName, szPassword, dwService, dwFlags, dwContext);

    if (szServerName) HeapFree(GetProcessHeap(), 0, szServerName);
    if (szUserName) HeapFree(GetProcessHeap(), 0, szUserName);
    if (szPassword) HeapFree(GetProcessHeap(), 0, szPassword);
    return rc;
}


/***********************************************************************
 *           InternetFindNextFileA (WININET.@)
 *
 * Continues a file search from a previous call to FindFirstFile
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI InternetFindNextFileA(HINTERNET hFind, LPVOID lpvFindData)
{
    LPWININETAPPINFOW hIC = NULL;
    LPWININETFINDNEXTA lpwh;

    TRACE("\n");

    lpwh = (LPWININETFINDNEXTA) WININET_GetObject( hFind );

    if (NULL == lpwh || lpwh->hdr.htype != WH_HFINDNEXT)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    hIC = GET_HWININET_FROM_LPWININETFINDNEXT(lpwh);
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_INTERNETFINDNEXTA *req;

        workRequest.asyncall = INTERNETFINDNEXTA;
	workRequest.handle = hFind;
        req = &workRequest.u.InternetFindNextA;
	req->lpFindFileData = lpvFindData;

	return INTERNET_AsyncCall(&workRequest);
    }
    else
    {
        return INTERNET_FindNextFileA(hFind, lpvFindData);
    }
}

/***********************************************************************
 *           INTERNET_FindNextFileA (Internal)
 *
 * Continues a file search from a previous call to FindFirstFile
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI INTERNET_FindNextFileA(HINTERNET hFind, LPVOID lpvFindData)
{
    BOOL bSuccess = TRUE;
    LPWININETAPPINFOW hIC = NULL;
    LPWIN32_FIND_DATAA lpFindFileData;
    LPWININETFINDNEXTA lpwh;

    TRACE("\n");

    lpwh = (LPWININETFINDNEXTA) WININET_GetObject( hFind );
    if (NULL == lpwh || lpwh->hdr.htype != WH_HFINDNEXT)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    /* Clear any error information */
    INTERNET_SetLastError(0);

    if (lpwh->hdr.lpwhparent->htype != WH_HFTPSESSION)
    {
        FIXME("Only FTP find next supported\n");
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
	return FALSE;
    }

    TRACE("index(%d) size(%ld)\n", lpwh->index, lpwh->size);

    lpFindFileData = (LPWIN32_FIND_DATAA) lpvFindData;
    ZeroMemory(lpFindFileData, sizeof(WIN32_FIND_DATAA));

    if (lpwh->index >= lpwh->size)
    {
        INTERNET_SetLastError(ERROR_NO_MORE_FILES);
        bSuccess = FALSE;
	goto lend;
    }

    FTP_ConvertFileProp(&lpwh->lpafp[lpwh->index], lpFindFileData);
    lpwh->index++;

    TRACE("\nName: %s\nSize: %ld\n", lpFindFileData->cFileName, lpFindFileData->nFileSizeLow);

lend:

    hIC = GET_HWININET_FROM_LPWININETFINDNEXT(lpwh);
    if (hIC->lpfnStatusCB)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = iar.dwError = bSuccess ? ERROR_SUCCESS :
                                               INTERNET_GetLastError();

        SendAsyncCallback(hIC, hFind, lpwh->hdr.dwContext,
                      INTERNET_STATUS_REQUEST_COMPLETE, &iar,
                       sizeof(INTERNET_ASYNC_RESULT));
    }

    return bSuccess;
}


/***********************************************************************
 *           INTERNET_CloseHandle (internal)
 *
 * Close internet handle
 *
 * RETURNS
 *    Void
 *
 */
VOID INTERNET_CloseHandle(LPWININETAPPINFOW lpwai)
{
    TRACE("%p\n",lpwai);

    SendAsyncCallback(lpwai, lpwai, lpwai->hdr.dwContext,
                      INTERNET_STATUS_HANDLE_CLOSING, lpwai,
                      sizeof(HINTERNET));

    if (lpwai->lpszAgent)
        HeapFree(GetProcessHeap(), 0, lpwai->lpszAgent);

    if (lpwai->lpszProxy)
        HeapFree(GetProcessHeap(), 0, lpwai->lpszProxy);

    if (lpwai->lpszProxyBypass)
        HeapFree(GetProcessHeap(), 0, lpwai->lpszProxyBypass);

    if (lpwai->lpszProxyUsername)
        HeapFree(GetProcessHeap(), 0, lpwai->lpszProxyUsername);

    if (lpwai->lpszProxyPassword)
        HeapFree(GetProcessHeap(), 0, lpwai->lpszProxyPassword);

    HeapFree(GetProcessHeap(), 0, lpwai);
}


/***********************************************************************
 *           InternetCloseHandle (WININET.@)
 *
 * Generic close handle function
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI InternetCloseHandle(HINTERNET hInternet)
{
    BOOL retval;
    LPWININETHANDLEHEADER lpwh;

    TRACE("%p\n",hInternet);

    lpwh = WININET_GetObject( hInternet );
    if (NULL == lpwh)
    {
	INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

	/* Clear any error information */
	INTERNET_SetLastError(0);
        retval = FALSE;

	switch (lpwh->htype)
	{
	    case WH_HINIT:
		INTERNET_CloseHandle((LPWININETAPPINFOW) lpwh);
		retval = TRUE;
		break;

	    case WH_HHTTPSESSION:
		HTTP_CloseHTTPSessionHandle((LPWININETHTTPSESSIONW) lpwh);
		retval = TRUE;
		break;

	    case WH_HHTTPREQ:
		HTTP_CloseHTTPRequestHandle((LPWININETHTTPREQW) lpwh);
		retval = TRUE;
		break;

	    case WH_HFTPSESSION:
		retval = FTP_CloseSessionHandle((LPWININETFTPSESSIONA) lpwh);
		break;

	    case WH_HFINDNEXT:
		retval = FTP_CloseFindNextHandle((LPWININETFINDNEXTA) lpwh);
		break;

	    case WH_HFILE:
		retval = FTP_CloseFileTransferHandle((LPWININETFILE) lpwh);
		break;
		
	    default:
		break;
	}
    if( retval )
        WININET_FreeHandle( hInternet );

    return retval;
}


/***********************************************************************
 *           ConvertUrlComponentValue (Internal)
 *
 * Helper function for InternetCrackUrlW
 *
 */
void ConvertUrlComponentValue(LPSTR* lppszComponent, LPDWORD dwComponentLen,
                              LPWSTR lpwszComponent, DWORD dwwComponentLen,
                              LPCSTR lpszStart,
                              LPCWSTR lpwszStart)
{
    if (*dwComponentLen != 0)
    {
        int nASCIILength=WideCharToMultiByte(CP_ACP,0,lpwszComponent,dwwComponentLen,NULL,0,NULL,NULL);
        if (*lppszComponent == NULL)
        {
            int nASCIIOffset=WideCharToMultiByte(CP_ACP,0,lpwszStart,lpwszComponent-lpwszStart,NULL,0,NULL,NULL);
            *lppszComponent = (LPSTR)lpszStart+nASCIIOffset;
            *dwComponentLen = nASCIILength;
        }
        else
        {
            INT ncpylen = min((*dwComponentLen)-1, nASCIILength);
            WideCharToMultiByte(CP_ACP,0,lpwszComponent,dwwComponentLen,*lppszComponent,ncpylen+1,NULL,NULL);
            (*lppszComponent)[ncpylen]=0;
            *dwComponentLen = ncpylen;
        }
    }
}


/***********************************************************************
 *           InternetCrackUrlA (WININET.@)
 *
 * Break up URL into its components
 *
 * TODO: Handle dwFlags
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI InternetCrackUrlA(LPCSTR lpszUrl, DWORD dwUrlLength, DWORD dwFlags,
    LPURL_COMPONENTSA lpUrlComponents)
{
  DWORD nLength;
  URL_COMPONENTSW UCW;
  WCHAR* lpwszUrl;
  if(dwUrlLength==0)
      dwUrlLength=strlen(lpszUrl);
  lpwszUrl=HeapAlloc(GetProcessHeap(), 0, sizeof(WCHAR)*(dwUrlLength+1));
  memset(lpwszUrl,0,sizeof(WCHAR)*(dwUrlLength+1));
  nLength=MultiByteToWideChar(CP_ACP,0,lpszUrl,dwUrlLength,lpwszUrl,dwUrlLength+1);
  memset(&UCW,0,sizeof(UCW));
  if(lpUrlComponents->dwHostNameLength!=0)
      UCW.dwHostNameLength=1;
  if(lpUrlComponents->dwUserNameLength!=0)
      UCW.dwUserNameLength=1;
  if(lpUrlComponents->dwPasswordLength!=0)
      UCW.dwPasswordLength=1;
  if(lpUrlComponents->dwUrlPathLength!=0)
      UCW.dwUrlPathLength=1;
  if(lpUrlComponents->dwSchemeLength!=0)
      UCW.dwSchemeLength=1;
  if(lpUrlComponents->dwExtraInfoLength!=0)
      UCW.dwExtraInfoLength=1;
  if(!InternetCrackUrlW(lpwszUrl,nLength,dwFlags,&UCW))
  {
      HeapFree(GetProcessHeap(), 0, lpwszUrl);
      return FALSE;
  }
  ConvertUrlComponentValue(&lpUrlComponents->lpszHostName, &lpUrlComponents->dwHostNameLength,
                           UCW.lpszHostName, UCW.dwHostNameLength,
                           lpszUrl, lpwszUrl);
  ConvertUrlComponentValue(&lpUrlComponents->lpszUserName, &lpUrlComponents->dwUserNameLength,
                           UCW.lpszUserName, UCW.dwUserNameLength,
                           lpszUrl, lpwszUrl);
  ConvertUrlComponentValue(&lpUrlComponents->lpszPassword, &lpUrlComponents->dwPasswordLength,
                           UCW.lpszPassword, UCW.dwPasswordLength,
                           lpszUrl, lpwszUrl);
  ConvertUrlComponentValue(&lpUrlComponents->lpszUrlPath, &lpUrlComponents->dwUrlPathLength,
                           UCW.lpszUrlPath, UCW.dwUrlPathLength,
                           lpszUrl, lpwszUrl);
  ConvertUrlComponentValue(&lpUrlComponents->lpszScheme, &lpUrlComponents->dwSchemeLength,
                           UCW.lpszScheme, UCW.dwSchemeLength,
                           lpszUrl, lpwszUrl);
  ConvertUrlComponentValue(&lpUrlComponents->lpszExtraInfo, &lpUrlComponents->dwExtraInfoLength,
                           UCW.lpszExtraInfo, UCW.dwExtraInfoLength,
                           lpszUrl, lpwszUrl);
  lpUrlComponents->nScheme=UCW.nScheme;
  lpUrlComponents->nPort=UCW.nPort;
  HeapFree(GetProcessHeap(), 0, lpwszUrl);
  
  TRACE("%s: scheme(%s) host(%s) path(%s) extra(%s)\n", lpszUrl,
          debugstr_an(lpUrlComponents->lpszScheme,lpUrlComponents->dwSchemeLength),
          debugstr_an(lpUrlComponents->lpszHostName,lpUrlComponents->dwHostNameLength),
          debugstr_an(lpUrlComponents->lpszUrlPath,lpUrlComponents->dwUrlPathLength),
          debugstr_an(lpUrlComponents->lpszExtraInfo,lpUrlComponents->dwExtraInfoLength));

  return TRUE;
}

/***********************************************************************
 *           GetInternetSchemeW (internal)
 *
 * Get scheme of url
 *
 * RETURNS
 *    scheme on success
 *    INTERNET_SCHEME_UNKNOWN on failure
 *
 */
INTERNET_SCHEME GetInternetSchemeW(LPCWSTR lpszScheme, INT nMaxCmp)
{
    INTERNET_SCHEME iScheme=INTERNET_SCHEME_UNKNOWN;
    WCHAR lpszFtp[]={'f','t','p',0};
    WCHAR lpszGopher[]={'g','o','p','h','e','r',0};
    WCHAR lpszHttp[]={'h','t','t','p',0};
    WCHAR lpszHttps[]={'h','t','t','p','s',0};
    WCHAR lpszFile[]={'f','i','l','e',0};
    WCHAR lpszNews[]={'n','e','w','s',0};
    WCHAR lpszMailto[]={'m','a','i','l','t','o',0};
    WCHAR lpszRes[]={'r','e','s',0};
    WCHAR* tempBuffer=NULL;
    TRACE("\n");
    if(lpszScheme==NULL)
        return INTERNET_SCHEME_UNKNOWN;

    tempBuffer=HeapAlloc(GetProcessHeap(),0,(nMaxCmp+1)*sizeof(WCHAR));
    strncpyW(tempBuffer,lpszScheme,nMaxCmp);
    tempBuffer[nMaxCmp]=0;
    strlwrW(tempBuffer);
    if (nMaxCmp==strlenW(lpszFtp) && !strncmpW(lpszFtp, tempBuffer, nMaxCmp))
        iScheme=INTERNET_SCHEME_FTP;
    else if (nMaxCmp==strlenW(lpszGopher) && !strncmpW(lpszGopher, tempBuffer, nMaxCmp))
        iScheme=INTERNET_SCHEME_GOPHER;
    else if (nMaxCmp==strlenW(lpszHttp) && !strncmpW(lpszHttp, tempBuffer, nMaxCmp))
        iScheme=INTERNET_SCHEME_HTTP;
    else if (nMaxCmp==strlenW(lpszHttps) && !strncmpW(lpszHttps, tempBuffer, nMaxCmp))
        iScheme=INTERNET_SCHEME_HTTPS;
    else if (nMaxCmp==strlenW(lpszFile) && !strncmpW(lpszFile, tempBuffer, nMaxCmp))
        iScheme=INTERNET_SCHEME_FILE;
    else if (nMaxCmp==strlenW(lpszNews) && !strncmpW(lpszNews, tempBuffer, nMaxCmp))
        iScheme=INTERNET_SCHEME_NEWS;
    else if (nMaxCmp==strlenW(lpszMailto) && !strncmpW(lpszMailto, tempBuffer, nMaxCmp))
        iScheme=INTERNET_SCHEME_MAILTO;
    else if (nMaxCmp==strlenW(lpszRes) && !strncmpW(lpszRes, tempBuffer, nMaxCmp))
        iScheme=INTERNET_SCHEME_RES;
    HeapFree(GetProcessHeap(),0,tempBuffer);
    return iScheme;
}

/***********************************************************************
 *           SetUrlComponentValueW (Internal)
 *
 * Helper function for InternetCrackUrlW
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL SetUrlComponentValueW(LPWSTR* lppszComponent, LPDWORD dwComponentLen, LPCWSTR lpszStart, INT len)
{
    TRACE("%s (%d)\n", debugstr_wn(lpszStart,len), len);

    if (*dwComponentLen != 0 || *lppszComponent == NULL)
    {
        if (*lppszComponent == NULL)
        {
            *lppszComponent = (LPWSTR)lpszStart;
            *dwComponentLen = len;
        }
        else
        {
            INT ncpylen = min((*dwComponentLen)-1, len);
            strncpyW(*lppszComponent, lpszStart, ncpylen);
            (*lppszComponent)[ncpylen] = '\0';
            *dwComponentLen = ncpylen;
        }
    }

    return TRUE;
}

/***********************************************************************
 *           InternetCrackUrlW   (WININET.@)
 */
BOOL WINAPI InternetCrackUrlW(LPCWSTR lpszUrl, DWORD dwUrlLength, DWORD dwFlags,
                              LPURL_COMPONENTSW lpUC)
{
  /*
   * RFC 1808
   * <protocol>:[//<net_loc>][/path][;<params>][?<query>][#<fragment>]
   *
   */
    LPWSTR lpszParam    = NULL;
    BOOL  bIsAbsolute = FALSE;
    LPWSTR lpszap = (WCHAR*)lpszUrl;
    LPWSTR lpszcp = NULL;
    WCHAR lpszSeparators[3]={';','?',0};
    WCHAR lpszSlash[2]={'/',0};
    if(dwUrlLength==0)
        dwUrlLength=strlenW(lpszUrl);

    TRACE("\n");

    /* Determine if the URI is absolute. */
    while (*lpszap != '\0')
    {
        if (isalnumW(*lpszap))
        {
            lpszap++;
            continue;
        }
        if ((*lpszap == ':') && (lpszap - lpszUrl >= 2))
        {
            bIsAbsolute = TRUE;
            lpszcp = lpszap;
        }
        else
        {
            lpszcp = (LPWSTR)lpszUrl; /* Relative url */
        }

        break;
    }

    /* Parse <params> */
    lpszParam = strpbrkW(lpszap, lpszSeparators);
    if (lpszParam != NULL)
    {
        if (!SetUrlComponentValueW(&lpUC->lpszExtraInfo, &lpUC->dwExtraInfoLength,
                                   lpszParam, dwUrlLength-(lpszParam-lpszUrl)))
        {
            return FALSE;
        }
    }

    if (bIsAbsolute) /* Parse <protocol>:[//<net_loc>] */
    {
        LPWSTR lpszNetLoc;
        WCHAR wszAbout[]={'a','b','o','u','t',':',0};

        /* Get scheme first. */
        lpUC->nScheme = GetInternetSchemeW(lpszUrl, lpszcp - lpszUrl);
        if (!SetUrlComponentValueW(&lpUC->lpszScheme, &lpUC->dwSchemeLength,
                                   lpszUrl, lpszcp - lpszUrl))
            return FALSE;

        /* Eat ':' in protocol. */
        lpszcp++;

        /* if the scheme is "about", there is no host */
        if(strncmpW(wszAbout,lpszUrl, lpszcp - lpszUrl)==0)
        {
            SetUrlComponentValueW(&lpUC->lpszUserName, &lpUC->dwUserNameLength, NULL, 0);
            SetUrlComponentValueW(&lpUC->lpszPassword, &lpUC->dwPasswordLength, NULL, 0);
            SetUrlComponentValueW(&lpUC->lpszHostName, &lpUC->dwHostNameLength, NULL, 0);
            lpUC->nPort = 0;
        }
        else
        {
            /* Skip over slashes. */
            if (*lpszcp == '/')
            {
                lpszcp++;
                if (*lpszcp == '/')
                {
                    lpszcp++;
                    if (*lpszcp == '/')
                        lpszcp++;
                }
            }

            lpszNetLoc = strpbrkW(lpszcp, lpszSlash);
            if (lpszParam)
            {
                if (lpszNetLoc)
                    lpszNetLoc = min(lpszNetLoc, lpszParam);
                else
                    lpszNetLoc = lpszParam;
            }
            else if (!lpszNetLoc)
                lpszNetLoc = lpszcp + dwUrlLength-(lpszcp-lpszUrl);

            /* Parse net-loc */
            if (lpszNetLoc)
            {
                LPWSTR lpszHost;
                LPWSTR lpszPort;

                /* [<user>[<:password>]@]<host>[:<port>] */
                /* First find the user and password if they exist */

                lpszHost = strchrW(lpszcp, '@');
                if (lpszHost == NULL || lpszHost > lpszNetLoc)
                {
                    /* username and password not specified. */
                    SetUrlComponentValueW(&lpUC->lpszUserName, &lpUC->dwUserNameLength, NULL, 0);
                    SetUrlComponentValueW(&lpUC->lpszPassword, &lpUC->dwPasswordLength, NULL, 0);
                }
                else /* Parse out username and password */
                {
                    LPWSTR lpszUser = lpszcp;
                    LPWSTR lpszPasswd = lpszHost;

                    while (lpszcp < lpszHost)
                    {
                        if (*lpszcp == ':')
                            lpszPasswd = lpszcp;

                        lpszcp++;
                    }

                    SetUrlComponentValueW(&lpUC->lpszUserName, &lpUC->dwUserNameLength,
                                          lpszUser, lpszPasswd - lpszUser);

                    if (lpszPasswd != lpszHost)
                        lpszPasswd++;
                    SetUrlComponentValueW(&lpUC->lpszPassword, &lpUC->dwPasswordLength,
                                          lpszPasswd == lpszHost ? NULL : lpszPasswd,
                                          lpszHost - lpszPasswd);

                    lpszcp++; /* Advance to beginning of host */
                }

                /* Parse <host><:port> */

                lpszHost = lpszcp;
                lpszPort = lpszNetLoc;

                /* special case for res:// URLs: there is no port here, so the host is the
                   entire string up to the first '/' */
                if(lpUC->nScheme==INTERNET_SCHEME_RES)
                {
                    SetUrlComponentValueW(&lpUC->lpszHostName, &lpUC->dwHostNameLength,
                                          lpszHost, lpszPort - lpszHost);
                    lpUC->nPort = 0;
                    lpszcp=lpszNetLoc;
                }
                else
                {
                    while (lpszcp < lpszNetLoc)
                    {
                        if (*lpszcp == ':')
                            lpszPort = lpszcp;

                        lpszcp++;
                    }

                    /* If the scheme is "file" and the host is just one letter, it's not a host */
                    if(lpUC->nScheme==INTERNET_SCHEME_FILE && (lpszPort-lpszHost)==1)
                    {
                        lpszcp=lpszHost;
                        SetUrlComponentValueW(&lpUC->lpszHostName, &lpUC->dwHostNameLength,
                                              NULL, 0);
                        lpUC->nPort = 0;
                    }
                    else
                    {
                        SetUrlComponentValueW(&lpUC->lpszHostName, &lpUC->dwHostNameLength,
                                              lpszHost, lpszPort - lpszHost);
                        if (lpszPort != lpszNetLoc)
                            lpUC->nPort = atoiW(++lpszPort);
                        else
                            lpUC->nPort = 0;
                    }
                }
            }
        }
    }

    /* Here lpszcp points to:
     *
     * <protocol>:[//<net_loc>][/path][;<params>][?<query>][#<fragment>]
     *                          ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
     */
    if (lpszcp != 0 && *lpszcp != '\0' && (!lpszParam || lpszcp < lpszParam))
    {
        INT len;

        /* Only truncate the parameter list if it's already been saved
         * in lpUC->lpszExtraInfo.
         */
        if (lpszParam && lpUC->dwExtraInfoLength)
            len = lpszParam - lpszcp;
        else
        {
            /* Leave the parameter list in lpszUrlPath.  Strip off any trailing
             * newlines if necessary.
             */
            LPWSTR lpsznewline = strchrW(lpszcp, '\n');
            if (lpsznewline != NULL)
                len = lpsznewline - lpszcp;
            else
                len = dwUrlLength-(lpszcp-lpszUrl);
        }

        if (!SetUrlComponentValueW(&lpUC->lpszUrlPath, &lpUC->dwUrlPathLength,
                                   lpszcp, len))
            return FALSE;
    }
    else
    {
        lpUC->dwUrlPathLength = 0;
    }

    TRACE("%s: host(%s) path(%s) extra(%s)\n", debugstr_wn(lpszUrl,dwUrlLength),
             debugstr_wn(lpUC->lpszHostName,lpUC->dwHostNameLength),
             debugstr_wn(lpUC->lpszUrlPath,lpUC->dwUrlPathLength),
             debugstr_wn(lpUC->lpszExtraInfo,lpUC->dwExtraInfoLength));

    return TRUE;
}

/***********************************************************************
 *           InternetAttemptConnect (WININET.@)
 *
 * Attempt to make a connection to the internet
 *
 * RETURNS
 *    ERROR_SUCCESS on success
 *    Error value   on failure
 *
 */
DWORD WINAPI InternetAttemptConnect(DWORD dwReserved)
{
    FIXME("Stub\n");
    return ERROR_SUCCESS;
}


/***********************************************************************
 *           InternetCanonicalizeUrlA (WININET.@)
 *
 * Escape unsafe characters and spaces
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI InternetCanonicalizeUrlA(LPCSTR lpszUrl, LPSTR lpszBuffer,
	LPDWORD lpdwBufferLength, DWORD dwFlags)
{
    HRESULT hr;
    TRACE("%s %p %p %08lx\n",debugstr_a(lpszUrl), lpszBuffer,
	  lpdwBufferLength, dwFlags);

    /* Flip this bit to correspond to URL_ESCAPE_UNSAFE */
    dwFlags ^= ICU_NO_ENCODE;

    dwFlags |= 0x80000000; /* Don't know what this means */

    hr = UrlCanonicalizeA(lpszUrl, lpszBuffer, lpdwBufferLength, dwFlags);

    return (hr == S_OK) ? TRUE : FALSE;
}

/***********************************************************************
 *           InternetCanonicalizeUrlW (WININET.@)
 *
 * Escape unsafe characters and spaces
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI InternetCanonicalizeUrlW(LPCWSTR lpszUrl, LPWSTR lpszBuffer,
    LPDWORD lpdwBufferLength, DWORD dwFlags)
{
    HRESULT hr;
    TRACE("%s %p %p %08lx\n", debugstr_w(lpszUrl), lpszBuffer,
        lpdwBufferLength, dwFlags);

    /* Flip this bit to correspond to URL_ESCAPE_UNSAFE */
    dwFlags ^= ICU_NO_ENCODE;

    dwFlags |= 0x80000000; /* Don't know what this means */

    hr = UrlCanonicalizeW(lpszUrl, lpszBuffer, lpdwBufferLength, dwFlags);

    return (hr == S_OK) ? TRUE : FALSE;
}


/***********************************************************************
 *           InternetSetStatusCallbackA (WININET.@)
 *
 * Sets up a callback function which is called as progress is made
 * during an operation.
 *
 * RETURNS
 *    Previous callback or NULL 	on success
 *    INTERNET_INVALID_STATUS_CALLBACK  on failure
 *
 */
INTERNET_STATUS_CALLBACK WINAPI InternetSetStatusCallbackA(
	HINTERNET hInternet ,INTERNET_STATUS_CALLBACK lpfnIntCB)
{
    INTERNET_STATUS_CALLBACK retVal;
    LPWININETAPPINFOW lpwai;

    lpwai = (LPWININETAPPINFOW)WININET_GetObject(hInternet);
    if (!lpwai)
        return NULL;

    TRACE("0x%08lx\n", (ULONG)hInternet);
    if (lpwai->hdr.htype != WH_HINIT)
        return INTERNET_INVALID_STATUS_CALLBACK;

    retVal = lpwai->lpfnStatusCB;
    lpwai->lpfnStatusCB = lpfnIntCB;

    return retVal;
}

/***********************************************************************
 *           InternetSetFilePointer (WININET.@)
 */
DWORD WINAPI InternetSetFilePointer(HINTERNET f1, LONG f2, PVOID f3, DWORD f4, DWORD f5)
{
    FIXME("stub\n");
    return FALSE;
}

/***********************************************************************
 *           InternetWriteFile (WININET.@)
 *
 * Write data to an open internet file
 *
 * RETURNS
 *    TRUE  on success
 *    FALSE on failure
 *
 */
BOOL WINAPI InternetWriteFile(HINTERNET hFile, LPCVOID lpBuffer ,
	DWORD dwNumOfBytesToWrite, LPDWORD lpdwNumOfBytesWritten)
{
    BOOL retval = FALSE;
    int nSocket = -1;
    LPWININETHANDLEHEADER lpwh;

    TRACE("\n");
    lpwh = (LPWININETHANDLEHEADER) WININET_GetObject( hFile );
    if (NULL == lpwh)
        return FALSE;

    switch (lpwh->htype)
    {
        case WH_HHTTPREQ:
            FIXME("This shouldn't be here! We don't support this kind"
                  " of connection anymore. Must use NETCON functions,"
                  " especially if using SSL\n");
            nSocket = ((LPWININETHTTPREQW)lpwh)->netConnection.socketFD;
            break;

        case WH_HFILE:
            nSocket = ((LPWININETFILE)lpwh)->nDataSocket;
            break;

        default:
            break;
    }

    if (nSocket != -1)
    {
        int res = send(nSocket, lpBuffer, dwNumOfBytesToWrite, 0);
        retval = (res >= 0);
        *lpdwNumOfBytesWritten = retval ? res : 0;
    }

    return retval;
}


/***********************************************************************
 *           InternetReadFile (WININET.@)
 *
 * Read data from an open internet file
 *
 * RETURNS
 *    TRUE  on success
 *    FALSE on failure
 *
 */
BOOL WINAPI InternetReadFile(HINTERNET hFile, LPVOID lpBuffer,
	DWORD dwNumOfBytesToRead, LPDWORD dwNumOfBytesRead)
{
    BOOL retval = FALSE;
    int nSocket = -1;
    LPWININETHANDLEHEADER lpwh;

    TRACE("\n");

    lpwh = (LPWININETHANDLEHEADER) WININET_GetObject( hFile );
    if (NULL == lpwh)
        return FALSE;

    /* FIXME: this should use NETCON functions! */
    switch (lpwh->htype)
    {
        case WH_HHTTPREQ:
            if (!NETCON_recv(&((LPWININETHTTPREQW)lpwh)->netConnection, lpBuffer,
                             dwNumOfBytesToRead, 0, (int *)dwNumOfBytesRead))
            {
                *dwNumOfBytesRead = 0;
                retval = TRUE; /* Under windows, it seems to return 0 even if nothing was read... */
            }
            else
                retval = TRUE;
            break;

        case WH_HFILE:
            /* FIXME: FTP should use NETCON_ stuff */
            nSocket = ((LPWININETFILE)lpwh)->nDataSocket;
            if (nSocket != -1)
            {
                int res = recv(nSocket, lpBuffer, dwNumOfBytesToRead, 0);
                retval = (res >= 0);
                *dwNumOfBytesRead = retval ? res : 0;
            }
            break;

        default:
            break;
    }

    return retval;
}

/***********************************************************************
 *           InternetReadFileExA (WININET.@)
 *
 * Read data from an open internet file
 *
 * RETURNS
 *    TRUE  on success
 *    FALSE on failure
 *
 */
BOOL WINAPI InternetReadFileExA(HINTERNET hFile, LPINTERNET_BUFFERSA lpBuffer,
	DWORD dwFlags, DWORD dwContext)
{
  FIXME("stub\n");
  return FALSE;
}

/***********************************************************************
 *           InternetReadFileExW (WININET.@)
 *
 * Read data from an open internet file
 *
 * RETURNS
 *    TRUE  on success
 *    FALSE on failure
 *
 */
BOOL WINAPI InternetReadFileExW(HINTERNET hFile, LPINTERNET_BUFFERSW lpBuffer,
	DWORD dwFlags, DWORD dwContext)
{
  FIXME("stub\n");

  INTERNET_SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
  return FALSE;
}

/***********************************************************************
 *           INET_QueryOptionHelper (internal)
 */
static BOOL INET_QueryOptionHelper(BOOL bIsUnicode, HINTERNET hInternet, DWORD dwOption,
                                   LPVOID lpBuffer, LPDWORD lpdwBufferLength)
{
    LPWININETHANDLEHEADER lpwhh;
    BOOL bSuccess = FALSE;

    TRACE("(%p, 0x%08lx, %p, %p)\n", hInternet, dwOption, lpBuffer, lpdwBufferLength);

    lpwhh = (LPWININETHANDLEHEADER) WININET_GetObject( hInternet );
    if( !lpwhh )
        return FALSE;

    switch (dwOption)
    {
        case INTERNET_OPTION_HANDLE_TYPE:
        {
            ULONG type = lpwhh->htype;
            TRACE("INTERNET_OPTION_HANDLE_TYPE: %ld\n", type);

            if (*lpdwBufferLength < sizeof(ULONG))
                INTERNET_SetLastError(ERROR_INSUFFICIENT_BUFFER);
            else
            {
                memcpy(lpBuffer, &type, sizeof(ULONG));
                    *lpdwBufferLength = sizeof(ULONG);
                bSuccess = TRUE;
            }
            break;
        }

        case INTERNET_OPTION_REQUEST_FLAGS:
        {
            ULONG flags = 4;
            TRACE("INTERNET_OPTION_REQUEST_FLAGS: %ld\n", flags);
            if (*lpdwBufferLength < sizeof(ULONG))
                INTERNET_SetLastError(ERROR_INSUFFICIENT_BUFFER);
            else
            {
                memcpy(lpBuffer, &flags, sizeof(ULONG));
                    *lpdwBufferLength = sizeof(ULONG);
                bSuccess = TRUE;
            }
            break;
        }

        case INTERNET_OPTION_URL:
        case INTERNET_OPTION_DATAFILE_NAME:
        {
            ULONG type = lpwhh->htype;
            if (type == WH_HHTTPREQ)
            {
                LPWININETHTTPREQW lpreq = (LPWININETHTTPREQW) lpwhh;
                WCHAR url[1023];
                WCHAR szFmt[] = {'h','t','t','p',':','/','/','%','s','%','s',0};

                sprintfW(url,szFmt,lpreq->lpszHostName,lpreq->lpszPath);
                TRACE("INTERNET_OPTION_URL: %s\n",debugstr_w(url));
                if (*lpdwBufferLength < strlenW(url)+1)
                    INTERNET_SetLastError(ERROR_INSUFFICIENT_BUFFER);
                else
                {
                    if(!bIsUnicode)
                    {
                        *lpdwBufferLength=WideCharToMultiByte(CP_ACP,0,url,-1,lpBuffer,*lpdwBufferLength,NULL,NULL);
                    }
                    else
                    {
                        strcpyW(lpBuffer, url);
                        *lpdwBufferLength = strlenW(url)+1;
                    }
                    bSuccess = TRUE;
                }
            }
            break;
        }
       case INTERNET_OPTION_HTTP_VERSION:
       {
            /*
             * Presently hardcoded to 1.1
             */
            ((HTTP_VERSION_INFO*)lpBuffer)->dwMajorVersion = 1;
            ((HTTP_VERSION_INFO*)lpBuffer)->dwMinorVersion = 1;
            bSuccess = TRUE;
        break;
       }
       case INTERNET_OPTION_SECURITY_FLAGS:
         FIXME("INTERNET_OPTION_SECURITY_FLAGS: Stub\n");
         break;

       default:
         FIXME("Stub! %ld \n",dwOption);
         break;
    }

    return bSuccess;
}

/***********************************************************************
 *           InternetQueryOptionW (WININET.@)
 *
 * Queries an options on the specified handle
 *
 * RETURNS
 *    TRUE  on success
 *    FALSE on failure
 *
 */
BOOL WINAPI InternetQueryOptionW(HINTERNET hInternet, DWORD dwOption,
                                 LPVOID lpBuffer, LPDWORD lpdwBufferLength)
{
    return INET_QueryOptionHelper(TRUE, hInternet, dwOption, lpBuffer, lpdwBufferLength);
}

/***********************************************************************
 *           InternetQueryOptionA (WININET.@)
 *
 * Queries an options on the specified handle
 *
 * RETURNS
 *    TRUE  on success
 *    FALSE on failure
 *
 */
BOOL WINAPI InternetQueryOptionA(HINTERNET hInternet, DWORD dwOption,
                                 LPVOID lpBuffer, LPDWORD lpdwBufferLength)
{
    return INET_QueryOptionHelper(FALSE, hInternet, dwOption, lpBuffer, lpdwBufferLength);
}


/***********************************************************************
 *           InternetSetOptionW (WININET.@)
 *
 * Sets an options on the specified handle
 *
 * RETURNS
 *    TRUE  on success
 *    FALSE on failure
 *
 */
BOOL WINAPI InternetSetOptionW(HINTERNET hInternet, DWORD dwOption,
                           LPVOID lpBuffer, DWORD dwBufferLength)
{
    LPWININETHANDLEHEADER lpwhh;

    TRACE("0x%08lx\n", dwOption);

    lpwhh = (LPWININETHANDLEHEADER) WININET_GetObject( hInternet );
    if( !lpwhh )
        return FALSE;

    switch (dwOption)
    {
    case INTERNET_OPTION_HTTP_VERSION:
      {
        HTTP_VERSION_INFO* pVersion=(HTTP_VERSION_INFO*)lpBuffer;
        FIXME("Option INTERNET_OPTION_HTTP_VERSION(%ld,%ld): STUB\n",pVersion->dwMajorVersion,pVersion->dwMinorVersion);
      }
      break;
    case INTERNET_OPTION_ERROR_MASK:
      {
        unsigned long flags=*(unsigned long*)lpBuffer;
        FIXME("Option INTERNET_OPTION_ERROR_MASK(%ld): STUB\n",flags);
      }
      break;
    case INTERNET_OPTION_CODEPAGE:
      {
        unsigned long codepage=*(unsigned long*)lpBuffer;
        FIXME("Option INTERNET_OPTION_CODEPAGE (%ld): STUB\n",codepage);
      }
      break;
    case INTERNET_OPTION_REQUEST_PRIORITY:
      {
        unsigned long priority=*(unsigned long*)lpBuffer;
        FIXME("Option INTERNET_OPTION_REQUEST_PRIORITY (%ld): STUB\n",priority);
      }
      break;
    case INTERNET_OPTION_CONNECT_TIMEOUT:
      {
        unsigned long connecttimeout=*(unsigned long*)lpBuffer;
        FIXME("Option INTERNET_OPTION_CONNECT_TIMEOUT (%ld): STUB\n",connecttimeout);
      }
      break;
    case INTERNET_OPTION_DATA_RECEIVE_TIMEOUT:
      {
        unsigned long receivetimeout=*(unsigned long*)lpBuffer;
        FIXME("Option INTERNET_OPTION_DATA_RECEIVE_TIMEOUT (%ld): STUB\n",receivetimeout);
      }
      break;
    case INTERNET_OPTION_RESET_URLCACHE_SESSION:
        FIXME("Option INTERNET_OPTION_RESET_URLCACHE_SESSION: STUB\n");
        break;
    case INTERNET_OPTION_END_BROWSER_SESSION:
        FIXME("Option INTERNET_OPTION_END_BROWSER_SESSION: STUB\n");
        break;
    case INTERNET_OPTION_CONNECTED_STATE:
        FIXME("Option INTERNET_OPTION_CONNECTED_STATE: STUB\n");
        break;
    default:
        FIXME("Option %ld STUB\n",dwOption);
        INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return TRUE;
}


/***********************************************************************
 *           InternetSetOptionA (WININET.@)
 *
 * Sets an options on the specified handle.
 *
 * RETURNS
 *    TRUE  on success
 *    FALSE on failure
 *
 */
BOOL WINAPI InternetSetOptionA(HINTERNET hInternet, DWORD dwOption,
                           LPVOID lpBuffer, DWORD dwBufferLength)
{
    LPVOID wbuffer;
    DWORD wlen;
    BOOL r;

    switch( dwOption )
    {
    case INTERNET_OPTION_PROXY:
        {
        LPINTERNET_PROXY_INFOA pi = (LPINTERNET_PROXY_INFOA) lpBuffer;
        LPINTERNET_PROXY_INFOW piw;
        DWORD proxlen, prbylen;
        LPWSTR prox, prby;

        proxlen = MultiByteToWideChar( CP_ACP, 0, pi->lpszProxy, -1, NULL, 0);
        prbylen= MultiByteToWideChar( CP_ACP, 0, pi->lpszProxyBypass, -1, NULL, 0);
        wlen = sizeof(*piw) + proxlen + prbylen;
        wbuffer = HeapAlloc( GetProcessHeap(), 0, wlen*sizeof(WCHAR) );
        piw = (LPINTERNET_PROXY_INFOW) wbuffer;
        piw->dwAccessType = pi->dwAccessType;
        prox = (LPWSTR) &piw[1];
        prby = &prox[proxlen+1];
        MultiByteToWideChar( CP_ACP, 0, pi->lpszProxy, -1, prox, proxlen);
        MultiByteToWideChar( CP_ACP, 0, pi->lpszProxyBypass, -1, prby, prbylen);
        piw->lpszProxy = prox;
        piw->lpszProxyBypass = prby;
        }
        break;
    case INTERNET_OPTION_USER_AGENT:
    case INTERNET_OPTION_USERNAME:
    case INTERNET_OPTION_PASSWORD:
        wlen = MultiByteToWideChar( CP_ACP, 0, lpBuffer, dwBufferLength,
                                   NULL, 0 );
        wbuffer = HeapAlloc( GetProcessHeap(), 0, wlen*sizeof(WCHAR) );
        MultiByteToWideChar( CP_ACP, 0, lpBuffer, dwBufferLength,
                                   wbuffer, wlen );
        break;
    default:
        wbuffer = lpBuffer;
        wlen = dwBufferLength;
    }

    r = InternetSetOptionW(hInternet,dwOption, wbuffer, wlen);

    if( lpBuffer != wbuffer )
        HeapFree( GetProcessHeap(), 0, wbuffer );

    return r;
}


/***********************************************************************
 *           InternetSetOptionExA (WININET.@)
 */
BOOL WINAPI InternetSetOptionExA(HINTERNET hInternet, DWORD dwOption,
                           LPVOID lpBuffer, DWORD dwBufferLength, DWORD dwFlags)
{
    FIXME("Flags %08lx ignored\n", dwFlags);
    return InternetSetOptionA( hInternet, dwOption, lpBuffer, dwBufferLength );
}

/***********************************************************************
 *           InternetSetOptionExW (WININET.@)
 */
BOOL WINAPI InternetSetOptionExW(HINTERNET hInternet, DWORD dwOption,
                           LPVOID lpBuffer, DWORD dwBufferLength, DWORD dwFlags)
{
    FIXME("Flags %08lx ignored\n", dwFlags);
    if( dwFlags & ~ISO_VALID_FLAGS )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    return InternetSetOptionW( hInternet, dwOption, lpBuffer, dwBufferLength );
}


/***********************************************************************
 *	InternetCheckConnectionA (WININET.@)
 *
 * Pings a requested host to check internet connection
 *
 * RETURNS
 *   TRUE on success and FALSE on failure. If a failure then
 *   ERROR_NOT_CONNECTED is placesd into GetLastError
 *
 */
BOOL WINAPI InternetCheckConnectionA( LPCSTR lpszUrl, DWORD dwFlags, DWORD dwReserved )
{
/*
 * this is a kludge which runs the resident ping program and reads the output.
 *
 * Anyone have a better idea?
 */

  BOOL   rc = FALSE;
  char command[1024];
  char host[1024];
  int status = -1;

  FIXME("\n");

  /*
   * Crack or set the Address
   */
  if (lpszUrl == NULL)
  {
     /*
      * According to the doc we are supost to use the ip for the next
      * server in the WnInet internal server database. I have
      * no idea what that is or how to get it.
      *
      * So someone needs to implement this.
      */
     FIXME("Unimplemented with URL of NULL\n");
     return TRUE;
  }
  else
  {
     URL_COMPONENTSA componets;

     ZeroMemory(&componets,sizeof(URL_COMPONENTSA));
     componets.lpszHostName = (LPSTR)&host;
     componets.dwHostNameLength = 1024;

     if (!InternetCrackUrlA(lpszUrl,0,0,&componets))
       goto End;

     TRACE("host name : %s\n",componets.lpszHostName);
  }

  /*
   * Build our ping command
   */
  strcpy(command,"ping -w 1 ");
  strcat(command,host);
  strcat(command," >/dev/null 2>/dev/null");

  TRACE("Ping command is : %s\n",command);

  status = system(command);

  TRACE("Ping returned a code of %i \n",status);

  /* Ping return code of 0 indicates success */
  if (status == 0)
     rc = TRUE;

End:

  if (rc == FALSE)
    SetLastError(ERROR_NOT_CONNECTED);

  return rc;
}


/***********************************************************************
 *	InternetCheckConnectionW (WININET.@)
 *
 * Pings a requested host to check internet connection
 *
 * RETURNS
 *   TRUE on success and FALSE on failure. If a failure then
 *   ERROR_NOT_CONNECTED is placed into GetLastError
 *
 */
BOOL WINAPI InternetCheckConnectionW(LPCWSTR lpszUrl, DWORD dwFlags, DWORD dwReserved)
{
    CHAR *szUrl;
    INT len;
    BOOL rc;

    len = WideCharToMultiByte(CP_ACP, 0, lpszUrl, -1, NULL, 0, NULL, NULL);
    if (!(szUrl = (CHAR *)HeapAlloc(GetProcessHeap(), 0, len*sizeof(CHAR))))
        return FALSE;
    WideCharToMultiByte(CP_ACP, 0, lpszUrl, -1, szUrl, len, NULL, NULL);
    rc = InternetCheckConnectionA((LPCSTR)szUrl, dwFlags, dwReserved);
    HeapFree(GetProcessHeap(), 0, szUrl);
    
    return rc;
}


/**********************************************************
 *	INTERNET_InternetOpenUrlA (internal)
 *
 * Opens an URL
 *
 * RETURNS
 *   handle of connection or NULL on failure
 */
HINTERNET WINAPI INTERNET_InternetOpenUrlW(HINTERNET hInternet, LPCWSTR lpszUrl,
    LPCWSTR lpszHeaders, DWORD dwHeadersLength, DWORD dwFlags, DWORD dwContext)
{
    URL_COMPONENTSW urlComponents;
    WCHAR protocol[32], hostName[MAXHOSTNAME], userName[1024];
    WCHAR password[1024], path[2048], extra[1024];
    HINTERNET client = NULL, client1 = NULL;
    
    TRACE("(%p, %s, %s, %08lx, %08lx, %08lx\n", hInternet, debugstr_w(lpszUrl), debugstr_w(lpszHeaders),
	  dwHeadersLength, dwFlags, dwContext);
    
    urlComponents.dwStructSize = sizeof(URL_COMPONENTSW);
    urlComponents.lpszScheme = protocol;
    urlComponents.dwSchemeLength = 32;
    urlComponents.lpszHostName = hostName;
    urlComponents.dwHostNameLength = MAXHOSTNAME;
    urlComponents.lpszUserName = userName;
    urlComponents.dwUserNameLength = 1024;
    urlComponents.lpszPassword = password;
    urlComponents.dwPasswordLength = 1024;
    urlComponents.lpszUrlPath = path;
    urlComponents.dwUrlPathLength = 2048;
    urlComponents.lpszExtraInfo = extra;
    urlComponents.dwExtraInfoLength = 1024;
    if(!InternetCrackUrlW(lpszUrl, strlenW(lpszUrl), 0, &urlComponents))
	return NULL;
    switch(urlComponents.nScheme) {
    case INTERNET_SCHEME_FTP:
	if(urlComponents.nPort == 0)
	    urlComponents.nPort = INTERNET_DEFAULT_FTP_PORT;
	client = InternetConnectW(hInternet, hostName, urlComponents.nPort,
				  userName, password, INTERNET_SERVICE_FTP, dwFlags, dwContext);
	client1 = FtpOpenFileW(client, path, GENERIC_READ, dwFlags, dwContext);
	break;
	
    case INTERNET_SCHEME_HTTP:
    case INTERNET_SCHEME_HTTPS: {
	WCHAR szStars[] = { '*','/','*', 0 };
	LPCWSTR accept[2] = { szStars, NULL };
	if(urlComponents.nPort == 0) {
	    if(urlComponents.nScheme == INTERNET_SCHEME_HTTP)
		urlComponents.nPort = INTERNET_DEFAULT_HTTP_PORT;
	    else
		urlComponents.nPort = INTERNET_DEFAULT_HTTPS_PORT;
	}
	client = InternetConnectW(hInternet, hostName, urlComponents.nPort, userName,
				  password, INTERNET_SERVICE_HTTP, dwFlags, dwContext);
	if(client == NULL)
	    break;
	client1 = HttpOpenRequestW(client, NULL, path, NULL, NULL, accept, dwFlags, dwContext);
	if(client1 == NULL) {
	    InternetCloseHandle(client);
	    break;
	}
	HttpAddRequestHeadersW(client1, lpszHeaders, dwHeadersLength, HTTP_ADDREQ_FLAG_ADD);
	if (!HTTP_HttpSendRequestW(client1, NULL, 0, NULL, 0)) {
	    InternetCloseHandle(client1);
	    InternetCloseHandle(client);
	    client1 = NULL;
	    break;
	}
    }
    case INTERNET_SCHEME_GOPHER:
	/* gopher doesn't seem to be implemented in wine, but it's supposed
	 * to be supported by InternetOpenUrlA. */
    default:
	break;
    }

    TRACE(" %p <--\n", client1);
    
    return client1;
}

/**********************************************************
 *	InternetOpenUrlA (WININET.@)
 *
 * Opens an URL
 *
 * RETURNS
 *   handle of connection or NULL on failure
 */
HINTERNET WINAPI InternetOpenUrlW(HINTERNET hInternet, LPCWSTR lpszUrl,
    LPCWSTR lpszHeaders, DWORD dwHeadersLength, DWORD dwFlags, DWORD dwContext)
{
    HINTERNET ret = NULL;
    LPWININETAPPINFOW hIC = NULL;

    TRACE("(%p, %s, %s, %08lx, %08lx, %08lx\n", hInternet, debugstr_w(lpszUrl), debugstr_w(lpszHeaders),
 	  dwHeadersLength, dwFlags, dwContext);
 
    hIC = (LPWININETAPPINFOW) WININET_GetObject( hInternet );
    if (NULL == hIC ||  hIC->hdr.htype != WH_HINIT) {
	INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
 	goto lend;
    }
    
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC) {
	WORKREQUEST workRequest;
	struct WORKREQ_INTERNETOPENURLW *req;
	
	workRequest.asyncall = INTERNETOPENURLW;
	workRequest.handle = hInternet;
	req = &workRequest.u.InternetOpenUrlW;
	if (lpszUrl)
	    req->lpszUrl = WININET_strdupW(lpszUrl);
	else
	    req->lpszUrl = 0;
	if (lpszHeaders)
	    req->lpszHeaders = WININET_strdupW(lpszHeaders);
	else
	    req->lpszHeaders = 0;
	req->dwHeadersLength = dwHeadersLength;
	req->dwFlags = dwFlags;
	req->dwContext = dwContext;
	
	INTERNET_AsyncCall(&workRequest);
	/*
	 * This is from windows.
	 */
	SetLastError(ERROR_IO_PENDING);
    } else {
	ret = INTERNET_InternetOpenUrlW(hInternet, lpszUrl, lpszHeaders, dwHeadersLength, dwFlags, dwContext);
    }
    
  lend:
    TRACE(" %p <--\n", ret);
    
    return ret;
}

/**********************************************************
 *	InternetOpenUrlW (WININET.@)
 *
 * Opens an URL
 *
 * RETURNS
 *   handle of connection or NULL on failure
 */
HINTERNET WINAPI InternetOpenUrlA(HINTERNET hInternet, LPCSTR lpszUrl,
    LPCSTR lpszHeaders, DWORD dwHeadersLength, DWORD dwFlags, DWORD dwContext)
{
    HINTERNET rc = (HINTERNET)NULL;

    INT lenUrl = MultiByteToWideChar(CP_ACP, 0, lpszUrl, -1, NULL, 0 );
    INT lenHeaders = MultiByteToWideChar(CP_ACP, 0, lpszHeaders, -1, NULL, 0 );
    LPWSTR szUrl = HeapAlloc(GetProcessHeap(), 0, lenUrl*sizeof(WCHAR));
    LPWSTR szHeaders = HeapAlloc(GetProcessHeap(), 0, lenHeaders*sizeof(WCHAR));

    TRACE("\n");

    if (!szUrl || !szHeaders)
    {
        if (szUrl)
            HeapFree(GetProcessHeap(), 0, szUrl);
        if (szHeaders)
            HeapFree(GetProcessHeap(), 0, szHeaders);
        return (HINTERNET)NULL;
    }

    MultiByteToWideChar(CP_ACP, 0, lpszUrl, -1, szUrl, lenUrl);
    MultiByteToWideChar(CP_ACP, 0, lpszHeaders, -1, szHeaders, lenHeaders);

    rc = InternetOpenUrlW(hInternet, szUrl, szHeaders,
        dwHeadersLength, dwFlags, dwContext);

    HeapFree(GetProcessHeap(), 0, szUrl);
    HeapFree(GetProcessHeap(), 0, szHeaders);

    return rc;
}


/***********************************************************************
 *           INTERNET_SetLastError (internal)
 *
 * Set last thread specific error
 *
 * RETURNS
 *
 */
void INTERNET_SetLastError(DWORD dwError)
{
    LPWITHREADERROR lpwite = (LPWITHREADERROR)TlsGetValue(g_dwTlsErrIndex);

    SetLastError(dwError);
    if(lpwite)
        lpwite->dwError = dwError;
}


/***********************************************************************
 *           INTERNET_GetLastError (internal)
 *
 * Get last thread specific error
 *
 * RETURNS
 *
 */
DWORD INTERNET_GetLastError()
{
    LPWITHREADERROR lpwite = (LPWITHREADERROR)TlsGetValue(g_dwTlsErrIndex);
    return lpwite->dwError;
}


/***********************************************************************
 *           INTERNET_WorkerThreadFunc (internal)
 *
 * Worker thread execution function
 *
 * RETURNS
 *
 */
DWORD INTERNET_WorkerThreadFunc(LPVOID *lpvParam)
{
    DWORD dwWaitRes;

    while (1)
    {
	if(dwNumJobs > 0) {
	    INTERNET_ExecuteWork();
	    continue;
	}
        dwWaitRes = WaitForMultipleObjects(2, hEventArray, FALSE, MAX_IDLE_WORKER);

        if (dwWaitRes == WAIT_OBJECT_0 + 1)
            INTERNET_ExecuteWork();
        else
            break;

        InterlockedIncrement(&dwNumIdleThreads);
    }

    InterlockedDecrement(&dwNumIdleThreads);
    InterlockedDecrement(&dwNumThreads);
    TRACE("Worker thread exiting\n");
    return TRUE;
}


/***********************************************************************
 *           INTERNET_InsertWorkRequest (internal)
 *
 * Insert work request into queue
 *
 * RETURNS
 *
 */
BOOL INTERNET_InsertWorkRequest(LPWORKREQUEST lpWorkRequest)
{
    BOOL bSuccess = FALSE;
    LPWORKREQUEST lpNewRequest;

    TRACE("\n");

    lpNewRequest = HeapAlloc(GetProcessHeap(), 0, sizeof(WORKREQUEST));
    if (lpNewRequest)
    {
        memcpy(lpNewRequest, lpWorkRequest, sizeof(WORKREQUEST));
	lpNewRequest->prev = NULL;

        EnterCriticalSection(&csQueue);

        lpNewRequest->next = lpWorkQueueTail;
	if (lpWorkQueueTail)
            lpWorkQueueTail->prev = lpNewRequest;
        lpWorkQueueTail = lpNewRequest;
	if (!lpHeadWorkQueue)
            lpHeadWorkQueue = lpWorkQueueTail;

        LeaveCriticalSection(&csQueue);

	bSuccess = TRUE;
	InterlockedIncrement(&dwNumJobs);
    }

    return bSuccess;
}


/***********************************************************************
 *           INTERNET_GetWorkRequest (internal)
 *
 * Retrieves work request from queue
 *
 * RETURNS
 *
 */
BOOL INTERNET_GetWorkRequest(LPWORKREQUEST lpWorkRequest)
{
    BOOL bSuccess = FALSE;
    LPWORKREQUEST lpRequest = NULL;

    TRACE("\n");

    EnterCriticalSection(&csQueue);

    if (lpHeadWorkQueue)
    {
        lpRequest = lpHeadWorkQueue;
        lpHeadWorkQueue = lpHeadWorkQueue->prev;
	if (lpRequest == lpWorkQueueTail)
            lpWorkQueueTail = lpHeadWorkQueue;
    }

    LeaveCriticalSection(&csQueue);

    if (lpRequest)
    {
        memcpy(lpWorkRequest, lpRequest, sizeof(WORKREQUEST));
        HeapFree(GetProcessHeap(), 0, lpRequest);
	bSuccess = TRUE;
	InterlockedDecrement(&dwNumJobs);
    }

    return bSuccess;
}


/***********************************************************************
 *           INTERNET_AsyncCall (internal)
 *
 * Retrieves work request from queue
 *
 * RETURNS
 *
 */
BOOL INTERNET_AsyncCall(LPWORKREQUEST lpWorkRequest)
{
    HANDLE hThread;
    DWORD dwTID;
    BOOL bSuccess = FALSE;

    TRACE("\n");

    if (InterlockedDecrement(&dwNumIdleThreads) < 0)
    {
        InterlockedIncrement(&dwNumIdleThreads);

	if (InterlockedIncrement(&dwNumThreads) > MAX_WORKER_THREADS ||
	    !(hThread = CreateThread(NULL, 0,
            (LPTHREAD_START_ROUTINE)INTERNET_WorkerThreadFunc, NULL, 0, &dwTID)))
	{
            InterlockedDecrement(&dwNumThreads);
            INTERNET_SetLastError(ERROR_INTERNET_ASYNC_THREAD_FAILED);
	    goto lerror;
	}

	TRACE("Created new thread\n");
    }

    bSuccess = TRUE;
    INTERNET_InsertWorkRequest(lpWorkRequest);
    SetEvent(hWorkEvent);

lerror:

    return bSuccess;
}


/***********************************************************************
 *           INTERNET_ExecuteWork (internal)
 *
 * RETURNS
 *
 */
VOID INTERNET_ExecuteWork()
{
    WORKREQUEST workRequest;

    TRACE("\n");

    if (!INTERNET_GetWorkRequest(&workRequest))
        return;

    if (TRACE_ON(wininet)) {
	static const wininet_flag_info work_request_types[] = {
#define FE(x) { x, #x }
	    FE(FTPPUTFILEA),
	    FE(FTPSETCURRENTDIRECTORYA),
	    FE(FTPCREATEDIRECTORYA),
	    FE(FTPFINDFIRSTFILEA),
	    FE(FTPGETCURRENTDIRECTORYA),
	    FE(FTPOPENFILEA),
	    FE(FTPGETFILEA),
	    FE(FTPDELETEFILEA),
	    FE(FTPREMOVEDIRECTORYA),
	    FE(FTPRENAMEFILEA),
	    FE(INTERNETFINDNEXTA),
	    FE(HTTPSENDREQUESTW),
	    FE(HTTPOPENREQUESTW),
	    FE(SENDCALLBACK),
	    FE(INTERNETOPENURLW)
#undef FE
	};
	int i;
	const char *val = "Unknown";

	for (i = 0; i < (sizeof(work_request_types) / sizeof(work_request_types[0])); i++) {
	    if (work_request_types[i].val == workRequest.asyncall) {
		val = work_request_types[i].name;
		break;
	    }
	}

	TRACE("Got work %d (%s)\n", workRequest.asyncall, val);
    }
    switch (workRequest.asyncall)
    {
    case FTPPUTFILEA:
        {
        struct WORKREQ_FTPPUTFILEA *req = &workRequest.u.FtpPutFileA;

	FTP_FtpPutFileA(workRequest.handle, req->lpszLocalFile,
                   req->lpszNewRemoteFile, req->dwFlags, req->dwContext);

	HeapFree(GetProcessHeap(), 0, req->lpszLocalFile);
	HeapFree(GetProcessHeap(), 0, req->lpszNewRemoteFile);
        }
	break;

    case FTPSETCURRENTDIRECTORYA:
        {
        struct WORKREQ_FTPSETCURRENTDIRECTORYA *req;

        req = &workRequest.u.FtpSetCurrentDirectoryA;
	FTP_FtpSetCurrentDirectoryA(workRequest.handle, req->lpszDirectory);
	HeapFree(GetProcessHeap(), 0, req->lpszDirectory);
        }
	break;

    case FTPCREATEDIRECTORYA:
        {
        struct WORKREQ_FTPCREATEDIRECTORYA *req;

        req = &workRequest.u.FtpCreateDirectoryA;
	FTP_FtpCreateDirectoryA(workRequest.handle, req->lpszDirectory);
	HeapFree(GetProcessHeap(), 0, req->lpszDirectory);
        }
	break;

    case FTPFINDFIRSTFILEA:
        {
        struct WORKREQ_FTPFINDFIRSTFILEA *req;

        req = &workRequest.u.FtpFindFirstFileA;
        FTP_FtpFindFirstFileA(workRequest.handle, req->lpszSearchFile,
           req->lpFindFileData, req->dwFlags, req->dwContext);
	HeapFree(GetProcessHeap(), 0, req->lpszSearchFile);
        }
	break;

    case FTPGETCURRENTDIRECTORYA:
        {
        struct WORKREQ_FTPGETCURRENTDIRECTORYA *req;

        req = &workRequest.u.FtpGetCurrentDirectoryA;
        FTP_FtpGetCurrentDirectoryA(workRequest.handle,
		req->lpszDirectory, req->lpdwDirectory);
        }
	break;

    case FTPOPENFILEA:
        {
        struct WORKREQ_FTPOPENFILEA *req = &workRequest.u.FtpOpenFileA;

        FTP_FtpOpenFileA(workRequest.handle, req->lpszFilename,
            req->dwAccess, req->dwFlags, req->dwContext);
        HeapFree(GetProcessHeap(), 0, req->lpszFilename);
        }
        break;

    case FTPGETFILEA:
        {
        struct WORKREQ_FTPGETFILEA *req = &workRequest.u.FtpGetFileA;

        FTP_FtpGetFileA(workRequest.handle, req->lpszRemoteFile,
                 req->lpszNewFile, req->fFailIfExists,
                 req->dwLocalFlagsAttribute, req->dwFlags, req->dwContext);
	HeapFree(GetProcessHeap(), 0, req->lpszRemoteFile);
	HeapFree(GetProcessHeap(), 0, req->lpszNewFile);
        }
	break;

    case FTPDELETEFILEA:
        {
        struct WORKREQ_FTPDELETEFILEA *req = &workRequest.u.FtpDeleteFileA;

        FTP_FtpDeleteFileA(workRequest.handle, req->lpszFilename);
	HeapFree(GetProcessHeap(), 0, req->lpszFilename);
        }
	break;

    case FTPREMOVEDIRECTORYA:
        {
        struct WORKREQ_FTPREMOVEDIRECTORYA *req;

        req = &workRequest.u.FtpRemoveDirectoryA;
        FTP_FtpRemoveDirectoryA(workRequest.handle, req->lpszDirectory);
	HeapFree(GetProcessHeap(), 0, req->lpszDirectory);
        }
	break;

    case FTPRENAMEFILEA:
        {
        struct WORKREQ_FTPRENAMEFILEA *req = &workRequest.u.FtpRenameFileA;

        FTP_FtpRenameFileA(workRequest.handle, req->lpszSrcFile, req->lpszDestFile);
	HeapFree(GetProcessHeap(), 0, req->lpszSrcFile);
	HeapFree(GetProcessHeap(), 0, req->lpszDestFile);
        }
	break;

    case INTERNETFINDNEXTA:
        {
        struct WORKREQ_INTERNETFINDNEXTA *req;

        req = &workRequest.u.InternetFindNextA;
	INTERNET_FindNextFileA(workRequest.handle, req->lpFindFileData);
        }
	break;

    case HTTPSENDREQUESTW:
        {
        struct WORKREQ_HTTPSENDREQUESTW *req = &workRequest.u.HttpSendRequestW;

        HTTP_HttpSendRequestW(workRequest.handle, req->lpszHeader,
                req->dwHeaderLength, req->lpOptional, req->dwOptionalLength);

        HeapFree(GetProcessHeap(), 0, req->lpszHeader);
        }
        break;

    case HTTPOPENREQUESTW:
        {
        struct WORKREQ_HTTPOPENREQUESTW *req = &workRequest.u.HttpOpenRequestW;

        HTTP_HttpOpenRequestW(workRequest.handle, req->lpszVerb,
            req->lpszObjectName, req->lpszVersion, req->lpszReferrer,
            req->lpszAcceptTypes, req->dwFlags, req->dwContext);

        HeapFree(GetProcessHeap(), 0, req->lpszVerb);
        HeapFree(GetProcessHeap(), 0, req->lpszObjectName);
        HeapFree(GetProcessHeap(), 0, req->lpszVersion);
        HeapFree(GetProcessHeap(), 0, req->lpszReferrer);
        }
        break;

    case SENDCALLBACK:
        {
        struct WORKREQ_SENDCALLBACK *req = &workRequest.u.SendCallback;

        SendAsyncCallbackInt(workRequest.handle, req->hHttpSession,
                req->dwContext, req->dwInternetStatus, req->lpvStatusInfo,
                req->dwStatusInfoLength);
        }
        break;

    case INTERNETOPENURLW:
	{
	struct WORKREQ_INTERNETOPENURLW *req = &workRequest.u.InternetOpenUrlW;
	
	INTERNET_InternetOpenUrlW(workRequest.handle, req->lpszUrl,
				  req->lpszHeaders, req->dwHeadersLength, req->dwFlags, req->dwContext);
	HeapFree(GetProcessHeap(), 0, req->lpszUrl);
	HeapFree(GetProcessHeap(), 0, req->lpszHeaders);
	}
	break;
    }
}


/***********************************************************************
 *          INTERNET_GetResponseBuffer
 *
 * RETURNS
 *
 */
LPSTR INTERNET_GetResponseBuffer()
{
    LPWITHREADERROR lpwite = (LPWITHREADERROR)TlsGetValue(g_dwTlsErrIndex);
    TRACE("\n");
    return lpwite->response;
}

/***********************************************************************
 *           INTERNET_GetNextLine  (internal)
 *
 * Parse next line in directory string listing
 *
 * RETURNS
 *   Pointer to beginning of next line
 *   NULL on failure
 *
 */

LPSTR INTERNET_GetNextLine(INT nSocket, LPSTR lpszBuffer, LPDWORD dwBuffer)
{
    struct timeval tv;
    fd_set infd;
    BOOL bSuccess = FALSE;
    INT nRecv = 0;

    TRACE("\n");

    FD_ZERO(&infd);
    FD_SET(nSocket, &infd);
    tv.tv_sec=RESPONSE_TIMEOUT;
    tv.tv_usec=0;

    while (nRecv < *dwBuffer)
    {
        if (select(nSocket+1,&infd,NULL,NULL,&tv) > 0)
        {
            if (recv(nSocket, &lpszBuffer[nRecv], 1, 0) <= 0)
            {
                INTERNET_SetLastError(ERROR_FTP_TRANSFER_IN_PROGRESS);
                goto lend;
            }

            if (lpszBuffer[nRecv] == '\n')
	    {
		bSuccess = TRUE;
                break;
	    }
            if (lpszBuffer[nRecv] != '\r')
                nRecv++;
        }
	else
	{
            INTERNET_SetLastError(ERROR_INTERNET_TIMEOUT);
            goto lend;
        }
    }

lend:
    if (bSuccess)
    {
        lpszBuffer[nRecv] = '\0';
	*dwBuffer = nRecv - 1;
        TRACE(":%d %s\n", nRecv, lpszBuffer);
        return lpszBuffer;
    }
    else
    {
        return NULL;
    }
}

/***********************************************************************
 *
 */
BOOL WINAPI InternetQueryDataAvailable( HINTERNET hFile,
                                LPDWORD lpdwNumberOfBytesAvailble,
                                DWORD dwFlags, DWORD dwConext)
{
    LPWININETHTTPREQW lpwhr;
    INT retval = -1;
    char buffer[4048];


    lpwhr = (LPWININETHTTPREQW) WININET_GetObject( hFile );
    if (NULL == lpwhr)
    {
        SetLastError(ERROR_NO_MORE_FILES);
        return FALSE;
    }

    TRACE("-->  %p %i\n",lpwhr,lpwhr->hdr.htype);

    switch (lpwhr->hdr.htype)
    {
    case WH_HHTTPREQ:
        if (!NETCON_recv(&lpwhr->netConnection, buffer,
                         4048, MSG_PEEK, (int *)lpdwNumberOfBytesAvailble))
        {
            SetLastError(ERROR_NO_MORE_FILES);
            retval = FALSE;
        }
        else
            retval = TRUE;
        break;

    default:
        FIXME("unsupported file type\n");
        break;
    }

    TRACE("<-- %i\n",retval);
    return (retval+1);
}


/***********************************************************************
 *
 */
BOOL WINAPI InternetLockRequestFile( HINTERNET hInternet, HANDLE
*lphLockReqHandle)
{
    FIXME("STUB\n");
    return FALSE;
}

BOOL WINAPI InternetUnlockRequestFile( HANDLE hLockHandle)
{
    FIXME("STUB\n");
    return FALSE;
}


/***********************************************************************
 *           InternetAutodial
 *
 * On windows this function is supposed to dial the default internet
 * connection. We don't want to have Wine dial out to the internet so
 * we return TRUE by default. It might be nice to check if we are connected.
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL WINAPI InternetAutodial(DWORD dwFlags, HWND hwndParent)
{
    FIXME("STUB\n");

    /* Tell that we are connected to the internet. */
    return TRUE;
}

/***********************************************************************
 *           InternetAutodialHangup
 *
 * Hangs up an connection made with InternetAutodial
 *
 * PARAM
 *    dwReserved
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL WINAPI InternetAutodialHangup(DWORD dwReserved)
{
    FIXME("STUB\n");

    /* we didn't dial, we don't disconnect */
    return TRUE;
}

/***********************************************************************
 *
 *         InternetCombineUrlA
 *
 * Combine a base URL with a relative URL
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */

BOOL WINAPI InternetCombineUrlA(LPCSTR lpszBaseUrl, LPCSTR lpszRelativeUrl,
                                LPSTR lpszBuffer, LPDWORD lpdwBufferLength,
                                DWORD dwFlags)
{
    HRESULT hr=S_OK;

    TRACE("(%s, %s, %p, %p, 0x%08lx)\n", debugstr_a(lpszBaseUrl), debugstr_a(lpszRelativeUrl), lpszBuffer, lpdwBufferLength, dwFlags);

    /* Flip this bit to correspond to URL_ESCAPE_UNSAFE */
    dwFlags ^= ICU_NO_ENCODE;
    hr=UrlCombineA(lpszBaseUrl,lpszRelativeUrl,lpszBuffer,lpdwBufferLength,dwFlags);

    return (hr==S_OK);
}

/***********************************************************************
 *
 *         InternetCombineUrlW
 *
 * Combine a base URL with a relative URL
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */

BOOL WINAPI InternetCombineUrlW(LPCWSTR lpszBaseUrl, LPCWSTR lpszRelativeUrl,
                                LPWSTR lpszBuffer, LPDWORD lpdwBufferLength,
                                DWORD dwFlags)
{
    HRESULT hr=S_OK;

    TRACE("(%s, %s, %p, %p, 0x%08lx)\n", debugstr_w(lpszBaseUrl), debugstr_w(lpszRelativeUrl), lpszBuffer, lpdwBufferLength, dwFlags);

    /* Flip this bit to correspond to URL_ESCAPE_UNSAFE */
    dwFlags ^= ICU_NO_ENCODE;
    hr=UrlCombineW(lpszBaseUrl,lpszRelativeUrl,lpszBuffer,lpdwBufferLength,dwFlags);

    return (hr==S_OK);
}

/***********************************************************************
 *
 *         InternetCreateUrlA
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL WINAPI InternetCreateUrlA(LPURL_COMPONENTSA lpUrlComponents, DWORD dwFlags,
                               LPSTR lpszUrl, LPDWORD lpdwUrlLength)
{
    FIXME("\n");
    return FALSE;
}

/***********************************************************************
 *
 *         InternetCreateUrlW
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL WINAPI InternetCreateUrlW(LPURL_COMPONENTSW lpUrlComponents, DWORD dwFlags,
                               LPWSTR lpszUrl, LPDWORD lpdwUrlLength)
{
    FIXME("\n");
    return FALSE;
}
