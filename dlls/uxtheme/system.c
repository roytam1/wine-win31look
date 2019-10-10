/*
 * Win32 5.1 Theme system
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

#include "config.h"

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "wingdi.h"
#include "winreg.h"
#include "shlwapi.h"
#include "uxtheme.h"
#include "tmschema.h"

#include "uxthemedll.h"
#include "msstyles.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(uxtheme);

/***********************************************************************
 * Defines and global variables
 */

static const WCHAR szThemeManager[] = {
    'S','o','f','t','w','a','r','e','\\',
    'M','i','c','r','o','s','o','f','t','\\',
    'W','i','n','d','o','w','s','\\',
    'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
    'T','h','e','m','e','M','a','n','a','g','e','r','\0'
};
static const WCHAR szThemeActive[] = {'T','h','e','m','e','A','c','t','i','v','e','\0'};
static const WCHAR szSizeName[] = {'S','i','z','e','N','a','m','e','\0'};
static const WCHAR szColorName[] = {'C','o','l','o','r','N','a','m','e','\0'};
static const WCHAR szDllName[] = {'D','l','l','N','a','m','e','\0'};

static const WCHAR szIniDocumentation[] = {'d','o','c','u','m','e','n','t','a','t','i','o','n','\0'};

HINSTANCE hDllInst;

DWORD dwThemeAppProperties = STAP_ALLOW_NONCLIENT | STAP_ALLOW_CONTROLS;
ATOM atWindowTheme;
ATOM atSubAppName;
ATOM atSubIdList;

BOOL bThemeActive = FALSE;
WCHAR szCurrentTheme[MAX_PATH];
WCHAR szCurrentColor[64];
WCHAR szCurrentSize[64];

/***********************************************************************/

/***********************************************************************
 *      UXTHEME_LoadTheme
 *
 * Set the current active theme from the registry
 */
void UXTHEME_LoadTheme()
{
    HKEY hKey;
    LONG buffsize;
    HRESULT hr;
    WCHAR tmp[10];
    PTHEME_FILE pt;

    /* Get current theme configuration */
    if(!RegOpenKeyW(HKEY_CURRENT_USER, szThemeManager, &hKey)) {
        TRACE("Loading theme config\n");
        buffsize = sizeof(tmp)/sizeof(tmp[0]);
        if(!RegQueryValueExW(hKey, szThemeActive, NULL, NULL, (LPBYTE)tmp, &buffsize)) {
            bThemeActive = (tmp[0] != '0');
        }
        else {
            bThemeActive = FALSE;
            TRACE("Failed to get ThemeActive: %ld\n", GetLastError());
        }
        buffsize = sizeof(szCurrentColor)/sizeof(szCurrentColor[0]);
        if(RegQueryValueExW(hKey, szColorName, NULL, NULL, (LPBYTE)szCurrentColor, &buffsize))
            szCurrentColor[0] = '\0';
        buffsize = sizeof(szCurrentSize)/sizeof(szCurrentSize[0]);
        if(RegQueryValueExW(hKey, szSizeName, NULL, NULL, (LPBYTE)szCurrentSize, &buffsize))
            szCurrentSize[0] = '\0';
        if(SHRegGetPathW(hKey, NULL, szDllName, szCurrentTheme, 0))
            szCurrentTheme[0] = '\0';
        RegCloseKey(hKey);
    }
    else
        TRACE("Failed to open theme registry key\n");

    if(bThemeActive) {
        /* Make sure the theme requested is actually valid */
        hr = MSSTYLES_OpenThemeFile(szCurrentTheme,
                                    szCurrentColor[0]?szCurrentColor:NULL,
                                    szCurrentSize[0]?szCurrentSize:NULL,
                                    &pt);
        if(FAILED(hr)) {
            bThemeActive = FALSE;
            szCurrentTheme[0] = '\0';
            szCurrentColor[0] = '\0';
            szCurrentSize[0] = '\0';
        }
        else {
            /* Make sure the global color & size match the theme */
            lstrcpynW(szCurrentColor, pt->pszSelectedColor, sizeof(szCurrentColor)/sizeof(szCurrentColor[0]));
            lstrcpynW(szCurrentSize, pt->pszSelectedSize, sizeof(szCurrentSize)/sizeof(szCurrentSize[0]));

            MSSTYLES_SetActiveTheme(pt);
            TRACE("Theme active: %s %s %s\n", debugstr_w(szCurrentTheme),
                debugstr_w(szCurrentColor), debugstr_w(szCurrentSize));
            MSSTYLES_CloseThemeFile(pt);
        }
    }
    if(!bThemeActive) {
        MSSTYLES_SetActiveTheme(NULL);
        TRACE("Themeing not active\n");
    }
}

