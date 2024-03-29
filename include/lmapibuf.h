/*
 * Copyright 2002 Andriy Palamarchuk
 *
 * Net API buffer calls
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

#ifndef __WINE_LMAPIBUF_H
#define __WINE_LMAPIBUF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Buffer functions */
NET_API_STATUS WINAPI NetApiBufferAllocate(DWORD ByteCount, LPVOID* Buffer);
NET_API_STATUS WINAPI NetApiBufferFree(LPVOID Buffer);
NET_API_STATUS WINAPI NetApiBufferReallocate(LPVOID OldBuffer, DWORD NewByteCount,
                                             LPVOID* NewBuffer);
NET_API_STATUS WINAPI NetApiBufferSize(LPVOID Buffer, LPDWORD ByteCount);
NET_API_STATUS WINAPI NetapipBufferAllocate(DWORD ByteCount, LPVOID* Buffer);

#ifdef __cplusplus
}
#endif

#endif
