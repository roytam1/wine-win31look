/*
 * Server-side debugger functions
 *
 * Copyright (C) 1999 Alexandre Julliard
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
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#include "windef.h"
#include "winbase.h"

#include "handle.h"
#include "process.h"
#include "thread.h"
#include "request.h"
#include "console.h"

enum debug_event_state { EVENT_QUEUED, EVENT_SENT, EVENT_CONTINUED };

/* debug event */
struct debug_event
{
    struct object          obj;       /* object header */
    struct debug_event    *next;      /* event queue */
    struct debug_event    *prev;
    struct thread         *sender;    /* thread which sent this event */
    struct thread         *debugger;  /* debugger thread receiving the event */
    enum debug_event_state state;     /* event state */
    int                    status;    /* continuation status */
    debug_event_t          data;      /* event data */
    CONTEXT                context;   /* register context */
};

/* debug context */
struct debug_ctx
{
    struct object        obj;         /* object header */
    struct debug_event  *event_head;  /* head of pending events queue */
    struct debug_event  *event_tail;  /* tail of pending events queue */
    int                  kill_on_exit;/* kill debuggees on debugger exit ? */
};


static void debug_event_dump( struct object *obj, int verbose );
static int debug_event_signaled( struct object *obj, struct thread *thread );
static void debug_event_destroy( struct object *obj );

static const struct object_ops debug_event_ops =
{
    sizeof(struct debug_event),    /* size */
    debug_event_dump,              /* dump */
    add_queue,                     /* add_queue */
    remove_queue,                  /* remove_queue */
    debug_event_signaled,          /* signaled */
    no_satisfied,                  /* satisfied */
    no_get_fd,                     /* get_fd */
    debug_event_destroy            /* destroy */
};

static void debug_ctx_dump( struct object *obj, int verbose );
static int debug_ctx_signaled( struct object *obj, struct thread *thread );
static void debug_ctx_destroy( struct object *obj );

static const struct object_ops debug_ctx_ops =
{
    sizeof(struct debug_ctx),      /* size */
    debug_ctx_dump,                /* dump */
    add_queue,                     /* add_queue */
    remove_queue,                  /* remove_queue */
    debug_ctx_signaled,            /* signaled */
    no_satisfied,                  /* satisfied */
    no_get_fd,                     /* get_fd */
    debug_ctx_destroy              /* destroy */
};


/* routines to build an event according to its type */

static int fill_exception_event( struct debug_event *event, void *arg )
{
    memcpy( &event->data.info.exception, arg, sizeof(event->data.info.exception) );
    return 1;
}

static int fill_create_thread_event( struct debug_event *event, void *arg )
{
    struct process *debugger = event->debugger->process;
    struct thread *thread = event->sender;
    obj_handle_t handle;

    /* documented: THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME */
    if (!(handle = alloc_handle( debugger, thread, THREAD_ALL_ACCESS, FALSE ))) return 0;
    event->data.info.create_thread.handle = handle;
    event->data.info.create_thread.teb    = thread->teb;
    event->data.info.create_thread.start  = arg;
    return 1;
}

static int fill_create_process_event( struct debug_event *event, void *arg )
{
    struct process *debugger = event->debugger->process;
    struct thread *thread = event->sender;
    struct process *process = thread->process;
    obj_handle_t handle;

    /* documented: PROCESS_VM_READ | PROCESS_VM_WRITE */
    if (!(handle = alloc_handle( debugger, process, PROCESS_ALL_ACCESS, FALSE ))) return 0;
    event->data.info.create_process.process = handle;

    /* documented: THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME */
    if (!(handle = alloc_handle( debugger, thread, THREAD_ALL_ACCESS, FALSE )))
    {
        close_handle( debugger, event->data.info.create_process.process, NULL );
        return 0;
    }
    event->data.info.create_process.thread = handle;

    handle = 0;
    if (process->exe.file &&
        /* the doc says write access too, but this doesn't seem a good idea */
        !(handle = alloc_handle( debugger, process->exe.file, GENERIC_READ, FALSE )))
    {
        close_handle( debugger, event->data.info.create_process.process, NULL );
        close_handle( debugger, event->data.info.create_process.thread, NULL );
        return 0;
    }
    event->data.info.create_process.file       = handle;
    event->data.info.create_process.teb        = thread->teb;
    event->data.info.create_process.base       = process->exe.base;
    event->data.info.create_process.start      = arg;
    event->data.info.create_process.dbg_offset = process->exe.dbg_offset;
    event->data.info.create_process.dbg_size   = process->exe.dbg_size;
    event->data.info.create_process.name       = process->exe.name;
    event->data.info.create_process.unicode    = 1;
    return 1;
}

