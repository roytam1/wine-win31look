/*
 * TTYDRV USER driver functions
 *
 * Copyright 1998 Patrik Stridvall
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "dinput.h"
#include "ttydrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ttydrv);

struct tagCURSORICONINFO;

/***********************************************************************
 *		VkKeyScanEx (TTYDRV.@)
 */
SHORT TTYDRV_VkKeyScanEx(WCHAR cChar, HKL hkl)
{
  return 0;
}

/***********************************************************************
 *		MapVirtualKeyEx (TTYDRV.@)
 */
UINT TTYDRV_MapVirtualKeyEx(UINT wCode, UINT wMapType, HKL hkl)
{
  return 0;
}

/***********************************************************************
 *		GetKeyNameText (TTYDRV.@)
 */
INT TTYDRV_GetKeyNameText( LONG lParam, LPWSTR lpBuffer, INT nSize )
{
  if(lpBuffer && nSize)
    {
      *lpBuffer = 0;
    }
  return 0;
}

/***********************************************************************
 *		ToUnicodeEx (TTYDRV.@)
 */
INT TTYDRV_ToUnicodeEx( UINT virtKey, UINT scanCode, LPBYTE lpKeyState,
		      LPWSTR pwszBuff, int cchBuff, UINT flags, HKL hkl )
{
  return 0;
}

/***********************************************************************
 *		Beep (TTYDRV.@)
 */
void TTYDRV_Beep(void)
{
}

/***********************************************************************
 *		SetCursor (TTYDRV.@)
 */
void TTYDRV_SetCursor( struct tagCURSORICONINFO *lpCursor )
{
}

/***********************************************************************
 *              GetScreenSaveActive (TTYDRV.@)
 *
 * Returns the active status of the screen saver
 */
BOOL TTYDRV_GetScreenSaveActive(void)
{
    return FALSE;
}

/***********************************************************************
 *              SetScreenSaveActive (TTYDRV.@)
 *
 * Activate/Deactivate the screen saver
 */
void TTYDRV_SetScreenSaveActive(BOOL bActivate)
{
    FIXME("(%d): stub\n", bActivate);
}

/***********************************************************************
 *		AcquireClipboard (TTYDRV.@)
 */
void TTYDRV_AcquireClipboard(void)
{
}

/***********************************************************************
 *		ReleaseClipboard (TTYDRV.@)
 */
void TTYDRV_ReleaseClipboard(void)
{
}

/***********************************************************************
 *		SetClipboardData (TTYDRV.@)
 */
void TTYDRV_SetClipboardData(UINT wFormat)
{
}

/***********************************************************************
 *		GetClipboardData (TTYDRV.@)
 */
BOOL TTYDRV_GetClipboardData(UINT wFormat)
{
  return FALSE;
}

/***********************************************************************
 *		IsClipboardFormatAvailable (TTYDRV.@)
 */
BOOL TTYDRV_IsClipboardFormatAvailable(UINT wFormat)
{
  return FALSE;
}

/**************************************************************************
 *		RegisterClipboardFormat (TTYDRV.@)
 *
 * Registers a custom clipboard format
 * Returns: TRUE - new format registered, FALSE - Format already registered
 */
BOOL TTYDRV_RegisterClipboardFormat( LPCSTR FormatName )
{
  return TRUE;
}

/**************************************************************************
 *		IsSelectionOwner (TTYDRV.@)
 *
 * Returns: TRUE - We(WINE) own the selection, FALSE - Selection not owned by us
 */
BOOL TTYDRV_IsSelectionOwner(void)
{
    return FALSE;
}
