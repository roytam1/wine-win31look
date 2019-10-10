/*
 * The parameters of many functions changes between different OS versions
 * (NT uses Unicode strings, 95 uses ASCII strings)
 *
 * Copyright 1997 Marcus Meissner
 *           1998 J�rgen Schmied
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

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "winerror.h"
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "wine/debug.h"
#include "winnls.h"

#include "shellapi.h"
#include "objbase.h"
#include "shlguid.h"
#include "wingdi.h"
#include "winuser.h"
#include "shlobj.h"
#include "shell32_main.h"
#include "undocshell.h"
#include "pidl.h"
#include "shlwapi.h"
#include "commdlg.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);
WINE_DECLARE_DEBUG_CHANNEL(pidl);

/* FIXME: !!! move CREATEMRULIST and flags to header file !!! */
/*        !!! it is in both here and comctl32undoc.c      !!! */
typedef struct tagCREATEMRULIST
{
    DWORD  cbSize;        /* size of struct */
    DWORD  nMaxItems;     /* max no. of items in list */
    DWORD  dwFlags;       /* see below */
    HKEY   hKey;          /* root reg. key under which list is saved */
    LPCSTR lpszSubKey;    /* reg. subkey */
    PROC   lpfnCompare;   /* item compare proc */
} CREATEMRULISTA, *LPCREATEMRULISTA;

/* dwFlags */
#define MRUF_STRING_LIST  0 /* list will contain strings */
#define MRUF_BINARY_LIST  1 /* list will contain binary data */
#define MRUF_DELAYED_SAVE 2 /* only save list order to reg. is FreeMRUList */

extern HANDLE WINAPI CreateMRUListA(LPCREATEMRULISTA lpcml);
extern DWORD  WINAPI FreeMRUList(HANDLE hMRUList);
extern INT    WINAPI AddMRUData(HANDLE hList, LPCVOID lpData, DWORD cbData);
extern INT    WINAPI FindMRUData(HANDLE hList, LPCVOID lpData, DWORD cbData, LPINT lpRegNum);
extern INT    WINAPI EnumMRUListA(HANDLE hList, INT nItemPos, LPVOID lpBuffer, DWORD nBufferSize);

/*************************************************************************
 * ParseFieldA					[internal]
 *
 * copies a field from a ',' delimited string
 *
 * first field is nField = 1
 */
DWORD WINAPI ParseFieldA(
	LPCSTR src,
	DWORD nField,
	LPSTR dst,
	DWORD len)
{
	WARN("(%s,0x%08lx,%p,%ld) semi-stub.\n",debugstr_a(src),nField,dst,len);

	if (!src || !src[0] || !dst || !len)
	  return 0;

	/* skip n fields delimited by ',' */
	while (nField > 1)
	{
	  if (*src=='\0') return FALSE;
	  if (*(src++)==',') nField--;
	}

	/* copy part till the next ',' to dst */
	while ( *src!='\0' && *src!=',' && (len--)>0 ) *(dst++)=*(src++);

	/* finalize the string */
	*dst=0x0;

	return TRUE;
}

/*************************************************************************
 * ParseFieldW			[internal]
 *
 * copies a field from a ',' delimited string
 *
 * first field is nField = 1
 */
DWORD WINAPI ParseFieldW(LPCWSTR src, DWORD nField, LPWSTR dst, DWORD len)
{
	WARN("(%s,0x%08lx,%p,%ld) semi-stub.\n", debugstr_w(src), nField, dst, len);

	if (!src || !src[0] || !dst || !len)
	  return 0;

	/* skip n fields delimited by ',' */
	while (nField > 1)
	{
	  if (*src == 0x0) return FALSE;
	  if (*src++ == ',') nField--;
	}

	/* copy part till the next ',' to dst */
	while ( *src != 0x0 && *src != ',' && (len--)>0 ) *(dst++) = *(src++);

	/* finalize the string */
	*dst = 0x0;

	return TRUE;
}

/*************************************************************************
 * ParseField			[SHELL32.58]
 */
DWORD WINAPI ParseFieldAW(LPCVOID src, DWORD nField, LPVOID dst, DWORD len)
{
	if (SHELL_OsIsUnicode())
	  return ParseFieldW(src, nField, dst, len);
	return ParseFieldA(src, nField, dst, len);
}

/*************************************************************************
 * GetFileNameFromBrowse			[SHELL32.63]
 *
 */