/***********************************************************************
 *      UXTHEME_SetActiveTheme
 *
 * Change the current active theme
 */
HRESULT UXTHEME_SetActiveTheme(PTHEME_FILE tf)
{
    HKEY hKey;
    WCHAR tmp[2];
    HRESULT hr;

    hr = MSSTYLES_SetActiveTheme(tf);
    if(FAILED(hr))
        return hr;
    if(tf) {
        bThemeActive = TRUE;
        lstrcpynW(szCurrentTheme, tf->szThemeFile, sizeof(szCurrentTheme)/sizeof(szCurrentTheme[0]));
        lstrcpynW(szCurrentColor, tf->pszSelectedColor, sizeof(szCurrentColor)/sizeof(szCurrentColor[0]));
        lstrcpynW(szCurrentSize, tf->pszSelectedSize, sizeof(szCurrentSize)/sizeof(szCurrentSize[0]));
    }
    else {
        bThemeActive = FALSE;
        szCurrentTheme[0] = '\0';
        szCurrentColor[0] = '\0';
        szCurrentSize[0] = '\0';
    }

    TRACE("Writing theme config to registry\n");
    if(!RegCreateKeyW(HKEY_CURRENT_USER, szThemeManager, &hKey)) {
        tmp[0] = bThemeActive?'1':'0';
        tmp[1] = '\0';
        RegSetValueExW(hKey, szThemeActive, 0, REG_SZ, (const BYTE*)tmp, sizeof(WCHAR)*2);
        if(bThemeActive) {
            RegSetValueExW(hKey, szColorName, 0, REG_SZ, (const BYTE*)szCurrentColor, lstrlenW(szCurrentColor)+1);
            RegSetValueExW(hKey, szSizeName, 0, REG_SZ, (const BYTE*)szCurrentSize, lstrlenW(szCurrentSize)+1);
            RegSetValueExW(hKey, szDllName, 0, REG_SZ, (const BYTE*)szCurrentTheme, lstrlenW(szCurrentTheme)+1);
        }
        else {
            RegDeleteValueW(hKey, szColorName);
            RegDeleteValueW(hKey, szSizeName);
            RegDeleteValueW(hKey, szDllName);

        }
        RegCloseKey(hKey);
    }
    else
        TRACE("Failed to open theme registry key\n");
    return hr;
}

/***********************************************************************
 *      UXTHEME_InitSystem
 */
void UXTHEME_InitSystem(HINSTANCE hInst)
{
    const WCHAR szWindowTheme[] = {
        'u','x','_','t','h','e','m','e','\0'
    };
    const WCHAR szSubAppName[] = {
        'u','x','_','s','u','b','a','p','p','\0'
    };
    const WCHAR szSubIdList[] = {
        'u','x','_','s','u','b','i','d','l','s','t','\0'
    };

    hDllInst = hInst;

    atWindowTheme = GlobalAddAtomW(szWindowTheme);
    atSubAppName  = GlobalAddAtomW(szSubAppName);
    atSubIdList   = GlobalAddAtomW(szSubIdList);

    UXTHEME_LoadTheme();
}

/***********************************************************************
 *      IsAppThemed                                         (UXTHEME.@)
 */
BOOL WINAPI IsAppThemed(void)
{
    return IsThemeActive();
}

