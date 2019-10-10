/*
 * Wininet - Url Cache functions
 *
 * Copyright 2001,2002 CodeWeavers
 * Copyright 2003 Robert Shearman
 *
 * Eric Kohl
 * Aric Stewart
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

#define COM_NO_WINDOWS_H
#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "wininet.h"
#include "winerror.h"
#include "internet.h"
#include "winreg.h"
#include "shlwapi.h"
#include "wingdi.h"
#include "shlobj.h"

#include "wine/unicode.h"
#include "wine/list.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wininet);

#define ENTRY_START_OFFSET  0x4000
#define DIR_LENGTH          8
#define BLOCKSIZE           128
#define HASHTABLE_SIZE      448
#define HASHTABLE_BLOCKSIZE 7
#define HASHTABLE_FREE      3
#define ALLOCATION_TABLE_OFFSET 0x250
#define ALLOCATION_TABLE_SIZE   (0x1000 - ALLOCATION_TABLE_OFFSET)
#define HASHTABLE_NUM_ENTRIES   (HASHTABLE_SIZE / HASHTABLE_BLOCKSIZE)

#define DWORD_SIG(a,b,c,d)  (a | (b << 8) | (c << 16) | (d << 24))
#define URL_SIGNATURE   DWORD_SIG('U','R','L',' ')
#define REDR_SIGNATURE  DWORD_SIG('R','E','D','R')
#define LEAK_SIGNATURE  DWORD_SIG('L','E','A','K')
#define HASH_SIGNATURE  DWORD_SIG('H','A','S','H')

#define DWORD_ALIGN(x) ( (DWORD)(((DWORD)(x)+sizeof(DWORD)-1)/sizeof(DWORD))*sizeof(DWORD) )

typedef struct _CACHEFILE_ENTRY
{
/*  union
    {*/
        DWORD dwSignature; /* e.g. "URL " */
/*      CHAR szSignature[4];
    };*/
    DWORD dwBlocksUsed; /* number of 128byte blocks used by this entry */
} CACHEFILE_ENTRY;

typedef struct _URL_CACHEFILE_ENTRY
{
    CACHEFILE_ENTRY CacheFileEntry;
    FILETIME LastModifiedTime;
    FILETIME LastAccessTime;
    WORD wExpiredDate; /* expire date in dos format */
    WORD wExpiredTime; /* expire time in dos format */
    DWORD dwUnknown1; /* usually zero */
    DWORD dwSizeLow; /* see INTERNET_CACHE_ENTRY_INFO::dwSizeLow */
    DWORD dwSizeHigh; /* see INTERNET_CACHE_ENTRY_INFO::dwSizeHigh */
    DWORD dwUnknown2; /* usually zero */
    DWORD dwExemptDelta; /* see INTERNET_CACHE_ENTRY_INFO::dwExemptDelta */
    DWORD dwUnknown3; /* usually 0x60 */
    DWORD dwOffsetUrl; /* usually 0x68 */
    BYTE CacheDir; /* index of cache directory this url is stored in */
    BYTE Unknown4; /* usually zero */
    WORD wUnknown5; /* usually 0x1010 */
    DWORD dwOffsetLocalName; /* offset of start of local filename from start of entry */
    DWORD CacheEntryType; /* see INTERNET_CACHE_ENTRY_INFO::CacheEntryType */
    DWORD dwOffsetHeaderInfo; /* offset of start of header info from start of entry */
    DWORD dwHeaderInfoSize;
    DWORD dwUnknown6; /* usually zero */
    WORD wLastSyncDate; /* last sync date in dos format */
    WORD wLastSyncTime; /* last sync time in dos format */
    DWORD dwHitRate; /* see INTERNET_CACHE_ENTRY_INFO::dwHitRate */
    DWORD dwUseCount; /* see INTERNET_CACHE_ENTRY_INFO::dwUseCount */
    WORD wUnknownDate; /* usually same as wLastSyncDate */
    WORD wUnknownTime; /* usually same as wLastSyncTime */
    DWORD dwUnknown7; /* usually zero */
    DWORD dwUnknown8; /* usually zero */
    CHAR szSourceUrlName[1]; /* start of url */
    /* packing to dword align start of next field */
    /* CHAR szLocalFileName[]; (local file name exluding path) */
    /* packing to dword align start of next field */
    /* CHAR szHeaderInfo[]; (header info) */
} URL_CACHEFILE_ENTRY;

struct _HASH_ENTRY
{
    DWORD dwHashKey;
    DWORD dwOffsetEntry;
};

typedef struct _HASH_CACHEFILE_ENTRY
{
    CACHEFILE_ENTRY CacheFileEntry;
    DWORD dwAddressNext;
    DWORD dwHashTableNumber;
    struct _HASH_ENTRY HashTable[HASHTABLE_SIZE];
} HASH_CACHEFILE_ENTRY;

typedef struct _DIRECTORY_DATA
{
    DWORD dwUnknown;
    char filename[DIR_LENGTH];
} DIRECTORY_DATA;

typedef struct _URLCACHE_HEADER
{
    char szSignature[28];
    DWORD dwFileSize;
    DWORD dwOffsetFirstHashTable;
    DWORD dwIndexCapacityInBlocks;
    DWORD dwBlocksInUse; /* is this right? */
    DWORD dwUnknown1;
    DWORD dwCacheLimitLow; /* disk space limit for cache */
    DWORD dwCacheLimitHigh; /* disk space limit for cache */
    DWORD dwUnknown4; /* current disk space usage for cache? */
    DWORD dwUnknown5; /* current disk space usage for cache? */
    DWORD dwUnknown6; /* possibly a flag? */
    DWORD dwUnknown7;
    BYTE DirectoryCount; /* number of directory_data's */
    BYTE Unknown8[3]; /* just padding? */
    DIRECTORY_DATA directory_data[1]; /* first directory entry */
} URLCACHE_HEADER, *LPURLCACHE_HEADER;
typedef const URLCACHE_HEADER *LPCURLCACHE_HEADER;

typedef struct _STREAM_HANDLE
{
    HANDLE hFile;
    CHAR lpszUrl[1];
} STREAM_HANDLE;

typedef struct _URLCACHECONTAINER
{
    struct list entry; /* part of a list */
    LPWSTR cache_prefix; /* string that has to be prefixed for this container to be used */
    LPWSTR path; /* path to url container directory */
    HANDLE hMapping; /* handle of file mapping */
    DWORD file_size; /* size of file when mapping was opened */
    HANDLE hMutex; /* hande of mutex */
} URLCACHECONTAINER;


/* List of all containers available */
static struct list UrlContainers = LIST_INIT(UrlContainers);


/***********************************************************************
 *           URLCache_PathToObjectName (Internal)
 *
 *  Converts a path to a name suitable for use as a Win32 object name.
 * Replaces '\\' characters in-place with the specified character
 * (usually '_' or '!')
 *
 * RETURNS
 *    nothing
 *
 */
static void URLCache_PathToObjectName(LPWSTR lpszPath, WCHAR replace)
{
    for (; *lpszPath; lpszPath++)
    {
        if (*lpszPath == '\\')
            *lpszPath = replace;
    }
}

/***********************************************************************
 *           URLCacheContainer_OpenIndex (Internal)
 *
 *  Opens the index file and saves mapping handle in hCacheIndexMapping
 *
 * RETURNS
 *    TRUE if succeeded
 *    FALSE if failed
 *
 */
static BOOL URLCacheContainer_OpenIndex(URLCACHECONTAINER * pContainer)
{
    HANDLE hFile;
    WCHAR wszFilePath[MAX_PATH];
    DWORD dwFileSize;

    static const WCHAR wszIndex[] = {'i','n','d','e','x','.','d','a','t',0};
    static const WCHAR wszMappingFormat[] = {'%','s','%','s','_','%','l','u',0};

    if (pContainer->hMapping)
        return TRUE;

    strcpyW(wszFilePath, pContainer->path);
    strcatW(wszFilePath, wszIndex);

    hFile = CreateFileW(wszFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        FIXME("need to create cache index file\n");
        return FALSE;
    }

    dwFileSize = GetFileSize(hFile, NULL);
    if (dwFileSize == INVALID_FILE_SIZE)
        return FALSE;

    if (dwFileSize == 0)
    {
        FIXME("need to create cache index file\n");
        return FALSE;
    }

    wsprintfW(wszFilePath, wszMappingFormat, pContainer->path, wszIndex, dwFileSize);
    URLCache_PathToObjectName(wszFilePath, '_');
    pContainer->hMapping = OpenFileMappingW(FILE_MAP_WRITE, FALSE, wszFilePath);
    if (!pContainer->hMapping)
        pContainer->hMapping = CreateFileMappingW(hFile, NULL, PAGE_READWRITE, 0, 0, wszFilePath);
    CloseHandle(hFile);
    if (!pContainer->hMapping)
    {
        ERR("Couldn't create file mapping (error is %ld)\n", GetLastError());
        return FALSE;
    }

    return TRUE;
}

/***********************************************************************
 *           URLCacheContainer_CloseIndex (Internal)
 *
 *  Closes the index
 *
 * RETURNS
 *    nothing
 *
 */
static void URLCacheContainer_CloseIndex(URLCACHECONTAINER * pContainer)
{
    CloseHandle(pContainer->hMapping);
    pContainer->hMapping = NULL;
}

static BOOL URLCacheContainers_AddContainer(LPCWSTR cache_prefix, LPCWSTR path, LPWSTR mutex_name)
{
    URLCACHECONTAINER * pContainer = HeapAlloc(GetProcessHeap(), 0, sizeof(URLCACHECONTAINER));
    int path_len = strlenW(path);
    int cache_prefix_len = strlenW(cache_prefix);

    if (!pContainer)
    {
        return FALSE;
    }

    pContainer->hMapping = NULL;
    pContainer->file_size = 0;

    pContainer->path = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, (path_len + 1) * sizeof(WCHAR));
    if (!pContainer->path)
    {
        HeapFree(GetProcessHeap(), 0, pContainer);
        return FALSE;
    }

    memcpy(pContainer->path, path, (path_len + 1) * sizeof(WCHAR));

    pContainer->cache_prefix = HeapAlloc(GetProcessHeap(), 0, (cache_prefix_len + 1) * sizeof(WCHAR));
    if (!pContainer->cache_prefix)
    {
        HeapFree(GetProcessHeap(), 0, pContainer->path);
        HeapFree(GetProcessHeap(), 0, pContainer);
        return FALSE;
    }

    memcpy(pContainer->cache_prefix, cache_prefix, (cache_prefix_len + 1) * sizeof(WCHAR));

    CharLowerW(mutex_name);
    URLCache_PathToObjectName(mutex_name, '!');

    if ((pContainer->hMutex = CreateMutexW(NULL, FALSE, mutex_name)) == NULL)
    {
        ERR("couldn't create mutex (error is %ld)\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, pContainer->path);
        HeapFree(GetProcessHeap(), 0, pContainer);
        return FALSE;
    }

    list_add_head(&UrlContainers, &pContainer->entry);

    return TRUE;
}

