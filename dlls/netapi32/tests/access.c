/*
 * Copyright 2002 Andriy Palamarchuk
 *
 * Conformance test of the access functions.
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

#include <wine/test.h>
#include <windef.h>
#include <winbase.h>
#include <winerror.h>
#include <lmaccess.h>
#include <lmerr.h>
#include <lmapibuf.h>

WCHAR user_name[UNLEN + 1];
WCHAR computer_name[MAX_COMPUTERNAME_LENGTH + 1];

const WCHAR sAdminUserName[] = {'A','d','m','i','n','i','s','t','r','a','t',
                                'o','r',0};
const WCHAR sGuestUserName[] = {'G','u','e','s','t',0};
const WCHAR sNonexistentUser[] = {'N','o','n','e','x','i','s','t','e','n','t',' ',
                                'U','s','e','r',0};
const WCHAR sBadNetPath[] = {'\\','\\','B','a',' ',' ','p','a','t','h',0};
const WCHAR sInvalidName[] = {'\\',0};
const WCHAR sInvalidName2[] = {'\\','\\',0};
const WCHAR sEmptyStr[] = { 0 };

static NET_API_STATUS (WINAPI *pNetApiBufferFree)(LPVOID)=NULL;
static NET_API_STATUS (WINAPI *pNetApiBufferSize)(LPVOID,LPDWORD)=NULL;
static NET_API_STATUS (WINAPI *pNetQueryDisplayInformation)(LPWSTR,DWORD,DWORD,DWORD,DWORD,LPDWORD,PVOID*)=NULL;
static NET_API_STATUS (WINAPI *pNetUserGetInfo)(LPCWSTR,LPCWSTR,DWORD,LPBYTE*)=NULL;

static int init_access_tests(void)
{
    DWORD dwSize;
    BOOL rc;

    user_name[0] = 0;
    dwSize = sizeof(user_name);
    rc=GetUserNameW(user_name, &dwSize);
    if (rc==FALSE && GetLastError()==ERROR_CALL_NOT_IMPLEMENTED)
        return 0;
    ok(rc, "User Name Retrieved\n");

    computer_name[0] = 0;
    dwSize = sizeof(computer_name);
    ok(GetComputerNameW(computer_name, &dwSize), "Computer Name Retrieved\n");
    return 1;
}

void run_usergetinfo_tests(void)
{
    NET_API_STATUS rc;
    PUSER_INFO_0 ui0 = NULL;
    PUSER_INFO_10 ui10 = NULL;
    DWORD dwSize;

    /* If this one is not defined then none of the others will be defined */
    if (!pNetUserGetInfo)
        return;

    /* Level 0 */
    rc=pNetUserGetInfo(NULL, sAdminUserName, 0, (LPBYTE *)&ui0);
    ok(rc == NERR_Success, "NetUserGetInfo: rc=%ld\n", rc);
    ok(!lstrcmpW(sAdminUserName, ui0->usri0_name), "This is really user name\n");
    pNetApiBufferSize(ui0, &dwSize);
    ok(dwSize >= (sizeof(USER_INFO_0) +
                  (lstrlenW(ui0->usri0_name) + 1) * sizeof(WCHAR)),
       "Is allocated with NetApiBufferAllocate\n");

    /* Level 10 */
    rc=pNetUserGetInfo(NULL, sAdminUserName, 10, (LPBYTE *)&ui10);
    ok(rc == NERR_Success, "NetUserGetInfo: rc=%ld\n", rc);
    ok(!lstrcmpW(sAdminUserName, ui10->usri10_name), "This is really user name\n");
    pNetApiBufferSize(ui10, &dwSize);
    ok(dwSize >= (sizeof(USER_INFO_10) +
                  (lstrlenW(ui10->usri10_name) + 1 +
                   lstrlenW(ui10->usri10_comment) + 1 +
                   lstrlenW(ui10->usri10_usr_comment) + 1 +
                   lstrlenW(ui10->usri10_full_name) + 1) * sizeof(WCHAR)),
       "Is allocated with NetApiBufferAllocate\n");

    pNetApiBufferFree(ui0);
    pNetApiBufferFree(ui10);

    /* errors handling */
    rc=pNetUserGetInfo(NULL, sAdminUserName, 10000, (LPBYTE *)&ui0);
    ok(rc == ERROR_INVALID_LEVEL,"Invalid Level: rc=%ld\n",rc);
    rc=pNetUserGetInfo(NULL, sNonexistentUser, 0, (LPBYTE *)&ui0);
    ok(rc == NERR_UserNotFound,"Invalid User Name: rc=%ld\n",rc);
    todo_wine {
        /* FIXME - Currently Wine can't verify whether the network path is good or bad */
        rc=pNetUserGetInfo(sBadNetPath, sAdminUserName, 0, (LPBYTE *)&ui0);
        ok(rc == ERROR_BAD_NETPATH || rc == ERROR_NETWORK_UNREACHABLE,
           "Bad Network Path: rc=%ld\n",rc);
    }
    rc=pNetUserGetInfo(sEmptyStr, sAdminUserName, 0, (LPBYTE *)&ui0);
    ok(rc == ERROR_BAD_NETPATH,"Bad Network Path: rc=%ld\n",rc);
    rc=pNetUserGetInfo(sInvalidName, sAdminUserName, 0, (LPBYTE *)&ui0);
    ok(rc == ERROR_INVALID_NAME,"Invalid Server Name: rc=%ld\n",rc);
    rc=pNetUserGetInfo(sInvalidName2, sAdminUserName, 0, (LPBYTE *)&ui0);
    ok(rc == ERROR_INVALID_NAME,"Invalid Server Name: rc=%ld\n",rc);
}

