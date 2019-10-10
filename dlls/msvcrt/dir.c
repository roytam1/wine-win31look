/*
 * msvcrt.dll drive/directory functions
 *
 * Copyright 1996,1998 Marcus Meissner
 * Copyright 1996 Jukka Iivonen
 * Copyright 1997,2000 Uwe Bonnes
 * Copyright 2000 Jon Griffiths
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

#include <stdarg.h>
#include <time.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winternl.h"
#include "wine/unicode.h"
#include "msvcrt.h"
#include "msvcrt/errno.h"

#include "wine/unicode.h"
#include "msvcrt/io.h"
#include "msvcrt/stdlib.h"
#include "msvcrt/string.h"
#include "msvcrt/dos.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msvcrt);

/* INTERNAL: Translate WIN32_FIND_DATAA to finddata_t  */
static void msvcrt_fttofd( const WIN32_FIND_DATAA *fd, struct MSVCRT__finddata_t* ft)
{
  DWORD dw;

  if (fd->dwFileAttributes == FILE_ATTRIBUTE_NORMAL)
    ft->attrib = 0;
  else
    ft->attrib = fd->dwFileAttributes;

  RtlTimeToSecondsSince1970( (LARGE_INTEGER *)&fd->ftCreationTime, &dw );
  ft->time_create = dw;
  RtlTimeToSecondsSince1970( (LARGE_INTEGER *)&fd->ftLastAccessTime, &dw );
  ft->time_access = dw;
  RtlTimeToSecondsSince1970( (LARGE_INTEGER *)&fd->ftLastWriteTime, &dw );
  ft->time_write = dw;
  ft->size = fd->nFileSizeLow;
  strcpy(ft->name, fd->cFileName);
}

/* INTERNAL: Translate WIN32_FIND_DATAW to wfinddata_t  */
static void msvcrt_wfttofd( const WIN32_FIND_DATAW *fd, struct MSVCRT__wfinddata_t* ft)
{
  DWORD dw;

  if (fd->dwFileAttributes == FILE_ATTRIBUTE_NORMAL)
    ft->attrib = 0;
  else
    ft->attrib = fd->dwFileAttributes;

  RtlTimeToSecondsSince1970( (LARGE_INTEGER *)&fd->ftCreationTime, &dw );
  ft->time_create = dw;
  RtlTimeToSecondsSince1970( (LARGE_INTEGER *)&fd->ftLastAccessTime, &dw );
  ft->time_access = dw;
  RtlTimeToSecondsSince1970( (LARGE_INTEGER *)&fd->ftLastWriteTime, &dw );
  ft->time_write = dw;
  ft->size = fd->nFileSizeLow;
  strcpyW(ft->name, fd->cFileName);
}

/* INTERNAL: Translate WIN32_FIND_DATAA to finddatai64_t  */
static void msvcrt_fttofdi64( const WIN32_FIND_DATAA *fd, struct MSVCRT__finddatai64_t* ft)
{
  DWORD dw;

  if (fd->dwFileAttributes == FILE_ATTRIBUTE_NORMAL)
    ft->attrib = 0;
  else
    ft->attrib = fd->dwFileAttributes;

  RtlTimeToSecondsSince1970( (LARGE_INTEGER *)&fd->ftCreationTime, &dw );
  ft->time_create = dw;
  RtlTimeToSecondsSince1970( (LARGE_INTEGER *)&fd->ftLastAccessTime, &dw );
  ft->time_access = dw;
  RtlTimeToSecondsSince1970( (LARGE_INTEGER *)&fd->ftLastWriteTime, &dw );
  ft->time_write = dw;
  ft->size = ((__int64)fd->nFileSizeHigh) << 32 | fd->nFileSizeLow;
  strcpy(ft->name, fd->cFileName);
}

/* INTERNAL: Translate WIN32_FIND_DATAW to wfinddatai64_t  */
static void msvcrt_wfttofdi64( const WIN32_FIND_DATAW *fd, struct MSVCRT__wfinddatai64_t* ft)
{
  DWORD dw;

  if (fd->dwFileAttributes == FILE_ATTRIBUTE_NORMAL)
    ft->attrib = 0;
  else
    ft->attrib = fd->dwFileAttributes;

  RtlTimeToSecondsSince1970( (LARGE_INTEGER *)&fd->ftCreationTime, &dw );
  ft->time_create = dw;
  RtlTimeToSecondsSince1970( (LARGE_INTEGER *)&fd->ftLastAccessTime, &dw );
  ft->time_access = dw;
  RtlTimeToSecondsSince1970( (LARGE_INTEGER *)&fd->ftLastWriteTime, &dw );
  ft->time_write = dw;
  ft->size = ((__int64)fd->nFileSizeHigh) << 32 | fd->nFileSizeLow;
  strcpyW(ft->name, fd->cFileName);
}

