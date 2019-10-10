/*
 * Win32 VxD functions
 *
 * Copyright 1998 Marcus Meissner
 * Copyright 1998 Ulrich Weigand
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
#include "wine/port.h"

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winerror.h"
#include "file.h"
#include "kernel_private.h"
#include "wine/library.h"
#include "wine/unicode.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(vxd);

typedef BOOL (WINAPI *DeviceIoProc)(DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef DWORD (WINAPI *VxDCallProc)(DWORD, CONTEXT86 *);

struct vxd_module
{
    dev_t        dev;
    ino_t        ino;
    HANDLE       handle;
    HMODULE      module;
    DeviceIoProc proc;
};

struct vxdcall_service
{
    WCHAR       name[12];
    DWORD       service;
    HMODULE     module;
    VxDCallProc proc;
};

#define MAX_VXD_MODULES 32

static struct vxd_module vxd_modules[MAX_VXD_MODULES];

static struct vxdcall_service vxd_services[] =
{
    { {'v','m','m','.','v','x','d',0},             0x0001, NULL, NULL },
    { {'v','w','i','n','3','2','.','v','x','d',0}, 0x002a, NULL, NULL }
};

#define NB_VXD_SERVICES  (sizeof(vxd_services)/sizeof(vxd_services[0]))

static CRITICAL_SECTION vxd_section;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &vxd_section,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { 0, (DWORD)(__FILE__ ": vxd_section") }
};
static CRITICAL_SECTION vxd_section = { &critsect_debug, -1, 0, 0, 0, 0 };


/* create a file handle to represent a VxD, by opening a dummy file in the wineserver directory */
static HANDLE open_vxd_handle( LPCWSTR name )
{
    const char *dir = wine_get_server_dir();
    char *unix_name;
    int len1, len2;
    HANDLE ret;

    len1 = strlen( dir );
    len2 = WideCharToMultiByte( CP_UNIXCP, 0, name, -1, NULL, 0, NULL, NULL);
    if (!(unix_name = HeapAlloc( GetProcessHeap(), 0, len1 + len2 + 1 )))
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return 0;
    }
    strcpy( unix_name, dir );
    unix_name[len1] = '/';
    WideCharToMultiByte( CP_UNIXCP, 0, name, -1, unix_name + len1 + 1, len2, NULL, NULL);
    ret = FILE_CreateFile( unix_name, 0, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                           OPEN_ALWAYS, 0, 0, TRUE, DRIVE_FIXED );
    HeapFree( GetProcessHeap(), 0, unix_name );
    return ret;
}

/* retrieve the DeviceIoControl function for a Vxd given a file handle */
static DeviceIoProc get_vxd_proc( HANDLE handle )
{
    struct stat st;
    DeviceIoProc ret = NULL;
    int status, i, fd;

    status = wine_server_handle_to_fd( handle, 0, &fd, NULL, NULL );
    if (status)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        return NULL;
    }
    if (fstat( fd, &st ) == -1)
    {
        wine_server_release_fd( handle, fd );
        SetLastError( ERROR_INVALID_HANDLE );
        return NULL;
    }
    wine_server_release_fd( handle, fd );

    RtlEnterCriticalSection( &vxd_section );

    for (i = 0; i < MAX_VXD_MODULES; i++)
    {
        if (!vxd_modules[i].module) break;
        if (vxd_modules[i].dev == st.st_dev && vxd_modules[i].ino == st.st_ino)
        {
            if (!(ret = vxd_modules[i].proc)) SetLastError( ERROR_INVALID_FUNCTION );
            goto done;
        }
    }
    /* FIXME: Here we could go through the directory to find the VxD name and load it. */
    /* Let's wait to find out if there are actually apps out there that try to share   */
    /* VxD handles between processes, before we go to the trouble of implementing it.  */
    ERR( "handle %p not found in module list, inherited from another process?\n", handle );

done:
    RtlLeaveCriticalSection( &vxd_section );
    return ret;
}


/* load a VxD and return a file handle to it */
HANDLE VXD_Open( LPCWSTR filenameW, DWORD access, SECURITY_ATTRIBUTES *sa )
{
    static const WCHAR dotVxDW[] = {'.','v','x','d',0};
    int i;
    HANDLE handle;
    HMODULE module;
    WCHAR *p, name[16];

    if (!(GetVersion() & 0x80000000))  /* there are no VxDs on NT */
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }

    /* normalize the filename */

    if (strlenW( filenameW ) >= sizeof(name)/sizeof(WCHAR) - 4 ||
        strchrW( filenameW, '/' ) || strchrW( filenameW, '\\' ))
    {
        SetLastError( ERROR_FILE_NOT_FOUND );
        return 0;
    }
    strcpyW( name, filenameW );
    strlwrW( name );
    p = strchrW( name, '.' );
    if (!p) strcatW( name, dotVxDW );
    else if (strcmpW( p, dotVxDW ))  /* existing extension has to be .vxd */
    {
        SetLastError( ERROR_FILE_NOT_FOUND );
        return 0;
    }

    /* try to load the module first */

    if (!(module = LoadLibraryW( name )))
    {
        FIXME( "Unknown/unsupported VxD %s. Try setting Windows version to 'nt40' or 'win31'.\n",
               debugstr_w(name) );
        SetLastError( ERROR_FILE_NOT_FOUND );
        return 0;
    }

    /* register the module in the global list if necessary */

    RtlEnterCriticalSection( &vxd_section );

    for (i = 0; i < MAX_VXD_MODULES; i++)
    {
        if (vxd_modules[i].module == module)
        {
            handle = vxd_modules[i].handle;
            goto done;  /* already registered */
        }
        if (!vxd_modules[i].module)  /* new one, register it */
        {
            struct stat st;
            int fd;

            /* get a file handle to the dummy file */
            if (!(handle = open_vxd_handle( name )))
            {
                FreeLibrary( module );
                goto done;
            }
            wine_server_handle_to_fd( handle, 0, &fd, NULL, NULL );
            if (fstat( fd, &st ) != -1)
            {
                vxd_modules[i].dev = st.st_dev;
                vxd_modules[i].ino = st.st_ino;
            }
            vxd_modules[i].module = module;
            vxd_modules[i].handle = handle;
            vxd_modules[i].proc = (DeviceIoProc)GetProcAddress( module, "DeviceIoControl" );
            wine_server_release_fd( handle, fd );
            goto done;
        }
    }

    ERR("too many open VxD modules, please report\n" );
    CloseHandle( handle );
    FreeLibrary( module );
    handle = 0;