/* checks Level 1 of NetQueryDisplayInformation */
void run_querydisplayinformation1_tests(void)
{
    PNET_DISPLAY_USER Buffer, rec;
    DWORD Result, EntryCount;
    DWORD i = 0;
    BOOL hasAdmin = FALSE;
    BOOL hasGuest = FALSE;

    if (!pNetQueryDisplayInformation)
        return;

    do
    {
        Result = pNetQueryDisplayInformation(
            NULL, 1, i, 1000, MAX_PREFERRED_LENGTH, &EntryCount,
            (PVOID *)&Buffer);

        ok((Result == ERROR_SUCCESS) || (Result == ERROR_MORE_DATA),
           "Information Retrieved\n");
        rec = Buffer;
        for(; EntryCount > 0; EntryCount--)
        {
            if (!lstrcmpW(rec->usri1_name, sAdminUserName))
            {
                ok(!hasAdmin, "One admin user\n");
                ok(rec->usri1_flags & UF_SCRIPT, "UF_SCRIPT flag is set\n");
                ok(rec->usri1_flags & UF_NORMAL_ACCOUNT, "UF_NORMAL_ACCOUNT flag is set\n");
                hasAdmin = TRUE;
            }
            else if (!lstrcmpW(rec->usri1_name, sGuestUserName))
            {
                ok(!hasGuest, "One guest record\n");
                ok(rec->usri1_flags & UF_SCRIPT, "UF_SCRIPT flag is set\n");
                ok(rec->usri1_flags & UF_NORMAL_ACCOUNT, "UF_NORMAL_ACCOUNT flag is set\n");
                hasGuest = TRUE;
            }

            i = rec->usri1_next_index;
            rec++;
        }

        pNetApiBufferFree(Buffer);
    } while (Result == ERROR_MORE_DATA);

    ok(hasAdmin, "Has Administrator account\n");
    ok(hasGuest, "Has Guest account\n");
}

START_TEST(access)
{
    HMODULE hnetapi32=LoadLibraryA("netapi32.dll");
    pNetApiBufferFree=(void*)GetProcAddress(hnetapi32,"NetApiBufferFree");
    pNetApiBufferSize=(void*)GetProcAddress(hnetapi32,"NetApiBufferSize");
    pNetQueryDisplayInformation=(void*)GetProcAddress(hnetapi32,"NetQueryDisplayInformation");
    pNetUserGetInfo=(void*)GetProcAddress(hnetapi32,"NetUserGetInfo");
    if (!pNetApiBufferSize)
        trace("It appears there is no netapi32 functionality on this platform\n");

    if (init_access_tests()) {
        run_usergetinfo_tests();
        run_querydisplayinformation1_tests();
    }
}
