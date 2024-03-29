/*
 * Internal uxtheme defines & declarations
 *
 * Copyright (C) 2003 Kevin Koltzau
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

#ifndef __WINE_UXTHEMEDLL_H
#define __WINE_UXTHEMEDLL_H

typedef HANDLE HTHEMEFILE;

/**********************************************************************
 *              EnumThemeProc
 *
 * Callback function for EnumThemes.
 *
 * RETURNS
 *     TRUE to continue enumeration, FALSE to stop
 *
 * PARAMS
 *     lpReserved          Always 0
 *     pszThemeFileName    Full path to theme msstyles file
 *     pszThemeName        Display name for theme
 *     pszToolTip          Tooltip name for theme
 *     lpReserved2         Always 0
 *     lpData              Value passed through lpData from EnumThemes
 */
typedef BOOL (CALLBACK *EnumThemeProc)(LPVOID lpReserved, LPCWSTR pszThemeFileName,
                                       LPCWSTR pszThemeName, LPCWSTR pszToolTip, LPVOID lpReserved2,
                                       LPVOID lpData);

/**********************************************************************
 *              ParseThemeIniFileProc
 *
 * Callback function for ParseThemeIniFile.
 *
 * RETURNS
 *     TRUE to continue enumeration, FALSE to stop
 *
 * PARAMS
 *     dwType              Entry type
 *     pszParam1           Use defined by entry type
 *     pszParam2           Use defined by entry type
 *     pszParam3           Use defined by entry type
 *     dwParam             Use defined by entry type
 *     lpData              Value passed through lpData from ParseThemeIniFile
 *
 * NOTES
 * I don't know what the valid entry types are
 */
typedef BOOL (CALLBACK*ParseThemeIniFileProc)(DWORD dwType, LPWSTR pszParam1,
                                              LPWSTR pszParam2, LPWSTR pszParam3,
                                              DWORD dwParam, LPVOID lpData);

/* Declarations for undocumented functions for use internally */
DWORD WINAPI QueryThemeServices();
HRESULT WINAPI OpenThemeFile(LPCWSTR pszThemeFileName, LPCWSTR pszColorName,
                             LPCWSTR pszSizeName, HTHEMEFILE *hThemeFile,
                             DWORD unknown);
HRESULT WINAPI CloseThemeFile(HTHEMEFILE hThemeFile);
HRESULT WINAPI ApplyTheme(HTHEMEFILE hThemeFile, char *unknown, HWND hWnd);
HRESULT WINAPI GetThemeDefaults(LPCWSTR pszThemeFileName, LPWSTR pszColorName,
                                DWORD dwColorNameLen, LPWSTR pszSizeName,
                                DWORD dwSizeNameLen);
HRESULT WINAPI EnumThemes(LPCWSTR pszThemePath, EnumThemeProc callback,
                          LPVOID lpData);
HRESULT WINAPI EnumThemeColors(LPWSTR pszThemeFileName, LPWSTR pszSizeName,
                               DWORD dwColorNum, LPWSTR pszColorName);
HRESULT WINAPI EnumThemeSizes(LPWSTR pszThemeFileName, LPWSTR pszColorName,
                              DWORD dwSizeNum, LPWSTR pszSizeName);
HRESULT WINAPI ParseThemeIniFile(LPCWSTR pszIniFileName, LPWSTR pszUnknown,
                                 ParseThemeIniFileProc callback, LPVOID lpData);

extern void UXTHEME_InitSystem(HINSTANCE hInst);

#endif