BOOL WINAPI GetFileNameFromBrowse(
	HWND hwndOwner,
	LPSTR lpstrFile,
	DWORD nMaxFile,
	LPCSTR lpstrInitialDir,
	LPCSTR lpstrDefExt,
	LPCSTR lpstrFilter,
	LPCSTR lpstrTitle)
{
    HMODULE hmodule;
    FARPROC pGetOpenFileNameA;
    OPENFILENAMEA ofn;
    BOOL ret;

    TRACE("%p, %s, %ld, %s, %s, %s, %s)\n",
	  hwndOwner, lpstrFile, nMaxFile, lpstrInitialDir, lpstrDefExt,
	  lpstrFilter, lpstrTitle);

    hmodule = LoadLibraryA("comdlg32.dll");
    if(!hmodule) return FALSE;
    pGetOpenFileNameA = GetProcAddress(hmodule, "GetOpenFileNameA");
    if(!pGetOpenFileNameA)
    {
	FreeLibrary(hmodule);
	return FALSE;
    }

    memset(&ofn, 0, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner;
    ofn.lpstrFilter = lpstrFilter;
    ofn.lpstrFile = lpstrFile;
    ofn.nMaxFile = nMaxFile;
    ofn.lpstrInitialDir = lpstrInitialDir;
    ofn.lpstrTitle = lpstrTitle;
    ofn.lpstrDefExt = lpstrDefExt;
    ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;
    ret = pGetOpenFileNameA(&ofn);

    FreeLibrary(hmodule);
    return ret;
}

/*************************************************************************
 * SHGetSetSettings				[SHELL32.68]
 */
VOID WINAPI SHGetSetSettings(LPSHELLSTATE lpss, DWORD dwMask, BOOL bSet)
{
  if(bSet)
  {
    FIXME("%p 0x%08lx TRUE\n", lpss, dwMask);
  }
  else
  {
    SHGetSettings((LPSHELLFLAGSTATE)lpss,dwMask);
  }
}

/*************************************************************************
 * SHGetSettings				[SHELL32.@]
 *
 * NOTES
 *  the registry path are for win98 (tested)
 *  and possibly are the same in nt40
 *
 */
VOID WINAPI SHGetSettings(LPSHELLFLAGSTATE lpsfs, DWORD dwMask)
{
	HKEY	hKey;
	DWORD	dwData;
	DWORD	dwDataSize = sizeof (DWORD);

	TRACE("(%p 0x%08lx)\n",lpsfs,dwMask);

	if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
				 0, 0, 0, KEY_ALL_ACCESS, 0, &hKey, 0))
	  return;

	if ( (SSF_SHOWEXTENSIONS & dwMask) && !RegQueryValueExA(hKey, "HideFileExt", 0, 0, (LPBYTE)&dwData, &dwDataSize))
	  lpsfs->fShowExtensions  = ((dwData == 0) ?  0 : 1);

	if ( (SSF_SHOWINFOTIP & dwMask) && !RegQueryValueExA(hKey, "ShowInfoTip", 0, 0, (LPBYTE)&dwData, &dwDataSize))
	  lpsfs->fShowInfoTip  = ((dwData == 0) ?  0 : 1);

	if ( (SSF_DONTPRETTYPATH & dwMask) && !RegQueryValueExA(hKey, "DontPrettyPath", 0, 0, (LPBYTE)&dwData, &dwDataSize))
	  lpsfs->fDontPrettyPath  = ((dwData == 0) ?  0 : 1);

	if ( (SSF_HIDEICONS & dwMask) && !RegQueryValueExA(hKey, "HideIcons", 0, 0, (LPBYTE)&dwData, &dwDataSize))
	  lpsfs->fHideIcons  = ((dwData == 0) ?  0 : 1);

	if ( (SSF_MAPNETDRVBUTTON & dwMask) && !RegQueryValueExA(hKey, "MapNetDrvBtn", 0, 0, (LPBYTE)&dwData, &dwDataSize))
	  lpsfs->fMapNetDrvBtn  = ((dwData == 0) ?  0 : 1);

	if ( (SSF_SHOWATTRIBCOL & dwMask) && !RegQueryValueExA(hKey, "ShowAttribCol", 0, 0, (LPBYTE)&dwData, &dwDataSize))
	  lpsfs->fShowAttribCol  = ((dwData == 0) ?  0 : 1);

	if (((SSF_SHOWALLOBJECTS | SSF_SHOWSYSFILES) & dwMask) && !RegQueryValueExA(hKey, "Hidden", 0, 0, (LPBYTE)&dwData, &dwDataSize))
	{ if (dwData == 0)
	  { if (SSF_SHOWALLOBJECTS & dwMask)	lpsfs->fShowAllObjects  = 0;
	    if (SSF_SHOWSYSFILES & dwMask)	lpsfs->fShowSysFiles  = 0;
	  }
	  else if (dwData == 1)
	  { if (SSF_SHOWALLOBJECTS & dwMask)	lpsfs->fShowAllObjects  = 1;
	    if (SSF_SHOWSYSFILES & dwMask)	lpsfs->fShowSysFiles  = 0;
	  }
	  else if (dwData == 2)
	  { if (SSF_SHOWALLOBJECTS & dwMask)	lpsfs->fShowAllObjects  = 0;
	    if (SSF_SHOWSYSFILES & dwMask)	lpsfs->fShowSysFiles  = 1;
	  }
	}
	RegCloseKey (hKey);

	TRACE("-- 0x%04x\n", *(WORD*)lpsfs);
}

/*************************************************************************
 * SHShellFolderView_Message			[SHELL32.73]
 *
 * Send a message to an explorer cabinet window.
 *
 * PARAMS
 *  hwndCabinet [I] The window containing the shellview to communicate with
 *  dwMessage   [I] The SFVM message to send
 *  dwParam     [I] Message parameter
 *
 * RETURNS
 *  fixme.
 *
 * NOTES
 *  Message SFVM_REARRANGE = 1
 *
 *    This message gets sent when a column gets clicked to instruct the
 *    shell view to re-sort the item list. dwParam identifies the column
 *    that was clicked.
 */
int WINAPI SHShellFolderView_Message(
	HWND hwndCabinet,
	DWORD dwMessage,
	DWORD dwParam)
{
	FIXME("%p %08lx %08lx stub\n",hwndCabinet, dwMessage, dwParam);
	return 0;
}

/*************************************************************************
 * RegisterShellHook				[SHELL32.181]
 *
 * Register a shell hook.
 *
 * PARAMS
 *      hwnd   [I]  Window handle
 *      dwType [I]  Type of hook.
 *
 * NOTES
 *     Exported by ordinal
 */
BOOL WINAPI RegisterShellHook(
	HWND hWnd,
	DWORD dwType)
{
	FIXME("(%p,0x%08lx):stub.\n",hWnd, dwType);
	return TRUE;
}

/*************************************************************************
 * ShellMessageBoxW				[SHELL32.182]
 *
 * See ShellMessageBoxA.
 */
int WINAPIV ShellMessageBoxW(
	HINSTANCE hInstance,
	HWND hWnd,
	LPCWSTR lpText,
	LPCWSTR lpCaption,
	UINT uType,
	...)
{
	WCHAR	szText[100],szTitle[100];
	LPCWSTR pszText = szText, pszTitle = szTitle, pszTemp;
	va_list args;
	int	ret;

	va_start(args, uType);
	/* wvsprintfA(buf,fmt, args); */

	TRACE("(%08lx,%08lx,%p,%p,%08x)\n",
	(DWORD)hInstance,(DWORD)hWnd,lpText,lpCaption,uType);

	if (!HIWORD(lpCaption))
	  LoadStringW(hInstance, (DWORD)lpCaption, szTitle, sizeof(szTitle)/sizeof(szTitle[0]));
	else
	  pszTitle = lpCaption;

	if (!HIWORD(lpText))
	  LoadStringW(hInstance, (DWORD)lpText, szText, sizeof(szText)/sizeof(szText[0]));
	else
	  pszText = lpText;

	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING,
		       pszText, 0, 0, (LPWSTR)&pszTemp, 0, &args);

	va_end(args);

	ret = MessageBoxW(hWnd,pszTemp,pszTitle,uType);
	LocalFree((HLOCAL)pszTemp);
	return ret;
}

/*************************************************************************
 * ShellMessageBoxA				[SHELL32.183]
 *
 * Format and output an error message.
 *
 * PARAMS
 *  hInstance [I] Instance handle of message creator
 *  hWnd      [I] Window handle of message creator
 *  lpText    [I] Resource Id of title or LPSTR
 *  lpCaption [I] Resource Id of title or LPSTR
 *  uType     [I] Type of error message
 *
 * RETURNS
 *  A return value from MessageBoxA().
 *
 * NOTES
 *     Exported by ordinal
 */
