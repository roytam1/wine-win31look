/*
 * Misc Toolhelp functions
 *
 * Copyright 1996 Marcus Meissner
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
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <ctype.h>
#include <assert.h>
#include "windef.h"
#include "winbase.h"
#include "wine/winbase16.h"
#include "winerror.h"
#include "local.h"
#include "tlhelp32.h"
#include "toolhelp.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(toolhelp);


/* FIXME: to make this work, we have to call back all these registered
 * functions from all over the WINE code. Someone with more knowledge than
 * me please do that. -Marcus
 */
static struct notify
{
    HTASK16   htask;
    FARPROC16 lpfnCallback;
    WORD     wFlags;
} *notifys = NULL;

static int nrofnotifys = 0;

static FARPROC16 HookNotify = NULL;

/***********************************************************************
 *		NotifyRegister (TOOLHELP.73)
 */
BOOL16 WINAPI NotifyRegister16( HTASK16 htask, FARPROC16 lpfnCallback,
                              WORD wFlags )
{
    int	i;

    FIXME("(%x,%lx,%x), semi-stub.\n",
                      htask, (DWORD)lpfnCallback, wFlags );
    if (!htask) htask = GetCurrentTask();
    for (i=0;i<nrofnotifys;i++)
        if (notifys[i].htask==htask)
            break;
    if (i==nrofnotifys) {
        if (notifys==NULL)
            notifys=(struct notify*)HeapAlloc( GetProcessHeap(), 0,
                                               sizeof(struct notify) );
        else
            notifys=(struct notify*)HeapReAlloc( GetProcessHeap(), 0, notifys,
                                        sizeof(struct notify)*(nrofnotifys+1));
        if (!notifys) return FALSE;
        nrofnotifys++;
    }
    notifys[i].htask=htask;
    notifys[i].lpfnCallback=lpfnCallback;
    notifys[i].wFlags=wFlags;
    return TRUE;
}

/***********************************************************************
 *		NotifyUnregister (TOOLHELP.74)
 */
BOOL16 WINAPI NotifyUnregister16( HTASK16 htask )
{
    int	i;

    FIXME("(%x), semi-stub.\n", htask );
    if (!htask) htask = GetCurrentTask();
    for (i=nrofnotifys;i--;)
        if (notifys[i].htask==htask)
            break;
    if (i==-1)
        return FALSE;
    memcpy(notifys+i,notifys+(i+1),sizeof(struct notify)*(nrofnotifys-i-1));
    notifys=(struct notify*)HeapReAlloc( GetProcessHeap(), 0, notifys,
                                        (nrofnotifys-1)*sizeof(struct notify));
    nrofnotifys--;
    return TRUE;
}

/***********************************************************************
 *		StackTraceCSIPFirst (TOOLHELP.67)
 */
BOOL16 WINAPI StackTraceCSIPFirst16(STACKTRACEENTRY *ste, WORD wSS, WORD wCS, WORD wIP, WORD wBP)
{
    FIXME("(%p, ss %04x, cs %04x, ip %04x, bp %04x): stub.\n", ste, wSS, wCS, wIP, wBP);
    return TRUE;
}

/***********************************************************************
 *		StackTraceFirst (TOOLHELP.66)
 */
BOOL16 WINAPI StackTraceFirst16(STACKTRACEENTRY *ste, HTASK16 Task)
{
    FIXME("(%p, %04x), stub.\n", ste, Task);
    return TRUE;
}

/***********************************************************************
 *		StackTraceNext (TOOLHELP.68)
 */
BOOL16 WINAPI StackTraceNext16(STACKTRACEENTRY *ste)
{
    FIXME("(%p), stub.\n", ste);
    return TRUE;
}

/***********************************************************************
 *		InterruptRegister (TOOLHELP.75)
 */
BOOL16 WINAPI InterruptRegister16( HTASK16 task, FARPROC callback )
{
    FIXME("(%04x, %p), stub.\n", task, callback);
    return TRUE;
}

/***********************************************************************
 *		InterruptUnRegister (TOOLHELP.76)
 */
BOOL16 WINAPI InterruptUnRegister16( HTASK16 task )
{
    FIXME("(%04x), stub.\n", task);
    return TRUE;
}

/***********************************************************************
 *           TimerCount   (TOOLHELP.80)
 */
BOOL16 WINAPI TimerCount16( TIMERINFO *pTimerInfo )
{
    /* FIXME
     * In standard mode, dwmsSinceStart = dwmsThisVM
     *
     * I tested this, under Windows in enhanced mode, and
     * if you never switch VM (ie start/stop DOS) these
     * values should be the same as well.
     *
     * Also, Wine should adjust for the hardware timer
     * to reduce the amount of error to ~1ms.
     * I can't be bothered, can you?
     */
    pTimerInfo->dwmsSinceStart = pTimerInfo->dwmsThisVM = GetTickCount();
    return TRUE;
}

/***********************************************************************
 *           SystemHeapInfo   (TOOLHELP.71)
 */
