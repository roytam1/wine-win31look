/*
 * Copyright 1999, 2000 Juergen Schmied
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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_ERRNO_H
#include <sys/errno.h>
#endif

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "wine/unicode.h"
#include "wine/debug.h"
#include "wine/server.h"
#include "async.h"
#include "ntdll_misc.h"

#include "winternl.h"
#include "winioctl.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);

/**************************************************************************
 *                 NtOpenFile				[NTDLL.@]
 *                 ZwOpenFile				[NTDLL.@]
 *
 * Open a file.
 *
 * PARAMS
 *  FileHandle       [O] Variable that receives the file handle on return
 *  DesiredAccess    [I] Access desired by the caller to the file
 *  ObjectAttributes [I] Structue describing the file to be opened
 *  IoStatusBlock    [O] Receives details about the result of the operation
 *  ShareAccess      [I] Type of shared access the caller requires
 *  OpenOptions      [I] Options for the file open
 *
 * RETURNS
 *  Success: 0. FileHandle and IoStatusBlock are updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtOpenFile(
	OUT PHANDLE FileHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	ULONG ShareAccess,
	ULONG OpenOptions)
{
	LPWSTR filename;
	static const WCHAR szDosDevices[] = {'\\','D','o','s','D','e','v','i','c','e','s','\\',0};

	FIXME("(%p,0x%08lx,%p,%p,0x%08lx,0x%08lx) partial stub\n",
		FileHandle, DesiredAccess, ObjectAttributes,
		IoStatusBlock, ShareAccess, OpenOptions);

	dump_ObjectAttributes (ObjectAttributes);

	if(ObjectAttributes->RootDirectory)
	{
		FIXME("Object root directory unknown %p\n",
			ObjectAttributes->RootDirectory);
		return STATUS_OBJECT_NAME_NOT_FOUND;
	}

	filename = ObjectAttributes->ObjectName->Buffer;

	/* FIXME: DOSFS stuff should call here, not vice-versa */
	if(strncmpW(filename, szDosDevices, strlenW(szDosDevices)))
		return STATUS_OBJECT_NAME_NOT_FOUND;

	/* FIXME: this calls SetLastError() -> bad */
        *FileHandle = pCreateFileW( &filename[strlenW(szDosDevices)], DesiredAccess, ShareAccess,
                                    NULL, OPEN_EXISTING, 0, 0 );
        if (*FileHandle == INVALID_HANDLE_VALUE) return STATUS_OBJECT_NAME_NOT_FOUND;
        return STATUS_SUCCESS;
}

/**************************************************************************
 *		NtCreateFile				[NTDLL.@]
 *		ZwCreateFile				[NTDLL.@]
 *
 * Either create a new file or directory, or open an existing file, device,
 * directory or volume.
 *
 * PARAMS
 *	FileHandle        [O] Points to a variable which receives the file handle on return
 *	DesiredAccess     [I] Desired access to the file
 *	ObjectAttributes  [I] Structure describing the file
 *	IoStatusBlock     [O] Receives information about the operation on return
 *	AllocationSize    [I] Initial size of the file in bytes
 *	FileAttributes    [I] Attributes to create the file with
 *	ShareAccess       [I] Type of shared access the caller would like to the file
 *	CreateDisposition [I] Specifies what to do, depending on whether the file already exists
 *	CreateOptions     [I] Options for creating a new file
 *	EaBuffer          [I] Undocumented
 *	EaLength          [I] Undocumented
 *
 * RETURNS
 *  Success: 0. FileHandle and IoStatusBlock are updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtCreateFile(
	OUT PHANDLE FileHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	PLARGE_INTEGER AllocateSize,
	ULONG FileAttributes,
	ULONG ShareAccess,
	ULONG CreateDisposition,
	ULONG CreateOptions,
	PVOID EaBuffer,
	ULONG EaLength)
{
	FIXME("(%p,0x%08lx,%p,%p,%p,0x%08lx,0x%08lx,0x%08lx,0x%08lx,%p,0x%08lx) stub\n",
	FileHandle,DesiredAccess,ObjectAttributes,
	IoStatusBlock,AllocateSize,FileAttributes,
	ShareAccess,CreateDisposition,CreateOptions,EaBuffer,EaLength);
	dump_ObjectAttributes (ObjectAttributes);
	return 0;
}

/***********************************************************************
 *                  Asynchronous file I/O                              *
 */
static DWORD fileio_get_async_count(const async_private *ovp);
static void CALLBACK fileio_call_completion_func(ULONG_PTR data);
static void fileio_async_cleanup(async_private *ovp);

static async_ops fileio_async_ops =
{
    fileio_get_async_count,        /* get_count */
    fileio_call_completion_func,   /* call_completion */
    fileio_async_cleanup           /* cleanup */
};

static async_ops fileio_nocomp_async_ops =
{
    fileio_get_async_count,        /* get_count */
    NULL,                          /* call_completion */
    fileio_async_cleanup           /* cleanup */
};