int WINAPIV ShellMessageBoxA(
	HINSTANCE hInstance,
	HWND hWnd,
	LPCSTR lpText,
	LPCSTR lpCaption,
	UINT uType,
	...)
{
	char	szText[100],szTitle[100];
	LPCSTR  pszText = szText, pszTitle = szTitle, pszTemp;
	va_list args;
	int	ret;

	va_start(args, uType);
	/* wvsprintfA(buf,fmt, args); */

	TRACE("(%08lx,%08lx,%p,%p,%08x)\n",
	(DWORD)hInstance,(DWORD)hWnd,lpText,lpCaption,uType);

	if (!HIWORD(lpCaption))
	  LoadStringA(hInstance, (DWORD)lpCaption, szTitle, sizeof(szTitle));
	else
	  pszTitle = lpCaption;

	if (!HIWORD(lpText))
	  LoadStringA(hInstance, (DWORD)lpText, szText, sizeof(szText));
	else
	  pszText = lpText;

	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING,
		       pszText, 0, 0, (LPSTR)&pszTemp, 0, &args);

	va_end(args);

	ret = MessageBoxA(hWnd,pszTemp,pszTitle,uType);
	LocalFree((HLOCAL)pszTemp);
	return ret;
}

/*************************************************************************
 * SHRegisterDragDrop				[SHELL32.86]
 *
 * NOTES
 *     exported by ordinal
 */
HRESULT WINAPI SHRegisterDragDrop(
	HWND hWnd,
	LPDROPTARGET pDropTarget)
{
	FIXME("(%p,%p):stub.\n", hWnd, pDropTarget);
	if (GetShellOle()) return pRegisterDragDrop(hWnd, pDropTarget);
        return 0;
}

/*************************************************************************
 * SHRevokeDragDrop				[SHELL32.87]
 *
 * NOTES
 *     exported by ordinal
 */
HRESULT WINAPI SHRevokeDragDrop(HWND hWnd)
{
    FIXME("(%p):stub.\n",hWnd);
    if (GetShellOle()) return pRevokeDragDrop(hWnd);
    return 0;
}

/*************************************************************************
 * SHDoDragDrop					[SHELL32.88]
 *
 * NOTES
 *     exported by ordinal
 */
HRESULT WINAPI SHDoDragDrop(
	HWND hWnd,
	LPDATAOBJECT lpDataObject,
	LPDROPSOURCE lpDropSource,
	DWORD dwOKEffect,
	LPDWORD pdwEffect)
{
    FIXME("(%p %p %p 0x%08lx %p):stub.\n",
    hWnd, lpDataObject, lpDropSource, dwOKEffect, pdwEffect);
	if (GetShellOle()) return pDoDragDrop(lpDataObject, lpDropSource, dwOKEffect, pdwEffect);
        return 0;
}

/*************************************************************************
 * ArrangeWindows				[SHELL32.184]
 *
 */
WORD WINAPI ArrangeWindows(
	HWND hwndParent,
	DWORD dwReserved,
	LPCRECT lpRect,
	WORD cKids,
	CONST HWND * lpKids)
{
    FIXME("(%p 0x%08lx %p 0x%04x %p):stub.\n",
	   hwndParent, dwReserved, lpRect, cKids, lpKids);
    return 0;
}

/*************************************************************************
 * SignalFileOpen				[SHELL32.103]
 *
 * NOTES
 *     exported by ordinal
 */
DWORD WINAPI
SignalFileOpen (DWORD dwParam1)
{
    FIXME("(0x%08lx):stub.\n", dwParam1);

    return 0;
}

/*************************************************************************
 * SHADD_get_policy - helper function for SHAddToRecentDocs
 *
 * PARAMETERS
 *   policy    [IN]  policy name (null termed string) to find
 *   type      [OUT] ptr to DWORD to receive type
 *   buffer    [OUT] ptr to area to hold data retrieved
 *   len       [IN/OUT] ptr to DWORD holding size of buffer and getting
 *                      length filled
 *
 * RETURNS
 *   result of the SHQueryValueEx call
 */
static INT SHADD_get_policy(LPSTR policy, LPDWORD type, LPVOID buffer, LPDWORD len)
{
    HKEY Policy_basekey;
    INT ret;

    /* Get the key for the policies location in the registry
     */
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
		      "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
		      0, KEY_READ, &Policy_basekey)) {

	if (RegOpenKeyExA(HKEY_CURRENT_USER,
			  "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
			  0, KEY_READ, &Policy_basekey)) {
	    TRACE("No Explorer Policies location exists. Policy wanted=%s\n",
		  policy);
	    *len = 0;
	    return ERROR_FILE_NOT_FOUND;
	}
    }

    /* Retrieve the data if it exists
     */
    ret = SHQueryValueExA(Policy_basekey, policy, 0, type, buffer, len);
    RegCloseKey(Policy_basekey);
    return ret;
}


/*************************************************************************
 * SHADD_compare_mru - helper function for SHAddToRecentDocs
 *
 * PARAMETERS
 *   data1     [IN] data being looked for
 *   data2     [IN] data in MRU
 *   cbdata    [IN] length from FindMRUData call (not used)
 *
 * RETURNS
 *   position within MRU list that data was added.
 */
static INT CALLBACK SHADD_compare_mru(LPCVOID data1, LPCVOID data2, DWORD cbData)
{
    return lstrcmpiA(data1, data2);
}

/*************************************************************************
 * SHADD_create_add_mru_data - helper function for SHAddToRecentDocs
 *
 * PARAMETERS
 *   mruhandle    [IN] handle for created MRU list
 *   doc_name     [IN] null termed pure doc name
 *   new_lnk_name [IN] null termed path and file name for .lnk file
 *   buffer       [IN/OUT] 2048 byte area to consturct MRU data
 *   len          [OUT] ptr to int to receive space used in buffer
 *
 * RETURNS
 *   position within MRU list that data was added.
 */
