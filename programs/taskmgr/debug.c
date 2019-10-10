/*
 *  ReactOS Task Manager
 *
 *  debug.c
 *
 *  Copyright (C) 1999 - 2001  Brian Palmer  <brianp@reactos.org>
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

#define WIN32_LEAN_AND_MEAN    /* Exclude rarely-used stuff from Windows headers */
#include <windows.h>
#include <commctrl.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <stdio.h>
#include <winnt.h>
    
#include "taskmgr.h"
#include "perfdata.h"

void ProcessPage_OnDebug(void)
{
    LVITEM                lvitem;
    ULONG                Index;
    DWORD                dwProcessId;
    TCHAR                strErrorText[260];
    HKEY                hKey;
    TCHAR                strDebugPath[260];
    TCHAR                strDebugger[260];
    DWORD                dwDebuggerSize;
    PROCESS_INFORMATION    pi;
    STARTUPINFO            si;
    HANDLE                hDebugEvent;

    for (Index=0; Index<(ULONG)ListView_GetItemCount(hProcessPageListCtrl); Index++)
    {
        memset(&lvitem, 0, sizeof(LVITEM));

        lvitem.mask = LVIF_STATE;
        lvitem.stateMask = LVIS_SELECTED;
        lvitem.iItem = Index;

        ListView_GetItem(hProcessPageListCtrl, &lvitem);

        if (lvitem.state & LVIS_SELECTED)
            break;
    }

    dwProcessId = PerfDataGetProcessId(Index);

    if ((ListView_GetSelectedCount(hProcessPageListCtrl) != 1) || (dwProcessId == 0))
        return;

    if (MessageBox(hMainWnd, _T("WARNING: Debugging this process may result in loss of data.\nAre you sure you wish to attach the debugger?"), _T("Task Manager Warning"), MB_YESNO|MB_ICONWARNING) != IDYES)
    {
        GetLastErrorText(strErrorText, 260);
        MessageBox(hMainWnd, strErrorText, _T("Unable to Debug Process"), MB_OK|MB_ICONSTOP);
        return;
    }

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug"), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
    {
        GetLastErrorText(strErrorText, 260);
        MessageBox(hMainWnd, strErrorText, _T("Unable to Debug Process"), MB_OK|MB_ICONSTOP);
        return;
    }

    dwDebuggerSize = 260;
    if (RegQueryValueEx(hKey, _T("Debugger"), NULL, NULL, (LPBYTE)strDebugger, &dwDebuggerSize) != ERROR_SUCCESS)
    {
        GetLastErrorText(strErrorText, 260);
        MessageBox(hMainWnd, strErrorText, _T("Unable to Debug Process"), MB_OK|MB_ICONSTOP);
        RegCloseKey(hKey);
        return;
    }

    RegCloseKey(hKey);

    hDebugEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!hDebugEvent)
    {
        GetLastErrorText(strErrorText, 260);
        MessageBox(hMainWnd, strErrorText, _T("Unable to Debug Process"), MB_OK|MB_ICONSTOP);
        return;
    }

    wsprintf(strDebugPath, strDebugger, dwProcessId, hDebugEvent);

    memset(&pi, 0, sizeof(PROCESS_INFORMATION));
    memset(&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    if (!CreateProcess(NULL, strDebugPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        GetLastErrorText(strErrorText, 260);
        MessageBox(hMainWnd, strErrorText, _T("Unable to Debug Process"), MB_OK|MB_ICONSTOP);
    }

    CloseHandle(hDebugEvent);
}
