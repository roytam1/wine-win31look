/*
 *	Object management functions
 *
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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_IO_H
# include <io.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include "wine/debug.h"

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winternl.h"
#include "ntdll_misc.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);

/* move to somewhere */
typedef void * POBJDIR_INFORMATION;

/*
 *	Generic object functions
 */

/******************************************************************************
 * NtQueryObject [NTDLL.@]
 * ZwQueryObject [NTDLL.@]
 */
NTSTATUS WINAPI NtQueryObject(IN HANDLE handle,
                              IN OBJECT_INFORMATION_CLASS info_class,
                              OUT PVOID ptr, IN ULONG len, OUT PULONG used_len)
{
    NTSTATUS status;

    TRACE("(%p,0x%08x,%p,0x%08lx,%p): stub\n",
          handle, info_class, ptr, len, used_len);

    if (used_len) *used_len = 0;

    switch (info_class)
    {
    case ObjectDataInformation:
        {
            OBJECT_DATA_INFORMATION* p = (OBJECT_DATA_INFORMATION*)ptr;

            if (len < sizeof(*p)) return STATUS_INVALID_BUFFER_SIZE;

            SERVER_START_REQ( set_handle_info )
            {
                req->handle = handle;
                req->flags  = 0;
                req->mask   = 0;
                req->fd     = -1;
                status = wine_server_call( req );
                if (status == STATUS_SUCCESS)
                {
                    p->InheritHandle = (reply->old_flags & HANDLE_FLAG_INHERIT) ? TRUE : FALSE;
                    p->ProtectFromClose = (reply->old_flags & HANDLE_FLAG_PROTECT_FROM_CLOSE) ? TRUE : FALSE;
                    if (used_len) *used_len = sizeof(*p);
                }
            }
            SERVER_END_REQ;
        }
        break;
    default:
        FIXME("Unsupported information class %u\n", info_class);
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }
    return status;
}

/******************************************************************
 *		NtSetInformationObject [NTDLL.@]
 *		ZwSetInformationObject [NTDLL.@]
 *
 */
NTSTATUS WINAPI NtSetInformationObject(IN HANDLE handle,
                                       IN OBJECT_INFORMATION_CLASS info_class,
                                       IN PVOID ptr, IN ULONG len)
{
    NTSTATUS status;

    TRACE("(%p,0x%08x,%p,0x%08lx): stub\n",
          handle, info_class, ptr, len);

    switch (info_class)
    {
    case ObjectDataInformation:
        {
            OBJECT_DATA_INFORMATION* p = (OBJECT_DATA_INFORMATION*)ptr;

            if (len < sizeof(*p)) return STATUS_INVALID_BUFFER_SIZE;

            SERVER_START_REQ( set_handle_info )
            {
                req->handle = handle;
                req->flags  = 0;
                req->mask   = HANDLE_FLAG_INHERIT | HANDLE_FLAG_PROTECT_FROM_CLOSE;
                req->fd     = -1;
                if (p->InheritHandle)    req->flags |= HANDLE_FLAG_INHERIT;
                if (p->ProtectFromClose) req->flags |= HANDLE_FLAG_PROTECT_FROM_CLOSE;
                status = wine_server_call( req );
            }
            SERVER_END_REQ;
        }
        break;
    default:
        FIXME("Unsupported information class %u\n", info_class);
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }
    return status;
}

/******************************************************************************
 *  NtQuerySecurityObject	[NTDLL.@]
 *
 * An ntdll analogue to GetKernelObjectSecurity().
 *
 * NOTES
 *  only the lowest 4 bit of SecurityObjectInformationClass are used
 *  0x7-0xf returns STATUS_ACCESS_DENIED (even running with system privileges)
 *
 * FIXME
 *  We are constructing a fake sid (Administrators:Full, System:Full, Everyone:Read)
 */