static INT SHADD_create_add_mru_data(HANDLE mruhandle, LPSTR doc_name, LPSTR new_lnk_name,
                                     LPSTR buffer, INT *len)
{
    LPSTR ptr;
    INT wlen;

    /*FIXME: Document:
     *  RecentDocs MRU data structure seems to be:
     *    +0h   document file name w/ terminating 0h
     *    +nh   short int w/ size of remaining
     *    +n+2h 02h 30h, or 01h 30h, or 00h 30h  -  unknown
     *    +n+4h 10 bytes zeros  -   unknown
     *    +n+eh shortcut file name w/ terminating 0h
     *    +n+e+nh 3 zero bytes  -  unknown
     */

    /* Create the MRU data structure for "RecentDocs"
	 */
    ptr = buffer;
    lstrcpyA(ptr, doc_name);
    ptr += (lstrlenA(buffer) + 1);
    wlen= lstrlenA(new_lnk_name) + 1 + 12;
    *((short int*)ptr) = wlen;
    ptr += 2;   /* step past the length */
    *(ptr++) = 0x30;  /* unknown reason */
    *(ptr++) = 0;     /* unknown, but can be 0x00, 0x01, 0x02 */
    memset(ptr, 0, 10);
    ptr += 10;
    lstrcpyA(ptr, new_lnk_name);
    ptr += (lstrlenA(new_lnk_name) + 1);
    memset(ptr, 0, 3);
    ptr += 3;
    *len = ptr - buffer;

    /* Add the new entry into the MRU list
     */
    return AddMRUData(mruhandle, (LPCVOID)buffer, *len);
}

/*************************************************************************
 * SHAddToRecentDocs				[SHELL32.@]
 *
 * PARAMETERS
 *   uFlags  [IN] SHARD_PATH or SHARD_PIDL
 *   pv      [IN] string or pidl, NULL clears the list
 *
 * NOTES
 *     exported by name
 *
 * FIXME: ?? MSDN shows this as a VOID
 */
