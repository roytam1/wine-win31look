/*
 * Unit tests for shell32 string operations
 *
 * Copyright 2004 Jon Griffiths
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
#include <stdio.h>

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#define WINE_NOWINSOCK
#include "windef.h"
#include "winbase.h"
#include "wtypes.h"
#include "shellapi.h"
#include "shtypes.h"
#include "objbase.h"

#include "wine/test.h"

static HMODULE hShell32;
static HRESULT (WINAPI *pStrRetToStrNAW)(LPVOID,DWORD,LPSTRRET,const ITEMIDLIST *);

static WCHAR *CoDupStrW(const char* src)
{
  INT len = MultiByteToWideChar(CP_ACP, 0, src, -1, NULL, 0);
  WCHAR* szTemp = (WCHAR*)CoTaskMemAlloc(len * sizeof(WCHAR));
  MultiByteToWideChar(CP_ACP, 0, src, -1, szTemp, len);
  return szTemp;
}

static inline int strcmpW(const WCHAR *str1, const WCHAR *str2)
{
    while (*str1 && (*str1 == *str2)) { str1++; str2++; }
    return *str1 - *str2;
}

static void test_StrRetToStringNA(void)
{
    trace("StrRetToStringNAW is Ascii\n");
    /* FIXME */
}

static void test_StrRetToStringNW(void)
{
    static const WCHAR szTestW[] = { 'T','e','s','t','\0' };
    ITEMIDLIST iidl[10];
    WCHAR buff[128];
    STRRET strret;
    BOOL ret;

    trace("StrRetToStringNAW is Unicode\n");

    strret.uType = STRRET_WSTR;
    strret.u.pOleStr = CoDupStrW("Test");
    memset(buff, 0xff, sizeof(buff));
    ret = pStrRetToStrNAW(buff, sizeof(buff)/sizeof(WCHAR), &strret, NULL);
    ok(ret == TRUE && !strcmpW(buff, szTestW),
       "STRRET_WSTR: dup failed, ret=%d\n", ret);

    strret.uType = STRRET_CSTR;
    lstrcpyA(strret.u.cStr, "Test");
    memset(buff, 0xff, sizeof(buff));
    ret = pStrRetToStrNAW(buff, sizeof(buff)/sizeof(WCHAR), &strret, NULL);
    ok(ret == TRUE && !strcmpW(buff, szTestW),
       "STRRET_CSTR: dup failed, ret=%d\n", ret);

    strret.uType = STRRET_OFFSET;
    strret.u.uOffset = 1;
    strcpy((char*)&iidl, " Test");
    memset(buff, 0xff, sizeof(buff));
    ret = pStrRetToStrNAW(buff, sizeof(buff)/sizeof(WCHAR), &strret, iidl);
    ok(ret == TRUE && !strcmpW(buff, szTestW),
       "STRRET_OFFSET: dup failed, ret=%d\n", ret);

    /* Invalid dest - returns FALSE */
    strret.uType = STRRET_WSTR;
    strret.u.pOleStr = CoDupStrW("Test");
    ret = pStrRetToStrNAW(NULL, sizeof(buff)/sizeof(WCHAR), &strret, NULL);
    ok(ret == FALSE, "NULL dest: expected FALSE, ret=%d\n", ret);

}

START_TEST(string)
{
    CoInitialize(0);

    hShell32 = LoadLibraryA("shell32.dll");
    if (!hShell32)
        return;

    pStrRetToStrNAW = (void*)GetProcAddress(hShell32, (LPSTR)96);
    if (pStrRetToStrNAW)
    {
        if (!(GetVersion() & 0x80000000))
            test_StrRetToStringNW();
        else
            test_StrRetToStringNA();
    }
}
