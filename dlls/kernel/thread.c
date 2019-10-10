/*
 * Win32 threads
 *
 * Copyright 1996 Alexandre Julliard
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
#include <fcntl.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/types.h>
#ifdef HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "ntstatus.h"
#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winnls.h"
#include "module.h"
#include "thread.h"
#include "wine/winbase16.h"
#include "wine/exception.h"
#include "wine/library.h"
#include "wine/pthread.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(thread);
WINE_DECLARE_DEBUG_CHANNEL(relay);


/***********************************************************************
 *           THREAD_InitStack
 *
 * Allocate the stack of a thread.
 */
TEB *THREAD_InitStack( TEB *teb, DWORD stack_size )
{
    DWORD old_prot;
    DWORD page_size = getpagesize();
    void *base;

    stack_size = (stack_size + (page_size - 1)) & ~(page_size - 1);
    if (stack_size < 1024 * 1024) stack_size = 1024 * 1024;  /* Xlib needs a large stack */

    if (!(base = VirtualAlloc( NULL, stack_size, MEM_COMMIT, PAGE_EXECUTE_READWRITE )))
        return NULL;

    teb->DeallocationStack = base;
    teb->Tib.StackBase     = (char *)base + stack_size;
    teb->Tib.StackLimit    = base;  /* note: limit is lower than base since the stack grows down */

    /* Setup guard pages */

    VirtualProtect( base, 1, PAGE_EXECUTE_READWRITE | PAGE_GUARD, &old_prot );
    return teb;
}


struct new_thread_info
{
    LPTHREAD_START_ROUTINE func;
    void                  *arg;
};

/***********************************************************************
 *           THREAD_Start
 *
 * Start execution of a newly created thread. Does not return.
 */
static void CALLBACK THREAD_Start( void *ptr )
{
    struct new_thread_info *info = ptr;
    LPTHREAD_START_ROUTINE func = info->func;
    void *arg = info->arg;

    RtlFreeHeap( GetProcessHeap(), 0, info );

    if (TRACE_ON(relay))
        DPRINTF("%04lx:Starting thread (entryproc=%p)\n", GetCurrentThreadId(), func );

    __TRY
    {
        MODULE_DllThreadAttach( NULL );
        ExitThread( func( arg ) );
    }
    __EXCEPT(UnhandledExceptionFilter)
    {
        TerminateThread( GetCurrentThread(), GetExceptionCode() );
    }
    __ENDTRY
}


/***********************************************************************
 *           CreateThread   (KERNEL32.@)
 */
HANDLE WINAPI CreateThread( SECURITY_ATTRIBUTES *sa, SIZE_T stack,
                            LPTHREAD_START_ROUTINE start, LPVOID param,
                            DWORD flags, LPDWORD id )
{
    HANDLE handle;
    CLIENT_ID client_id;
    NTSTATUS status;
    SIZE_T stack_reserve = 0, stack_commit = 0;
    struct new_thread_info *info;

    if (!(info = RtlAllocateHeap( GetProcessHeap(), 0, sizeof(*info) )))
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return 0;
    }
    info->func = start;
    info->arg  = param;

    if (flags & STACK_SIZE_PARAM_IS_A_RESERVATION) stack_reserve = stack;
    else stack_commit = stack;

    status = RtlCreateUserThread( GetCurrentProcess(), NULL, (flags & CREATE_SUSPENDED) != 0,
                                  NULL, stack_reserve, stack_commit,
                                  THREAD_Start, info, &handle, &client_id );
    if (status == STATUS_SUCCESS)
    {
        if (id) *id = (DWORD)client_id.UniqueThread;
        if (sa && (sa->nLength >= sizeof(*sa)) && sa->bInheritHandle)
            SetHandleInformation( handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT );
    }
    else
    {
        RtlFreeHeap( GetProcessHeap(), 0, info );
        SetLastError( RtlNtStatusToDosError(status) );
        handle = 0;
    }
    return handle;
}


