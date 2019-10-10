/*
 * Server-side event management
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
#include <stdio.h>
#include <stdlib.h>

#include "windef.h"

#include "handle.h"
#include "thread.h"
#include "request.h"

struct event
{
    struct object  obj;             /* object header */
    int            manual_reset;    /* is it a manual reset event? */
    int            signaled;        /* event has been signaled */
};

static void event_dump( struct object *obj, int verbose );
static int event_signaled( struct object *obj, struct thread *thread );
static int event_satisfied( struct object *obj, struct thread *thread );

static const struct object_ops event_ops =
{
    sizeof(struct event),      /* size */
    event_dump,                /* dump */
    add_queue,                 /* add_queue */
    remove_queue,              /* remove_queue */
    event_signaled,            /* signaled */
    event_satisfied,           /* satisfied */
    no_get_fd,                 /* get_fd */
    no_destroy                 /* destroy */
};


struct event *create_event( const WCHAR *name, size_t len,
                            int manual_reset, int initial_state )
{
    struct event *event;

    if ((event = create_named_object( sync_namespace, &event_ops, name, len )))
    {
        if (get_error() != STATUS_OBJECT_NAME_COLLISION)
        {
            /* initialize it if it didn't already exist */
            event->manual_reset = manual_reset;
            event->signaled     = initial_state;
        }
    }
    return event;
}

struct event *get_event_obj( struct process *process, obj_handle_t handle, unsigned int access )
{
    return (struct event *)get_handle_obj( process, handle, access, &event_ops );
}

void pulse_event( struct event *event )
{
    event->signaled = 1;
    /* wake up all waiters if manual reset, a single one otherwise */
    wake_up( &event->obj, !event->manual_reset );
    event->signaled = 0;
}

void set_event( struct event *event )
{
    event->signaled = 1;
    /* wake up all waiters if manual reset, a single one otherwise */
    wake_up( &event->obj, !event->manual_reset );
}

void reset_event( struct event *event )
{
    event->signaled = 0;
}

static void event_dump( struct object *obj, int verbose )
{
    struct event *event = (struct event *)obj;
    assert( obj->ops == &event_ops );
    fprintf( stderr, "Event manual=%d signaled=%d ",
             event->manual_reset, event->signaled );
    dump_object_name( &event->obj );
    fputc( '\n', stderr );
}

static int event_signaled( struct object *obj, struct thread *thread )
{
    struct event *event = (struct event *)obj;
    assert( obj->ops == &event_ops );
    return event->signaled;
}

static int event_satisfied( struct object *obj, struct thread *thread )
{
    struct event *event = (struct event *)obj;
    assert( obj->ops == &event_ops );
    /* Reset if it's an auto-reset event */
    if (!event->manual_reset) event->signaled = 0;
    return 0;  /* Not abandoned */
}

/* create an event */
DECL_HANDLER(create_event)
{
    struct event *event;

    reply->handle = 0;
    if ((event = create_event( get_req_data(), get_req_data_size(),
                               req->manual_reset, req->initial_state )))
    {
        reply->handle = alloc_handle( current->process, event, EVENT_ALL_ACCESS, req->inherit );
        release_object( event );
    }
}

/* open a handle to an event */
DECL_HANDLER(open_event)
{
    reply->handle = open_object( sync_namespace, get_req_data(), get_req_data_size(),
                                 &event_ops, req->access, req->inherit );
}

/* do an event operation */
DECL_HANDLER(event_op)
{
    struct event *event;

    if (!(event = get_event_obj( current->process, req->handle, EVENT_MODIFY_STATE ))) return;
    switch(req->op)
    {
    case PULSE_EVENT:
        pulse_event( event );
        break;
    case SET_EVENT:
        set_event( event );
        break;
    case RESET_EVENT:
        reset_event( event );
        break;
    default:
        fatal_protocol_error( current, "event_op: invalid operation %d\n", req->op );
    }
    release_object( event );
}
