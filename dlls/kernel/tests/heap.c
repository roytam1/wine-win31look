/*
 * Unit test suite for heap functions
 *
 * Copyright 2003 Dimitrie O. Paun
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

#include <stdarg.h>
#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "wine/test.h"

START_TEST(heap)
{
    void *mem;
    HGLOBAL gbl;
    SIZE_T size;

    /* Heap*() functions */
    mem = HeapAlloc(GetProcessHeap(), 0, 0);
    ok(mem != NULL, "memory not allocated for size 0\n");

    mem = HeapReAlloc(GetProcessHeap(), 0, NULL, 10);
    ok(mem == NULL, "memory allocated by HeapReAlloc\n");

    /* Global*() functions */
    gbl = GlobalAlloc(GMEM_MOVEABLE, 0);
    ok(gbl != NULL, "global memory not allocated for size 0\n");

    gbl = GlobalReAlloc(gbl, 10, GMEM_MOVEABLE);
    ok(gbl != NULL, "Can't realloc global memory\n");
    size = GlobalSize(gbl);
    ok(size >= 10 && size <= 16, "Memory not resized to size 10, instead size=%ld\n", size);

    todo_wine
    { 
        gbl = GlobalReAlloc(gbl, 0, GMEM_MOVEABLE);
        ok(gbl != NULL, "GlobalReAlloc should not fail on size 0\n");
    }

    size = GlobalSize(gbl);
    ok(size == 0, "Memory not resized to size 0, instead size=%ld\n", size);
    ok(GlobalFree(gbl) == NULL, "Memory not freed\n");
    size = GlobalSize(gbl);
    ok(size == 0, "Memory should have been freed, size=%ld\n", size);

    gbl = GlobalReAlloc(0, 10, GMEM_MOVEABLE);
    ok(gbl == NULL, "global realloc allocated memory\n");

    /* Local*() functions */
    gbl = LocalAlloc(GMEM_MOVEABLE, 0);
    ok(gbl != NULL, "local memory not allocated for size 0\n");

    gbl = LocalReAlloc(gbl, 10, GMEM_MOVEABLE);
    ok(gbl != NULL, "Can't realloc local memory\n");
    size = LocalSize(gbl);
    ok(size >= 10 && size <= 16, "Memory not resized to size 10, instead size=%ld\n", size);

    todo_wine
    {
        gbl = LocalReAlloc(gbl, 0, GMEM_MOVEABLE);
        ok(gbl != NULL, "LocalReAlloc should not fail on size 0\n");
    }

    size = LocalSize(gbl);
    ok(size == 0, "Memory not resized to size 0, instead size=%ld\n", size);
    ok(LocalFree(gbl) == NULL, "Memory not freed\n");
    size = LocalSize(gbl);
    ok(size == 0, "Memory should have been freed, size=%ld\n", size);

    gbl = LocalReAlloc(0, 10, GMEM_MOVEABLE);
    ok(gbl == NULL, "local realloc allocated memory\n");

}