/*********************************************************************
 *		_chdir (MSVCRT.@)
 *
 * Change the current working directory.
 *
 * PARAMS
 *  newdir [I] Directory to change to
 *
 * RETURNS
 *  Success: 0. The current working directory is set to newdir.
 *  Failure: -1. errno indicates the error.
 *
 * NOTES
 *  See SetCurrentDirectoryA.
 */
int _chdir(const char * newdir)
{
  if (!SetCurrentDirectoryA(newdir))
  {
    MSVCRT__set_errno(newdir?GetLastError():0);
    return -1;
  }
  return 0;
}

/*********************************************************************
 *		_wchdir (MSVCRT.@)
 *
 * Unicode version of _chdir.
 */
int _wchdir(const MSVCRT_wchar_t * newdir)
{
  if (!SetCurrentDirectoryW(newdir))
  {
    MSVCRT__set_errno(newdir?GetLastError():0);
    return -1;
  }
  return 0;
}

/*********************************************************************
 *		_chdrive (MSVCRT.@)
 *
 * Change the current drive.
 *
 * PARAMS
 *  newdrive [I] Drive number to change to (1 = 'A', 2 = 'B', ...)
 *
 * RETURNS
 *  Success: 0. The current drive is set to newdrive.
 *  Failure: -1. errno indicates the error.
 *
 * NOTES
 *  See SetCurrentDirectoryA.
 */
int _chdrive(int newdrive)
{
  WCHAR buffer[3] = {'A', ':', 0};

  buffer[0] += newdrive - 1;
  if (!SetCurrentDirectoryW( buffer ))
  {
    MSVCRT__set_errno(GetLastError());
    if (newdrive <= 0)
      *MSVCRT__errno() = MSVCRT_EACCES;
    return -1;
  }
  return 0;
}

/*********************************************************************
 *		_findclose (MSVCRT.@)
 *
 * Close a handle returned by _findfirst().
 *
 * PARAMS
 *  hand [I] Handle to close
 *
 * RETURNS
 *  Success: 0. All resources associated with hand are freed.
 *  Failure: -1. errno indicates the error.
 *
 * NOTES
 *  See FindClose.
 */
int _findclose(long hand)
{
  TRACE(":handle %ld\n",hand);
  if (!FindClose((HANDLE)hand))
  {
    MSVCRT__set_errno(GetLastError());
    return -1;
  }
  return 0;
}

/*********************************************************************
 *		_findfirst (MSVCRT.@)
 *
 * Open a handle for iterating through a directory.
 *
 * PARAMS
 *  fspec [I] File specification of files to iterate.
 *  ft    [O] Information for the first file found.
 *
 * RETURNS
 *  Success: A handle suitable for passing to _findnext() and _findclose().
 *           ft is populated with the details of the found file.
 *  Failure: -1. errno indicates the error.
 *
 * NOTES
 *  See FindFirstFileA.
 */
long MSVCRT__findfirst(const char * fspec, struct MSVCRT__finddata_t* ft)
{
  WIN32_FIND_DATAA find_data;
  HANDLE hfind;

  hfind  = FindFirstFileA(fspec, &find_data);
  if (hfind == INVALID_HANDLE_VALUE)
  {
    MSVCRT__set_errno(GetLastError());
    return -1;
  }
  msvcrt_fttofd(&find_data,ft);
  TRACE(":got handle %p\n",hfind);
  return (long)hfind;
}

/*********************************************************************
 *		_wfindfirst (MSVCRT.@)
 *
 * Unicode version of _findfirst.
 */
long MSVCRT__wfindfirst(const MSVCRT_wchar_t * fspec, struct MSVCRT__wfinddata_t* ft)
{
  WIN32_FIND_DATAW find_data;
  HANDLE hfind;

  hfind  = FindFirstFileW(fspec, &find_data);
  if (hfind == INVALID_HANDLE_VALUE)
  {
    MSVCRT__set_errno(GetLastError());
    return -1;
  }
  msvcrt_wfttofd(&find_data,ft);
  TRACE(":got handle %p\n",hfind);
  return (long)hfind;
}

/*********************************************************************
 *		_findfirsti64 (MSVCRT.@)
 *
 * 64-bit version of _findfirst.
 */
