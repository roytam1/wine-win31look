/*
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

#ifndef __WINE_SHELLAPI_H
#define __WINE_SHELLAPI_H

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

#include <pshpack1.h>

DECLARE_HANDLE(HDROP);

/******************************************
 * DragObject
 */

typedef struct _DRAGINFOA
{	UINT uSize;
	POINT pt;
	BOOL fNC;
	LPSTR   lpFileList;
	DWORD grfKeyState;
} DRAGINFOA, *LPDRAGINFOA;

typedef struct _DRAGINFOW
{	UINT uSize;
	POINT pt;
	BOOL fNC;
	LPWSTR  lpFileList;
	DWORD grfKeyState;
} DRAGINFOW, *LPDRAGINFOW;

DECL_WINELIB_TYPE_AW(DRAGINFO)
DECL_WINELIB_TYPE_AW(LPDRAGINFO)

void	WINAPI DragAcceptFiles(HWND hWnd, BOOL b);
UINT	WINAPI DragQueryFileA(HDROP hDrop, UINT lFile, LPSTR lpszFile, UINT lLength);
UINT	WINAPI DragQueryFileW(HDROP hDrop, UINT lFile, LPWSTR lpszFile, UINT lLength);
#define        DragQueryFile WINELIB_NAME_AW(DragQueryFile)
void	WINAPI DragFinish(HDROP h);
BOOL	WINAPI DragQueryPoint(HDROP hDrop, POINT *p);

#define NIF_MESSAGE             0x00000001
#define NIF_ICON                0x00000002
#define NIF_TIP                 0x00000004

#define NIM_ADD                 0x00000000
#define NIM_MODIFY              0x00000001
#define NIM_DELETE              0x00000002



/******************************************
 * Application Bar
 */
#define ABM_NEW			0x00000000
#define ABM_REMOVE		0x00000001
#define ABM_QUERYPOS		0x00000002
#define ABM_SETPOS		0x00000003
#define ABM_GETSTATE		0x00000004
#define ABM_GETTASKBARPOS	0x00000005
#define ABM_ACTIVATE		0x00000006
#define ABM_GETAUTOHIDEBAR	0x00000007
#define ABM_SETAUTOHIDEBAR	0x00000008
#define ABM_WINDOWPOSCHANGED	0x00000009

#define ABN_STATECHANGE		0x00000000
#define ABN_POSCHANGED		0x00000001
#define ABN_FULLSCREENAPP	0x00000002
#define ABN_WINDOWARRANGE	0x00000003

#define ABS_AUTOHIDE		0x00000001
#define ABS_ALWAYSONTOP		0x00000002

#define ABE_LEFT		0
#define ABE_TOP			1
#define ABE_RIGHT		2
#define ABE_BOTTOM		3

typedef struct _AppBarData
{	DWORD	cbSize;
	HWND	hWnd;
	UINT	uCallbackMessage;
	UINT	uEdge;
	RECT	rc;
	LPARAM	lParam;
} APPBARDATA, *PAPPBARDATA;

UINT    WINAPI SHAppBarMessage(DWORD,PAPPBARDATA);

/******************************************
 * SHGetFileInfo
 */

#define SHGFI_LARGEICON         0x000000000     /* get large icon */
#define SHGFI_SMALLICON         0x000000001     /* get small icon */
#define SHGFI_OPENICON          0x000000002     /* get open icon */
#define SHGFI_SHELLICONSIZE     0x000000004     /* get shell size icon */
#define SHGFI_PIDL              0x000000008     /* pszPath is a pidl */
#define SHGFI_USEFILEATTRIBUTES 0x000000010     /* use passed dwFileAttribute */
#define SHGFI_UNKNOWN1          0x000000020
#define SHGFI_UNKNOWN2          0x000000040
#define SHGFI_UNKNOWN3          0x000000080
#define SHGFI_ICON              0x000000100     /* get icon */
#define SHGFI_DISPLAYNAME       0x000000200     /* get display name */
#define SHGFI_TYPENAME          0x000000400     /* get type name */
#define SHGFI_ATTRIBUTES        0x000000800     /* get attributes */
#define SHGFI_ICONLOCATION      0x000001000     /* get icon location */
#define SHGFI_EXETYPE           0x000002000     /* return exe type */
#define SHGFI_SYSICONINDEX      0x000004000     /* get system icon index */
#define SHGFI_LINKOVERLAY       0x000008000     /* put a link overlay on icon */
#define SHGFI_SELECTED          0x000010000     /* show icon in selected state */
#define SHGFI_ATTR_SPECIFIED    0x000020000     /* get only specified attributes */

