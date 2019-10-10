/*
 * Server-side thread management
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
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "windef.h"
#include "winbase.h"

#include "file.h"
#include "handle.h"
#include "process.h"
#include "thread.h"
#include "request.h"
#include "user.h"


/* thread queues */

struct thread_wait
{
    struct thread_wait     *next;       /* next wait structure for this thread */
    struct thread          *thread;     /* owner thread */
    int                     count;      /* count of objects */
    int                     flags;
    void                   *cookie;     /* magic cookie to return to client */
    struct timeval          timeout;
    struct timeout_user    *user;
    struct wait_queue_entry queues[1];
};

/* asynchronous procedure calls */

struct thread_apc
{
    struct thread_apc  *next;     /* queue linked list */
    struct thread_apc  *prev;
    struct object      *owner;    /* object that queued this apc */
    void               *func;     /* function to call in client */
    enum apc_type       type;     /* type of apc function */
    int                 nb_args;  /* number of arguments */
    void               *arg1;     /* function arguments */
    void               *arg2;
    void               *arg3;
};


/* thread operations */

static void dump_thread( struct object *obj, int verbose );
static int thread_signaled( struct object *obj, struct thread *thread );
static void thread_poll_event( struct fd *fd, int event );
static void destroy_thread( struct object *obj );
static struct thread_apc *thread_dequeue_apc( struct thread *thread, int system_only );

static const struct object_ops thread_ops =
{
    sizeof(struct thread),      /* size */
    dump_thread,                /* dump */
    add_queue,                  /* add_queue */
    remove_queue,               /* remove_queue */
    thread_signaled,            /* signaled */
    no_satisfied,               /* satisfied */
    no_get_fd,                  /* get_fd */
    destroy_thread              /* destroy */
};

static const struct fd_ops thread_fd_ops =
{
    NULL,                       /* get_poll_events */
    thread_poll_event,          /* poll_event */
    no_flush,                   /* flush */
    no_get_file_info,           /* get_file_info */
    no_queue_async              /* queue_async */
};

static struct thread *first_thread;
static struct thread *booting_thread;

/* initialize the structure for a newly allocated thread */
inline static void init_thread_structure( struct thread *thread )
{
    int i;

    thread->unix_pid        = -1;  /* not known yet */
    thread->unix_tid        = -1;  /* not known yet */
    thread->context         = NULL;
    thread->teb             = NULL;
    thread->mutex           = NULL;
    thread->debug_ctx       = NULL;
    thread->debug_event     = NULL;
    thread->queue           = NULL;
    thread->wait            = NULL;
    thread->system_apc.head = NULL;
    thread->system_apc.tail = NULL;
    thread->user_apc.head   = NULL;
    thread->user_apc.tail   = NULL;
    thread->error           = 0;
    thread->req_data        = NULL;
    thread->req_toread      = 0;
    thread->reply_data      = NULL;
    thread->reply_towrite   = 0;
    thread->request_fd      = NULL;
    thread->reply_fd        = NULL;
    thread->wait_fd         = NULL;
    thread->state           = RUNNING;
    thread->attached        = 0;
    thread->exit_code       = 0;
    thread->next            = NULL;
    thread->prev            = NULL;
    thread->priority        = THREAD_PRIORITY_NORMAL;
    thread->affinity        = 1;
    thread->suspend         = 0;
    thread->creation_time   = time(NULL);
    thread->exit_time       = 0;

    for (i = 0; i < MAX_INFLIGHT_FDS; i++)
        thread->inflight[i].server = thread->inflight[i].client = -1;
}

/* create a new thread */
struct thread *create_thread( int fd, struct process *process )
{
    struct thread *thread;

    if (!(thread = alloc_object( &thread_ops ))) return NULL;

    init_thread_structure( thread );

    thread->process = (struct process *)grab_object( process );
    if (!current) current = thread;