long MSVCRT__findfirsti64(const char * fspec, struct MSVCRT__finddatai64_t* ft)
{
  WIN32_FIND_DATAA find_data;
  HANDLE hfind;

  hfind  = FindFirstFileA(fspec, &find_data);
  if (hfind == INVALID_HANDLE_VALUE)
  {
    MSVCRT__set_errno(GetLastError());
    return -1;
  }
  msvcrt_fttofdi64(&find_data,ft);
  TRACE(":got handle %p\n",hfind);
  return (long)hfind;
}

/*********************************************************************
 *		_wfindfirsti64 (MSVCRT.@)
 *
 * Unicode version of _findfirsti64.
 */
long MSVCRT__wfindfirsti64(const MSVCRT_wchar_t * fspec, struct MSVCRT__wfinddatai64_t* ft)
{
  WIN32_FIND_DATAW find_data;
  HANDLE hfind;

  hfind  = FindFirstFileW(fspec, &find_data);
  if (hfind == INVALID_HANDLE_VALUE)
  {
    MSVCRT__set_errno(GetLastError());
    return -1;
  }
  msvcrt_wfttofdi64(&find_data,ft);
  TRACE(":got handle %p\n",hfind);
  return (long)hfind;
}

/*********************************************************************
 *		_findnext (MSVCRT.@)
 *
 * Find the next file from a file search handle.
 *
 * PARAMS
 *  hand  [I] Handle to the search returned from _findfirst().
 *  ft    [O] Information for the file found.
 *
 * RETURNS
 *  Success: 0. ft is populated with the details of the found file.
 *  Failure: -1. errno indicates the error.
 *
 * NOTES
 *  See FindNextFileA.
 */
int MSVCRT__findnext(long hand, struct MSVCRT__finddata_t * ft)
{
  WIN32_FIND_DATAA find_data;

  if (!FindNextFileA((HANDLE)hand, &find_data))
  {
    *MSVCRT__errno() = MSVCRT_ENOENT;
    return -1;
  }

  msvcrt_fttofd(&find_data,ft);
  return 0;
}

/*********************************************************************
 *		_wfindnext (MSVCRT.@)
 *
 * Unicode version of _findnext.
 */
int MSVCRT__wfindnext(long hand, struct MSVCRT__wfinddata_t * ft)
{
  WIN32_FIND_DATAW find_data;

  if (!FindNextFileW((HANDLE)hand, &find_data))
  {
    *MSVCRT__errno() = MSVCRT_ENOENT;
    return -1;
  }

  msvcrt_wfttofd(&find_data,ft);
  return 0;
}

/*********************************************************************
 *		_findnexti64 (MSVCRT.@)
 *
 * 64-bit version of _findnext.
 */
int MSVCRT__findnexti64(long hand, struct MSVCRT__finddatai64_t * ft)
{
  WIN32_FIND_DATAA find_data;

  if (!FindNextFileA((HANDLE)hand, &find_data))
  {
    *MSVCRT__errno() = MSVCRT_ENOENT;
    return -1;
  }

  msvcrt_fttofdi64(&find_data,ft);
  return 0;
}

/*********************************************************************
 *		_wfindnexti64 (MSVCRT.@)
 *
 * Unicode version of _findnexti64.
 */
int MSVCRT__wfindnexti64(long hand, struct MSVCRT__wfinddatai64_t * ft)
{
  WIN32_FIND_DATAW find_data;

  if (!FindNextFileW((HANDLE)hand, &find_data))
  {
    *MSVCRT__errno() = MSVCRT_ENOENT;
    return -1;
  }

  msvcrt_wfttofdi64(&find_data,ft);
  return 0;
}

/*********************************************************************
 *		_getcwd (MSVCRT.@)
 *
 * Get the current working directory.
 *
 * PARAMS
 *  buf  [O] Destination for current working directory.
 *  size [I] Size of buf in characters
 *
 * RETURNS
 * Success: If buf is NULL, returns an allocated string containing the path.
 *          Otherwise populates buf with the path and returns it.
 * Failure: NULL. errno indicates the error.
 */
char* _getcwd(char * buf, int size)
{
  char dir[MAX_PATH];
  int dir_len = GetCurrentDirectoryA(MAX_PATH,dir);

  if (dir_len < 1)
    return NULL; /* FIXME: Real return value untested */

  if (!buf)
  {
    if (size < 0)
      return _strdup(dir);
    return msvcrt_strndup(dir,size);
  }
  if (dir_len >= size)
  {
    *MSVCRT__errno() = MSVCRT_ERANGE;
    return NULL; /* buf too small */
  }
  strcpy(buf,dir);
  return buf;
}

/*********************************************************************
 *		_wgetcwd (MSVCRT.@)
 *
 * Unicode version of _getcwd.
 */
