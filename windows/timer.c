/*
 * Timer functions
 *
 * Copyright 1993 Alexandre Julliard
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "wine/winuser16.h"
#include "winuser.h"
#include "winerror.h"

#include "winproc.h"
#include "message.h"
#include "win.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(timer);


typedef struct tagTIMER
{
    HWND           hwnd;
    DWORD          thread;
    UINT           msg;  /* WM_TIMER or WM_SYSTIMER */
    UINT           id;
    UINT           timeout;
    WNDPROC        proc;
} TIMER;

#define NB_TIMERS            34
#define NB_RESERVED_TIMERS    2  /* for SetSystemTimer */

#define SYS_TIMER_RATE  55   /* min. timer rate in ms (actually 54.925)*/

static TIMER TimersArray[NB_TIMERS];

static CRITICAL_SECTION csTimer;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &csTimer,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { 0, (DWORD)(__FILE__ ": csTimer") }
};
static CRITICAL_SECTION csTimer = { &critsect_debug, -1, 0, 0, 0, 0 };


/***********************************************************************
 *           TIMER_ClearTimer
 *
 * Clear and remove a timer.
 */
static void TIMER_ClearTimer( TIMER * pTimer )
{
    pTimer->hwnd    = 0;
    pTimer->msg     = 0;
    pTimer->id      = 0;
    pTimer->timeout = 0;
    WINPROC_FreeProc( pTimer->proc, WIN_PROC_TIMER );
}


/***********************************************************************
 *           TIMER_RemoveWindowTimers
 *
 * Remove all timers for a given window.
 */
void TIMER_RemoveWindowTimers( HWND hwnd )
{
    int i;
    TIMER *pTimer;

    EnterCriticalSection( &csTimer );

    for (i = NB_TIMERS, pTimer = TimersArray; i > 0; i--, pTimer++)
	if ((pTimer->hwnd == hwnd) && pTimer->timeout)
            TIMER_ClearTimer( pTimer );

    LeaveCriticalSection( &csTimer );
}


/***********************************************************************
 *           TIMER_RemoveThreadTimers
 *
 * Remove all timers for the current thread.
 */
void TIMER_RemoveThreadTimers(void)
{
    int i;
    TIMER *pTimer;

    EnterCriticalSection( &csTimer );

    for (i = NB_TIMERS, pTimer = TimersArray; i > 0; i--, pTimer++)
        if ((pTimer->thread == GetCurrentThreadId()) && pTimer->timeout)
            TIMER_ClearTimer( pTimer );

    LeaveCriticalSection( &csTimer );
}


/***********************************************************************
 *           TIMER_SetTimer
 */
static UINT_PTR TIMER_SetTimer( HWND hwnd, UINT_PTR id, UINT timeout,
                                WNDPROC proc, WINDOWPROCTYPE type, BOOL sys )
{
    int i;
    TIMER * pTimer;
    WNDPROC winproc = 0;

    if (hwnd && !(hwnd = WIN_IsCurrentThread( hwnd )))
    {
        SetLastError( ERROR_INVALID_WINDOW_HANDLE );
        return 0;
    }

    if (!timeout)
      {       /* timeout==0 is a legal argument  UB 990821*/
       WARN("Timeout== 0 not implemented, using timeout=1\n");
        timeout=1;
      }

    EnterCriticalSection( &csTimer );

      /* Check if there's already a timer with the same hwnd and id */

    for (i = 0, pTimer = TimersArray; i < NB_TIMERS; i++, pTimer++)
        if ((pTimer->hwnd == hwnd) && (pTimer->id == id) &&
            (pTimer->timeout != 0))
        {
            TIMER_ClearTimer( pTimer );
            break;
        }

    if ( i == NB_TIMERS )
    {
          /* Find a free timer */

        for (i = 0, pTimer = TimersArray; i < NB_TIMERS; i++, pTimer++)
            if (!pTimer->timeout) break;

        if ( (i >= NB_TIMERS) ||
             (!sys && (i >= NB_TIMERS-NB_RESERVED_TIMERS)) )
        {
            LeaveCriticalSection( &csTimer );
            return 0;
        }
    }

    if (!hwnd) id = i + 1;

    if (proc) WINPROC_SetProc( &winproc, proc, type, WIN_PROC_TIMER );

    SERVER_START_REQ( set_win_timer )
    {
        req->win    = hwnd;
        req->msg    = sys ? WM_SYSTIMER : WM_TIMER;
        req->id     = id;
        req->rate   = max( timeout, SYS_TIMER_RATE );
        req->lparam = (unsigned int)winproc;
        wine_server_call( req );
    }
    SERVER_END_REQ;

      /* Add the timer */

    pTimer->hwnd    = hwnd;
    pTimer->thread  = GetCurrentThreadId();
    pTimer->msg     = sys ? WM_SYSTIMER : WM_TIMER;
    pTimer->id      = id;
    pTimer->timeout = timeout;
    pTimer->proc    = winproc;

    TRACE("Timer added: %p, %p, %04x, %04x, %p\n",
          pTimer, pTimer->hwnd, pTimer->msg, pTimer->id, pTimer->proc );

    LeaveCriticalSection( &csTimer );

    if (!id) return TRUE;
    else return id;
}