    if (!booting_thread)  /* first thread ever */
    {
        booting_thread = thread;
        lock_master_socket(1);
    }

    if ((thread->next = first_thread) != NULL) thread->next->prev = thread;
    first_thread = thread;

    if (!(thread->id = alloc_ptid( thread )))
    {
        release_object( thread );
        return NULL;
    }
    if (!(thread->request_fd = create_anonymous_fd( &thread_fd_ops, fd, &thread->obj )))
    {
        release_object( thread );
        return NULL;
    }

    thread->token = (struct token *) grab_object( process->token );

    set_fd_events( thread->request_fd, POLLIN );  /* start listening to events */
    add_process_thread( thread->process, thread );
    return thread;
}

/* handle a client event */
static void thread_poll_event( struct fd *fd, int event )
{
    struct thread *thread = get_fd_user( fd );
    assert( thread->obj.ops == &thread_ops );

    if (event & (POLLERR | POLLHUP)) kill_thread( thread, 0 );
    else if (event & POLLIN) read_request( thread );
    else if (event & POLLOUT) write_reply( thread );
}

/* cleanup everything that is no longer needed by a dead thread */
/* used by destroy_thread and kill_thread */
static void cleanup_thread( struct thread *thread )
{
    int i;
    struct thread_apc *apc;

    while ((apc = thread_dequeue_apc( thread, 0 ))) free( apc );
    if (thread->req_data) free( thread->req_data );
    if (thread->reply_data) free( thread->reply_data );
    if (thread->request_fd) release_object( thread->request_fd );
    if (thread->reply_fd) release_object( thread->reply_fd );
    if (thread->wait_fd) release_object( thread->wait_fd );
    free_msg_queue( thread );
    cleanup_clipboard_thread(thread);
    destroy_thread_windows( thread );
    for (i = 0; i < MAX_INFLIGHT_FDS; i++)
    {
        if (thread->inflight[i].client != -1)
        {
            close( thread->inflight[i].server );
            thread->inflight[i].client = thread->inflight[i].server = -1;
        }
    }
    thread->req_data = NULL;
    thread->reply_data = NULL;
    thread->request_fd = NULL;
    thread->reply_fd = NULL;
    thread->wait_fd = NULL;

    if (thread == booting_thread)  /* killing booting thread */
    {
        booting_thread = NULL;
        lock_master_socket(0);
    }
}

/* destroy a thread when its refcount is 0 */
static void destroy_thread( struct object *obj )
{
    struct thread_apc *apc;
    struct thread *thread = (struct thread *)obj;
    assert( obj->ops == &thread_ops );

    assert( !thread->debug_ctx );  /* cannot still be debugging something */
    if (thread->next) thread->next->prev = thread->prev;
    if (thread->prev) thread->prev->next = thread->next;
    else first_thread = thread->next;
    while ((apc = thread_dequeue_apc( thread, 0 ))) free( apc );
    cleanup_thread( thread );
    release_object( thread->process );
    if (thread->id) free_ptid( thread->id );
    if (thread->token) release_object( thread->token );
}

/* dump a thread on stdout for debugging purposes */
static void dump_thread( struct object *obj, int verbose )
{
    struct thread *thread = (struct thread *)obj;
    assert( obj->ops == &thread_ops );

    fprintf( stderr, "Thread id=%04x unix pid=%d unix tid=%d teb=%p state=%d\n",
             thread->id, thread->unix_pid, thread->unix_tid, thread->teb, thread->state );
}

static int thread_signaled( struct object *obj, struct thread *thread )
{
    struct thread *mythread = (struct thread *)obj;
    return (mythread->state == TERMINATED);
}

/* get a thread pointer from a thread id (and increment the refcount) */
struct thread *get_thread_from_id( thread_id_t id )
{
    struct object *obj = get_ptid_entry( id );

    if (obj && obj->ops == &thread_ops) return (struct thread *)grab_object( obj );
    set_error( STATUS_INVALID_PARAMETER );
    return NULL;
}