typedef struct async_fileio
{
    struct async_private             async;
    PIO_APC_ROUTINE                  apc;
    void*                            apc_user;
    char                             *buffer;
    unsigned int                     count;
    unsigned long                    offset;
    enum fd_type                     fd_type;
} async_fileio;

static DWORD fileio_get_async_count(const struct async_private *ovp)
{
    async_fileio *fileio = (async_fileio*) ovp;

    if (fileio->count < fileio->async.iosb->Information)
    	return 0;
    return fileio->count - fileio->async.iosb->Information;
}

static void CALLBACK fileio_call_completion_func(ULONG_PTR data)
{
    async_fileio *ovp = (async_fileio*) data;
    TRACE("data: %p\n", ovp);

    ovp->apc( ovp->apc_user, ovp->async.iosb, ovp->async.iosb->Information );

    fileio_async_cleanup( &ovp->async );
}

static void fileio_async_cleanup( struct async_private *ovp )
{
    RtlFreeHeap( GetProcessHeap(), 0, ovp );
}

/***********************************************************************
 *           FILE_GetNtStatus(void)
 *
 * Retrieve the Nt Status code from errno.
 * Try to be consistent with FILE_SetDosError().
 */
NTSTATUS FILE_GetNtStatus(void)
{
    int err = errno;
    DWORD nt;

    TRACE( "errno = %d\n", errno );
    switch (err)
    {
    case EAGAIN:       nt = STATUS_SHARING_VIOLATION;       break;
    case EBADF:        nt = STATUS_INVALID_HANDLE;          break;
    case ENOSPC:       nt = STATUS_DISK_FULL;               break;
    case EPERM:
    case EROFS:
    case EACCES:       nt = STATUS_ACCESS_DENIED;           break;
    case ENOENT:       nt = STATUS_SHARING_VIOLATION;       break;
    case EISDIR:       nt = STATUS_FILE_IS_A_DIRECTORY;     break;
    case EMFILE:
    case ENFILE:       nt = STATUS_NO_MORE_FILES;           break;
    case EINVAL:
    case ENOTEMPTY:    nt = STATUS_DIRECTORY_NOT_EMPTY;     break;
    case EPIPE:        nt = STATUS_PIPE_BROKEN;             break;
    case EIO:          nt = STATUS_DEVICE_NOT_READY;        break;
    case ENOEXEC:      /* ?? */
    case ESPIPE:       /* ?? */
    case EEXIST:       /* ?? */
    default:
        FIXME( "Converting errno %d to STATUS_UNSUCCESSFUL\n", err );
        nt = STATUS_UNSUCCESSFUL;
    }
    return nt;
}

/***********************************************************************
 *             FILE_AsyncReadService      (INTERNAL)
 *
 *  This function is called while the client is waiting on the
 *  server, so we can't make any server calls here.
 */
static void FILE_AsyncReadService(async_private *ovp)
{
    async_fileio *fileio = (async_fileio*) ovp;
    IO_STATUS_BLOCK*  io_status = fileio->async.iosb;
    int result;
    int already = io_status->Information;

    TRACE("%p %p\n", io_status, fileio->buffer );

    /* check to see if the data is ready (non-blocking) */

    if ( fileio->fd_type == FD_TYPE_SOCKET )
        result = read(ovp->fd, &fileio->buffer[already], fileio->count - already);
    else
    {
        result = pread(ovp->fd, &fileio->buffer[already], fileio->count - already,
                       fileio->offset + already);
        if ((result < 0) && (errno == ESPIPE))
            result = read(ovp->fd, &fileio->buffer[already], fileio->count - already);
    }

    if ((result < 0) && ((errno == EAGAIN) || (errno == EINTR)))
    {
        TRACE("Deferred read %d\n",errno);
        io_status->u.Status = STATUS_PENDING;
        return;
    }

    /* check to see if the transfer is complete */
    if (result < 0)
    {
        io_status->u.Status = FILE_GetNtStatus();
        return;
    }
    else if (result == 0)
    {
        io_status->u.Status = io_status->Information ? STATUS_SUCCESS : STATUS_END_OF_FILE;
        return;
    }

    io_status->Information += result;
    if (io_status->Information >= fileio->count || fileio->fd_type == FD_TYPE_SOCKET )
        io_status->u.Status = STATUS_SUCCESS;
    else
        io_status->u.Status = STATUS_PENDING;

    TRACE("read %d more bytes %ld/%d so far\n",
          result, io_status->Information, fileio->count);
}


