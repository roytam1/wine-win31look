/*
 * Wine server requests
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

#ifndef __WINE_SERVER_REQUEST_H
#define __WINE_SERVER_REQUEST_H

#include <assert.h>

#include "thread.h"
#include "wine/server_protocol.h"

/* max request length */
#define MAX_REQUEST_LENGTH  8192

/* request handler definition */
#define DECL_HANDLER(name) \
    void req_##name( const struct name##_request *req, struct name##_reply *reply )

/* request functions */

#ifdef __GNUC__
extern void fatal_protocol_error( struct thread *thread,
                                  const char *err, ... ) __attribute__((format (printf,2,3)));
extern void fatal_protocol_perror( struct thread *thread,
                                   const char *err, ... ) __attribute__((format (printf,2,3)));
extern void fatal_error( const char *err, ... )  __attribute__((noreturn,format(printf,1,2)));
extern void fatal_perror( const char *err, ... )  __attribute__((noreturn,format(printf,1,2)));
#else
extern void fatal_protocol_error( struct thread *thread, const char *err, ... );
extern void fatal_protocol_perror( struct thread *thread, const char *err, ... );
extern void fatal_error( const char *err, ... );
extern void fatal_perror( const char *err, ... );
#endif

extern const char *get_config_dir(void);
extern void *set_reply_data_size( size_t size );
extern int receive_fd( struct process *process );
extern int send_client_fd( struct process *process, int fd, obj_handle_t handle );
extern void read_request( struct thread *thread );
extern void write_reply( struct thread *thread );
extern unsigned int get_tick_count(void);
extern void open_master_socket(void);
extern void close_master_socket(void);
extern void lock_master_socket( int locked );
extern int wait_for_lock(void);
extern int kill_lock_owner( int sig );

extern void trace_request(void);
extern void trace_reply( enum request req, const union generic_reply *reply );

/* get the request vararg data */
inline static const void *get_req_data(void)
{
    return current->req_data;
}

/* get the request vararg size */
inline static size_t get_req_data_size(void)
{
    return current->req.request_header.request_size;
}

/* get the reply maximum vararg size */
inline static size_t get_reply_max_size(void)
{
    return current->req.request_header.reply_size;
}

/* allocate and fill the reply data */
inline static void *set_reply_data( const void *data, size_t size )
{
    void *ret = set_reply_data_size( size );
    if (ret) memcpy( ret, data, size );
    return ret;
}

/* set the reply data pointer directly (will be freed by request code) */
inline static void set_reply_data_ptr( void *data, size_t size )
{
    assert( size <= get_reply_max_size() );
    current->reply_size = size;
    current->reply_data = data;
}


/* Everything below this line is generated automatically by tools/make_requests */
/* ### make_requests begin ### */