/* get a thread from a handle (and increment the refcount) */
struct thread *get_thread_from_handle( obj_handle_t handle, unsigned int access )
{
    return (struct thread *)get_handle_obj( current->process, handle,
                                            access, &thread_ops );
}

/* find a thread from a Unix pid */
struct thread *get_thread_from_pid( int pid )
{
    struct thread *t;

    for (t = first_thread; t; t = t->next) if (t->unix_tid == pid) return t;
    for (t = first_thread; t; t = t->next) if (t->unix_pid == pid) return t;
    return NULL;
}

/* set all information about a thread */
static void set_thread_info( struct thread *thread,
                             const struct set_thread_info_request *req )
{
    if (req->mask & SET_THREAD_INFO_PRIORITY)
        thread->priority = req->priority;
    if (req->mask & SET_THREAD_INFO_AFFINITY)
    {
        if (req->affinity != 1) set_error( STATUS_INVALID_PARAMETER );
        else thread->affinity = req->affinity;
    }
}

/* stop a thread (at the Unix level) */
void stop_thread( struct thread *thread )
{
    /* can't stop a thread while initialisation is in progress */
    if (is_process_init_done(thread->process)) send_thread_signal( thread, SIGUSR1 );
}

/* suspend a thread */
static int suspend_thread( struct thread *thread )
{
    int old_count = thread->suspend;
    if (thread->suspend < MAXIMUM_SUSPEND_COUNT)
    {
        if (!(thread->process->suspend + thread->suspend++)) stop_thread( thread );
    }
    else set_error( STATUS_SUSPEND_COUNT_EXCEEDED );
    return old_count;
}

/* resume a thread */
static int resume_thread( struct thread *thread )
{
    int old_count = thread->suspend;
    if (thread->suspend > 0)
    {
        if (!(--thread->suspend + thread->process->suspend)) wake_thread( thread );
    }
    return old_count;
}

/* add a thread to an object wait queue; return 1 if OK, 0 on error */
int add_queue( struct object *obj, struct wait_queue_entry *entry )
{
    grab_object( obj );
    entry->obj    = obj;
    entry->prev   = obj->tail;
    entry->next   = NULL;
    if (obj->tail) obj->tail->next = entry;
    else obj->head = entry;
    obj->tail = entry;
    return 1;
}

/* remove a thread from an object wait queue */
void remove_queue( struct object *obj, struct wait_queue_entry *entry )
{
    if (entry->next) entry->next->prev = entry->prev;
    else obj->tail = entry->prev;
    if (entry->prev) entry->prev->next = entry->next;
    else obj->head = entry->next;
    release_object( obj );
}

/* finish waiting */
static void end_wait( struct thread *thread )
{
    struct thread_wait *wait = thread->wait;
    struct wait_queue_entry *entry;
    int i;

    assert( wait );
    for (i = 0, entry = wait->queues; i < wait->count; i++, entry++)
        entry->obj->ops->remove_queue( entry->obj, entry );
    if (wait->user) remove_timeout_user( wait->user );
    thread->wait = wait->next;
    free( wait );
}

/* build the thread wait structure */
static int wait_on( int count, struct object *objects[], int flags, const abs_time_t *timeout )
{
    struct thread_wait *wait;
    struct wait_queue_entry *entry;
    int i;

    if (!(wait = mem_alloc( sizeof(*wait) + (count-1) * sizeof(*entry) ))) return 0;
    wait->next    = current->wait;
    wait->thread  = current;
    wait->count   = count;
    wait->flags   = flags;
    wait->user    = NULL;
    current->wait = wait;
    if (flags & SELECT_TIMEOUT)
    {
        wait->timeout.tv_sec  = timeout->sec;
        wait->timeout.tv_usec = timeout->usec;
    }

    for (i = 0, entry = wait->queues; i < count; i++, entry++)
    {
        struct object *obj = objects[i];
        entry->thread = current;
        if (!obj->ops->add_queue( obj, entry ))
        {
            wait->count = i;
            end_wait( current );
            return 0;
        }
    }
    return 1;
}