static int fill_exit_thread_event( struct debug_event *event, void *arg )
{
    struct thread *thread = arg;
    event->data.info.exit.exit_code = thread->exit_code;
    return 1;
}

static int fill_exit_process_event( struct debug_event *event, void *arg )
{
    struct process *process = arg;
    event->data.info.exit.exit_code = process->exit_code;
    return 1;
}

static int fill_load_dll_event( struct debug_event *event, void *arg )
{
    struct process *debugger = event->debugger->process;
    struct process_dll *dll = arg;
    obj_handle_t handle = 0;

    if (dll->file && !(handle = alloc_handle( debugger, dll->file, GENERIC_READ, FALSE )))
        return 0;
    event->data.info.load_dll.handle     = handle;
    event->data.info.load_dll.base       = dll->base;
    event->data.info.load_dll.dbg_offset = dll->dbg_offset;
    event->data.info.load_dll.dbg_size   = dll->dbg_size;
    event->data.info.load_dll.name       = dll->name;
    event->data.info.load_dll.unicode    = 1;
    return 1;
}

static int fill_unload_dll_event( struct debug_event *event, void *arg )
{
    event->data.info.unload_dll.base = arg;
    return 1;
}

static int fill_output_debug_string_event( struct debug_event *event, void *arg )
{
    struct debug_event_output_string *data = arg;
    event->data.info.output_string = *data;
    return 1;
}

typedef int (*fill_event_func)( struct debug_event *event, void *arg );

#define NB_DEBUG_EVENTS OUTPUT_DEBUG_STRING_EVENT  /* RIP_EVENT not supported */

static const fill_event_func fill_debug_event[NB_DEBUG_EVENTS] =
{
    fill_exception_event,            /* EXCEPTION_DEBUG_EVENT */
    fill_create_thread_event,        /* CREATE_THREAD_DEBUG_EVENT */
    fill_create_process_event,       /* CREATE_PROCESS_DEBUG_EVENT */
    fill_exit_thread_event,          /* EXIT_THREAD_DEBUG_EVENT */
    fill_exit_process_event,         /* EXIT_PROCESS_DEBUG_EVENT */
    fill_load_dll_event,             /* LOAD_DLL_DEBUG_EVENT */
    fill_unload_dll_event,           /* UNLOAD_DLL_DEBUG_EVENT */
    fill_output_debug_string_event   /* OUTPUT_DEBUG_STRING_EVENT */
};


/* unlink the first event from the queue */
static void unlink_event( struct debug_ctx *debug_ctx, struct debug_event *event )
{
    if (event->prev) event->prev->next = event->next;
    else debug_ctx->event_head = event->next;
    if (event->next) event->next->prev = event->prev;
    else debug_ctx->event_tail = event->prev;
    event->next = event->prev = NULL;
    if (event->sender->debug_event == event) event->sender->debug_event = NULL;
    release_object( event );
}

/* link an event at the end of the queue */
static void link_event( struct debug_event *event )
{
    struct debug_ctx *debug_ctx = event->debugger->debug_ctx;

    assert( debug_ctx );
    grab_object( event );
    event->next = NULL;
    event->prev = debug_ctx->event_tail;
    debug_ctx->event_tail = event;
    if (event->prev) event->prev->next = event;
    else debug_ctx->event_head = event;
    if (!event->sender->debug_event) wake_up( &debug_ctx->obj, 0 );
}

/* find the next event that we can send to the debugger */
static struct debug_event *find_event_to_send( struct debug_ctx *debug_ctx )
{
    struct debug_event *event;
    for (event = debug_ctx->event_head; event; event = event->next)
    {
        if (event->state == EVENT_SENT) continue;  /* already sent */
        if (event->sender->debug_event) continue;  /* thread busy with another one */
        break;
    }
    return event;
}

static void debug_event_dump( struct object *obj, int verbose )
{
    struct debug_event *debug_event = (struct debug_event *)obj;
    assert( obj->ops == &debug_event_ops );
    fprintf( stderr, "Debug event sender=%p code=%d state=%d\n",
             debug_event->sender, debug_event->data.code, debug_event->state );
}

static int debug_event_signaled( struct object *obj, struct thread *thread )
{
    struct debug_event *debug_event = (struct debug_event *)obj;
    assert( obj->ops == &debug_event_ops );
    return debug_event->state == EVENT_CONTINUED;
}