/***********************************************************************
 * OpenThread  [KERNEL32.@]   Retrieves a handle to a thread from its thread id
 */
HANDLE WINAPI OpenThread( DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId )
{
    HANDLE ret = 0;
    SERVER_START_REQ( open_thread )
    {
        req->tid     = dwThreadId;
        req->access  = dwDesiredAccess;
        req->inherit = bInheritHandle;
        if (!wine_server_call_err( req )) ret = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 * ExitThread [KERNEL32.@]  Ends a thread
 *
 * RETURNS
 *    None
 */
void WINAPI ExitThread( DWORD code ) /* [in] Exit code for this thread */
{
    BOOL last;
    SERVER_START_REQ( terminate_thread )
    {
        /* send the exit code to the server */
        req->handle    = GetCurrentThread();
        req->exit_code = code;
        wine_server_call( req );
        last = reply->last;
    }
    SERVER_END_REQ;

    if (last)
    {
        LdrShutdownProcess();
        exit( code );
    }
    else
    {
        struct wine_pthread_thread_info info;
        sigset_t block_set;
        ULONG size;

        LdrShutdownThread();
        RtlAcquirePebLock();
        RemoveEntryList( &NtCurrentTeb()->TlsLinks );
        RtlReleasePebLock();

        info.stack_base  = NtCurrentTeb()->DeallocationStack;
        info.teb_base    = NtCurrentTeb();
        info.teb_sel     = wine_get_fs();
        info.exit_status = code;

        size = 0;
        NtFreeVirtualMemory( GetCurrentProcess(), &info.stack_base, &size, MEM_RELEASE | MEM_SYSTEM );
        info.stack_size = size;

        size = 0;
        NtFreeVirtualMemory( GetCurrentProcess(), &info.teb_base, &size, MEM_RELEASE | MEM_SYSTEM );
        info.teb_size = size;

        /* block the async signals */
        sigemptyset( &block_set );
        sigaddset( &block_set, SIGALRM );
        sigaddset( &block_set, SIGIO );
        sigaddset( &block_set, SIGINT );
        sigaddset( &block_set, SIGHUP );
        sigaddset( &block_set, SIGUSR1 );
        sigaddset( &block_set, SIGUSR2 );
        sigaddset( &block_set, SIGTERM );
        sigprocmask( SIG_BLOCK, &block_set, NULL );

        close( NtCurrentTeb()->wait_fd[0] );
        close( NtCurrentTeb()->wait_fd[1] );
        close( NtCurrentTeb()->reply_fd );
        close( NtCurrentTeb()->request_fd );

        wine_pthread_exit_thread( &info );
    }
}


/**********************************************************************
 * TerminateThread [KERNEL32.@]  Terminates a thread
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI TerminateThread( HANDLE handle,    /* [in] Handle to thread */
                             DWORD exit_code)  /* [in] Exit code for thread */
{
    NTSTATUS status = NtTerminateThread( handle, exit_code );
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}


/***********************************************************************
 *           FreeLibraryAndExitThread (KERNEL32.@)
 */
void WINAPI FreeLibraryAndExitThread(HINSTANCE hLibModule, DWORD dwExitCode)
{
    FreeLibrary(hLibModule);
    ExitThread(dwExitCode);
}


/**********************************************************************
 *		GetExitCodeThread (KERNEL32.@)
 *
 * Gets termination status of thread.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI GetExitCodeThread(
    HANDLE hthread, /* [in]  Handle to thread */
    LPDWORD exitcode) /* [out] Address to receive termination status */
{
    THREAD_BASIC_INFORMATION info;
    NTSTATUS status = NtQueryInformationThread( hthread, ThreadBasicInformation,
                                                &info, sizeof(info), NULL );

    if (status)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        return FALSE;
    }
    if (exitcode) *exitcode = info.ExitStatus;
    return TRUE;
}