MSVCRT_wchar_t* _wgetcwd(MSVCRT_wchar_t * buf, int size)
{
  MSVCRT_wchar_t dir[MAX_PATH];
  int dir_len = GetCurrentDirectoryW(MAX_PATH,dir);

  if (dir_len < 1)
    return NULL; /* FIXME: Real return value untested */

  if (!buf)
  {
    if (size < 0)
      return _wcsdup(dir);
    return msvcrt_wstrndup(dir,size);
  }
  if (dir_len >= size)
  {
    *MSVCRT__errno() = MSVCRT_ERANGE;
    return NULL; /* buf too small */
  }
  strcpyW(buf,dir);
  return buf;
}

/*********************************************************************
 *		_getdrive (MSVCRT.@)
 *
 * Get the current drive number.
 *
 * PARAMS
 *  None.
 *
 * RETURNS
 *  Success: The drive letter number from 1 to 26 ("A:" to "Z:").
 *  Failure: 0.
 */
int _getdrive(void)
{
    WCHAR buffer[MAX_PATH];
    if (GetCurrentDirectoryW( MAX_PATH, buffer ) &&
        buffer[0] >= 'A' && buffer[0] <= 'z' && buffer[1] == ':')
        return toupperW(buffer[0]) - 'A' + 1;
    return 0;
}

/*********************************************************************
 *		_getdcwd (MSVCRT.@)
 *
 * Get the current working directory on a given disk.
 * 
 * PARAMS
 *  drive [I] Drive letter to get the current working directory from.
 *  buf   [O] Destination for the current working directory.
 *  size  [I] Length of drive in characters.
 *
 * RETURNS
 *  Success: If drive is NULL, returns an allocated string containing the path.
 *           Otherwise populates drive with the path and returns it.
 *  Failure: NULL. errno indicates the error.
 */
char* _getdcwd(int drive, char * buf, int size)
{
  static char* dummy;

  TRACE(":drive %d(%c), size %d\n",drive, drive + 'A' - 1, size);

  if (!drive || drive == _getdrive())
    return _getcwd(buf,size); /* current */
  else
  {
    char dir[MAX_PATH];
    char drivespec[4] = {'A', ':', '\\', 0};
    int dir_len;

    drivespec[0] += drive - 1;
    if (GetDriveTypeA(drivespec) < DRIVE_REMOVABLE)
    {
      *MSVCRT__errno() = MSVCRT_EACCES;
      return NULL;
    }

    dir_len = GetFullPathNameA(drivespec,MAX_PATH,dir,&dummy);
    if (dir_len >= size || dir_len < 1)
    {
      *MSVCRT__errno() = MSVCRT_ERANGE;
      return NULL; /* buf too small */
    }

    TRACE(":returning '%s'\n", dir);
    if (!buf)
      return _strdup(dir); /* allocate */

    strcpy(buf,dir);
  }
  return buf;
}

/*********************************************************************
 *		_wgetdcwd (MSVCRT.@)
 *
 * Unicode version of _wgetdcwd.
 */
MSVCRT_wchar_t* _wgetdcwd(int drive, MSVCRT_wchar_t * buf, int size)
{
  static MSVCRT_wchar_t* dummy;

  TRACE(":drive %d(%c), size %d\n",drive, drive + 'A' - 1, size);

  if (!drive || drive == _getdrive())
    return _wgetcwd(buf,size); /* current */
  else
  {
    MSVCRT_wchar_t dir[MAX_PATH];
    MSVCRT_wchar_t drivespec[4] = {'A', ':', '\\', 0};
    int dir_len;

    drivespec[0] += drive - 1;
    if (GetDriveTypeW(drivespec) < DRIVE_REMOVABLE)
    {
      *MSVCRT__errno() = MSVCRT_EACCES;
      return NULL;
    }

    dir_len = GetFullPathNameW(drivespec,MAX_PATH,dir,&dummy);
    if (dir_len >= size || dir_len < 1)
    {
      *MSVCRT__errno() = MSVCRT_ERANGE;
      return NULL; /* buf too small */
    }

    TRACE(":returning %s\n", debugstr_w(dir));
    if (!buf)
      return _wcsdup(dir); /* allocate */
    strcpyW(buf,dir);
  }
  return buf;
}

/*********************************************************************
 *		_getdiskfree (MSVCRT.@)
 *
 * Get information about the free space on a drive.
 *
 * PARAMS
 *  disk [I] Drive number to get information about (1 = 'A', 2 = 'B', ...)
 *  info [O] Destination for the resulting information.
 *
 * RETURNS
 *  Success: 0. info is updated with the free space information.
 *  Failure: An error code from GetLastError().
 *
 * NOTES
 *  See GetLastError().
 */
