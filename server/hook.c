/*
 * Server-side window hooks support
 *
 * Copyright (C) 2002 Alexandre Julliard
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
#include <stdarg.h>
#include <stdio.h>

#include "windef.h"
#include "winbase.h"
#include "winuser.h"

#include "object.h"
#include "process.h"
#include "request.h"
#include "user.h"

struct hook_table;

struct hook
{
    struct list         chain;    /* hook chain entry */
    user_handle_t       handle;   /* user handle for this hook */
    struct thread      *thread;   /* thread owning the hook */
    int                 index;    /* hook table index */
    void               *proc;     /* hook function */
    int                 unicode;  /* is it a unicode hook? */
    WCHAR              *module;   /* module name for global hooks */
    size_t              module_size;
};

#define NB_HOOKS (WH_MAXHOOK-WH_MINHOOK+1)
#define HOOK_ENTRY(p)  LIST_ENTRY( (p), struct hook, chain )

struct hook_table
{
    struct object obj;              /* object header */
    struct list   hooks[NB_HOOKS];  /* array of hook chains */
    int           counts[NB_HOOKS]; /* use counts for each hook chain */
};

static void hook_table_dump( struct object *obj, int verbose );
static void hook_table_destroy( struct object *obj );

static const struct object_ops hook_table_ops =
{
    sizeof(struct hook_table),    /* size */
    hook_table_dump,              /* dump */
    no_add_queue,                 /* add_queue */
    NULL,                         /* remove_queue */
    NULL,                         /* signaled */
    NULL,                         /* satisfied */
    no_get_fd,                    /* get_fd */
    hook_table_destroy            /* destroy */
};


static struct hook_table *global_hooks;

/* create a new hook table */
static struct hook_table *alloc_hook_table(void)
{
    struct hook_table *table;
    int i;

    if ((table = alloc_object( &hook_table_ops )))
    {
        for (i = 0; i < NB_HOOKS; i++)
        {
            list_init( &table->hooks[i] );
            table->counts[i] = 0;
        }
    }
    return table;
}

/* create a new hook and add it to the specified table */
static struct hook *add_hook( struct thread *thread, int index, int global )
{
    struct hook *hook;
    struct hook_table *table = global ? global_hooks : get_queue_hooks(thread);

    if (!table)
    {
        if (!(table = alloc_hook_table())) return NULL;
        if (global) global_hooks = table;
        else set_queue_hooks( thread, table );
    }
    if (!(hook = mem_alloc( sizeof(*hook) ))) return NULL;

    if (!(hook->handle = alloc_user_handle( hook, USER_HOOK )))
    {
        free( hook );
        return NULL;
    }
    hook->thread = thread ? (struct thread *)grab_object( thread ) : NULL;
    hook->index  = index;
    list_add_head( &table->hooks[index], &hook->chain );
    return hook;
}

/* free a hook, removing it from its chain */
static void free_hook( struct hook *hook )
{
    free_user_handle( hook->handle );
    if (hook->module) free( hook->module );
    if (hook->thread) release_object( hook->thread );
    list_remove( &hook->chain );
    free( hook );
}

/* find a hook from its index and proc */
static struct hook *find_hook( struct thread *thread, int index, void *proc )
{
    struct list *p;
    struct hook_table *table = get_queue_hooks( thread );

    if (table)
    {
        LIST_FOR_EACH( p, &table->hooks[index] )
        {
            struct hook *hook = HOOK_ENTRY( p );
            if (hook->proc == proc) return hook;
        }
    }
    return NULL;
}

/* get the hook table that a given hook belongs to */
inline static struct hook_table *get_table( struct hook *hook )
{
    if (!hook->thread) return global_hooks;
    if (hook->index + WH_MINHOOK == WH_KEYBOARD_LL) return global_hooks;
    if (hook->index + WH_MINHOOK == WH_MOUSE_LL) return global_hooks;
    return get_queue_hooks(hook->thread);
}