/* check if the thread waiting condition is satisfied */
static int check_wait( struct thread *thread )
{
    int i, signaled;
    struct thread_wait *wait = thread->wait;
    struct wait_queue_entry *entry = wait->queues;

    /* Suspended threads may not acquire locks */
    if( thread->process->suspend + thread->suspend > 0 ) return -1;

    assert( wait );
    if (wait->flags & SELECT_ALL)
    {
        int not_ok = 0;
        /* Note: we must check them all anyway, as some objects may
         * want to do something when signaled, even if others are not */
        for (i = 0, entry = wait->queues; i < wait->count; i++, entry++)
            not_ok |= !entry->obj->ops->signaled( entry->obj, thread );
        if (not_ok) goto other_checks;
        /* Wait satisfied: tell it to all objects */
        signaled = 0;
        for (i = 0, entry = wait->queues; i < wait->count; i++, entry++)
            if (entry->obj->ops->satisfied( entry->obj, thread ))
                signaled = STATUS_ABANDONED_WAIT_0;
        return signaled;
    }
    else
    {
        for (i = 0, entry = wait->queues; i < wait->count; i++, entry++)
        {
            if (!entry->obj->ops->signaled( entry->obj, thread )) continue;
            /* Wait satisfied: tell it to the object */
            signaled = i;
            if (entry->obj->ops->satisfied( entry->obj, thread ))
                signaled = i + STATUS_ABANDONED_WAIT_0;
            return signaled;
        }
    }

 other_checks:
    if ((wait->flags & SELECT_INTERRUPTIBLE) && thread->system_apc.head) return STATUS_USER_APC;
    if ((wait->flags & SELECT_ALERTABLE) && thread->user_apc.head) return STATUS_USER_APC;
    if (wait->flags & SELECT_TIMEOUT)
    {
        struct timeval now;
        gettimeofday( &now, NULL );
        if (!time_before( &now, &wait->timeout )) return STATUS_TIMEOUT;
    }
    return -1;
}

/* send the wakeup signal to a thread */
static int send_thread_wakeup( struct thread *thread, void *cookie, int signaled )
{
    struct wake_up_reply reply;
    int ret;

    reply.cookie   = cookie;
    reply.signaled = signaled;
    if ((ret = write( get_unix_fd( thread->wait_fd ), &reply, sizeof(reply) )) == sizeof(reply))
        return 0;
    if (ret >= 0)
        fatal_protocol_error( thread, "partial wakeup write %d\n", ret );
    else if (errno == EPIPE)
        kill_thread( thread, 0 );  /* normal death */
    else
        fatal_protocol_perror( thread, "write" );
    return -1;
}

/* attempt to wake up a thread */
/* return >0 if OK, 0 if the wait condition is still not satisfied */
int wake_thread( struct thread *thread )
{
    int signaled, count;
    void *cookie;

    for (count = 0; thread->wait; count++)
    {
        if ((signaled = check_wait( thread )) == -1) break;

        cookie = thread->wait->cookie;
        if (debug_level) fprintf( stderr, "%04x: *wakeup* signaled=%d cookie=%p\n",
                                  thread->id, signaled, cookie );
        end_wait( thread );
        if (send_thread_wakeup( thread, cookie, signaled ) == -1) /* error */
	    break;
    }
    return count;
}

/* thread wait timeout */
static void thread_timeout( void *ptr )
{
    struct thread_wait *wait = ptr;
    struct thread *thread = wait->thread;
    void *cookie = wait->cookie;

    wait->user = NULL;
    if (thread->wait != wait) return; /* not the top-level wait, ignore it */
    if (thread->suspend + thread->process->suspend > 0) return;  /* suspended, ignore it */

    if (debug_level) fprintf( stderr, "%04x: *wakeup* signaled=%d cookie=%p\n",
                              thread->id, STATUS_TIMEOUT, cookie );
    end_wait( thread );
    if (send_thread_wakeup( thread, cookie, STATUS_TIMEOUT ) == -1) return;
    /* check if other objects have become signaled in the meantime */
    wake_thread( thread );
}

