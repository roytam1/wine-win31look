/*
 * Win32 critical sections
 *
 * Copyright 1998 Alexandre Julliard
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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include "winerror.h"
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winternl.h"
#include "wine/debug.h"
#include "ntdll_misc.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);
WINE_DECLARE_DEBUG_CHANNEL(relay);

inline static LONG interlocked_inc( PLONG dest )
{
    return interlocked_xchg_add( dest, 1 ) + 1;
}

inline static LONG interlocked_dec( PLONG dest )
{
    return interlocked_xchg_add( dest, -1 ) - 1;
}

/***********************************************************************
 *           get_semaphore
 */
static inline HANDLE get_semaphore( RTL_CRITICAL_SECTION *crit )
{
    HANDLE ret = crit->LockSemaphore;
    if (!ret)
    {
        HANDLE sem;
        if (NtCreateSemaphore( &sem, SEMAPHORE_ALL_ACCESS, NULL, 0, 1 )) return 0;
        if (!(ret = (HANDLE)interlocked_cmpxchg_ptr( (PVOID *)&crit->LockSemaphore,
                                                     (PVOID)sem, 0 )))
            ret = sem;
        else
            NtClose(sem);  /* somebody beat us to it */
    }
    return ret;
}

/***********************************************************************
 *           RtlInitializeCriticalSection   (NTDLL.@)
 *
 * Initialise a new RTL_CRITICAL_SECTION.
 *
 * PARAMS
 *  crit [O] Critical section to initialise
 *
 * RETURN
 *  STATUS_SUCCESS.
 */
NTSTATUS WINAPI RtlInitializeCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    if (!GetProcessHeap()) crit->DebugInfo = NULL;
    else
    {
        crit->DebugInfo = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(CRITICAL_SECTION_DEBUG));
        if (crit->DebugInfo)
        {
            crit->DebugInfo->Type = 0;
            crit->DebugInfo->CreatorBackTraceIndex = 0;
            crit->DebugInfo->CriticalSection = crit;
            crit->DebugInfo->ProcessLocksList.Blink = &(crit->DebugInfo->ProcessLocksList);
            crit->DebugInfo->ProcessLocksList.Flink = &(crit->DebugInfo->ProcessLocksList);
            crit->DebugInfo->EntryCount = 0;
            crit->DebugInfo->ContentionCount = 0;
            crit->DebugInfo->Spare[0] = 0;
            crit->DebugInfo->Spare[1] = 0;
        }
    }
    crit->LockCount      = -1;
    crit->RecursionCount = 0;
    crit->OwningThread   = 0;
    crit->LockSemaphore  = 0;
    return STATUS_SUCCESS;
}

/***********************************************************************
 *           RtlInitializeCriticalSectionAndSpinCount   (NTDLL.@)
 *
 * Initialise a new RTL_CRITICAL_SECTION with a given spin count.
 *
 * PARAMS
 *   crit      [O] Critical section to initialise
 *   spincount [I] Spin count for crit
 * 
 * RETURNS
 *  STATUS_SUCCESS.
 *
 * NOTES
 * The InitializeCriticalSectionAndSpinCount() (KERNEL32) function is
 * available on NT4SP3 or later, and Win98 or later.
 * I am assuming that this is the correct definition given the MSDN
 * docs for the kernel32 functions.
 */
NTSTATUS WINAPI RtlInitializeCriticalSectionAndSpinCount( RTL_CRITICAL_SECTION *crit, DWORD spincount )
{
    if(spincount) TRACE("critsection=%p: spincount=%ld not supported\n", crit, spincount);
    crit->SpinCount = spincount;
    return RtlInitializeCriticalSection( crit );
}


/***********************************************************************
 *           RtlDeleteCriticalSection   (NTDLL.@)
 *
 * Free the resources used by an RTL_CRITICAL_SECTION.
 *
 * PARAMS
 *  crit [I/O] Critical section to free
 *
 * RETURNS
 *  STATUS_SUCCESS.
 */