DECL_HANDLER(new_process);
DECL_HANDLER(get_new_process_info);
DECL_HANDLER(new_thread);
DECL_HANDLER(boot_done);
DECL_HANDLER(init_process);
DECL_HANDLER(get_startup_info);
DECL_HANDLER(init_process_done);
DECL_HANDLER(init_thread);
DECL_HANDLER(terminate_process);
DECL_HANDLER(terminate_thread);
DECL_HANDLER(get_process_info);
DECL_HANDLER(set_process_info);
DECL_HANDLER(get_thread_info);
DECL_HANDLER(set_thread_info);
DECL_HANDLER(get_dll_info);
DECL_HANDLER(suspend_thread);
DECL_HANDLER(resume_thread);
DECL_HANDLER(load_dll);
DECL_HANDLER(unload_dll);
DECL_HANDLER(queue_apc);
DECL_HANDLER(get_apc);
DECL_HANDLER(close_handle);
DECL_HANDLER(set_handle_info);
DECL_HANDLER(dup_handle);
DECL_HANDLER(open_process);
DECL_HANDLER(open_thread);
DECL_HANDLER(select);
DECL_HANDLER(create_event);
DECL_HANDLER(event_op);
DECL_HANDLER(open_event);
DECL_HANDLER(create_mutex);
DECL_HANDLER(release_mutex);
DECL_HANDLER(open_mutex);
DECL_HANDLER(create_semaphore);
DECL_HANDLER(release_semaphore);
DECL_HANDLER(open_semaphore);
DECL_HANDLER(create_file);
DECL_HANDLER(alloc_file_handle);
DECL_HANDLER(get_handle_fd);
DECL_HANDLER(set_file_pointer);
DECL_HANDLER(truncate_file);
DECL_HANDLER(flush_file);
DECL_HANDLER(get_file_info);
DECL_HANDLER(lock_file);
DECL_HANDLER(unlock_file);
DECL_HANDLER(create_socket);
DECL_HANDLER(accept_socket);
DECL_HANDLER(set_socket_event);
DECL_HANDLER(get_socket_event);
DECL_HANDLER(enable_socket_event);
DECL_HANDLER(set_socket_deferred);
DECL_HANDLER(alloc_console);
DECL_HANDLER(free_console);
DECL_HANDLER(get_console_renderer_events);
DECL_HANDLER(open_console);
DECL_HANDLER(get_console_wait_event);
DECL_HANDLER(get_console_mode);
DECL_HANDLER(set_console_mode);
DECL_HANDLER(set_console_input_info);
DECL_HANDLER(get_console_input_info);
DECL_HANDLER(append_console_input_history);
DECL_HANDLER(get_console_input_history);
DECL_HANDLER(create_console_output);
DECL_HANDLER(set_console_output_info);
DECL_HANDLER(get_console_output_info);
DECL_HANDLER(write_console_input);
DECL_HANDLER(read_console_input);
DECL_HANDLER(write_console_output);
DECL_HANDLER(fill_console_output);
DECL_HANDLER(read_console_output);
DECL_HANDLER(move_console_output);
DECL_HANDLER(send_console_signal);
DECL_HANDLER(create_change_notification);
DECL_HANDLER(next_change_notification);
DECL_HANDLER(create_mapping);
DECL_HANDLER(open_mapping);
DECL_HANDLER(get_mapping_info);
DECL_HANDLER(create_snapshot);
DECL_HANDLER(next_process);
DECL_HANDLER(next_thread);
DECL_HANDLER(next_module);
DECL_HANDLER(wait_debug_event);
DECL_HANDLER(queue_exception_event);
DECL_HANDLER(get_exception_status);
DECL_HANDLER(output_debug_string);
DECL_HANDLER(continue_debug_event);
DECL_HANDLER(debug_process);
DECL_HANDLER(debug_break);
DECL_HANDLER(set_debugger_kill_on_exit);
DECL_HANDLER(read_process_memory);
DECL_HANDLER(write_process_memory);
DECL_HANDLER(create_key);
DECL_HANDLER(open_key);
DECL_HANDLER(delete_key);
DECL_HANDLER(flush_key);
DECL_HANDLER(enum_key);
DECL_HANDLER(set_key_value);
DECL_HANDLER(get_key_value);
DECL_HANDLER(enum_key_value);
DECL_HANDLER(delete_key_value);
DECL_HANDLER(load_registry);
DECL_HANDLER(unload_registry);
DECL_HANDLER(save_registry);
DECL_HANDLER(save_registry_atexit);
DECL_HANDLER(set_registry_levels);
DECL_HANDLER(set_registry_notification);
DECL_HANDLER(create_timer);
DECL_HANDLER(open_timer);
DECL_HANDLER(set_timer);
DECL_HANDLER(cancel_timer);
DECL_HANDLER(get_thread_context);
DECL_HANDLER(set_thread_context);
DECL_HANDLER(get_selector_entry);
DECL_HANDLER(add_atom);
DECL_HANDLER(delete_atom);
DECL_HANDLER(find_atom);
DECL_HANDLER(get_atom_name);
DECL_HANDLER(init_atom_table);
DECL_HANDLER(get_msg_queue);
DECL_HANDLER(set_queue_mask);
DECL_HANDLER(get_queue_status);
DECL_HANDLER(wait_input_idle);
DECL_HANDLER(send_message);
DECL_HANDLER(get_message);
DECL_HANDLER(reply_message);
DECL_HANDLER(get_message_reply);
DECL_HANDLER(set_win_timer);
DECL_HANDLER(kill_win_timer);
DECL_HANDLER(get_serial_info);
DECL_HANDLER(set_serial_info);
DECL_HANDLER(register_async);
DECL_HANDLER(create_named_pipe);
DECL_HANDLER(open_named_pipe);
DECL_HANDLER(connect_named_pipe);
DECL_HANDLER(wait_named_pipe);
DECL_HANDLER(disconnect_named_pipe);
DECL_HANDLER(get_named_pipe_info);
DECL_HANDLER(create_smb);
DECL_HANDLER(get_smb_info);
DECL_HANDLER(create_window);
DECL_HANDLER(link_window);
DECL_HANDLER(destroy_window);
DECL_HANDLER(set_window_owner);
DECL_HANDLER(get_window_info);
DECL_HANDLER(set_window_info);
DECL_HANDLER(get_window_parents);
DECL_HANDLER(get_window_children);
DECL_HANDLER(get_window_tree);
DECL_HANDLER(set_window_rectangles);
DECL_HANDLER(get_window_rectangles);
DECL_HANDLER(get_window_text);
DECL_HANDLER(set_window_text);
DECL_HANDLER(inc_window_paint_count);
DECL_HANDLER(get_windows_offset);
DECL_HANDLER(set_window_property);
DECL_HANDLER(remove_window_property);
DECL_HANDLER(get_window_property);
DECL_HANDLER(get_window_properties);
DECL_HANDLER(attach_thread_input);
DECL_HANDLER(get_thread_input);
DECL_HANDLER(get_key_state);
DECL_HANDLER(set_key_state);
DECL_HANDLER(set_foreground_window);
DECL_HANDLER(set_focus_window);
DECL_HANDLER(set_active_window);
DECL_HANDLER(set_capture_window);
DECL_HANDLER(set_caret_window);
DECL_HANDLER(set_caret_info);
DECL_HANDLER(set_hook);
DECL_HANDLER(remove_hook);
DECL_HANDLER(start_hook_chain);
DECL_HANDLER(finish_hook_chain);
DECL_HANDLER(get_next_hook);
DECL_HANDLER(create_class);
DECL_HANDLER(destroy_class);
DECL_HANDLER(set_class_info);
DECL_HANDLER(set_clipboard_info);
DECL_HANDLER(open_token);
DECL_HANDLER(set_global_windows);