static void URLCacheContainer_DeleteContainer(URLCACHECONTAINER * pContainer)
{
    list_remove(&pContainer->entry);

    URLCacheContainer_CloseIndex(pContainer);
    CloseHandle(pContainer->hMutex);
    HeapFree(GetProcessHeap(), 0, pContainer->path);
    HeapFree(GetProcessHeap(), 0, pContainer->cache_prefix);
    HeapFree(GetProcessHeap(), 0, pContainer);
}

void URLCacheContainers_CreateDefaults()
{
    static const WCHAR UrlSuffix[] = {'C','o','n','t','e','n','t','.','I','E','5',0};
    static const WCHAR UrlPrefix[] = {0};
    static const WCHAR HistorySuffix[] = {'H','i','s','t','o','r','y','.','I','E','5',0};
    static const WCHAR HistoryPrefix[] = {'V','i','s','i','t','e','d',':',0};
    static const WCHAR CookieSuffix[] = {0};
    static const WCHAR CookiePrefix[] = {'C','o','o','k','i','e',':',0};
    static const struct
    {
        int nFolder; /* CSIDL_* constant */
        const WCHAR * shpath_suffix; /* suffix on path returned by SHGetSpecialFolderPath */
        const WCHAR * cache_prefix; /* prefix used to reference the container */
    } DefaultContainerData[] = 
    {
        { CSIDL_INTERNET_CACHE, UrlSuffix, UrlPrefix },
        { CSIDL_HISTORY, HistorySuffix, HistoryPrefix },
        { CSIDL_COOKIES, CookieSuffix, CookiePrefix },
    };
    int i;

    for (i = 0; i < sizeof(DefaultContainerData) / sizeof(DefaultContainerData[0]); i++)
    {
        WCHAR wszCachePath[MAX_PATH];
        WCHAR wszMutexName[MAX_PATH];
        int path_len, suffix_len;

        if (FAILED(SHGetSpecialFolderPathW(NULL, wszCachePath, DefaultContainerData[i].nFolder, TRUE)))
        {
            ERR("Couldn't get path for default container %d\n", i);
            continue;
        }
        path_len = strlenW(wszCachePath);
        suffix_len = strlenW(DefaultContainerData[i].shpath_suffix);

        if (path_len + suffix_len + 2 > MAX_PATH)
        {
            ERR("Path too long\n");
            continue;
        }

        wszCachePath[path_len] = '\\';

        strcpyW(wszMutexName, wszCachePath);
        
        if (suffix_len)
        {
            memcpy(wszCachePath + path_len + 1, DefaultContainerData[i].shpath_suffix, (suffix_len + 1) * sizeof(WCHAR));
            wszCachePath[path_len + suffix_len + 1] = '\\';
            wszCachePath[path_len + suffix_len + 2] = '\0';
        }

        URLCacheContainers_AddContainer(DefaultContainerData[i].cache_prefix, wszCachePath, wszMutexName);
    }
}

void URLCacheContainers_DeleteAll()
{
    while(!list_empty(&UrlContainers))
        URLCacheContainer_DeleteContainer(
            LIST_ENTRY(list_head(&UrlContainers), URLCACHECONTAINER, entry)
        );
}

static BOOL URLCacheContainers_FindContainerW(LPCWSTR lpwszUrl, URLCACHECONTAINER ** ppContainer)
{
    struct list * cursor;

    TRACE("searching for prefix for URL: %s\n", debugstr_w(lpwszUrl));

    LIST_FOR_EACH(cursor, &UrlContainers)
    {
        URLCACHECONTAINER * pContainer = LIST_ENTRY(cursor, URLCACHECONTAINER, entry);
        int prefix_len = strlenW(pContainer->cache_prefix);
        if (!strncmpW(pContainer->cache_prefix, lpwszUrl, prefix_len))
        {
            TRACE("found container with prefx %s for URL %s\n", debugstr_w(pContainer->cache_prefix), debugstr_w(lpwszUrl));
            *ppContainer = pContainer;
            return TRUE;
        }
    }
    ERR("no container found\n");
    SetLastError(ERROR_FILE_NOT_FOUND);
    return FALSE;
}

static BOOL URLCacheContainers_FindContainerA(LPCSTR lpszUrl, URLCACHECONTAINER ** ppContainer)
{
    BOOL ret;
    LPWSTR lpwszUrl;
    int url_len = MultiByteToWideChar(CP_ACP, 0, lpszUrl, -1, NULL, 0);
    if (url_len && (lpwszUrl = HeapAlloc(GetProcessHeap(), 0, url_len * sizeof(WCHAR))))
    {
        MultiByteToWideChar(CP_ACP, 0, lpszUrl, -1, lpwszUrl, url_len);
        ret = URLCacheContainers_FindContainerW(lpwszUrl, ppContainer);
        HeapFree(GetProcessHeap(), 0, lpwszUrl);
        return ret;
    }
    return FALSE;
}

/***********************************************************************
 *           URLCacheContainer_LockIndex (Internal)
 *
 */
static LPURLCACHE_HEADER URLCacheContainer_LockIndex(URLCACHECONTAINER * pContainer)
{
    BYTE index;
    LPVOID pIndexData;
    URLCACHE_HEADER * pHeader;

    /* acquire mutex */
    WaitForSingleObject(pContainer->hMutex, INFINITE);

    pIndexData = MapViewOfFile(pContainer->hMapping, FILE_MAP_WRITE, 0, 0, 0);

    if (!pIndexData)
    {
        ReleaseMutex(pContainer->hMutex);
        ERR("Couldn't MapViewOfFile. Error: %ld\n", GetLastError());
        return FALSE;
    }
    pHeader = (URLCACHE_HEADER *)pIndexData;

    /* file has grown - we need to remap to prevent us getting
     * access violations when we try and access beyond the end
     * of the memory mapped file */
    if (pHeader->dwFileSize != pContainer->file_size)
    {
        URLCacheContainer_CloseIndex(pContainer);
        if (!URLCacheContainer_OpenIndex(pContainer))
        {
            ReleaseMutex(pContainer->hMutex);
            return FALSE;
        }
        pIndexData = MapViewOfFile(pContainer->hMapping, FILE_MAP_WRITE, 0, 0, 0);

        if (!pIndexData)
        {
            ReleaseMutex(pContainer->hMutex);
            ERR("Couldn't MapViewOfFile. Error: %ld\n", GetLastError());
            return FALSE;
        }
        pHeader = (URLCACHE_HEADER *)pIndexData;
    }

    TRACE("Signature: %s, file size: %ld bytes\n", pHeader->szSignature, pHeader->dwFileSize);

    for (index = 0; index < pHeader->DirectoryCount; index++)
    {
        TRACE("Directory[%d] = \"%.8s\"\n", index, pHeader->directory_data[index].filename);
    }
    
    return pHeader;
}

/***********************************************************************
 *           URLCacheContainer_UnlockIndex (Internal)
 *
 */
static BOOL URLCacheContainer_UnlockIndex(URLCACHECONTAINER * pContainer, LPURLCACHE_HEADER pHeader)
{
    /* release mutex */
    ReleaseMutex(pContainer->hMutex);
    return UnmapViewOfFile(pHeader);
}


#ifndef CHAR_BIT
#define CHAR_BIT    (8 * sizeof(CHAR))
#endif

/***********************************************************************
 *           URLCache_Allocation_BlockIsFree (Internal)
 *
 *  Is the specified block number free?
 *
 * RETURNS
 *    zero if free
 *    non-zero otherwise
 *
 */
static inline BYTE URLCache_Allocation_BlockIsFree(BYTE * AllocationTable, DWORD dwBlockNumber)
{
    BYTE mask = 1 << (dwBlockNumber % CHAR_BIT);
    return (AllocationTable[dwBlockNumber / CHAR_BIT] & mask) == 0;
}

/***********************************************************************
 *           URLCache_Allocation_BlockFree (Internal)
 *
 *  Marks the specified block as free
 *
 * RETURNS
 *    nothing
 *
 */
static inline void URLCache_Allocation_BlockFree(BYTE * AllocationTable, DWORD dwBlockNumber)
{
    BYTE mask = ~(1 << (dwBlockNumber % CHAR_BIT));
    AllocationTable[dwBlockNumber / CHAR_BIT] &= mask;
}

/***********************************************************************
 *           URLCache_Allocation_BlockAllocate (Internal)
 *
 *  Marks the specified block as allocated
 *
 * RETURNS
 *    nothing
 *
 */
static inline void URLCache_Allocation_BlockAllocate(BYTE * AllocationTable, DWORD dwBlockNumber)
{
    BYTE mask = 1 << (dwBlockNumber % CHAR_BIT);
    AllocationTable[dwBlockNumber / CHAR_BIT] |= mask;
}

/***********************************************************************
 *           URLCache_FindFirstFreeEntry (Internal)
 *
 *  Finds and allocates the first block of free space big enough and
 * sets ppEntry to point to it.
 *
 * RETURNS
 *    TRUE if it had enough space
 *    FALSE if it couldn't find enough space
 *
 */
static BOOL URLCache_FindFirstFreeEntry(URLCACHE_HEADER * pHeader, DWORD dwBlocksNeeded, CACHEFILE_ENTRY ** ppEntry)
{
    LPBYTE AllocationTable = (LPBYTE)pHeader + ALLOCATION_TABLE_OFFSET;
    DWORD dwBlockNumber;
    DWORD dwFreeCounter;
    for (dwBlockNumber = 0; dwBlockNumber < pHeader->dwIndexCapacityInBlocks; dwBlockNumber++)
    {
        for (dwFreeCounter = 0; 
            dwFreeCounter < dwBlocksNeeded &&
              dwFreeCounter + dwBlockNumber < pHeader->dwIndexCapacityInBlocks &&
              URLCache_Allocation_BlockIsFree(AllocationTable, dwBlockNumber + dwFreeCounter);
            dwFreeCounter++)
                TRACE("Found free block at no. %ld (0x%lx)\n", dwBlockNumber, ENTRY_START_OFFSET + dwBlockNumber * BLOCKSIZE);

        if (dwFreeCounter == dwBlocksNeeded)
        {
            DWORD index;
            TRACE("Found free blocks starting at no. %ld (0x%lx)\n", dwBlockNumber, ENTRY_START_OFFSET + dwBlockNumber * BLOCKSIZE);
            for (index = 0; index < dwBlocksNeeded; index++)
                URLCache_Allocation_BlockAllocate(AllocationTable, dwBlockNumber + index);
            *ppEntry = (CACHEFILE_ENTRY *)((LPBYTE)pHeader + ENTRY_START_OFFSET + dwBlockNumber * BLOCKSIZE);
            (*ppEntry)->dwBlocksUsed = dwBlocksNeeded;
            return TRUE;
        }
    }
    FIXME("Grow file\n");
    return FALSE;
}

