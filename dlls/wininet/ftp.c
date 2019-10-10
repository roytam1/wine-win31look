/*
 * WININET - Ftp implementation
 *
 * Copyright 1999 Corel Corporation
 * Copyright 2004 Mike McCormack for CodeWeavers
 *
 * Ulrich Czekalla
 * Noureddine Jemmali
 *
 * Copyright 2000 Andreas Mohr
 * Copyright 2002 Jaco Greeff
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
#include "wine/port.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <time.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "wininet.h"
#include "winnls.h"
#include "winerror.h"
#include "winreg.h"
#include "winternl.h"

#include "wine/debug.h"
#include "internet.h"

WINE_DEFAULT_DEBUG_CHANNEL(wininet);

#define NOACCOUNT 		"noaccount"
#define DATA_PACKET_SIZE 	0x2000
#define szCRLF 			"\r\n"
#define MAX_BACKLOG 		5

typedef enum {
  /* FTP commands with arguments. */
  FTP_CMD_ACCT,
  FTP_CMD_CWD,
  FTP_CMD_DELE,
  FTP_CMD_MKD,
  FTP_CMD_PASS,
  FTP_CMD_PORT,
  FTP_CMD_RETR,
  FTP_CMD_RMD,
  FTP_CMD_RNFR,
  FTP_CMD_RNTO,
  FTP_CMD_STOR,
  FTP_CMD_TYPE,
  FTP_CMD_USER,
  FTP_CMD_SIZE,

  /* FTP commands without arguments. */
  FTP_CMD_ABOR,
  FTP_CMD_LIST,
  FTP_CMD_NLST,
  FTP_CMD_PASV,
  FTP_CMD_PWD,
  FTP_CMD_QUIT,
} FTP_COMMAND;

static const CHAR *szFtpCommands[] = {
  "ACCT",
  "CWD",
  "DELE",
  "MKD",
  "PASS",
  "PORT",
  "RETR",
  "RMD",
  "RNFR",
  "RNTO",
  "STOR",
  "TYPE",
  "USER",
  "SIZE",
  "ABOR",
  "LIST",
  "NLST",
  "PASV",
  "PWD",
  "QUIT",
};

static const CHAR szMonths[] = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";

BOOL FTP_SendCommand(INT nSocket, FTP_COMMAND ftpCmd, LPCSTR lpszParam,
	INTERNET_STATUS_CALLBACK lpfnStatusCB, HINTERNET hHandle, DWORD dwContext);
BOOL FTP_SendStore(LPWININETFTPSESSIONA lpwfs, LPCSTR lpszRemoteFile, DWORD dwType);
BOOL FTP_GetDataSocket(LPWININETFTPSESSIONA lpwfs, LPINT nDataSocket);
BOOL FTP_SendData(LPWININETFTPSESSIONA lpwfs, INT nDataSocket, HANDLE hFile);
INT FTP_ReceiveResponse(INT nSocket, LPSTR lpszResponse, DWORD dwResponse,
	INTERNET_STATUS_CALLBACK lpfnStatusCB, HINTERNET hHandle, DWORD dwContext);
DWORD FTP_SendRetrieve(LPWININETFTPSESSIONA lpwfs, LPCSTR lpszRemoteFile, DWORD dwType);
BOOL FTP_RetrieveFileData(LPWININETFTPSESSIONA lpwfs, INT nDataSocket, DWORD nBytes, HANDLE hFile);
BOOL FTP_InitListenSocket(LPWININETFTPSESSIONA lpwfs);
BOOL FTP_ConnectToHost(LPWININETFTPSESSIONA lpwfs);
BOOL FTP_SendPassword(LPWININETFTPSESSIONA lpwfs);
BOOL FTP_SendAccount(LPWININETFTPSESSIONA lpwfs);
BOOL FTP_SendType(LPWININETFTPSESSIONA lpwfs, DWORD dwType);
BOOL FTP_GetFileSize(LPWININETFTPSESSIONA lpwfs, LPCSTR lpszRemoteFile, DWORD *dwSize);
BOOL FTP_SendPort(LPWININETFTPSESSIONA lpwfs);
BOOL FTP_DoPassive(LPWININETFTPSESSIONA lpwfs);
BOOL FTP_SendPortOrPasv(LPWININETFTPSESSIONA lpwfs);
BOOL FTP_ParsePermission(LPCSTR lpszPermission, LPFILEPROPERTIESA lpfp);
BOOL FTP_ParseDirectory(LPWININETFTPSESSIONA lpwfs, INT nSocket, LPFILEPROPERTIESA *lpafp, LPDWORD dwfp);
HINTERNET FTP_ReceiveFileList(LPWININETFTPSESSIONA lpwfs, INT nSocket,
	LPWIN32_FIND_DATAA lpFindFileData, DWORD dwContext);
DWORD FTP_SetResponseError(DWORD dwResponse);

inline static LPSTR FTP_strdup( LPCSTR str )
{
    LPSTR ret = HeapAlloc( GetProcessHeap(), 0, strlen(str) + 1 );
    if (ret) strcpy( ret, str );
    return ret;
}

inline static LPSTR FTP_strdup_WtoA( LPCWSTR str )
{
    int len = WideCharToMultiByte( CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
    LPSTR ret = HeapAlloc( GetProcessHeap(), 0, len );
    if (ret)
        WideCharToMultiByte( CP_ACP, 0, str, -1, ret, len, NULL, NULL);
    return ret;
}

/***********************************************************************
 *           FtpPutFileA (WININET.@)
 *
 * Uploads a file to the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpPutFileA(HINTERNET hConnect, LPCSTR lpszLocalFile,
    LPCSTR lpszNewRemoteFile, DWORD dwFlags, DWORD dwContext)
{
    LPWININETFTPSESSIONA lpwfs;
    LPWININETAPPINFOW hIC = NULL;

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hConnect );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPPUTFILEA *req = &workRequest.u.FtpPutFileA;

	workRequest.asyncall = FTPPUTFILEA;
	workRequest.handle = hConnect;
	req->lpszLocalFile = FTP_strdup(lpszLocalFile);
	req->lpszNewRemoteFile = FTP_strdup(lpszNewRemoteFile);
	req->dwFlags = dwFlags;
	req->dwContext = dwContext;

	return INTERNET_AsyncCall(&workRequest);
    }
    else
    {
	return FTP_FtpPutFileA(hConnect, lpszLocalFile,
		lpszNewRemoteFile, dwFlags, dwContext);
    }
}

/***********************************************************************
 *           FTP_FtpPutFileA (Internal)
 *
 * Uploads a file to the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FTP_FtpPutFileA(HINTERNET hConnect, LPCSTR lpszLocalFile,
    LPCSTR lpszNewRemoteFile, DWORD dwFlags, DWORD dwContext)
{
    HANDLE hFile = NULL;
    BOOL bSuccess = FALSE;
    LPWININETAPPINFOW hIC = NULL;
    LPWININETFTPSESSIONA lpwfs;
    INT nResCode;

    TRACE(" lpszLocalFile(%s) lpszNewRemoteFile(%s)\n", lpszLocalFile, lpszNewRemoteFile);

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hConnect );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    /* Clear any error information */
    INTERNET_SetLastError(0);

    /* Open file to be uploaded */
    if (INVALID_HANDLE_VALUE ==
        (hFile = CreateFileA(lpszLocalFile, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0)))
    {
        INTERNET_SetLastError(ERROR_FILE_NOT_FOUND);
        goto lend;
    }

    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->lpfnStatusCB)
        hIC->lpfnStatusCB(hConnect, lpwfs->hdr.dwContext, INTERNET_STATUS_SENDING_REQUEST, NULL, 0);

    if (FTP_SendStore(lpwfs, lpszNewRemoteFile, dwFlags))
    {
        INT nDataSocket;

        /* Get data socket to server */
        if (FTP_GetDataSocket(lpwfs, &nDataSocket))
        {
            FTP_SendData(lpwfs, nDataSocket, hFile);
            close(nDataSocket);
	    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
					   MAX_REPLY_LEN, 0, 0, 0);
	    if (nResCode)
	    {
	        if (nResCode == 226)
		    bSuccess = TRUE;
		else
		    FTP_SetResponseError(nResCode);
	    }
        }
    }

lend:
    if (lpwfs->lstnSocket != -1)
        close(lpwfs->lstnSocket);

    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC  && hIC->lpfnStatusCB)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : INTERNET_GetLastError();
        hIC->lpfnStatusCB(hConnect, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    if (hFile)
        CloseHandle(hFile);

    return bSuccess;
}


/***********************************************************************
 *           FtpSetCurrentDirectoryA (WININET.@)
 *
 * Change the working directory on the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpSetCurrentDirectoryA(HINTERNET hConnect, LPCSTR lpszDirectory)
{
    LPWININETFTPSESSIONA lpwfs;
    LPWININETAPPINFOW hIC = NULL;

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hConnect );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    TRACE("lpszDirectory(%s)\n", lpszDirectory);

    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPSETCURRENTDIRECTORYA *req;

        workRequest.asyncall = FTPSETCURRENTDIRECTORYA;
	workRequest.handle = hConnect;
        req = &workRequest.u.FtpSetCurrentDirectoryA;
        req->lpszDirectory = FTP_strdup(lpszDirectory);

	return INTERNET_AsyncCall(&workRequest);
    }
    else
    {
	return FTP_FtpSetCurrentDirectoryA(hConnect, lpszDirectory);
    }
}


/***********************************************************************
 *           FtpSetCurrentDirectoryW (WININET.@)
 *
 * Change the working directory on the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpSetCurrentDirectoryW(HINTERNET hConnect, LPCWSTR lpszDirectory)
{
    CHAR *szDir;
    INT len;
    BOOL rc;

    len = WideCharToMultiByte(CP_ACP, 0, lpszDirectory, -1, NULL, 0, NULL, NULL);
    szDir = HeapAlloc(GetProcessHeap(), 0, len);
    if(!szDir)
        return FALSE;
    WideCharToMultiByte(CP_ACP, 0, lpszDirectory, -1, szDir, len, NULL, NULL);
    rc = FtpSetCurrentDirectoryA(hConnect, szDir);
    HeapFree(GetProcessHeap(), 0, szDir);

    return rc;
}


/***********************************************************************
 *           FTP_FtpSetCurrentDirectoryA (Internal)
 *
 * Change the working directory on the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FTP_FtpSetCurrentDirectoryA(HINTERNET hConnect, LPCSTR lpszDirectory)
{
    INT nResCode;
    LPWININETFTPSESSIONA lpwfs;
    LPWININETAPPINFOW hIC = NULL;
    DWORD bSuccess = FALSE;

    TRACE("lpszDirectory(%s)\n", lpszDirectory);

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hConnect );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    /* Clear any error information */
    INTERNET_SetLastError(0);

    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_CWD, lpszDirectory,
        hIC->lpfnStatusCB, hConnect, lpwfs->hdr.dwContext))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
        MAX_REPLY_LEN, hIC->lpfnStatusCB, hConnect, lpwfs->hdr.dwContext);

    if (nResCode)
    {
        if (nResCode == 250)
            bSuccess = TRUE;
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC && hIC->lpfnStatusCB)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : ERROR_INTERNET_EXTENDED_ERROR;
        hIC->lpfnStatusCB(hConnect, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }
    return bSuccess;
}