NTSTATUS WINAPI
NtQuerySecurityObject(
	IN HANDLE Object,
	IN SECURITY_INFORMATION RequestedInformation,
	OUT PSECURITY_DESCRIPTOR pSecurityDesriptor,
	IN ULONG Length,
	OUT PULONG ResultLength)
{
	static SID_IDENTIFIER_AUTHORITY localSidAuthority = {SECURITY_NT_AUTHORITY};
	static SID_IDENTIFIER_AUTHORITY worldSidAuthority = {SECURITY_WORLD_SID_AUTHORITY};
	BYTE Buffer[256];
	PISECURITY_DESCRIPTOR_RELATIVE psd = (PISECURITY_DESCRIPTOR_RELATIVE)Buffer;
	UINT BufferIndex = sizeof(SECURITY_DESCRIPTOR_RELATIVE);

	FIXME("(%p,0x%08lx,%p,0x%08lx,%p) stub!\n",
	Object, RequestedInformation, pSecurityDesriptor, Length, ResultLength);

	RequestedInformation &= 0x0000000f;

	if (RequestedInformation & SACL_SECURITY_INFORMATION) return STATUS_ACCESS_DENIED;

	ZeroMemory(Buffer, 256);
	RtlCreateSecurityDescriptor((PSECURITY_DESCRIPTOR)psd, SECURITY_DESCRIPTOR_REVISION);
	psd->Control = SE_SELF_RELATIVE |
	  ((RequestedInformation & DACL_SECURITY_INFORMATION) ? SE_DACL_PRESENT:0);

	/* owner: administrator S-1-5-20-220*/
	if (OWNER_SECURITY_INFORMATION & RequestedInformation)
	{
	  PSID psid = (PSID)&(Buffer[BufferIndex]);

	  psd->Owner = BufferIndex;
	  BufferIndex += RtlLengthRequiredSid(2);

	  psid->Revision = SID_REVISION;
	  psid->SubAuthorityCount = 2;
	  psid->IdentifierAuthority = localSidAuthority;
	  psid->SubAuthority[0] = SECURITY_BUILTIN_DOMAIN_RID;
	  psid->SubAuthority[1] = DOMAIN_ALIAS_RID_ADMINS;
	}

	/* group: built in domain S-1-5-12 */
	if (GROUP_SECURITY_INFORMATION & RequestedInformation)
	{
	  PSID psid = (PSID) &(Buffer[BufferIndex]);

	  psd->Group = BufferIndex;
	  BufferIndex += RtlLengthRequiredSid(1);

	  psid->Revision = SID_REVISION;
	  psid->SubAuthorityCount = 1;
	  psid->IdentifierAuthority = localSidAuthority;
	  psid->SubAuthority[0] = SECURITY_LOCAL_SYSTEM_RID;
	}

	/* discretionary ACL */
	if (DACL_SECURITY_INFORMATION & RequestedInformation)
	{
	  /* acl header */
	  PACL pacl = (PACL)&(Buffer[BufferIndex]);
	  PACCESS_ALLOWED_ACE pace;
	  PSID psid;

	  psd->Dacl = BufferIndex;

	  pacl->AclRevision = MIN_ACL_REVISION;
	  pacl->AceCount = 3;
	  pacl->AclSize = BufferIndex; /* storing the start index temporary */

	  BufferIndex += sizeof(ACL);

	  /* ACE System - full access */
	  pace = (PACCESS_ALLOWED_ACE)&(Buffer[BufferIndex]);
	  BufferIndex += sizeof(ACCESS_ALLOWED_ACE)-sizeof(DWORD);

	  pace->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
	  pace->Header.AceFlags = CONTAINER_INHERIT_ACE;
	  pace->Header.AceSize = sizeof(ACCESS_ALLOWED_ACE)-sizeof(DWORD) + RtlLengthRequiredSid(1);
	  pace->Mask = DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER  | 0x3f;
	  pace->SidStart = BufferIndex;

	  /* SID S-1-5-12 (System) */
	  psid = (PSID)&(Buffer[BufferIndex]);

	  BufferIndex += RtlLengthRequiredSid(1);

	  psid->Revision = SID_REVISION;
	  psid->SubAuthorityCount = 1;
	  psid->IdentifierAuthority = localSidAuthority;
	  psid->SubAuthority[0] = SECURITY_LOCAL_SYSTEM_RID;

	  /* ACE Administrators - full access*/
	  pace = (PACCESS_ALLOWED_ACE) &(Buffer[BufferIndex]);
	  BufferIndex += sizeof(ACCESS_ALLOWED_ACE)-sizeof(DWORD);

	  pace->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
	  pace->Header.AceFlags = CONTAINER_INHERIT_ACE;
	  pace->Header.AceSize = sizeof(ACCESS_ALLOWED_ACE)-sizeof(DWORD) + RtlLengthRequiredSid(2);
	  pace->Mask = DELETE | READ_CONTROL | WRITE_DAC | WRITE_OWNER  | 0x3f;
	  pace->SidStart = BufferIndex;

	  /* S-1-5-12 (Administrators) */
	  psid = (PSID)&(Buffer[BufferIndex]);

	  BufferIndex += RtlLengthRequiredSid(2);

	  psid->Revision = SID_REVISION;
	  psid->SubAuthorityCount = 2;
	  psid->IdentifierAuthority = localSidAuthority;
	  psid->SubAuthority[0] = SECURITY_BUILTIN_DOMAIN_RID;
	  psid->SubAuthority[1] = DOMAIN_ALIAS_RID_ADMINS;

	  /* ACE Everyone - read access */
	  pace = (PACCESS_ALLOWED_ACE)&(Buffer[BufferIndex]);
	  BufferIndex += sizeof(ACCESS_ALLOWED_ACE)-sizeof(DWORD);

	  pace->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
	  pace->Header.AceFlags = CONTAINER_INHERIT_ACE;
	  pace->Header.AceSize = sizeof(ACCESS_ALLOWED_ACE)-sizeof(DWORD) + RtlLengthRequiredSid(1);
	  pace->Mask = READ_CONTROL| 0x19;
	  pace->SidStart = BufferIndex;

	  /* SID S-1-1-0 (Everyone) */
	  psid = (PSID)&(Buffer[BufferIndex]);

	  BufferIndex += RtlLengthRequiredSid(1);

	  psid->Revision = SID_REVISION;
	  psid->SubAuthorityCount = 1;
	  psid->IdentifierAuthority = worldSidAuthority;
	  psid->SubAuthority[0] = 0;

	  /* calculate used bytes */
	  pacl->AclSize = BufferIndex - pacl->AclSize;
	}
	*ResultLength = BufferIndex;
	TRACE("len=%lu\n", *ResultLength);
	if (Length < *ResultLength) return STATUS_BUFFER_TOO_SMALL;
	memcpy(pSecurityDesriptor, Buffer, *ResultLength);

	return STATUS_SUCCESS;
}


