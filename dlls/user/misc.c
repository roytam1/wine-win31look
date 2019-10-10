/*
 * Misc USER functions
 *
 * Copyright 1995 Thomas Sandford
 * Copyright 1997 Marcus Meissner
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
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "winnls.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(win);

/* callback to allow EnumDesktopsA to use EnumDesktopsW */
typedef struct {
    DESKTOPENUMPROCA lpEnumFunc;
    LPARAM lParam;
} ENUMDESKTOPS_LPARAM;

/* EnumDesktopsA passes this callback function to EnumDesktopsW.
 * It simply converts the string to ASCII and calls the callback
 * function provided by the original caller
 */
static BOOL CALLBACK EnumDesktopProcWtoA(LPWSTR lpszDesktop, LPARAM lParam)
{
    LPSTR buffer;
    INT   len;
    BOOL  ret;
    ENUMDESKTOPS_LPARAM *data = (ENUMDESKTOPS_LPARAM *)lParam;

    len = WideCharToMultiByte(CP_ACP, 0, lpszDesktop, -1, NULL, 0, NULL, NULL);
    if (!(buffer = HeapAlloc( GetProcessHeap(), 0, len))) return FALSE;
    WideCharToMultiByte(CP_ACP, 0, lpszDesktop, -1, buffer, len, NULL, NULL);

    ret = data->lpEnumFunc(buffer, data->lParam);

    HeapFree(GetProcessHeap(), 0, buffer);
    return ret;
}

/**********************************************************************
 * SetLastErrorEx [USER32.@]  Sets the last-error code.
 *
 * RETURNS
 *    None.
 */
void WINAPI SetLastErrorEx(
    DWORD error, /* [in] Per-thread error code */
    DWORD type)  /* [in] Error type */
{
    TRACE("(0x%08lx, 0x%08lx)\n", error,type);
    switch(type) {
        case 0:
            break;
        case SLE_ERROR:
        case SLE_MINORERROR:
        case SLE_WARNING:
            /* Fall through for now */
        default:
            FIXME("(error=%08lx, type=%08lx): Unhandled type\n", error,type);
            break;
    }
    SetLastError( error );
}


/******************************************************************************
 * GetProcessWindowStation [USER32.@]  Returns handle of window station
 *
 * NOTES
 *    Docs say the return value is HWINSTA
 *
 * RETURNS
 *    Success: Handle to window station associated with calling process
 *    Failure: NULL
 */
HWINSTA WINAPI GetProcessWindowStation(void)
{
    FIXME("(void): stub\n");
    return (HWINSTA)1;
}


/******************************************************************************
 * GetThreadDesktop [USER32.@]  Returns handle to desktop
 *
 * NOTES
 *    Docs say the return value is HDESK
 *
 * PARAMS
 *    dwThreadId [I] Thread identifier
 *
 * RETURNS
 *    Success: Handle to desktop associated with specified thread
 *    Failure: NULL
 */
DWORD WINAPI GetThreadDesktop( DWORD dwThreadId )
{
    FIXME("(%lx): stub\n",dwThreadId);
    return 1;
}


/******************************************************************************
 * SetDebugErrorLevel [USER32.@]
 * Sets the minimum error level for generating debugging events
 *
 * PARAMS
 *    dwLevel [I] Debugging error level
 */
VOID WINAPI SetDebugErrorLevel( DWORD dwLevel )
{
    FIXME("(%ld): stub\n", dwLevel);
}


/******************************************************************************
 *                    GetProcessDefaultLayout [USER32.@]
 *
 * Gets the default layout for parentless windows.
 * Right now, just returns 0 (left-to-right).
 *
 * RETURNS
 *    Success: Nonzero
 *    Failure: Zero
 *
 * BUGS
 *    No RTL
 */
BOOL WINAPI GetProcessDefaultLayout( DWORD *pdwDefaultLayout )
{
    if ( !pdwDefaultLayout ) {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
     }
    FIXME( "( %p ): No BiDi\n", pdwDefaultLayout );
    *pdwDefaultLayout = 0;
    return TRUE;
}


/******************************************************************************
 *                    SetProcessDefaultLayout [USER32.@]
 *
 * Sets the default layout for parentless windows.
 * Right now, only accepts 0 (left-to-right).
 *
 * RETURNS
 *    Success: Nonzero
 *    Failure: Zero
 *
 * BUGS
 *    No RTL
 */
