/*
 * W32SKRNL
 * DLL for Win32s
 *
 * Copyright (c) 1997 Andreas Mohr
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
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wine/windef16.h"
#include "thread.h"

/***********************************************************************
 *		_kGetWin32sDirectory@0 (W32SKRNL.14)
 */
LPSTR WINAPI GetWin32sDirectory(void)
{
    static char sysdir[0x80];
    LPSTR text;

    GetEnvironmentVariableA("winsysdir", sysdir, 0x80);
    if (!sysdir) return NULL;
    strcat(sysdir, "\\WIN32S");
    text = HeapAlloc(GetProcessHeap(), 0, strlen(sysdir)+1);
    strcpy(text, sysdir);
    return text;
}

/***********************************************************************
 *		_GetThunkBuff
 * FIXME: ???
 */
SEGPTR WINAPI _GetThunkBuff(void)
{
	return (SEGPTR)NULL;
}


/***********************************************************************
 *           GetCurrentTask32   (W32SKRNL.3)
 */
HTASK16 WINAPI GetCurrentTask32(void)
{
    return NtCurrentTeb()->htask16;
}

