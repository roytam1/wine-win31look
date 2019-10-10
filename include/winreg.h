/*
 * Win32 registry defines (see also winnt.h)
 *
 * Copyright (C) the Wine project
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

#ifndef __WINE_WINREG_H
#define __WINE_WINREG_H

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

#define HKEY_CLASSES_ROOT       ((HKEY) 0x80000000)
#define HKEY_CURRENT_USER       ((HKEY) 0x80000001)
#define HKEY_LOCAL_MACHINE      ((HKEY) 0x80000002)
#define HKEY_USERS              ((HKEY) 0x80000003)
#define HKEY_PERFORMANCE_DATA   ((HKEY) 0x80000004)
#define HKEY_CURRENT_CONFIG     ((HKEY) 0x80000005)
#define HKEY_DYN_DATA           ((HKEY) 0x80000006)

/*
 *	registry provider structs
 */
typedef struct value_entA
{   LPSTR	ve_valuename;
    DWORD	ve_valuelen;
    DWORD_PTR	ve_valueptr;
    DWORD	ve_type;
} VALENTA, *PVALENTA;

typedef struct value_entW {
    LPWSTR	ve_valuename;
    DWORD	ve_valuelen;
    DWORD_PTR	ve_valueptr;
    DWORD	ve_type;
} VALENTW, *PVALENTW;

typedef ACCESS_MASK REGSAM;

BOOL        WINAPI AbortSystemShutdownA(LPSTR);
BOOL        WINAPI AbortSystemShutdownW(LPWSTR);
#define     AbortSystemShutdown WINELIB_NAME_AW(AbortSystemShutdown)
BOOL        WINAPI InitiateSystemShutdownA(LPSTR,LPSTR,DWORD,BOOL,BOOL);
BOOL        WINAPI InitiateSystemShutdownW(LPWSTR,LPWSTR,DWORD,BOOL,BOOL);
#define     InitiateSystemShutdown WINELIB_NAME_AW(InitiateSystemShutdown);
BOOL        WINAPI InitiateSystemShutdownExA(LPSTR,LPSTR,DWORD,BOOL,BOOL,DWORD);
BOOL        WINAPI InitiateSystemShutdownExW(LPWSTR,LPWSTR,DWORD,BOOL,BOOL,DWORD);
#define     InitiateSystemShutdownEx WINELIB_NAME_AW(InitiateSystemShutdownEx);
DWORD       WINAPI RegCreateKeyExA(HKEY,LPCSTR,DWORD,LPCSTR,DWORD,REGSAM,
                                     LPSECURITY_ATTRIBUTES,PHKEY,LPDWORD);
DWORD       WINAPI RegCreateKeyExW(HKEY,LPCWSTR,DWORD,LPCWSTR,DWORD,REGSAM,
                                     LPSECURITY_ATTRIBUTES,PHKEY,LPDWORD);
#define     RegCreateKeyEx WINELIB_NAME_AW(RegCreateKeyEx)
LONG        WINAPI RegSaveKeyA(HKEY,LPCSTR,LPSECURITY_ATTRIBUTES);
LONG        WINAPI RegSaveKeyW(HKEY,LPCWSTR,LPSECURITY_ATTRIBUTES);
#define     RegSaveKey WINELIB_NAME_AW(RegSaveKey)
LONG        WINAPI RegSetKeySecurity(HKEY,SECURITY_INFORMATION,PSECURITY_DESCRIPTOR);
LONG        WINAPI RegConnectRegistryA(LPCSTR,HKEY,PHKEY);
LONG        WINAPI RegConnectRegistryW(LPCWSTR,HKEY,PHKEY);
#define     RegConnectRegistry WINELIB_NAME_AW(RegConnectRegistry)
DWORD       WINAPI RegEnumKeyExA(HKEY,DWORD,LPSTR,LPDWORD,LPDWORD,LPSTR,
                                   LPDWORD,LPFILETIME);
DWORD       WINAPI RegEnumKeyExW(HKEY,DWORD,LPWSTR,LPDWORD,LPDWORD,LPWSTR,
                                   LPDWORD,LPFILETIME);
#define     RegEnumKeyEx WINELIB_NAME_AW(RegEnumKeyEx)
LONG        WINAPI RegGetKeySecurity(HKEY,SECURITY_INFORMATION,PSECURITY_DESCRIPTOR,LPDWORD);
LONG        WINAPI RegLoadKeyA(HKEY,LPCSTR,LPCSTR);
LONG        WINAPI RegLoadKeyW(HKEY,LPCWSTR,LPCWSTR);
#define     RegLoadKey WINELIB_NAME_AW(RegLoadKey)
LONG        WINAPI RegNotifyChangeKeyValue(HKEY,BOOL,DWORD,HANDLE,BOOL);
DWORD       WINAPI RegOpenKeyExW(HKEY,LPCWSTR,DWORD,REGSAM,PHKEY);
DWORD       WINAPI RegOpenKeyExA(HKEY,LPCSTR,DWORD,REGSAM,PHKEY);
#define     RegOpenKeyEx WINELIB_NAME_AW(RegOpenKeyEx)
DWORD       WINAPI RegQueryInfoKeyW(HKEY,LPWSTR,LPDWORD,LPDWORD,LPDWORD,
                                      LPDWORD,LPDWORD,LPDWORD,LPDWORD,LPDWORD,
                                      LPDWORD,LPFILETIME);
