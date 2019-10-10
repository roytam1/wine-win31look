/*
 * Server-side handle management
 *
 * Copyright (C) 1998 Alexandre Julliard
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

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "windef.h"
#include "winbase.h"

#include "handle.h"
#include "process.h"
#include "thread.h"
#include "request.h"

struct handle_entry
{
    struct object *ptr;       /* object */
    unsigned int   access;    /* access rights */
    int            fd;        /* file descriptor (in client process) */
};

struct handle_table
{
    struct object        obj;         /* object header */
    struct process      *process;     /* process owning this table */
    int                  count;       /* number of allocated entries */
    int                  last;        /* last used entry */
    int                  free;        /* first entry that may be free */
    struct handle_entry *entries;     /* handle entries */
};

static struct handle_table *global_table;

/* reserved handle access rights */
#define RESERVED_SHIFT         25
#define RESERVED_INHERIT       (HANDLE_FLAG_INHERIT << RESERVED_SHIFT)
#define RESERVED_CLOSE_PROTECT (HANDLE_FLAG_PROTECT_FROM_CLOSE << RESERVED_SHIFT)
#define RESERVED_ALL           (RESERVED_INHERIT | RESERVED_CLOSE_PROTECT)

#define MIN_HANDLE_ENTRIES  32


/* handle to table index conversion */

/* handles are a multiple of 4 under NT; handle 0 is not used */
inline static obj_handle_t index_to_handle( int index )
{
    return (obj_handle_t)((index + 1) << 2);
}
inline static int handle_to_index( obj_handle_t handle )
{
    return ((unsigned int)handle >> 2) - 1;
}

/* global handle conversion */

#define HANDLE_OBFUSCATOR 0x544a4def

inline static int handle_is_global( obj_handle_t handle)
{
    return ((unsigned long)handle ^ HANDLE_OBFUSCATOR) < 0x10000;
}
inline static obj_handle_t handle_local_to_global( obj_handle_t handle )
{
    if (!handle) return 0;
    return (obj_handle_t)((unsigned long)handle ^ HANDLE_OBFUSCATOR);
}
inline static obj_handle_t handle_global_to_local( obj_handle_t handle )
{
    return (obj_handle_t)((unsigned long)handle ^ HANDLE_OBFUSCATOR);
}


static void handle_table_dump( struct object *obj, int verbose );
static void handle_table_destroy( struct object *obj );

static const struct object_ops handle_table_ops =
{
    sizeof(struct handle_table),     /* size */
    handle_table_dump,               /* dump */
    no_add_queue,                    /* add_queue */
    NULL,                            /* remove_queue */
    NULL,                            /* signaled */
    NULL,                            /* satisfied */
    no_get_fd,                       /* get_fd */
    handle_table_destroy             /* destroy */
};

/* dump a handle table */
static void handle_table_dump( struct object *obj, int verbose )
{
    int i;
    struct handle_table *table = (struct handle_table *)obj;
    struct handle_entry *entry = table->entries;

    assert( obj->ops == &handle_table_ops );

    fprintf( stderr, "Handle table last=%d count=%d process=%p\n",
             table->last, table->count, table->process );
    if (!verbose) return;
    entry = table->entries;
    for (i = 0; i <= table->last; i++, entry++)
    {
        if (!entry->ptr) continue;
        fprintf( stderr, "%9u: %p %08x ",
                 (unsigned int)index_to_handle(i), entry->ptr, entry->access );
        entry->ptr->ops->dump( entry->ptr, 0 );
    }
}

/* destroy a handle table */
static void handle_table_destroy( struct object *obj )
{
    int i;
    struct handle_table *table = (struct handle_table *)obj;
    struct handle_entry *entry = table->entries;

    assert( obj->ops == &handle_table_ops );

    for (i = 0; i <= table->last; i++, entry++)
    {
        struct object *obj = entry->ptr;
        entry->ptr = NULL;
        if (obj) release_object( obj );
    }
    free( table->entries );
}

/* allocate a new handle table */
struct handle_table *alloc_handle_table( struct process *process, int count )
{
    struct handle_table *table;