#ifdef WANT_REQUEST_HANDLERS

typedef void (*req_handler)( const void *req, void *reply );
static const req_handler req_handlers[REQ_NB_REQUESTS] =
{
    (req_handler)req_new_process,
    (req_handler)req_get_new_process_info,
    (req_handler)req_new_thread,
    (req_handler)req_boot_done,
    (req_handler)req_init_process,
    (req_handler)req_get_startup_info,
    (req_handler)req_init_process_done,
    (req_handler)req_init_thread,
    (req_handler)req_terminate_process,
    (req_handler)req_terminate_thread,
    (req_handler)req_get_process_info,
    (req_handler)req_set_process_info,
    (req_handler)req_get_thread_info,
    (req_handler)req_set_thread_info,
    (req_handler)req_get_dll_info,
    (req_handler)req_suspend_thread,
    (req_handler)req_resume_thread,
    (req_handler)req_load_dll,
    (req_handler)req_unload_dll,
    (req_handler)req_queue_apc,
    (req_handler)req_get_apc,
    (req_handler)req_close_handle,
    (req_handler)req_set_handle_info,
    (req_handler)req_dup_handle,
    (req_handler)req_open_process,
    (req_handler)req_open_thread,
    (req_handler)req_select,
    (req_handler)req_create_event,
    (req_handler)req_event_op,
    (req_handler)req_open_event,
    (req_handler)req_create_mutex,
    (req_handler)req_release_mutex,
    (req_handler)req_open_mutex,
    (req_handler)req_create_semaphore,
    (req_handler)req_release_semaphore,
    (req_handler)req_open_semaphore,
    (req_handler)req_create_file,
    (req_handler)req_alloc_file_handle,
    (req_handler)req_get_handle_fd,
    (req_handler)req_set_file_pointer,
    (req_handler)req_truncate_file,
    (req_handler)req_flush_file,
    (req_handler)req_get_file_info,
    (req_handler)req_lock_file,
    (req_handler)req_unlock_file,
    (req_handler)req_create_socket,
    (req_handler)req_accept_socket,
    (req_handler)req_set_socket_event,
    (req_handler)req_get_socket_event,
    (req_handler)req_enable_socket_event,
    (req_handler)req_set_socket_deferred,
    (req_handler)req_alloc_console,
    (req_handler)req_free_console,
    (req_handler)req_get_console_renderer_events,
    (req_handler)req_open_console,
    (req_handler)req_get_console_wait_event,
    (req_handler)req_get_console_mode,
    (req_handler)req_set_console_mode,
    (req_handler)req_set_console_input_info,
    (req_handler)req_get_console_input_info,
    (req_handler)req_append_console_input_history,
    (req_handler)req_get_console_input_history,
    (req_handler)req_create_console_output,
    (req_handler)req_set_console_output_info,
    (req_handler)req_get_console_output_info,
    (req_handler)req_write_console_input,
    (req_handler)req_read_console_input,
    (req_handler)req_write_console_output,
    (req_handler)req_fill_console_output,
    (req_handler)req_read_console_output,
    (req_handler)req_move_console_output,
    (req_handler)req_send_console_signal,
    (req_handler)req_create_change_notification,
    (req_handler)req_next_change_notification,
    (req_handler)req_create_mapping,
    (req_handler)req_open_mapping,
    (req_handler)req_get_mapping_info,
    (req_handler)req_create_snapshot,
    (req_handler)req_next_process,
    (req_handler)req_next_thread,
    (req_handler)req_next_module,
    (req_handler)req_wait_debug_event,
    (req_handler)req_queue_exception_event,
    (req_handler)req_get_exception_status,
    (req_handler)req_output_debug_string,
    (req_handler)req_continue_debug_event,
    (req_handler)req_debug_process,
    (req_handler)req_debug_break,
    (req_handler)req_set_debugger_kill_on_exit,
    (req_handler)req_read_process_memory,
    (req_handler)req_write_process_memory,
    (req_handler)req_create_key,
    (req_handler)req_open_key,
    (req_handler)req_delete_key,
    (req_handler)req_flush_key,
    (req_handler)req_enum_key,
    (req_handler)req_set_key_value,
    (req_handler)req_get_key_value,
    (req_handler)req_enum_key_value,
    (req_handler)req_delete_key_value,
    (req_handler)req_load_registry,
    (req_handler)req_unload_registry,
    (req_handler)req_save_registry,
    (req_handler)req_save_registry_atexit,
    (req_handler)req_set_registry_levels,
    (req_handler)req_set_registry_notification,
    (req_handler)req_create_timer,
    (req_handler)req_open_timer,
    (req_handler)req_set_timer,
    (req_handler)req_cancel_timer,
    (req_handler)req_get_thread_context,
    (req_handler)req_set_thread_context,
    (req_handler)req_get_selector_entry,
    (req_handler)req_add_atom,
    (req_handler)req_delete_atom,
    (req_handler)req_find_atom,
    (req_handler)req_get_atom_name,
    (req_handler)req_init_atom_table,
    (req_handler)req_get_msg_queue,
    (req_handler)req_set_queue_mask,
    (req_handler)req_get_queue_status,
    (req_handler)req_wait_input_idle,
    (req_handler)req_send_message,
    (req_handler)req_get_message,
    (req_handler)req_reply_message,
    (req_handler)req_get_message_reply,
    (req_handler)req_set_win_timer,
    (req_handler)req_kill_win_timer,
    (req_handler)req_get_serial_info,
    (req_handler)req_set_serial_info,
    (req_handler)req_register_async,
    (req_handler)req_create_named_pipe,
    (req_handler)req_open_named_pipe,
    (req_handler)req_connect_named_pipe,
    (req_handler)req_wait_named_pipe,
    (req_handler)req_disconnect_named_pipe,
    (req_handler)req_get_named_pipe_info,
    (req_handler)req_create_smb,
    (req_handler)req_get_smb_info,
    (req_handler)req_create_window,
    (req_handler)req_link_window,
    (req_handler)req_destroy_window,
    (req_handler)req_set_window_owner,
    (req_handler)req_get_window_info,
    (req_handler)req_set_window_info,
    (req_handler)req_get_window_parents,
    (req_handler)req_get_window_children,
    (req_handler)req_get_window_tree,
    (req_handler)req_set_window_rectangles,
    (req_handler)req_get_window_rectangles,
    (req_handler)req_get_window_text,
    (req_handler)req_set_window_text,
    (req_handler)req_inc_window_paint_count,
    (req_handler)req_get_windows_offset,
    (req_handler)req_set_window_property,
    (req_handler)req_remove_window_property,
    (req_handler)req_get_window_property,
    (req_handler)req_get_window_properties,
    (req_handler)req_attach_thread_input,
    (req_handler)req_get_thread_input,
    (req_handler)req_get_key_state,
    (req_handler)req_set_key_state,
    (req_handler)req_set_foreground_window,
    (req_handler)req_set_focus_window,
    (req_handler)req_set_active_window,
    (req_handler)req_set_capture_window,
    (req_handler)req_set_caret_window,
    (req_handler)req_set_caret_info,
    (req_handler)req_set_hook,
    (req_handler)req_remove_hook,
    (req_handler)req_start_hook_chain,
    (req_handler)req_finish_hook_chain,
    (req_handler)req_get_next_hook,
    (req_handler)req_create_class,
    (req_handler)req_destroy_class,
    (req_handler)req_set_class_info,
    (req_handler)req_set_clipboard_info,
    (req_handler)req_open_token,
    (req_handler)req_set_global_windows,
};
#endif  /* WANT_REQUEST_HANDLERS */

/* ### make_requests end ### */
/* Everything above this line is generated automatically by tools/make_requests */

#endif  /* __WINE_SERVER_REQUEST_H */