unsigned int MSVCRT__getdiskfree(unsigned int disk, struct MSVCRT(_diskfree_t)* d)
{
  WCHAR drivespec[4] = {'@', ':', '\\', 0};
  DWORD ret[4];
  unsigned int err;

  if (disk > 26)
    return ERROR_INVALID_PARAMETER; /* MSVCRT doesn't set errno here */

  drivespec[0] += disk; /* make a drive letter */

  if (GetDiskFreeSpaceW(disk==0?NULL:drivespec,ret,ret+1,ret+2,ret+3))
  {
    d->sectors_per_cluster = (unsigned)ret[0];
    d->bytes_per_sector = (unsigned)ret[1];
    d->avail_clusters = (unsigned)ret[2];
    d->total_clusters = (unsigned)ret[3];
    return 0;
  }
  err = GetLastError();
  MSVCRT__set_errno(err);
  return err;
}

/*********************************************************************
 *		_mkdir (MSVCRT.@)
 *
 * Create a directory.
 *
 * PARAMS
 *  newdir [I] Name of directory to create.
 *
 * RETURNS
 *  Success: 0. The directory indicated by newdir is created.
 *  Failure: -1. errno indicates the error.
 *
 * NOTES
 *  See CreateDirectoryA.
 */
int _mkdir(const char * newdir)
{
  if (CreateDirectoryA(newdir,NULL))
    return 0;
  MSVCRT__set_errno(GetLastError());
  return -1;
}

/*********************************************************************
 *		_wmkdir (MSVCRT.@)
 *
 * Unicode version of _mkdir.
 */
int _wmkdir(const MSVCRT_wchar_t* newdir)
{
  if (CreateDirectoryW(newdir,NULL))
    return 0;
  MSVCRT__set_errno(GetLastError());
  return -1;
}

/*********************************************************************
 *		_rmdir (MSVCRT.@)
 *
 * Delete a directory.
 *
 * PARAMS
 *  dir [I] Name of directory to delete.
 *
 * RETURNS
 *  Success: 0. The directory indicated by newdir is deleted.
 *  Failure: -1. errno indicates the error.
 *
 * NOTES
 *  See RemoveDirectoryA.
 */
int _rmdir(const char * dir)
{
  if (RemoveDirectoryA(dir))
    return 0;
  MSVCRT__set_errno(GetLastError());
  return -1;
}

/*********************************************************************
 *		_wrmdir (MSVCRT.@)
 *
 * Unicode version of _rmdir.
 */
int _wrmdir(const MSVCRT_wchar_t * dir)
{
  if (RemoveDirectoryW(dir))
    return 0;
  MSVCRT__set_errno(GetLastError());
  return -1;
}

/*********************************************************************
 *		_wsplitpath (MSVCRT.@)
 *
 * Unicode version of _splitpath.
 */
void _wsplitpath(const MSVCRT_wchar_t *inpath, MSVCRT_wchar_t *drv, MSVCRT_wchar_t *dir,
                 MSVCRT_wchar_t *fname, MSVCRT_wchar_t *ext )
{
    const MSVCRT_wchar_t *p, *end;

    if (inpath[0] && inpath[1] == ':')
    {
        if (drv)
        {
            drv[0] = inpath[0];
            drv[1] = inpath[1];
            drv[2] = 0;
        }
        inpath += 2;
    }
    else if (drv) drv[0] = 0;

    /* look for end of directory part */
    end = NULL;
    for (p = inpath; *p; p++) if (*p == '/' || *p == '\\') end = p + 1;

    if (end)  /* got a directory */
    {
        if (dir)
        {
            memcpy( dir, inpath, (end - inpath) * sizeof(MSVCRT_wchar_t) );
            dir[end - inpath] = 0;
        }
        inpath = end;
    }
    else if (dir) dir[0] = 0;

    /* look for extension: what's after the last dot */
    end = NULL;
    for (p = inpath; *p; p++) if (*p == '.') end = p;

    if (!end) end = p; /* there's no extension */

    if (fname)
    {
        memcpy( fname, inpath, (end - inpath) * sizeof(MSVCRT_wchar_t) );
        fname[end - inpath] = 0;
    }
    if (ext) strcpyW( ext, end );
}

