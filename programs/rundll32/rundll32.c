/*
 * PURPOSE: Load a DLL and run an entry point with the specified parameters
 *
 * Copyright 2002 Alberto Massari
 * Copyright 2001-2003 Aric Stewart for CodeWeavers
 * Copyright 2003 Mike McCormack for CodeWeavers
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
 *
 */

/*
 *
 *  rundll32 dllname,entrypoint [arguments]
 *
 *  Documentation for this utility found on KB Q164787
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Exclude rarely-used stuff from Windows headers */
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "wine/winbase16.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(rundll32);


/*
 * Control_RunDLL has these parameters
 */
typedef void (WINAPI *EntryPointW)(HWND hWnd, HINSTANCE hInst, LPWSTR lpszCmdLine, int nCmdShow);
typedef void (WINAPI *EntryPointA)(HWND hWnd, HINSTANCE hInst, LPSTR lpszCmdLine, int nCmdShow);

/*
 * Control_RunDLL needs to have a window. So lets make us a very
 * simple window class.
 */
static TCHAR  *szTitle = "rundll32";
static TCHAR  *szWindowClass = "class_rundll32";

static HINSTANCE16 (WINAPI *pLoadLibrary16)(LPCSTR libname);
static FARPROC16 (WINAPI *pGetProcAddress16)(HMODULE16 hModule, LPCSTR name);
static void (WINAPI *pRunDLL_CallEntry16)( FARPROC proc, HWND hwnd, HINSTANCE inst,
                                           LPCSTR cmdline, INT cmdshow );

static ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = (WNDPROC)DefWindowProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = NULL;

    return RegisterClassEx(&wcex);
}

static HINSTANCE16 load_dll16( LPCWSTR dll )
{
    HINSTANCE16 ret = 0;
    DWORD len = WideCharToMultiByte( CP_ACP, 0, dll, -1, NULL, 0, NULL, NULL );
    char *dllA = HeapAlloc( GetProcessHeap(), 0, len );
    WideCharToMultiByte( CP_ACP, 0, dll, -1, dllA, len, NULL, NULL );
    pLoadLibrary16 = (void *)GetProcAddress( GetModuleHandleA("kernel32.dll"), "LoadLibrary16" );
    if (pLoadLibrary16) ret = pLoadLibrary16( dllA );
    HeapFree( GetProcessHeap(), 0, dllA );
    return ret;
}

static FARPROC16 get_entry_point16( HINSTANCE16 inst, LPCWSTR entry )
{
    FARPROC16 ret = 0;
    DWORD len = WideCharToMultiByte( CP_ACP, 0, entry, -1, NULL, 0, NULL, NULL );
    char *entryA = HeapAlloc( GetProcessHeap(), 0, len );
    WideCharToMultiByte( CP_ACP, 0, entry, -1, entryA, len, NULL, NULL );
    pGetProcAddress16 = (void *)GetProcAddress( GetModuleHandleA("kernel32.dll"), "GetProcAddress16" );
    if (pGetProcAddress16) ret = pGetProcAddress16( inst, entryA );
    HeapFree( GetProcessHeap(), 0, entryA );
    return ret;
}

static void *get_entry_point32( HMODULE module, LPCWSTR entry, BOOL *unicode )
{
    void *ret;
    DWORD len = WideCharToMultiByte( CP_ACP, 0, entry, -1, NULL, 0, NULL, NULL );
    char *entryA = HeapAlloc( GetProcessHeap(), 0, len + 1 );
    WideCharToMultiByte( CP_ACP, 0, entry, -1, entryA, len, NULL, NULL );

    /* first try the W version */
    *unicode = TRUE;
    strcat( entryA, "W" );
    if (!(ret = GetProcAddress( module, entryA )))
    {
        /* now the A version */
        *unicode = FALSE;
        entryA[strlen(entryA)-1] = 'A';
        if (!(ret = GetProcAddress( module, entryA )))
        {
            /* now the version without suffix */
            entryA[strlen(entryA)-1] = 0;
            ret = GetProcAddress( module, entryA );
        }
    }
    HeapFree( GetProcessHeap(), 0, entryA );
    return ret;
}

static LPWSTR GetNextArg(LPWSTR *cmdline)
{
    LPWSTR s;
    LPWSTR arg,d;
    int in_quotes,bcount,len=0;

    /* count the chars */
    bcount=0;
    in_quotes=0;
    s=*cmdline;
    while (1) {
        if (*s==0 || ((*s=='\t' || *s==' ') && !in_quotes)) {
            /* end of this command line argument */
            break;
        } else if (*s=='\\') {
            /* '\', count them */
            bcount++;
        } else if ((*s=='"') && ((bcount & 1)==0)) {
            /* unescaped '"' */
            in_quotes=!in_quotes;
            bcount=0;
        } else {
            /* a regular character */
            bcount=0;
        }
        s++;
        len++;
    }
    arg=HeapAlloc(GetProcessHeap(), 0, (len+1)*sizeof(WCHAR));
    if (!arg)
        return NULL;

    bcount=0;
    in_quotes=0;
    d=arg;
    s=*cmdline;
    while (*s) {
        if ((*s=='\t' || *s==' ') && !in_quotes) {
            /* end of this command line argument */
            break;
        } else if (*s=='\\') {
            /* '\\' */
            *d++=*s++;
            bcount++;
        } else if (*s=='"') {
            /* '"' */
            if ((bcount & 1)==0) {
                /* Preceeded by an even number of '\', this is half that
                 * number of '\', plus a quote which we erase.
                 */
                d-=bcount/2;
                in_quotes=!in_quotes;
                s++;
            } else {
                /* Preceeded by an odd number of '\', this is half that
                 * number of '\' followed by a '"'
                 */
                d=d-bcount/2-1;
                *d++='"';
                s++;
            }
            bcount=0;
        } else {
            /* a regular character */
            *d++=*s++;
            bcount=0;
        }
    }
    *d=0;
    *cmdline=s;

    /* skip the remaining spaces */
    while (**cmdline=='\t' || **cmdline==' ') {
        (*cmdline)++;
    }

    return arg;
}

