/*
 * Stopwatch Functions
 *
 * Copyright 2004 Jon Griffiths
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
 * NOTES
 * These functions probably never need to be implemented unless we
 * A) Rewrite explorer from scratch, and
 * B) Want to use a substandard API to tune its performance.
 */

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "wine/unicode.h"
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winreg.h"
#define NO_SHLWAPI_STREAM
#include "shlwapi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

/*************************************************************************
 *      @	[SHLWAPI.241]
 *
 * Get the current performance monitoring mode.
 *
 * PARAMS
 *  None.
 *
 * RETURNS
 *  The current performance monitoring mode. This is zero if monitoring
 *  is disabled (the default).
 *
 * NOTES
 *  If this function returns 0, no further StopWatch functions should be called.
 */
DWORD WINAPI StopWatchMode()
{
  FIXME("() stub!\n");
  return 0;
}

/*************************************************************************
 *      @	[SHLWAPI.242]
 *
 * Write captured performance nodes to a log file.
 *
 * PARAMS
 *  None.
 *
 * RETURNS
 *  Nothing.
 */
void WINAPI StopWatchFlush()
{
  FIXME("() stub!\n");
}


/*************************************************************************
 *      @	[SHLWAPI.243]
 *
 * Write a performance event to a log file
 *
 * PARAMS
 *  dwClass     [I] Class of event
 *  lpszStr     [I] Text of event to log
 *  dwUnknown   [I] Unknown
 *  dwMode      [I] Mode flags
 *  dwTimeStamp [I] Timestamp
 *
 * RETURNS
 *  Success: ERROR_SUCCESS.
 *  Failure: A standard Win32 error code indicating the failure.
 */
DWORD WINAPI StopWatchA(DWORD dwClass, LPCSTR lpszStr, DWORD dwUnknown,
                        DWORD dwMode, DWORD dwTimeStamp)
{
  FIXME("(%ld,%s,%ld,%ld,%ld) stub!\n", dwClass, debugstr_a(lpszStr),
        dwUnknown, dwMode, dwTimeStamp);
  return ERROR_SUCCESS;
}

/*************************************************************************
 *      @	[SHLWAPI.244]
 *
 * See StopWatchA.
 */
DWORD WINAPI StopWatchW(DWORD dwClass, LPCWSTR lpszStr, DWORD dwUnknown,
                        DWORD dwMode, DWORD dwTimeStamp)
{
  char szBuff[MAX_PATH];

  if(!WideCharToMultiByte(0, 0, lpszStr, -1, szBuff, MAX_PATH, 0, 0))
    return ERROR_NOT_ENOUGH_MEMORY;

  return StopWatchA(dwClass, szBuff, dwUnknown, dwMode, dwTimeStamp);
}

/*************************************************************************
 *      @	[SHLWAPI.245]
 *
 * Log a shell frame event.
 *
 * PARAMS
 *  hWnd       [I] Window having the event
 *  pvUnknown1 [I] Unknown
 *  bUnknown2  [I] Unknown
 *  pClassWnd  [I] Window of class to log
 *
 * RETURNS
 *  Nothing.
 */
void WINAPI StopWatch_TimerHandler(HWND hWnd, PVOID pvUnknown1, BOOL bUnknown2, HWND *pClassWnd)
{
  FIXME("(%p,%p,%d,%p) stub!\n", hWnd, pvUnknown1, bUnknown2 ,pClassWnd);
}

/* FIXME: Parameters for @246:StopWatch_CheckMsg unknown */

/*************************************************************************
 *      @	[SHLWAPI.247]
 *
 * Log the start of an applet.
 *
 * PARAMS
 *  lpszName [I] Name of the applet
 *
 * RETURNS
 *  Nothing.
 */
void WINAPI StopWatch_MarkFrameStart(LPCSTR lpszName)
{
  FIXME("(%s) stub!\n", debugstr_a(lpszName));
}

/* FIXME: Parameters for @248:StopWatch_MarkSameFrameStart unknown */

/*************************************************************************
 *      @	[SHLWAPI.249]
 *
 * Log a java applet stopping.
 *
 * PARAMS
 *  lpszEvent  [I] Name of the event (applet)
 *  hWnd       [I] Window running the applet
 *  dwReserved [I] Unused
 *
 * RETURNS
 *  Nothing.
 */
void WINAPI StopWatch_MarkJavaStop(LPCWSTR lpszEvent, HWND hWnd, DWORD dwReserved)
{
  FIXME("(%s,%p,0x%08lx) stub!\n", debugstr_w(lpszEvent), hWnd, dwReserved);
}

/*************************************************************************
 *      @	[SHLWAPI.250]
 *
 * Read the performance counter.
 *
 * PARAMS
 *  None.
 *
 * RETURNS
 *  The low 32 bits of the current performance counter reading.
 */
DWORD WINAPI GetPerfTime()
{
  static LONG64 iCounterFreq = 0;
  LARGE_INTEGER iCounter;

  TRACE("()\n");

  if (!iCounterFreq)
   QueryPerformanceFrequency((LARGE_INTEGER*)&iCounterFreq);

  QueryPerformanceCounter(&iCounter);
  iCounter.QuadPart = iCounter.QuadPart * 1000 / iCounterFreq;
  return iCounter.u.LowPart;
}

/* FIXME: Parameters for @251:StopWatch_DispatchTime unknown */

/*************************************************************************
 *      @	[SHLWAPI.252]
 *
 * Set an as yet unidentified performance value.
 *
 * PARAMS
 *  dwUnknown [I] Value to set
 *
 * RETURNS
 *  dwUnknown.
 */
DWORD WINAPI StopWatch_SetMsgLastLocation(DWORD dwUnknown)
{
  FIXME("(%ld) stub!\n", dwUnknown);

  return dwUnknown;
}

/* FIXME: Parameters for @253:StopWatchExA, 254:StopWatchExW unknown */