/* INTERNAL: Helper for _fullpath. Modified PD code from 'snippets'. */
static void wmsvcrt_fln_fix(MSVCRT_wchar_t *path)
{
  int dir_flag = 0, root_flag = 0;
  MSVCRT_wchar_t *r, *p, *q, *s;
  MSVCRT_wchar_t szbsdot[] = { '\\', '.', 0 };

  /* Skip drive */
  if (NULL == (r = strrchrW(path, ':')))
    r = path;
  else
    ++r;

  /* Ignore leading slashes */
  while ('\\' == *r)
    if ('\\' == r[1])
      strcpyW(r, &r[1]);
    else
    {
      root_flag = 1;
      ++r;
    }

  p = r; /* Change "\\" to "\" */
  while (NULL != (p = strchrW(p, '\\')))
    if ('\\' ==  p[1])
      strcpyW(p, &p[1]);
    else
      ++p;

  while ('.' == *r) /* Scrunch leading ".\" */
  {
    if ('.' == r[1])
    {
      /* Ignore leading ".." */
      for (p = (r += 2); *p && (*p != '\\'); ++p)
        ;
    }
    else
    {
      for (p = r + 1 ;*p && (*p != '\\'); ++p)
	;
    }
    strcpyW(r, p + ((*p) ? 1 : 0));
  }

  while ('\\' == path[strlenW(path)-1])   /* Strip last '\\' */
  {
    dir_flag = 1;
    path[strlenW(path)-1] = '\0';
  }

  s = r;

  /* Look for "\." in path */

  while (NULL != (p = strstrW(s, szbsdot)))
  {
    if ('.' == p[2])
    {
      /* Execute this section if ".." found */
      q = p - 1;
      while (q > r)           /* Backup one level           */
      {
        if (*q == '\\')
          break;
        --q;
      }
      if (q > r)
      {
        strcpyW(q, p + 3);
        s = q;
      }
      else if ('.' != *q)
      {
        strcpyW(q + ((*q == '\\') ? 1 : 0),
                p + 3 + ((*(p + 3)) ? 1 : 0));
        s = q;
      }
      else  s = ++p;
    }
    else
    {
      /* Execute this section if "." found */
      q = p + 2;
      for ( ;*q && (*q != '\\'); ++q)
        ;
      strcpyW (p, q);
    }
  }

  if (root_flag)  /* Embedded ".." could have bubbled up to root  */
  {
    for (p = r; *p && ('.' == *p || '\\' == *p); ++p)
      ;
    if (r != p)
      strcpyW(r, p);
  }

  if (dir_flag)
  {
    MSVCRT_wchar_t szbs[] = { '\\', 0 };

    strcatW(path, szbs);
  }
}

/*********************************************************************
 *		_wfullpath (MSVCRT.@)
 *
 * Unicode version of _fullpath.
 */
MSVCRT_wchar_t *_wfullpath(MSVCRT_wchar_t * absPath, const MSVCRT_wchar_t* relPath, MSVCRT_size_t size)
{
  MSVCRT_wchar_t drive[5],dir[MAX_PATH],file[MAX_PATH],ext[MAX_PATH];
  MSVCRT_wchar_t res[MAX_PATH];
  size_t len;
  MSVCRT_wchar_t szbs[] = { '\\', 0 };


  res[0] = '\0';

  if (!relPath || !*relPath)
    return _wgetcwd(absPath, size);

  if (size < 4)
  {
    *MSVCRT__errno() = MSVCRT_ERANGE;
    return NULL;
  }

  TRACE(":resolving relative path '%s'\n",debugstr_w(relPath));

  _wsplitpath(relPath, drive, dir, file, ext);

  /* Get Directory and drive into 'res' */
  if (!dir[0] || (dir[0] != '/' && dir[0] != '\\'))
  {
    /* Relative or no directory given */
    _wgetdcwd(drive[0] ? toupper(drive[0]) - 'A' + 1 :  0, res, MAX_PATH);
    strcatW(res,szbs);
    if (dir[0])
      strcatW(res,dir);
    if (drive[0])
      res[0] = drive[0]; /* If given a drive, preserve the letter case */
  }
  else
  {
    strcpyW(res,drive);
    strcatW(res,dir);
  }

  strcatW(res,szbs);
  strcatW(res, file);
  strcatW(res, ext);
  wmsvcrt_fln_fix(res);

  len = strlenW(res);
  if (len >= MAX_PATH || len >= (size_t)size)
    return NULL; /* FIXME: errno? */

  if (!absPath)
    return _wcsdup(res);
  strcpyW(absPath,res);
  return absPath;
}

