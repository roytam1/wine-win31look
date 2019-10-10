/*
 *  ReactOS Task Manager
 *
 *  column.c
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
    
#include "taskmgr.h"
#include "column.h"

UINT    ColumnDataHints[25];

int                    InsertColumn(int nCol, LPCTSTR lpszColumnHeading, int nFormat, int nWidth, int nSubItem);
LRESULT CALLBACK    ColumnsDialogWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

void AddColumns(void)
{
    int        size;

    if (TaskManagerSettings.Column_ImageName)
        InsertColumn(0, _T("Image Name"), LVCFMT_LEFT, TaskManagerSettings.ColumnSizeArray[0], -1);
    if (TaskManagerSettings.Column_PID)
        InsertColumn(1, _T("PID"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[1], -1);
    if (TaskManagerSettings.Column_UserName)
        InsertColumn(2, _T("Username"), LVCFMT_LEFT, TaskManagerSettings.ColumnSizeArray[2], -1);
    if (TaskManagerSettings.Column_SessionID)
        InsertColumn(3, _T("Session ID"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[3], -1);
    if (TaskManagerSettings.Column_CPUUsage)
        InsertColumn(4, _T("CPU"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[4], -1);
    if (TaskManagerSettings.Column_CPUTime)
        InsertColumn(5, _T("CPU Time"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[5], -1);
    if (TaskManagerSettings.Column_MemoryUsage)
        InsertColumn(6, _T("Mem Usage"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[6], -1);
    if (TaskManagerSettings.Column_PeakMemoryUsage)
        InsertColumn(7, _T("Peak Mem Usage"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[7], -1);
    if (TaskManagerSettings.Column_MemoryUsageDelta)
        InsertColumn(8, _T("Mem Delta"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[8], -1);
    if (TaskManagerSettings.Column_PageFaults)
        InsertColumn(9, _T("Page Faults"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[9], -1);
    if (TaskManagerSettings.Column_PageFaultsDelta)
        InsertColumn(10, _T("PF Delta"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[10], -1);
    if (TaskManagerSettings.Column_VirtualMemorySize)
        InsertColumn(11, _T("VM Size"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[11], -1);
    if (TaskManagerSettings.Column_PagedPool)
        InsertColumn(12, _T("Paged Pool"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[12], -1);
    if (TaskManagerSettings.Column_NonPagedPool)
        InsertColumn(13, _T("NP Pool"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[13], -1);
    if (TaskManagerSettings.Column_BasePriority)
        InsertColumn(14, _T("Base Pri"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[14], -1);
    if (TaskManagerSettings.Column_HandleCount)
        InsertColumn(15, _T("Handles"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[15], -1);
    if (TaskManagerSettings.Column_ThreadCount)
        InsertColumn(16, _T("Threads"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[16], -1);
    if (TaskManagerSettings.Column_USERObjects)
        InsertColumn(17, _T("USER Objects"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[17], -1);
    if (TaskManagerSettings.Column_GDIObjects)
        InsertColumn(18, _T("GDI Objects"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[18], -1);
    if (TaskManagerSettings.Column_IOReads)
        InsertColumn(19, _T("I/O Reads"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[19], -1);
    if (TaskManagerSettings.Column_IOWrites)
        InsertColumn(20, _T("I/O Writes"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[20], -1);
    if (TaskManagerSettings.Column_IOOther)
        InsertColumn(21, _T("I/O Other"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[21], -1);
    if (TaskManagerSettings.Column_IOReadBytes)
        InsertColumn(22, _T("I/O Read Bytes"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[22], -1);
    if (TaskManagerSettings.Column_IOWriteBytes)
        InsertColumn(23, _T("I/O Write Bytes"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[23], -1);
    if (TaskManagerSettings.Column_IOOtherBytes)
        InsertColumn(24, _T("I/O Other Bytes"), LVCFMT_RIGHT, TaskManagerSettings.ColumnSizeArray[24], -1);

    size = SendMessage(hProcessPageHeaderCtrl, HDM_GETITEMCOUNT, 0, 0);
    SendMessage(hProcessPageHeaderCtrl, HDM_SETORDERARRAY, (WPARAM) size, (LPARAM) &TaskManagerSettings.ColumnOrderArray);

    UpdateColumnDataHints();
}

int InsertColumn(int nCol, LPCTSTR lpszColumnHeading, int nFormat, int nWidth, int nSubItem)
{
    LVCOLUMN    column;

    column.mask = LVCF_TEXT|LVCF_FMT;
    column.pszText = (LPTSTR)lpszColumnHeading;
    column.fmt = nFormat;

    if (nWidth != -1)
    {
        column.mask |= LVCF_WIDTH;
        column.cx = nWidth;
    }

    if (nSubItem != -1)
    {
        column.mask |= LVCF_SUBITEM;
        column.iSubItem = nSubItem;
    }

    return ListView_InsertColumn(hProcessPageListCtrl, nCol, &column);
}

void SaveColumnSettings(void)
{
    HDITEM    hditem;
    int        i;
    TCHAR    text[260];
    int        size;

    /* Reset column data */
    for (i=0; i<25; i++)
        TaskManagerSettings.ColumnOrderArray[i] = i;

    TaskManagerSettings.Column_ImageName = FALSE;
    TaskManagerSettings.Column_PID = FALSE;
    TaskManagerSettings.Column_CPUUsage = FALSE;
    TaskManagerSettings.Column_CPUTime = FALSE;
    TaskManagerSettings.Column_MemoryUsage = FALSE;
    TaskManagerSettings.Column_MemoryUsageDelta = FALSE;
    TaskManagerSettings.Column_PeakMemoryUsage = FALSE;
    TaskManagerSettings.Column_PageFaults = FALSE;
    TaskManagerSettings.Column_USERObjects = FALSE;
    TaskManagerSettings.Column_IOReads = FALSE;
    TaskManagerSettings.Column_IOReadBytes = FALSE;
    TaskManagerSettings.Column_SessionID = FALSE;
    TaskManagerSettings.Column_UserName = FALSE;
    TaskManagerSettings.Column_PageFaultsDelta = FALSE;
    TaskManagerSettings.Column_VirtualMemorySize = FALSE;
    TaskManagerSettings.Column_PagedPool = FALSE;
    TaskManagerSettings.Column_NonPagedPool = FALSE;
    TaskManagerSettings.Column_BasePriority = FALSE;
    TaskManagerSettings.Column_HandleCount = FALSE;
    TaskManagerSettings.Column_ThreadCount = FALSE;
    TaskManagerSettings.Column_GDIObjects = FALSE;
    TaskManagerSettings.Column_IOWrites = FALSE;
    TaskManagerSettings.Column_IOWriteBytes = FALSE;
    TaskManagerSettings.Column_IOOther = FALSE;
    TaskManagerSettings.Column_IOOtherBytes = FALSE;
    TaskManagerSettings.ColumnSizeArray[0] = 105;
    TaskManagerSettings.ColumnSizeArray[1] = 50;
    TaskManagerSettings.ColumnSizeArray[2] = 107;
    TaskManagerSettings.ColumnSizeArray[3] = 70;
    TaskManagerSettings.ColumnSizeArray[4] = 35;
    TaskManagerSettings.ColumnSizeArray[5] = 70;
    TaskManagerSettings.ColumnSizeArray[6] = 70;
    TaskManagerSettings.ColumnSizeArray[7] = 100;
    TaskManagerSettings.ColumnSizeArray[8] = 70;
    TaskManagerSettings.ColumnSizeArray[9] = 70;
    TaskManagerSettings.ColumnSizeArray[10] = 70;
    TaskManagerSettings.ColumnSizeArray[11] = 70;
    TaskManagerSettings.ColumnSizeArray[12] = 70;
    TaskManagerSettings.ColumnSizeArray[13] = 70;
    TaskManagerSettings.ColumnSizeArray[14] = 60;
    TaskManagerSettings.ColumnSizeArray[15] = 60;
    TaskManagerSettings.ColumnSizeArray[16] = 60;
    TaskManagerSettings.ColumnSizeArray[17] = 60;
    TaskManagerSettings.ColumnSizeArray[18] = 60;
    TaskManagerSettings.ColumnSizeArray[19] = 70;
    TaskManagerSettings.ColumnSizeArray[20] = 70;
    TaskManagerSettings.ColumnSizeArray[21] = 70;
    TaskManagerSettings.ColumnSizeArray[22] = 70;
    TaskManagerSettings.ColumnSizeArray[23] = 70;
    TaskManagerSettings.ColumnSizeArray[24] = 70;

    /* Get header order */
    size = SendMessage(hProcessPageHeaderCtrl, HDM_GETITEMCOUNT, 0, 0);
    SendMessage(hProcessPageHeaderCtrl, HDM_GETORDERARRAY, (WPARAM) size, (LPARAM) &TaskManagerSettings.ColumnOrderArray);

    /* Get visible columns */
    for (i=0; i<SendMessage(hProcessPageHeaderCtrl, HDM_GETITEMCOUNT, 0, 0); i++) {
        memset(&hditem, 0, sizeof(HDITEM));

        hditem.mask = HDI_TEXT|HDI_WIDTH;
        hditem.pszText = text;
        hditem.cchTextMax = 260;

        SendMessage(hProcessPageHeaderCtrl, HDM_GETITEM, i, (LPARAM) &hditem);

        if (_tcsicmp(text, _T("Image Name")) == 0)
        {
            TaskManagerSettings.Column_ImageName = TRUE;
            TaskManagerSettings.ColumnSizeArray[0] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("PID")) == 0)
        {
            TaskManagerSettings.Column_PID = TRUE;
            TaskManagerSettings.ColumnSizeArray[1] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("Username")) == 0)
        {
            TaskManagerSettings.Column_UserName = TRUE;
            TaskManagerSettings.ColumnSizeArray[2] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("Session ID")) == 0)
        {
            TaskManagerSettings.Column_SessionID = TRUE;
            TaskManagerSettings.ColumnSizeArray[3] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("CPU")) == 0)
        {
            TaskManagerSettings.Column_CPUUsage = TRUE;
            TaskManagerSettings.ColumnSizeArray[4] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("CPU Time")) == 0)
        {
            TaskManagerSettings.Column_CPUTime = TRUE;
            TaskManagerSettings.ColumnSizeArray[5] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("Mem Usage")) == 0)
        {
            TaskManagerSettings.Column_MemoryUsage = TRUE;
            TaskManagerSettings.ColumnSizeArray[6] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("Peak Mem Usage")) == 0)
        {
            TaskManagerSettings.Column_PeakMemoryUsage = TRUE;
            TaskManagerSettings.ColumnSizeArray[7] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("Mem Delta")) == 0)
        {
            TaskManagerSettings.Column_MemoryUsageDelta = TRUE;
            TaskManagerSettings.ColumnSizeArray[8] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("Page Faults")) == 0)
        {
            TaskManagerSettings.Column_PageFaults = TRUE;
            TaskManagerSettings.ColumnSizeArray[9] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("PF Delta")) == 0)
        {
            TaskManagerSettings.Column_PageFaultsDelta = TRUE;
            TaskManagerSettings.ColumnSizeArray[10] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("VM Size")) == 0)
        {
            TaskManagerSettings.Column_VirtualMemorySize = TRUE;
            TaskManagerSettings.ColumnSizeArray[11] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("Paged Pool")) == 0)
        {
            TaskManagerSettings.Column_PagedPool = TRUE;
            TaskManagerSettings.ColumnSizeArray[12] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("NP Pool")) == 0)
        {
            TaskManagerSettings.Column_NonPagedPool = TRUE;
            TaskManagerSettings.ColumnSizeArray[13] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("Base Pri")) == 0)
        {
            TaskManagerSettings.Column_BasePriority = TRUE;
            TaskManagerSettings.ColumnSizeArray[14] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("Handles")) == 0)
        {
            TaskManagerSettings.Column_HandleCount = TRUE;
            TaskManagerSettings.ColumnSizeArray[15] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("Threads")) == 0)
        {
            TaskManagerSettings.Column_ThreadCount = TRUE;
            TaskManagerSettings.ColumnSizeArray[16] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("USER Objects")) == 0)
        {
            TaskManagerSettings.Column_USERObjects = TRUE;
            TaskManagerSettings.ColumnSizeArray[17] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("GDI Objects")) == 0)
        {
            TaskManagerSettings.Column_GDIObjects = TRUE;
            TaskManagerSettings.ColumnSizeArray[18] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("I/O Reads")) == 0)
        {
            TaskManagerSettings.Column_IOReads = TRUE;
            TaskManagerSettings.ColumnSizeArray[19] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("I/O Writes")) == 0)
        {
            TaskManagerSettings.Column_IOWrites = TRUE;
            TaskManagerSettings.ColumnSizeArray[20] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("I/O Other")) == 0)
        {
            TaskManagerSettings.Column_IOOther = TRUE;
            TaskManagerSettings.ColumnSizeArray[21] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("I/O Read Bytes")) == 0)
        {
            TaskManagerSettings.Column_IOReadBytes = TRUE;
            TaskManagerSettings.ColumnSizeArray[22] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("I/O Write Bytes")) == 0)
        {
            TaskManagerSettings.Column_IOWriteBytes = TRUE;
            TaskManagerSettings.ColumnSizeArray[23] = hditem.cxy;
        }
        if (_tcsicmp(text, _T("I/O Other Bytes")) == 0)
        {
            TaskManagerSettings.Column_IOOtherBytes = TRUE;
            TaskManagerSettings.ColumnSizeArray[24] = hditem.cxy;
        }
    }
}