NTSTATUS WINAPI RtlDeleteCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    crit->LockCount      = -1;
    crit->RecursionCount = 0;
    crit->OwningThread   = 0;
    if (crit->LockSemaphore) NtClose( crit->LockSemaphore );
    crit->LockSemaphore  = 0;
    if (crit->DebugInfo)
    {
        /* only free the ones we made in here */
        if (!crit->DebugInfo->Spare[1])
        {
            RtlFreeHeap( GetProcessHeap(), 0, crit->DebugInfo );
            crit->DebugInfo = NULL;
        }
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           RtlpWaitForCriticalSection   (NTDLL.@)
 *
 * Wait for an RTL_CRITICAL_SECTION to become free.
 * 
 * PARAMS
 *  crit [I/O] Critical section to wait for
 *
 * RETURNS
 *  STATUS_SUCCESS.
 */
NTSTATUS WINAPI RtlpWaitForCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    for (;;)
    {
        EXCEPTION_RECORD rec;
        HANDLE sem = get_semaphore( crit );
        LARGE_INTEGER time;
        DWORD status;

        time.QuadPart = -5000 * 10000;  /* 5 seconds */
        status = NtWaitForSingleObject( sem, FALSE, &time );
        if ( status == WAIT_TIMEOUT )
        {
            const char *name = NULL;
            if (crit->DebugInfo) name = (char *)crit->DebugInfo->Spare[1];
            if (!name) name = "?";
            ERR( "section %p %s wait timed out in thread %04lx, blocked by %04lx, retrying (60 sec)\n",
                 crit, debugstr_a(name), GetCurrentThreadId(), (DWORD)crit->OwningThread );
            time.QuadPart = -60000 * 10000;
            status = NtWaitForSingleObject( sem, FALSE, &time );
            if ( status == WAIT_TIMEOUT && TRACE_ON(relay) )
            {
                ERR( "section %p %s wait timed out in thread %04lx, blocked by %04lx, retrying (5 min)\n",
                     crit, debugstr_a(name), GetCurrentThreadId(), (DWORD) crit->OwningThread );
                time.QuadPart = -300000 * (ULONGLONG)10000;
                status = NtWaitForSingleObject( sem, FALSE, &time );
            }
        }
        if (status == STATUS_WAIT_0) return STATUS_SUCCESS;

        /* Throw exception only for Wine internal locks */
        if ((!crit->DebugInfo) || (!crit->DebugInfo->Spare[1])) continue;

        rec.ExceptionCode    = STATUS_POSSIBLE_DEADLOCK;
        rec.ExceptionFlags   = 0;
        rec.ExceptionRecord  = NULL;
        rec.ExceptionAddress = RtlRaiseException;  /* sic */
        rec.NumberParameters = 1;
        rec.ExceptionInformation[0] = (DWORD)crit;
        RtlRaiseException( &rec );
    }
}


/***********************************************************************
 *           RtlpUnWaitCriticalSection   (NTDLL.@)
 */
NTSTATUS WINAPI RtlpUnWaitCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    HANDLE sem = get_semaphore( crit );
    NTSTATUS res = NtReleaseSemaphore( sem, 1, NULL );
    if (res) RtlRaiseStatus( res );
    return res;
}


/***********************************************************************
 *           RtlEnterCriticalSection   (NTDLL.@)
 *
 * Enter an RTL_CRITICAL_SECTION.
 *
 * PARAMS
 *  crit [I/O] Critical section to enter
 *
 * RETURNS
 *  STATUS_SUCCESS. The critical section is held by the caller.
 *  
 * NOTES
 *  The caller will wait until the critical section is availale.
 */
NTSTATUS WINAPI RtlEnterCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    if (interlocked_inc( &crit->LockCount ))
    {
        if (crit->OwningThread == (HANDLE)GetCurrentThreadId())
        {
            crit->RecursionCount++;
            return STATUS_SUCCESS;
        }

        /* Now wait for it */
        RtlpWaitForCriticalSection( crit );
    }
    crit->OwningThread   = (HANDLE)GetCurrentThreadId();
    crit->RecursionCount = 1;
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           RtlTryEnterCriticalSection   (NTDLL.@)
 *
 * Enter an RTL_CRITICAL_SECTION without waiting.
 *
 * PARAMS
 *  crit [I/O] Critical section to enter
 *
 * RETURNS
 *  Success: TRUE. The critical section is held by the caller.
 *  Failure: FALSE. The critical section is currently held by another thread.
 */
BOOL WINAPI RtlTryEnterCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    BOOL ret = FALSE;
    if (interlocked_cmpxchg( &crit->LockCount, 0L, -1 ) == -1)
    {
        crit->OwningThread   = (HANDLE)GetCurrentThreadId();
        crit->RecursionCount = 1;
        ret = TRUE;
    }
    else if (crit->OwningThread == (HANDLE)GetCurrentThreadId())
    {
        interlocked_inc( &crit->LockCount );
        crit->RecursionCount++;
        ret = TRUE;
    }
    return ret;
}


/***********************************************************************
 *           RtlLeaveCriticalSection   (NTDLL.@)
 *
 * Leave an RTL_CRITICAL_SECTION.
 *
 * PARAMS
 *  crit [I/O] Critical section to enter
 *
 * RETURNS
 *  STATUS_SUCCESS.
 */
NTSTATUS WINAPI RtlLeaveCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    if (--crit->RecursionCount) interlocked_dec( &crit->LockCount );
    else
    {
        crit->OwningThread = 0;
        if (interlocked_dec( &crit->LockCount ) >= 0)
        {
            /* someone is waiting */
            RtlpUnWaitCriticalSection( crit );
        }
    }
    return STATUS_SUCCESS;
}