DWORD WINAPI SHAddToRecentDocs (UINT uFlags,LPCVOID pv)
{
/* If list is a string list lpfnCompare has the following prototype
 * int CALLBACK MRUCompareString(LPCSTR s1, LPCSTR s2)
 * for binary lists the prototype is
 * int CALLBACK MRUCompareBinary(LPCVOID data1, LPCVOID data2, DWORD cbData)
 * where cbData is the no. of bytes to compare.
 * Need to check what return value means identical - 0?
 */


    UINT olderrormode;
    HKEY HCUbasekey;
    CHAR doc_name[MAX_PATH];
    CHAR link_dir[MAX_PATH];
    CHAR new_lnk_filepath[MAX_PATH];
    CHAR new_lnk_name[MAX_PATH];
    IMalloc *ppM;
    LPITEMIDLIST pidl;
    HWND hwnd = 0;       /* FIXME:  get real window handle */
    INT ret;
    DWORD data[64], datalen, type;

    /*FIXME: Document:
     *  RecentDocs MRU data structure seems to be:
     *    +0h   document file name w/ terminating 0h
     *    +nh   short int w/ size of remaining
     *    +n+2h 02h 30h, or 01h 30h, or 00h 30h  -  unknown
     *    +n+4h 10 bytes zeros  -   unknown
     *    +n+eh shortcut file name w/ terminating 0h
     *    +n+e+nh 3 zero bytes  -  unknown
     */

    /* See if we need to do anything.
     */
    datalen = 64;
    ret=SHADD_get_policy( "NoRecentDocsHistory", &type, &data, &datalen);
    if ((ret > 0) && (ret != ERROR_FILE_NOT_FOUND)) {
	ERR("Error %d getting policy \"NoRecentDocsHistory\"\n", ret);
	return 0;
    }
    if (ret == ERROR_SUCCESS) {
	if (!( (type == REG_DWORD) ||
	       ((type == REG_BINARY) && (datalen == 4)) )) {
	    ERR("Error policy data for \"NoRecentDocsHistory\" not formated correctly, type=%ld, len=%ld\n",
		type, datalen);
	    return 0;
	}

	TRACE("policy value for NoRecentDocsHistory = %08lx\n", data[0]);
	/* now test the actual policy value */
	if ( data[0] != 0)
	    return 0;
    }

    /* Open key to where the necessary info is
     */
    /* FIXME: This should be done during DLL PROCESS_ATTACH (or THREAD_ATTACH)
     *        and the close should be done during the _DETACH. The resulting
     *        key is stored in the DLL global data.
     */
    if (RegCreateKeyExA(HKEY_CURRENT_USER,
			"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer",
			0, 0, 0, KEY_READ, 0, &HCUbasekey, 0)) {
	ERR("Failed to create 'Software\\Microsoft\\Windows\\CurrentVersion\\Explorer'\n");
	return 0;
    }

    /* Get path to user's "Recent" directory
     */
    if(SUCCEEDED(SHGetMalloc(&ppM))) {
	if (SUCCEEDED(SHGetSpecialFolderLocation(hwnd, CSIDL_RECENT,
						 &pidl))) {
	    SHGetPathFromIDListA(pidl, link_dir);
	    IMalloc_Free(ppM, pidl);
	}
	else {
	    /* serious issues */
	    link_dir[0] = 0;
	    ERR("serious issues 1\n");
	}
	IMalloc_Release(ppM);
    }
    else {
	/* serious issues */
	link_dir[0] = 0;
	ERR("serious issues 2\n");
    }
    TRACE("Users Recent dir %s\n", link_dir);

    /* If no input, then go clear the lists */
    if (!pv) {
	/* clear user's Recent dir
	 */

	/* FIXME: delete all files in "link_dir"
	 *
	 * while( more files ) {
	 *    lstrcpyA(old_lnk_name, link_dir);
	 *    PathAppendA(old_lnk_name, filenam);
	 *    DeleteFileA(old_lnk_name);
	 * }
	 */
	FIXME("should delete all files in %s\\ \n", link_dir);

	/* clear MRU list
	 */
	/* MS Bug ?? v4.72.3612.1700 of shell32 does the delete against
	 *  HKEY_LOCAL_MACHINE version of ...CurrentVersion\Explorer
	 *  and naturally it fails w/ rc=2. It should do it against
	 *  HKEY_CURRENT_USER which is where it is stored, and where
	 *  the MRU routines expect it!!!!
	 */
	RegDeleteKeyA(HCUbasekey, "RecentDocs");
	RegCloseKey(HCUbasekey);
	return 0;
    }

    /* Have data to add, the jobs to be done:
     *   1. Add document to MRU list in registry "HKCU\Software\
     *      Microsoft\Windows\CurrentVersion\Explorer\RecentDocs".
     *   2. Add shortcut to document in the user's Recent directory
     *      (CSIDL_RECENT).
     *   3. Add shortcut to Start menu's Documents submenu.
     */

    /* Get the pure document name from the input
     */
    if (uFlags & SHARD_PIDL) {
	SHGetPathFromIDListA((LPCITEMIDLIST) pv, doc_name);
    }
    else {
	lstrcpyA(doc_name, (LPSTR) pv);
    }
    TRACE("full document name %s\n", doc_name);
    PathStripPathA(doc_name);
    TRACE("stripped document name %s\n", doc_name);


    /* ***  JOB 1: Update registry for ...\Explorer\RecentDocs list  *** */

    {  /* on input needs:
	*      doc_name    -  pure file-spec, no path
	*      link_dir    -  path to the user's Recent directory
	*      HCUbasekey  -  key of ...Windows\CurrentVersion\Explorer" node
	* creates:
	*      new_lnk_name-  pure file-spec, no path for new .lnk file
	*      new_lnk_filepath
	*                  -  path and file name of new .lnk file
	*/
	CREATEMRULISTA mymru;
	HANDLE mruhandle;
	INT len, pos, bufused, err;
	INT i;
	DWORD attr;
	CHAR buffer[2048];
	CHAR *ptr;
	CHAR old_lnk_name[MAX_PATH];
	short int slen;

	mymru.cbSize = sizeof(CREATEMRULISTA);
	mymru.nMaxItems = 15;
	mymru.dwFlags = MRUF_BINARY_LIST | MRUF_DELAYED_SAVE;
	mymru.hKey = HCUbasekey;
	mymru.lpszSubKey = "RecentDocs";
	mymru.lpfnCompare = &SHADD_compare_mru;
	mruhandle = CreateMRUListA(&mymru);
	if (!mruhandle) {
	    /* MRU failed */
	    ERR("MRU processing failed, handle zero\n");
	    RegCloseKey(HCUbasekey);
	    return 0;
	}
	len = lstrlenA(doc_name);
	pos = FindMRUData(mruhandle, doc_name, len, 0);

	/* Now get the MRU entry that will be replaced
	 * and delete the .lnk file for it
	 */
	if ((bufused = EnumMRUListA(mruhandle, (pos == -1) ? 14 : pos,
                                    buffer, 2048)) != -1) {
	    ptr = buffer;
	    ptr += (lstrlenA(buffer) + 1);
	    slen = *((short int*)ptr);
	    ptr += 2;  /* skip the length area */
	    if (bufused >= slen + (ptr-buffer)) {
		/* buffer size looks good */
		ptr += 12; /* get to string */
		len = bufused - (ptr-buffer);  /* get length of buf remaining */
		if ((lstrlenA(ptr) > 0) && (lstrlenA(ptr) <= len-1)) {
		    /* appears to be good string */
		    lstrcpyA(old_lnk_name, link_dir);
		    PathAppendA(old_lnk_name, ptr);
		    if (!DeleteFileA(old_lnk_name)) {
			if ((attr = GetFileAttributesA(old_lnk_name)) == INVALID_FILE_ATTRIBUTES) {
			    if ((err = GetLastError()) != ERROR_FILE_NOT_FOUND) {
				ERR("Delete for %s failed, err=%d, attr=%08lx\n",
				    old_lnk_name, err, attr);
			    }
			    else {
				TRACE("old .lnk file %s did not exist\n",
				      old_lnk_name);
			    }
			}
			else {
			    ERR("Delete for %s failed, attr=%08lx\n",
				old_lnk_name, attr);
			}
		    }
		    else {
			TRACE("deleted old .lnk file %s\n", old_lnk_name);
		    }
		}
	    }
	}

	/* Create usable .lnk file name for the "Recent" directory
	 */
	wsprintfA(new_lnk_name, "%s.lnk", doc_name);
	lstrcpyA(new_lnk_filepath, link_dir);
	PathAppendA(new_lnk_filepath, new_lnk_name);
	i = 1;
	olderrormode = SetErrorMode(SEM_FAILCRITICALERRORS);
	while (GetFileAttributesA(new_lnk_filepath) != INVALID_FILE_ATTRIBUTES) {
	    i++;
	    wsprintfA(new_lnk_name, "%s (%u).lnk", doc_name, i);
	    lstrcpyA(new_lnk_filepath, link_dir);
	    PathAppendA(new_lnk_filepath, new_lnk_name);
	}
	SetErrorMode(olderrormode);
	TRACE("new shortcut will be %s\n", new_lnk_filepath);

	/* Now add the new MRU entry and data
	 */
	pos = SHADD_create_add_mru_data(mruhandle, doc_name, new_lnk_name,
					buffer, &len);
	FreeMRUList(mruhandle);
	TRACE("Updated MRU list, new doc is position %d\n", pos);
    }

    /* ***  JOB 2: Create shortcut in user's "Recent" directory  *** */

    {  /* on input needs:
	*      doc_name    -  pure file-spec, no path
	*      new_lnk_filepath
	*                  -  path and file name of new .lnk file
 	*      uFlags[in]  -  flags on call to SHAddToRecentDocs
	*      pv[in]      -  document path/pidl on call to SHAddToRecentDocs
	*/
	IShellLinkA *psl = NULL;
	IPersistFile *pPf = NULL;
	HRESULT hres;
	CHAR desc[MAX_PATH];
	WCHAR widelink[MAX_PATH];

	CoInitialize(0);

	hres = CoCreateInstance( &CLSID_ShellLink,
				 NULL,
				 CLSCTX_INPROC_SERVER,
				 &IID_IShellLinkA,
				 (LPVOID )&psl);
	if(SUCCEEDED(hres)) {

	    hres = IShellLinkA_QueryInterface(psl, &IID_IPersistFile,
					     (LPVOID *)&pPf);
	    if(FAILED(hres)) {
		/* bombed */
		ERR("failed QueryInterface for IPersistFile %08lx\n", hres);
		goto fail;
	    }

	    /* Set the document path or pidl */
	    if (uFlags & SHARD_PIDL) {
		hres = IShellLinkA_SetIDList(psl, (LPCITEMIDLIST) pv);
	    } else {
		hres = IShellLinkA_SetPath(psl, (LPCSTR) pv);
	    }
	    if(FAILED(hres)) {
		/* bombed */
		ERR("failed Set{IDList|Path} %08lx\n", hres);
		goto fail;
	    }

	    lstrcpyA(desc, "Shortcut to ");
	    lstrcatA(desc, doc_name);
	    hres = IShellLinkA_SetDescription(psl, desc);
	    if(FAILED(hres)) {
		/* bombed */
		ERR("failed SetDescription %08lx\n", hres);
		goto fail;
	    }

	    MultiByteToWideChar(CP_ACP, 0, new_lnk_filepath, -1,
				widelink, MAX_PATH);
	    /* create the short cut */
	    hres = IPersistFile_Save(pPf, widelink, TRUE);
	    if(FAILED(hres)) {
		/* bombed */
		ERR("failed IPersistFile::Save %08lx\n", hres);
		IPersistFile_Release(pPf);
		IShellLinkA_Release(psl);
		goto fail;
	    }
	    hres = IPersistFile_SaveCompleted(pPf, widelink);
	    IPersistFile_Release(pPf);
	    IShellLinkA_Release(psl);
	    TRACE("shortcut %s has been created, result=%08lx\n",
		  new_lnk_filepath, hres);
	}
	else {
	    ERR("CoCreateInstance failed, hres=%08lx\n", hres);
	}
    }

 fail:
    CoUninitialize();

    /* all done */
    RegCloseKey(HCUbasekey);
    return 0;
}