BOOL16 WINAPI SystemHeapInfo16( SYSHEAPINFO *pHeapInfo )
{
    WORD user = LoadLibrary16( "USER.EXE" );
    WORD gdi = LoadLibrary16( "GDI.EXE" );
    pHeapInfo->wUserFreePercent = (int)LOCAL_CountFree(user) * 100 / LOCAL_HeapSize(user);
    pHeapInfo->wGDIFreePercent  = (int)LOCAL_CountFree(gdi) * 100 / LOCAL_HeapSize(gdi);
    pHeapInfo->hUserSegment = user;
    pHeapInfo->hGDISegment  = gdi;
    FreeLibrary16( user );
    FreeLibrary16( gdi );
    return TRUE;
}


/***********************************************************************
 *           ToolHelpHook                             (KERNEL.341)
 *	see "Undocumented Windows"
 */
FARPROC16 WINAPI ToolHelpHook16(FARPROC16 lpfnNotifyHandler)
{
	FARPROC16 tmp;

	FIXME("(%p), stub.\n", lpfnNotifyHandler);
	tmp = HookNotify;
	HookNotify = lpfnNotifyHandler;
	/* just return previously installed notification function */
	return tmp;
}


/***********************************************************************
 *           CreateToolhelp32Snapshot			(KERNEL32.@)
 */
HANDLE WINAPI CreateToolhelp32Snapshot( DWORD flags, DWORD process )
{
    HANDLE ret;

    TRACE("%lx,%lx\n", flags, process );
    if (!(flags & (TH32CS_SNAPPROCESS|TH32CS_SNAPTHREAD|TH32CS_SNAPMODULE)))
    {
        FIXME("flags %lx not implemented\n", flags );
        SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
        return INVALID_HANDLE_VALUE;
    }

    /* Now do the snapshot */
    SERVER_START_REQ( create_snapshot )
    {
        req->flags = 0;
        if (flags & TH32CS_SNAPMODULE)   req->flags |= SNAP_MODULE;
        if (flags & TH32CS_SNAPPROCESS)  req->flags |= SNAP_PROCESS;
        if (flags & TH32CS_SNAPTHREAD)   req->flags |= SNAP_THREAD;
    
        req->inherit = (flags & TH32CS_INHERIT) != 0;
        req->pid     = process;
        wine_server_call_err( req );
        ret = reply->handle;
    }
    SERVER_END_REQ;
    if (!ret) ret = INVALID_HANDLE_VALUE;
    return ret;
}


/***********************************************************************
 *		TOOLHELP_Thread32Next
 *
 * Implementation of Thread32First/Next
 */
static BOOL TOOLHELP_Thread32Next( HANDLE handle, LPTHREADENTRY32 lpte, BOOL first )
{
    BOOL ret;

    if (lpte->dwSize < sizeof(THREADENTRY32))
    {
        SetLastError( ERROR_INSUFFICIENT_BUFFER );
        ERR("Result buffer too small (req: %d, was: %ld)\n", sizeof(THREADENTRY32), lpte->dwSize);
        return FALSE;
    }
    SERVER_START_REQ( next_thread )
    {
        req->handle = handle;
        req->reset = first;
        if ((ret = !wine_server_call_err( req )))
        {
            lpte->cntUsage           = reply->count;
            lpte->th32ThreadID       = (DWORD)reply->tid;
            lpte->th32OwnerProcessID = (DWORD)reply->pid;
            lpte->tpBasePri          = reply->base_pri;
            lpte->tpDeltaPri         = reply->delta_pri;
            lpte->dwFlags            = 0;  /* SDK: "reserved; do not use" */
        }
    }
    SERVER_END_REQ;
    return ret;
}

/***********************************************************************
 *		Thread32First    (KERNEL32.@)
 *
 * Return info about the first thread in a toolhelp32 snapshot
 */
BOOL WINAPI Thread32First(HANDLE hSnapshot, LPTHREADENTRY32 lpte)
{
    return TOOLHELP_Thread32Next(hSnapshot, lpte, TRUE);
}

/***********************************************************************
 *		Thread32Next   (KERNEL32.@)
 *
 * Return info about the "next" thread in a toolhelp32 snapshot
 */
BOOL WINAPI Thread32Next(HANDLE hSnapshot, LPTHREADENTRY32 lpte)
{
    return TOOLHELP_Thread32Next(hSnapshot, lpte, FALSE);
}

/***********************************************************************
 *		TOOLHELP_Process32Next
 *
 * Implementation of Process32First/Next
 */