/* INTERNAL: Helper for _fullpath. Modified PD code from 'snippets'. */
static void msvcrt_fln_fix(char *path)
{
  int dir_flag = 0, root_flag = 0;
  char *r, *p, *q, *s;

  /* Skip drive */
  if (NULL == (r = strrchr(path, ':')))
    r = path;
  else
    ++r;

  /* Ignore leading slashes */
  while ('\\' == *r)
    if ('\\' == r[1])
      strcpy(r, &r[1]);
    else
    {
      root_flag = 1;
      ++r;
    }

  p = r; /* Change "\\" to "\" */
  while (NULL != (p = strchr(p, '\\')))
    if ('\\' ==  p[1])
      strcpy(p, &p[1]);
    else
      ++p;

  while ('.' == *r) /* Scrunch leading ".\" */
  {
    if ('.' == r[1])
    {
      /* Ignore leading ".." */
      for (p = (r += 2); *p && (*p != '\\'); ++p)
        ;
    }
    else
    {
      for (p = r + 1 ;*p && (*p != '\\'); ++p)
        ;
    }
    strcpy(r, p + ((*p) ? 1 : 0));
  }

  while ('\\' == path[strlen(path)-1])   /* Strip last '\\' */
  {
    dir_flag = 1;
    path[strlen(path)-1] = '\0';
  }

  s = r;

  /* Look for "\." in path */

  while (NULL != (p = strstr(s, "\\.")))
  {
    if ('.' == p[2])
    {
      /* Execute this section if ".." found */
      q = p - 1;
      while (q > r)           /* Backup one level           */
      {
        if (*q == '\\')
          break;
        --q;
      }
      if (q > r)
      {
        strcpy(q, p + 3);
        s = q;
      }
      else if ('.' != *q)
      {
        strcpy(q + ((*q == '\\') ? 1 : 0),
               p + 3 + ((*(p + 3)) ? 1 : 0));
        s = q;
      }
      else  s = ++p;
    }
    else
    {
      /* Execute this section if "." found */
      q = p + 2;
      for ( ;*q && (*q != '\\'); ++q)
        ;
      strcpy (p, q);
    }
  }

  if (root_flag)  /* Embedded ".." could have bubbled up to root  */
  {
    for (p = r; *p && ('.' == *p || '\\' == *p); ++p)
      ;
    if (r != p)
      strcpy(r, p);
  }

  if (dir_flag)
    strcat(path, "\\");
}

/*********************************************************************
 *		_fullpath (MSVCRT.@)
 *
 * Create an absolute path from a relative path.
 *
 * PARAMS
 *  absPath [O] Destination for absolute path
 *  relPath [I] Relative path to convert to absolute
 *  size    [I] Length of absPath in characters.
 *
 * RETURNS
 * Success: If absPath is NULL, returns an allocated string containing the path.
 *          Otherwise populates absPath with the path and returns it.
 * Failure: NULL. errno indicates the error.
 */
char *_fullpath(char * absPath, const char* relPath, unsigned int size)
{
  char drive[5],dir[MAX_PATH],file[MAX_PATH],ext[MAX_PATH];
  char res[MAX_PATH];
  size_t len;

  res[0] = '\0';

  if (!relPath || !*relPath)
    return _getcwd(absPath, size);

  if (size < 4)
  {
    *MSVCRT__errno() = MSVCRT_ERANGE;
    return NULL;
  }

  TRACE(":resolving relative path '%s'\n",relPath);

  _splitpath(relPath, drive, dir, file, ext);

  /* Get Directory and drive into 'res' */
  if (!dir[0] || (dir[0] != '/' && dir[0] != '\\'))
  {
    /* Relative or no directory given */
    _getdcwd(drive[0] ? toupper(drive[0]) - 'A' + 1 :  0, res, MAX_PATH);
    strcat(res,"\\");
    if (dir[0])
      strcat(res,dir);
    if (drive[0])
      res[0] = drive[0]; /* If given a drive, preserve the letter case */
  }
  else
  {
    strcpy(res,drive);
    strcat(res,dir);
  }

  strcat(res,"\\");
  strcat(res, file);
  strcat(res, ext);
  msvcrt_fln_fix(res);

  len = strlen(res);
  if (len >= MAX_PATH || len >= (size_t)size)
    return NULL; /* FIXME: errno? */

  if (!absPath)
    return _strdup(res);
  strcpy(absPath,res);
  return absPath;
}

/*********************************************************************
 *		_makepath (MSVCRT.@)
 *
 * Create a pathname.
 *
 * PARAMS
 *  path      [O] Destination for created pathname
 *  drive     [I] Drive letter (e.g. "A:")
 *  directory [I] Directory
 *  filename  [I] Name of the file, excluding extension
 *  extension [I] File extension (e.g. ".TXT")
 *
 * RETURNS
 *  Nothing. If path is not large enough to hold the resulting pathname,
 *  random process memory will be overwritten.
 */