done:
    RtlLeaveCriticalSection( &vxd_section );
    if (!DuplicateHandle( GetCurrentProcess(), handle, GetCurrentProcess(), &handle, 0,
                          (sa && (sa->nLength>=sizeof(*sa)) && sa->bInheritHandle),
                          DUP_HANDLE_SAME_ACCESS ))
        handle = 0;
    return handle;
}


/***********************************************************************
 *		VxDCall0 (KERNEL32.1)
 *		VxDCall1 (KERNEL32.2)
 *		VxDCall2 (KERNEL32.3)
 *		VxDCall3 (KERNEL32.4)
 *		VxDCall4 (KERNEL32.5)
 *		VxDCall5 (KERNEL32.6)
 *		VxDCall6 (KERNEL32.7)
 *		VxDCall7 (KERNEL32.8)
 *		VxDCall8 (KERNEL32.9)
 */
void VxDCall( DWORD service, CONTEXT86 *context )
{
    int i;
    VxDCallProc proc = NULL;

    RtlEnterCriticalSection( &vxd_section );
    for (i = 0; i < NB_VXD_SERVICES; i++)
    {
        if (HIWORD(service) != vxd_services[i].service) continue;
        if (!vxd_services[i].module)  /* need to load it */
        {
            if ((vxd_services[i].module = LoadLibraryW( vxd_services[i].name )))
                vxd_services[i].proc = (VxDCallProc)GetProcAddress( vxd_services[i].module, "VxDCall" );
        }
        proc = vxd_services[i].proc;
        break;
    }
    RtlLeaveCriticalSection( &vxd_section );

    if (proc) context->Eax = proc( service, context );
    else
    {
        FIXME( "Unknown/unimplemented VxD (%08lx)\n", service);
        context->Eax = 0xffffffff; /* FIXME */
    }
}


/***********************************************************************
 *		OpenVxDHandle (KERNEL32.@)
 *
 *	This function is supposed to return the corresponding Ring 0
 *	("kernel") handle for a Ring 3 handle in Win9x.
 *	Evidently, Wine will have problems with this. But we try anyway,
 *	maybe it helps...
 */
HANDLE WINAPI OpenVxDHandle(HANDLE hHandleRing3)
{
    FIXME( "(%p), stub! (returning Ring 3 handle instead of Ring 0)\n", hHandleRing3);
    return hHandleRing3;
}


/****************************************************************************
 *		DeviceIoControl (KERNEL32.@)
 * This is one of those big ugly nasty procedure which can do
 * a million and one things when it comes to devices. It can also be
 * used for VxD communication.
 *
 * A return value of FALSE indicates that something has gone wrong which
 * GetLastError can decipher.
 */
BOOL WINAPI DeviceIoControl(HANDLE hDevice, DWORD dwIoControlCode,
                            LPVOID lpvInBuffer, DWORD cbInBuffer,
                            LPVOID lpvOutBuffer, DWORD cbOutBuffer,
                            LPDWORD lpcbBytesReturned,
                            LPOVERLAPPED lpOverlapped)
{
    NTSTATUS status;

    TRACE( "(%p,%lx,%p,%ld,%p,%ld,%p,%p)\n",
           hDevice,dwIoControlCode,lpvInBuffer,cbInBuffer,
           lpvOutBuffer,cbOutBuffer,lpcbBytesReturned,lpOverlapped );

    /* Check if this is a user defined control code for a VxD */

    if( HIWORD( dwIoControlCode ) == 0 )
    {
        DeviceIoProc proc = get_vxd_proc( hDevice );
        if (proc) return proc( dwIoControlCode, lpvInBuffer, cbInBuffer,
                               lpvOutBuffer, cbOutBuffer, lpcbBytesReturned, lpOverlapped );
        return FALSE;
    }

    /* Not a VxD, let ntdll handle it */

    if (lpOverlapped)
    {
        status = NtDeviceIoControlFile(hDevice, lpOverlapped->hEvent,
                                       NULL, NULL, (PIO_STATUS_BLOCK)lpOverlapped,
                                       dwIoControlCode, lpvInBuffer, cbInBuffer,
                                       lpvOutBuffer, cbOutBuffer);
        if (lpcbBytesReturned) *lpcbBytesReturned = lpOverlapped->InternalHigh;
    }
    else
    {
        IO_STATUS_BLOCK iosb;

        status = NtDeviceIoControlFile(hDevice, NULL, NULL, NULL, &iosb,
                                       dwIoControlCode, lpvInBuffer, cbInBuffer,
                                       lpvOutBuffer, cbOutBuffer);
        if (lpcbBytesReturned) *lpcbBytesReturned = iosb.Information;
    }
    if (status) SetLastError( RtlNtStatusToDosError(status) );
    return !status;
}