/*************************************************************************
 * SHCreateShellFolderViewEx			[SHELL32.174]
 *
 * NOTES
 *  see IShellFolder::CreateViewObject
 */
HRESULT WINAPI SHCreateShellFolderViewEx(
	LPCSHELLFOLDERVIEWINFO psvcbi, /* [in] shelltemplate struct */
	LPSHELLVIEW* ppv)              /* [out] IShellView pointer */
{
	IShellView * psf;
	HRESULT hRes;

	TRACE("sf=%p pidl=%p cb=%p mode=0x%08x parm=0x%08lx\n",
	  psvcbi->pshf, psvcbi->pidlFolder, psvcbi->lpfnCallback,
	  psvcbi->uViewMode, psvcbi->dwUser);

	psf = IShellView_Constructor(psvcbi->pshf);

	if (!psf)
	  return E_OUTOFMEMORY;

	IShellView_AddRef(psf);
	hRes = IShellView_QueryInterface(psf, &IID_IShellView, (LPVOID *)ppv);
	IShellView_Release(psf);

	return hRes;
}
/*************************************************************************
 *  SHWinHelp					[SHELL32.127]
 *
 */
HRESULT WINAPI SHWinHelp (DWORD v, DWORD w, DWORD x, DWORD z)
{	FIXME("0x%08lx 0x%08lx 0x%08lx 0x%08lx stub\n",v,w,x,z);
	return 0;
}
/*************************************************************************
 *  SHRunControlPanel [SHELL32.161]
 *
 */
HRESULT WINAPI SHRunControlPanel (DWORD x, DWORD z)
{	FIXME("0x%08lx 0x%08lx stub\n",x,z);
	return 0;
}

static LPUNKNOWN SHELL32_IExplorerInterface=0;
/*************************************************************************
 * SHSetInstanceExplorer			[SHELL32.176]
 *
 * NOTES
 *  Sets the interface
 */
HRESULT WINAPI SHSetInstanceExplorer (LPUNKNOWN lpUnknown)
{	TRACE("%p\n", lpUnknown);
	SHELL32_IExplorerInterface = lpUnknown;
	return (HRESULT) lpUnknown;
}
/*************************************************************************
 * SHGetInstanceExplorer			[SHELL32.@]
 *
 * NOTES
 *  gets the interface pointer of the explorer and a reference
 */
HRESULT WINAPI SHGetInstanceExplorer (LPUNKNOWN * lpUnknown)
{	TRACE("%p\n", lpUnknown);

	*lpUnknown = SHELL32_IExplorerInterface;

	if (!SHELL32_IExplorerInterface)
	  return E_FAIL;

	IUnknown_AddRef(SHELL32_IExplorerInterface);
	return NOERROR;
}
/*************************************************************************
 * SHFreeUnusedLibraries			[SHELL32.123]
 *
 * NOTES
 *  exported by name
 */
void WINAPI SHFreeUnusedLibraries (void)
{
	FIXME("stub\n");
}
/*************************************************************************
 * DAD_AutoScroll				[SHELL32.129]
 *
 */
BOOL WINAPI DAD_AutoScroll(HWND hwnd, AUTO_SCROLL_DATA *samples, LPPOINT pt)
{
    FIXME("hwnd = %p %p %p\n",hwnd,samples,pt);
    return 0;
}
/*************************************************************************
 * DAD_DragEnter				[SHELL32.130]
 *
 */
BOOL WINAPI DAD_DragEnter(HWND hwnd)
{
    FIXME("hwnd = %p\n",hwnd);
    return FALSE;
}
/*************************************************************************
 * DAD_DragEnterEx				[SHELL32.131]
 *
 */
BOOL WINAPI DAD_DragEnterEx(HWND hwnd, POINT p)
{
    FIXME("hwnd = %p (%ld,%ld)\n",hwnd,p.x,p.y);
    return FALSE;
}
/*************************************************************************
 * DAD_DragMove				[SHELL32.134]
 *
 */
BOOL WINAPI DAD_DragMove(POINT p)
{
    FIXME("(%ld,%ld)\n",p.x,p.y);
    return FALSE;
}
/*************************************************************************
 * DAD_DragLeave				[SHELL32.132]
 *
 */
BOOL WINAPI DAD_DragLeave(VOID)
{
    FIXME("\n");
    return FALSE;
}
/*************************************************************************
 * DAD_SetDragImage				[SHELL32.136]
 *
 * NOTES
 *  exported by name
 */
BOOL WINAPI DAD_SetDragImage(
	HIMAGELIST himlTrack,
	LPPOINT lppt)
{
	FIXME("%p %p stub\n",himlTrack, lppt);
  return 0;
}
/*************************************************************************
 * DAD_ShowDragImage				[SHELL32.137]
 *
 * NOTES
 *  exported by name
 */
BOOL WINAPI DAD_ShowDragImage(BOOL bShow)
{
	FIXME("0x%08x stub\n",bShow);
	return 0;
}

static const WCHAR szwCabLocation[] = {
  'S','o','f','t','w','a','r','e','\\',
  'M','i','c','r','o','s','o','f','t','\\',
  'W','i','n','d','o','w','s','\\',
  'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
  'E','x','p','l','o','r','e','r','\\',
  'C','a','b','i','n','e','t','S','t','a','t','e',0
};

static const WCHAR szwSettings[] = { 'S','e','t','t','i','n','g','s',0 };

/*************************************************************************
 * ReadCabinetState				[SHELL32.651] NT 4.0
 *
 */