int main(int argc, char* argv[])
{
    HWND hWnd;
    LPWSTR szCmdLine;
    LPWSTR szDllName,szEntryPoint;
    void *entry_point;
    BOOL unicode, win16;
    STARTUPINFOW info;
    HMODULE hDll, instance;

    hWnd=NULL;
    hDll=NULL;
    szDllName=NULL;

    /* Initialize the rundll32 class */
    MyRegisterClass( NULL );
    hWnd = CreateWindow(szWindowClass, szTitle,
          WS_OVERLAPPEDWINDOW|WS_VISIBLE,
          CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, NULL, NULL);

    /* Skip the rundll32.exe path */
    szCmdLine=GetCommandLineW();
    WINE_TRACE("CmdLine=%s\n",wine_dbgstr_w(szCmdLine));
    szDllName=GetNextArg(&szCmdLine);
    if (!szDllName || *szDllName==0)
        goto CLEANUP;
    HeapFree(GetProcessHeap(),0,szDllName);

    /* Get the dll name and API EntryPoint */
    szDllName=GetNextArg(&szCmdLine);
    if (!szDllName || *szDllName==0)
        goto CLEANUP;
    WINE_TRACE("DllName=%s\n",wine_dbgstr_w(szDllName));
    if (!(szEntryPoint = strchrW( szDllName, ',' ))) goto CLEANUP;
    *szEntryPoint++=0;
    WINE_TRACE("EntryPoint=%s\n",wine_dbgstr_w(szEntryPoint));

    /* Load the library */
    hDll=LoadLibraryW(szDllName);
    if (hDll)
    {
        win16 = FALSE;
        entry_point = get_entry_point32( hDll, szEntryPoint, &unicode );
    }
    else
    {
        HINSTANCE16 dll = load_dll16( szDllName );
        if (dll <= 32)
        {
            /* Windows has a MessageBox here... */
            WINE_ERR("Unable to load %s\n",wine_dbgstr_w(szDllName));
            goto CLEANUP;
        }
        win16 = TRUE;
        unicode = FALSE;
        entry_point = get_entry_point16( dll, szEntryPoint );
    }

    if (!entry_point)
    {
        /* Windows has a MessageBox here... */
        WINE_ERR( "Unable to find the entry point %s in %s\n",
                  wine_dbgstr_w(szEntryPoint), wine_dbgstr_w(szDllName) );
        goto CLEANUP;
    }

    GetStartupInfoW( &info );
    if (!(info.dwFlags & STARTF_USESHOWWINDOW)) info.wShowWindow = SW_SHOWDEFAULT;
    instance = GetModuleHandleW(NULL);  /* Windows always uses that, not hDll */

    if (unicode)
    {
        EntryPointW pEntryPointW = entry_point;

        WINE_TRACE( "Calling %s (%p,%p,%s,%d)\n", wine_dbgstr_w(szEntryPoint),
                    hWnd, instance, wine_dbgstr_w(szCmdLine), info.wShowWindow );

        pEntryPointW( hWnd, instance, szCmdLine, info.wShowWindow );
    }
    else
    {
        DWORD len = WideCharToMultiByte( CP_ACP, 0, szCmdLine, -1, NULL, 0, NULL, NULL );
        char *cmdline = HeapAlloc( GetProcessHeap(), 0, len );
        WideCharToMultiByte( CP_ACP, 0, szCmdLine, -1, cmdline, len, NULL, NULL );

        WINE_TRACE( "Calling %s (%p,%p,%s,%d)\n", wine_dbgstr_w(szEntryPoint),
                    hWnd, instance, wine_dbgstr_a(cmdline), info.wShowWindow );

        if (win16)
        {
            HMODULE shell = LoadLibraryA( "shell32.dll" );
            if (shell) pRunDLL_CallEntry16 = (void *)GetProcAddress( shell, (LPCSTR)122 );
            if (pRunDLL_CallEntry16)
                pRunDLL_CallEntry16( entry_point, hWnd, instance, cmdline, info.wShowWindow );
        }
        else
        {
            EntryPointA pEntryPointA = entry_point;
            pEntryPointA( hWnd, instance, cmdline, info.wShowWindow );
        }
        HeapFree( GetProcessHeap(), 0, cmdline );
    }

CLEANUP:
    if (hWnd)
        DestroyWindow(hWnd);
    if (hDll)
        FreeLibrary(hDll);
    if (szDllName)
        HeapFree(GetProcessHeap(),0,szDllName);
    return 0; /* rundll32 always returns 0! */
}