/* select on a list of handles */
static void select_on( int count, void *cookie, const obj_handle_t *handles,
                       int flags, const abs_time_t *timeout )
{
    int ret, i;
    struct object *objects[MAXIMUM_WAIT_OBJECTS];

    if ((count < 0) || (count > MAXIMUM_WAIT_OBJECTS))
    {
        set_error( STATUS_INVALID_PARAMETER );
        return;
    }
    for (i = 0; i < count; i++)
    {
        if (!(objects[i] = get_handle_obj( current->process, handles[i], SYNCHRONIZE, NULL )))
            break;
    }

    if (i < count) goto done;
    if (!wait_on( count, objects, flags, timeout )) goto done;

    if ((ret = check_wait( current )) != -1)
    {
        /* condition is already satisfied */
        end_wait( current );
        set_error( ret );
        goto done;
    }

    /* now we need to wait */
    if (flags & SELECT_TIMEOUT)
    {
        if (!(current->wait->user = add_timeout_user( &current->wait->timeout,
                                                      thread_timeout, current->wait )))
        {
            end_wait( current );
            goto done;
        }
    }
    current->wait->cookie = cookie;
    set_error( STATUS_PENDING );

done:
    while (--i >= 0) release_object( objects[i] );
}

/* attempt to wake threads sleeping on the object wait queue */
void wake_up( struct object *obj, int max )
{
    struct wait_queue_entry *entry = obj->head;

    while (entry)
    {
        struct thread *thread = entry->thread;
        entry = entry->next;
        if (wake_thread( thread ))
        {
            if (max && !--max) break;
        }
    }
}

/* queue an async procedure call */
int thread_queue_apc( struct thread *thread, struct object *owner, void *func,
                      enum apc_type type, int system, void *arg1, void *arg2, void *arg3 )
{
    struct thread_apc *apc;
    struct apc_queue *queue = system ? &thread->system_apc : &thread->user_apc;

    /* cancel a possible previous APC with the same owner */
    if (owner) thread_cancel_apc( thread, owner, system );
    if (thread->state == TERMINATED) return 0;

    if (!(apc = mem_alloc( sizeof(*apc) ))) return 0;
    apc->prev   = queue->tail;
    apc->next   = NULL;
    apc->owner  = owner;
    apc->func   = func;
    apc->type   = type;
    apc->arg1   = arg1;
    apc->arg2   = arg2;
    apc->arg3   = arg3;
    queue->tail = apc;
    if (!apc->prev)  /* first one */
    {
        queue->head = apc;
        wake_thread( thread );
    }
    else apc->prev->next = apc;

    return 1;
}

/* cancel the async procedure call owned by a specific object */
void thread_cancel_apc( struct thread *thread, struct object *owner, int system )
{
    struct thread_apc *apc;
    struct apc_queue *queue = system ? &thread->system_apc : &thread->user_apc;
    for (apc = queue->head; apc; apc = apc->next)
    {
        if (apc->owner != owner) continue;
        if (apc->next) apc->next->prev = apc->prev;
        else queue->tail = apc->prev;
        if (apc->prev) apc->prev->next = apc->next;
        else queue->head = apc->next;
        free( apc );
        return;
    }
}

/* remove the head apc from the queue; the returned pointer must be freed by the caller */
static struct thread_apc *thread_dequeue_apc( struct thread *thread, int system_only )
{
    struct thread_apc *apc;
    struct apc_queue *queue = &thread->system_apc;