/******************************************************************************
 *  NtDuplicateObject		[NTDLL.@]
 *  ZwDuplicateObject		[NTDLL.@]
 */
NTSTATUS WINAPI NtDuplicateObject( HANDLE source_process, HANDLE source,
                                   HANDLE dest_process, PHANDLE dest,
                                   ACCESS_MASK access, ULONG attributes, ULONG options )
{
    NTSTATUS ret;
    SERVER_START_REQ( dup_handle )
    {
        req->src_process = source_process;
        req->src_handle  = source;
        req->dst_process = dest_process;
        req->access      = access;
        req->inherit     = (attributes & OBJ_INHERIT) != 0;
        req->options     = options;

        if (!(ret = wine_server_call( req )))
        {
            if (dest) *dest = reply->handle;
            if (reply->fd != -1) close( reply->fd );
        }
    }
    SERVER_END_REQ;
    return ret;
}

/**************************************************************************
 *                 NtClose				[NTDLL.@]
 *
 * Close a handle reference to an object.
 * 
 * PARAMS
 *  Handle [I] handle to close
 *
 * RETURNS
 *  Success: ERROR_SUCCESS.
 *  Failure: An NTSTATUS error code.
 */
NTSTATUS WINAPI NtClose( HANDLE Handle )
{
    NTSTATUS ret;
    SERVER_START_REQ( close_handle )
    {
        req->handle = Handle;
        ret = wine_server_call( req );
        if (!ret && reply->fd != -1) close( reply->fd );
    }
    SERVER_END_REQ;
    return ret;
}

/*
 *	Directory functions
 */

/**************************************************************************
 * NtOpenDirectoryObject [NTDLL.@]
 * ZwOpenDirectoryObject [NTDLL.@]
 *
 * Open a namespace directory object.
 * 
 * PARAMS
 *  DirectoryHandle  [O] Destination for the new directory handle
 *  DesiredAccess    [I] Desired access to the directory
 *  ObjectAttributes [I] Structure describing the directory
 *
 * RETURNS
 *  Success: ERROR_SUCCESS.
 *  Failure: An NTSTATUS error code.
 */
NTSTATUS WINAPI NtOpenDirectoryObject(
	PHANDLE DirectoryHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes)
{
	FIXME("(%p,0x%08lx,%p): stub\n",
	DirectoryHandle, DesiredAccess, ObjectAttributes);
	dump_ObjectAttributes(ObjectAttributes);
	return 0;
}