    if (count < MIN_HANDLE_ENTRIES) count = MIN_HANDLE_ENTRIES;
    if (!(table = alloc_object( &handle_table_ops )))
        return NULL;
    table->process = process;
    table->count   = count;
    table->last    = -1;
    table->free    = 0;
    if ((table->entries = mem_alloc( count * sizeof(*table->entries) ))) return table;
    release_object( table );
    return NULL;
}

/* grow a handle table */
static int grow_handle_table( struct handle_table *table )
{
    struct handle_entry *new_entries;
    int count = table->count;

    if (count >= INT_MAX / 2) return 0;
    count *= 2;
    if (!(new_entries = realloc( table->entries, count * sizeof(struct handle_entry) )))
    {
        set_error( STATUS_NO_MEMORY );
        return 0;
    }
    table->entries = new_entries;
    table->count   = count;
    return 1;
}

/* allocate the first free entry in the handle table */
static obj_handle_t alloc_entry( struct handle_table *table, void *obj, unsigned int access )
{
    struct handle_entry *entry = table->entries + table->free;
    int i;

    for (i = table->free; i <= table->last; i++, entry++) if (!entry->ptr) goto found;
    if (i >= table->count)
    {
        if (!grow_handle_table( table )) return 0;
        entry = table->entries + i;  /* the entries may have moved */
    }
    table->last = i;
 found:
    table->free = i + 1;
    entry->ptr    = grab_object( obj );
    entry->access = access;
    entry->fd     = -1;
    return index_to_handle(i);
}

/* allocate a handle for an object, incrementing its refcount */
/* return the handle, or 0 on error */
obj_handle_t alloc_handle( struct process *process, void *obj, unsigned int access, int inherit )
{
    struct handle_table *table = process->handles;

    assert( table );
    assert( !(access & RESERVED_ALL) );
    if (inherit) access |= RESERVED_INHERIT;
    return alloc_entry( table, obj, access );
}

/* allocate a global handle for an object, incrementing its refcount */
/* return the handle, or 0 on error */
static obj_handle_t alloc_global_handle( void *obj, unsigned int access )
{
    if (!global_table)
    {
        if (!(global_table = (struct handle_table *)alloc_handle_table( NULL, 0 )))
            return 0;
    }
    return handle_local_to_global( alloc_entry( global_table, obj, access ));
}

/* return a handle entry, or NULL if the handle is invalid */
static struct handle_entry *get_handle( struct process *process, obj_handle_t handle )
{
    struct handle_table *table = process->handles;
    struct handle_entry *entry;
    int index;

    if (handle_is_global(handle))
    {
        handle = handle_global_to_local(handle);
        table = global_table;
    }
    if (!table) goto error;
    index = handle_to_index( handle );
    if (index < 0) goto error;
    if (index > table->last) goto error;
    entry = table->entries + index;
    if (!entry->ptr) goto error;
    return entry;

 error:
    set_error( STATUS_INVALID_HANDLE );
    return NULL;
}

/* attempt to shrink a table */
static void shrink_handle_table( struct handle_table *table )
{
    struct handle_entry *entry = table->entries + table->last;
    struct handle_entry *new_entries;
    int count = table->count;

    while (table->last >= 0)
    {
        if (entry->ptr) break;
        table->last--;
        entry--;
    }
    if (table->last >= count / 4) return;  /* no need to shrink */
    if (count < MIN_HANDLE_ENTRIES * 2) return;  /* too small to shrink */
    count /= 2;
    if (!(new_entries = realloc( table->entries, count * sizeof(*new_entries) ))) return;
    table->count   = count;
    table->entries = new_entries;
}

/* copy the handle table of the parent process */
/* return 1 if OK, 0 on error */
struct handle_table *copy_handle_table( struct process *process, struct process *parent )
{
    struct handle_table *parent_table = parent->handles;
    struct handle_table *table;
    int i;

    assert( parent_table );
    assert( parent_table->obj.ops == &handle_table_ops );

    if (!(table = (struct handle_table *)alloc_handle_table( process, parent_table->count )))
        return NULL;