/* get the first hook in the chain */
inline static struct hook *get_first_hook( struct hook_table *table, int index )
{
    struct list *elem = list_head( &table->hooks[index] );
    return elem ? HOOK_ENTRY( elem ) : NULL;
}

/* find the first non-deleted hook in the chain */
inline static struct hook *get_first_valid_hook( struct hook_table *table, int index )
{
    struct hook *hook = get_first_hook( table, index );
    while (hook && !hook->proc)
        hook = HOOK_ENTRY( list_next( &table->hooks[index], &hook->chain ) );
    return hook;
}

/* find the next hook in the chain, skipping the deleted ones */
static struct hook *get_next_hook( struct hook *hook )
{
    struct hook_table *table = get_table( hook );
    int index = hook->index;

    while ((hook = HOOK_ENTRY( list_next( &table->hooks[index], &hook->chain ) )))
    {
        if (hook->proc) return hook;
    }
    if (global_hooks && table != global_hooks)  /* now search through the global table */
    {
        hook = get_first_valid_hook( global_hooks, index );
    }
    return hook;
}

static void hook_table_dump( struct object *obj, int verbose )
{
    struct hook_table *table = (struct hook_table *)obj;
    if (table == global_hooks) fprintf( stderr, "Global hook table\n" );
    else fprintf( stderr, "Hook table\n" );
}

static void hook_table_destroy( struct object *obj )
{
    int i;
    struct hook *hook;
    struct hook_table *table = (struct hook_table *)obj;

    for (i = 0; i < NB_HOOKS; i++)
    {
        while ((hook = get_first_hook( table, i )) != NULL) free_hook( hook );
    }
}

/* free the global hooks table */
void close_global_hooks(void)
{
    if (global_hooks) release_object( global_hooks );
}

/* remove a hook, freeing it if the chain is not in use */
static void remove_hook( struct hook *hook )
{
    struct hook_table *table = get_table( hook );
    if (table->counts[hook->index])
        hook->proc = NULL; /* chain is in use, just mark it and return */
    else
        free_hook( hook );
}

/* release a hook chain, removing deleted hooks if the use count drops to 0 */
static void release_hook_chain( struct hook_table *table, int index )
{
    if (!table->counts[index])  /* use count shouldn't already be 0 */
    {
        set_error( STATUS_INVALID_PARAMETER );
        return;
    }
    if (!--table->counts[index])
    {
        struct hook *hook = get_first_hook( table, index );
        while (hook)
        {
            struct hook *next = HOOK_ENTRY( list_next( &table->hooks[hook->index], &hook->chain ) );
            if (!hook->proc) free_hook( hook );
            hook = next;
        }
    }
}

/* remove all global hooks owned by a given thread */
void remove_thread_hooks( struct thread *thread )
{
    int index;

    if (!global_hooks) return;

    /* only low-level keyboard/mouse global hooks can be owned by a thread */
    for (index = WH_KEYBOARD_LL - WH_MINHOOK; index <= WH_MOUSE_LL - WH_MINHOOK; index++)
    {
        struct hook *hook = get_first_hook( global_hooks, index );
        while (hook)
        {
            struct hook *next = HOOK_ENTRY( list_next( &global_hooks->hooks[index], &hook->chain ) );
            if (hook->thread == thread) remove_hook( hook );
            hook = next;
        }
    }
}

