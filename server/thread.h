/*
 * Wine server threads
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

#ifndef __WINE_SERVER_THREAD_H
#define __WINE_SERVER_THREAD_H

#include "object.h"
#include "ntstatus.h"

/* thread structure */

struct process;
struct thread_wait;
struct thread_apc;
struct mutex;
struct debug_ctx;
struct debug_event;
struct msg_queue;

enum run_state
{
    RUNNING,    /* running normally */
    TERMINATED  /* terminated */
};

struct apc_queue
{
    struct thread_apc *head;
    struct thread_apc *tail;
};

/* descriptor for fds currently in flight from client to server */
struct inflight_fd
{
    int client;  /* fd on the client side (or -1 if entry is free) */
    int server;  /* fd on the server side */
};
#define MAX_INFLIGHT_FDS 16  /* max number of fds in flight per thread */

struct thread
{
    struct object          obj;           /* object header */
    struct thread         *next;          /* system-wide thread list */
    struct thread         *prev;
    struct thread         *proc_next;     /* per-process thread list */
    struct thread         *proc_prev;
    struct process        *process;
    thread_id_t            id;            /* thread id */
    struct mutex          *mutex;         /* list of currently owned mutexes */
    struct debug_ctx      *debug_ctx;     /* debugger context if this thread is a debugger */
    struct debug_event    *debug_event;   /* debug event being sent to debugger */
    struct msg_queue      *queue;         /* message queue */
    struct thread_wait    *wait;          /* current wait condition if sleeping */
    struct apc_queue       system_apc;    /* queue of system async procedure calls */
    struct apc_queue       user_apc;      /* queue of user async procedure calls */
    struct inflight_fd     inflight[MAX_INFLIGHT_FDS];  /* fds currently in flight */
    unsigned int           error;         /* current error code */
    union generic_request  req;           /* current request */
    void                  *req_data;      /* variable-size data for request */
    unsigned int           req_toread;    /* amount of data still to read in request */
    void                  *reply_data;    /* variable-size data for reply */
    unsigned int           reply_size;    /* size of reply data */
    unsigned int           reply_towrite; /* amount of data still to write in reply */
    struct fd             *request_fd;    /* fd for receiving client requests */
    struct fd             *reply_fd;      /* fd to send a reply to a client */
    struct fd             *wait_fd;       /* fd to use to wake a sleeping client */
    enum run_state         state;         /* running state */
    int                    attached;      /* is thread attached with ptrace? */
    int                    exit_code;     /* thread exit code */
    int                    unix_pid;      /* Unix pid of client */
    int                    unix_tid;      /* Unix tid of client */
    CONTEXT               *context;       /* current context if in an exception handler */
    void                  *teb;           /* TEB address (in client address space) */
    int                    priority;      /* priority level */
    int                    affinity;      /* affinity mask */
    int                    suspend;       /* suspend count */
    time_t                 creation_time; /* Thread creation time */
    time_t                 exit_time;     /* Thread exit time */
    struct token          *token;         /* security token associated with this thread */
};

struct thread_snapshot
{
    struct thread  *thread;    /* thread ptr */
    int             count;     /* thread refcount */
    int             priority;  /* priority class */
};

extern struct thread *current;

/* thread functions */

extern struct thread *create_thread( int fd, struct process *process );
extern struct thread *get_thread_from_id( thread_id_t id );
extern struct thread *get_thread_from_handle( obj_handle_t handle, unsigned int access );
extern struct thread *get_thread_from_pid( int pid );
extern void stop_thread( struct thread *thread );
extern int wake_thread( struct thread *thread );
extern int add_queue( struct object *obj, struct wait_queue_entry *entry );
extern void remove_queue( struct object *obj, struct wait_queue_entry *entry );
extern void kill_thread( struct thread *thread, int violent_death );
extern void wake_up( struct object *obj, int max );
extern int thread_queue_apc( struct thread *thread, struct object *owner, void *func,
                             enum apc_type type, int system, void *arg1, void *arg2, void *arg3 );
extern void thread_cancel_apc( struct thread *thread, struct object *owner, int system );
extern int thread_add_inflight_fd( struct thread *thread, int client, int server );
extern int thread_get_inflight_fd( struct thread *thread, int client );
extern struct thread_snapshot *thread_snap( int *count );

/* ptrace functions */

extern void sigchld_callback(void);
extern int get_ptrace_pid( struct thread *thread );
extern void detach_thread( struct thread *thread, int sig );
extern int attach_process( struct process *process );
extern void detach_process( struct process *process );
extern int suspend_for_ptrace( struct thread *thread );
extern void resume_after_ptrace( struct thread *thread );
extern int read_thread_int( struct thread *thread, const int *addr, int *data );
extern int write_thread_int( struct thread *thread, int *addr, int data, unsigned int mask );
extern void *get_thread_ip( struct thread *thread );
extern int get_thread_single_step( struct thread *thread );
extern int tkill( int pid, int sig );
extern int send_thread_signal( struct thread *thread, int sig );

extern unsigned int global_error;  /* global error code for when no thread is current */

static inline unsigned int get_error(void)       { return current ? current->error : global_error; }
static inline void set_error( unsigned int err ) { global_error = err; if (current) current->error = err; }
static inline void clear_error(void)             { set_error(0); }
static inline void set_win32_error( unsigned int err ) { set_error( 0xc0010000 | err ); }

static inline thread_id_t get_thread_id( struct thread *thread ) { return thread->id; }

#endif  /* __WINE_SERVER_THREAD_H */