DWORD       WINAPI RegQueryInfoKeyA(HKEY,LPSTR,LPDWORD,LPDWORD,LPDWORD,
                                      LPDWORD,LPDWORD,LPDWORD,LPDWORD,LPDWORD,
                                      LPDWORD,LPFILETIME);
#define     RegQueryInfoKey WINELIB_NAME_AW(RegQueryInfoKey)
DWORD       WINAPI RegQueryMultipleValuesA(HKEY,PVALENTA,DWORD,LPSTR,LPDWORD);
DWORD       WINAPI RegQueryMultipleValuesW(HKEY,PVALENTW,DWORD,LPWSTR,LPDWORD);
#define     RegQueryMultipleValues WINELIB_NAME_AW(RegQueryMultipleValues)
LONG        WINAPI RegReplaceKeyA(HKEY,LPCSTR,LPCSTR,LPCSTR);
LONG        WINAPI RegReplaceKeyW(HKEY,LPCWSTR,LPCWSTR,LPCWSTR);
#define     RegReplaceKey WINELIB_NAME_AW(RegReplaceKey)
LONG        WINAPI RegRestoreKeyA(HKEY,LPCSTR,DWORD);
LONG        WINAPI RegRestoreKeyW(HKEY,LPCWSTR,DWORD);
#define     RegRestoreKey WINELIB_NAME_AW(RegRestoreKey)
LONG        WINAPI RegUnLoadKeyA(HKEY,LPCSTR);
LONG        WINAPI RegUnLoadKeyW(HKEY,LPCWSTR);
#define     RegUnLoadKey WINELIB_NAME_AW(RegUnLoadKey)

/* Declarations for functions that are the same in Win16 and Win32 */

DWORD       WINAPI RegCloseKey(HKEY);
DWORD       WINAPI RegFlushKey(HKEY);

DWORD       WINAPI RegCreateKeyA(HKEY,LPCSTR,PHKEY);
DWORD       WINAPI RegCreateKeyW(HKEY,LPCWSTR,PHKEY);
#define     RegCreateKey WINELIB_NAME_AW(RegCreateKey)
DWORD       WINAPI RegDeleteKeyA(HKEY,LPCSTR);
DWORD       WINAPI RegDeleteKeyW(HKEY,LPCWSTR);
#define     RegDeleteKey WINELIB_NAME_AW(RegDeleteKey)
DWORD       WINAPI RegDeleteValueA(HKEY,LPCSTR);
DWORD       WINAPI RegDeleteValueW(HKEY,LPCWSTR);
#define     RegDeleteValue WINELIB_NAME_AW(RegDeleteValue)
DWORD       WINAPI RegEnumKeyA(HKEY,DWORD,LPSTR,DWORD);
DWORD       WINAPI RegEnumKeyW(HKEY,DWORD,LPWSTR,DWORD);
#define     RegEnumKey WINELIB_NAME_AW(RegEnumKey)
DWORD       WINAPI RegEnumValueA(HKEY,DWORD,LPSTR,LPDWORD,LPDWORD,LPDWORD,LPBYTE,LPDWORD);
DWORD       WINAPI RegEnumValueW(HKEY,DWORD,LPWSTR,LPDWORD,LPDWORD,LPDWORD,LPBYTE,LPDWORD);
#define     RegEnumValue WINELIB_NAME_AW(RegEnumValue)
DWORD       WINAPI RegOpenKeyA(HKEY,LPCSTR,PHKEY);
DWORD       WINAPI RegOpenKeyW(HKEY,LPCWSTR,PHKEY);
#define     RegOpenKey WINELIB_NAME_AW(RegOpenKey)
DWORD       WINAPI RegQueryValueA(HKEY,LPCSTR,LPSTR,LPLONG);
DWORD       WINAPI RegQueryValueW(HKEY,LPCWSTR,LPWSTR,LPLONG);
#define     RegQueryValue WINELIB_NAME_AW(RegQueryValue)
DWORD       WINAPI RegQueryValueExA(HKEY,LPCSTR,LPDWORD,LPDWORD,LPBYTE,LPDWORD);
DWORD       WINAPI RegQueryValueExW(HKEY,LPCWSTR,LPDWORD,LPDWORD,LPBYTE,LPDWORD);
#define     RegQueryValueEx WINELIB_NAME_AW(RegQueryValueEx)
DWORD       WINAPI RegSetValueA(HKEY,LPCSTR,DWORD,LPCSTR,DWORD);
DWORD       WINAPI RegSetValueW(HKEY,LPCWSTR,DWORD,LPCWSTR,DWORD);
#define     RegSetValue WINELIB_NAME_AW(RegSetValue)
DWORD       WINAPI RegSetValueExA(HKEY,LPCSTR,DWORD,DWORD,CONST BYTE*,DWORD);
DWORD       WINAPI RegSetValueExW(HKEY,LPCWSTR,DWORD,DWORD,CONST BYTE*,DWORD);
#define     RegSetValueEx WINELIB_NAME_AW(RegSetValueEx)

#ifdef __cplusplus
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif  /* __WINE_WINREG_H */