/***********************************************************************
 *           FtpCreateDirectoryA (WININET.@)
 *
 * Create new directory on the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpCreateDirectoryA(HINTERNET hConnect, LPCSTR lpszDirectory)
{
    LPWININETFTPSESSIONA lpwfs;
    LPWININETAPPINFOW hIC = NULL;

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hConnect );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPCREATEDIRECTORYA *req;

        workRequest.asyncall = FTPCREATEDIRECTORYA;
	workRequest.handle = hConnect;
        req = &workRequest.u.FtpCreateDirectoryA;
	req->lpszDirectory = FTP_strdup(lpszDirectory);

	return INTERNET_AsyncCall(&workRequest);
    }
    else
    {
	return FTP_FtpCreateDirectoryA(hConnect, lpszDirectory);
    }
}


/***********************************************************************
 *           FtpCreateDirectoryW (WININET.@)
 *
 * Create new directory on the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpCreateDirectoryW(HINTERNET hConnect, LPCWSTR lpszDirectory)
{
    CHAR *szDir;
    INT len;
    BOOL rc;

    len = WideCharToMultiByte(CP_ACP, 0, lpszDirectory, -1, NULL, 0, NULL, NULL);
    szDir = HeapAlloc(GetProcessHeap(), 0, len);
    if (!szDir)
        return FALSE;
    WideCharToMultiByte(CP_ACP, 0, lpszDirectory, -1, szDir, len, NULL, NULL);
    rc = FtpCreateDirectoryA(hConnect, szDir);
    HeapFree(GetProcessHeap(), 0, szDir);

    return rc;
}


/***********************************************************************
 *           FTP_FtpCreateDirectoryA (Internal)
 *
 * Create new directory on the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FTP_FtpCreateDirectoryA(HINTERNET hConnect, LPCSTR lpszDirectory)
{
    INT nResCode;
    BOOL bSuccess = FALSE;
    LPWININETAPPINFOW hIC = NULL;
    LPWININETFTPSESSIONA lpwfs;

    TRACE("\n");

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hConnect );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    /* Clear any error information */
    INTERNET_SetLastError(0);

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_MKD, lpszDirectory, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
        MAX_REPLY_LEN, 0, 0, 0);
    if (nResCode)
    {
        if (nResCode == 257)
            bSuccess = TRUE;
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC && hIC->lpfnStatusCB)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : INTERNET_GetLastError();
        hIC->lpfnStatusCB(hConnect, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    return bSuccess;
}


/***********************************************************************
 *           FtpFindFirstFileA (WININET.@)
 *
 * Search the specified directory
 *
 * RETURNS
 *    HINTERNET on success
 *    NULL on failure
 *
 */
HINTERNET WINAPI FtpFindFirstFileA(HINTERNET hConnect,
    LPCSTR lpszSearchFile, LPWIN32_FIND_DATAA lpFindFileData, DWORD dwFlags, DWORD dwContext)
{
    LPWININETFTPSESSIONA lpwfs;
    LPWININETAPPINFOW hIC = NULL;

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hConnect );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPFINDFIRSTFILEA *req;

        workRequest.asyncall = FTPFINDFIRSTFILEA;
	workRequest.handle = hConnect;
        req = &workRequest.u.FtpFindFirstFileA;
	req->lpszSearchFile = FTP_strdup(lpszSearchFile);
	req->lpFindFileData = lpFindFileData;
	req->dwFlags = dwFlags;
	req->dwContext= dwContext;

	INTERNET_AsyncCall(&workRequest);
	return NULL;
    }
    else
    {
	return FTP_FtpFindFirstFileA(hConnect, lpszSearchFile, lpFindFileData,
		dwFlags, dwContext);
    }
}


/***********************************************************************
 *           FtpFindFirstFileA (WININET.@)
 *
 * Search the specified directory
 *
 * RETURNS
 *    HINTERNET on success
 *    NULL on failure
 *
 */
HINTERNET WINAPI FtpFindFirstFileW(HINTERNET hConnect,
    LPCWSTR lpszSearchFile, LPWIN32_FIND_DATAW lpFindFileData, DWORD dwFlags, DWORD dwContext)
{
    FIXME("STUB\n");
    return NULL;
}


/***********************************************************************
 *           FTP_FtpFindFirstFileA (Internal)
 *
 * Search the specified directory
 *
 * RETURNS
 *    HINTERNET on success
 *    NULL on failure
 *
 */
HINTERNET WINAPI FTP_FtpFindFirstFileA(HINTERNET hConnect,
    LPCSTR lpszSearchFile, LPWIN32_FIND_DATAA lpFindFileData, DWORD dwFlags, DWORD dwContext)
{
    INT nResCode;
    LPWININETAPPINFOW hIC = NULL;
    LPWININETFTPSESSIONA lpwfs;
    HINTERNET hFindNext = NULL;

    TRACE("\n");

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hConnect );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    /* Clear any error information */
    INTERNET_SetLastError(0);

    if (!FTP_InitListenSocket(lpwfs))
        goto lend;

    if (!FTP_SendType(lpwfs, INTERNET_FLAG_TRANSFER_ASCII))
        goto lend;

    if (!FTP_SendPortOrPasv(lpwfs))
        goto lend;

    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_LIST, lpszSearchFile,
        hIC->lpfnStatusCB, hConnect, lpwfs->hdr.dwContext))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
        MAX_REPLY_LEN, hIC->lpfnStatusCB, hConnect, lpwfs->hdr.dwContext);
    if (nResCode)
    {
        if (nResCode == 125 || nResCode == 150)
        {
            INT nDataSocket;

            /* Get data socket to server */
            if (FTP_GetDataSocket(lpwfs, &nDataSocket))
            {
                hFindNext = FTP_ReceiveFileList(lpwfs, nDataSocket, lpFindFileData, dwContext);

                nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
                    MAX_REPLY_LEN, hIC->lpfnStatusCB, hConnect, lpwfs->hdr.dwContext);
                if (nResCode != 226 && nResCode != 250)
                    INTERNET_SetLastError(ERROR_NO_MORE_FILES);

                close(nDataSocket);
            }
        }
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    if (lpwfs->lstnSocket != -1)
        close(lpwfs->lstnSocket);

    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC && hIC->lpfnStatusCB)
    {
        INTERNET_ASYNC_RESULT iar;

        if (hFindNext)
	{
            iar.dwResult = (DWORD)hFindNext;
            iar.dwError = ERROR_SUCCESS;
            hIC->lpfnStatusCB(hConnect, lpwfs->hdr.dwContext, INTERNET_STATUS_HANDLE_CREATED,
                &iar, sizeof(INTERNET_ASYNC_RESULT));
	}

        iar.dwResult = (DWORD)hFindNext;
        iar.dwError = hFindNext ? ERROR_SUCCESS : INTERNET_GetLastError();
        hIC->lpfnStatusCB(hConnect, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    return hFindNext;
}


/***********************************************************************
 *           FtpGetCurrentDirectoryA (WININET.@)
 *
 * Retrieves the current directory
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpGetCurrentDirectoryA(HINTERNET hFtpSession, LPSTR lpszCurrentDirectory,
    LPDWORD lpdwCurrentDirectory)
{
    LPWININETFTPSESSIONA lpwfs;
    LPWININETAPPINFOW hIC = NULL;

    TRACE("len(%ld)\n", *lpdwCurrentDirectory);

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hFtpSession );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPGETCURRENTDIRECTORYA *req;

        workRequest.asyncall =  FTPGETCURRENTDIRECTORYA;
	workRequest.handle = hFtpSession;
        req = &workRequest.u.FtpGetCurrentDirectoryA;
	req->lpszDirectory = lpszCurrentDirectory;
	req->lpdwDirectory = lpdwCurrentDirectory;

	return INTERNET_AsyncCall(&workRequest);
    }
    else
    {
	return FTP_FtpGetCurrentDirectoryA(hFtpSession, lpszCurrentDirectory,
		lpdwCurrentDirectory);
    }
}


/***********************************************************************
 *           FtpGetCurrentDirectoryW (WININET.@)
 *
 * Retrieves the current directory
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpGetCurrentDirectoryW(HINTERNET hFtpSession, LPWSTR lpszCurrentDirectory,
    LPDWORD lpdwCurrentDirectory)
{
    FIXME("STUB\n");
    return FALSE;
}


/***********************************************************************
 *           FTP_FtpGetCurrentDirectoryA (Internal)
 *
 * Retrieves the current directory
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FTP_FtpGetCurrentDirectoryA(HINTERNET hFtpSession, LPSTR lpszCurrentDirectory,
	LPDWORD lpdwCurrentDirectory)
{
    INT nResCode;
    LPWININETFTPSESSIONA lpwfs;
    LPWININETAPPINFOW hIC = NULL;
    DWORD bSuccess = FALSE;

    TRACE("len(%ld)\n", *lpdwCurrentDirectory);

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hFtpSession );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    /* Clear any error information */
    INTERNET_SetLastError(0);

    ZeroMemory(lpszCurrentDirectory, *lpdwCurrentDirectory);

    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_PWD, NULL,
        hIC->lpfnStatusCB, hFtpSession, lpwfs->hdr.dwContext))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
        MAX_REPLY_LEN, hIC->lpfnStatusCB, hFtpSession, lpwfs->hdr.dwContext);
    if (nResCode)
    {
        if (nResCode == 257) /* Extract directory name */
        {
            INT firstpos, lastpos, len;
	    LPSTR lpszResponseBuffer = INTERNET_GetResponseBuffer();

            for (firstpos = 0, lastpos = 0; lpszResponseBuffer[lastpos]; lastpos++)
            {
                if ('"' == lpszResponseBuffer[lastpos])
                {
                    if (!firstpos)
                        firstpos = lastpos;
                    else
                        break;
		}
            }

            len = lastpos - firstpos - 1;
            strncpy(lpszCurrentDirectory, &lpszResponseBuffer[firstpos+1],
                len < *lpdwCurrentDirectory ? len : *lpdwCurrentDirectory);
            *lpdwCurrentDirectory = len;
            bSuccess = TRUE;
        }
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC && hIC->lpfnStatusCB)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : ERROR_INTERNET_EXTENDED_ERROR;
        hIC->lpfnStatusCB(hFtpSession, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    return (DWORD) bSuccess;
}