    if ((table->last = parent_table->last) >= 0)
    {
        struct handle_entry *ptr = table->entries;
        memcpy( ptr, parent_table->entries, (table->last + 1) * sizeof(struct handle_entry) );
        for (i = 0; i <= table->last; i++, ptr++)
        {
            if (!ptr->ptr) continue;
            ptr->fd = -1;
            if (ptr->access & RESERVED_INHERIT) grab_object( ptr->ptr );
            else ptr->ptr = NULL; /* don't inherit this entry */
        }
    }
    /* attempt to shrink the table */
    shrink_handle_table( table );
    return table;
}

/* close a handle and decrement the refcount of the associated object */
/* return 1 if OK, 0 on error */
int close_handle( struct process *process, obj_handle_t handle, int *fd )
{
    struct handle_table *table;
    struct handle_entry *entry;
    struct object *obj;

    if (!(entry = get_handle( process, handle ))) return 0;
    if (entry->access & RESERVED_CLOSE_PROTECT)
    {
        set_error( STATUS_INVALID_HANDLE );
        return 0;
    }
    obj = entry->ptr;
    entry->ptr = NULL;
    if (fd) *fd = entry->fd;
    else if (entry->fd != -1) return 1;  /* silently ignore close attempt if we cannot close the fd */
    entry->fd = -1;
    table = handle_is_global(handle) ? global_table : process->handles;
    if (entry < table->entries + table->free) table->free = entry - table->entries;
    if (entry == table->entries + table->last) shrink_handle_table( table );
    /* hack: windows seems to treat registry handles differently */
    registry_close_handle( obj, handle );
    release_object( obj );
    return 1;
}

/* close all the global handles */
void close_global_handles(void)
{
    if (global_table)
    {
        release_object( global_table );
        global_table = NULL;
    }
}

/* retrieve the object corresponding to one of the magic pseudo-handles */
static inline struct object *get_magic_handle( obj_handle_t handle )
{
    switch((unsigned long)handle)
    {
        case 0xfffffffe:  /* current thread pseudo-handle */
            return &current->obj;
        case 0x7fffffff:  /* current process pseudo-handle */
        case 0xffffffff:  /* current process pseudo-handle */
            return (struct object *)current->process;
        default:
            return NULL;
    }
}

/* retrieve the object corresponding to a handle, incrementing its refcount */
struct object *get_handle_obj( struct process *process, obj_handle_t handle,
                               unsigned int access, const struct object_ops *ops )
{
    struct handle_entry *entry;
    struct object *obj;

    if (!(obj = get_magic_handle( handle )))
    {
        if (!(entry = get_handle( process, handle ))) return NULL;
        if ((entry->access & access) != access)
        {
            set_error( STATUS_ACCESS_DENIED );
            return NULL;
        }
        obj = entry->ptr;
    }
    if (ops && (obj->ops != ops))
    {
        set_error( STATUS_OBJECT_TYPE_MISMATCH );  /* not the right type */
        return NULL;
    }
    return grab_object( obj );
}

/* retrieve the cached fd for a given handle */
int get_handle_unix_fd( struct process *process, obj_handle_t handle, unsigned int access )
{
    struct handle_entry *entry;

    if (!(entry = get_handle( process, handle ))) return -1;
    if ((entry->access & access) != access)
    {
        set_error( STATUS_ACCESS_DENIED );
        return -1;
    }
    return entry->fd;
}

/* remove the cached fd and return it */
int flush_cached_fd( struct process *process, obj_handle_t handle )
{
    struct handle_entry *entry = get_handle( process, handle );
    int fd = -1;

    if (entry)
    {
        fd = entry->fd;
        entry->fd = -1;
    }
    return fd;
}

/* find the first inherited handle of the given type */
/* this is needed for window stations and desktops (don't ask...) */
obj_handle_t find_inherited_handle( struct process *process, const struct object_ops *ops )
{
    struct handle_table *table = process->handles;
    struct handle_entry *ptr;
    int i;

    if (!table) return 0;

    for (i = 0, ptr = table->entries; i <= table->last; i++, ptr++)
    {
        if (!ptr->ptr) continue;
        if (ptr->ptr->ops != ops) continue;
        if (ptr->access & RESERVED_INHERIT) return index_to_handle(i);
    }
    return 0;
}