/***********************************************************************
 *           URLCache_DeleteEntry (Internal)
 *
 *  Deletes the specified entry and frees the space allocated to it
 *
 * RETURNS
 *    TRUE if it succeeded
 *    FALSE if it failed
 *
 */
static BOOL URLCache_DeleteEntry(CACHEFILE_ENTRY * pEntry)
{
    ZeroMemory(pEntry, pEntry->dwBlocksUsed * BLOCKSIZE);
    return TRUE;
}

/***********************************************************************
 *           URLCache_LocalFileNameToPathW (Internal)
 *
 *  Copies the full path to the specified buffer given the local file
 * name and the index of the directory it is in. Always sets value in
 * lpBufferSize to the required buffer size (in bytes).
 *
 * RETURNS
 *    TRUE if the buffer was big enough
 *    FALSE if the buffer was too small
 *
 */
static BOOL URLCache_LocalFileNameToPathW(
    const URLCACHECONTAINER * pContainer,
    LPCURLCACHE_HEADER pHeader,
    LPCSTR szLocalFileName,
    BYTE Directory,
    LPWSTR wszPath,
    LPLONG lpBufferSize)
{
    LONG nRequired;
    int path_len = strlenW(pContainer->path);
    int file_name_len = MultiByteToWideChar(CP_ACP, 0, szLocalFileName, -1, NULL, 0);
    if (Directory >= pHeader->DirectoryCount)
    {
        *lpBufferSize = 0;
        return FALSE;
    }

    nRequired = (path_len + DIR_LENGTH + file_name_len + 1) * sizeof(WCHAR);
    if (nRequired < *lpBufferSize)
    {
        int dir_len;

        memcpy(wszPath, pContainer->path, path_len * sizeof(WCHAR));
        dir_len = MultiByteToWideChar(CP_ACP, 0, pHeader->directory_data[Directory].filename, DIR_LENGTH, wszPath + path_len, DIR_LENGTH);
        wszPath[dir_len + path_len] = '\\';
        MultiByteToWideChar(CP_ACP, 0, szLocalFileName, -1, wszPath + dir_len + path_len + 1, file_name_len);
        *lpBufferSize = nRequired;
        return TRUE;
    }
    *lpBufferSize = nRequired;
    return FALSE;
}

/***********************************************************************
 *           URLCache_LocalFileNameToPathA (Internal)
 *
 *  Copies the full path to the specified buffer given the local file
 * name and the index of the directory it is in. Always sets value in
 * lpBufferSize to the required buffer size.
 *
 * RETURNS
 *    TRUE if the buffer was big enough
 *    FALSE if the buffer was too small
 *
 */
static BOOL URLCache_LocalFileNameToPathA(
    const URLCACHECONTAINER * pContainer,
    LPCURLCACHE_HEADER pHeader,
    LPCSTR szLocalFileName,
    BYTE Directory,
    LPSTR szPath,
    LPLONG lpBufferSize)
{
    LONG nRequired;
    int path_len = WideCharToMultiByte(CP_ACP, 0, pContainer->path, -1, NULL, 0, NULL, NULL);
    int file_name_len = strlen(szLocalFileName);
    int dir_len = DIR_LENGTH;
    if (Directory >= pHeader->DirectoryCount)
    {
        *lpBufferSize = 0;
        return FALSE;
    }

    nRequired = (path_len + dir_len + file_name_len + 1) * sizeof(WCHAR);
    if (nRequired < *lpBufferSize)
    {
        WideCharToMultiByte(CP_ACP, 0, pContainer->path, -1, szPath, -1, NULL, NULL);
        strncpy(szPath, pHeader->directory_data[Directory].filename, DIR_LENGTH);
        szPath[dir_len + path_len] = '\\';
        strcpy(szPath + dir_len + path_len + 1, szLocalFileName);
        *lpBufferSize = nRequired;
        return TRUE;
    }
    *lpBufferSize = nRequired;
    return FALSE;
}

/***********************************************************************
 *           URLCache_CopyEntry (Internal)
 *
 *  Copies an entry from the cache index file to the Win32 structure
 *
 * RETURNS
 *    TRUE if the buffer was big enough
 *    FALSE if the buffer was too small
 *
 */
