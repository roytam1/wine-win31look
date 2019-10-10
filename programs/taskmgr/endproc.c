/*
 *  ReactOS Task Manager
 *
 *  endproc.c
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

void ProcessPage_OnEndProcess(void)
{
    LVITEM            lvitem;
    ULONG            Index;
    DWORD            dwProcessId;
    HANDLE            hProcess;
    TCHAR            strErrorText[260];

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

    if (MessageBox(hMainWnd, _T("WARNING: Terminating a process can cause undesired\nresults including loss of data and system instability. The\nprocess will not be given the chance to save its state or\ndata before it is terminated. Are you sure you want to\nterminate the process?"), _T("Task Manager Warning"), MB_YESNO|MB_ICONWARNING) != IDYES)
        return;

    hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, dwProcessId);

    if (!hProcess)
    {
        GetLastErrorText(strErrorText, 260);
        MessageBox(hMainWnd, strErrorText, _T("Unable to Terminate Process"), MB_OK|MB_ICONSTOP);
        return;
    }

    if (!TerminateProcess(hProcess, 0))
    {
        GetLastErrorText(strErrorText, 260);
        MessageBox(hMainWnd, strErrorText, _T("Unable to Terminate Process"), MB_OK|MB_ICONSTOP);
    }

    CloseHandle(hProcess);
}

void ProcessPage_OnEndProcessTree(void)
{
    LVITEM            lvitem;
    ULONG            Index;
    DWORD            dwProcessId;
    HANDLE            hProcess;
    TCHAR            strErrorText[260];

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

    if (MessageBox(hMainWnd, _T("WARNING: Terminating a process can cause undesired\nresults including loss of data and system instability. The\nprocess will not be given the chance to save its state or\ndata before it is terminated. Are you sure you want to\nterminate the process?"), _T("Task Manager Warning"), MB_YESNO|MB_ICONWARNING) != IDYES)
        return;

    hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, dwProcessId);

    if (!hProcess)
    {
        GetLastErrorText(strErrorText, 260);
        MessageBox(hMainWnd, strErrorText, _T("Unable to Terminate Process"), MB_OK|MB_ICONSTOP);
        return;
    }

    if (!TerminateProcess(hProcess, 0))
    {
        GetLastErrorText(strErrorText, 260);
        MessageBox(hMainWnd, strErrorText, _T("Unable to Terminate Process"), MB_OK|MB_ICONSTOP);
    }

    CloseHandle(hProcess);
}