BOOL WINAPI ReadCabinetState(CABINETSTATE *cs, int length)
{
	HKEY hkey = 0;
	DWORD type, r;

	TRACE("%p %d \n",cs,length);

	if( (cs == NULL) || (length < (int)sizeof(*cs))  )
		return FALSE;

	r = RegOpenKeyW( HKEY_CURRENT_USER, szwCabLocation, &hkey );
	if( r == ERROR_SUCCESS )
	{
		type = REG_BINARY;
		r = RegQueryValueExW( hkey, szwSettings, 
			NULL, &type, (LPBYTE)cs, (LPDWORD)&length );
		RegCloseKey( hkey );
			
	}

	/* if we can't read from the registry, create default values */
	if ( (r != ERROR_SUCCESS) || (cs->cLength < sizeof(*cs)) ||
		(cs->cLength != length) )
	{
		ERR("Initializing shell cabinet settings\n");
		memset(cs, 0, sizeof(*cs));
		cs->cLength          = sizeof(*cs);
		cs->nVersion         = 2;
		cs->fFullPathTitle   = FALSE;
		cs->fSaveLocalView   = TRUE;
		cs->fNotShell        = FALSE;
		cs->fSimpleDefault   = TRUE;
		cs->fDontShowDescBar = FALSE;
		cs->fNewWindowMode   = FALSE;
		cs->fShowCompColor   = FALSE;
		cs->fDontPrettyNames = FALSE;
		cs->fAdminsCreateCommonGroups = TRUE;
		cs->fMenuEnumFilter  = 96;
	}
	
	return TRUE;
}

/*************************************************************************
 * WriteCabinetState				[SHELL32.652] NT 4.0
 *
 */
BOOL WINAPI WriteCabinetState(CABINETSTATE *cs)
{
	DWORD r;
	HKEY hkey = 0;

	TRACE("%p\n",cs);

	if( cs == NULL )
		return FALSE;

	r = RegCreateKeyExW( HKEY_CURRENT_USER, szwCabLocation, 0,
		 NULL, 0, KEY_ALL_ACCESS, NULL, &hkey, NULL);
	if( r == ERROR_SUCCESS )
	{
		r = RegSetValueExW( hkey, szwSettings, 0, 
			REG_BINARY, (LPBYTE) cs, cs->cLength);

		RegCloseKey( hkey );
	}

	return (r==ERROR_SUCCESS);
}

/*************************************************************************
 * FileIconInit 				[SHELL32.660]
 *
 */
BOOL WINAPI FileIconInit(BOOL bFullInit)
{	FIXME("(%s)\n", bFullInit ? "true" : "false");
	return 0;
}
/*************************************************************************
 * IsUserAdmin					[SHELL32.680] NT 4.0
 *
 */
HRESULT WINAPI IsUserAdmin(void)
{	FIXME("stub\n");
	return TRUE;
}

/*************************************************************************
 * SHAllocShared				[SHELL32.520]
 *
 * NOTES
 *  parameter1 is return value from HeapAlloc
 *  parameter2 is equal to the size allocated with HeapAlloc
 *  parameter3 is return value from GetCurrentProcessId
 *
 *  the return value is posted as lParam with 0x402 (WM_USER+2) to somewhere
 *  WM_USER+2 could be the undocumented CWM_SETPATH
 *  the allocated memory contains a pidl
 */
HGLOBAL WINAPI SHAllocShared(LPVOID psrc, DWORD size, DWORD procID)
{	HGLOBAL hmem;
	LPVOID pmem;

	TRACE("ptr=%p size=0x%04lx procID=0x%04lx\n",psrc,size,procID);
	hmem = GlobalAlloc(GMEM_FIXED, size);
	if (!hmem)
	  return 0;

	pmem =  GlobalLock (hmem);

	if (! pmem)
	  return 0;

	memcpy (pmem, psrc, size);
	GlobalUnlock(hmem);
	return hmem;
}
/*************************************************************************
 * SHLockShared					[SHELL32.521]
 *
 * NOTES
 *  parameter1 is return value from SHAllocShared
 *  parameter2 is return value from GetCurrentProcessId
 *  the receiver of (WM_USER+2) tries to lock the HANDLE (?)
 *  the return value seems to be a memory address
 */
LPVOID WINAPI SHLockShared(HANDLE hmem, DWORD procID)
{	TRACE("handle=%p procID=0x%04lx\n",hmem,procID);
	return GlobalLock(hmem);
}
/*************************************************************************
 * SHUnlockShared				[SHELL32.522]
 *
 * NOTES
 *  parameter1 is return value from SHLockShared
 */
BOOL WINAPI SHUnlockShared(LPVOID pv)
{
	TRACE("%p\n",pv);
	return GlobalUnlock((HANDLE)pv);
}
/*************************************************************************
 * SHFreeShared					[SHELL32.523]
 *
 * NOTES
 *  parameter1 is return value from SHAllocShared
 *  parameter2 is return value from GetCurrentProcessId
 */
BOOL WINAPI SHFreeShared(
	HANDLE hMem,
	DWORD pid)
{
	TRACE("handle=%p 0x%04lx\n",hMem,pid);
	return (BOOL)GlobalFree(hMem);
}

/*************************************************************************
 * SetAppStartingCursor				[SHELL32.99]
 */
HRESULT WINAPI SetAppStartingCursor(HWND u, DWORD v)
{	FIXME("hwnd=%p 0x%04lx stub\n",u,v );
	return 0;
}
/*************************************************************************
 * SHLoadOLE					[SHELL32.151]
 *
 */
HRESULT WINAPI SHLoadOLE(DWORD u)
{	FIXME("0x%04lx stub\n",u);
	return S_OK;
}
/*************************************************************************
 * DriveType					[SHELL32.64]
 *
 */
HRESULT WINAPI DriveType(DWORD u)
{	FIXME("0x%04lx stub\n",u);
	return 0;
}
/*************************************************************************
 * SHAbortInvokeCommand				[SHELL32.198]
 *
 */
HRESULT WINAPI SHAbortInvokeCommand(void)
{	FIXME("stub\n");
	return 1;
}
/*************************************************************************
 * SHOutOfMemoryMessageBox			[SHELL32.126]
 *
 */
int WINAPI SHOutOfMemoryMessageBox(
	HWND hwndOwner,
	LPCSTR lpCaption,
	UINT uType)
{
	FIXME("%p %s 0x%08x stub\n",hwndOwner, lpCaption, uType);
	return 0;
}
/*************************************************************************
 * SHFlushClipboard				[SHELL32.121]
 *
 */
HRESULT WINAPI SHFlushClipboard(void)
{	FIXME("stub\n");
	return 1;
}

/*************************************************************************
 * SHWaitForFileToOpen				[SHELL32.97]
 *
 */
BOOL WINAPI SHWaitForFileToOpen(
	LPCITEMIDLIST pidl,
	DWORD dwFlags,
	DWORD dwTimeout)
{
	FIXME("%p 0x%08lx 0x%08lx stub\n", pidl, dwFlags, dwTimeout);
	return 0;
}