static BOOL URLCache_CopyEntry(
    URLCACHECONTAINER * pContainer,
    LPCURLCACHE_HEADER pHeader, 
    LPINTERNET_CACHE_ENTRY_INFOA lpCacheEntryInfo, 
    LPDWORD lpdwBufferSize, 
    URL_CACHEFILE_ENTRY * pUrlEntry,
    BOOL bUnicode)
{
    int lenUrl;
    DWORD dwRequiredSize = sizeof(*lpCacheEntryInfo);

    if (*lpdwBufferSize >= dwRequiredSize)
    {
        lpCacheEntryInfo->lpHeaderInfo = NULL;
        lpCacheEntryInfo->lpszFileExtension = NULL;
        lpCacheEntryInfo->lpszLocalFileName = NULL;
        lpCacheEntryInfo->lpszSourceUrlName = NULL;
        lpCacheEntryInfo->CacheEntryType = pUrlEntry->CacheEntryType;
        lpCacheEntryInfo->u.dwExemptDelta = pUrlEntry->dwExemptDelta;
        lpCacheEntryInfo->dwHeaderInfoSize = pUrlEntry->dwHeaderInfoSize;
        lpCacheEntryInfo->dwHitRate = pUrlEntry->dwHitRate;
        lpCacheEntryInfo->dwSizeHigh = pUrlEntry->dwSizeHigh;
        lpCacheEntryInfo->dwSizeLow = pUrlEntry->dwSizeLow;
        lpCacheEntryInfo->dwStructSize = sizeof(*lpCacheEntryInfo);
        lpCacheEntryInfo->dwUseCount = pUrlEntry->dwUseCount;
        DosDateTimeToFileTime(pUrlEntry->wExpiredDate, pUrlEntry->wExpiredTime, &lpCacheEntryInfo->ExpireTime);
        lpCacheEntryInfo->LastAccessTime.dwHighDateTime = pUrlEntry->LastAccessTime.dwHighDateTime;
        lpCacheEntryInfo->LastAccessTime.dwLowDateTime = pUrlEntry->LastAccessTime.dwLowDateTime;
        lpCacheEntryInfo->LastModifiedTime.dwHighDateTime = pUrlEntry->LastModifiedTime.dwHighDateTime;
        lpCacheEntryInfo->LastModifiedTime.dwLowDateTime = pUrlEntry->LastModifiedTime.dwLowDateTime;
        DosDateTimeToFileTime(pUrlEntry->wLastSyncDate, pUrlEntry->wLastSyncTime, &lpCacheEntryInfo->LastSyncTime);
    }

    if ((dwRequiredSize % 4) && (dwRequiredSize < *lpdwBufferSize))
        ZeroMemory((LPBYTE)lpCacheEntryInfo + dwRequiredSize, 4 - (dwRequiredSize % 4));
    dwRequiredSize = DWORD_ALIGN(dwRequiredSize);
    if (bUnicode)
        lenUrl = MultiByteToWideChar(CP_ACP, 0, pUrlEntry->szSourceUrlName, -1, NULL, 0);
    else
        lenUrl = strlen(pUrlEntry->szSourceUrlName);
    dwRequiredSize += lenUrl + 1;
    
    /* FIXME: is source url optional? */
    if (*lpdwBufferSize >= dwRequiredSize)
    {
        lpCacheEntryInfo->lpszSourceUrlName = (LPSTR)lpCacheEntryInfo + dwRequiredSize - lenUrl - 1;
        if (bUnicode)
            MultiByteToWideChar(CP_ACP, 0, pUrlEntry->szSourceUrlName, -1, (LPWSTR)lpCacheEntryInfo->lpszSourceUrlName, lenUrl + 1);
        else
            memcpy(lpCacheEntryInfo->lpszSourceUrlName, pUrlEntry->szSourceUrlName, (lenUrl + 1) * sizeof(CHAR));
    }

    if ((dwRequiredSize % 4) && (dwRequiredSize < *lpdwBufferSize))
        ZeroMemory((LPBYTE)lpCacheEntryInfo + dwRequiredSize, 4 - (dwRequiredSize % 4));
    dwRequiredSize = DWORD_ALIGN(dwRequiredSize);

    if (pUrlEntry->dwOffsetLocalName)
    {
        LONG nLocalFilePathSize;
        LPSTR lpszLocalFileName;
        lpszLocalFileName = (LPSTR)lpCacheEntryInfo + dwRequiredSize;
        nLocalFilePathSize = *lpdwBufferSize - dwRequiredSize;
        if ((bUnicode && URLCache_LocalFileNameToPathW(pContainer, pHeader, (LPCSTR)pUrlEntry + pUrlEntry->dwOffsetLocalName, pUrlEntry->CacheDir, (LPWSTR)lpszLocalFileName, &nLocalFilePathSize)) ||
            URLCache_LocalFileNameToPathA(pContainer, pHeader, (LPCSTR)pUrlEntry + pUrlEntry->dwOffsetLocalName, pUrlEntry->CacheDir, lpszLocalFileName, &nLocalFilePathSize))
        {
            lpCacheEntryInfo->lpszLocalFileName = lpszLocalFileName;
        }
        dwRequiredSize += nLocalFilePathSize;

        if ((dwRequiredSize % 4) && (dwRequiredSize < *lpdwBufferSize))
            ZeroMemory((LPBYTE)lpCacheEntryInfo + dwRequiredSize, 4 - (dwRequiredSize % 4));
        dwRequiredSize = DWORD_ALIGN(dwRequiredSize);
    }
    dwRequiredSize += pUrlEntry->dwHeaderInfoSize + 1;

    if (*lpdwBufferSize >= dwRequiredSize)
    {
        lpCacheEntryInfo->lpHeaderInfo = (LPSTR)lpCacheEntryInfo + dwRequiredSize - pUrlEntry->dwHeaderInfoSize - 1;
        memcpy(lpCacheEntryInfo->lpHeaderInfo, (LPSTR)pUrlEntry + pUrlEntry->dwOffsetHeaderInfo, pUrlEntry->dwHeaderInfoSize);
        ((LPBYTE)lpCacheEntryInfo)[dwRequiredSize - 1] = '\0';
    }
    if ((dwRequiredSize % 4) && (dwRequiredSize < *lpdwBufferSize))
        ZeroMemory((LPBYTE)lpCacheEntryInfo + dwRequiredSize, 4 - (dwRequiredSize % 4));
    dwRequiredSize = DWORD_ALIGN(dwRequiredSize);

    if (dwRequiredSize > *lpdwBufferSize)
    {
        *lpdwBufferSize = dwRequiredSize;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    *lpdwBufferSize = dwRequiredSize;
    return TRUE;
}


/***********************************************************************
 *           URLCache_SetEntryInfo (Internal)
 *
 *  Helper for SetUrlCacheEntryInfo{A,W}. Sets fields in URL entry
 * according the the flags set by dwFieldControl.
 *
 * RETURNS
 *    TRUE if the buffer was big enough
 *    FALSE if the buffer was too small
 *
 */
static BOOL URLCache_SetEntryInfo(URL_CACHEFILE_ENTRY * pUrlEntry, const INTERNET_CACHE_ENTRY_INFOW * lpCacheEntryInfo, DWORD dwFieldControl)
{
    if (dwFieldControl & CACHE_ENTRY_ACCTIME_FC)
        pUrlEntry->LastAccessTime = lpCacheEntryInfo->LastAccessTime;
    if (dwFieldControl & CACHE_ENTRY_ATTRIBUTE_FC)
        pUrlEntry->CacheEntryType = lpCacheEntryInfo->CacheEntryType;
    if (dwFieldControl & CACHE_ENTRY_EXEMPT_DELTA_FC)
        pUrlEntry->dwExemptDelta = lpCacheEntryInfo->u.dwExemptDelta;
    if (dwFieldControl & CACHE_ENTRY_EXPTIME_FC)
        FIXME("CACHE_ENTRY_EXPTIME_FC unimplemented\n");
    if (dwFieldControl & CACHE_ENTRY_HEADERINFO_FC)
        FIXME("CACHE_ENTRY_HEADERINFO_FC unimplemented\n");
    if (dwFieldControl & CACHE_ENTRY_HITRATE_FC)
        pUrlEntry->dwHitRate = lpCacheEntryInfo->dwHitRate;
    if (dwFieldControl & CACHE_ENTRY_MODTIME_FC)
        pUrlEntry->LastModifiedTime = lpCacheEntryInfo->LastModifiedTime;
    if (dwFieldControl & CACHE_ENTRY_SYNCTIME_FC)
        FileTimeToDosDateTime(&lpCacheEntryInfo->LastAccessTime, &pUrlEntry->wLastSyncDate, &pUrlEntry->wLastSyncTime);

    return TRUE;
}

/***********************************************************************
 *           URLCache_HashKey (Internal)
 *
 *  Returns the hash key for a given string
 *
 * RETURNS
 *    hash key for the string
 *
 */
static DWORD URLCache_HashKey(LPCSTR lpszKey)
{
    /* NOTE: this uses the same lookup table as SHLWAPI.UrlHash{A,W}
     * but the algorithm and result are not the same!
     */
    static const unsigned char lookupTable[256] = 
    {
        0x01, 0x0E, 0x6E, 0x19, 0x61, 0xAE, 0x84, 0x77,
        0x8A, 0xAA, 0x7D, 0x76, 0x1B, 0xE9, 0x8C, 0x33,
        0x57, 0xC5, 0xB1, 0x6B, 0xEA, 0xA9, 0x38, 0x44,
        0x1E, 0x07, 0xAD, 0x49, 0xBC, 0x28, 0x24, 0x41,
        0x31, 0xD5, 0x68, 0xBE, 0x39, 0xD3, 0x94, 0xDF,
        0x30, 0x73, 0x0F, 0x02, 0x43, 0xBA, 0xD2, 0x1C,
        0x0C, 0xB5, 0x67, 0x46, 0x16, 0x3A, 0x4B, 0x4E,
        0xB7, 0xA7, 0xEE, 0x9D, 0x7C, 0x93, 0xAC, 0x90,
        0xB0, 0xA1, 0x8D, 0x56, 0x3C, 0x42, 0x80, 0x53,
        0x9C, 0xF1, 0x4F, 0x2E, 0xA8, 0xC6, 0x29, 0xFE,
        0xB2, 0x55, 0xFD, 0xED, 0xFA, 0x9A, 0x85, 0x58,
        0x23, 0xCE, 0x5F, 0x74, 0xFC, 0xC0, 0x36, 0xDD,
        0x66, 0xDA, 0xFF, 0xF0, 0x52, 0x6A, 0x9E, 0xC9,
        0x3D, 0x03, 0x59, 0x09, 0x2A, 0x9B, 0x9F, 0x5D,
        0xA6, 0x50, 0x32, 0x22, 0xAF, 0xC3, 0x64, 0x63,
        0x1A, 0x96, 0x10, 0x91, 0x04, 0x21, 0x08, 0xBD,
        0x79, 0x40, 0x4D, 0x48, 0xD0, 0xF5, 0x82, 0x7A,
        0x8F, 0x37, 0x69, 0x86, 0x1D, 0xA4, 0xB9, 0xC2,
        0xC1, 0xEF, 0x65, 0xF2, 0x05, 0xAB, 0x7E, 0x0B,
        0x4A, 0x3B, 0x89, 0xE4, 0x6C, 0xBF, 0xE8, 0x8B,
        0x06, 0x18, 0x51, 0x14, 0x7F, 0x11, 0x5B, 0x5C,
        0xFB, 0x97, 0xE1, 0xCF, 0x15, 0x62, 0x71, 0x70,
        0x54, 0xE2, 0x12, 0xD6, 0xC7, 0xBB, 0x0D, 0x20,
        0x5E, 0xDC, 0xE0, 0xD4, 0xF7, 0xCC, 0xC4, 0x2B,
        0xF9, 0xEC, 0x2D, 0xF4, 0x6F, 0xB6, 0x99, 0x88,
        0x81, 0x5A, 0xD9, 0xCA, 0x13, 0xA5, 0xE7, 0x47,
        0xE6, 0x8E, 0x60, 0xE3, 0x3E, 0xB3, 0xF6, 0x72,
        0xA2, 0x35, 0xA0, 0xD7, 0xCD, 0xB4, 0x2F, 0x6D,
        0x2C, 0x26, 0x1F, 0x95, 0x87, 0x00, 0xD8, 0x34,
        0x3F, 0x17, 0x25, 0x45, 0x27, 0x75, 0x92, 0xB8,
        0xA3, 0xC8, 0xDE, 0xEB, 0xF8, 0xF3, 0xDB, 0x0A,
        0x98, 0x83, 0x7B, 0xE5, 0xCB, 0x4C, 0x78, 0xD1
    };
    BYTE key[4];
    int i;
    int subscript[sizeof(key) / sizeof(key[0])];

    subscript[0] = *lpszKey;
    subscript[1] = (char)(*lpszKey + 1);
    subscript[2] = (char)(*lpszKey + 2);
    subscript[3] = (char)(*lpszKey + 3);

    for (i = 0; i < sizeof(key) / sizeof(key[0]); i++)
        key[i] = lookupTable[i];

    for (lpszKey++; *lpszKey && ((lpszKey[0] != '/') || (lpszKey[1] != 0)); lpszKey++)
    {
        for (i = 0; i < sizeof(key) / sizeof(key[0]); i++)
            key[i] = lookupTable[*lpszKey ^ key[i]];
    }

    return *(DWORD *)key;
}

static inline HASH_CACHEFILE_ENTRY * URLCache_HashEntryFromOffset(LPCURLCACHE_HEADER pHeader, DWORD dwOffset)
{
    return (HASH_CACHEFILE_ENTRY *)((LPBYTE)pHeader + dwOffset);
}

static BOOL URLCache_FindHash(LPCURLCACHE_HEADER pHeader, LPCSTR lpszUrl, struct _HASH_ENTRY ** ppHashEntry)
{
    /* structure of hash table:
     *  448 entries divided into 64 blocks
     *  each block therefore contains a chain of 7 key/offset pairs
     * how position in table is calculated:
     *  1. the url is hashed in helper function
     *  2. the key % 64 * 8 is the offset
     *  3. the key in the hash table is the hash key aligned to 64
     *
     * note:
     *  there can be multiple hash tables in the file and the offset to
     *  the next one is stored in the header of the hash table
     */
    DWORD key = URLCache_HashKey(lpszUrl);
    DWORD offset = (key % HASHTABLE_NUM_ENTRIES) * sizeof(struct _HASH_ENTRY);
    HASH_CACHEFILE_ENTRY * pHashEntry;
    DWORD dwHashTableNumber = 0;

    key = (DWORD)(key / HASHTABLE_NUM_ENTRIES) * HASHTABLE_NUM_ENTRIES;

    for (pHashEntry = URLCache_HashEntryFromOffset(pHeader, pHeader->dwOffsetFirstHashTable);
         ((DWORD)((LPBYTE)pHashEntry - (LPBYTE)pHeader) >= ENTRY_START_OFFSET) && ((DWORD)((LPBYTE)pHashEntry - (LPBYTE)pHeader) < pHeader->dwFileSize);
         pHashEntry = URLCache_HashEntryFromOffset(pHeader, pHashEntry->dwAddressNext))
    {
        int i;
        if (pHashEntry->dwHashTableNumber != dwHashTableNumber++)
        {
            ERR("Error: not right hash table number (%ld) expected %ld\n", pHashEntry->dwHashTableNumber, dwHashTableNumber);
            continue;
        }
        /* make sure that it is in fact a hash entry */
        if (pHashEntry->CacheFileEntry.dwSignature != HASH_SIGNATURE)
        {
            ERR("Error: not right signature (\"%.4s\") - expected \"HASH\"\n", (LPCSTR)&pHashEntry->CacheFileEntry.dwSignature);
            continue;
        }

        for (i = 0; i < HASHTABLE_BLOCKSIZE; i++)
        {
            struct _HASH_ENTRY * pHashElement = &pHashEntry->HashTable[offset + i];
            if (key == (DWORD)(pHashElement->dwHashKey / HASHTABLE_NUM_ENTRIES) * HASHTABLE_NUM_ENTRIES)
            {
                /* FIXME: we should make sure that this is the right element
                 * before returning and claiming that it is. We can do this
                 * by doing a simple compare between the URL we were given
                 * and the URL stored in the entry. However, this assumes
                 * we know the format of all the entries stored in the
                 * hash table */
                *ppHashEntry = pHashElement;
                return TRUE;
            }
        }
    }
    return FALSE;
}

/***********************************************************************
 *           URLCache_FindEntryInHash (Internal)
 *
 *  Searches all the hash tables in the index for the given URL and
 * returns the entry, if it was found, in ppEntry
 *
 * RETURNS
 *    TRUE if the entry was found
 *    FALSE if the entry could not be found
 *
 */
static BOOL URLCache_FindEntryInHash(LPCURLCACHE_HEADER pHeader, LPCSTR lpszUrl, CACHEFILE_ENTRY ** ppEntry)
{
    struct _HASH_ENTRY * pHashEntry;
    if (URLCache_FindHash(pHeader, lpszUrl, &pHashEntry))
    {
        *ppEntry = (CACHEFILE_ENTRY *)((LPBYTE)pHeader + pHashEntry->dwOffsetEntry);
        return TRUE;
    }
    return FALSE;
}

/***********************************************************************
 *           URLCache_HashEntrySetUse (Internal)
 *
 *  Searches all the hash tables in the index for the given URL and
 * sets the use count (stored or'ed with key)
 *
 * RETURNS
 *    TRUE if the entry was found
 *    FALSE if the entry could not be found
 *
 */
static BOOL URLCache_HashEntrySetUse(LPCURLCACHE_HEADER pHeader, LPCSTR lpszUrl, DWORD dwUseCount)
{
    struct _HASH_ENTRY * pHashEntry;
    if (URLCache_FindHash(pHeader, lpszUrl, &pHashEntry))
    {
        pHashEntry->dwHashKey = dwUseCount | (DWORD)(pHashEntry->dwHashKey / HASHTABLE_NUM_ENTRIES) * HASHTABLE_NUM_ENTRIES;
        return TRUE;
    }
    return FALSE;
}

/***********************************************************************
 *           URLCache_DeleteEntryFromHash (Internal)
 *
 *  Searches all the hash tables in the index for the given URL and
 * then if found deletes the entry.
 *
 * RETURNS
 *    TRUE if the entry was found
 *    FALSE if the entry could not be found
 *
 */
static BOOL URLCache_DeleteEntryFromHash(LPCURLCACHE_HEADER pHeader, LPCSTR lpszUrl)
{
    struct _HASH_ENTRY * pHashEntry;
    if (URLCache_FindHash(pHeader, lpszUrl, &pHashEntry))
    {
        pHashEntry->dwHashKey = HASHTABLE_FREE;
        pHashEntry->dwOffsetEntry = HASHTABLE_FREE;
        return TRUE;
    }
    return FALSE;
}

/***********************************************************************
 *           URLCache_AddEntryToHash (Internal)
 *
 *  Searches all the hash tables for a free slot based on the offset
 * generated from the hash key. If a free slot is found, the offset and
 * key are entered into the hash table.
 *
 * RETURNS
 *    TRUE if the entry was added
 *    FALSE if the entry could not be added
 *
 */
static BOOL URLCache_AddEntryToHash(LPCURLCACHE_HEADER pHeader, LPCSTR lpszUrl, DWORD dwOffsetEntry)
{
    /* see URLCache_FindEntryInHash for structure of hash tables */

    DWORD key = URLCache_HashKey(lpszUrl);
    DWORD offset = (key % HASHTABLE_NUM_ENTRIES) * sizeof(struct _HASH_ENTRY);
    HASH_CACHEFILE_ENTRY * pHashEntry;
    DWORD dwHashTableNumber = 0;

    key = (DWORD)(key / HASHTABLE_NUM_ENTRIES) * HASHTABLE_NUM_ENTRIES;

    for (pHashEntry = URLCache_HashEntryFromOffset(pHeader, pHeader->dwOffsetFirstHashTable);
         ((DWORD)((LPBYTE)pHashEntry - (LPBYTE)pHeader) >= ENTRY_START_OFFSET) && ((DWORD)((LPBYTE)pHashEntry - (LPBYTE)pHeader) < pHeader->dwFileSize);
         pHashEntry = URLCache_HashEntryFromOffset(pHeader, pHashEntry->dwAddressNext))
    {
        int i;
        if (pHashEntry->dwHashTableNumber != dwHashTableNumber++)
        {
            ERR("not right hash table number (%ld) expected %ld\n", pHashEntry->dwHashTableNumber, dwHashTableNumber);
            break;
        }
        /* make sure that it is in fact a hash entry */
        if (pHashEntry->CacheFileEntry.dwSignature != HASH_SIGNATURE)
        {
            ERR("not right signature (\"%.4s\") - expected \"HASH\"\n", (LPCSTR)&pHashEntry->CacheFileEntry.dwSignature);
            break;
        }

        for (i = 0; i < HASHTABLE_BLOCKSIZE; i++)
        {
            struct _HASH_ENTRY * pHashElement = &pHashEntry->HashTable[offset + i];
            if (pHashElement->dwHashKey == HASHTABLE_FREE) /* if the slot is free */
            {
                pHashElement->dwHashKey = key;
                pHashElement->dwOffsetEntry = dwOffsetEntry;
                return TRUE;
            }
        }
    }
    FIXME("need to create another hash table\n");
    return FALSE;
}

/***********************************************************************
 *           GetUrlCacheEntryInfoExA (WININET.@)
 *
 */
BOOL WINAPI GetUrlCacheEntryInfoExA(
    LPCSTR lpszUrl,
    LPINTERNET_CACHE_ENTRY_INFOA lpCacheEntryInfo,
    LPDWORD lpdwCacheEntryInfoBufSize,
    LPSTR lpszReserved,
    LPDWORD lpdwReserved,
    LPVOID lpReserved,
    DWORD dwFlags)
{
    TRACE("(%s, %p, %p, %p, %p, %p, %lx)\n",
        debugstr_a(lpszUrl), 
        lpCacheEntryInfo,
        lpdwCacheEntryInfoBufSize,
        lpszReserved,
        lpdwReserved,
        lpReserved,
        dwFlags);

    if ((lpszReserved != NULL) ||
        (lpdwReserved != NULL) ||
        (lpReserved != NULL))
    {
        ERR("Reserved value was not 0\n");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (dwFlags != 0)
        FIXME("Undocumented flag(s): %lx\n", dwFlags);
    return GetUrlCacheEntryInfoA(lpszUrl, lpCacheEntryInfo, lpdwCacheEntryInfoBufSize);
}

/***********************************************************************
 *           GetUrlCacheEntryInfoA (WININET.@)
 *
 */
BOOL WINAPI GetUrlCacheEntryInfoA(
    IN LPCSTR lpszUrlName,
    IN LPINTERNET_CACHE_ENTRY_INFOA lpCacheEntryInfo,
    IN OUT LPDWORD lpdwCacheEntryInfoBufferSize
)
{
    LPURLCACHE_HEADER pHeader;
    CACHEFILE_ENTRY * pEntry;
    URL_CACHEFILE_ENTRY * pUrlEntry;
    URLCACHECONTAINER * pContainer;

    TRACE("(%s, %p, %p)\n", debugstr_a(lpszUrlName), lpCacheEntryInfo, lpdwCacheEntryInfoBufferSize);

    if (!URLCacheContainers_FindContainerA(lpszUrlName, &pContainer))
        return FALSE;

    if (!URLCacheContainer_OpenIndex(pContainer))
        return FALSE;

    if (!(pHeader = URLCacheContainer_LockIndex(pContainer)))
        return FALSE;

    if (!URLCache_FindEntryInHash(pHeader, lpszUrlName, &pEntry))
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        WARN("entry %s not found!\n", debugstr_a(lpszUrlName));
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }

    if (pEntry->dwSignature != URL_SIGNATURE)
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        FIXME("Trying to retrieve entry of unknown format %s\n", debugstr_an((LPSTR)&pEntry->dwSignature, sizeof(DWORD)));
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }

    pUrlEntry = (URL_CACHEFILE_ENTRY *)pEntry;
    TRACE("Found URL: %s\n", debugstr_a(pUrlEntry->szSourceUrlName));
    if (pUrlEntry->dwOffsetHeaderInfo)
        TRACE("Header info: %s\n", debugstr_a((LPBYTE)pUrlEntry + pUrlEntry->dwOffsetHeaderInfo));

    if (!URLCache_CopyEntry(
        pContainer,
        pHeader,
        lpCacheEntryInfo,
        lpdwCacheEntryInfoBufferSize,
        pUrlEntry,
        FALSE /* ANSI */))
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        return FALSE;
    }
    TRACE("Local File Name: %s\n", debugstr_a(lpCacheEntryInfo->lpszLocalFileName));

    URLCacheContainer_UnlockIndex(pContainer, pHeader);

    return TRUE;
}