/***********************************************************************
 *           FtpOpenFileA (WININET.@)
 *
 * Open a remote file for writing or reading
 *
 * RETURNS
 *    HINTERNET handle on success
 *    NULL on failure
 *
 */
HINTERNET WINAPI FtpOpenFileA(HINTERNET hFtpSession,
    LPCSTR lpszFileName, DWORD fdwAccess, DWORD dwFlags,
    DWORD dwContext)
{
    LPWININETFTPSESSIONA lpwfs;
    LPWININETAPPINFOW hIC = NULL;

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hFtpSession );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    if (lpwfs->download_in_progress != NULL) {
	INTERNET_SetLastError(ERROR_FTP_TRANSFER_IN_PROGRESS);
	return FALSE;
    }
    
    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPOPENFILEA *req;

        workRequest.asyncall = FTPOPENFILEA;
	workRequest.handle = hFtpSession;
        req = &workRequest.u.FtpOpenFileA;
	req->lpszFilename = FTP_strdup(lpszFileName);
	req->dwAccess = fdwAccess;
	req->dwFlags = dwFlags;
	req->dwContext = dwContext;

	INTERNET_AsyncCall(&workRequest);
	return NULL;
    }
    else
    {
	return FTP_FtpOpenFileA(hFtpSession, lpszFileName, fdwAccess, dwFlags, dwContext);
    }
}


/***********************************************************************
 *           FtpOpenFileW (WININET.@)
 *
 * Open a remote file for writing or reading
 *
 * RETURNS
 *    HINTERNET handle on success
 *    NULL on failure
 *
 */
HINTERNET WINAPI FtpOpenFileW(HINTERNET hFtpSession,
    LPCWSTR lpszFileName, DWORD fdwAccess, DWORD dwFlags,
    DWORD dwContext)
{
    FIXME("STUB\n");
    return NULL;
}


/***********************************************************************
 *           FTP_FtpOpenFileA (Internal)
 *
 * Open a remote file for writing or reading
 *
 * RETURNS
 *    HINTERNET handle on success
 *    NULL on failure
 *
 */
HINTERNET FTP_FtpOpenFileA(HINTERNET hFtpSession,
	LPCSTR lpszFileName, DWORD fdwAccess, DWORD dwFlags,
	DWORD dwContext)
{
    INT nDataSocket;
    BOOL bSuccess = FALSE;
    LPWININETFILE lpwh = NULL;
    LPWININETAPPINFOW hIC = NULL;
    LPWININETFTPSESSIONA lpwfs;
    HINTERNET handle = NULL;

    TRACE("\n");

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hFtpSession );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    /* Clear any error information */
    INTERNET_SetLastError(0);

    if (GENERIC_READ == fdwAccess)
    {
        /* Set up socket to retrieve data */
        bSuccess = FTP_SendRetrieve(lpwfs, lpszFileName, dwFlags);
    }
    else if (GENERIC_WRITE == fdwAccess)
    {
        /* Set up socket to send data */
        bSuccess = FTP_SendStore(lpwfs, lpszFileName, dwFlags);
    }

    /* Get data socket to server */
    if (bSuccess && FTP_GetDataSocket(lpwfs, &nDataSocket))
    {
        lpwh = HeapAlloc(GetProcessHeap(), 0, sizeof(WININETFILE));
        handle = WININET_AllocHandle( &lpwh->hdr );
        lpwh->hdr.htype = WH_HFILE;
        lpwh->hdr.dwFlags = dwFlags;
        lpwh->hdr.dwContext = dwContext;
        lpwh->hdr.lpwhparent = &lpwfs->hdr;
        lpwh->nDataSocket = nDataSocket;
	lpwh->session_deleted = FALSE;
	
	/* Indicate that a download is currently in progress */
	lpwfs->download_in_progress = lpwh;
    }

    if (lpwfs->lstnSocket != -1)
        close(lpwfs->lstnSocket);

    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC && hIC->lpfnStatusCB)
    {
        INTERNET_ASYNC_RESULT iar;

	if (lpwh)
	{
            iar.dwResult = (DWORD)handle;
            iar.dwError = ERROR_SUCCESS;
            hIC->lpfnStatusCB(hFtpSession, lpwfs->hdr.dwContext, INTERNET_STATUS_HANDLE_CREATED,
                &iar, sizeof(INTERNET_ASYNC_RESULT));
	}

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : INTERNET_GetLastError();
        hIC->lpfnStatusCB(hFtpSession, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    return handle;
}


/***********************************************************************
 *           FtpGetFileA (WININET.@)
 *
 * Retrieve file from the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpGetFileA(HINTERNET hInternet, LPCSTR lpszRemoteFile, LPCSTR lpszNewFile,
    BOOL fFailIfExists, DWORD dwLocalFlagsAttribute, DWORD dwInternetFlags,
    DWORD dwContext)
{
    LPWININETFTPSESSIONA lpwfs;
    LPWININETAPPINFOW hIC = NULL;

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hInternet );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    if (lpwfs->download_in_progress != NULL) {
	INTERNET_SetLastError(ERROR_FTP_TRANSFER_IN_PROGRESS);
	return FALSE;
    }
    
    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPGETFILEA *req;

        workRequest.asyncall = FTPGETFILEA;
	workRequest.handle = hInternet;
        req = &workRequest.u.FtpGetFileA;
	req->lpszRemoteFile = FTP_strdup(lpszRemoteFile);
	req->lpszNewFile = FTP_strdup(lpszNewFile);
	req->dwLocalFlagsAttribute = dwLocalFlagsAttribute;
	req->fFailIfExists = fFailIfExists;
	req->dwFlags = dwInternetFlags;
	req->dwContext = dwContext;

	return INTERNET_AsyncCall(&workRequest);
    }
    else
    {
	return FTP_FtpGetFileA(hInternet, lpszRemoteFile, lpszNewFile,
           fFailIfExists, dwLocalFlagsAttribute, dwInternetFlags, dwContext);
    }
}


/***********************************************************************
 *           FtpGetFileW (WININET.@)
 *
 * Retrieve file from the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpGetFileW(HINTERNET hInternet, LPCWSTR lpszRemoteFile, LPCWSTR lpszNewFile,
    BOOL fFailIfExists, DWORD dwLocalFlagsAttribute, DWORD dwInternetFlags,
    DWORD dwContext)
{
    FIXME("STUB\n");
    return FALSE;
}


/***********************************************************************
 *           FTP_FtpGetFileA (Internal)
 *
 * Retrieve file from the FTP server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FTP_FtpGetFileA(HINTERNET hInternet, LPCSTR lpszRemoteFile, LPCSTR lpszNewFile,
	BOOL fFailIfExists, DWORD dwLocalFlagsAttribute, DWORD dwInternetFlags,
	DWORD dwContext)
{
    DWORD nBytes;
    BOOL bSuccess = FALSE;
    HANDLE hFile;
    LPWININETAPPINFOW hIC = NULL;
    LPWININETFTPSESSIONA lpwfs;

    TRACE("lpszRemoteFile(%s) lpszNewFile(%s)\n", lpszRemoteFile, lpszNewFile);

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hInternet );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    /* Clear any error information */
    INTERNET_SetLastError(0);

    /* Ensure we can write to lpszNewfile by opening it */
    hFile = CreateFileA(lpszNewFile, GENERIC_WRITE, 0, 0, fFailIfExists ?
        CREATE_NEW : CREATE_ALWAYS, dwLocalFlagsAttribute, 0);
    if (INVALID_HANDLE_VALUE == hFile)
        goto lend;

    /* Set up socket to retrieve data */
    nBytes = FTP_SendRetrieve(lpwfs, lpszRemoteFile, dwInternetFlags);

    if (nBytes > 0)
    {
        INT nDataSocket;

        /* Get data socket to server */
        if (FTP_GetDataSocket(lpwfs, &nDataSocket))
        {
            INT nResCode;

            /* Receive data */
            FTP_RetrieveFileData(lpwfs, nDataSocket, nBytes, hFile);
            nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
            MAX_REPLY_LEN, 0, 0, 0);
            if (nResCode)
            {
                if (nResCode == 226)
                    bSuccess = TRUE;
		else
                    FTP_SetResponseError(nResCode);
            }
	    close(nDataSocket);
        }
    }

lend:
    if (lpwfs->lstnSocket != -1)
        close(lpwfs->lstnSocket);

    if (hFile)
        CloseHandle(hFile);

    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC && hIC->lpfnStatusCB)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : INTERNET_GetLastError();
        hIC->lpfnStatusCB(hInternet, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    return bSuccess;
}