    if (!queue->head && !system_only) queue = &thread->user_apc;
    if ((apc = queue->head))
    {
        if (apc->next) apc->next->prev = NULL;
        else queue->tail = NULL;
        queue->head = apc->next;
    }
    return apc;
}

/* add an fd to the inflight list */
/* return list index, or -1 on error */
int thread_add_inflight_fd( struct thread *thread, int client, int server )
{
    int i;

    if (server == -1) return -1;
    if (client == -1)
    {
        close( server );
        return -1;
    }

    /* first check if we already have an entry for this fd */
    for (i = 0; i < MAX_INFLIGHT_FDS; i++)
        if (thread->inflight[i].client == client)
        {
            close( thread->inflight[i].server );
            thread->inflight[i].server = server;
            return i;
        }

    /* now find a free spot to store it */
    for (i = 0; i < MAX_INFLIGHT_FDS; i++)
        if (thread->inflight[i].client == -1)
        {
            thread->inflight[i].client = client;
            thread->inflight[i].server = server;
            return i;
        }
    return -1;
}

/* get an inflight fd and purge it from the list */
/* the fd must be closed when no longer used */
int thread_get_inflight_fd( struct thread *thread, int client )
{
    int i, ret;

    if (client == -1) return -1;

    do
    {
        for (i = 0; i < MAX_INFLIGHT_FDS; i++)
        {
            if (thread->inflight[i].client == client)
            {
                ret = thread->inflight[i].server;
                thread->inflight[i].server = thread->inflight[i].client = -1;
                return ret;
            }
        }
    } while (!receive_fd( thread->process ));  /* in case it is still in the socket buffer */
    return -1;
}

/* retrieve an LDT selector entry */
static void get_selector_entry( struct thread *thread, int entry,
                                unsigned int *base, unsigned int *limit,
                                unsigned char *flags )
{
    if (!thread->process->ldt_copy)
    {
        set_error( STATUS_ACCESS_DENIED );
        return;
    }
    if (entry >= 8192)
    {
        set_error( STATUS_INVALID_PARAMETER );  /* FIXME */
        return;
    }
    if (suspend_for_ptrace( thread ))
    {
        unsigned char flags_buf[4];
        int *addr = (int *)thread->process->ldt_copy + entry;
        if (read_thread_int( thread, addr, base ) == -1) goto done;
        if (read_thread_int( thread, addr + 8192, limit ) == -1) goto done;
        addr = (int *)thread->process->ldt_copy + 2*8192 + (entry >> 2);
        if (read_thread_int( thread, addr, (int *)flags_buf ) == -1) goto done;
        *flags = flags_buf[entry & 3];
    done:
        resume_after_ptrace( thread );
    }
}

/* kill a thread on the spot */
void kill_thread( struct thread *thread, int violent_death )
{
    if (thread->state == TERMINATED) return;  /* already killed */
    thread->state = TERMINATED;
    thread->exit_time = time(NULL);
    if (current == thread) current = NULL;
    if (debug_level)
        fprintf( stderr,"%04x: *killed* exit_code=%d\n",
                 thread->id, thread->exit_code );
    if (thread->wait)
    {
        while (thread->wait) end_wait( thread );
        send_thread_wakeup( thread, NULL, STATUS_PENDING );
        /* if it is waiting on the socket, we don't need to send a SIGTERM */
        violent_death = 0;
    }
    kill_console_processes( thread, 0 );
    debug_exit_thread( thread );
    abandon_mutexes( thread );
    remove_process_thread( thread->process, thread );
    wake_up( &thread->obj, 0 );
    detach_thread( thread, violent_death ? SIGTERM : 0 );
    cleanup_thread( thread );
    release_object( thread );
}

/* take a snapshot of currently running threads */
struct thread_snapshot *thread_snap( int *count )
{
    struct thread_snapshot *snapshot, *ptr;
    struct thread *thread;
    int total = 0;