static void debug_event_destroy( struct object *obj )
{
    struct debug_event *event = (struct debug_event *)obj;
    assert( obj->ops == &debug_event_ops );

    /* cannot still be in the queue */
    assert( !event->next );
    assert( !event->prev );

    /* If the event has been sent already, the handles are now under the */
    /* responsibility of the debugger process, so we don't touch them    */
    if (event->state == EVENT_QUEUED)
    {
        struct process *debugger = event->debugger->process;
        switch(event->data.code)
        {
        case CREATE_THREAD_DEBUG_EVENT:
            close_handle( debugger, event->data.info.create_thread.handle, NULL );
            break;
        case CREATE_PROCESS_DEBUG_EVENT:
            if (event->data.info.create_process.file)
                close_handle( debugger, event->data.info.create_process.file, NULL );
            close_handle( debugger, event->data.info.create_process.thread, NULL );
            close_handle( debugger, event->data.info.create_process.process, NULL );
            break;
        case LOAD_DLL_DEBUG_EVENT:
            if (event->data.info.load_dll.handle)
                close_handle( debugger, event->data.info.load_dll.handle, NULL );
            break;
        }
    }
    if (event->sender->context == &event->context) event->sender->context = NULL;
    release_object( event->sender );
    release_object( event->debugger );
}

static void debug_ctx_dump( struct object *obj, int verbose )
{
    struct debug_ctx *debug_ctx = (struct debug_ctx *)obj;
    assert( obj->ops == &debug_ctx_ops );
    fprintf( stderr, "Debug context head=%p tail=%p\n",
             debug_ctx->event_head, debug_ctx->event_tail );
}

static int debug_ctx_signaled( struct object *obj, struct thread *thread )
{
    struct debug_ctx *debug_ctx = (struct debug_ctx *)obj;
    assert( obj->ops == &debug_ctx_ops );
    return find_event_to_send( debug_ctx ) != NULL;
}

static void debug_ctx_destroy( struct object *obj )
{
    struct debug_event *event;
    struct debug_ctx *debug_ctx = (struct debug_ctx *)obj;
    assert( obj->ops == &debug_ctx_ops );

    /* free all pending events */
    while ((event = debug_ctx->event_head) != NULL) unlink_event( debug_ctx, event );
}

/* continue a debug event */
static int continue_debug_event( struct process *process, struct thread *thread, int status )
{
    struct debug_event *event;
    struct debug_ctx *debug_ctx = current->debug_ctx;

    if (!debug_ctx || process->debugger != current || thread->process != process) goto error;

    /* find the event in the queue */
    for (event = debug_ctx->event_head; event; event = event->next)
    {
        if (event->state != EVENT_SENT) continue;
        if (event->sender == thread) break;
    }
    if (!event) goto error;

    assert( event->sender->debug_event == event );

    event->status = status;
    event->state  = EVENT_CONTINUED;
    wake_up( &event->obj, 0 );

    unlink_event( debug_ctx, event );
    resume_process( process );
    return 1;
 error:
    /* not debugging this process, or no such event */
    set_error( STATUS_ACCESS_DENIED );  /* FIXME */
    return 0;
}

/* alloc a debug event for a debugger */
static struct debug_event *alloc_debug_event( struct thread *thread, int code,
                                              void *arg, const CONTEXT *context )
{
    struct thread *debugger = thread->process->debugger;
    struct debug_event *event;

    assert( code > 0 && code <= NB_DEBUG_EVENTS );
    /* cannot queue a debug event for myself */
    assert( debugger->process != thread->process );

    /* build the event */
    if (!(event = alloc_object( &debug_event_ops ))) return NULL;
    event->next      = NULL;
    event->prev      = NULL;
    event->state     = EVENT_QUEUED;
    event->sender    = (struct thread *)grab_object( thread );
    event->debugger  = (struct thread *)grab_object( debugger );
    event->data.code = code;

    if (!fill_debug_event[code-1]( event, arg ))
    {
        event->data.code = -1;  /* make sure we don't attempt to close handles */
        release_object( event );
        return NULL;
    }
    if (context)
    {
        memcpy( &event->context, context, sizeof(event->context) );
        thread->context = &event->context;
    }
    return event;
}

/* generate a debug event from inside the server and queue it */
void generate_debug_event( struct thread *thread, int code, void *arg )
{
    if (thread->process->debugger)
    {
        struct debug_event *event = alloc_debug_event( thread, code, arg, NULL );
        if (event)
        {
            link_event( event );
            suspend_process( thread->process );
            release_object( event );
        }
    }
}

