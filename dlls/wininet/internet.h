/*
 * Wininet
 *
 * Copyright 1999 Corel Corporation
 *
 * Ulrich Czekalla
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

#ifndef _WINE_INTERNET_H_
#define _WINE_INTERNET_H_

#include "wine/unicode.h"

#include <time.h>
#ifdef HAVE_NETDB_H
# include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <sys/types.h>
# include <netinet/in.h>
#endif
#ifdef HAVE_OPENSSL_SSL_H
#define DSA __ssl_DSA  /* avoid conflict with commctrl.h */
#undef FAR
# include <openssl/ssl.h>
#undef FAR
#define FAR do_not_use_this_in_wine
#undef DSA
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

/* used for netconnection.c stuff */
typedef struct
{
    BOOL useSSL;
    int socketFD;
#ifdef HAVE_OPENSSL_SSL_H
    SSL *ssl_s;
    int ssl_sock;
#endif
} WININET_NETCONNECTION;

inline static LPSTR WININET_strdup( LPCSTR str )
{
    LPSTR ret = HeapAlloc( GetProcessHeap(), 0, strlen(str) + 1 );
    if (ret) strcpy( ret, str );
    return ret;
}

inline static LPWSTR WININET_strdupW( LPCWSTR str )
{
    LPWSTR ret = HeapAlloc( GetProcessHeap(), 0, (strlenW(str) + 1)*sizeof(WCHAR) );
    if (ret) strcpyW( ret, str );
    return ret;
}

typedef enum
{
    WH_HINIT = INTERNET_HANDLE_TYPE_INTERNET,
    WH_HFTPSESSION = INTERNET_HANDLE_TYPE_CONNECT_FTP,
    WH_HGOPHERSESSION = INTERNET_HANDLE_TYPE_CONNECT_GOPHER,
    WH_HHTTPSESSION = INTERNET_HANDLE_TYPE_CONNECT_HTTP,
    WH_HFILE = INTERNET_HANDLE_TYPE_FTP_FILE,
    WH_HFINDNEXT = INTERNET_HANDLE_TYPE_FTP_FIND,
    WH_HHTTPREQ = INTERNET_HANDLE_TYPE_HTTP_REQUEST,
} WH_TYPE;

typedef struct _WININETHANDLEHEADER
{
    WH_TYPE htype;
    DWORD  dwFlags;
    DWORD  dwContext;
    DWORD  dwError;
    struct _WININETHANDLEHEADER *lpwhparent;
} WININETHANDLEHEADER, *LPWININETHANDLEHEADER;


typedef struct
{
    WININETHANDLEHEADER hdr;
    LPWSTR  lpszAgent;
    LPWSTR  lpszProxy;
    LPWSTR  lpszProxyBypass;
    LPWSTR  lpszProxyUsername;
    LPWSTR  lpszProxyPassword;
    DWORD   dwAccessType;
    INTERNET_STATUS_CALLBACK lpfnStatusCB;
} WININETAPPINFOW, *LPWININETAPPINFOW;


typedef struct
{
    WININETHANDLEHEADER hdr;
    LPWSTR  lpszServerName;
    LPWSTR  lpszUserName;
    INTERNET_PORT nServerPort;
    struct sockaddr_in socketAddress;
    struct hostent *phostent;
} WININETHTTPSESSIONW, *LPWININETHTTPSESSIONW;

#define HDR_ISREQUEST		0x0001
#define HDR_COMMADELIMITED	0x0002
#define HDR_SEMIDELIMITED	0x0004

typedef struct
{
    LPWSTR lpszField;
    LPWSTR lpszValue;
    WORD wFlags;
    WORD wCount;
} HTTPHEADERW, *LPHTTPHEADERW;


typedef struct
{
    WININETHANDLEHEADER hdr;
    LPWSTR lpszPath;
    LPWSTR lpszVerb;
    LPWSTR lpszHostName;
    WININET_NETCONNECTION netConnection;
    HTTPHEADERW StdHeaders[HTTP_QUERY_MAX+1];
    HTTPHEADERW *pCustHeaders;
    INT nCustHeaders;
} WININETHTTPREQW, *LPWININETHTTPREQW;