/***********************************************************************
 *           GetUrlCacheEntryInfoW (WININET.@)
 *
 */
BOOL WINAPI GetUrlCacheEntryInfoW(LPCWSTR lpszUrl,
  LPINTERNET_CACHE_ENTRY_INFOW lpCacheEntryInfo,
  LPDWORD lpdwCacheEntryInfoBufferSize)
{
    LPURLCACHE_HEADER pHeader;
    CACHEFILE_ENTRY * pEntry;
    URL_CACHEFILE_ENTRY * pUrlEntry;
    URLCACHECONTAINER * pContainer;
    LPSTR lpszUrlA;
    int url_len;

    TRACE("(%s, %p, %p)\n", debugstr_w(lpszUrl), lpCacheEntryInfo, lpdwCacheEntryInfoBufferSize);

    url_len = WideCharToMultiByte(CP_ACP, 0, lpszUrl, -1, NULL, 0, NULL, NULL);
    lpszUrlA = HeapAlloc(GetProcessHeap(), 0, url_len * sizeof(CHAR));
    if (!lpszUrlA)
    {
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    WideCharToMultiByte(CP_ACP, 0, lpszUrl, -1, lpszUrlA, url_len, NULL, NULL);

    if (!URLCacheContainers_FindContainerW(lpszUrl, &pContainer))
    {
        HeapFree(GetProcessHeap(), 0, lpszUrlA);
        return FALSE;
    }

    if (!URLCacheContainer_OpenIndex(pContainer))
    {
        HeapFree(GetProcessHeap(), 0, lpszUrlA);
        return FALSE;
    }

    if (!(pHeader = URLCacheContainer_LockIndex(pContainer)))
    {
        HeapFree(GetProcessHeap(), 0, lpszUrlA);
        return FALSE;
    }

    if (!URLCache_FindEntryInHash(pHeader, lpszUrlA, &pEntry))
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        HeapFree(GetProcessHeap(), 0, lpszUrlA);
        WARN("entry %s not found!\n", debugstr_a(lpszUrlA));
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    HeapFree(GetProcessHeap(), 0, lpszUrlA);

    if (pEntry->dwSignature != URL_SIGNATURE)
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        FIXME("Trying to retrieve entry of unknown format %s\n", debugstr_an((LPSTR)&pEntry->dwSignature, sizeof(DWORD)));
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }

    pUrlEntry = (URL_CACHEFILE_ENTRY *)pEntry;
    TRACE("Found URL: %s\n", debugstr_a(pUrlEntry->szSourceUrlName));
    TRACE("Header info: %s\n", debugstr_a((LPBYTE)pUrlEntry + pUrlEntry->dwOffsetHeaderInfo));

    if (!URLCache_CopyEntry(
        pContainer,
        pHeader,
        (LPINTERNET_CACHE_ENTRY_INFOA)lpCacheEntryInfo,
        lpdwCacheEntryInfoBufferSize,
        pUrlEntry,
        TRUE /* UNICODE */))
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        return FALSE;
    }
    TRACE("Local File Name: %s\n", debugstr_w(lpCacheEntryInfo->lpszLocalFileName));

    URLCacheContainer_UnlockIndex(pContainer, pHeader);

    return TRUE;
}

