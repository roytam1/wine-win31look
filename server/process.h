/*
 * Wine server processes
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

#ifndef __WINE_SERVER_PROCESS_H
#define __WINE_SERVER_PROCESS_H

#include "object.h"

struct msg_queue;
struct atom_table;
struct handle_table;
struct startup_info;

/* process startup state */
enum startup_state { STARTUP_IN_PROGRESS, STARTUP_DONE, STARTUP_ABORTED };

/* process structures */

struct process_dll
{
    struct process_dll  *next;            /* per-process dll list */
    struct process_dll  *prev;
    struct file         *file;            /* dll file */
    void                *base;            /* dll base address (in process addr space) */
    size_t               size;            /* dll size */
    void                *name;            /* ptr to ptr to name (in process addr space) */
    int                  dbg_offset;      /* debug info offset */
    int                  dbg_size;        /* debug info size */
    size_t               namelen;         /* length of dll file name */
    WCHAR               *filename;        /* dll file name */
};

struct process
{
    struct object        obj;             /* object header */
    struct process      *next;            /* system-wide process list */
    struct process      *prev;
    struct process      *parent;          /* parent process */
    struct thread       *thread_list;     /* head of the thread list */
    struct thread       *debugger;        /* thread debugging this process */
    struct handle_table *handles;         /* handle entries */
    struct fd           *msg_fd;          /* fd for sendmsg/recvmsg */
    process_id_t         id;              /* id of the process */
    process_id_t         group_id;        /* group id of the process */
    int                  exit_code;       /* process exit code */
    int                  running_threads; /* number of threads running in this process */
    struct timeval       start_time;      /* absolute time at process start */
    struct timeval       end_time;        /* absolute time at process end */
    int                  priority;        /* priority class */
    int                  affinity;        /* process affinity mask */
    int                  suspend;         /* global process suspend count */
    int                  create_flags;    /* process creation flags */
    struct list          locks;           /* list of file locks owned by the process */
    struct list          classes;         /* window classes owned by the process */
    struct console_input*console;         /* console input */
    enum startup_state   startup_state;   /* startup state */
    struct startup_info *startup_info;    /* startup info while init is in progress */
    struct event        *idle_event;      /* event for input idle */
    struct msg_queue    *queue;           /* main message queue */
    struct atom_table   *atom_table;      /* pointer to local atom table */
    struct token        *token;           /* security token associated with this process */
    struct process_dll   exe;             /* main exe file */
    void                *peb;             /* PEB address in client address space */
    void                *ldt_copy;        /* pointer to LDT copy in client addr space */
};

struct process_snapshot
{
    struct process *process;  /* process ptr */
    int             count;    /* process refcount */
    int             threads;  /* number of threads */
    int             priority; /* priority class */
    int             handles;  /* number of handles */
};

struct module_snapshot
{
    void           *base;     /* module base addr */
    size_t          size;     /* module size */
    size_t          namelen;  /* length of file name */
    WCHAR          *filename; /* module file name */
};

/* process functions */

extern unsigned int alloc_ptid( void *ptr );
extern void free_ptid( unsigned int id );
extern void *get_ptid_entry( unsigned int id );
extern struct thread *create_process( int fd );
extern struct process *get_process_from_id( process_id_t id );
extern struct process *get_process_from_handle( obj_handle_t handle, unsigned int access );
extern int process_set_debugger( struct process *process, struct thread *thread );
extern int debugger_detach( struct process* process, struct thread* debugger );
extern int set_process_debug_flag( struct process *process, int flag );

extern void add_process_thread( struct process *process,
                                struct thread *thread );
extern void remove_process_thread( struct process *process,
                                   struct thread *thread );
extern void suspend_process( struct process *process );
extern void resume_process( struct process *process );
extern void kill_all_processes( struct process *skip, int exit_code );
extern void kill_process( struct process *process, struct thread *skip, int exit_code );
extern void kill_console_processes( struct thread *renderer, int exit_code );
extern void kill_debugged_processes( struct thread *debugger, int exit_code );
extern void detach_debugged_processes( struct thread *debugger );
extern struct process_snapshot *process_snap( int *count );
extern struct module_snapshot *module_snap( struct process *process, int *count );
extern void enum_processes( int (*cb)(struct process*, void*), void *user);

inline static process_id_t get_process_id( struct process *process ) { return process->id; }

inline static int is_process_init_done( struct process *process )
{
    return process->startup_state == STARTUP_DONE;
}

#endif  /* __WINE_SERVER_PROCESS_H */