/* set a window hook */
DECL_HANDLER(set_hook)
{
    struct thread *thread;
    struct hook *hook;
    WCHAR *module;
    int global;
    size_t module_size = get_req_data_size();

    if (!req->proc || req->id < WH_MINHOOK || req->id > WH_MAXHOOK)
    {
        set_error( STATUS_INVALID_PARAMETER );
        return;
    }
    if (req->id == WH_KEYBOARD_LL || req->id == WH_MOUSE_LL)
    {
        /* low-level hardware hooks are special: always global, but without a module */
        thread = (struct thread *)grab_object( current );
        module = NULL;
        global = 1;
    }
    else if (!req->tid)
    {
        if (!module_size)
        {
            set_error( STATUS_INVALID_PARAMETER );
            return;
        }
        if (!(module = memdup( get_req_data(), module_size ))) return;
        thread = NULL;
        global = 1;
    }
    else
    {
        module = NULL;
        global = 0;
        if (!(thread = get_thread_from_id( req->tid ))) return;
    }

    if ((hook = add_hook( thread, req->id - WH_MINHOOK, global )))
    {
        hook->proc        = req->proc;
        hook->unicode     = req->unicode;
        hook->module      = module;
        hook->module_size = module_size;
        reply->handle = hook->handle;
    }
    else if (module) free( module );

    if (thread) release_object( thread );
}


/* remove a window hook */
DECL_HANDLER(remove_hook)
{
    struct hook *hook;

    if (req->handle) hook = get_user_object( req->handle, USER_HOOK );
    else
    {
        if (!req->proc || req->id < WH_MINHOOK || req->id > WH_MAXHOOK)
        {
            set_error( STATUS_INVALID_PARAMETER );
            return;
        }
        if (!(hook = find_hook( current, req->id - WH_MINHOOK, req->proc )))
            set_error( STATUS_INVALID_PARAMETER );
    }
    if (hook) remove_hook( hook );
}


/* start calling a hook chain */
DECL_HANDLER(start_hook_chain)
{
    struct hook *hook;
    struct hook_table *table = get_queue_hooks( current );

    if (req->id < WH_MINHOOK || req->id > WH_MAXHOOK)
    {
        set_error( STATUS_INVALID_PARAMETER );
        return;
    }

    if (!table || !(hook = get_first_valid_hook( table, req->id - WH_MINHOOK )))
    {
        /* try global table */
        if (!(table = global_hooks) ||
            !(hook = get_first_valid_hook( global_hooks, req->id - WH_MINHOOK )))
            return;  /* no hook set */
    }

    if (hook->thread && hook->thread != current)  /* must run in other thread */
    {
        reply->pid  = get_process_id( hook->thread->process );
        reply->tid  = get_thread_id( hook->thread );
        reply->proc = 0;
    }
    else
    {
        reply->pid  = 0;
        reply->tid  = 0;
        reply->proc = hook->proc;
    }
    reply->handle  = hook->handle;
    reply->unicode = hook->unicode;
    table->counts[hook->index]++;
    if (hook->module) set_reply_data( hook->module, hook->module_size );
}


/* finished calling a hook chain */
DECL_HANDLER(finish_hook_chain)
{
    struct hook_table *table = get_queue_hooks( current );
    int index = req->id - WH_MINHOOK;

    if (req->id < WH_MINHOOK || req->id > WH_MAXHOOK)
    {
        set_error( STATUS_INVALID_PARAMETER );
        return;
    }
    if (table) release_hook_chain( table, index );
    if (global_hooks) release_hook_chain( global_hooks, index );
}


/* get the next hook to call */
DECL_HANDLER(get_next_hook)
{
    struct hook *hook, *next;

    if (!(hook = get_user_object( req->handle, USER_HOOK ))) return;
    if (hook->thread && (hook->thread != current))
    {
        set_error( STATUS_INVALID_HANDLE );
        return;
    }
    if ((next = get_next_hook( hook )))
    {
        reply->next = next->handle;
        reply->id   = next->index + WH_MINHOOK;
        reply->prev_unicode = hook->unicode;
        reply->next_unicode = next->unicode;
        if (next->module) set_reply_data( next->module, next->module_size );
        if (next->thread && next->thread != current)
        {
            reply->pid  = get_process_id( next->thread->process );
            reply->tid  = get_thread_id( next->thread );
            reply->proc = 0;
        }
        else
        {
            reply->pid  = 0;
            reply->tid  = 0;
            reply->proc = next->proc;
        }
    }
}