/******************************************************************************
 *  NtReadFile					[NTDLL.@]
 *  ZwReadFile					[NTDLL.@]
 *
 * Read from an open file handle.
 *
 * PARAMS
 *  FileHandle    [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  Event         [I] Event to signal upon completion (or NULL)
 *  ApcRoutine    [I] Callback to call upon completion (or NULL)
 *  ApcContext    [I] Context for ApcRoutine (or NULL)
 *  IoStatusBlock [O] Receives information about the operation on return
 *  Buffer        [O] Destination for the data read
 *  Length        [I] Size of Buffer
 *  ByteOffset    [O] Destination for the new file pointer position (or NULL)
 *  Key           [O] Function unknown (may be NULL)
 *
 * RETURNS
 *  Success: 0. IoStatusBlock is updated, and the Information member contains
 *           The number of bytes read.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtReadFile(HANDLE hFile, HANDLE hEvent,
                           PIO_APC_ROUTINE apc, void* apc_user,
                           PIO_STATUS_BLOCK io_status, void* buffer, ULONG length,
                           PLARGE_INTEGER offset, PULONG key)
{
    int unix_handle, flags;
    enum fd_type type;

    TRACE("(%p,%p,%p,%p,%p,%p,0x%08lx,%p,%p),partial stub!\n",
          hFile,hEvent,apc,apc_user,io_status,buffer,length,offset,key);

    io_status->Information = 0;
    io_status->u.Status = wine_server_handle_to_fd( hFile, GENERIC_READ, &unix_handle, &type, &flags );
    if (io_status->u.Status) return io_status->u.Status;

    if (flags & FD_FLAG_RECV_SHUTDOWN)
    {
        wine_server_release_fd( hFile, unix_handle );
        return STATUS_PIPE_DISCONNECTED;
    }

    if (flags & FD_FLAG_TIMEOUT)
    {
        if (hEvent)
        {
            /* this shouldn't happen, but check it */
            FIXME("NIY-hEvent\n");
            wine_server_release_fd( hFile, unix_handle );
            return STATUS_NOT_IMPLEMENTED;
        }
        io_status->u.Status = NtCreateEvent(&hEvent, SYNCHRONIZE, NULL, 0, 0);
        if (io_status->u.Status)
        {
            wine_server_release_fd( hFile, unix_handle );
            return io_status->u.Status;
        }
    }

    if (flags & (FD_FLAG_OVERLAPPED|FD_FLAG_TIMEOUT))
    {
        async_fileio*   ovp;
        NTSTATUS ret;

        if (!(ovp = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(async_fileio))))
        {
            wine_server_release_fd( hFile, unix_handle );
            return STATUS_NO_MEMORY;
        }
        ovp->async.ops = (apc ? &fileio_async_ops : &fileio_nocomp_async_ops );
        ovp->async.handle = hFile;
        ovp->async.fd = unix_handle;  /* FIXME */
        ovp->async.type = ASYNC_TYPE_READ;
        ovp->async.func = FILE_AsyncReadService;
        ovp->async.event = hEvent;
        ovp->async.iosb = io_status;
        ovp->count = length;
        if ( offset == NULL ) 
            ovp->offset = 0;
        else
        {
            ovp->offset = offset->u.LowPart;
            if (offset->u.HighPart) FIXME("NIY-high part\n");
        } 
        ovp->apc = apc;
        ovp->apc_user = apc_user;
        ovp->buffer = buffer;
        ovp->fd_type = type;

        io_status->Information = 0;
        ret = register_new_async(&ovp->async);
        if (ret != STATUS_SUCCESS)
            return ret;
        if (flags & FD_FLAG_TIMEOUT)
        {
            NtWaitForSingleObject(hEvent, TRUE, NULL);
            NtClose(hEvent);
        }
        else
        {
            LARGE_INTEGER   timeout;

            /* let some APC be run, this will read some already pending data */
            timeout.u.LowPart = timeout.u.HighPart = 0;
            NtDelayExecution( TRUE, &timeout );
        }
        return io_status->u.Status;
    }
    switch (type)
    {
    case FD_TYPE_SMB:
        FIXME("NIY-SMB\n");
        /* FIXME */
        /* return SMB_ReadFile(hFile, unix_handle, buffer, length, io_status); */
        wine_server_release_fd( hFile, unix_handle );
        return STATUS_INVALID_HANDLE;

    case FD_TYPE_DEFAULT:
        /* normal unix file */
        break;

    default:
        FIXME("Unsupported type of fd %d\n", type);
        wine_server_release_fd( hFile, unix_handle );
        return STATUS_INVALID_HANDLE;
    }

    if (offset)
    {
        FILE_POSITION_INFORMATION   fpi;

        fpi.CurrentByteOffset = *offset;
        io_status->u.Status = NtSetInformationFile(hFile, io_status, &fpi, sizeof(fpi), 
                                                   FilePositionInformation);
        if (io_status->u.Status)
        {
            wine_server_release_fd( hFile, unix_handle );
            return io_status->u.Status;
        }
    }
    /* code for synchronous reads */
    while ((io_status->Information = read( unix_handle, buffer, length )) == -1)
    {
        if ((errno == EAGAIN) || (errno == EINTR)) continue;
        if (errno == EFAULT) FIXME( "EFAULT handling broken for now\n" );
	io_status->u.Status = FILE_GetNtStatus();
	break;
    }
    wine_server_release_fd( hFile, unix_handle );
    return io_status->u.Status;
}

