/*
 * NtDll debug buffer functions
 *
 * Copyright 2004 Raphael Junqueira
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
#include <stdlib.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "winreg.h"
#include "winternl.h"
#include "ntdll_misc.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(debug_buffer);

static void dump_DEBUG_MODULE_INFORMATION(PDEBUG_MODULE_INFORMATION iBuf)
{
  TRACE( "MODULE_INFORMATION:%p\n", iBuf );
  if (NULL == iBuf) return ;
  TRACE( "Base:%ld\n", iBuf->Base );
  TRACE( "Size:%ld\n", iBuf->Size );
  TRACE( "Flags:%ld\n", iBuf->Flags );
}

static void dump_DEBUG_HEAP_INFORMATION(PDEBUG_HEAP_INFORMATION iBuf)
{
  TRACE( "HEAP_INFORMATION:%p\n", iBuf );
  if (NULL == iBuf) return ;
  TRACE( "Base:%ld\n", iBuf->Base );
  TRACE( "Flags:%ld\n", iBuf->Flags );
}

static void dump_DEBUG_LOCK_INFORMATION(PDEBUG_LOCK_INFORMATION iBuf)
{
  TRACE( "LOCK_INFORMATION:%p\n", iBuf );

  if (NULL == iBuf) return ;

  TRACE( "Address:%p\n", iBuf->Address );
  TRACE( "Type:%d\n", iBuf->Type );
  TRACE( "CreatorBackTraceIndex:%d\n", iBuf->CreatorBackTraceIndex );
  TRACE( "OwnerThreadId:%ld\n", iBuf->OwnerThreadId );
  TRACE( "ActiveCount:%ld\n", iBuf->ActiveCount );
  TRACE( "ContentionCount:%ld\n", iBuf->ContentionCount );
  TRACE( "EntryCount:%ld\n", iBuf->EntryCount );
  TRACE( "RecursionCount:%ld\n", iBuf->RecursionCount );
  TRACE( "NumberOfSharedWaiters:%ld\n", iBuf->NumberOfSharedWaiters );
  TRACE( "NumberOfExclusiveWaiters:%ld\n", iBuf->NumberOfExclusiveWaiters );
}

static void dump_DEBUG_BUFFER(PDEBUG_BUFFER iBuf)
{
  if (NULL == iBuf) return ;
  TRACE( "SectionHandle:%p\n", iBuf->SectionHandle);
  TRACE( "SectionBase:%p\n", iBuf->SectionBase);
  TRACE( "RemoteSectionBase:%p\n", iBuf->RemoteSectionBase);
  TRACE( "SectionBaseDelta:%ld\n", iBuf->SectionBaseDelta);
  TRACE( "EventPairHandle:%p\n", iBuf->EventPairHandle);
  TRACE( "RemoteThreadHandle:%p\n", iBuf->RemoteThreadHandle);
  TRACE( "InfoClassMask:%lx\n", iBuf->InfoClassMask);
  TRACE( "SizeOfInfo:%ld\n", iBuf->SizeOfInfo);
  TRACE( "AllocatedSize:%ld\n", iBuf->AllocatedSize);
  TRACE( "SectionSize:%ld\n", iBuf->SectionSize);
  TRACE( "BackTraceInfo:%p\n", iBuf->BackTraceInformation);
  dump_DEBUG_MODULE_INFORMATION(iBuf->ModuleInformation);
  dump_DEBUG_HEAP_INFORMATION(iBuf->HeapInformation);
  dump_DEBUG_LOCK_INFORMATION(iBuf->LockInformation);
}

PDEBUG_BUFFER WINAPI RtlCreateQueryDebugBuffer(IN ULONG iSize, IN BOOLEAN iEventPair) 
{
   PDEBUG_BUFFER oBuf = NULL;
   FIXME("(%ld, %d): stub\n", iSize, iEventPair);
   if (iSize < sizeof(DEBUG_BUFFER)) {
     iSize = sizeof(DEBUG_BUFFER);
   }
   oBuf = (PDEBUG_BUFFER) RtlAllocateHeap(GetProcessHeap(), 0, iSize);
   memset(oBuf, 0, iSize);
   FIXME("(%ld, %d): returning %p\n", iSize, iEventPair, oBuf);
   return oBuf;
}

NTSTATUS WINAPI RtlDestroyQueryDebugBuffer(IN PDEBUG_BUFFER iBuf) 
{
   NTSTATUS nts = STATUS_SUCCESS;
   FIXME("(%p): stub\n", iBuf);
   if (NULL != iBuf) {
     if (NULL != iBuf->ModuleInformation) RtlFreeHeap(GetProcessHeap(), 0, iBuf->ModuleInformation);
     if (NULL != iBuf->HeapInformation) RtlFreeHeap(GetProcessHeap(), 0, iBuf->HeapInformation);
     if (NULL != iBuf->LockInformation) RtlFreeHeap(GetProcessHeap(), 0, iBuf->LockInformation);
     RtlFreeHeap(GetProcessHeap(), 0, iBuf);
   }
   return nts;
}

NTSTATUS WINAPI RtlQueryProcessDebugInformation(IN ULONG iProcessId, IN ULONG iDebugInfoMask, IN OUT PDEBUG_BUFFER iBuf) 
{
   NTSTATUS nts = STATUS_SUCCESS;
   FIXME("(%ld, %lx, %p): stub\n", iProcessId, iDebugInfoMask, iBuf);
   iBuf->InfoClassMask = iDebugInfoMask;
   
   if (iDebugInfoMask & PDI_MODULES) {
     PDEBUG_MODULE_INFORMATION info = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(DEBUG_MODULE_INFORMATION));
     memset(info, 0, sizeof(DEBUG_MODULE_INFORMATION));
     iBuf->ModuleInformation = info;
   }
   if (iDebugInfoMask & PDI_HEAPS) {
     PDEBUG_HEAP_INFORMATION info = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(DEBUG_HEAP_INFORMATION));
     memset(info, 0, sizeof(DEBUG_HEAP_INFORMATION));
     if (iDebugInfoMask & PDI_HEAP_TAGS) {
     }
     if (iDebugInfoMask & PDI_HEAP_BLOCKS) {
     }
     iBuf->HeapInformation = info;
   }
   if (iDebugInfoMask & PDI_LOCKS) {
     PDEBUG_LOCK_INFORMATION info = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(DEBUG_LOCK_INFORMATION));
     memset(info, 0, sizeof(DEBUG_LOCK_INFORMATION));
     iBuf->LockInformation = info;
   }
   TRACE("returns:%p\n", iBuf);
   dump_DEBUG_BUFFER(iBuf);
   return nts;
}
