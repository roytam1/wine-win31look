/*
 * *DeferWindowPos() structure and definitions
 *
 * Copyright 1994 Alexandre Julliard
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

#ifndef __WINE_WINPOS_H
#define __WINE_WINPOS_H

#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <wingdi.h>
#include <winuser.h>

/* undocumented SWP flags - from SDK 3.1 */
#define SWP_NOCLIENTSIZE	0x0800
#define SWP_NOCLIENTMOVE	0x1000

/* Wine extra SWP flag */
#define SWP_WINE_NOHOSTMOVE	0x80000000

struct tagWINDOWPOS16;

extern BOOL WINPOS_RedrawIconTitle( HWND hWnd );
extern BOOL WINPOS_ShowIconTitle( HWND hwnd, BOOL bShow );
extern void WINPOS_GetMinMaxInfo( HWND hwnd, POINT *maxSize, POINT *maxPos, POINT *minTrack,
                                  POINT *maxTrack );
extern LONG WINPOS_HandleWindowPosChanging16(HWND hwnd, struct tagWINDOWPOS16 *winpos);
extern LONG WINPOS_HandleWindowPosChanging(HWND hwnd, WINDOWPOS *winpos);
extern HWND WINPOS_WindowFromPoint( HWND hwndScope, POINT pt, INT *hittest );
extern void WINPOS_CheckInternalPos( HWND hwnd );
extern void WINPOS_ActivateOtherWindow( HWND hwnd );
extern BOOL WINPOS_CreateInternalPosAtom(void);

#endif  /* __WINE_WINPOS_H */