/***********************************************************************
 *      IsThemeActive                                       (UXTHEME.@)
 */
BOOL WINAPI IsThemeActive(void)
{
    TRACE("\n");
    return bThemeActive;
}

/***********************************************************************
 *      EnableTheming                                       (UXTHEME.@)
 *
 * NOTES
 * This is a global and persistent change
 */
HRESULT WINAPI EnableTheming(BOOL fEnable)
{
    HKEY hKey;
    WCHAR szEnabled[] = {'0','\0'};

    TRACE("(%d)\n", fEnable);

    if(fEnable != bThemeActive) {
        bThemeActive = fEnable;
        if(bThemeActive) szEnabled[0] = '1';
        if(!RegOpenKeyW(HKEY_CURRENT_USER, szThemeManager, &hKey)) {
            RegSetValueExW(hKey, szThemeActive, 0, REG_SZ, (LPBYTE)szEnabled, sizeof(WCHAR));
            RegCloseKey(hKey);
        }
        PostMessageW(HWND_BROADCAST, WM_THEMECHANGED, 0, 0);
    }
    return S_OK;
}

/***********************************************************************
 *      UXTHEME_SetWindowProperty
 *
 * I'm using atoms as there may be large numbers of duplicated strings
 * and they do the work of keeping memory down as a cause of that quite nicely
 */
HRESULT UXTHEME_SetWindowProperty(HWND hwnd, ATOM aProp, LPCWSTR pszValue)
{
    ATOM oldValue = (ATOM)(size_t)RemovePropW(hwnd, MAKEINTATOMW(aProp));
    if(oldValue)
        DeleteAtom(oldValue);
    if(pszValue) {
        ATOM atValue = AddAtomW(pszValue);
        if(!atValue
           || !SetPropW(hwnd, MAKEINTATOMW(aProp), (LPWSTR)MAKEINTATOMW(atValue))) {
            HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
            if(atValue) DeleteAtom(atValue);
            return hr;
        }
    }
    return S_OK;
}

LPWSTR UXTHEME_GetWindowProperty(HWND hwnd, ATOM aProp, LPWSTR pszBuffer, int dwLen)
{
    ATOM atValue = (ATOM)(size_t)GetPropW(hwnd, MAKEINTATOMW(aProp));
    if(atValue) {
        if(GetAtomNameW(atValue, pszBuffer, dwLen))
            return pszBuffer;
        TRACE("property defined, but unable to get value\n");
    }
    return NULL;
}

/***********************************************************************
 *      OpenThemeData                                       (UXTHEME.@)
 */
HTHEME WINAPI OpenThemeData(HWND hwnd, LPCWSTR pszClassList)
{
    WCHAR szAppBuff[256];
    WCHAR szClassBuff[256];
    LPCWSTR pszAppName;
    LPCWSTR pszUseClassList;
    HTHEME hTheme;
    TRACE("(%p,%s)\n", hwnd, debugstr_w(pszClassList));
    if(!bThemeActive)
        return NULL;

    pszAppName = UXTHEME_GetWindowProperty(hwnd, atSubAppName, szAppBuff, sizeof(szAppBuff)/sizeof(szAppBuff[0]));
    /* If SetWindowTheme was used on the window, that overrides the class list passed to this function */
    pszUseClassList = UXTHEME_GetWindowProperty(hwnd, atSubIdList, szClassBuff, sizeof(szClassBuff)/sizeof(szClassBuff[0]));
    if(!pszUseClassList)
        pszUseClassList = pszClassList;

    hTheme = MSSTYLES_OpenThemeClass(pszAppName, pszUseClassList);
    if(IsWindow(hwnd))
        SetPropW(hwnd, MAKEINTATOMW(atWindowTheme), hTheme);
    return hTheme;
}

/***********************************************************************
 *      GetWindowTheme                                      (UXTHEME.@)
 *
 * Retrieve the last theme opened for a window
 */
HTHEME WINAPI GetWindowTheme(HWND hwnd)
{
    TRACE("(%p)\n", hwnd);
    return GetPropW(hwnd, MAKEINTATOMW(atWindowTheme));
}