/***********************************************************************
 *           FtpDeleteFileA  (WININET.@)
 *
 * Delete a file on the ftp server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpDeleteFileA(HINTERNET hFtpSession, LPCSTR lpszFileName)
{
    LPWININETFTPSESSIONA lpwfs;
    LPWININETAPPINFOW hIC = NULL;

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hFtpSession );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPDELETEFILEA *req;

        workRequest.asyncall = FTPDELETEFILEA;
	workRequest.handle = hFtpSession;
        req = &workRequest.u.FtpDeleteFileA;
	req->lpszFilename = FTP_strdup(lpszFileName);

	return INTERNET_AsyncCall(&workRequest);
    }
    else
    {
	return FTP_FtpDeleteFileA(hFtpSession, lpszFileName);
    }
}


/***********************************************************************
 *           FTP_FtpDeleteFileA  (Internal)
 *
 * Delete a file on the ftp server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL FTP_FtpDeleteFileA(HINTERNET hFtpSession, LPCSTR lpszFileName)
{
    INT nResCode;
    BOOL bSuccess = FALSE;
    LPWININETAPPINFOW hIC = NULL;
    LPWININETFTPSESSIONA lpwfs;

    TRACE("0x%08lx\n", (ULONG) hFtpSession);

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hFtpSession );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    /* Clear any error information */
    INTERNET_SetLastError(0);

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_DELE, lpszFileName, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
        MAX_REPLY_LEN, 0, 0, 0);
    if (nResCode)
    {
        if (nResCode == 250)
            bSuccess = TRUE;
        else
            FTP_SetResponseError(nResCode);
    }
lend:
    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC && hIC->lpfnStatusCB)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : INTERNET_GetLastError();
        hIC->lpfnStatusCB(hFtpSession, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    return bSuccess;
}


/***********************************************************************
 *           FtpRemoveDirectoryA  (WININET.@)
 *
 * Remove a directory on the ftp server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpRemoveDirectoryA(HINTERNET hFtpSession, LPCSTR lpszDirectory)
{
    LPWININETFTPSESSIONA lpwfs;
    LPWININETAPPINFOW hIC = NULL;

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hFtpSession );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPREMOVEDIRECTORYA *req;

        workRequest.asyncall = FTPREMOVEDIRECTORYA;
	workRequest.handle = hFtpSession;
        req = &workRequest.u.FtpRemoveDirectoryA;
	req->lpszDirectory = FTP_strdup(lpszDirectory);

	return INTERNET_AsyncCall(&workRequest);
    }
    else
    {
	return FTP_FtpRemoveDirectoryA(hFtpSession, lpszDirectory);
    }
}


/***********************************************************************
 *           FTP_FtpRemoveDirectoryA  (Internal)
 *
 * Remove a directory on the ftp server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL FTP_FtpRemoveDirectoryA(HINTERNET hFtpSession, LPCSTR lpszDirectory)
{
    INT nResCode;
    BOOL bSuccess = FALSE;
    LPWININETAPPINFOW hIC = NULL;
    LPWININETFTPSESSIONA lpwfs;

    TRACE("\n");

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hFtpSession );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    /* Clear any error information */
    INTERNET_SetLastError(0);

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_RMD, lpszDirectory, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
        MAX_REPLY_LEN, 0, 0, 0);
    if (nResCode)
    {
        if (nResCode == 250)
            bSuccess = TRUE;
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC && hIC->lpfnStatusCB)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : INTERNET_GetLastError();
        hIC->lpfnStatusCB(hFtpSession, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    return bSuccess;
}


/***********************************************************************
 *           FtpRenameFileA  (WININET.@)
 *
 * Rename a file on the ftp server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL WINAPI FtpRenameFileA(HINTERNET hFtpSession, LPCSTR lpszSrc, LPCSTR lpszDest)
{
    LPWININETFTPSESSIONA lpwfs;
    LPWININETAPPINFOW hIC = NULL;

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hFtpSession );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC)
    {
        WORKREQUEST workRequest;
        struct WORKREQ_FTPRENAMEFILEA *req;

        workRequest.asyncall = FTPRENAMEFILEA;
	workRequest.handle = hFtpSession;
        req = &workRequest.u.FtpRenameFileA;
	req->lpszSrcFile = FTP_strdup(lpszSrc);
	req->lpszDestFile = FTP_strdup(lpszDest);

	return INTERNET_AsyncCall(&workRequest);
    }
    else
    {
	return FTP_FtpRenameFileA(hFtpSession, lpszSrc, lpszDest);
    }
}

/***********************************************************************
 *           FTP_FtpRenameFileA  (Internal)
 *
 * Rename a file on the ftp server
 *
 * RETURNS
 *    TRUE on success
 *    FALSE on failure
 *
 */
BOOL FTP_FtpRenameFileA(HINTERNET hFtpSession, LPCSTR lpszSrc, LPCSTR lpszDest)
{
    INT nResCode;
    BOOL bSuccess = FALSE;
    LPWININETAPPINFOW hIC = NULL;
    LPWININETFTPSESSIONA lpwfs;

    TRACE("\n");

    lpwfs = (LPWININETFTPSESSIONA) WININET_GetObject( hFtpSession );
    if (NULL == lpwfs || WH_HFTPSESSION != lpwfs->hdr.htype)
    {
        INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_HANDLE_TYPE);
        return FALSE;
    }

    /* Clear any error information */
    INTERNET_SetLastError(0);

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_RNFR, lpszSrc, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket,
        INTERNET_GetResponseBuffer(), MAX_REPLY_LEN, 0, 0, 0);
    if (nResCode == 350)
    {
        if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_RNTO, lpszDest, 0, 0, 0))
            goto lend;

        nResCode = FTP_ReceiveResponse(lpwfs->sndSocket,
		    INTERNET_GetResponseBuffer(), MAX_REPLY_LEN, 0, 0, 0);
    }

    if (nResCode == 250)
        bSuccess = TRUE;
    else
        FTP_SetResponseError(nResCode);

lend:
    hIC = (LPWININETAPPINFOW) lpwfs->hdr.lpwhparent;
    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC && hIC->lpfnStatusCB)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)bSuccess;
        iar.dwError = bSuccess ? ERROR_SUCCESS : INTERNET_GetLastError();
        hIC->lpfnStatusCB(hFtpSession, lpwfs->hdr.dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    return bSuccess;
}


/***********************************************************************
 *           FTP_Connect (internal)
 *
 * Connect to a ftp server
 *
 * RETURNS
 *   HINTERNET a session handle on success
 *   NULL on failure
 *
 */

HINTERNET FTP_Connect(HINTERNET hInternet, LPCWSTR lpszServerName,
	INTERNET_PORT nServerPort, LPCWSTR lpszUserName,
	LPCWSTR lpszPassword, DWORD dwFlags, DWORD dwContext)
{
    struct sockaddr_in socketAddr;
    struct hostent *phe = NULL;
    INT nsocket = -1, sock_namelen;
    LPWININETAPPINFOW hIC = NULL;
    BOOL bSuccess = FALSE;
    LPWININETFTPSESSIONA lpwfs = NULL;
    HINTERNET handle = NULL;

    TRACE("0x%08lx  Server(%s) Port(%d) User(%s) Paswd(%s)\n",
	    (ULONG) hInternet, debugstr_w(lpszServerName),
	    nServerPort, debugstr_w(lpszUserName), debugstr_w(lpszPassword));

    hIC = (LPWININETAPPINFOW) WININET_GetObject( hInternet );
    if ( (hIC == NULL) || (hIC->hdr.htype != WH_HINIT) )
        goto lerror;

    if (NULL == lpszUserName && NULL != lpszPassword)
    {
	INTERNET_SetLastError(ERROR_INTERNET_INCORRECT_USER_NAME);
        goto lerror;
    }

    if (nServerPort == INTERNET_INVALID_PORT_NUMBER)
	nServerPort = INTERNET_DEFAULT_FTP_PORT;

    if (hIC->lpfnStatusCB)
        hIC->lpfnStatusCB(hInternet, dwContext, INTERNET_STATUS_RESOLVING_NAME,
            (LPWSTR) lpszServerName, strlenW(lpszServerName));

    if (!GetAddress(lpszServerName, nServerPort, &phe, &socketAddr))
    {
	INTERNET_SetLastError(ERROR_INTERNET_NAME_NOT_RESOLVED);
        goto lerror;
    }

    if (hIC->lpfnStatusCB)
        hIC->lpfnStatusCB(hInternet, dwContext, INTERNET_STATUS_NAME_RESOLVED,
            (LPWSTR) lpszServerName, strlenW(lpszServerName));

    nsocket = socket(AF_INET,SOCK_STREAM,0);
    if (nsocket == -1)
    {
	INTERNET_SetLastError(ERROR_INTERNET_CANNOT_CONNECT);
        goto lerror;
    }

    if (hIC->lpfnStatusCB)
        hIC->lpfnStatusCB(hInternet, dwContext, INTERNET_STATUS_CONNECTING_TO_SERVER,
             &socketAddr, sizeof(struct sockaddr_in));

    if (connect(nsocket, (struct sockaddr *)&socketAddr, sizeof(socketAddr)) < 0)
    {
	ERR("Unable to connect (%s)\n", strerror(errno));
	INTERNET_SetLastError(ERROR_INTERNET_CANNOT_CONNECT);
    }
    else
    {
        TRACE("Connected to server\n");
        if (hIC->lpfnStatusCB)
            hIC->lpfnStatusCB(hInternet, dwContext, INTERNET_STATUS_CONNECTED_TO_SERVER,
                &socketAddr, sizeof(struct sockaddr_in));

        lpwfs = HeapAlloc(GetProcessHeap(), 0, sizeof(WININETFTPSESSIONA));
        if (NULL == lpwfs)
        {
            INTERNET_SetLastError(ERROR_OUTOFMEMORY);
            goto lerror;
        }

        handle = WININET_AllocHandle( &lpwfs->hdr );
        if( !handle )
        {
            INTERNET_SetLastError(ERROR_OUTOFMEMORY);
            goto lerror;
        }

        lpwfs->hdr.htype = WH_HFTPSESSION;
        lpwfs->hdr.dwFlags = dwFlags;
        lpwfs->hdr.dwContext = dwContext;
        lpwfs->hdr.lpwhparent = (LPWININETHANDLEHEADER)hInternet;
        lpwfs->sndSocket = nsocket;
	lpwfs->download_in_progress = NULL;
	sock_namelen = sizeof(lpwfs->socketAddress);
	getsockname(nsocket, (struct sockaddr *) &lpwfs->socketAddress, &sock_namelen);
        lpwfs->phostent = phe;

        if (NULL == lpszUserName)
        {
            lpwfs->lpszUserName = FTP_strdup("anonymous");
            lpwfs->lpszPassword = FTP_strdup("user@server");
        }
        else
        {
            lpwfs->lpszUserName = FTP_strdup_WtoA(lpszUserName);
            lpwfs->lpszPassword = FTP_strdup_WtoA(lpszPassword);
        }

        if (FTP_ConnectToHost(lpwfs))
        {
            if (hIC->lpfnStatusCB)
            {
                INTERNET_ASYNC_RESULT iar;

                iar.dwResult = (DWORD)handle;
                iar.dwError = ERROR_SUCCESS;

                hIC->lpfnStatusCB(hInternet, dwContext, INTERNET_STATUS_HANDLE_CREATED,
                    &iar, sizeof(INTERNET_ASYNC_RESULT));
            }
            TRACE("Successfully logged into server\n");
            bSuccess = TRUE;
        }
    }

lerror:
    if (!bSuccess && nsocket == -1)
        close(nsocket);

    if (!bSuccess && lpwfs)
    {
        HeapFree(GetProcessHeap(), 0, lpwfs);
        WININET_FreeHandle( handle );
        lpwfs = NULL;
    }

    if (hIC->hdr.dwFlags & INTERNET_FLAG_ASYNC && hIC->lpfnStatusCB)
    {
        INTERNET_ASYNC_RESULT iar;

        iar.dwResult = (DWORD)lpwfs;
        iar.dwError = bSuccess ? ERROR_SUCCESS : INTERNET_GetLastError();
        hIC->lpfnStatusCB(hInternet, dwContext, INTERNET_STATUS_REQUEST_COMPLETE,
            &iar, sizeof(INTERNET_ASYNC_RESULT));
    }

    return handle;
}