typedef struct
{
    WININETHANDLEHEADER hdr;
    BOOL session_deleted;
    int nDataSocket;
} WININETFILE, *LPWININETFILE;


typedef struct
{
    WININETHANDLEHEADER hdr;
    int sndSocket;
    int lstnSocket;
    int pasvSocket; /* data socket connected by us in case of passive FTP */
    LPWININETFILE download_in_progress;
    struct sockaddr_in socketAddress;
    struct sockaddr_in lstnSocketAddress;
    struct hostent *phostent;
    LPSTR  lpszPassword;
    LPSTR  lpszUserName;
} WININETFTPSESSIONA, *LPWININETFTPSESSIONA;


typedef struct
{
    BOOL bIsDirectory;
    LPSTR lpszName;
    DWORD nSize;
    struct tm tmLastModified;
    unsigned short permissions;
} FILEPROPERTIESA, *LPFILEPROPERTIESA;


typedef struct
{
    WININETHANDLEHEADER hdr;
    int index;
    DWORD size;
    LPFILEPROPERTIESA lpafp;
} WININETFINDNEXTA, *LPWININETFINDNEXTA;

typedef enum
{
    FTPPUTFILEA,
    FTPSETCURRENTDIRECTORYA,
    FTPCREATEDIRECTORYA,
    FTPFINDFIRSTFILEA,
    FTPGETCURRENTDIRECTORYA,
    FTPOPENFILEA,
    FTPGETFILEA,
    FTPDELETEFILEA,
    FTPREMOVEDIRECTORYA,
    FTPRENAMEFILEA,
    INTERNETFINDNEXTA,
    HTTPSENDREQUESTW,
    HTTPOPENREQUESTW,
    SENDCALLBACK,
    INTERNETOPENURLW,
} ASYNC_FUNC;

struct WORKREQ_FTPPUTFILEA
{
    LPSTR lpszLocalFile;
    LPSTR lpszNewRemoteFile;
    DWORD  dwFlags;
    DWORD  dwContext;
};

struct WORKREQ_FTPSETCURRENTDIRECTORYA
{
    LPSTR lpszDirectory;
};

struct WORKREQ_FTPCREATEDIRECTORYA
{
    LPSTR lpszDirectory;
};

struct WORKREQ_FTPFINDFIRSTFILEA
{
    LPSTR lpszSearchFile;
    LPWIN32_FIND_DATAA lpFindFileData;
    DWORD  dwFlags;
    DWORD  dwContext;
};

struct WORKREQ_FTPGETCURRENTDIRECTORYA
{
    LPSTR lpszDirectory;
    DWORD *lpdwDirectory;
};

struct WORKREQ_FTPOPENFILEA
{
    LPSTR lpszFilename;
    DWORD  dwAccess;
    DWORD  dwFlags;
    DWORD  dwContext;
};

struct WORKREQ_FTPGETFILEA
{
    LPSTR lpszRemoteFile;
    LPSTR lpszNewFile;
    BOOL   fFailIfExists;
    DWORD  dwLocalFlagsAttribute;
    DWORD  dwFlags;
    DWORD  dwContext;
};

struct WORKREQ_FTPDELETEFILEA
{
    LPSTR lpszFilename;
};

struct WORKREQ_FTPREMOVEDIRECTORYA
{
    LPSTR lpszDirectory;
};

struct WORKREQ_FTPRENAMEFILEA
{
    LPSTR lpszSrcFile;
    LPSTR lpszDestFile;
};

struct WORKREQ_INTERNETFINDNEXTA
{
    LPWIN32_FIND_DATAA lpFindFileData;
};

struct WORKREQ_HTTPOPENREQUESTW
{
    LPWSTR lpszVerb;
    LPWSTR lpszObjectName;
    LPWSTR lpszVersion;
    LPWSTR lpszReferrer;
    LPCWSTR *lpszAcceptTypes;
    DWORD  dwFlags;
    DWORD  dwContext;
};

struct WORKREQ_HTTPSENDREQUESTW
{
    LPWSTR lpszHeader;
    DWORD  dwHeaderLength;
    LPVOID lpOptional;
    DWORD  dwOptionalLength;
};