/******************************************************************************
 *  NtCreateDirectoryObject	[NTDLL.@]
 *  ZwCreateDirectoryObject	[NTDLL.@]
 */
NTSTATUS WINAPI NtCreateDirectoryObject(
	PHANDLE DirectoryHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes)
{
	FIXME("(%p,0x%08lx,%p),stub!\n",
	DirectoryHandle,DesiredAccess,ObjectAttributes);
	dump_ObjectAttributes(ObjectAttributes);
	return 0;
}

/******************************************************************************
 * NtQueryDirectoryObject [NTDLL.@]
 * ZwQueryDirectoryObject [NTDLL.@]
 *
 * Read information from a namespace directory.
 * 
 * PARAMS
 *  DirObjHandle      [I]   Object handle
 *  DirObjInformation [O]   Buffer to hold the data read
 *  BufferLength      [I]   Size of the buffer in bytes
 *  GetNextIndex      [I]   Set ObjectIndex to TRUE=next object, FALSE=last object
 *  IgnoreInputIndex  [I]   Start reading at index TRUE=0, FALSE=ObjectIndex
 *  ObjectIndex       [I/O] 0 based index into the directory, see IgnoreInputIndex and GetNextIndex
 *  DataWritten       [O]   Caller supplied storage for the number of bytes written (or NULL)
 *
 * RETURNS
 *  Success: ERROR_SUCCESS.
 *  Failure: An NTSTATUS error code.
 */
NTSTATUS WINAPI NtQueryDirectoryObject(
	IN HANDLE DirObjHandle,
	OUT POBJDIR_INFORMATION DirObjInformation,
	IN ULONG BufferLength,
	IN BOOLEAN GetNextIndex,
	IN BOOLEAN IgnoreInputIndex,
	IN OUT PULONG ObjectIndex,
	OUT PULONG DataWritten OPTIONAL)
{
	FIXME("(%p,%p,0x%08lx,0x%08x,0x%08x,%p,%p) stub\n",
		DirObjHandle, DirObjInformation, BufferLength, GetNextIndex,
		IgnoreInputIndex, ObjectIndex, DataWritten);
    return 0xc0000000; /* We don't have any. Whatever. (Yet.) */
}

/*
 *	Link objects
 */

/******************************************************************************
 *  NtOpenSymbolicLinkObject	[NTDLL.@]
 */
NTSTATUS WINAPI NtOpenSymbolicLinkObject(
	OUT PHANDLE LinkHandle,
	IN ACCESS_MASK DesiredAccess,
	IN POBJECT_ATTRIBUTES ObjectAttributes)
{
	FIXME("(%p,0x%08lx,%p) stub\n",
	LinkHandle, DesiredAccess, ObjectAttributes);
	dump_ObjectAttributes(ObjectAttributes);
	return 0;
}

/******************************************************************************
 *  NtCreateSymbolicLinkObject	[NTDLL.@]
 */
NTSTATUS WINAPI NtCreateSymbolicLinkObject(
	OUT PHANDLE SymbolicLinkHandle,
	IN ACCESS_MASK DesiredAccess,
	IN POBJECT_ATTRIBUTES ObjectAttributes,
	IN PUNICODE_STRING Name)
{
	FIXME("(%p,0x%08lx,%p, %p) stub\n",
	SymbolicLinkHandle, DesiredAccess, ObjectAttributes, debugstr_us(Name));
	dump_ObjectAttributes(ObjectAttributes);
	return 0;
}

/******************************************************************************
 *  NtQuerySymbolicLinkObject	[NTDLL.@]
 */
NTSTATUS WINAPI NtQuerySymbolicLinkObject(
	IN HANDLE LinkHandle,
	IN OUT PUNICODE_STRING LinkTarget,
	OUT PULONG ReturnedLength OPTIONAL)
{
	FIXME("(%p,%p,%p) stub\n",
	LinkHandle, debugstr_us(LinkTarget), ReturnedLength);

	return 0;
}

/******************************************************************************
 *  NtAllocateUuids   [NTDLL.@]
 *
 * NOTES
 *  I have seen lpdwCount pointing to a pointer once...
 */
NTSTATUS WINAPI NtAllocateUuids(LPDWORD lpdwCount, LPDWORD *p2, LPDWORD *p3)
{
	FIXME("(%p[%ld],%p,%p), stub.\n", lpdwCount,
					 lpdwCount ? *lpdwCount : 0,
					 p2, p3);
	return 0;
}