/***********************************************************************
 *           FTP_ConnectToHost (internal)
 *
 * Connect to a ftp server
 *
 * RETURNS
 *   TRUE on success
 *   NULL on failure
 *
 */
BOOL FTP_ConnectToHost(LPWININETFTPSESSIONA lpwfs)
{
    INT nResCode;
    BOOL bSuccess = FALSE;

    TRACE("\n");
    FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(), MAX_REPLY_LEN, 0, 0, 0);

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_USER, lpwfs->lpszUserName, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
        MAX_REPLY_LEN, 0, 0, 0);
    if (nResCode)
    {
        /* Login successful... */
        if (nResCode == 230)
            bSuccess = TRUE;
        /* User name okay, need password... */
        else if (nResCode == 331)
            bSuccess = FTP_SendPassword(lpwfs);
        /* Need account for login... */
        else if (nResCode == 332)
            bSuccess = FTP_SendAccount(lpwfs);
        else
            FTP_SetResponseError(nResCode);
    }

    TRACE("Returning %d\n", bSuccess);
lend:
    return bSuccess;
}


/***********************************************************************
 *           FTP_SendCommand (internal)
 *
 * Send command to server
 *
 * RETURNS
 *   TRUE on success
 *   NULL on failure
 *
 */
BOOL FTP_SendCommand(INT nSocket, FTP_COMMAND ftpCmd, LPCSTR lpszParam,
	INTERNET_STATUS_CALLBACK lpfnStatusCB, HINTERNET hHandle, DWORD dwContext)
{
    	DWORD len;
	CHAR *buf;
	DWORD nBytesSent = 0;
	DWORD nRC = 0;
	BOOL bParamHasLen;

	TRACE("%d: (%s) %d\n", ftpCmd, lpszParam, nSocket);

	if (lpfnStatusCB)
		lpfnStatusCB(hHandle, dwContext, INTERNET_STATUS_SENDING_REQUEST, NULL, 0);

	bParamHasLen = lpszParam && strlen(lpszParam) > 0;
	len = (bParamHasLen ? strlen(lpszParam) : -1) + strlen(szFtpCommands[ftpCmd]) +
	    strlen(szCRLF)+ 1;
	if (NULL == (buf = HeapAlloc(GetProcessHeap(), 0, len+1)))
	{
	    INTERNET_SetLastError(ERROR_OUTOFMEMORY);
	    return FALSE;
	}
	sprintf(buf, "%s%s%s%s", szFtpCommands[ftpCmd], bParamHasLen ? " " : "",
		bParamHasLen ? lpszParam : "", szCRLF);

	TRACE("Sending (%s) len(%ld)\n", buf, len);
	while((nBytesSent < len) && (nRC != -1))
	{
		nRC = send(nSocket, buf+nBytesSent, len - nBytesSent, 0);
		nBytesSent += nRC;
	}

	HeapFree(GetProcessHeap(), 0, (LPVOID)buf);

	if (lpfnStatusCB)
		lpfnStatusCB(hHandle, dwContext, INTERNET_STATUS_REQUEST_SENT,
			&nBytesSent, sizeof(DWORD));

	TRACE("Sent %ld bytes\n", nBytesSent);
	return (nRC != -1);
}


/***********************************************************************
 *           FTP_ReceiveResponse (internal)
 *
 * Receive response from server
 *
 * RETURNS
 *   Reply code on success
 *   0 on failure
 *
 */

INT FTP_ReceiveResponse(INT nSocket, LPSTR lpszResponse, DWORD dwResponse,
	INTERNET_STATUS_CALLBACK lpfnStatusCB, HINTERNET hHandle, DWORD dwContext)
{
    DWORD nRecv;
    INT rc = 0;
    char firstprefix[5];
    BOOL multiline = FALSE;


    TRACE("socket(%d) \n", nSocket);

    if (lpfnStatusCB)
        lpfnStatusCB(hHandle, dwContext, INTERNET_STATUS_RECEIVING_RESPONSE, NULL, 0);

    while(1)
    {
	nRecv = dwResponse;
	if (!INTERNET_GetNextLine(nSocket, lpszResponse, &nRecv))
	    goto lerror;

        if (nRecv >= 3)
	{
	    if(!multiline)
	    {
	        if(lpszResponse[3] != '-')
		    break;
		else
		{  /* Start of multiline repsonse.  Loop until we get "nnn " */
		    multiline = TRUE;
		    memcpy(firstprefix, lpszResponse, 3);
		    firstprefix[3] = ' ';
		    firstprefix[4] = '\0';
		}
	    }
	    else
	    {
	        if(!memcmp(firstprefix, lpszResponse, 4))
		    break;
	    }
	}
    }

    if (nRecv >= 3)
    {
        rc = atoi(lpszResponse);

        if (lpfnStatusCB)
            lpfnStatusCB(hHandle, dwContext, INTERNET_STATUS_RESPONSE_RECEIVED,
		    &nRecv, sizeof(DWORD));
    }

lerror:
    TRACE("return %d\n", rc);
    return rc;
}


/***********************************************************************
 *           FTP_SendPassword (internal)
 *
 * Send password to ftp server
 *
 * RETURNS
 *   TRUE on success
 *   NULL on failure
 *
 */
BOOL FTP_SendPassword(LPWININETFTPSESSIONA lpwfs)
{
    INT nResCode;
    BOOL bSuccess = FALSE;

    TRACE("\n");
    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_PASS, lpwfs->lpszPassword, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
        MAX_REPLY_LEN, 0, 0, 0);
    if (nResCode)
    {
        TRACE("Received reply code %d\n", nResCode);
        /* Login successful... */
        if (nResCode == 230)
            bSuccess = TRUE;
        /* Command not implemented, superfluous at the server site... */
        /* Need account for login... */
        else if (nResCode == 332)
            bSuccess = FTP_SendAccount(lpwfs);
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    TRACE("Returning %d\n", bSuccess);
    return bSuccess;
}


/***********************************************************************
 *           FTP_SendAccount (internal)
 *
 *
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL FTP_SendAccount(LPWININETFTPSESSIONA lpwfs)
{
    INT nResCode;
    BOOL bSuccess = FALSE;

    TRACE("\n");
    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_ACCT, NOACCOUNT, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
        MAX_REPLY_LEN, 0,  0, 0);
    if (nResCode)
        bSuccess = TRUE;
    else
        FTP_SetResponseError(nResCode);

lend:
    return bSuccess;
}


/***********************************************************************
 *           FTP_SendStore (internal)
 *
 * Send request to upload file to ftp server
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL FTP_SendStore(LPWININETFTPSESSIONA lpwfs, LPCSTR lpszRemoteFile, DWORD dwType)
{
    INT nResCode;
    BOOL bSuccess = FALSE;

    TRACE("\n");
    if (!FTP_InitListenSocket(lpwfs))
        goto lend;

    if (!FTP_SendType(lpwfs, dwType))
        goto lend;

    if (!FTP_SendPortOrPasv(lpwfs))
        goto lend;

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_STOR, lpszRemoteFile, 0, 0, 0))
	    goto lend;
    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
       MAX_REPLY_LEN, 0, 0, 0);
    if (nResCode)
    {
        if (nResCode == 150)
            bSuccess = TRUE;
	else
            FTP_SetResponseError(nResCode);
    }

lend:
    if (!bSuccess && lpwfs->lstnSocket != -1)
    {
        close(lpwfs->lstnSocket);
        lpwfs->lstnSocket = -1;
    }

    return bSuccess;
}


/***********************************************************************
 *           FTP_InitListenSocket (internal)
 *
 * Create a socket to listen for server response
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL FTP_InitListenSocket(LPWININETFTPSESSIONA lpwfs)
{
    BOOL bSuccess = FALSE;
    size_t namelen = sizeof(struct sockaddr_in);

    TRACE("\n");

    lpwfs->lstnSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (lpwfs->lstnSocket == -1)
    {
        TRACE("Unable to create listening socket\n");
            goto lend;
    }

    /* We obtain our ip addr from the name of the command channel socket */
    lpwfs->lstnSocketAddress = lpwfs->socketAddress;

    /* and get the system to assign us a port */
    lpwfs->lstnSocketAddress.sin_port = htons((u_short) 0);

    if (bind(lpwfs->lstnSocket,(struct sockaddr *) &lpwfs->lstnSocketAddress, sizeof(struct sockaddr_in)) == -1)
    {
        TRACE("Unable to bind socket\n");
        goto lend;
    }

    if (listen(lpwfs->lstnSocket, MAX_BACKLOG) == -1)
    {
        TRACE("listen failed\n");
        goto lend;
    }

    if (getsockname(lpwfs->lstnSocket, (struct sockaddr *) &lpwfs->lstnSocketAddress, &namelen) != -1)
        bSuccess = TRUE;