void ProcessPage_OnViewSelectColumns(void)
{
    int        i;

    if (DialogBox(hInst, MAKEINTRESOURCE(IDD_COLUMNS_DIALOG), hMainWnd, (DLGPROC)ColumnsDialogWndProc) == IDOK)
    {
        for (i=Header_GetItemCount(hProcessPageHeaderCtrl)-1; i>=0; i--)
        {
            ListView_DeleteColumn(hProcessPageListCtrl, i);
        }

        for (i=0; i<25; i++)
            TaskManagerSettings.ColumnOrderArray[i] = i;

        TaskManagerSettings.ColumnSizeArray[0] = 105;
        TaskManagerSettings.ColumnSizeArray[1] = 50;
        TaskManagerSettings.ColumnSizeArray[2] = 107;
        TaskManagerSettings.ColumnSizeArray[3] = 70;
        TaskManagerSettings.ColumnSizeArray[4] = 35;
        TaskManagerSettings.ColumnSizeArray[5] = 70;
        TaskManagerSettings.ColumnSizeArray[6] = 70;
        TaskManagerSettings.ColumnSizeArray[7] = 100;
        TaskManagerSettings.ColumnSizeArray[8] = 70;
        TaskManagerSettings.ColumnSizeArray[9] = 70;
        TaskManagerSettings.ColumnSizeArray[10] = 70;
        TaskManagerSettings.ColumnSizeArray[11] = 70;
        TaskManagerSettings.ColumnSizeArray[12] = 70;
        TaskManagerSettings.ColumnSizeArray[13] = 70;
        TaskManagerSettings.ColumnSizeArray[14] = 60;
        TaskManagerSettings.ColumnSizeArray[15] = 60;
        TaskManagerSettings.ColumnSizeArray[16] = 60;
        TaskManagerSettings.ColumnSizeArray[17] = 60;
        TaskManagerSettings.ColumnSizeArray[18] = 60;
        TaskManagerSettings.ColumnSizeArray[19] = 70;
        TaskManagerSettings.ColumnSizeArray[20] = 70;
        TaskManagerSettings.ColumnSizeArray[21] = 70;
        TaskManagerSettings.ColumnSizeArray[22] = 70;
        TaskManagerSettings.ColumnSizeArray[23] = 70;
        TaskManagerSettings.ColumnSizeArray[24] = 70;

        AddColumns();
    }
}