BOOL WINAPI SetProcessDefaultLayout( DWORD dwDefaultLayout )
{
    if ( dwDefaultLayout == 0 )
        return TRUE;
    FIXME( "( %08lx ): No BiDi\n", dwDefaultLayout );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}


/******************************************************************************
 * OpenDesktopA [USER32.@]
 *
 * NOTES
 *    Return type should be HDESK
 *
 *    Not supported on Win9x - returns NULL and calls SetLastError.
 */
HANDLE WINAPI OpenDesktopA( LPCSTR lpszDesktop, DWORD dwFlags,
                                BOOL fInherit, DWORD dwDesiredAccess )
{
    FIXME("(%s,%lx,%i,%lx): stub\n",debugstr_a(lpszDesktop),dwFlags,
          fInherit,dwDesiredAccess);

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

/******************************************************************************
 *              EnumDesktopsA [USER32.@]
 */
BOOL WINAPI EnumDesktopsA( HWINSTA hwinsta, DESKTOPENUMPROCA lpEnumFunc,
                    LPARAM lParam )
{
    ENUMDESKTOPS_LPARAM caller_data;

    caller_data.lpEnumFunc = lpEnumFunc;
    caller_data.lParam     = lParam;
    
    return EnumDesktopsW(hwinsta, EnumDesktopProcWtoA, (LPARAM) &caller_data);
}

/******************************************************************************
 *              EnumDesktopsW [USER32.@]
 */
BOOL WINAPI EnumDesktopsW( HWINSTA hwinsta, DESKTOPENUMPROCW lpEnumFunc,
                    LPARAM lParam )
{
    FIXME("%p,%p,%lx): stub\n",hwinsta,lpEnumFunc,lParam);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

/******************************************************************************
 *		SetUserObjectInformationA   (USER32.@)
 */
BOOL WINAPI SetUserObjectInformationA( HANDLE hObj, INT nIndex,
				       LPVOID pvInfo, DWORD nLength )
{
    FIXME("(%p,%d,%p,%lx): stub\n",hObj,nIndex,pvInfo,nLength);
    return TRUE;
}

/******************************************************************************
 *		SetThreadDesktop   (USER32.@)
 */
BOOL WINAPI SetThreadDesktop( HANDLE hDesktop )
{
    FIXME("(%p): stub\n",hDesktop);
    return TRUE;
}


/***********************************************************************
 *           RegisterShellHookWindow			[USER32.@]
 */
BOOL WINAPI RegisterShellHookWindow ( HWND hWnd )
{
    FIXME("(%p): stub\n", hWnd);
    return 0;
}


/***********************************************************************
 *           DeregisterShellHookWindow			[USER32.@]
 */
HRESULT WINAPI DeregisterShellHookWindow ( DWORD u )
{
    FIXME("0x%08lx stub\n",u);
    return 0;

}


/***********************************************************************
 *           RegisterTasklist   			[USER32.@]
 */
DWORD WINAPI RegisterTasklist (DWORD x)
{
    FIXME("0x%08lx\n",x);
    return TRUE;
}


/***********************************************************************
 *           GetAppCompatFlags   (USER32.@)
 */
DWORD WINAPI GetAppCompatFlags( HTASK hTask )
{
    FIXME("stub\n");
    return 0;
}


/***********************************************************************
 *           AlignRects   (USER32.@)
 */
BOOL WINAPI AlignRects(LPRECT rect, DWORD b, DWORD c, DWORD d)
{
    FIXME("(%p, %ld, %ld, %ld): stub\n", rect, b, c, d);
    if (rect)
        FIXME("rect: [[%ld, %ld], [%ld, %ld]]\n", rect->left, rect->top, rect->right, rect->bottom);
    /* Calls OffsetRect */
    return FALSE;
}


/***********************************************************************
 *		USER_489 (USER.489)
 */
LONG WINAPI stub_USER_489(void) { FIXME("stub\n"); return 0; }

/***********************************************************************
 *		USER_490 (USER.490)
 */
LONG WINAPI stub_USER_490(void) { FIXME("stub\n"); return 0; }

/***********************************************************************
 *		USER_492 (USER.492)
 */
LONG WINAPI stub_USER_492(void) { FIXME("stub\n"); return 0; }

/***********************************************************************
 *		USER_496 (USER.496)
 */
LONG WINAPI stub_USER_496(void) { FIXME("stub\n"); return 0; }

/***********************************************************************
 *		User32InitializeImmEntryTable
 */
BOOL WINAPI User32InitializeImmEntryTable(LPVOID ptr) { 
  FIXME("(%p): stub\n", ptr); 
  return TRUE; 
}