/***********************************************************************
 *           GetUrlCacheEntryInfoExW (WININET.@)
 *
 */
BOOL WINAPI GetUrlCacheEntryInfoExW(
    LPCWSTR lpszUrl,
    LPINTERNET_CACHE_ENTRY_INFOW lpCacheEntryInfo,
    LPDWORD lpdwCacheEntryInfoBufSize,
    LPWSTR lpszReserved,
    LPDWORD lpdwReserved,
    LPVOID lpReserved,
    DWORD dwFlags)
{
    TRACE("(%s, %p, %p, %p, %p, %p, %lx)\n",
        debugstr_w(lpszUrl), 
        lpCacheEntryInfo,
        lpdwCacheEntryInfoBufSize,
        lpszReserved,
        lpdwReserved,
        lpReserved,
        dwFlags);

    if ((lpszReserved != NULL) ||
        (lpdwReserved != NULL) ||
        (lpReserved != NULL))
    {
        ERR("Reserved value was not 0\n");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (dwFlags != 0)
        FIXME("Undocumented flag(s): %lx\n", dwFlags);
    return GetUrlCacheEntryInfoW(lpszUrl, lpCacheEntryInfo, lpdwCacheEntryInfoBufSize);
}

/***********************************************************************
 *           SetUrlCacheEntryInfoA (WININET.@)
 */
BOOL WINAPI SetUrlCacheEntryInfoA(
    LPCSTR lpszUrlName,
    LPINTERNET_CACHE_ENTRY_INFOA lpCacheEntryInfo,
    DWORD dwFieldControl)
{
    LPURLCACHE_HEADER pHeader;
    CACHEFILE_ENTRY * pEntry;
    URLCACHECONTAINER * pContainer;

    TRACE("(%s, %p, 0x%08lx)\n", debugstr_a(lpszUrlName), lpCacheEntryInfo, dwFieldControl);

    if (!URLCacheContainers_FindContainerA(lpszUrlName, &pContainer))
        return FALSE;

    if (!URLCacheContainer_OpenIndex(pContainer))
        return FALSE;

    if (!(pHeader = URLCacheContainer_LockIndex(pContainer)))
        return FALSE;

    if (!URLCache_FindEntryInHash(pHeader, lpszUrlName, &pEntry))
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        WARN("entry %s not found!\n", debugstr_a(lpszUrlName));
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }

    if (pEntry->dwSignature != URL_SIGNATURE)
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        FIXME("Trying to retrieve entry of unknown format %s\n", debugstr_an((LPSTR)&pEntry->dwSignature, sizeof(DWORD)));
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }

    URLCache_SetEntryInfo(
        (URL_CACHEFILE_ENTRY *)pEntry,
        (const INTERNET_CACHE_ENTRY_INFOW *)lpCacheEntryInfo,
        dwFieldControl);

    URLCacheContainer_UnlockIndex(pContainer, pHeader);

    return TRUE;
}

/***********************************************************************
 *           SetUrlCacheEntryInfoW (WININET.@)
 */
BOOL WINAPI SetUrlCacheEntryInfoW(LPCWSTR lpszUrl, LPINTERNET_CACHE_ENTRY_INFOW lpCacheEntryInfo, DWORD dwFieldControl)
{
    LPURLCACHE_HEADER pHeader;
    CACHEFILE_ENTRY * pEntry;
    URLCACHECONTAINER * pContainer;
    LPSTR lpszUrlA;
    int url_len;

    TRACE("(%s, %p, 0x%08lx)\n", debugstr_w(lpszUrl), lpCacheEntryInfo, dwFieldControl);

    url_len = WideCharToMultiByte(CP_ACP, 0, lpszUrl, -1, NULL, 0, NULL, NULL);
    lpszUrlA = HeapAlloc(GetProcessHeap(), 0, url_len * sizeof(CHAR));
    if (!lpszUrlA)
    {
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
    WideCharToMultiByte(CP_ACP, 0, lpszUrl, -1, lpszUrlA, url_len, NULL, NULL);

    if (!URLCacheContainers_FindContainerW(lpszUrl, &pContainer))
    {
        HeapFree(GetProcessHeap(), 0, lpszUrlA);
        return FALSE;
    }

    if (!URLCacheContainer_OpenIndex(pContainer))
    {
        HeapFree(GetProcessHeap(), 0, lpszUrlA);
        return FALSE;
    }

    if (!(pHeader = URLCacheContainer_LockIndex(pContainer)))
    {
        HeapFree(GetProcessHeap(), 0, lpszUrlA);
        return FALSE;
    }

    if (!URLCache_FindEntryInHash(pHeader, lpszUrlA, &pEntry))
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        HeapFree(GetProcessHeap(), 0, lpszUrlA);
        WARN("entry %s not found!\n", debugstr_a(lpszUrlA));
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }
    HeapFree(GetProcessHeap(), 0, lpszUrlA);

    if (pEntry->dwSignature != URL_SIGNATURE)
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        FIXME("Trying to retrieve entry of unknown format %s\n", debugstr_an((LPSTR)&pEntry->dwSignature, sizeof(DWORD)));
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }

    URLCache_SetEntryInfo(
        (URL_CACHEFILE_ENTRY *)pEntry,
        lpCacheEntryInfo,
        dwFieldControl);

    URLCacheContainer_UnlockIndex(pContainer, pHeader);

    return TRUE;
}

/***********************************************************************
 *           RetrieveUrlCacheEntryFileA (WININET.@)
 *
 */
BOOL WINAPI RetrieveUrlCacheEntryFileA(
    IN LPCSTR lpszUrlName,
    OUT LPINTERNET_CACHE_ENTRY_INFOA lpCacheEntryInfo, 
    IN OUT LPDWORD lpdwCacheEntryInfoBufferSize,
    IN DWORD dwReserved
    )
{
    LPURLCACHE_HEADER pHeader;
    CACHEFILE_ENTRY * pEntry;
    URL_CACHEFILE_ENTRY * pUrlEntry;
    URLCACHECONTAINER * pContainer;

    TRACE("(%s, %p, %p, 0x%08lx)\n",
        debugstr_a(lpszUrlName),
        lpCacheEntryInfo,
        lpdwCacheEntryInfoBufferSize,
        dwReserved);

    if (!URLCacheContainers_FindContainerA(lpszUrlName, &pContainer))
        return FALSE;

    if (!URLCacheContainer_OpenIndex(pContainer))
        return FALSE;

    if (!(pHeader = URLCacheContainer_LockIndex(pContainer)))
        return FALSE;

    if (!URLCache_FindEntryInHash(pHeader, lpszUrlName, &pEntry))
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        TRACE("entry %s not found!\n", lpszUrlName);
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }

    if (pEntry->dwSignature != URL_SIGNATURE)
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        FIXME("Trying to retrieve entry of unknown format %s\n", debugstr_an((LPSTR)&pEntry->dwSignature, sizeof(DWORD)));
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }

    pUrlEntry = (URL_CACHEFILE_ENTRY *)pEntry;
    TRACE("Found URL: %s\n", pUrlEntry->szSourceUrlName);
    TRACE("Header info: %s\n", (LPBYTE)pUrlEntry + pUrlEntry->dwOffsetHeaderInfo);

    pUrlEntry->dwHitRate++;
    pUrlEntry->dwUseCount++;
    URLCache_HashEntrySetUse(pHeader, lpszUrlName, pUrlEntry->dwUseCount);

    if (!URLCache_CopyEntry(pContainer, pHeader, lpCacheEntryInfo, lpdwCacheEntryInfoBufferSize, pUrlEntry, FALSE))
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        return FALSE;
    }
    TRACE("Local File Name: %s\n", lpCacheEntryInfo->lpszLocalFileName);

    URLCacheContainer_UnlockIndex(pContainer, pHeader);

    return TRUE;
}

/***********************************************************************
 *           UnlockUrlCacheEntryFileA (WININET.@)
 *
 */
BOOL WINAPI UnlockUrlCacheEntryFileA(
    IN LPCSTR lpszUrlName, 
    IN DWORD dwReserved
    )
{
    LPURLCACHE_HEADER pHeader;
    CACHEFILE_ENTRY * pEntry;
    URL_CACHEFILE_ENTRY * pUrlEntry;
    URLCACHECONTAINER * pContainer;

    TRACE("(%s, 0x%08lx)\n", debugstr_a(lpszUrlName), dwReserved);

    if (dwReserved)
    {
        ERR("dwReserved != 0\n");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!URLCacheContainers_FindContainerA(lpszUrlName, &pContainer))
       return FALSE;

    if (!URLCacheContainer_OpenIndex(pContainer))
        return FALSE;

    if (!(pHeader = URLCacheContainer_LockIndex(pContainer)))
        return FALSE;

    if (!URLCache_FindEntryInHash(pHeader, lpszUrlName, &pEntry))
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        TRACE("entry %s not found!\n", lpszUrlName);
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }

    if (pEntry->dwSignature != URL_SIGNATURE)
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        FIXME("Trying to retrieve entry of unknown format %s\n", debugstr_an((LPSTR)&pEntry->dwSignature, sizeof(DWORD)));
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }

    pUrlEntry = (URL_CACHEFILE_ENTRY *)pEntry;

    if (pUrlEntry->dwUseCount == 0)
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        return FALSE;
    }
    pUrlEntry->dwUseCount--;
    URLCache_HashEntrySetUse(pHeader, lpszUrlName, pUrlEntry->dwUseCount);

    URLCacheContainer_UnlockIndex(pContainer, pHeader);

    return TRUE;
}