struct WORKREQ_SENDCALLBACK
{
    HINTERNET hHttpSession;
    DWORD     dwContext;
    DWORD     dwInternetStatus;
    LPVOID    lpvStatusInfo;
    DWORD     dwStatusInfoLength;
};

struct WORKREQ_INTERNETOPENURLW
{
    HINTERNET hInternet;
    LPWSTR     lpszUrl;
    LPWSTR     lpszHeaders;
    DWORD     dwHeadersLength;
    DWORD     dwFlags;
    DWORD     dwContext;
};

typedef struct WORKREQ
{
    ASYNC_FUNC asyncall;
    HINTERNET handle;

    union {
        struct WORKREQ_FTPPUTFILEA              FtpPutFileA;
        struct WORKREQ_FTPSETCURRENTDIRECTORYA  FtpSetCurrentDirectoryA;
        struct WORKREQ_FTPCREATEDIRECTORYA      FtpCreateDirectoryA;
        struct WORKREQ_FTPFINDFIRSTFILEA        FtpFindFirstFileA;
        struct WORKREQ_FTPGETCURRENTDIRECTORYA  FtpGetCurrentDirectoryA;
        struct WORKREQ_FTPOPENFILEA             FtpOpenFileA;
        struct WORKREQ_FTPGETFILEA              FtpGetFileA;
        struct WORKREQ_FTPDELETEFILEA           FtpDeleteFileA;
        struct WORKREQ_FTPREMOVEDIRECTORYA      FtpRemoveDirectoryA;
        struct WORKREQ_FTPRENAMEFILEA           FtpRenameFileA;
        struct WORKREQ_INTERNETFINDNEXTA        InternetFindNextA;
        struct WORKREQ_HTTPOPENREQUESTW         HttpOpenRequestW;
        struct WORKREQ_HTTPSENDREQUESTW         HttpSendRequestW;
        struct WORKREQ_SENDCALLBACK             SendCallback;
	struct WORKREQ_INTERNETOPENURLW         InternetOpenUrlW;
    } u;

    struct WORKREQ *next;
    struct WORKREQ *prev;

} WORKREQUEST, *LPWORKREQUEST;

HINTERNET WININET_AllocHandle( LPWININETHANDLEHEADER info );
LPWININETHANDLEHEADER WININET_GetObject( HINTERNET hinternet );
BOOL WININET_FreeHandle( HINTERNET hinternet );
HINTERNET WININET_FindHandle( LPWININETHANDLEHEADER info );

time_t ConvertTimeString(LPCWSTR asctime);

HINTERNET FTP_Connect(HINTERNET hInterent, LPCWSTR lpszServerName,
	INTERNET_PORT nServerPort, LPCWSTR lpszUserName,
	LPCWSTR lpszPassword, DWORD dwFlags, DWORD dwContext);

HINTERNET HTTP_Connect(HINTERNET hInterent, LPCWSTR lpszServerName,
	INTERNET_PORT nServerPort, LPCWSTR lpszUserName,
	LPCWSTR lpszPassword, DWORD dwFlags, DWORD dwContext);

BOOL GetAddress(LPCWSTR lpszServerName, INTERNET_PORT nServerPort,
	struct hostent **phe, struct sockaddr_in *psa);

void INTERNET_SetLastError(DWORD dwError);
DWORD INTERNET_GetLastError();
BOOL INTERNET_AsyncCall(LPWORKREQUEST lpWorkRequest);
LPSTR INTERNET_GetResponseBuffer();
LPSTR INTERNET_GetNextLine(INT nSocket, LPSTR lpszBuffer, LPDWORD dwBuffer);

BOOL FTP_CloseSessionHandle(LPWININETFTPSESSIONA lpwfs);
BOOL FTP_CloseFindNextHandle(LPWININETFINDNEXTA lpwfn);
BOOL FTP_CloseFileTransferHandle(LPWININETFILE lpwfn);
BOOLAPI FTP_FtpPutFileA(HINTERNET hConnect, LPCSTR lpszLocalFile,
    LPCSTR lpszNewRemoteFile, DWORD dwFlags, DWORD dwContext);