lend:
    if (!bSuccess && lpwfs->lstnSocket == -1)
    {
        close(lpwfs->lstnSocket);
        lpwfs->lstnSocket = -1;
    }

    return bSuccess;
}


/***********************************************************************
 *           FTP_SendType (internal)
 *
 * Tell server type of data being transferred
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 * W98SE doesn't cache the type that's currently set
 * (i.e. it sends it always),
 * so we probably don't want to do that either.
 */
BOOL FTP_SendType(LPWININETFTPSESSIONA lpwfs, DWORD dwType)
{
    INT nResCode;
    CHAR type[2] = { "I" };
    BOOL bSuccess = FALSE;

    TRACE("\n");
    if (dwType & INTERNET_FLAG_TRANSFER_ASCII)
        *type = 'A';

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_TYPE, type, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
        MAX_REPLY_LEN, 0, 0, 0)/100;
    if (nResCode)
    {
        if (nResCode == 2)
            bSuccess = TRUE;
	else
            FTP_SetResponseError(nResCode);
    }

lend:
    return bSuccess;
}

/***********************************************************************
 *           FTP_GetFileSize (internal)
 *
 * Retrieves from the server the size of the given file
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL FTP_GetFileSize(LPWININETFTPSESSIONA lpwfs, LPCSTR lpszRemoteFile, DWORD *dwSize)
{
    INT nResCode;
    BOOL bSuccess = FALSE;

    TRACE("\n");

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_SIZE, lpszRemoteFile, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
        MAX_REPLY_LEN, 0, 0, 0);
    if (nResCode)
    {
        if (nResCode == 213) {
	    /* Now parses the output to get the actual file size */
	    int i;
	    LPSTR lpszResponseBuffer = INTERNET_GetResponseBuffer();

	    for (i = 0; (lpszResponseBuffer[i] != ' ') && (lpszResponseBuffer[i] != '\0'); i++) ;
	    if (lpszResponseBuffer[i] == '\0') return FALSE;
	    *dwSize = atol(&(lpszResponseBuffer[i + 1]));
	    
            bSuccess = TRUE;
	} else {
            FTP_SetResponseError(nResCode);
	}
    }

lend:
    return bSuccess;
}


/***********************************************************************
 *           FTP_SendPort (internal)
 *
 * Tell server which port to use
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL FTP_SendPort(LPWININETFTPSESSIONA lpwfs)
{
    INT nResCode;
    CHAR szIPAddress[64];
    BOOL bSuccess = FALSE;
    TRACE("\n");

    sprintf(szIPAddress, "%d,%d,%d,%d,%d,%d",
	 lpwfs->lstnSocketAddress.sin_addr.s_addr&0x000000FF,
        (lpwfs->lstnSocketAddress.sin_addr.s_addr&0x0000FF00)>>8,
        (lpwfs->lstnSocketAddress.sin_addr.s_addr&0x00FF0000)>>16,
        (lpwfs->lstnSocketAddress.sin_addr.s_addr&0xFF000000)>>24,
        lpwfs->lstnSocketAddress.sin_port & 0xFF,
        (lpwfs->lstnSocketAddress.sin_port & 0xFF00)>>8);

    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_PORT, szIPAddress, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
        MAX_REPLY_LEN,0, 0, 0);
    if (nResCode)
    {
        if (nResCode == 200)
            bSuccess = TRUE;
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    return bSuccess;
}


/***********************************************************************
 *           FTP_DoPassive (internal)
 *
 * Tell server that we want to do passive transfers
 * and connect data socket
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL FTP_DoPassive(LPWININETFTPSESSIONA lpwfs)
{
    INT nResCode;
    BOOL bSuccess = FALSE;

    TRACE("\n");
    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_PASV, NULL, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
        MAX_REPLY_LEN,0, 0, 0);
    if (nResCode)
    {
        if (nResCode == 227)
	{
	    LPSTR lpszResponseBuffer = INTERNET_GetResponseBuffer();
	    LPSTR p;
	    int f[6];
	    int i;
	    char *pAddr, *pPort;
	    INT nsocket = -1;
	    struct sockaddr_in dataSocketAddress;

	    p = lpszResponseBuffer+4; /* skip status code */

	    /* do a very strict check; we can improve that later. */

	    if (strncmp(p, "Entering Passive Mode", 21))
	    {
		ERR("unknown response '%.*s', aborting\n", 21, p);
		goto lend;
	    }
	    p += 21; /* skip string */
	    if ((*p++ != ' ') || (*p++ != '('))
	    {
		ERR("unknown response format, aborting\n");
		goto lend;
	    }

	    if (sscanf(p, "%d,%d,%d,%d,%d,%d",  &f[0], &f[1], &f[2], &f[3],
				    		&f[4], &f[5]) != 6)
	    {
		ERR("unknown response address format '%s', aborting\n", p);
		goto lend;
	    }
	    for (i=0; i < 6; i++)
		f[i] = f[i] & 0xff;

	    dataSocketAddress = lpwfs->socketAddress;
	    pAddr = (char *)&(dataSocketAddress.sin_addr.s_addr);
	    pPort = (char *)&(dataSocketAddress.sin_port);
            pAddr[0] = f[0];
            pAddr[1] = f[1];
            pAddr[2] = f[2];
            pAddr[3] = f[3];
	    pPort[0] = f[4];
	    pPort[1] = f[5];

            nsocket = socket(AF_INET,SOCK_STREAM,0);
            if (nsocket == -1)
                goto lend;

	    if (connect(nsocket, (struct sockaddr *)&dataSocketAddress, sizeof(dataSocketAddress)))
            {
	        ERR("can't connect passive FTP data port.\n");
	        goto lend;
            }
	    lpwfs->pasvSocket = nsocket;
            bSuccess = TRUE;
	}
        else
            FTP_SetResponseError(nResCode);
    }

lend:
    return bSuccess;
}


BOOL FTP_SendPortOrPasv(LPWININETFTPSESSIONA lpwfs)
{
    if (lpwfs->hdr.dwFlags & INTERNET_FLAG_PASSIVE)
    {
        if (!FTP_DoPassive(lpwfs))
            return FALSE;
    }
    else
    {
	if (!FTP_SendPort(lpwfs))
            return FALSE;
    }
    return TRUE;
}


/***********************************************************************
 *           FTP_GetDataSocket (internal)
 *
 * Either accepts an incoming data socket connection from the server
 * or just returns the already opened socket after a PASV command
 * in case of passive FTP.
 *
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL FTP_GetDataSocket(LPWININETFTPSESSIONA lpwfs, LPINT nDataSocket)
{
    struct sockaddr_in saddr;
    size_t addrlen = sizeof(struct sockaddr);

    TRACE("\n");
    if (lpwfs->hdr.dwFlags & INTERNET_FLAG_PASSIVE)
    {
	*nDataSocket = lpwfs->pasvSocket;
    }
    else
    {
        *nDataSocket = accept(lpwfs->lstnSocket, (struct sockaddr *) &saddr, &addrlen);
        close(lpwfs->lstnSocket);
        lpwfs->lstnSocket = -1;
    }
    return *nDataSocket != -1;
}


/***********************************************************************
 *           FTP_SendData (internal)
 *
 * Send data to the server
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL FTP_SendData(LPWININETFTPSESSIONA lpwfs, INT nDataSocket, HANDLE hFile)
{
    BY_HANDLE_FILE_INFORMATION fi;
    DWORD nBytesRead = 0;
    DWORD nBytesSent = 0;
    DWORD nTotalSent = 0;
    DWORD nBytesToSend, nLen, nRC = 1;
    time_t s_long_time, e_long_time;
    LONG nSeconds;
    CHAR *lpszBuffer;

    TRACE("\n");
    lpszBuffer = HeapAlloc(GetProcessHeap(), 0, sizeof(CHAR)*DATA_PACKET_SIZE);
    memset(lpszBuffer, 0, sizeof(CHAR)*DATA_PACKET_SIZE);

    /* Get the size of the file. */
    GetFileInformationByHandle(hFile, &fi);
    time(&s_long_time);

    do
    {
        nBytesToSend = nBytesRead - nBytesSent;

        if (nBytesToSend <= 0)
        {
            /* Read data from file. */
            nBytesSent = 0;
            if (!ReadFile(hFile, lpszBuffer, DATA_PACKET_SIZE, &nBytesRead, 0))
            ERR("Failed reading from file\n");

            if (nBytesRead > 0)
                nBytesToSend = nBytesRead;
            else
                break;
        }

        nLen = DATA_PACKET_SIZE < nBytesToSend ?
            DATA_PACKET_SIZE : nBytesToSend;
        nRC  = send(nDataSocket, lpszBuffer, nLen, 0);

        if (nRC != -1)
        {
            nBytesSent += nRC;
            nTotalSent += nRC;
        }

        /* Do some computation to display the status. */
        time(&e_long_time);
        nSeconds = e_long_time - s_long_time;
        if( nSeconds / 60 > 0 )
        {
            TRACE( "%ld bytes of %ld bytes (%ld%%) in %ld min %ld sec estimated remainig time %ld sec\n",
            nTotalSent, fi.nFileSizeLow, nTotalSent*100/fi.nFileSizeLow, nSeconds / 60,
            nSeconds % 60, (fi.nFileSizeLow - nTotalSent) * nSeconds / nTotalSent );
        }
        else
        {
            TRACE( "%ld bytes of %ld bytes (%ld%%) in %ld sec estimated remainig time %ld sec\n",
            nTotalSent, fi.nFileSizeLow, nTotalSent*100/fi.nFileSizeLow, nSeconds,
            (fi.nFileSizeLow - nTotalSent) * nSeconds / nTotalSent);
        }
    } while (nRC != -1);

    TRACE("file transfer complete!\n");

    if(lpszBuffer != NULL)
        HeapFree(GetProcessHeap(), 0, lpszBuffer);

    return nTotalSent;
}


/***********************************************************************
 *           FTP_SendRetrieve (internal)
 *
 * Send request to retrieve a file
 *
 * RETURNS
 *   Number of bytes to be received on success
 *   0 on failure
 *
 */