/***********************************************************************
 *      SetWindowTheme                                      (UXTHEME.@)
 *
 * Persistent through the life of the window, even after themes change
 */
HRESULT WINAPI SetWindowTheme(HWND hwnd, LPCWSTR pszSubAppName,
                              LPCWSTR pszSubIdList)
{
    HRESULT hr;
    TRACE("(%p,%s,%s)\n", hwnd, debugstr_w(pszSubAppName),
          debugstr_w(pszSubIdList));
    hr = UXTHEME_SetWindowProperty(hwnd, atSubAppName, pszSubAppName);
    if(SUCCEEDED(hr))
        hr = UXTHEME_SetWindowProperty(hwnd, atSubIdList, pszSubIdList);
    if(SUCCEEDED(hr))
        PostMessageW(hwnd, WM_THEMECHANGED, 0, 0);
    return hr;
}

/***********************************************************************
 *      GetCurrentThemeName                                 (UXTHEME.@)
 */
HRESULT WINAPI GetCurrentThemeName(LPWSTR pszThemeFileName, int dwMaxNameChars,
                                   LPWSTR pszColorBuff, int cchMaxColorChars,
                                   LPWSTR pszSizeBuff, int cchMaxSizeChars)
{
    if(!bThemeActive)
        return E_PROP_ID_UNSUPPORTED;
    if(pszThemeFileName) lstrcpynW(pszThemeFileName, szCurrentTheme, dwMaxNameChars);
    if(pszColorBuff) lstrcpynW(pszColorBuff, szCurrentColor, cchMaxColorChars);
    if(pszSizeBuff) lstrcpynW(pszSizeBuff, szCurrentSize, cchMaxSizeChars);
    return S_OK;
}

/***********************************************************************
 *      GetThemeAppProperties                               (UXTHEME.@)
 */
DWORD WINAPI GetThemeAppProperties(void)
{
    return dwThemeAppProperties;
}

/***********************************************************************
 *      SetThemeAppProperties                               (UXTHEME.@)
 */
void WINAPI SetThemeAppProperties(DWORD dwFlags)
{
    TRACE("(0x%08lx)\n", dwFlags);
    dwThemeAppProperties = dwFlags;
}

/***********************************************************************
 *      CloseThemeData                                      (UXTHEME.@)
 */
HRESULT WINAPI CloseThemeData(HTHEME hTheme)
{
    TRACE("(%p)\n", hTheme);
    if(!hTheme)
        return E_HANDLE;
    return MSSTYLES_CloseThemeClass(hTheme);
}

/***********************************************************************
 *      HitTestThemeBackground                              (UXTHEME.@)
 */