typedef struct tagSHFILEINFOA
{	HICON	hIcon;			/* icon */
	int	iIcon;			/* icon index */
	DWORD	dwAttributes;		/* SFGAO_ flags */
	CHAR	szDisplayName[MAX_PATH];/* display name (or path) */
	CHAR	szTypeName[80];		/* type name */
} SHFILEINFOA;

typedef struct tagSHFILEINFOW
{	HICON	hIcon;			/* icon */
	int	iIcon;			/* icon index */
	DWORD	dwAttributes;		/* SFGAO_ flags */
	WCHAR	szDisplayName[MAX_PATH];/* display name (or path) */
	WCHAR	szTypeName[80];		/* type name */
} SHFILEINFOW;

DECL_WINELIB_TYPE_AW(SHFILEINFO)

DWORD	WINAPI SHGetFileInfoA(LPCSTR,DWORD,SHFILEINFOA*,UINT,UINT);
DWORD	WINAPI SHGetFileInfoW(LPCWSTR,DWORD,SHFILEINFOW*,UINT,UINT);
#define  SHGetFileInfo WINELIB_NAME_AW(SHGetFileInfo)

/******************************************
 * SHSetFileInfo
 */

/******************************************
* SHFileOperation
*/
#define FO_MOVE           0x0001
#define FO_COPY           0x0002
#define FO_DELETE         0x0003
#define FO_RENAME         0x0004

#define FOF_MULTIDESTFILES         0x0001
#define FOF_CONFIRMMOUSE           0x0002
#define FOF_SILENT                 0x0004
#define FOF_RENAMEONCOLLISION      0x0008
#define FOF_NOCONFIRMATION         0x0010
#define FOF_WANTMAPPINGHANDLE      0x0020
#define FOF_ALLOWUNDO              0x0040
#define FOF_FILESONLY              0x0080
#define FOF_SIMPLEPROGRESS         0x0100
#define FOF_NOCONFIRMMKDIR         0x0200
#define FOF_NOERRORUI              0x0400
#define FOF_NOCOPYSECURITYATTRIBS  0x0800
#define FOF_NORECURSION            0x1000  /* don't do recursion into directories */
#define FOF_NO_CONNECTED_ELEMENTS  0x2000  /* don't do connected files */
#define FOF_WANTNUKEWARNING        0x4000  /* during delete operation, warn if delete instead
                                              of recycling (even if FOF_NOCONFIRMATION) */
#define FOF_NORECURSEREPARSE       0x8000  /* don't do recursion into reparse points */

typedef WORD FILEOP_FLAGS;

#define PO_DELETE       0x0013
#define PO_RENAME       0x0014
#define PO_PORTCHANGE   0x0020
#define PO_REN_PORT     (PO_RENAME | PO_PORTCHANGE)

typedef WORD PRINTEROP_FLAGS;

typedef struct _SHFILEOPSTRUCTA
{ HWND          hwnd;
  UINT          wFunc;
  LPCSTR          pFrom;
  LPCSTR          pTo;
  FILEOP_FLAGS    fFlags;
  BOOL          fAnyOperationsAborted;
  LPVOID          hNameMappings;
  LPCSTR          lpszProgressTitle;
} SHFILEOPSTRUCTA, *LPSHFILEOPSTRUCTA;

typedef struct _SHFILEOPSTRUCTW
{ HWND          hwnd;
  UINT          wFunc;
  LPCWSTR         pFrom;
  LPCWSTR         pTo;
  FILEOP_FLAGS    fFlags;
  BOOL          fAnyOperationsAborted;
  LPVOID          hNameMappings;
  LPCWSTR         lpszProgressTitle;
} SHFILEOPSTRUCTW, *LPSHFILEOPSTRUCTW;

#define  SHFILEOPSTRUCT WINELIB_NAME_AW(SHFILEOPSTRUCT)
#define  LPSHFILEOPSTRUCT WINELIB_NAME_AW(LPSHFILEOPSTRUCT)

int	WINAPI SHFileOperationA (LPSHFILEOPSTRUCTA lpFileOp);
int	WINAPI SHFileOperationW (LPSHFILEOPSTRUCTW lpFileOp);
#define  SHFileOperation WINELIB_NAME_AW(SHFileOperation)

typedef struct _SHNAMEMAPPINGA
{
  LPSTR    pszOldPath;
  LPSTR    pszNewPath;
  int      cchOldPath;
  int      cchNewPath;
} SHNAMEMAPPINGA, *LPSHNAMEMAPPINGA;

typedef struct _SHNAMEMAPPINGW
{
  LPWSTR   pszOldPath;
  LPWSTR   pszNewPath;
  int      cchOldPath;
  int      cchNewPath;
} SHNAMEMAPPINGW, *LPSHNAMEMAPPINGW;