/* attach a process to a debugger thread and suspend it */
static int debugger_attach( struct process *process, struct thread *debugger )
{
    struct thread *thread;

    if (process->debugger) goto error;  /* already being debugged */
    if (!is_process_init_done( process )) goto error;  /* still starting up */
    if (!process->thread_list) goto error;  /* no thread running in the process */

    /* make sure we don't create a debugging loop */
    for (thread = debugger; thread; thread = thread->process->debugger)
        if (thread->process == process) goto error;

    /* don't let a debugger debug its console... won't work */
    if (debugger->process->console && debugger->process->console->renderer->process == process)
        goto error;

    suspend_process( process );
    if (!attach_process( process ) || !set_process_debugger( process, debugger ))
    {
        resume_process( process );
        return 0;
    }
    if (!set_process_debug_flag( process, 1 ))
    {
        process->debugger = NULL;
        resume_process( process );
        return 0;
    }
    return 1;

 error:
    set_error( STATUS_ACCESS_DENIED );
    return 0;
}


/* detach a process from a debugger thread (and resume it ?) */
int debugger_detach( struct process *process, struct thread *debugger )
{
    struct debug_event *event;
    struct debug_ctx *debug_ctx;

    if (!process->debugger || process->debugger != debugger)
        goto error;  /* not currently debugged, or debugged by another debugger */
    if (!debugger->debug_ctx ) goto error; /* should be a debugger */
    /* init should be done, otherwise wouldn't be attached */
    assert(is_process_init_done(process));

    suspend_process( process );
    /* send continue indication for all events */
    debug_ctx = debugger->debug_ctx;

    /* find the event in the queue
     * FIXME: could loop on process' threads and look the debug_event field */
    for (event = debug_ctx->event_head; event; event = event->next)
    {
        if (event->state != EVENT_QUEUED) continue;

        if (event->sender->process == process)
        {
            assert( event->sender->debug_event == event );
            event->status = DBG_CONTINUE;
            event->state  = EVENT_CONTINUED;
            wake_up( &event->obj, 0 );
            unlink_event( debug_ctx, event );
            /* from queued debug event */
            resume_process( process );
        }
    }

    /* remove relationships between process and its debugger */
    process->debugger = NULL;
    if (!set_process_debug_flag( process, 0 )) clear_error();  /* ignore error */
    detach_process( process );

    /* from this function */
    resume_process( process );
    return 0;

 error:
    set_error( STATUS_ACCESS_DENIED );
    return 0;
}

/* generate all startup events of a given process */
void generate_startup_debug_events( struct process *process, void *entry )
{
    struct process_dll *dll;
    struct thread *thread = process->thread_list;

    /* generate creation events */
    generate_debug_event( thread, CREATE_PROCESS_DEBUG_EVENT, entry );
    while ((thread = thread->proc_next))
        generate_debug_event( thread, CREATE_THREAD_DEBUG_EVENT, NULL );

    /* generate dll events (in loading order, i.e. reverse list order) */
    dll = &process->exe;
    while (dll->next) dll = dll->next;
    while (dll != &process->exe)
    {
        generate_debug_event( process->thread_list, LOAD_DLL_DEBUG_EVENT, dll );
        dll = dll->prev;
    }
}

/* set the debugger of a given process */
int set_process_debugger( struct process *process, struct thread *debugger )
{
    struct debug_ctx *debug_ctx;

    assert( !process->debugger );

    if (!debugger->debug_ctx)  /* need to allocate a context */
    {
        if (!(debug_ctx = alloc_object( &debug_ctx_ops ))) return 0;
        debug_ctx->event_head = NULL;
        debug_ctx->event_tail = NULL;
        debug_ctx->kill_on_exit = 1;
        debugger->debug_ctx = debug_ctx;
    }
    process->debugger = debugger;
    return 1;
}

/* a thread is exiting */
void debug_exit_thread( struct thread *thread )
{
    if (thread->debug_ctx)  /* this thread is a debugger */
    {
        if (thread->debug_ctx->kill_on_exit)
        {
            /* kill all debugged processes */
            kill_debugged_processes( thread, thread->exit_code );
        }
        else
        {
            detach_debugged_processes( thread );
        }
        release_object( thread->debug_ctx );
        thread->debug_ctx = NULL;
    }
}