DWORD FTP_SendRetrieve(LPWININETFTPSESSIONA lpwfs, LPCSTR lpszRemoteFile, DWORD dwType)
{
    INT nResCode;
    DWORD nResult = 0;

    TRACE("\n");
    if (!FTP_InitListenSocket(lpwfs))
        goto lend;

    if (!FTP_SendType(lpwfs, dwType))
        goto lend;

    if (!FTP_SendPortOrPasv(lpwfs))
        goto lend;

    if (!FTP_GetFileSize(lpwfs, lpszRemoteFile, &nResult))
	goto lend;

    TRACE("Waiting to receive %ld bytes\n", nResult);
    
    if (!FTP_SendCommand(lpwfs->sndSocket, FTP_CMD_RETR, lpszRemoteFile, 0, 0, 0))
        goto lend;

    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
        MAX_REPLY_LEN, 0, 0, 0);
    if ((nResCode != 125) && (nResCode != 150)) {
	/* That means that we got an error getting the file. */
	nResult = 0;
    }

lend:
    if (0 == nResult && lpwfs->lstnSocket != -1)
    {
        close(lpwfs->lstnSocket);
        lpwfs->lstnSocket = -1;
    }

    return nResult;
}


/***********************************************************************
 *           FTP_RetrieveData  (internal)
 *
 * Retrieve data from server
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL FTP_RetrieveFileData(LPWININETFTPSESSIONA lpwfs, INT nDataSocket, DWORD nBytes, HANDLE hFile)
{
    DWORD nBytesWritten;
    DWORD nBytesReceived = 0;
    INT nRC = 0;
    CHAR *lpszBuffer;

    TRACE("\n");

    if (INVALID_HANDLE_VALUE == hFile)
        return FALSE;

    lpszBuffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CHAR)*DATA_PACKET_SIZE);
    if (NULL == lpszBuffer)
    {
        INTERNET_SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }

    while (nBytesReceived < nBytes && nRC != -1)
    {
        nRC = recv(nDataSocket, lpszBuffer, DATA_PACKET_SIZE, 0);
        if (nRC != -1)
        {
            /* other side closed socket. */
            if (nRC == 0)
                goto recv_end;
            WriteFile(hFile, lpszBuffer, nRC, &nBytesWritten, NULL);
            nBytesReceived += nRC;
        }

        TRACE("%ld bytes of %ld (%ld%%)\r", nBytesReceived, nBytes,
           nBytesReceived * 100 / nBytes);
    }

    TRACE("Data transfer complete\n");
    if (NULL != lpszBuffer)
        HeapFree(GetProcessHeap(), 0, lpszBuffer);

recv_end:
    return  (nRC != -1);
}


/***********************************************************************
 *           FTP_CloseSessionHandle (internal)
 *
 * Deallocate session handle
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL FTP_CloseSessionHandle(LPWININETFTPSESSIONA lpwfs)
{
    TRACE("\n");

    if (lpwfs->download_in_progress != NULL)
	lpwfs->download_in_progress->session_deleted = TRUE;
    
    if (lpwfs->sndSocket != -1)
        close(lpwfs->sndSocket);

    if (lpwfs->lstnSocket != -1)
        close(lpwfs->lstnSocket);

    if (lpwfs->lpszPassword)
        HeapFree(GetProcessHeap(), 0, lpwfs->lpszPassword);

    if (lpwfs->lpszUserName)
        HeapFree(GetProcessHeap(), 0, lpwfs->lpszUserName);

    HeapFree(GetProcessHeap(), 0, lpwfs);

    return TRUE;
}


/***********************************************************************
 *           FTP_CloseFindNextHandle (internal)
 *
 * Deallocate session handle
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL FTP_CloseFindNextHandle(LPWININETFINDNEXTA lpwfn)
{
    INT i;

    TRACE("\n");

    for (i = 0; i < lpwfn->size; i++)
    {
        if (NULL != lpwfn->lpafp[i].lpszName)
            HeapFree(GetProcessHeap(), 0, lpwfn->lpafp[i].lpszName);
    }

    HeapFree(GetProcessHeap(), 0, lpwfn->lpafp);
    HeapFree(GetProcessHeap(), 0, lpwfn);

    return TRUE;
}

/***********************************************************************
 *           FTP_CloseFileTransferHandle (internal)
 *
 * Closes the file transfer handle. This also 'cleans' the data queue of
 * the 'transfer conplete' message (this is a bit of a hack though :-/ )
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL FTP_CloseFileTransferHandle(LPWININETFILE lpwh)
{
    LPWININETFTPSESSIONA lpwfs = (LPWININETFTPSESSIONA) lpwh->hdr.lpwhparent;
    INT nResCode;

    TRACE("\n");

    if (!lpwh->session_deleted)
	lpwfs->download_in_progress = NULL;

    /* This just serves to flush the control socket of any spurrious lines written
       to it (like '226 Transfer complete.').

       Wonder what to do if the server sends us an error code though...
    */
    nResCode = FTP_ReceiveResponse(lpwfs->sndSocket, INTERNET_GetResponseBuffer(),
				   MAX_REPLY_LEN, 0, 0, 0);
    
    if (lpwh->nDataSocket != -1)
        close(lpwh->nDataSocket);

    HeapFree(GetProcessHeap(), 0, lpwh);
    
    return TRUE;
}

/***********************************************************************
 *           FTP_ReceiveFileList (internal)
 *
 * Read file list from server
 *
 * RETURNS
 *   Handle to file list on success
 *   NULL on failure
 *
 */
HINTERNET FTP_ReceiveFileList(LPWININETFTPSESSIONA lpwfs, INT nSocket,
	LPWIN32_FIND_DATAA lpFindFileData, DWORD dwContext)
{
    DWORD dwSize = 0;
    LPFILEPROPERTIESA lpafp = NULL;
    LPWININETFINDNEXTA lpwfn = NULL;
    HINTERNET handle = 0;

    TRACE("\n");

    if (FTP_ParseDirectory(lpwfs, nSocket, &lpafp, &dwSize))
    {
        FTP_ConvertFileProp(lpafp, lpFindFileData);

        lpwfn = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WININETFINDNEXTA));
        if (lpwfn)
        {
            handle = WININET_AllocHandle( &lpwfn->hdr );
            if( handle )
            {
                lpwfn->hdr.htype = WH_HFINDNEXT;
                lpwfn->hdr.lpwhparent = (LPWININETHANDLEHEADER)lpwfs;
	        lpwfn->hdr.dwContext = dwContext;
                lpwfn->index = 1; /* Next index is 1 since we return index 0 */
                lpwfn->size = dwSize;
                lpwfn->lpafp = lpafp;
            }
            else
                HeapFree( GetProcessHeap(), 0, lpwfn );
        }
    }

    TRACE("Matched %ld files\n", dwSize);
    return handle;
}


/***********************************************************************
 *           FTP_ConvertFileProp (internal)
 *
 * Converts FILEPROPERTIESA struct to WIN32_FIND_DATAA
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL FTP_ConvertFileProp(LPFILEPROPERTIESA lpafp, LPWIN32_FIND_DATAA lpFindFileData)
{
    BOOL bSuccess = FALSE;

    ZeroMemory(lpFindFileData, sizeof(WIN32_FIND_DATAA));

    if (lpafp)
    {
	/* Convert 'Unix' time to Windows time */
	RtlSecondsSince1970ToTime(mktime(&lpafp->tmLastModified),
				  (LARGE_INTEGER *) &(lpFindFileData->ftLastAccessTime));
	
        /* Not all fields are filled in */
        lpFindFileData->nFileSizeHigh = 0; /* We do not handle files bigger than 0xFFFFFFFF bytes yet :-) */
        lpFindFileData->nFileSizeLow = lpafp->nSize;

	if (lpafp->bIsDirectory)
	    lpFindFileData->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;

        if (lpafp->lpszName)
            strncpy(lpFindFileData->cFileName, lpafp->lpszName, MAX_PATH);

	bSuccess = TRUE;
    }

    return bSuccess;
}


/***********************************************************************
 *           FTP_ParseDirectory (internal)
 *
 * Parse string of directory information
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 * FIXME: - This function needs serious clea-up
 *        - We should consider both UNIX and NT list formats
 */
#define MAX_MONTH_LEN 10
#define MIN_LEN_DIR_ENTRY 15