/***********************************************************************
 *             FILE_AsyncWriteService      (INTERNAL)
 *
 *  This function is called while the client is waiting on the
 *  server, so we can't make any server calls here.
 */
static void FILE_AsyncWriteService(struct async_private *ovp)
{
    async_fileio *fileio = (async_fileio *) ovp;
    PIO_STATUS_BLOCK io_status = fileio->async.iosb;
    int result;
    int already = io_status->Information;

    TRACE("(%p %p)\n",io_status,fileio->buffer);

    /* write some data (non-blocking) */

    if ( fileio->fd_type == FD_TYPE_SOCKET )
        result = write(ovp->fd, &fileio->buffer[already], fileio->count - already);
    else
    {
        result = pwrite(ovp->fd, &fileio->buffer[already], fileio->count - already,
                        fileio->offset + already);
        if ((result < 0) && (errno == ESPIPE))
            result = write(ovp->fd, &fileio->buffer[already], fileio->count - already);
    }

    if ((result < 0) && ((errno == EAGAIN) || (errno == EINTR)))
    {
        io_status->u.Status = STATUS_PENDING;
        return;
    }

    /* check to see if the transfer is complete */
    if (result < 0)
    {
        io_status->u.Status = FILE_GetNtStatus();
        return;
    }

    io_status->Information += result;
    io_status->u.Status = (io_status->Information < fileio->count) ? STATUS_PENDING : STATUS_SUCCESS;
    TRACE("wrote %d more bytes %ld/%d so far\n",result,io_status->Information,fileio->count);
}

/******************************************************************************
 *  NtWriteFile					[NTDLL.@]
 *  ZwWriteFile					[NTDLL.@]
 *
 * Write to an open file handle.
 *
 * PARAMS
 *  FileHandle    [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  Event         [I] Event to signal upon completion (or NULL)
 *  ApcRoutine    [I] Callback to call upon completion (or NULL)
 *  ApcContext    [I] Context for ApcRoutine (or NULL)
 *  IoStatusBlock [O] Receives information about the operation on return
 *  Buffer        [I] Source for the data to write
 *  Length        [I] Size of Buffer
 *  ByteOffset    [O] Destination for the new file pointer position (or NULL)
 *  Key           [O] Function unknown (may be NULL)
 *
 * RETURNS
 *  Success: 0. IoStatusBlock is updated, and the Information member contains
 *           The number of bytes written.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtWriteFile(HANDLE hFile, HANDLE hEvent,
                            PIO_APC_ROUTINE apc, void* apc_user,
                            PIO_STATUS_BLOCK io_status, 
                            const void* buffer, ULONG length,
                            PLARGE_INTEGER offset, PULONG key)
{
    int unix_handle, flags;
    enum fd_type type;

    TRACE("(%p,%p,%p,%p,%p,%p,0x%08lx,%p,%p)!\n",
          hFile,hEvent,apc,apc_user,io_status,buffer,length,offset,key);

    TRACE("(%p,%p,%p,%p,%p,%p,0x%08lx,%p,%p),partial stub!\n",
          hFile,hEvent,apc,apc_user,io_status,buffer,length,offset,key);

    io_status->Information = 0;
    io_status->u.Status = wine_server_handle_to_fd( hFile, GENERIC_WRITE, &unix_handle, &type, &flags );
    if (io_status->u.Status) return io_status->u.Status;

    if (flags & FD_FLAG_SEND_SHUTDOWN)
    {
        wine_server_release_fd( hFile, unix_handle );
        return STATUS_PIPE_DISCONNECTED;
    }

    if (flags & (FD_FLAG_OVERLAPPED|FD_FLAG_TIMEOUT))
    {
        async_fileio*   ovp;
        NTSTATUS ret;

        if (!(ovp = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(async_fileio))))
        {
            wine_server_release_fd( hFile, unix_handle );
            return STATUS_NO_MEMORY;
        }
        ovp->async.ops = (apc ? &fileio_async_ops : &fileio_nocomp_async_ops );
        ovp->async.handle = hFile;
        ovp->async.fd = unix_handle;  /* FIXME */
        ovp->async.type = ASYNC_TYPE_WRITE;
        ovp->async.func = FILE_AsyncWriteService;
        ovp->async.event = hEvent;
        ovp->async.iosb = io_status;
        ovp->count = length;
        if (offset) {
            ovp->offset = offset->u.LowPart;
            if (offset->u.HighPart) FIXME("NIY-high part\n");
        } else {
            ovp->offset = 0;
        }
        ovp->apc = apc;
        ovp->apc_user = apc_user;
        ovp->buffer = (void*)buffer;
        ovp->fd_type = type;

        io_status->Information = 0;
        ret = register_new_async(&ovp->async);
        if (ret != STATUS_SUCCESS)
            return ret;
        if (flags & FD_FLAG_TIMEOUT)
        {
            NtWaitForSingleObject(hEvent, TRUE, NULL);
            NtClose(hEvent);
        }
        else
        {
            LARGE_INTEGER   timeout;

            /* let some APC be run, this will write as much data as possible */
            timeout.u.LowPart = timeout.u.HighPart = 0;
            NtDelayExecution( TRUE, &timeout );
        }
        return io_status->u.Status;
    }
    switch (type)
    {
    case FD_TYPE_SMB:
        FIXME("NIY-SMB\n");
        wine_server_release_fd( hFile, unix_handle );
        return STATUS_NOT_IMPLEMENTED;

    case FD_TYPE_DEFAULT:
        /* normal unix files */
        if (unix_handle == -1) return STATUS_INVALID_HANDLE;
        break;

    default:
        FIXME("Unsupported type of fd %d\n", type);
        wine_server_release_fd( hFile, unix_handle );
        return STATUS_INVALID_HANDLE;
    }

    if (offset)
    {
        FILE_POSITION_INFORMATION   fpi;

        fpi.CurrentByteOffset = *offset;
        io_status->u.Status = NtSetInformationFile(hFile, io_status, &fpi, sizeof(fpi),
                                                   FilePositionInformation);
        if (io_status->u.Status)
        {
            wine_server_release_fd( hFile, unix_handle );
            return io_status->u.Status;
        }
    }

    /* synchronous file write */
    while ((io_status->Information = write( unix_handle, buffer, length )) == -1)
    {
        if ((errno == EAGAIN) || (errno == EINTR)) continue;
        if (errno == EFAULT) FIXME( "EFAULT handling broken for now\n" );
        if (errno == ENOSPC) io_status->u.Status = STATUS_DISK_FULL;
        else io_status->u.Status = FILE_GetNtStatus();
        break;
    }
    wine_server_release_fd( hFile, unix_handle );
    return io_status->u.Status;
}