HRESULT WINAPI HitTestThemeBackground(HTHEME hTheme, HDC hdc, int iPartId,
                                     int iStateId, DWORD dwOptions,
                                     const RECT *pRect, HRGN hrgn,
                                     POINT ptTest, WORD *pwHitTestCode)
{
    FIXME("%d %d 0x%08lx: stub\n", iPartId, iStateId, dwOptions);
    if(!hTheme)
        return E_HANDLE;
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/***********************************************************************
 *      IsThemePartDefined                                  (UXTHEME.@)
 */
BOOL WINAPI IsThemePartDefined(HTHEME hTheme, int iPartId, int iStateId)
{
    TRACE("(%p,%d,%d)\n", hTheme, iPartId, iStateId);
    if(MSSTYLES_FindPartState(hTheme, iPartId, iStateId, NULL))
        return TRUE;
    return FALSE;
}

/***********************************************************************
 *      GetThemeDocumentationProperty                       (UXTHEME.@)
 *
 * Try and retrieve the documentation property from string resources
 * if that fails, get it from the [documentation] section of themes.ini
 */
HRESULT WINAPI GetThemeDocumentationProperty(LPCWSTR pszThemeName,
                                             LPCWSTR pszPropertyName,
                                             LPWSTR pszValueBuff,
                                             int cchMaxValChars)
{
    const WORD wDocToRes[] = {
        TMT_DISPLAYNAME,5000,
        TMT_TOOLTIP,5001,
        TMT_COMPANY,5002,
        TMT_AUTHOR,5003,
        TMT_COPYRIGHT,5004,
        TMT_URL,5005,
        TMT_VERSION,5006,
        TMT_DESCRIPTION,5007
    };

    PTHEME_FILE pt;
    HRESULT hr;
    int i;
    int iDocId;
    TRACE("(%s,%s,%p,%d)\n", debugstr_w(pszThemeName), debugstr_w(pszPropertyName),
          pszValueBuff, cchMaxValChars);

    hr = MSSTYLES_OpenThemeFile(pszThemeName, NULL, NULL, &pt);
    if(FAILED(hr)) return hr;

    /* Try to load from string resources */
    hr = E_PROP_ID_UNSUPPORTED;
    if(MSSTYLES_LookupProperty(pszPropertyName, NULL, &iDocId)) {
        for(i=0; i<sizeof(wDocToRes)/sizeof(wDocToRes[0]); i+=2) {
            if(wDocToRes[i] == iDocId) {
                if(LoadStringW(pt->hTheme, wDocToRes[i+1], pszValueBuff, cchMaxValChars)) {
                    hr = S_OK;
                    break;
                }
            }
        }
    }
    /* If loading from string resource failed, try getting it from the theme.ini */
    if(FAILED(hr)) {
        PUXINI_FILE uf = MSSTYLES_GetThemeIni(pt);
        if(UXINI_FindSection(uf, szIniDocumentation)) {
            LPCWSTR lpValue;
            DWORD dwLen;
            if(UXINI_FindValue(uf, pszPropertyName, &lpValue, &dwLen)) {
                lstrcpynW(pszValueBuff, lpValue, min(dwLen+1,cchMaxValChars));
                hr = S_OK;
            }
        }
        UXINI_CloseINI(uf);
    }

    MSSTYLES_CloseThemeFile(pt);
    return hr;
}

/**********************************************************************
 *      Undocumented functions
 */

/**********************************************************************
 *      QueryThemeServices                                 (UXTHEME.1)
 *
 * RETURNS
 *     some kind of status flag
 */
DWORD WINAPI QueryThemeServices()
{
    FIXME("stub\n");
    return 3; /* This is what is returned under XP in most cases */
}


/**********************************************************************
 *      OpenThemeFile                                      (UXTHEME.2)
 *
 * Opens a theme file, which can be used to change the current theme, etc
 *
 * PARAMS
 *     pszThemeFileName    Path to a msstyles theme file
 *     pszColorName        Color defined in the theme, eg. NormalColor
 *     pszSizeName         Size defined in the theme, eg. NormalSize
 *     hThemeFile          Handle to theme file
 */
HRESULT WINAPI OpenThemeFile(LPCWSTR pszThemeFileName, LPCWSTR pszColorName,
                             LPCWSTR pszSizeName, HTHEMEFILE *hThemeFile,
                             DWORD unknown)
{
    TRACE("(%s,%s,%s,%p,%ld)\n", debugstr_w(pszThemeFileName),
          debugstr_w(pszColorName), debugstr_w(pszSizeName),
          hThemeFile, unknown);
    return MSSTYLES_OpenThemeFile(pszThemeFileName, pszColorName, pszSizeName, (PTHEME_FILE*)hThemeFile);
}

/**********************************************************************
 *      CloseThemeFile                                     (UXTHEME.3)
 *
 * Releases theme file handle returned by OpenThemeFile
 *
 * PARAMS
 *     hThemeFile           Handle to theme file
 */
HRESULT WINAPI CloseThemeFile(HTHEMEFILE hThemeFile)
{
    TRACE("(%p)\n", hThemeFile);
    MSSTYLES_CloseThemeFile(hThemeFile);
    return S_OK;
}

/**********************************************************************
 *      ApplyTheme                                         (UXTHEME.4)
 *
 * Set a theme file to be the currently active theme
 *
 * PARAMS
 *     hThemeFile           Handle to theme file
 *     unknown              See notes
 *     hWnd                 Window requesting the theme change
 *
 * NOTES
 * I'm not sure what the second parameter is (the datatype is likely wrong), other then this:
 * Under XP if I pass
 * char b[] = "";
 *   the theme is applied with the screen redrawing really badly (flickers)
 * char b[] = "\0"; where \0 can be one or more of any character, makes no difference
 *   the theme is applied smoothly (screen does not flicker)
 * char *b = "\0" or NULL; where \0 can be zero or more of any character, makes no difference
 *   the function fails returning invalid parameter...very strange
 */
HRESULT WINAPI ApplyTheme(HTHEMEFILE hThemeFile, char *unknown, HWND hWnd)
{
    HRESULT hr;
    TRACE("(%p,%s,%p)\n", hThemeFile, unknown, hWnd);
    hr = UXTHEME_SetActiveTheme(hThemeFile);
    PostMessageW(HWND_BROADCAST, WM_THEMECHANGED, 0, 0);
    return hr;
}

/**********************************************************************
 *      GetThemeDefaults                                   (UXTHEME.7)
 *
 * Get the default color & size for a theme
 *
 * PARAMS
 *     pszThemeFileName    Path to a msstyles theme file
 *     pszColorName        Buffer to receive the default color name
 *     dwColorNameLen      Length, in characters, of color name buffer
 *     pszSizeName         Buffer to receive the default size name
 *     dwSizeNameLen       Length, in characters, of size name buffer
 */
HRESULT WINAPI GetThemeDefaults(LPCWSTR pszThemeFileName, LPWSTR pszColorName,
                                DWORD dwColorNameLen, LPWSTR pszSizeName,
                                DWORD dwSizeNameLen)
{
    PTHEME_FILE pt;
    HRESULT hr;
    TRACE("(%s,%p,%ld,%p,%ld)\n", debugstr_w(pszThemeFileName),
          pszColorName, dwColorNameLen,
          pszSizeName, dwSizeNameLen);

    hr = MSSTYLES_OpenThemeFile(pszThemeFileName, NULL, NULL, &pt);
    if(FAILED(hr)) return hr;

    lstrcpynW(pszColorName, pt->pszSelectedColor, dwColorNameLen);
    lstrcpynW(pszSizeName, pt->pszSelectedSize, dwSizeNameLen);

    MSSTYLES_CloseThemeFile(pt);
    return S_OK;
}

/**********************************************************************
 *      EnumThemes                                         (UXTHEME.8)
 *
 * Enumerate available themes, calls specified EnumThemeProc for each
 * theme found. Passes lpData through to callback function.
 *
 * PARAMS
 *     pszThemePath        Path containing themes
 *     callback            Called for each theme found in path
 *     lpData              Passed through to callback
 */
HRESULT WINAPI EnumThemes(LPCWSTR pszThemePath, EnumThemeProc callback,
                          LPVOID lpData)
{
    FIXME("%s: stub\n", debugstr_w(pszThemePath));
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/**********************************************************************
 *      EnumThemeColors                                    (UXTHEME.9)
 *
 * Enumerate theme colors available with a particular size
 *
 * PARAMS
 *     pszThemeFileName    Path to a msstyles theme file
 *     pszSizeName         Theme size to enumerate available colors
 *                         If NULL the default theme size is used
 *     dwColorNum          Color index to retrieve, increment from 0
 *     pszColorName        Output color name
 *
 * RETURNS
 *     S_OK on success
 *     E_PROP_ID_UNSUPPORTED when dwColorName does not refer to a color
 *          or when pszSizeName does not refer to a valid size
 *
 * NOTES
 * XP fails with E_POINTER when pszColorName points to a buffer smaller then 605
 * characters
 *
 * Not very efficient that I'm opening & validating the theme every call, but
 * this is undocumented and almost never called..
 * (and this is how windows works too)
 */
HRESULT WINAPI EnumThemeColors(LPWSTR pszThemeFileName, LPWSTR pszSizeName,
                               DWORD dwColorNum, LPWSTR pszColorName)
{
    PTHEME_FILE pt;
    HRESULT hr;
    LPWSTR tmp;
    TRACE("(%s,%s,%ld)\n", debugstr_w(pszThemeFileName),
          debugstr_w(pszSizeName), dwColorNum);

    hr = MSSTYLES_OpenThemeFile(pszThemeFileName, NULL, pszSizeName, &pt);
    if(FAILED(hr)) return hr;

    tmp = pt->pszAvailColors;
    while(dwColorNum && *tmp) {
        dwColorNum--;
        tmp += lstrlenW(tmp)+1;
    }
    if(!dwColorNum && *tmp) {
        TRACE("%s\n", debugstr_w(tmp));
        lstrcpyW(pszColorName, tmp);
    }
    else
        hr = E_PROP_ID_UNSUPPORTED;

    MSSTYLES_CloseThemeFile(pt);
    return hr;
}

/**********************************************************************
 *      EnumThemeSizes                                     (UXTHEME.10)
 *
 * Enumerate theme colors available with a particular size
 *
 * PARAMS
 *     pszThemeFileName    Path to a msstyles theme file
 *     pszColorName        Theme color to enumerate available sizes
 *                         If NULL the default theme color is used
 *     dwSizeNum           Size index to retrieve, increment from 0
 *     pszSizeName         Output size name
 *
 * RETURNS
 *     S_OK on success
 *     E_PROP_ID_UNSUPPORTED when dwSizeName does not refer to a size
 *          or when pszColorName does not refer to a valid color
 *
 * NOTES
 * XP fails with E_POINTER when pszSizeName points to a buffer smaller then 605
 * characters
 *
 * Not very efficient that I'm opening & validating the theme every call, but
 * this is undocumented and almost never called..
 * (and this is how windows works too)
 */
HRESULT WINAPI EnumThemeSizes(LPWSTR pszThemeFileName, LPWSTR pszColorName,
                              DWORD dwSizeNum, LPWSTR pszSizeName)
{
    PTHEME_FILE pt;
    HRESULT hr;
    LPWSTR tmp;
    TRACE("(%s,%s,%ld)\n", debugstr_w(pszThemeFileName),
          debugstr_w(pszColorName), dwSizeNum);

    hr = MSSTYLES_OpenThemeFile(pszThemeFileName, pszColorName, NULL, &pt);
    if(FAILED(hr)) return hr;

    tmp = pt->pszAvailSizes;
    while(dwSizeNum && *tmp) {
        dwSizeNum--;
        tmp += lstrlenW(tmp)+1;
    }
    if(!dwSizeNum && *tmp) {
        TRACE("%s\n", debugstr_w(tmp));
        lstrcpyW(pszSizeName, tmp);
    }
    else
        hr = E_PROP_ID_UNSUPPORTED;

    MSSTYLES_CloseThemeFile(pt);
    return hr;
}

/**********************************************************************
 *      ParseThemeIniFile                                  (UXTHEME.11)
 *
 * Enumerate data in a theme INI file.
 *
 * PARAMS
 *     pszIniFileName      Path to a theme ini file
 *     pszUnknown          Cannot be NULL, L"" is valid
 *     callback            Called for each found entry
 *     lpData              Passed through to callback
 *
 * RETURNS
 *     S_OK on success
 *     0x800706488 (Unknown property) when enumeration is canceled from callback
 *
 * NOTES
 * When pszUnknown is NULL the callback is never called, the value does not seem to surve
 * any other purpose
 */
HRESULT WINAPI ParseThemeIniFile(LPCWSTR pszIniFileName, LPWSTR pszUnknown,
                                 ParseThemeIniFileProc callback, LPVOID lpData)
{
    FIXME("%s %s: stub\n", debugstr_w(pszIniFileName), debugstr_w(pszUnknown));
    return ERROR_CALL_NOT_IMPLEMENTED;
}