    for (thread = first_thread; thread; thread = thread->next)
        if (thread->state != TERMINATED) total++;
    if (!total || !(snapshot = mem_alloc( sizeof(*snapshot) * total ))) return NULL;
    ptr = snapshot;
    for (thread = first_thread; thread; thread = thread->next)
    {
        if (thread->state == TERMINATED) continue;
        ptr->thread   = thread;
        ptr->count    = thread->obj.refcount;
        ptr->priority = thread->priority;
        grab_object( thread );
        ptr++;
    }
    *count = total;
    return snapshot;
}

/* signal that we are finished booting on the client side */
DECL_HANDLER(boot_done)
{
    debug_level = max( debug_level, req->debug_level );
    if (current == booting_thread)
    {
        booting_thread = (struct thread *)~0UL;  /* make sure it doesn't match other threads */
        lock_master_socket(0);  /* allow other clients now */
    }
}

/* create a new thread */
DECL_HANDLER(new_thread)
{
    struct thread *thread;
    int request_fd = thread_get_inflight_fd( current, req->request_fd );

    if (request_fd == -1 || fcntl( request_fd, F_SETFL, O_NONBLOCK ) == -1)
    {
        if (request_fd != -1) close( request_fd );
        set_error( STATUS_INVALID_HANDLE );
        return;
    }

    if ((thread = create_thread( request_fd, current->process )))
    {
        if (req->suspend) thread->suspend++;
        reply->tid = get_thread_id( thread );
        if ((reply->handle = alloc_handle( current->process, thread,
                                           THREAD_ALL_ACCESS, req->inherit )))
        {
            /* thread object will be released when the thread gets killed */
            return;
        }
        kill_thread( thread, 1 );
    }
}

/* initialize a new thread */
DECL_HANDLER(init_thread)
{
    int reply_fd = thread_get_inflight_fd( current, req->reply_fd );
    int wait_fd = thread_get_inflight_fd( current, req->wait_fd );

    if (current->unix_pid != -1)
    {
        fatal_protocol_error( current, "init_thread: already running\n" );
        goto error;
    }
    if (reply_fd == -1 || fcntl( reply_fd, F_SETFL, O_NONBLOCK ) == -1)
    {
        fatal_protocol_error( current, "bad reply fd\n" );
        goto error;
    }
    if (wait_fd == -1)
    {
        fatal_protocol_error( current, "bad wait fd\n" );
        goto error;
    }
    if (!(current->reply_fd = create_anonymous_fd( &thread_fd_ops, reply_fd, &current->obj )))
    {
        reply_fd = -1;
        fatal_protocol_error( current, "could not allocate reply fd\n" );
        goto error;
    }
    if (!(current->wait_fd  = create_anonymous_fd( &thread_fd_ops, wait_fd, &current->obj )))
        return;

    current->unix_pid = req->unix_pid;
    current->unix_tid = req->unix_tid;
    current->teb      = req->teb;

    if (current->suspend + current->process->suspend > 0) stop_thread( current );
    if (current->process->running_threads > 1)
        generate_debug_event( current, CREATE_THREAD_DEBUG_EVENT, req->entry );

    reply->pid     = get_process_id( current->process );
    reply->tid     = get_thread_id( current );
    reply->boot    = (current == booting_thread);
    reply->version = SERVER_PROTOCOL_VERSION;
    return;

 error:
    if (reply_fd != -1) close( reply_fd );
    if (wait_fd != -1) close( wait_fd );
}

/* terminate a thread */
DECL_HANDLER(terminate_thread)
{
    struct thread *thread;

    reply->self = 0;
    reply->last = 0;
    if ((thread = get_thread_from_handle( req->handle, THREAD_TERMINATE )))
    {
        thread->exit_code = req->exit_code;
        if (thread != current) kill_thread( thread, 1 );
        else
        {
            reply->self = 1;
            reply->last = (thread->process->running_threads == 1);
        }
        release_object( thread );
    }
}