/************************************************************************
 *	@				[SHELL32.654]
 *
 * NOTES: first parameter seems to be a pointer (same as passed to WriteCabinetState)
 * second one could be a size (0x0c). The size is the same as the structure saved to
 * HCU\Software\Microsoft\Windows\CurrentVersion\Explorer\CabinetState
 * I'm (js) guessing: this one is just ReadCabinetState ;-)
 */
HRESULT WINAPI shell32_654 (CABINETSTATE *cs, int length)
{
	TRACE("%p %d\n",cs,length);
	return ReadCabinetState(cs,length);
}

/************************************************************************
 *	RLBuildListOfPaths			[SHELL32.146]
 *
 * NOTES
 *   builds a DPA
 */
DWORD WINAPI RLBuildListOfPaths (void)
{	FIXME("stub\n");
	return 0;
}
/************************************************************************
 *	SHValidateUNC				[SHELL32.173]
 *
 */
HRESULT WINAPI SHValidateUNC (DWORD x, DWORD y, DWORD z)
{
	FIXME("0x%08lx 0x%08lx 0x%08lx stub\n",x,y,z);
	return 0;
}

/************************************************************************
 *	DoEnvironmentSubstA			[SHELL32.@]
 *
 */
HRESULT WINAPI DoEnvironmentSubstA(LPSTR x, LPSTR y)
{
	FIXME("(%s, %s) stub\n", debugstr_a(x), debugstr_a(y));
	return 0;
}

/************************************************************************
 *	DoEnvironmentSubstW			[SHELL32.@]
 *
 */
HRESULT WINAPI DoEnvironmentSubstW(LPWSTR x, LPWSTR y)
{
	FIXME("(%s, %s): stub\n", debugstr_w(x), debugstr_w(y));
	return 0;
}

/************************************************************************
 *	DoEnvironmentSubst			[SHELL32.53]
 *
 */
HRESULT WINAPI DoEnvironmentSubstAW(LPVOID x, LPVOID y)
{
	if (SHELL_OsIsUnicode())
	  return DoEnvironmentSubstW(x, y);
	return DoEnvironmentSubstA(x, y);
}

/*************************************************************************
 *      @                             [SHELL32.243]
 *
 * Win98+ by-ordinal routine.  In Win98 this routine returns zero and
 * does nothing else.  Possibly this does something in NT or SHELL32 5.0?
 *
 */

BOOL WINAPI shell32_243(DWORD a, DWORD b)
{
  return FALSE;
}

/*************************************************************************
 *      @	[SHELL32.714]
 */
DWORD WINAPI SHELL32_714(LPVOID x)
{
 	FIXME("(%s)stub\n", debugstr_w(x));
	return 0;
}

/*************************************************************************
 *      SHAddFromPropSheetExtArray	[SHELL32.167]
 */
DWORD WINAPI SHAddFromPropSheetExtArray(DWORD a, DWORD b, DWORD c)
{
 	FIXME("(%08lx,%08lx,%08lx)stub\n", a, b, c);
	return 0;
}

/*************************************************************************
 *      SHCreatePropSheetExtArray	[SHELL32.168]
 */
DWORD WINAPI SHCreatePropSheetExtArray(DWORD a, LPCSTR b, DWORD c)
{
 	FIXME("(%08lx,%s,%08lx)stub\n", a, debugstr_a(b), c);
	return 0;
}

/*************************************************************************
 *      SHReplaceFromPropSheetExtArray	[SHELL32.170]
 */
DWORD WINAPI SHReplaceFromPropSheetExtArray(DWORD a, DWORD b, DWORD c, DWORD d)
{
 	FIXME("(%08lx,%08lx,%08lx,%08lx)stub\n", a, b, c, d);
	return 0;
}

/*************************************************************************
 *      SHDestroyPropSheetExtArray	[SHELL32.169]
 */
DWORD WINAPI SHDestroyPropSheetExtArray(DWORD a)
{
 	FIXME("(%08lx)stub\n", a);
	return 0;
}

/*************************************************************************
 *      CIDLData_CreateFromIDArray	[SHELL32.83]
 *
 *  Create IDataObject from PIDLs??
 */
HRESULT WINAPI CIDLData_CreateFromIDArray(
	LPCITEMIDLIST pidlFolder,
	DWORD cpidlFiles,
	LPCITEMIDLIST *lppidlFiles,
	LPDATAOBJECT *ppdataObject)
{
    UINT i;
    HWND hwnd = 0;   /*FIXME: who should be hwnd of owner? set to desktop */

    TRACE("(%p, %ld, %p, %p)\n", pidlFolder, cpidlFiles, lppidlFiles, ppdataObject);
    if (TRACE_ON(pidl))
    {
	pdump (pidlFolder);
	for (i=0; i<cpidlFiles; i++) pdump (lppidlFiles[i]);
    }
    *ppdataObject = IDataObject_Constructor( hwnd, pidlFolder,
					     lppidlFiles, cpidlFiles);
    if (*ppdataObject) return S_OK;
    return E_OUTOFMEMORY;
}

/*************************************************************************
 * SHCreateStdEnumFmtEtc			[SHELL32.74]
 *
 * NOTES
 *
 */
HRESULT WINAPI SHCreateStdEnumFmtEtc(
	DWORD cFormats,
	const FORMATETC *lpFormats,
	LPENUMFORMATETC *ppenumFormatetc)
{
	IEnumFORMATETC *pef;
	HRESULT hRes;
	TRACE("cf=%ld fe=%p pef=%p\n", cFormats, lpFormats, ppenumFormatetc);

	pef = IEnumFORMATETC_Constructor(cFormats, lpFormats);
	if (!pef)
	  return E_OUTOFMEMORY;

	IEnumFORMATETC_AddRef(pef);
	hRes = IEnumFORMATETC_QueryInterface(pef, &IID_IEnumFORMATETC, (LPVOID*)ppenumFormatetc);
	IEnumFORMATETC_Release(pef);

	return hRes;
}


/*************************************************************************
 *                              SHELL32_256
 */
HRESULT WINAPI SHELL32_256(LPDWORD lpdw0, LPDWORD lpdw1)
{
    HRESULT ret = S_OK;

    FIXME("stub %p 0x%08lx %p\n", lpdw0, lpdw0 ? *lpdw0 : 0, lpdw1);

    if (!lpdw0 || *lpdw0 != 0x10)
        ret = E_INVALIDARG;
    else
    {
        LPVOID lpdata = 0;/*LocalAlloc(GMEM_ZEROINIT, 0x4E4);*/

	if (!lpdata)
            ret = E_OUTOFMEMORY;
	else
	{
            /* Initialize and return unknown lpdata structure */
	}
    }

    return ret;
}