DECL_WINELIB_TYPE_AW(SHNAMEMAPPING)
DECL_WINELIB_TYPE_AW(LPSHNAMEMAPPING)

/******************************************
 * ShellExecute
 */
#define SE_ERR_SHARE            26
#define SE_ERR_ASSOCINCOMPLETE  27
#define SE_ERR_DDETIMEOUT       28
#define SE_ERR_DDEFAIL          29
#define SE_ERR_DDEBUSY          30
#define SE_ERR_NOASSOC          31

HINSTANCE	WINAPI ShellExecuteA(HWND,LPCSTR,LPCSTR,LPCSTR,LPCSTR,INT);
HINSTANCE	WINAPI ShellExecuteW(HWND,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,INT);
#define     ShellExecute WINELIB_NAME_AW(ShellExecute)

#define SE_ERR_FNF              2       /* file not found */
#define SE_ERR_PNF              3       /* path not found */
#define SE_ERR_ACCESSDENIED     5       /* access denied */
#define SE_ERR_OOM              8       /* out of memory */
#define SE_ERR_DLLNOTFOUND      32
#define SE_ERR_SHARE                    26
#define SE_ERR_ASSOCINCOMPLETE          27
#define SE_ERR_DDETIMEOUT               28
#define SE_ERR_DDEFAIL                  29
#define SE_ERR_DDEBUSY                  30
#define SE_ERR_NOASSOC                  31

#define SEE_MASK_CLASSNAME        0x00000001
#define SEE_MASK_CLASSKEY         0x00000003
#define SEE_MASK_IDLIST           0x00000004
#define SEE_MASK_INVOKEIDLIST     0x0000000c
#define SEE_MASK_ICON             0x00000010
#define SEE_MASK_HOTKEY           0x00000020
#define SEE_MASK_NOCLOSEPROCESS   0x00000040
#define SEE_MASK_CONNECTNETDRV    0x00000080
#define SEE_MASK_FLAG_DDEWAIT     0x00000100
#define SEE_MASK_DOENVSUBST       0x00000200
#define SEE_MASK_FLAG_NO_UI       0x00000400
#define SEE_MASK_UNICODE          0x00004000
#define SEE_MASK_NO_CONSOLE       0x00008000
#define SEE_MASK_ASYNCOK          0x00100000
#define SEE_MASK_HMONITOR         0x00200000

typedef struct _SHELLEXECUTEINFOA
{       DWORD cbSize;
        ULONG fMask;
        HWND hwnd;
        LPCSTR   lpVerb;
        LPCSTR   lpFile;
        LPCSTR   lpParameters;
        LPCSTR   lpDirectory;
       INT nShow;
        HINSTANCE hInstApp;
        /* Optional fields */
        LPVOID lpIDList;
        LPCSTR   lpClass;
        HKEY hkeyClass;
        DWORD dwHotKey;
        union
        { HANDLE hIcon;
          HANDLE hMonitor;
        } DUMMYUNIONNAME;
        HANDLE hProcess;
} SHELLEXECUTEINFOA, *LPSHELLEXECUTEINFOA;

typedef struct _SHELLEXECUTEINFOW
{       DWORD cbSize;
        ULONG fMask;
        HWND hwnd;
        LPCWSTR  lpVerb;
        LPCWSTR  lpFile;
        LPCWSTR  lpParameters;
        LPCWSTR  lpDirectory;
       INT nShow;
        HINSTANCE hInstApp;
        /* Optional fields*/
        LPVOID lpIDList;
        LPCWSTR  lpClass;
        HKEY hkeyClass;
        DWORD dwHotKey;
        union
        { HANDLE hIcon;
          HANDLE hMonitor;
        } DUMMYUNIONNAME;
        HANDLE hProcess;
} SHELLEXECUTEINFOW, *LPSHELLEXECUTEINFOW;

#define SHELLEXECUTEINFO   WINELIB_NAME_AW(SHELLEXECUTEINFO)
#define LPSHELLEXECUTEINFO WINELIB_NAME_AW(LPSHELLEXECUTEINFO)

BOOL WINAPI ShellExecuteExA(LPSHELLEXECUTEINFOA lpExecInfo);
BOOL WINAPI ShellExecuteExW(LPSHELLEXECUTEINFOW lpExecInfo);
#define ShellExecuteEx  WINELIB_NAME_AW(ShellExecuteEx)

