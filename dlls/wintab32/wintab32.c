/*
 * WinTab32 library
 *
 * Copyright 2003 CodeWeavers, Aric Stewart
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
#include "wintab.h"
#include "wintab_internal.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wintab32);

HWND hwndDefault = NULL;
static const WCHAR
  WC_TABLETCLASSNAME[] = {'W','i','n','e','T','a','b','l','e','t','C','l','a','s','s',0};
CRITICAL_SECTION csTablet;

int (*pLoadTabletInfo)(HWND hwnddefault) = NULL;
int (*pGetCurrentPacket)(LPWTPACKET packet) = NULL;
int (*pAttachEventQueueToTablet)(HWND hOwner) = NULL;
UINT (*pWTInfoA)(UINT wCategory, UINT nIndex, LPVOID lpOutput) = NULL;

static LRESULT WINAPI TABLET_WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                          LPARAM lParam);

static VOID TABLET_Register()
{
    WNDCLASSW wndClass;
    ZeroMemory(&wndClass, sizeof(WNDCLASSW));
    wndClass.style = CS_GLOBALCLASS;
    wndClass.lpfnWndProc = TABLET_WindowProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hCursor = (HCURSOR)NULL;
    wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW +1);
    wndClass.lpszClassName = WC_TABLETCLASSNAME;
    RegisterClassW(&wndClass);
}

static VOID TABLET_Unregister()
{
    UnregisterClassW(WC_TABLETCLASSNAME, (HINSTANCE)NULL);
}

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    static const WCHAR name[] = {'T','a','b','l','e','t',0};
    HMODULE hx11drv;

    TRACE("%p, %lx, %p\n",hInstDLL,fdwReason,lpReserved);
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            TRACE("Initialization\n");
            InitializeCriticalSection(&csTablet);
            hx11drv = GetModuleHandleA("x11drv.dll");
            if (hx11drv)
            {
                pLoadTabletInfo = (void *)GetProcAddress(hx11drv, "LoadTabletInfo");
                pAttachEventQueueToTablet = (void *)GetProcAddress(hx11drv, "AttachEventQueueToTablet");
                pGetCurrentPacket = (void *)GetProcAddress(hx11drv, "GetCurrentPacket");
                pWTInfoA = (void *)GetProcAddress(hx11drv, "WTInfoA");
                TABLET_Register();
                hwndDefault = CreateWindowW(WC_TABLETCLASSNAME, name,
                                WS_POPUPWINDOW,0,0,0,0,0,0,hInstDLL,0);
            }
            else
                return FALSE;
            break;
        case DLL_PROCESS_DETACH:
            TRACE("Detaching\n");
            if (hwndDefault)
            {
                DestroyWindow(hwndDefault);
                hwndDefault = 0;
            }
            TABLET_Unregister();
            DeleteCriticalSection(&csTablet);
            break;
    }
    return TRUE;
}


/*
 * The window proc for the default TABLET window
 */
static LRESULT WINAPI TABLET_WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                          LPARAM lParam)
{
    TRACE("Incoming Message 0x%x  (0x%08x, 0x%08x)\n", uMsg, (UINT)wParam,
           (UINT)lParam);

    switch(uMsg)
    {
        case WM_NCCREATE:
            return TRUE;

        case WT_PACKET:
            {
                WTPACKET packet;
                LPOPENCONTEXT handler;
                pGetCurrentPacket(&packet);
                handler = AddPacketToContextQueue(&packet,(HWND)lParam);
                if (handler)
                    TABLET_PostTabletMessage(handler, WT_PACKET,
                                (WPARAM)packet.pkSerialNumber,
                                (LPARAM)handler->handle, FALSE);
                break;
            }
        case WT_PROXIMITY:
            {
                LPOPENCONTEXT handler;
                LPARAM prox;
                handler = FindOpenContext((HWND)lParam);
                if (handler)
                {
                    prox = MAKELPARAM( wParam, 1 );
                    TABLET_PostTabletMessage(handler, WT_PROXIMITY,
                                        (WPARAM)handler->handle, (LPARAM)prox,
                                        TRUE);
                }
                break;
            }
    }
    return 0;
}