/**************************************************************************
 *		NtDeviceIoControlFile			[NTDLL.@]
 *		ZwDeviceIoControlFile			[NTDLL.@]
 *
 * Perform an I/O control operation on an open file handle.
 *
 * PARAMS
 *  DeviceHandle     [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  Event            [I] Event to signal upon completion (or NULL)
 *  ApcRoutine       [I] Callback to call upon completion (or NULL)
 *  ApcContext       [I] Context for ApcRoutine (or NULL)
 *  IoStatusBlock    [O] Receives information about the operation on return
 *  IoControlCode    [I] Control code for the operation to perform
 *  InputBuffer      [I] Source for any input data required (or NULL)
 *  InputBufferSize  [I] Size of InputBuffer
 *  OutputBuffer     [O] Source for any output data returned (or NULL)
 *  OutputBufferSize [I] Size of OutputBuffer
 *
 * RETURNS
 *  Success: 0. IoStatusBlock is updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtDeviceIoControlFile(HANDLE DeviceHandle, HANDLE hEvent,
                                      PIO_APC_ROUTINE UserApcRoutine, 
                                      PVOID UserApcContext,
                                      PIO_STATUS_BLOCK IoStatusBlock,
                                      ULONG IoControlCode,
                                      PVOID InputBuffer,
                                      ULONG InputBufferSize,
                                      PVOID OutputBuffer,
                                      ULONG OutputBufferSize)
{
    TRACE("(%p,%p,%p,%p,%p,0x%08lx,%p,0x%08lx,%p,0x%08lx)\n",
          DeviceHandle, hEvent, UserApcRoutine, UserApcContext,
          IoStatusBlock, IoControlCode, 
          InputBuffer, InputBufferSize, OutputBuffer, OutputBufferSize);

    if (CDROM_DeviceIoControl(DeviceHandle, hEvent,
                              UserApcRoutine, UserApcContext,
                              IoStatusBlock, IoControlCode,
                              InputBuffer, InputBufferSize,
                              OutputBuffer, OutputBufferSize) == STATUS_NO_SUCH_DEVICE)
    {
        /* it wasn't a CDROM */
        FIXME("Unimplemented dwIoControlCode=%08lx\n", IoControlCode);
        IoStatusBlock->u.Status = STATUS_NOT_IMPLEMENTED;
        IoStatusBlock->Information = 0;
        if (hEvent) NtSetEvent(hEvent, NULL);
    }
    return IoStatusBlock->u.Status;
}

/******************************************************************************
 * NtFsControlFile [NTDLL.@]
 * ZwFsControlFile [NTDLL.@]
 */
NTSTATUS WINAPI NtFsControlFile(
	IN HANDLE DeviceHandle,
	IN HANDLE Event OPTIONAL,
	IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
	IN PVOID ApcContext OPTIONAL,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	IN ULONG IoControlCode,
	IN PVOID InputBuffer,
	IN ULONG InputBufferSize,
	OUT PVOID OutputBuffer,
	IN ULONG OutputBufferSize)
{
	FIXME("(%p,%p,%p,%p,%p,0x%08lx,%p,0x%08lx,%p,0x%08lx): stub\n",
	DeviceHandle,Event,ApcRoutine,ApcContext,IoStatusBlock,IoControlCode,
	InputBuffer,InputBufferSize,OutputBuffer,OutputBufferSize);
	return 0;
}