BOOLAPI FTP_FtpSetCurrentDirectoryA(HINTERNET hConnect, LPCSTR lpszDirectory);
BOOLAPI FTP_FtpCreateDirectoryA(HINTERNET hConnect, LPCSTR lpszDirectory);
INTERNETAPI HINTERNET WINAPI FTP_FtpFindFirstFileA(HINTERNET hConnect,
    LPCSTR lpszSearchFile, LPWIN32_FIND_DATAA lpFindFileData, DWORD dwFlags, DWORD dwContext);
BOOLAPI FTP_FtpGetCurrentDirectoryA(HINTERNET hFtpSession, LPSTR lpszCurrentDirectory,
	LPDWORD lpdwCurrentDirectory);
BOOL FTP_ConvertFileProp(LPFILEPROPERTIESA lpafp, LPWIN32_FIND_DATAA lpFindFileData);
BOOL FTP_FtpRenameFileA(HINTERNET hFtpSession, LPCSTR lpszSrc, LPCSTR lpszDest);
BOOL FTP_FtpRemoveDirectoryA(HINTERNET hFtpSession, LPCSTR lpszDirectory);
BOOL FTP_FtpDeleteFileA(HINTERNET hFtpSession, LPCSTR lpszFileName);
HINTERNET FTP_FtpOpenFileA(HINTERNET hFtpSession, LPCSTR lpszFileName,
	DWORD fdwAccess, DWORD dwFlags, DWORD dwContext);
BOOLAPI FTP_FtpGetFileA(HINTERNET hInternet, LPCSTR lpszRemoteFile, LPCSTR lpszNewFile,
	BOOL fFailIfExists, DWORD dwLocalFlagsAttribute, DWORD dwInternetFlags,
	DWORD dwContext);

BOOLAPI HTTP_HttpSendRequestW(HINTERNET hHttpRequest, LPCWSTR lpszHeaders,
	DWORD dwHeaderLength, LPVOID lpOptional ,DWORD dwOptionalLength);
INTERNETAPI HINTERNET WINAPI HTTP_HttpOpenRequestW(HINTERNET hHttpSession,
	LPCWSTR lpszVerb, LPCWSTR lpszObjectName, LPCWSTR lpszVersion,
	LPCWSTR lpszReferrer , LPCWSTR *lpszAcceptTypes,
	DWORD dwFlags, DWORD dwContext);
void HTTP_CloseHTTPSessionHandle(LPWININETHTTPSESSIONW lpwhs);
void HTTP_CloseHTTPRequestHandle(LPWININETHTTPREQW lpwhr);

VOID SendAsyncCallback(LPWININETAPPINFOW hIC, HINTERNET hHttpSession,
                             DWORD dwContext, DWORD dwInternetStatus, LPVOID
                             lpvStatusInfo , DWORD dwStatusInfoLength);

VOID SendAsyncCallbackInt(LPWININETAPPINFOW hIC, HINTERNET hHttpSession,
                             DWORD dwContext, DWORD dwInternetStatus, LPVOID
                             lpvStatusInfo , DWORD dwStatusInfoLength);

BOOL HTTP_InsertProxyAuthorization( LPWININETHTTPREQW lpwhr,
                       LPCWSTR username, LPCWSTR password );

BOOL NETCON_connected(WININET_NETCONNECTION *connection);
void NETCON_init(WININET_NETCONNECTION *connnection, BOOL useSSL);
BOOL NETCON_create(WININET_NETCONNECTION *connection, int domain,
	      int type, int protocol);
BOOL NETCON_close(WININET_NETCONNECTION *connection);
BOOL NETCON_connect(WININET_NETCONNECTION *connection, const struct sockaddr *serv_addr,
		    socklen_t addrlen);
BOOL NETCON_send(WININET_NETCONNECTION *connection, const void *msg, size_t len, int flags,
		int *sent /* out */);
BOOL NETCON_recv(WININET_NETCONNECTION *connection, void *buf, size_t len, int flags,
		int *recvd /* out */);
BOOL NETCON_getNextLine(WININET_NETCONNECTION *connection, LPSTR lpszBuffer, LPDWORD dwBuffer);

#define MAX_REPLY_LEN	 	0x5B4

/* Used for debugging - maybe need to be shared in the Wine debugging code ? */
typedef struct
{
    DWORD val;
    const char* name;
} wininet_flag_info;

#endif /* _WINE_INTERNET_H_ */