/* get/set the handle reserved flags */
/* return the old flags (or -1 on error) */
int set_handle_info( struct process *process, obj_handle_t handle, int mask, int flags, int *fd )
{
    struct handle_entry *entry;
    unsigned int old_access;

    if (get_magic_handle( handle ))
    {
        /* we can retrieve but not set info for magic handles */
        if (mask) set_error( STATUS_ACCESS_DENIED );
        return 0;
    }
    if (!(entry = get_handle( process, handle ))) return -1;
    old_access = entry->access;
    mask  = (mask << RESERVED_SHIFT) & RESERVED_ALL;
    flags = (flags << RESERVED_SHIFT) & mask;
    entry->access = (entry->access & ~mask) | flags;
    /* if no current fd set it, otherwise return current fd */
    if (entry->fd == -1) entry->fd = *fd;
    *fd = entry->fd;
    return (old_access & RESERVED_ALL) >> RESERVED_SHIFT;
}

/* duplicate a handle */
obj_handle_t duplicate_handle( struct process *src, obj_handle_t src_handle, struct process *dst,
                           unsigned int access, int inherit, int options )
{
    obj_handle_t res;
    struct object *obj = get_handle_obj( src, src_handle, 0, NULL );

    if (!obj) return 0;
    if (options & DUP_HANDLE_SAME_ACCESS)
    {
        struct handle_entry *entry = get_handle( src, src_handle );
        if (entry)
            access = entry->access;
        else  /* pseudo-handle, give it full access */
        {
            access = STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL;
            clear_error();
        }
    }
    access &= ~RESERVED_ALL;
    if (options & DUP_HANDLE_MAKE_GLOBAL)
        res = alloc_global_handle( obj, access );
    else
        res = alloc_handle( dst, obj, access, inherit );
    release_object( obj );
    return res;
}

/* open a new handle to an existing object */
obj_handle_t open_object( const struct namespace *namespace, const WCHAR *name, size_t len,
                          const struct object_ops *ops, unsigned int access, int inherit )
{
    obj_handle_t handle = 0;
    struct object *obj = find_object( namespace, name, len );
    if (obj)
    {
        if (ops && obj->ops != ops)
            set_error( STATUS_OBJECT_TYPE_MISMATCH );
        else
            handle = alloc_handle( current->process, obj, access, inherit );
        release_object( obj );
    }
    else
        set_error( STATUS_OBJECT_NAME_NOT_FOUND );
    return handle;
}

/* return the size of the handle table of a given process */
unsigned int get_handle_table_count( struct process *process )
{
    return process->handles->count;
}

/* close a handle */
DECL_HANDLER(close_handle)
{
    close_handle( current->process, req->handle, &reply->fd );
}

/* set a handle information */
DECL_HANDLER(set_handle_info)
{
    int fd = req->fd;

    if (handle_is_global(req->handle)) fd = -1;  /* no fd cache for global handles */
    reply->old_flags = set_handle_info( current->process, req->handle,
                                        req->mask, req->flags, &fd );
    reply->cur_fd = fd;
}

/* duplicate a handle */
DECL_HANDLER(dup_handle)
{
    struct process *src, *dst;

    reply->handle = 0;
    reply->fd = -1;
    if ((src = get_process_from_handle( req->src_process, PROCESS_DUP_HANDLE )))
    {
        if (req->options & DUP_HANDLE_MAKE_GLOBAL)
        {
            reply->handle = duplicate_handle( src, req->src_handle, NULL,
                                              req->access, req->inherit, req->options );
        }
        else if ((dst = get_process_from_handle( req->dst_process, PROCESS_DUP_HANDLE )))
        {
            reply->handle = duplicate_handle( src, req->src_handle, dst,
                                              req->access, req->inherit, req->options );
            release_object( dst );
        }
        /* close the handle no matter what happened */
        if (req->options & DUP_HANDLE_CLOSE_SOURCE)
        {
            if (src == current->process) close_handle( src, req->src_handle, &reply->fd );
            else close_handle( src, req->src_handle, NULL );
        }
        release_object( src );
    }
}