/******************************************************************************
 *  NtSetVolumeInformationFile		[NTDLL.@]
 *  ZwSetVolumeInformationFile		[NTDLL.@]
 *
 * Set volume information for an open file handle.
 *
 * PARAMS
 *  FileHandle         [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  IoStatusBlock      [O] Receives information about the operation on return
 *  FsInformation      [I] Source for volume information
 *  Length             [I] Size of FsInformation
 *  FsInformationClass [I] Type of volume information to set
 *
 * RETURNS
 *  Success: 0. IoStatusBlock is updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtSetVolumeInformationFile(
	IN HANDLE FileHandle,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID FsInformation,
        ULONG Length,
	FS_INFORMATION_CLASS FsInformationClass)
{
	FIXME("(%p,%p,%p,0x%08lx,0x%08x) stub\n",
	FileHandle,IoStatusBlock,FsInformation,Length,FsInformationClass);
	return 0;
}

/******************************************************************************
 *  NtQueryInformationFile		[NTDLL.@]
 *  ZwQueryInformationFile		[NTDLL.@]
 *
 * Get information about an open file handle.
 *
 * PARAMS
 *  FileHandle           [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  IoStatusBlock        [O] Receives information about the operation on return
 *  FileInformation      [O] Destination for file information
 *  Length               [I] Size of FileInformation
 *  FileInformationClass [I] Type of file information to get
 *
 * RETURNS
 *  Success: 0. IoStatusBlock and FileInformation are updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtQueryInformationFile(HANDLE hFile, PIO_STATUS_BLOCK io_status,
                                       PVOID ptr, LONG len,
                                       FILE_INFORMATION_CLASS class)
{
    NTSTATUS    status;
    LONG        used = 0;
    BYTE        answer[256];
    time_t      ct = 0, wt = 0, at = 0;

    TRACE("(%p,%p,%p,0x%08lx,0x%08x)\n", hFile, io_status, ptr, len, class);

    switch (class)
    {
    case FileBasicInformation:
        {
            FILE_BASIC_INFORMATION*  fbi = (FILE_BASIC_INFORMATION*)answer;
            if (sizeof(answer) < sizeof(*fbi)) goto too_small;

            SERVER_START_REQ( get_file_info )
            {
                req->handle = hFile;
                if (!(status = wine_server_call( req )))
                {
                    /* FIXME: which file types are supported ?
                     * Serial ports (FILE_TYPE_CHAR) are not,
                     * and MSDN also says that pipes are not supported.
                     * FILE_TYPE_REMOTE seems to be supported according to
                     * MSDN q234741.txt */
                    if ((reply->type == FILE_TYPE_DISK) ||
                        (reply->type == FILE_TYPE_REMOTE))
                    {
                        at = reply->access_time;
                        wt = reply->write_time;
                        ct = reply->change_time;
                        fbi->FileAttributes = reply->attr;
                        used = sizeof(*fbi);
                    }
                    else status = STATUS_INVALID_HANDLE; /* FIXME ??? */
                }
            }
            SERVER_END_REQ;
            if (used)
            {
                RtlSecondsSince1970ToTime(wt, &fbi->CreationTime);
                RtlSecondsSince1970ToTime(wt, &fbi->LastWriteTime);
                RtlSecondsSince1970ToTime(ct, &fbi->ChangeTime);
                RtlSecondsSince1970ToTime(at, &fbi->LastAccessTime);
            }
        }
        break;
    case FileStandardInformation:
        {
            FILE_STANDARD_INFORMATION*  fsi = (FILE_STANDARD_INFORMATION*)answer;
            if (sizeof(answer) < sizeof(*fsi)) goto too_small;

            SERVER_START_REQ( get_file_info )
            {
                req->handle = hFile;
                if (!(status = wine_server_call( req )))
                {
                    /* FIXME: which file types are supported ?
                     * Serial ports (FILE_TYPE_CHAR) are not,
                     * and MSDN also says that pipes are not supported.
                     * FILE_TYPE_REMOTE seems to be supported according to
                     * MSDN q234741.txt */
                    if ((reply->type == FILE_TYPE_DISK) ||
                        (reply->type == FILE_TYPE_REMOTE))
                    {
                        fsi->AllocationSize.u.HighPart = reply->alloc_high;
                        fsi->AllocationSize.u.LowPart  = reply->alloc_low;
                        fsi->EndOfFile.u.HighPart      = reply->size_high;
                        fsi->EndOfFile.u.LowPart       = reply->size_low;
                        fsi->NumberOfLinks             = reply->links;
                        fsi->DeletePending             = FALSE; /* FIXME */
                        fsi->Directory                 = (reply->attr & FILE_ATTRIBUTE_DIRECTORY);
                        used = sizeof(*fsi);
                    }
                    else status = STATUS_INVALID_HANDLE; /* FIXME ??? */
                }
            }
            SERVER_END_REQ;
        }
        break;
    case FilePositionInformation:
        {
            FILE_POSITION_INFORMATION*  fpi = (FILE_POSITION_INFORMATION*)answer;
            if (sizeof(answer) < sizeof(*fpi)) goto too_small;

            SERVER_START_REQ( set_file_pointer )
            {
                req->handle = hFile;
                req->low = 0;
                req->high = 0;
                req->whence = SEEK_CUR;
                if (!(status = wine_server_call( req )))
                {
                    fpi->CurrentByteOffset.u.HighPart = reply->new_high;
                    fpi->CurrentByteOffset.u.LowPart  = reply->new_low;
                    used = sizeof(*fpi);
                }
            }
            SERVER_END_REQ;
        }
        break;
    default:
        FIXME("Unsupported class (%d)\n", class);
        return io_status->u.Status = STATUS_NOT_IMPLEMENTED;
    }
    if (used) memcpy(ptr, answer, min(used, len));
    io_status->u.Status = status;
    io_status->Information = len;
    return status;
 too_small:
    io_status->Information = 0;
    return io_status->u.Status = STATUS_BUFFER_TOO_SMALL;
}