static BOOL TOOLHELP_Process32Next( HANDLE handle, LPPROCESSENTRY32 lppe, BOOL first )
{
    BOOL ret;
    WCHAR exe[MAX_PATH];
    DWORD len;

    if (lppe->dwSize < sizeof(PROCESSENTRY32))
    {
        SetLastError( ERROR_INSUFFICIENT_BUFFER );
        ERR("Result buffer too small (req: %d, was: %ld)\n", sizeof(PROCESSENTRY32), lppe->dwSize);
        return FALSE;
    }
    SERVER_START_REQ( next_process )
    {
        req->handle = handle;
        req->reset = first;
        wine_server_set_reply( req, exe, sizeof(exe) );
        if ((ret = !wine_server_call_err( req )))
        {
            lppe->cntUsage            = reply->count;
            lppe->th32ProcessID       = reply->pid;
            lppe->th32DefaultHeapID   = (DWORD)reply->heap;
            lppe->th32ModuleID        = (DWORD)reply->module;
            lppe->cntThreads          = reply->threads;
            lppe->th32ParentProcessID = reply->ppid;
            lppe->pcPriClassBase      = reply->priority;
            lppe->dwFlags             = -1; /* FIXME */
            len = WideCharToMultiByte( CP_ACP, 0, exe, wine_server_reply_size(reply) / sizeof(WCHAR),
                                       lppe->szExeFile, sizeof(lppe->szExeFile), NULL, NULL );
            lppe->szExeFile[len] = 0;
        }
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *		Process32First    (KERNEL32.@)
 *
 * Return info about the first process in a toolhelp32 snapshot
 */
BOOL WINAPI Process32First(HANDLE hSnapshot, LPPROCESSENTRY32 lppe)
{
    return TOOLHELP_Process32Next( hSnapshot, lppe, TRUE );
}

/***********************************************************************
 *		Process32Next   (KERNEL32.@)
 *
 * Return info about the "next" process in a toolhelp32 snapshot
 */
BOOL WINAPI Process32Next(HANDLE hSnapshot, LPPROCESSENTRY32 lppe)
{
    return TOOLHELP_Process32Next( hSnapshot, lppe, FALSE );
}


/***********************************************************************
 *		TOOLHELP_Module32Next
 *
 * Implementation of Module32First/Next
 */
static BOOL TOOLHELP_Module32Next( HANDLE handle, LPMODULEENTRY32 lpme, BOOL first )
{
    BOOL ret;
    WCHAR exe[MAX_PATH];
    DWORD len;

    if (lpme->dwSize < sizeof (MODULEENTRY32))
    {
        SetLastError( ERROR_INSUFFICIENT_BUFFER );
        ERR("Result buffer too small (req: %d, was: %ld)\n", sizeof(MODULEENTRY32), lpme->dwSize);
        return FALSE;
    }
    SERVER_START_REQ( next_module )
    {
        req->handle = handle;
        req->reset = first;
        wine_server_set_reply( req, exe, sizeof(exe) );
        if ((ret = !wine_server_call_err( req )))
        {
            lpme->th32ModuleID   = 0;  /* toolhelp internal id, never used */
            lpme->th32ProcessID  = reply->pid;
            lpme->GlblcntUsage   = 0; /* FIXME */
            lpme->ProccntUsage   = 0; /* FIXME */
            lpme->modBaseAddr    = reply->base;
            lpme->modBaseSize    = reply->size;
            lpme->hModule        = reply->base;
            lpme->szModule[0]    = 0;  /* FIXME */
            len = WideCharToMultiByte( CP_ACP, 0, exe, wine_server_reply_size(reply) / sizeof(WCHAR),
                                       lpme->szExePath, sizeof(lpme->szExePath), NULL, NULL );
            lpme->szExePath[len] = 0;
        }
    }
    SERVER_END_REQ;
    return ret;
}

/***********************************************************************
 *		Module32First   (KERNEL32.@)
 *
 * Return info about the "first" module in a toolhelp32 snapshot
 */
BOOL WINAPI Module32First(HANDLE hSnapshot, LPMODULEENTRY32 lpme)
{
    return TOOLHELP_Module32Next( hSnapshot, lpme, TRUE );
}

/***********************************************************************
 *		Module32Next   (KERNEL32.@)
 *
 * Return info about the "next" module in a toolhelp32 snapshot
 */
BOOL WINAPI Module32Next(HANDLE hSnapshot, LPMODULEENTRY32 lpme)
{
    return TOOLHELP_Module32Next( hSnapshot, lpme, FALSE );
}

/************************************************************************
 *              GlobalMasterHandle (KERNEL.28)
 *
 *
 * Should return selector and handle of the information structure for
 * the global heap. selector and handle are stored in the THHOOK as
 * pGlobalHeap and hGlobalHeap.
 * As Wine doesn't have this structure, we return both values as zero
 * Applications should interpret this as "No Global Heap"
 */
DWORD WINAPI GlobalMasterHandle16(void)
{
    FIXME(": stub\n");
    return 0;
}

/************************************************************************
 *              Heap32ListFirst (KERNEL.@)
 *
 */
BOOL WINAPI Heap32ListFirst(HANDLE hSnapshot, LPHEAPLIST32 lphl)
{
    FIXME(": stub\n");
    return FALSE;
}

/******************************************************************
 *		Toolhelp32ReadProcessMemory (KERNEL.@)
 *
 *
 */
BOOL WINAPI Toolhelp32ReadProcessMemory(DWORD pid, const void* base,
                                        void* buf, SIZE_T len, SIZE_T* r)
{
    HANDLE h;
    BOOL   ret = FALSE;

    h = (pid) ? OpenProcess(PROCESS_VM_READ, FALSE, pid) : GetCurrentProcess();
    if (h != NULL)
    {
        ret = ReadProcessMemory(h, base, buf, len, r);
        if (pid) CloseHandle(h);
    }
    return ret;
}