void WINAPI WinExecErrorA(HWND hwnd,INT error, LPCSTR lpstrFileName, LPCSTR lpstrTitle);
void WINAPI WinExecErrorW(HWND hwnd,INT error, LPCWSTR lpstrFileName, LPCWSTR lpstrTitle);
#define WinExecError  WINELIB_NAME_AW(WinExecError)

/******************************************
 * Tray Notification
 */
typedef struct _NOTIFYICONDATAA
{	DWORD cbSize;
	HWND hWnd;
	UINT uID;
	UINT uFlags;
	UINT uCallbackMessage;
	HICON hIcon;
	CHAR szTip[64];
} NOTIFYICONDATAA, *PNOTIFYICONDATAA;

typedef struct _NOTIFYICONDATAW
{	DWORD cbSize;
	HWND hWnd;
	UINT uID;
	UINT uFlags;
	UINT uCallbackMessage;
	HICON hIcon;
	WCHAR szTip[64];
} NOTIFYICONDATAW, *PNOTIFYICONDATAW;

DECL_WINELIB_TYPE_AW(NOTIFYICONDATA)
DECL_WINELIB_TYPE_AW(PNOTIFYICONDATA)

BOOL WINAPI Shell_NotifyIconA(DWORD dwMessage, PNOTIFYICONDATAA lpData);
BOOL WINAPI Shell_NotifyIconW(DWORD dwMessage, PNOTIFYICONDATAW lpData);

#define Shell_NotifyIcon WINELIB_NAME_AW(Shell_NotifyIcon)

/******************************************
 * Links
 */

#define SHGNLI_PIDL        0x01
#define SHGNLI_PREFIXNAME  0x02
#define SHGNLI_NOUNIQUE    0x04
#define SHGNLI_NOLNK       0x08

BOOL WINAPI SHGetNewLinkInfoA(LPCSTR,LPCSTR,LPSTR,BOOL*,UINT);
BOOL WINAPI SHGetNewLinkInfoW(LPCWSTR,LPCWSTR,LPWSTR,BOOL*,UINT);
#define     SHGetNewLinkInfo WINELIB_NAME_AW(SHGetNewLinkInfo)

/******************************************
 * Misc
 */

LPWSTR*     WINAPI CommandLineToArgvW(LPCWSTR,int*);
HICON       WINAPI ExtractIconA(HINSTANCE,LPCSTR,UINT);
HICON       WINAPI ExtractIconW(HINSTANCE,LPCWSTR,UINT);
#define     ExtractIcon WINELIB_NAME_AW(ExtractIcon)
HICON       WINAPI ExtractAssociatedIconA(HINSTANCE,LPSTR,LPWORD);
HICON       WINAPI ExtractAssociatedIconW(HINSTANCE,LPWSTR,LPWORD);
#define     ExtractAssociatedIcon WINELIB_NAME_AW(ExtractAssociatedIcon)
HICON       WINAPI ExtractAssociatedIconExA(HINSTANCE,LPSTR,LPWORD,LPWORD);
HICON       WINAPI ExtractAssociatedIconExW(HINSTANCE,LPWSTR,LPWORD,LPWORD);
#define     ExtractAssociatedIconEx WINELIB_NAME_AW(ExtractAssociatedIconEx)
UINT        WINAPI ExtractIconExA(LPCSTR,INT,HICON*,HICON*,UINT);
UINT        WINAPI ExtractIconExW(LPCWSTR,INT,HICON*,HICON*,UINT);
#define     ExtractIconEx WINELIB_NAME_AW(ExtractIconEx)
UINT        WINAPI ExtractIconExAW(LPCVOID,INT,HICON*,HICON*,UINT);
HINSTANCE   WINAPI FindExecutableA(LPCSTR,LPCSTR,LPSTR);
HINSTANCE   WINAPI FindExecutableW(LPCWSTR,LPCWSTR,LPWSTR);
#define     FindExecutable WINELIB_NAME_AW(FindExecutable)
BOOL        WINAPI ShellAboutA(HWND,LPCSTR,LPCSTR,HICON);
BOOL        WINAPI ShellAboutW(HWND,LPCWSTR,LPCWSTR,HICON);
#define     ShellAbout WINELIB_NAME_AW(ShellAbout)
int         WINAPIV ShellMessageBoxA(HINSTANCE,HWND,LPCSTR,LPCSTR,UINT,...);
int         WINAPIV ShellMessageBoxW(HINSTANCE,HWND,LPCWSTR,LPCWSTR,UINT,...);
#define     ShellMessageBox WINELIB_NAME_AW(ShellMessageBox)

#ifdef __cplusplus
} /* extern "C" */
#endif /* defined(__cplusplus) */

#include <poppack.h>

#endif /* __WINE_SHELLAPI_H */