/******************************************************************************
 *  NtSetInformationFile		[NTDLL.@]
 *  ZwSetInformationFile		[NTDLL.@]
 *
 * Set information about an open file handle.
 *
 * PARAMS
 *  FileHandle           [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  IoStatusBlock        [O] Receives information about the operation on return
 *  FileInformation      [I] Source for file information
 *  Length               [I] Size of FileInformation
 *  FileInformationClass [I] Type of file information to set
 *
 * RETURNS
 *  Success: 0. IoStatusBlock is updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtSetInformationFile(HANDLE hFile, PIO_STATUS_BLOCK io_status,
                                     PVOID ptr, ULONG len,
                                     FILE_INFORMATION_CLASS class)
{
    NTSTATUS    status = STATUS_INVALID_PARAMETER_3;

    TRACE("(%p,%p,%p,0x%08lx,0x%08x)\n", hFile, io_status, ptr, len, class);

    switch (class)
    {
    case FilePositionInformation:
        if (len >= sizeof(FILE_POSITION_INFORMATION))
        {
            FILE_POSITION_INFORMATION*  fpi = (FILE_POSITION_INFORMATION*)ptr;

            SERVER_START_REQ( set_file_pointer )
            {
                req->handle = hFile;
                req->low = fpi->CurrentByteOffset.u.LowPart;
                req->high = fpi->CurrentByteOffset.u.HighPart;
                req->whence = SEEK_SET;
                status = wine_server_call( req );
            }
            SERVER_END_REQ;
            status = STATUS_SUCCESS;
        }
        break;
    default:
        FIXME("Unsupported class (%d)\n", class);
        return STATUS_NOT_IMPLEMENTED;
    }
    io_status->u.Status = status;
    io_status->Information = 0;
    return status;
}

/******************************************************************************
 *  NtQueryVolumeInformationFile		[NTDLL.@]
 *  ZwQueryVolumeInformationFile		[NTDLL.@]
 *
 * Get volume information for an open file handle.
 *
 * PARAMS
 *  FileHandle         [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  IoStatusBlock      [O] Receives information about the operation on return
 *  FsInformation      [O] Destination for volume information
 *  Length             [I] Size of FsInformation
 *  FsInformationClass [I] Type of volume information to set
 *
 * RETURNS
 *  Success: 0. IoStatusBlock and FsInformation are updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtQueryVolumeInformationFile (
	IN HANDLE FileHandle,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	OUT PVOID FSInformation,
	IN ULONG Length,
	IN FS_INFORMATION_CLASS FSInformationClass)
{
	ULONG len = 0;

	FIXME("(%p %p %p 0x%08lx 0x%08x) stub!\n",
	FileHandle, IoStatusBlock, FSInformation, Length, FSInformationClass);

	switch ( FSInformationClass )
	{
	  case FileFsVolumeInformation:
	    len = sizeof( FILE_FS_VOLUME_INFORMATION );
	    break;
	  case FileFsLabelInformation:
	    len = 0;
	    break;

	  case FileFsSizeInformation:
	    len = sizeof( FILE_FS_SIZE_INFORMATION );
	    break;

	  case FileFsDeviceInformation:
	    len = sizeof( FILE_FS_DEVICE_INFORMATION );
	    break;
	  case FileFsAttributeInformation:
	    len = sizeof( FILE_FS_ATTRIBUTE_INFORMATION );
	    break;

	  case FileFsControlInformation:
	    len = 0;
	    break;

	  case FileFsFullSizeInformation:
	    len = 0;
	    break;

	  case FileFsObjectIdInformation:
	    len = 0;
	    break;

	  case FileFsMaximumInformation:
	    len = 0;
	    break;
	}

	if (Length < len)
	  return STATUS_BUFFER_TOO_SMALL;

	switch ( FSInformationClass )
	{
	  case FileFsVolumeInformation:
	    break;
	  case FileFsLabelInformation:
	    break;

	  case FileFsSizeInformation:
	    break;

	  case FileFsDeviceInformation:
	    if (FSInformation)
	    {
	      FILE_FS_DEVICE_INFORMATION * DeviceInfo = FSInformation;
	      DeviceInfo->DeviceType = FILE_DEVICE_DISK;
	      DeviceInfo->Characteristics = 0;
	      break;
	    };
	  case FileFsAttributeInformation:
	    break;

	  case FileFsControlInformation:
	    break;

	  case FileFsFullSizeInformation:
	    break;

	  case FileFsObjectIdInformation:
	    break;

	  case FileFsMaximumInformation:
	    break;
	}
	IoStatusBlock->u.Status = STATUS_SUCCESS;
	IoStatusBlock->Information = len;
	return STATUS_SUCCESS;
}

/******************************************************************
 *		NtFlushBuffersFile  (NTDLL.@)
 *
 * Flush any buffered data on an open file handle.
 *
 * PARAMS
 *  FileHandle         [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  IoStatusBlock      [O] Receives information about the operation on return
 *
 * RETURNS
 *  Success: 0. IoStatusBlock is updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtFlushBuffersFile( HANDLE hFile, IO_STATUS_BLOCK* IoStatusBlock )
{
    NTSTATUS ret;
    HANDLE hEvent = NULL;

    SERVER_START_REQ( flush_file )
    {
        req->handle = hFile;
        ret = wine_server_call( req );
        hEvent = reply->event;
    }
    SERVER_END_REQ;
    if (!ret && hEvent)
    {
        ret = NtWaitForSingleObject( hEvent, FALSE, NULL );
        NtClose( hEvent );
    }
    return ret;
}

/******************************************************************
 *		NtLockFile       (NTDLL.@)
 *
 *
 */