/***********************************************************************
 * SetThreadContext [KERNEL32.@]  Sets context of thread.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI SetThreadContext( HANDLE handle,           /* [in]  Handle to thread with context */
                              const CONTEXT *context ) /* [in] Address of context structure */
{
    NTSTATUS status = NtSetContextThread( handle, context );
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}


/***********************************************************************
 * GetThreadContext [KERNEL32.@]  Retrieves context of thread.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI GetThreadContext( HANDLE handle,     /* [in]  Handle to thread with context */
                              CONTEXT *context ) /* [out] Address of context structure */
{
    NTSTATUS status = NtGetContextThread( handle, context );
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}


/**********************************************************************
 * SuspendThread [KERNEL32.@]  Suspends a thread.
 *
 * RETURNS
 *    Success: Previous suspend count
 *    Failure: 0xFFFFFFFF
 */
DWORD WINAPI SuspendThread( HANDLE hthread ) /* [in] Handle to the thread */
{
    DWORD ret;
    NTSTATUS status = NtSuspendThread( hthread, &ret );

    if (status)
    {
        ret = ~0U;
        SetLastError( RtlNtStatusToDosError(status) );
    }
    return ret;
}


/**********************************************************************
 * ResumeThread [KERNEL32.@]  Resumes a thread.
 *
 * Decrements a thread's suspend count.  When count is zero, the
 * execution of the thread is resumed.
 *
 * RETURNS
 *    Success: Previous suspend count
 *    Failure: 0xFFFFFFFF
 *    Already running: 0
 */
DWORD WINAPI ResumeThread( HANDLE hthread ) /* [in] Identifies thread to restart */
{
    DWORD ret;
    NTSTATUS status = NtResumeThread( hthread, &ret );

    if (status)
    {
        ret = ~0U;
        SetLastError( RtlNtStatusToDosError(status) );
    }
    return ret;
}


/**********************************************************************
 * GetThreadPriority [KERNEL32.@]  Returns priority for thread.
 *
 * RETURNS
 *    Success: Thread's priority level.
 *    Failure: THREAD_PRIORITY_ERROR_RETURN
 */
INT WINAPI GetThreadPriority(
    HANDLE hthread) /* [in] Handle to thread */
{
    THREAD_BASIC_INFORMATION info;
    NTSTATUS status = NtQueryInformationThread( hthread, ThreadBasicInformation,
                                                &info, sizeof(info), NULL );

    if (status)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        return THREAD_PRIORITY_ERROR_RETURN;
    }
    return info.Priority;
}