/***********************************************************************
 *           CreateUrlCacheEntryA (WININET.@)
 *
 */
BOOL WINAPI CreateUrlCacheEntryA(
    IN LPCSTR lpszUrlName,
    IN DWORD dwExpectedFileSize,
    IN LPCSTR lpszFileExtension,
    OUT LPSTR lpszFileName,
    IN DWORD dwReserved
)
{
    URLCACHECONTAINER * pContainer;
    LPURLCACHE_HEADER pHeader;
    CHAR szFile[MAX_PATH];
    CHAR szExtension[MAX_PATH];
    LPCSTR lpszUrlPart;
    LPCSTR lpszUrlEnd;
    LPCSTR lpszFileNameExtension;
    LPSTR lpszFileNameNoPath;
    int i;
    int countnoextension;
    BYTE CacheDir;
    LONG lBufferSize;
    BOOL bFound = FALSE;
    int count;

    TRACE("(%s, 0x%08lx, %s, %p, 0x%08lx)\n",
        debugstr_a(lpszUrlName),
        dwExpectedFileSize,
        debugstr_a(lpszFileExtension),
        lpszFileName,
        dwReserved);

    if (dwReserved)
    {
        ERR("dwReserved != 0\n");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    for (lpszUrlEnd = lpszUrlName; *lpszUrlEnd; lpszUrlEnd++)
        ;
    
    if (((lpszUrlEnd - lpszUrlName) > 1) && (*(lpszUrlEnd - 1) == '/'))
        lpszUrlEnd--;

    for (lpszUrlPart = lpszUrlEnd; 
        (lpszUrlPart >= lpszUrlName); 
        lpszUrlPart--)
    {
        if ((*lpszUrlPart == '/') && ((lpszUrlEnd - lpszUrlPart) > 1))
        {
            bFound = TRUE;
            lpszUrlPart++;
            break;
        }
    }
    if (!strcmp(lpszUrlPart, "www"))
    {
        lpszUrlPart += strlen("www");
    }

    count = lpszUrlEnd - lpszUrlPart;

    if (bFound && (count < MAX_PATH))
    {
        memcpy(szFile, lpszUrlPart, count * sizeof(CHAR));
        szFile[count] = '\0';
        /* FIXME: get rid of illegal characters like \, / and : */
    }
    else
    {
        FIXME("need to generate a random filename\n");
    }

    TRACE("File name: %s\n", szFile);

    if (!URLCacheContainers_FindContainerA(lpszUrlName, &pContainer))
        return FALSE;

    if (!URLCacheContainer_OpenIndex(pContainer))
        return FALSE;

    if (!(pHeader = URLCacheContainer_LockIndex(pContainer)))
        return FALSE;

    CacheDir = (BYTE)(rand() % pHeader->DirectoryCount);

    lBufferSize = MAX_PATH * sizeof(CHAR);
    URLCache_LocalFileNameToPathA(pContainer, pHeader, szFile, CacheDir, lpszFileName, &lBufferSize);

    URLCacheContainer_UnlockIndex(pContainer, pHeader);

    lpszFileNameNoPath = lpszFileName + lBufferSize / sizeof(CHAR) + DIR_LENGTH + 1;

    countnoextension = strlen(lpszFileNameNoPath);
    lpszFileNameExtension = PathFindExtensionA(lpszFileNameNoPath);
    if (lpszFileNameExtension)
        countnoextension -= strlen(lpszFileNameExtension);
    *szExtension = '\0';

    if (lpszFileExtension)
    {
        szExtension[0] = '.';
        strcpy(szExtension+1, lpszFileExtension);
    }

    for (i = 0; i < 255; i++)
    {
        HANDLE hFile;
        strncpy(lpszFileNameNoPath, szFile, countnoextension);
        sprintf(lpszFileNameNoPath + countnoextension, "[%u]%s", i, szExtension);
        TRACE("Trying: %s\n", lpszFileName);
        hFile = CreateFileA(lpszFileName, GENERIC_READ, 0, NULL, CREATE_NEW, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hFile);
            return TRUE;
        }
    }

    return FALSE;
}

/***********************************************************************
 *           CommitUrlCacheEntryA (WININET.@)
 *
 */
BOOL WINAPI CommitUrlCacheEntryA(
    IN LPCSTR lpszUrlName,
    IN LPCSTR lpszLocalFileName,
    IN FILETIME ExpireTime,
    IN FILETIME LastModifiedTime,
    IN DWORD CacheEntryType,
    IN LPBYTE lpHeaderInfo,
    IN DWORD dwHeaderSize,
    IN LPCSTR lpszFileExtension,
    IN LPCSTR dwReserved
    )
{
    URLCACHECONTAINER * pContainer;
    LPURLCACHE_HEADER pHeader;
    CACHEFILE_ENTRY * pEntry;
    URL_CACHEFILE_ENTRY * pUrlEntry;
    DWORD dwBytesNeeded = sizeof(*pUrlEntry) - sizeof(pUrlEntry->szSourceUrlName);
    DWORD dwOffsetLocalFileName = 0;
    DWORD dwOffsetHeader = 0;
    DWORD dwFileSizeLow = 0;
    DWORD dwFileSizeHigh = 0;
    BYTE cDirectory = 0;

    TRACE("(%s, %s, ..., ..., %lx, %p, %ld, %s, %p)\n",
        debugstr_a(lpszUrlName),
        debugstr_a(lpszLocalFileName),
        CacheEntryType,
        lpHeaderInfo,
        dwHeaderSize,
        lpszFileExtension,
        dwReserved);

    if (dwReserved)
    {
        ERR("dwReserved != 0\n");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (lpHeaderInfo == NULL)
    {
        FIXME("lpHeaderInfo == NULL - will crash at the moment\n");
    }
 
    if (lpszLocalFileName)
    {
        HANDLE hFile;
        hFile = CreateFileA(lpszLocalFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            ERR("couldn't open file %s (error is %ld)\n", debugstr_a(lpszLocalFileName), GetLastError());
            return FALSE;
        }

        /* Get file size */
        dwFileSizeLow = GetFileSize(hFile, &dwFileSizeHigh);
        if ((dwFileSizeLow == -1) && (GetLastError() != NO_ERROR))
        {
            ERR("couldn't get file size (error is %ld)\n", GetLastError());
            CloseHandle(hFile);
            return FALSE;
        }

        CloseHandle(hFile);
    }

    if (!URLCacheContainers_FindContainerA(lpszUrlName, &pContainer))
        return FALSE;

    if (!URLCacheContainer_OpenIndex(pContainer))
        return FALSE;

    if (!(pHeader = URLCacheContainer_LockIndex(pContainer)))
        return FALSE;

    if (URLCache_FindEntryInHash(pHeader, lpszUrlName, &pEntry))
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        FIXME("entry already in cache - don't know what to do!\n");
/*
 *        SetLastError(ERROR_FILE_NOT_FOUND);
 *        return FALSE;
 */
        return TRUE;
    }

    if (lpszLocalFileName)
    {
        BOOL bFound = FALSE;
        char szContainerPath[MAX_PATH];
        int container_path_len;
        container_path_len = WideCharToMultiByte(CP_ACP, 0, pContainer->path, -1, szContainerPath, sizeof(szContainerPath), NULL, NULL);
        if (!container_path_len)
        {
            /* WideCharToMultiByte should have called SetLastError */
            return FALSE;
        }

        if (strncmp(lpszLocalFileName, szContainerPath, container_path_len))
        {
            URLCacheContainer_UnlockIndex(pContainer, pHeader);
            ERR("path %s must begin with cache content path %s\n", debugstr_a(lpszLocalFileName), debugstr_a(szContainerPath));
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        /* skip container path prefix */
        lpszLocalFileName += container_path_len;

        for (cDirectory = 0; cDirectory < pHeader->DirectoryCount; cDirectory++)
        {
            if (!strncmp(pHeader->directory_data[cDirectory].filename, lpszLocalFileName, DIR_LENGTH))
            {
                bFound = TRUE;
                break;
            }
        }

        if (!bFound)
        {
            URLCacheContainer_UnlockIndex(pContainer, pHeader);
            ERR("cache directory not found in path %s\n", lpszLocalFileName);
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        lpszLocalFileName += (DIR_LENGTH + 1); /* "1234WXYZ\" */
    }

    dwBytesNeeded = DWORD_ALIGN(dwBytesNeeded + strlen(lpszUrlName) + 1);
    if (lpszLocalFileName)
    {
        dwOffsetLocalFileName = dwBytesNeeded;
        dwBytesNeeded = DWORD_ALIGN(dwBytesNeeded + strlen(lpszLocalFileName) + 1);
    }
    if (lpHeaderInfo)
    {
        dwOffsetHeader = dwBytesNeeded;
        dwBytesNeeded = DWORD_ALIGN(dwBytesNeeded + dwHeaderSize);
    }

    /* round up to next block */
    if (dwBytesNeeded % BLOCKSIZE)
    {
        dwBytesNeeded -= dwBytesNeeded % BLOCKSIZE;
        dwBytesNeeded += BLOCKSIZE;
    }

    if (!URLCache_FindFirstFreeEntry(pHeader, dwBytesNeeded / BLOCKSIZE, &pEntry))
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        ERR("no free entries\n");
        return FALSE;
    }

    /* FindFirstFreeEntry fills in blocks used */
    pUrlEntry = (URL_CACHEFILE_ENTRY *)pEntry;
    pUrlEntry->CacheFileEntry.dwSignature = URL_SIGNATURE;
    pUrlEntry->CacheDir = cDirectory;
    pUrlEntry->CacheEntryType = CacheEntryType;
    pUrlEntry->dwHeaderInfoSize = dwHeaderSize;
    pUrlEntry->dwExemptDelta = 0;
    pUrlEntry->dwHitRate = 0;
    pUrlEntry->dwOffsetHeaderInfo = dwOffsetHeader;
    pUrlEntry->dwOffsetLocalName = dwOffsetLocalFileName;
    pUrlEntry->dwOffsetUrl = sizeof(*pUrlEntry) - sizeof(pUrlEntry->szSourceUrlName);
    pUrlEntry->dwSizeHigh = 0;
    pUrlEntry->dwSizeLow = dwFileSizeLow;
    pUrlEntry->dwSizeHigh = dwFileSizeHigh;
    pUrlEntry->dwUseCount = 0;
    GetSystemTimeAsFileTime(&pUrlEntry->LastAccessTime);
    pUrlEntry->LastModifiedTime = LastModifiedTime;
    FileTimeToDosDateTime(&pUrlEntry->LastAccessTime, &pUrlEntry->wLastSyncDate, &pUrlEntry->wLastSyncTime);
    FileTimeToDosDateTime(&ExpireTime, &pUrlEntry->wExpiredDate, &pUrlEntry->wExpiredTime);
    pUrlEntry->wUnknownDate = pUrlEntry->wLastSyncDate;
    pUrlEntry->wUnknownTime = pUrlEntry->wLastSyncTime;

    /*** Unknowns ***/
    pUrlEntry->dwUnknown1 = 0;
    pUrlEntry->dwUnknown2 = 0;
    pUrlEntry->dwUnknown3 = 0x60;
    pUrlEntry->Unknown4 = 0;
    pUrlEntry->wUnknown5 = 0x1010;
    pUrlEntry->dwUnknown6 = 0;
    pUrlEntry->dwUnknown7 = 0;
    pUrlEntry->dwUnknown8 = 0;

    strcpy(pUrlEntry->szSourceUrlName, lpszUrlName);
    if (dwOffsetLocalFileName)
        strcpy((LPSTR)((LPBYTE)pUrlEntry + dwOffsetLocalFileName), lpszLocalFileName);
    if (dwOffsetHeader)
        memcpy((LPSTR)((LPBYTE)pUrlEntry + dwOffsetHeader), lpHeaderInfo, dwHeaderSize);

    if (!URLCache_AddEntryToHash(pHeader, lpszUrlName, (DWORD)((LPBYTE)pUrlEntry - (LPBYTE)pHeader)))
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        return FALSE;
    }

    URLCacheContainer_UnlockIndex(pContainer, pHeader);

    return TRUE;
}