LRESULT CALLBACK ColumnsDialogWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{

    switch (message)
    {
    case WM_INITDIALOG:

        if (TaskManagerSettings.Column_ImageName)
            SendMessage(GetDlgItem(hDlg, IDC_IMAGENAME), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_PID)
            SendMessage(GetDlgItem(hDlg, IDC_PID), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_UserName)
            SendMessage(GetDlgItem(hDlg, IDC_USERNAME), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_SessionID)
            SendMessage(GetDlgItem(hDlg, IDC_SESSIONID), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_CPUUsage)
            SendMessage(GetDlgItem(hDlg, IDC_CPUUSAGE), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_CPUTime)
            SendMessage(GetDlgItem(hDlg, IDC_CPUTIME), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_MemoryUsage)
            SendMessage(GetDlgItem(hDlg, IDC_MEMORYUSAGE), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_PeakMemoryUsage)
            SendMessage(GetDlgItem(hDlg, IDC_PEAKMEMORYUSAGE), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_MemoryUsageDelta)
            SendMessage(GetDlgItem(hDlg, IDC_MEMORYUSAGEDELTA), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_PageFaults)
            SendMessage(GetDlgItem(hDlg, IDC_PAGEFAULTS), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_PageFaultsDelta)
            SendMessage(GetDlgItem(hDlg, IDC_PAGEFAULTSDELTA), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_VirtualMemorySize)
            SendMessage(GetDlgItem(hDlg, IDC_VIRTUALMEMORYSIZE), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_PagedPool)
            SendMessage(GetDlgItem(hDlg, IDC_PAGEDPOOL), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_NonPagedPool)
            SendMessage(GetDlgItem(hDlg, IDC_NONPAGEDPOOL), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_BasePriority)
            SendMessage(GetDlgItem(hDlg, IDC_BASEPRIORITY), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_HandleCount)
            SendMessage(GetDlgItem(hDlg, IDC_HANDLECOUNT), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_ThreadCount)
            SendMessage(GetDlgItem(hDlg, IDC_THREADCOUNT), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_USERObjects)
            SendMessage(GetDlgItem(hDlg, IDC_USEROBJECTS), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_GDIObjects)
            SendMessage(GetDlgItem(hDlg, IDC_GDIOBJECTS), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_IOReads)
            SendMessage(GetDlgItem(hDlg, IDC_IOREADS), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_IOWrites)
            SendMessage(GetDlgItem(hDlg, IDC_IOWRITES), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_IOOther)
            SendMessage(GetDlgItem(hDlg, IDC_IOOTHER), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_IOReadBytes)
            SendMessage(GetDlgItem(hDlg, IDC_IOREADBYTES), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_IOWriteBytes)
            SendMessage(GetDlgItem(hDlg, IDC_IOWRITEBYTES), BM_SETCHECK, BST_CHECKED, 0);
        if (TaskManagerSettings.Column_IOOtherBytes)
            SendMessage(GetDlgItem(hDlg, IDC_IOOTHERBYTES), BM_SETCHECK, BST_CHECKED, 0);

        return TRUE;

    case WM_COMMAND:

        if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }

        if (LOWORD(wParam) == IDOK)
        {
            TaskManagerSettings.Column_ImageName = SendMessage(GetDlgItem(hDlg, IDC_IMAGENAME), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_PID = SendMessage(GetDlgItem(hDlg, IDC_PID), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_UserName = SendMessage(GetDlgItem(hDlg, IDC_USERNAME), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_SessionID = SendMessage(GetDlgItem(hDlg, IDC_SESSIONID), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_CPUUsage = SendMessage(GetDlgItem(hDlg, IDC_CPUUSAGE), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_CPUTime = SendMessage(GetDlgItem(hDlg, IDC_CPUTIME), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_MemoryUsage = SendMessage(GetDlgItem(hDlg, IDC_MEMORYUSAGE), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_PeakMemoryUsage = SendMessage(GetDlgItem(hDlg, IDC_PEAKMEMORYUSAGE), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_MemoryUsageDelta = SendMessage(GetDlgItem(hDlg, IDC_MEMORYUSAGEDELTA), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_PageFaults = SendMessage(GetDlgItem(hDlg, IDC_PAGEFAULTS), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_PageFaultsDelta = SendMessage(GetDlgItem(hDlg, IDC_PAGEFAULTSDELTA), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_VirtualMemorySize = SendMessage(GetDlgItem(hDlg, IDC_VIRTUALMEMORYSIZE), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_PagedPool = SendMessage(GetDlgItem(hDlg, IDC_PAGEDPOOL), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_NonPagedPool = SendMessage(GetDlgItem(hDlg, IDC_NONPAGEDPOOL), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_BasePriority = SendMessage(GetDlgItem(hDlg, IDC_BASEPRIORITY), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_HandleCount = SendMessage(GetDlgItem(hDlg, IDC_HANDLECOUNT), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_ThreadCount = SendMessage(GetDlgItem(hDlg, IDC_THREADCOUNT), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_USERObjects = SendMessage(GetDlgItem(hDlg, IDC_USEROBJECTS), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_GDIObjects = SendMessage(GetDlgItem(hDlg, IDC_GDIOBJECTS), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_IOReads = SendMessage(GetDlgItem(hDlg, IDC_IOREADS), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_IOWrites = SendMessage(GetDlgItem(hDlg, IDC_IOWRITES), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_IOOther = SendMessage(GetDlgItem(hDlg, IDC_IOOTHER), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_IOReadBytes = SendMessage(GetDlgItem(hDlg, IDC_IOREADBYTES), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_IOWriteBytes = SendMessage(GetDlgItem(hDlg, IDC_IOWRITEBYTES), BM_GETCHECK, 0, 0);
            TaskManagerSettings.Column_IOOtherBytes = SendMessage(GetDlgItem(hDlg, IDC_IOOTHERBYTES), BM_GETCHECK, 0, 0);

            EndDialog(hDlg, LOWORD(wParam));
            return TRUE;
        }

        break;
    }

    return 0;
}

void UpdateColumnDataHints(void)
{
    HDITEM            hditem;
    TCHAR            text[260];
    ULONG            Index;

    for (Index=0; Index<(ULONG)SendMessage(hProcessPageHeaderCtrl, HDM_GETITEMCOUNT, 0, 0); Index++)
    {
        memset(&hditem, 0, sizeof(HDITEM));

        hditem.mask = HDI_TEXT;
        hditem.pszText = text;
        hditem.cchTextMax = 260;

        SendMessage(hProcessPageHeaderCtrl, HDM_GETITEM, Index, (LPARAM) &hditem);

        if (_tcsicmp(text, _T("Image Name")) == 0)
            ColumnDataHints[Index] = COLUMN_IMAGENAME;
        if (_tcsicmp(text, _T("PID")) == 0)
            ColumnDataHints[Index] = COLUMN_PID;
        if (_tcsicmp(text, _T("Username")) == 0)
            ColumnDataHints[Index] = COLUMN_USERNAME;
        if (_tcsicmp(text, _T("Session ID")) == 0)
            ColumnDataHints[Index] = COLUMN_SESSIONID;
        if (_tcsicmp(text, _T("CPU")) == 0)
            ColumnDataHints[Index] = COLUMN_CPUUSAGE;
        if (_tcsicmp(text, _T("CPU Time")) == 0)
            ColumnDataHints[Index] = COLUMN_CPUTIME;
        if (_tcsicmp(text, _T("Mem Usage")) == 0)
            ColumnDataHints[Index] = COLUMN_MEMORYUSAGE;
        if (_tcsicmp(text, _T("Peak Mem Usage")) == 0)
            ColumnDataHints[Index] = COLUMN_PEAKMEMORYUSAGE;
        if (_tcsicmp(text, _T("Mem Delta")) == 0)
            ColumnDataHints[Index] = COLUMN_MEMORYUSAGEDELTA;
        if (_tcsicmp(text, _T("Page Faults")) == 0)
            ColumnDataHints[Index] = COLUMN_PAGEFAULTS;
        if (_tcsicmp(text, _T("PF Delta")) == 0)
            ColumnDataHints[Index] = COLUMN_PAGEFAULTSDELTA;
        if (_tcsicmp(text, _T("VM Size")) == 0)
            ColumnDataHints[Index] = COLUMN_VIRTUALMEMORYSIZE;
        if (_tcsicmp(text, _T("Paged Pool")) == 0)
            ColumnDataHints[Index] = COLUMN_PAGEDPOOL;
        if (_tcsicmp(text, _T("NP Pool")) == 0)
            ColumnDataHints[Index] = COLUMN_NONPAGEDPOOL;
        if (_tcsicmp(text, _T("Base Pri")) == 0)
            ColumnDataHints[Index] = COLUMN_BASEPRIORITY;
        if (_tcsicmp(text, _T("Handles")) == 0)
            ColumnDataHints[Index] = COLUMN_HANDLECOUNT;
        if (_tcsicmp(text, _T("Threads")) == 0)
            ColumnDataHints[Index] = COLUMN_THREADCOUNT;
        if (_tcsicmp(text, _T("USER Objects")) == 0)
            ColumnDataHints[Index] = COLUMN_USEROBJECTS;
        if (_tcsicmp(text, _T("GDI Objects")) == 0)
            ColumnDataHints[Index] = COLUMN_GDIOBJECTS;
        if (_tcsicmp(text, _T("I/O Reads")) == 0)
            ColumnDataHints[Index] = COLUMN_IOREADS;
        if (_tcsicmp(text, _T("I/O Writes")) == 0)
            ColumnDataHints[Index] = COLUMN_IOWRITES;
        if (_tcsicmp(text, _T("I/O Other")) == 0)
            ColumnDataHints[Index] = COLUMN_IOOTHER;
        if (_tcsicmp(text, _T("I/O Read Bytes")) == 0)
            ColumnDataHints[Index] = COLUMN_IOREADBYTES;
        if (_tcsicmp(text, _T("I/O Write Bytes")) == 0)
            ColumnDataHints[Index] = COLUMN_IOWRITEBYTES;
        if (_tcsicmp(text, _T("I/O Other Bytes")) == 0)
            ColumnDataHints[Index] = COLUMN_IOOTHERBYTES;
    }
}