BOOL FTP_ParseDirectory(LPWININETFTPSESSIONA lpwfs, INT nSocket, LPFILEPROPERTIESA *lpafp, LPDWORD dwfp)
{
  /*
   * <Permissions> <NoLinks> <owner>   <group> <size> <date>  <time or year> <filename>
   *
   * For instance:
   * drwx--s---     2         pcarrier  ens     512    Sep 28  1995           pcarrier
   */
    CHAR* pszMinutes;
    CHAR* pszHour;
    time_t aTime;
    struct tm* apTM;
    CHAR pszMonth[MAX_MONTH_LEN];
    CHAR* pszMatch;
    BOOL bSuccess = TRUE;
    DWORD nBufLen = MAX_REPLY_LEN;
    LPFILEPROPERTIESA curFileProp = NULL;
    CHAR* pszLine  = NULL;
    CHAR* pszToken = NULL;
    INT nTokenToSkip = 3;
    INT nCount = 0;
    INT nSeconds = 0;
    INT nMinutes = 0;
    INT nHour    = 0;
    INT nDay     = 0;
    INT nMonth   = 0;
    INT nYear    = 0;
    INT sizeFilePropArray = 20;
    INT indexFilePropArray = 0;

    TRACE("\n");

    /* Allocate intial file properties array */
    *lpafp = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(FILEPROPERTIESA)*(sizeFilePropArray));
    if (NULL == lpafp)
    {
        bSuccess = FALSE;
        goto lend;
    }

    while ((pszLine = INTERNET_GetNextLine(nSocket, INTERNET_GetResponseBuffer(), &nBufLen)) != NULL)
    {
        if (sizeFilePropArray <= indexFilePropArray)
        {
            LPFILEPROPERTIESA tmpafp;

            sizeFilePropArray *= 2;
            tmpafp = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, *lpafp,
                sizeof(FILEPROPERTIESA)*sizeFilePropArray);
            if (NULL == tmpafp)
            {
                bSuccess = FALSE;
                goto lend;
            }

            *lpafp = tmpafp;
        }

        curFileProp = &((*lpafp)[indexFilePropArray]);

        /* First Parse the permissions. */
        pszToken = strtok(pszLine, " \t" );

        /* HACK! If this is not a file listing skip the line */
        if (!pszToken || nBufLen <= MIN_LEN_DIR_ENTRY)
	{
            nBufLen = MAX_REPLY_LEN;
            continue;
	}
	if (10 == strlen(pszToken)) {
	    /* Unix way of parsing ... */
	    FTP_ParsePermission(pszToken, curFileProp);
	    
	    nTokenToSkip = 3;
	    nCount = 0;
	    do {
		pszToken = strtok( NULL, " \t" );
		nCount++;
	    } while( nCount <= nTokenToSkip );
	    
	    /* Store the size of the file in the param list. */
	    TRACE("nSize-> %s\n", pszToken);
	    if (pszToken != NULL)
		curFileProp->nSize = atol(pszToken);
	    
	    /* Parse last modified time. */
	    nSeconds = 0;
	    nMinutes = 0;
	    nHour    = 0;
	    nDay     = 0;
	    nMonth   = 0;
	    nYear    = 0;
	    
	    pszToken = strtok( NULL, " \t" );
	    strncpy(pszMonth, pszToken, MAX_MONTH_LEN);
	    CharUpperA(pszMonth);
	    pszMatch = strstr(szMonths, pszMonth);
	    if( pszMatch != NULL )
		nMonth = (pszMatch - szMonths) / 3;
	    
	    pszToken = strtok(NULL, " \t");
	    TRACE("nDay -> %s\n", pszToken);
	    if (pszToken != NULL)
		nDay = atoi(pszToken);
	    
	    pszToken = strtok(NULL, " \t");
	    pszMinutes = strchr(pszToken, ':');
	    if( pszMinutes != NULL ) {
		pszMinutes++;
		nMinutes = atoi(pszMinutes);
		pszHour = pszMinutes - 3;
		if (pszHour != NULL)
		    nHour = atoi(pszHour);
		time(&aTime);
		apTM = localtime( &aTime );
		nYear = apTM->tm_year;
	    } else {
		nYear  = atoi(pszToken);
		nYear -= 1900;
		nHour  = 12;
	    }

	    curFileProp->tmLastModified.tm_sec  = nSeconds;
	    curFileProp->tmLastModified.tm_min  = nMinutes;
	    curFileProp->tmLastModified.tm_hour = nHour;
	    curFileProp->tmLastModified.tm_mday = nDay;
	    curFileProp->tmLastModified.tm_mon  = nMonth;
	    curFileProp->tmLastModified.tm_year = nYear;
	    
	    pszToken = strtok(NULL, " \t");
	    if(pszToken != NULL) {
		curFileProp->lpszName = FTP_strdup(pszToken);
		TRACE(": %s\n", curFileProp->lpszName);
	    }
	    
	    nBufLen = MAX_REPLY_LEN;
	    indexFilePropArray++;
	} else if (8 == strlen(pszToken)) {
	    /* NT way of parsing ... :
	       
	          07-13-03  08:55PM       <DIR>          sakpatch
		  05-09-03  06:02PM             12656686 2003-04-21bgm_cmd_e.rgz
	    */

	    curFileProp->permissions = 0xFFFF; /* No idea, put full permission :-) */
	    
	    sscanf(pszToken, "%d-%d-%d",
		   &curFileProp->tmLastModified.tm_mon,
		   &curFileProp->tmLastModified.tm_mday,
		   &curFileProp->tmLastModified.tm_year);

	    /* Hacky and bad Y2K protection :-) */
	    if (curFileProp->tmLastModified.tm_year < 70)
		curFileProp->tmLastModified.tm_year += 100;
	    
	    pszToken = strtok(NULL, " \t");
	    if (pszToken == NULL) {
		nBufLen = MAX_REPLY_LEN;
		continue;
	    }
	    sscanf(pszToken, "%d:%d",
		   &curFileProp->tmLastModified.tm_hour,
		   &curFileProp->tmLastModified.tm_min);
	    if ((pszToken[5] == 'P') && (pszToken[6] == 'M')) {
		curFileProp->tmLastModified.tm_hour += 12;
	    }
	    curFileProp->tmLastModified.tm_sec = 0;

	    TRACE("Mod time: %2d:%2d:%2d  %2d/%2d/%2d\n",
		  curFileProp->tmLastModified.tm_hour, curFileProp->tmLastModified.tm_min, curFileProp->tmLastModified.tm_sec,
		  (curFileProp->tmLastModified.tm_year >= 100) ? curFileProp->tmLastModified.tm_year - 100 : curFileProp->tmLastModified.tm_year,
		  curFileProp->tmLastModified.tm_mon, curFileProp->tmLastModified.tm_mday);
	    
	    pszToken = strtok(NULL, " \t");
	    if (pszToken == NULL) {
		nBufLen = MAX_REPLY_LEN;
		continue;
	    }
	    if (!strcasecmp(pszToken, "<DIR>")) {
		curFileProp->bIsDirectory = TRUE;
		TRACE("Is directory\n");
	    } else {
		curFileProp->bIsDirectory = FALSE;
		curFileProp->nSize = atol(pszToken);
		TRACE("nSize: %ld\n", curFileProp->nSize);
	    }
	    
	    pszToken = strtok(NULL, " \t");
	    if (pszToken == NULL) {
		nBufLen = MAX_REPLY_LEN;
		continue;
	    }
	    curFileProp->lpszName = FTP_strdup(pszToken);
	    TRACE("Name: %s\n", curFileProp->lpszName);
	    
	    nBufLen = MAX_REPLY_LEN;
	    indexFilePropArray++;	    
	} else {
	    nBufLen = MAX_REPLY_LEN;
	}
    }

    if (bSuccess && indexFilePropArray)
    {
        if (indexFilePropArray < sizeFilePropArray - 1)
        {
            LPFILEPROPERTIESA tmpafp;

            tmpafp = HeapReAlloc(GetProcessHeap(), 0, *lpafp,
                sizeof(FILEPROPERTIESA)*indexFilePropArray);
            if (NULL == tmpafp)
                *lpafp = tmpafp;
        }
        *dwfp = indexFilePropArray;
    }
    else
    {
        HeapFree(GetProcessHeap(), 0, *lpafp);
        INTERNET_SetLastError(ERROR_NO_MORE_FILES);
        bSuccess = FALSE;
    }

lend:
    return bSuccess;
}


/***********************************************************************
 *           FTP_ParsePermission (internal)
 *
 * Parse permission string of directory information
 *
 * RETURNS
 *   TRUE on success
 *   FALSE on failure
 *
 */
BOOL FTP_ParsePermission(LPCSTR lpszPermission, LPFILEPROPERTIESA lpfp)
{
    BOOL bSuccess = TRUE;
    unsigned short nPermission = 0;
    INT nPos = 1;
    INT nLast  = 9;

    TRACE("\n");
    if ((*lpszPermission != 'd') && (*lpszPermission != '-') && (*lpszPermission != 'l'))
    {
        bSuccess = FALSE;
        return bSuccess;
    }

    lpfp->bIsDirectory = (*lpszPermission == 'd');
    do
    {
        switch (nPos)
        {
            case 1:
                nPermission |= (*(lpszPermission+1) == 'r' ? 1 : 0) << 8;
                break;
            case 2:
                nPermission |= (*(lpszPermission+2) == 'w' ? 1 : 0) << 7;
                break;
            case 3:
                nPermission |= (*(lpszPermission+3) == 'x' ? 1 : 0) << 6;
                break;
            case 4:
                nPermission |= (*(lpszPermission+4) == 'r' ? 1 : 0) << 5;
                break;
            case 5:
                nPermission |= (*(lpszPermission+5) == 'w' ? 1 : 0) << 4;
                break;
            case 6:
                nPermission |= (*(lpszPermission+6) == 'x' ? 1 : 0) << 3;
                break;
            case 7:
                nPermission |= (*(lpszPermission+7) == 'r' ? 1 : 0) << 2;
                break;
            case 8:
                nPermission |= (*(lpszPermission+8) == 'w' ? 1 : 0) << 1;
                break;
            case 9:
                nPermission |= (*(lpszPermission+9) == 'x' ? 1 : 0);
                break;
        }
        nPos++;
    }while (nPos <= nLast);

    lpfp->permissions = nPermission;
    return bSuccess;
}


/***********************************************************************
 *           FTP_SetResponseError (internal)
 *
 * Set the appropriate error code for a given response from the server
 *
 * RETURNS
 *
 */
DWORD FTP_SetResponseError(DWORD dwResponse)
{
    DWORD dwCode = 0;

    switch(dwResponse)
    {
	case 421: /* Service not available - Server may be shutting down. */
	    dwCode = ERROR_INTERNET_TIMEOUT;
	    break;

	case 425: /* Cannot open data connection. */
	    dwCode = ERROR_INTERNET_CANNOT_CONNECT;
	    break;

	case 426: /* Connection closed, transer aborted. */
	    dwCode = ERROR_INTERNET_CONNECTION_ABORTED;
	    break;

	case 500: /* Syntax error. Command unrecognized. */
	case 501: /* Syntax error. Error in parameters or arguments. */
	    dwCode = ERROR_INTERNET_INCORRECT_FORMAT;
	    break;

	case 530: /* Not logged in. Login incorrect. */
	    dwCode = ERROR_INTERNET_LOGIN_FAILURE;
	    break;

	case 550: /* File action not taken. File not found or no access. */
	    dwCode = ERROR_INTERNET_ITEM_NOT_FOUND;
	    break;

	case 450: /* File action not taken. File may be busy. */
	case 451: /* Action aborted. Server error. */
	case 452: /* Action not taken. Insufficient storage space on server. */
	case 502: /* Command not implemented. */
	case 503: /* Bad sequence of command. */
	case 504: /* Command not implemented for that parameter. */
	case 532: /* Need account for storing files */
	case 551: /* Requested action aborted. Page type unknown */
	case 552: /* Action aborted. Exceeded storage allocation */
	case 553: /* Action not taken. File name not allowed. */

	default:
            dwCode = ERROR_INTERNET_INTERNAL_ERROR;
	    break;
    }

    INTERNET_SetLastError(dwCode);
    return dwCode;
}