BOOL WINAPI ReadUrlCacheEntryStream(
    IN HANDLE hUrlCacheStream,
    IN  DWORD dwLocation,
    IN OUT LPVOID lpBuffer,
    IN OUT LPDWORD lpdwLen,
    IN DWORD dwReserved
    )
{
    /* Get handle to file from 'stream' */
    STREAM_HANDLE * pStream = (STREAM_HANDLE *)hUrlCacheStream;

    if (dwReserved != 0)
    {
        ERR("dwReserved != 0\n");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (IsBadReadPtr(pStream, sizeof(*pStream)) || IsBadStringPtrA(pStream->lpszUrl, INTERNET_MAX_URL_LENGTH))
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if (SetFilePointer(pStream->hFile, dwLocation, NULL, FILE_CURRENT) == -1)
        return FALSE;
    return ReadFile(pStream->hFile, lpBuffer, *lpdwLen, lpdwLen, NULL);
}

/***********************************************************************
 *           RetrieveUrlCacheEntryStreamA (WININET.@)
 *
 */
HANDLE WINAPI RetrieveUrlCacheEntryStreamA(
    IN LPCSTR lpszUrlName,
    OUT LPINTERNET_CACHE_ENTRY_INFOA lpCacheEntryInfo,
    IN OUT LPDWORD lpdwCacheEntryInfoBufferSize,
    IN BOOL fRandomRead,
    IN DWORD dwReserved
    )
{
    /* NOTE: this is not the same as the way that the native
     * version allocates 'stream' handles. I did it this way
     * as it is much easier and no applications should depend
     * on this behaviour. (Native version appears to allocate
     * indices into a table)
     */
    STREAM_HANDLE * pStream;
    HANDLE hFile;

    if (!RetrieveUrlCacheEntryFileA(lpszUrlName, 
        lpCacheEntryInfo, 
        lpdwCacheEntryInfoBufferSize,
        dwReserved))
    {
        return NULL;
    }
    
    hFile = CreateFileA(lpCacheEntryInfo->lpszLocalFileName,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        fRandomRead ? FILE_FLAG_RANDOM_ACCESS : 0,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE;
    
    /* allocate handle storage space */
    pStream = (STREAM_HANDLE *)HeapAlloc(GetProcessHeap(), 0, sizeof(STREAM_HANDLE) + strlen(lpszUrlName) * sizeof(CHAR));
    if (!pStream)
    {
        CloseHandle(hFile);
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }

    pStream->hFile = hFile;
    strcpy(pStream->lpszUrl, lpszUrlName);
    return (HANDLE)pStream;
}

/***********************************************************************
 *           UnlockUrlCacheEntryStream (WININET.@)
 *
 */
BOOL WINAPI UnlockUrlCacheEntryStream(
    IN HANDLE hUrlCacheStream,
    IN DWORD dwReserved
)
{
    STREAM_HANDLE * pStream = (STREAM_HANDLE *)hUrlCacheStream;

    if (dwReserved != 0)
    {
        ERR("dwReserved != 0\n");
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (IsBadReadPtr(pStream, sizeof(*pStream)) || IsBadStringPtrA(pStream->lpszUrl, INTERNET_MAX_URL_LENGTH))
    {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    if (!UnlockUrlCacheEntryFileA(pStream->lpszUrl, 0))
        return FALSE;

    /* close file handle */
    CloseHandle(pStream->hFile);

    /* free allocated space */
    HeapFree(GetProcessHeap(), 0, pStream);

    return TRUE;
}


/***********************************************************************
 *           DeleteUrlCacheEntryA (WININET.@)
 *
 */
BOOL WINAPI DeleteUrlCacheEntryA(LPCSTR lpszUrlName)
{
    URLCACHECONTAINER * pContainer;
    LPURLCACHE_HEADER pHeader;
    CACHEFILE_ENTRY * pEntry;
    DWORD dwStartBlock;
    DWORD dwBlock;
    BYTE * AllocationTable;

    TRACE("(%s)\n", debugstr_a(lpszUrlName));

    if (!URLCacheContainers_FindContainerA(lpszUrlName, &pContainer))
        return FALSE;

    if (!URLCacheContainer_OpenIndex(pContainer))
        return FALSE;

    if (!(pHeader = URLCacheContainer_LockIndex(pContainer)))
        return FALSE;

    if (!URLCache_FindEntryInHash(pHeader, lpszUrlName, &pEntry))
    {
        URLCacheContainer_UnlockIndex(pContainer, pHeader);
        TRACE("entry %s not found!\n", lpszUrlName);
        SetLastError(ERROR_FILE_NOT_FOUND);
        return FALSE;
    }

    AllocationTable = (LPBYTE)pHeader + ALLOCATION_TABLE_OFFSET;

    /* update allocation table */
    dwStartBlock = ((DWORD)pEntry - (DWORD)pHeader) / BLOCKSIZE;
    for (dwBlock = dwStartBlock; dwBlock < dwStartBlock + pEntry->dwBlocksUsed; dwBlock++)
        URLCache_Allocation_BlockFree(AllocationTable, dwBlock);

    URLCache_DeleteEntry(pEntry);

    URLCache_DeleteEntryFromHash(pHeader, lpszUrlName);

    URLCacheContainer_UnlockIndex(pContainer, pHeader);

    return TRUE;
}

/***********************************************************************
 *           FindFirstUrlCacheEntryA (WININET.@)
 *
 */
INTERNETAPI HANDLE WINAPI FindFirstUrlCacheEntryA(LPCSTR lpszUrlSearchPattern,
 LPINTERNET_CACHE_ENTRY_INFOA lpFirstCacheEntryInfo, LPDWORD lpdwFirstCacheEntryInfoBufferSize)
{
  FIXME("(%s, %p, %p): stub\n", debugstr_a(lpszUrlSearchPattern), lpFirstCacheEntryInfo, lpdwFirstCacheEntryInfoBufferSize);
  SetLastError(ERROR_FILE_NOT_FOUND);
  return 0;
}

/***********************************************************************
 *           FindFirstUrlCacheEntryW (WININET.@)
 *
 */
INTERNETAPI HANDLE WINAPI FindFirstUrlCacheEntryW(LPCWSTR lpszUrlSearchPattern,
 LPINTERNET_CACHE_ENTRY_INFOW lpFirstCacheEntryInfo, LPDWORD lpdwFirstCacheEntryInfoBufferSize)
{
  FIXME("(%s, %p, %p): stub\n", debugstr_w(lpszUrlSearchPattern), lpFirstCacheEntryInfo, lpdwFirstCacheEntryInfoBufferSize);
  return 0;
}

/***********************************************************************
 *           CreateUrlCacheGroup (WININET.@)
 *
 */
INTERNETAPI GROUPID WINAPI CreateUrlCacheGroup(DWORD dwFlags, LPVOID 
lpReserved)
{
  FIXME("(%lx, %p): stub\n", dwFlags, lpReserved);
  return FALSE;
}

/***********************************************************************
 *           DeleteUrlCacheGroup (WININET.@)
 *
 */
BOOL WINAPI DeleteUrlCacheGroup(GROUPID GroupId, DWORD dwFlags, LPVOID lpReserved)
{
    FIXME("STUB\n");
    return FALSE;
}

/***********************************************************************
 *           SetUrlCacheEntryGroup (WININET.@)
 *
 */
BOOL WINAPI SetUrlCacheEntryGroup(LPCSTR lpszUrlName, DWORD dwFlags,
  GROUPID GroupId, LPBYTE pbGroupAttributes, DWORD cbGroupAttributes,
  LPVOID lpReserved)
{
    FIXME("STUB\n");
    SetLastError(ERROR_FILE_NOT_FOUND);
    return FALSE;
}

/***********************************************************************
 *           GetUrlCacheConfigInfoW (WININET.@)
 */
BOOL WINAPI GetUrlCacheConfigInfoW(LPDWORD CacheInfo, LPDWORD size, DWORD bitmask)
{
    FIXME("(%p, %p, %lx)\n", CacheInfo, size, bitmask);
    INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}

/***********************************************************************
 *           GetUrlCacheConfigInfoA (WININET.@)
 *
 * CacheInfo is some CACHE_CONFIG_INFO structure, with no MS info found by google
 */
BOOL WINAPI GetUrlCacheConfigInfoA(LPDWORD CacheInfo, LPDWORD size, DWORD bitmask)
{
    FIXME("(%p, %p, %lx)\n", CacheInfo, size, bitmask);
    INTERNET_SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
}