VOID _makepath(char * path, const char * drive,
               const char *directory, const char * filename,
               const char * extension)
{
    char ch;
    char tmpPath[MAX_PATH];
    TRACE("got %s %s %s %s\n", debugstr_a(drive), debugstr_a(directory),
          debugstr_a(filename), debugstr_a(extension) );

    if ( !path )
        return;

    tmpPath[0] = '\0';
    if (drive && drive[0])
    {
        tmpPath[0] = drive[0];
        tmpPath[1] = ':';
        tmpPath[2] = 0;
    }
    if (directory && directory[0])
    {
        strcat(tmpPath, directory);
        ch = tmpPath[strlen(tmpPath)-1];
        if (ch != '/' && ch != '\\')
            strcat(tmpPath,"\\");
    }
    if (filename && filename[0])
    {
        strcat(tmpPath, filename);
        if (extension && extension[0])
        {
            if ( extension[0] != '.' )
                strcat(tmpPath,".");
            strcat(tmpPath,extension);
        }
    }

    strcpy( path, tmpPath );

    TRACE("returning %s\n",path);
}

/*********************************************************************
 *		_wmakepath (MSVCRT.@)
 *
 * Unicode version of _wmakepath.
 */
VOID _wmakepath(MSVCRT_wchar_t *path, const MSVCRT_wchar_t *drive, const MSVCRT_wchar_t *directory,
                const MSVCRT_wchar_t *filename, const MSVCRT_wchar_t *extension)
{
    MSVCRT_wchar_t ch;
    TRACE("%s %s %s %s\n", debugstr_w(drive), debugstr_w(directory),
          debugstr_w(filename), debugstr_w(extension));

    if ( !path )
        return;

    path[0] = 0;
    if (drive && drive[0])
    {
        path[0] = drive[0];
        path[1] = ':';
        path[2] = 0;
    }
    if (directory && directory[0])
    {
        strcatW(path, directory);
        ch = path[strlenW(path) - 1];
        if (ch != '/' && ch != '\\')
        {
            static const MSVCRT_wchar_t backslashW[] = {'\\',0};
            strcatW(path, backslashW);
        }
    }
    if (filename && filename[0])
    {
        strcatW(path, filename);
        if (extension && extension[0])
        {
            if ( extension[0] != '.' )
            {
                static const MSVCRT_wchar_t dotW[] = {'.',0};
                strcatW(path, dotW);
            }
            strcatW(path, extension);
        }
    }

    TRACE("returning %s\n", debugstr_w(path));
}

/*********************************************************************
 *		_searchenv (MSVCRT.@)
 *
 * Search for a file in a list of paths from an envronment variable.
 *
 * PARAMS
 *  file   [I] Name of the file to search for.
 *  env    [I] Name of the environment variable containing a list of paths.
 *  buf    [O] Destination for the found file path.
 *
 * RETURNS
 *  Nothing. If the file is not found, buf will contain an empty string
 *  and errno is set.
 */
void _searchenv(const char* file, const char* env, char *buf)
{
  char*envVal, *penv;
  char curPath[MAX_PATH];

  *buf = '\0';

  /* Try CWD first */
  if (GetFileAttributesA( file ) != INVALID_FILE_ATTRIBUTES)
  {
    GetFullPathNameA( file, MAX_PATH, buf, NULL );
    /* Sigh. This error is *always* set, regardless of success */
    MSVCRT__set_errno(ERROR_FILE_NOT_FOUND);
    return;
  }

  /* Search given environment variable */
  envVal = MSVCRT_getenv(env);
  if (!envVal)
  {
    MSVCRT__set_errno(ERROR_FILE_NOT_FOUND);
    return;
  }

  penv = envVal;
  TRACE(":searching for %s in paths %s\n", file, envVal);

  do
  {
    char *end = penv;

    while(*end && *end != ';') end++; /* Find end of next path */
    if (penv == end || !*penv)
    {
      MSVCRT__set_errno(ERROR_FILE_NOT_FOUND);
      return;
    }
    strncpy(curPath, penv, end - penv);
    if (curPath[end - penv] != '/' || curPath[end - penv] != '\\')
    {
      curPath[end - penv] = '\\';
      curPath[end - penv + 1] = '\0';
    }
    else
      curPath[end - penv] = '\0';

    strcat(curPath, file);
    TRACE("Checking for file %s\n", curPath);
    if (GetFileAttributesA( curPath ) != INVALID_FILE_ATTRIBUTES)
    {
      strcpy(buf, curPath);
      MSVCRT__set_errno(ERROR_FILE_NOT_FOUND);
      return; /* Found */
    }
    penv = *end ? end + 1 : end;
  } while(1);
}