/* open a handle to a thread */
DECL_HANDLER(open_thread)
{
    struct thread *thread = get_thread_from_id( req->tid );

    reply->handle = 0;
    if (thread)
    {
        reply->handle = alloc_handle( current->process, thread, req->access, req->inherit );
        release_object( thread );
    }
}

/* fetch information about a thread */
DECL_HANDLER(get_thread_info)
{
    struct thread *thread;
    obj_handle_t handle = req->handle;

    if (!handle) thread = get_thread_from_id( req->tid_in );
    else thread = get_thread_from_handle( req->handle, THREAD_QUERY_INFORMATION );

    if (thread)
    {
        reply->pid            = get_process_id( thread->process );
        reply->tid            = get_thread_id( thread );
        reply->teb            = thread->teb;
        reply->exit_code      = (thread->state == TERMINATED) ? thread->exit_code : STILL_ACTIVE;
        reply->priority       = thread->priority;
        reply->affinity       = thread->affinity;
        reply->creation_time  = thread->creation_time;
        reply->exit_time      = thread->exit_time;

        release_object( thread );
    }
}

/* set information about a thread */
DECL_HANDLER(set_thread_info)
{
    struct thread *thread;

    if ((thread = get_thread_from_handle( req->handle, THREAD_SET_INFORMATION )))
    {
        set_thread_info( thread, req );
        release_object( thread );
    }
}

/* suspend a thread */
DECL_HANDLER(suspend_thread)
{
    struct thread *thread;

    if ((thread = get_thread_from_handle( req->handle, THREAD_SUSPEND_RESUME )))
    {
        if (thread->state == TERMINATED) set_error( STATUS_ACCESS_DENIED );
        else reply->count = suspend_thread( thread );
        release_object( thread );
    }
}

/* resume a thread */
DECL_HANDLER(resume_thread)
{
    struct thread *thread;

    if ((thread = get_thread_from_handle( req->handle, THREAD_SUSPEND_RESUME )))
    {
        if (thread->state == TERMINATED) set_error( STATUS_ACCESS_DENIED );
        else reply->count = resume_thread( thread );
        release_object( thread );
    }
}

/* select on a handle list */
DECL_HANDLER(select)
{
    int count = get_req_data_size() / sizeof(int);
    select_on( count, req->cookie, get_req_data(), req->flags, &req->timeout );
}

/* queue an APC for a thread */
DECL_HANDLER(queue_apc)
{
    struct thread *thread;
    if ((thread = get_thread_from_handle( req->handle, THREAD_SET_CONTEXT )))
    {
        thread_queue_apc( thread, NULL, req->func, APC_USER, !req->user,
                          req->arg1, req->arg2, req->arg3 );
        release_object( thread );
    }
}

/* get next APC to call */
DECL_HANDLER(get_apc)
{
    struct thread_apc *apc;

    for (;;)
    {
        if (!(apc = thread_dequeue_apc( current, !req->alertable )))
        {
            /* no more APCs */
            reply->func = NULL;
            reply->type = APC_NONE;
            return;
        }
        /* Optimization: ignore APCs that have a NULL func; they are only used
         * to wake up a thread, but since we got here the thread woke up already.
         * Exception: for APC_ASYNC_IO, func == NULL is legal.
         */
        if (apc->func || apc->type == APC_ASYNC_IO) break;
        free( apc );
    }
    reply->func = apc->func;
    reply->type = apc->type;
    reply->arg1 = apc->arg1;
    reply->arg2 = apc->arg2;
    reply->arg3 = apc->arg3;
    free( apc );
}

/* fetch a selector entry for a thread */
DECL_HANDLER(get_selector_entry)
{
    struct thread *thread;
    if ((thread = get_thread_from_handle( req->handle, THREAD_QUERY_INFORMATION )))
    {
        get_selector_entry( thread, req->entry, &reply->base, &reply->limit, &reply->flags );
        release_object( thread );
    }
}