/**********************************************************************
 * SetThreadPriority [KERNEL32.@]  Sets priority for thread.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI SetThreadPriority(
    HANDLE hthread, /* [in] Handle to thread */
    INT priority)   /* [in] Thread priority level */
{
    BOOL ret;
    SERVER_START_REQ( set_thread_info )
    {
        req->handle   = hthread;
        req->priority = priority;
        req->mask     = SET_THREAD_INFO_PRIORITY;
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


/**********************************************************************
 * GetThreadPriorityBoost [KERNEL32.@]  Returns priority boost for thread.
 *
 * Always reports that priority boost is disabled.
 *
 * RETURNS
 *    Success: TRUE.
 *    Failure: FALSE
 */
BOOL WINAPI GetThreadPriorityBoost(
    HANDLE hthread, /* [in] Handle to thread */
    PBOOL pstate)   /* [out] pointer to var that receives the boost state */
{
    if (pstate) *pstate = FALSE;
    return NO_ERROR;
}


/**********************************************************************
 * SetThreadPriorityBoost [KERNEL32.@]  Sets priority boost for thread.
 *
 * Priority boost is not implemented. Thsi function always returns
 * FALSE and sets last error to ERROR_CALL_NOT_IMPLEMENTED
 *
 * RETURNS
 *    Always returns FALSE to indicate a failure
 */
BOOL WINAPI SetThreadPriorityBoost(
    HANDLE hthread, /* [in] Handle to thread */
    BOOL disable)   /* [in] TRUE to disable priority boost */
{
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}


/**********************************************************************
 *           SetThreadAffinityMask   (KERNEL32.@)
 */
DWORD WINAPI SetThreadAffinityMask( HANDLE hThread, DWORD dwThreadAffinityMask )
{
    DWORD ret;
    SERVER_START_REQ( set_thread_info )
    {
        req->handle   = hThread;
        req->affinity = dwThreadAffinityMask;
        req->mask     = SET_THREAD_INFO_AFFINITY;
        ret = !wine_server_call_err( req );
        /* FIXME: should return previous value */
    }
    SERVER_END_REQ;
    return ret;
}


/**********************************************************************
 * SetThreadIdealProcessor [KERNEL32.@]  Obtains timing information.
 *
 * RETURNS
 *    Success: Value of last call to SetThreadIdealProcessor
 *    Failure: -1
 */
DWORD WINAPI SetThreadIdealProcessor(
    HANDLE hThread,          /* [in] Specifies the thread of interest */
    DWORD dwIdealProcessor)  /* [in] Specifies the new preferred processor */
{
    FIXME("(%p): stub\n",hThread);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return -1L;
}


/* callback for QueueUserAPC */
static void CALLBACK call_user_apc( ULONG_PTR arg1, ULONG_PTR arg2, ULONG_PTR arg3 )
{
    PAPCFUNC func = (PAPCFUNC)arg1;
    func( arg2 );
}

/***********************************************************************
 *              QueueUserAPC  (KERNEL32.@)
 */
DWORD WINAPI QueueUserAPC( PAPCFUNC func, HANDLE hthread, ULONG_PTR data )
{
    NTSTATUS status = NtQueueApcThread( hthread, call_user_apc, (ULONG_PTR)func, data, 0 );

    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}


/**********************************************************************
 * GetThreadTimes [KERNEL32.@]  Obtains timing information.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI GetThreadTimes(
    HANDLE thread,         /* [in]  Specifies the thread of interest */
    LPFILETIME creationtime, /* [out] When the thread was created */
    LPFILETIME exittime,     /* [out] When the thread was destroyed */
    LPFILETIME kerneltime,   /* [out] Time thread spent in kernel mode */
    LPFILETIME usertime)     /* [out] Time thread spent in user mode */
{
    BOOL ret = TRUE;

    if (creationtime || exittime)
    {
        /* We need to do a server call to get the creation time or exit time */
        /* This works on any thread */

        SERVER_START_REQ( get_thread_info )
        {
            req->handle = thread;
            req->tid_in = 0;
            if ((ret = !wine_server_call_err( req )))
            {
                if (creationtime)
                    RtlSecondsSince1970ToTime( reply->creation_time, (LARGE_INTEGER*)creationtime );
                if (exittime)
                    RtlSecondsSince1970ToTime( reply->exit_time, (LARGE_INTEGER*)exittime );
            }
        }
        SERVER_END_REQ;
    }
    if (ret && (kerneltime || usertime))
    {
        /* We call times(2) for kernel time or user time */
        /* We can only (portably) do this for the current thread */
        if (thread == GetCurrentThread())
        {
            ULONGLONG time;
            struct tms time_buf;
            long clocks_per_sec = sysconf(_SC_CLK_TCK);

            times(&time_buf);
            if (kerneltime)
            {
                time = (ULONGLONG)time_buf.tms_stime * 10000000 / clocks_per_sec;
                kerneltime->dwHighDateTime = time >> 32;
                kerneltime->dwLowDateTime = (DWORD)time;
            }
            if (usertime)
            {
                time = (ULONGLONG)time_buf.tms_utime * 10000000 / clocks_per_sec;
                usertime->dwHighDateTime = time >> 32;
                usertime->dwLowDateTime = (DWORD)time;
            }
        }
        else
        {
            if (kerneltime) kerneltime->dwHighDateTime = kerneltime->dwLowDateTime = 0;
            if (usertime) usertime->dwHighDateTime = usertime->dwLowDateTime = 0;
            FIXME("Cannot get kerneltime or usertime of other threads\n");
        }
    }
    return ret;
}


/**********************************************************************
 * VWin32_BoostThreadGroup [KERNEL.535]
 */
VOID WINAPI VWin32_BoostThreadGroup( DWORD threadId, INT boost )
{
    FIXME("(0x%08lx,%d): stub\n", threadId, boost);
}


/**********************************************************************
 * VWin32_BoostThreadStatic [KERNEL.536]
 */
VOID WINAPI VWin32_BoostThreadStatic( DWORD threadId, INT boost )
{
    FIXME("(0x%08lx,%d): stub\n", threadId, boost);
}


/***********************************************************************
 * GetCurrentThread [KERNEL32.@]  Gets pseudohandle for current thread
 *
 * RETURNS
 *    Pseudohandle for the current thread
 */
#undef GetCurrentThread
HANDLE WINAPI GetCurrentThread(void)
{
    return (HANDLE)0xfffffffe;
}


#ifdef __i386__

/***********************************************************************
 *		SetLastError (KERNEL.147)
 *		SetLastError (KERNEL32.@)
 */
/* void WINAPI SetLastError( DWORD error ); */
__ASM_GLOBAL_FUNC( SetLastError,
                   "movl 4(%esp),%eax\n\t"
                   ".byte 0x64\n\t"
                   "movl %eax,0x34\n\t"
                   "ret $4" );

/***********************************************************************
 *		GetLastError (KERNEL.148)
 *		GetLastError (KERNEL32.@)
 */
/* DWORD WINAPI GetLastError(void); */
__ASM_GLOBAL_FUNC( GetLastError, ".byte 0x64\n\tmovl 0x34,%eax\n\tret" );

/***********************************************************************
 *		GetCurrentProcessId (KERNEL.471)
 *		GetCurrentProcessId (KERNEL32.@)
 */
/* DWORD WINAPI GetCurrentProcessId(void) */
__ASM_GLOBAL_FUNC( GetCurrentProcessId, ".byte 0x64\n\tmovl 0x20,%eax\n\tret" );

/***********************************************************************
 *		GetCurrentThreadId (KERNEL.462)
 *		GetCurrentThreadId (KERNEL32.@)
 */
/* DWORD WINAPI GetCurrentThreadId(void) */
__ASM_GLOBAL_FUNC( GetCurrentThreadId, ".byte 0x64\n\tmovl 0x24,%eax\n\tret" );

#else  /* __i386__ */

/**********************************************************************
 *		SetLastError (KERNEL.147)
 *		SetLastError (KERNEL32.@)
 *
 * Sets the last-error code.
 */
void WINAPI SetLastError( DWORD error ) /* [in] Per-thread error code */
{
    NtCurrentTeb()->LastErrorValue = error;
}

/**********************************************************************
 *		GetLastError (KERNEL.148)
 *              GetLastError (KERNEL32.@)
 *
 * Returns last-error code.
 */
DWORD WINAPI GetLastError(void)
{
    return NtCurrentTeb()->LastErrorValue;
}

/***********************************************************************
 *		GetCurrentProcessId (KERNEL.471)
 *		GetCurrentProcessId (KERNEL32.@)
 *
 * Returns process identifier.
 */
DWORD WINAPI GetCurrentProcessId(void)
{
    return (DWORD)NtCurrentTeb()->ClientId.UniqueProcess;
}

/***********************************************************************
 *		GetCurrentThreadId (KERNEL.462)
 *		GetCurrentThreadId (KERNEL32.@)
 *
 * Returns thread identifier.
 */
DWORD WINAPI GetCurrentThreadId(void)
{
    return (DWORD)NtCurrentTeb()->ClientId.UniqueThread;
}

#endif  /* __i386__ */