/***********************************************************************
 *           TIMER_KillTimer
 */
static BOOL TIMER_KillTimer( HWND hwnd, UINT_PTR id, BOOL sys )
{
    int i;
    TIMER * pTimer;

    SERVER_START_REQ( kill_win_timer )
    {
        req->win = hwnd;
        req->msg = sys ? WM_SYSTIMER : WM_TIMER;
        req->id  = id;
        wine_server_call( req );
    }
    SERVER_END_REQ;

    EnterCriticalSection( &csTimer );

    /* Find the timer */

    for (i = 0, pTimer = TimersArray; i < NB_TIMERS; i++, pTimer++)
	if ((pTimer->hwnd == hwnd) && (pTimer->id == id) &&
	    (pTimer->timeout != 0)) break;

    if ( (i >= NB_TIMERS) ||
         (!sys && (i >= NB_TIMERS-NB_RESERVED_TIMERS)) ||
         (!sys && (pTimer->msg != WM_TIMER)) ||
         (sys && (pTimer->msg != WM_SYSTIMER)) )
    {
        LeaveCriticalSection( &csTimer );
        return FALSE;
    }

    /* Delete the timer */

    TIMER_ClearTimer( pTimer );

    LeaveCriticalSection( &csTimer );

    return TRUE;
}


/***********************************************************************
 *		SetTimer (USER.10)
 */
UINT16 WINAPI SetTimer16( HWND16 hwnd, UINT16 id, UINT16 timeout,
                          TIMERPROC16 proc )
{
    TRACE("%04x %d %d %08lx\n",
                   hwnd, id, timeout, (LONG)proc );
    return TIMER_SetTimer( WIN_Handle32(hwnd), id, timeout, (WNDPROC)proc,
                           WIN_PROC_16, FALSE );
}


/***********************************************************************
 *		SetTimer (USER32.@)
 */
UINT_PTR WINAPI SetTimer( HWND hwnd, UINT_PTR id, UINT timeout,
                          TIMERPROC proc )
{
    TRACE("%p %d %d %p\n", hwnd, id, timeout, proc );
    return TIMER_SetTimer( hwnd, id, timeout, (WNDPROC)proc, WIN_PROC_32A, FALSE );
}


/***********************************************************************
 *           TIMER_IsTimerValid
 */
BOOL TIMER_IsTimerValid( HWND hwnd, UINT_PTR id, WNDPROC proc )
{
    int i;
    TIMER *pTimer;
    BOOL ret = FALSE;

    hwnd = WIN_GetFullHandle( hwnd );
    EnterCriticalSection( &csTimer );

    for (i = 0, pTimer = TimersArray; i < NB_TIMERS; i++, pTimer++)
        if ((pTimer->hwnd == hwnd) && (pTimer->id == id) && (pTimer->proc == proc))
        {
            ret = TRUE;
            break;
        }

   LeaveCriticalSection( &csTimer );
   return ret;
}


/***********************************************************************
 *		SetSystemTimer (USER.11)
 */
UINT16 WINAPI SetSystemTimer16( HWND16 hwnd, UINT16 id, UINT16 timeout,
                                TIMERPROC16 proc )
{
    TRACE("%04x %d %d %08lx\n",
                   hwnd, id, timeout, (LONG)proc );
    return TIMER_SetTimer( WIN_Handle32(hwnd), id, timeout, (WNDPROC)proc, WIN_PROC_16, TRUE );
}


/***********************************************************************
 *		SetSystemTimer (USER32.@)
 */
UINT_PTR WINAPI SetSystemTimer( HWND hwnd, UINT_PTR id, UINT timeout,
                                TIMERPROC proc )
{
    TRACE("%p %d %d %p\n", hwnd, id, timeout, proc );
    return TIMER_SetTimer( hwnd, id, timeout, (WNDPROC)proc, WIN_PROC_32A, TRUE );
}


/***********************************************************************
 *		KillTimer (USER32.@)
 */
BOOL WINAPI KillTimer( HWND hwnd, UINT_PTR id )
{
    TRACE("%p %d\n", hwnd, id );
    return TIMER_KillTimer( hwnd, id, FALSE );
}


/***********************************************************************
 *		KillSystemTimer (USER32.@)
 */
BOOL WINAPI KillSystemTimer( HWND hwnd, UINT_PTR id )
{
    TRACE("%p %d\n", hwnd, id );
    return TIMER_KillTimer( hwnd, id, TRUE );
}