/* Wait for a debug event */
DECL_HANDLER(wait_debug_event)
{
    struct debug_ctx *debug_ctx = current->debug_ctx;
    struct debug_event *event;

    if (!debug_ctx)  /* current thread is not a debugger */
    {
        set_error( STATUS_INVALID_HANDLE );
        return;
    }
    reply->wait = 0;
    if ((event = find_event_to_send( debug_ctx )))
    {
        size_t size = get_reply_max_size();
        event->state = EVENT_SENT;
        event->sender->debug_event = event;
        reply->pid = get_process_id( event->sender->process );
        reply->tid = get_thread_id( event->sender );
        if (size > sizeof(debug_event_t)) size = sizeof(debug_event_t);
        set_reply_data( &event->data, size );
    }
    else  /* no event ready */
    {
        reply->pid  = 0;
        reply->tid  = 0;
        if (req->get_handle)
            reply->wait = alloc_handle( current->process, debug_ctx, SYNCHRONIZE, FALSE );
    }
}

/* Continue a debug event */
DECL_HANDLER(continue_debug_event)
{
    struct process *process = get_process_from_id( req->pid );
    if (process)
    {
        struct thread *thread = get_thread_from_id( req->tid );
        if (thread)
        {
            continue_debug_event( process, thread, req->status );
            release_object( thread );
        }
        release_object( process );
    }
}

/* Start debugging an existing process */
DECL_HANDLER(debug_process)
{
    struct process *process = get_process_from_id( req->pid );
    if (!process) return;

    if (!req->attach)
    {
        debugger_detach( process, current );
    }
    else if (debugger_attach( process, current ))
    {
        struct debug_event_exception data;

        generate_startup_debug_events( process, NULL );
        resume_process( process );

        data.record.ExceptionCode    = EXCEPTION_BREAKPOINT;
        data.record.ExceptionFlags   = EXCEPTION_CONTINUABLE;
        data.record.ExceptionRecord  = NULL;
        data.record.ExceptionAddress = get_thread_ip( process->thread_list );
        data.record.NumberParameters = 0;
        data.first = 1;
        generate_debug_event( process->thread_list, EXCEPTION_DEBUG_EVENT, &data );
    }
    release_object( process );
}

/* queue an exception event */
DECL_HANDLER(queue_exception_event)
{
    reply->handle = 0;
    if (current->process->debugger)
    {
        struct debug_event_exception data;
        struct debug_event *event;
        const CONTEXT *context = get_req_data();
        EXCEPTION_RECORD *rec = (EXCEPTION_RECORD *)(context + 1);

        if (get_req_data_size() < sizeof(*rec) + sizeof(*context))
        {
            set_error( STATUS_INVALID_PARAMETER );
            return;
        }
        data.record = *rec;
        data.first  = req->first;
        if ((event = alloc_debug_event( current, EXCEPTION_DEBUG_EVENT, &data, context )))
        {
            if ((reply->handle = alloc_handle( current->process, event, SYNCHRONIZE, FALSE )))
            {
                link_event( event );
                suspend_process( current->process );
            }
            release_object( event );
        }
    }
}

/* retrieve the status of an exception event */
DECL_HANDLER(get_exception_status)
{
    struct debug_event *event;

    reply->status = 0;
    if ((event = (struct debug_event *)get_handle_obj( current->process, req->handle,
                                                       0, &debug_event_ops )))
    {
        if (event->state == EVENT_CONTINUED)
        {
            reply->status = event->status;
            if (current->context == &event->context)
            {
                size_t size = min( sizeof(CONTEXT), get_reply_max_size() );
                set_reply_data( &event->context, size );
                current->context = NULL;
            }
        }
        else set_error( STATUS_PENDING );
        release_object( event );
    }
}

/* send an output string to the debugger */
DECL_HANDLER(output_debug_string)
{
    struct debug_event_output_string data;

    data.string  = req->string;
    data.unicode = req->unicode;
    data.length  = req->length;
    generate_debug_event( current, OUTPUT_DEBUG_STRING_EVENT, &data );
}

/* simulate a breakpoint in a process */
DECL_HANDLER(debug_break)
{
    struct process *process;

    reply->self = 0;
    if (!(process = get_process_from_handle( req->handle, PROCESS_SET_INFORMATION /*FIXME*/ )))
        return;
    if (process != current->process)
    {
        /* find a suitable thread to signal */
        struct thread *thread;
        for (thread = process->thread_list; thread; thread = thread->proc_next)
        {
            if (send_thread_signal( thread, SIGTRAP )) break;
        }
        if (!thread) set_error( STATUS_ACCESS_DENIED );
    }
    else reply->self = 1;
    release_object( process );
}

/* set debugger kill on exit flag */
DECL_HANDLER(set_debugger_kill_on_exit)
{
    if (!current->debug_ctx)
    {
        set_error( STATUS_ACCESS_DENIED );
        return;
    }
    current->debug_ctx->kill_on_exit = req->kill_on_exit;
}