NTSTATUS WINAPI NtLockFile( HANDLE hFile, HANDLE lock_granted_event,
                            PIO_APC_ROUTINE apc, void* apc_user,
                            PIO_STATUS_BLOCK io_status, PLARGE_INTEGER offset,
                            PLARGE_INTEGER count, ULONG* key, BOOLEAN dont_wait,
                            BOOLEAN exclusive )
{
    NTSTATUS    ret;
    HANDLE      handle;
    BOOLEAN     async;

    if (apc || io_status || key)
    {
        FIXME("Unimplemented yet parameter\n");
        return STATUS_NOT_IMPLEMENTED;
    }

    for (;;)
    {
        SERVER_START_REQ( lock_file )
        {
            req->handle      = hFile;
            req->offset_low  = offset->u.LowPart;
            req->offset_high = offset->u.HighPart;
            req->count_low   = count->u.LowPart;
            req->count_high  = count->u.HighPart;
            req->shared      = !exclusive;
            req->wait        = !dont_wait;
            ret = wine_server_call( req );
            handle = reply->handle;
            async  = reply->overlapped;
        }
        SERVER_END_REQ;
        if (ret != STATUS_PENDING)
        {
            if (!ret && lock_granted_event) NtSetEvent(lock_granted_event, NULL);
            return ret;
        }

        if (async)
        {
            FIXME( "Async I/O lock wait not implemented, might deadlock\n" );
            if (handle) NtClose( handle );
            return STATUS_PENDING;
        }
        if (handle)
        {
            NtWaitForSingleObject( handle, FALSE, NULL );
            NtClose( handle );
        }
        else
        {
            LARGE_INTEGER time;
    
            /* Unix lock conflict, sleep a bit and retry */
            time.QuadPart = 100 * (ULONGLONG)10000;
            time.QuadPart = -time.QuadPart;
            NtDelayExecution( FALSE, &time );
        }
    }
}


/******************************************************************
 *		NtUnlockFile    (NTDLL.@)
 *
 *
 */
NTSTATUS WINAPI NtUnlockFile( HANDLE hFile, PIO_STATUS_BLOCK io_status,
                              PLARGE_INTEGER offset, PLARGE_INTEGER count,
                              PULONG key )
{
    NTSTATUS status;

    TRACE( "%p %lx%08lx %lx%08lx\n",
           hFile, offset->u.HighPart, offset->u.LowPart, count->u.HighPart, count->u.LowPart );

    if (io_status || key)
    {
        FIXME("Unimplemented yet parameter\n");
        return STATUS_NOT_IMPLEMENTED;
    }

    SERVER_START_REQ( unlock_file )
    {
        req->handle      = hFile;
        req->offset_low  = offset->u.LowPart;
        req->offset_high = offset->u.HighPart;
        req->count_low   = count->u.LowPart;
        req->count_high  = count->u.HighPart;
        status = wine_server_call( req );
    }
    SERVER_END_REQ;
    return status;
}
