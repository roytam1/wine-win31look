/*
 * statvfs function
 *
 * Copyright 2004 Alexandre Julliard
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

#ifndef HAVE_STATVFS

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef STATFS_DEFINED_BY_SYS_VFS
# include <sys/vfs.h>
#else
# ifdef STATFS_DEFINED_BY_SYS_MOUNT
#  include <sys/mount.h>
# else
#  ifdef STATFS_DEFINED_BY_SYS_STATFS
#   include <sys/statfs.h>
#  endif
# endif
#endif

int statvfs( const char *path, struct statvfs *buf )
{
    int ret;
#ifdef HAVE_STATFS
    struct statfs info;

/* FIXME: add autoconf check for this */
#if defined(__svr4__) || defined(_SCO_DS) || defined(__sun)
    ret = statfs( path, &info, 0, 0 );
#else
    ret = statfs( path, &info );
#endif
    if (ret >= 0)
    {
        memset( buf, 0, sizeof(*buf) );
        buf->f_bsize   = info.f_bsize;
        buf->f_blocks  = info.f_blocks;
        buf->f_files   = info.f_files;
#ifdef HAVE_STRUCT_STATFS_F_NAMELEN
        buf->f_namemax = info.f_namelen;
#endif
#ifdef HAVE_STRUCT_STATFS_F_FRSIZE
        buf->f_frsize  = info.f_frsize;
#else
        buf->f_frsize  = info.f_bsize;
#endif
#ifdef HAVE_STRUCT_STATFS_F_BFREE
        buf->f_bfree   = info.f_bfree;
#else
        buf->f_bfree   = info.f_bavail;
#endif
#ifdef HAVE_STRUCT_STATFS_F_BAVAIL
        buf->f_bavail  = info.f_bavail;
#else
        buf->f_bavail  = info.f_bfree;
#endif
#ifdef HAVE_STRUCT_STATFS_F_FFREE
        buf->f_ffree   = info.f_ffree;
#else
        buf->f_ffree   = info.f_favail;
#endif
#ifdef HAVE_STRUCT_STATFS_F_FAVAIL
        buf->f_favail  = info.f_favail;
#else
        buf->f_favail  = info.f_ffree;
#endif
    }
#else  /* HAVE_STATFS */
    ret = -1;
    errno = ENOSYS;
#endif  /* HAVE_STATFS */
    return ret;
}

#endif /* HAVE_STATVFS */
