/*
 * an application for displaying Win32 console
 *
 * Copyright 2001, 2002 Eric Pouech
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

#include <stdio.h>
#include "wine/server.h"
#include "winecon_private.h"
#include "winnls.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wineconsole);

void WINECON_Fatal(const char* msg)
{
    WINE_ERR("%s\n", msg);
    ExitProcess(0);
}

/******************************************************************
 *		WINECON_FetchCells
 *
 * updates the local copy of cells (band to update)
 */
void WINECON_FetchCells(struct inner_data* data, int upd_tp, int upd_bm)
{
    SERVER_START_REQ( read_console_output )
    {
        req->handle = (obj_handle_t)data->hConOut;
        req->x      = 0;
        req->y      = upd_tp;
        req->mode   = CHAR_INFO_MODE_TEXTATTR;
        req->wrap   = TRUE;
        wine_server_set_reply( req, &data->cells[upd_tp * data->curcfg.sb_width],
                               (upd_bm-upd_tp+1) * data->curcfg.sb_width * sizeof(CHAR_INFO) );
        wine_server_call( req );
    }
    SERVER_END_REQ;
    data->fnRefresh(data, upd_tp, upd_bm);
}

/******************************************************************
 *		WINECON_NotifyWindowChange
 *
 * Inform server that visible window on sb has changed
 */
void WINECON_NotifyWindowChange(struct inner_data* data)
{
    SERVER_START_REQ( set_console_output_info )
    {
        req->handle       = (obj_handle_t)data->hConOut;
        req->win_left     = data->curcfg.win_pos.X;
        req->win_top      = data->curcfg.win_pos.Y;
        req->win_right    = data->curcfg.win_pos.X + data->curcfg.win_width - 1;
        req->win_bottom   = data->curcfg.win_pos.Y + data->curcfg.win_height - 1;
        req->mask         = SET_CONSOLE_OUTPUT_INFO_DISPLAY_WINDOW;
        wine_server_call( req );
    }
    SERVER_END_REQ;
}

/******************************************************************
 *		WINECON_GetHistorySize
 *
 *
 */
int	WINECON_GetHistorySize(HANDLE hConIn)
{
    int	ret = 0;

    SERVER_START_REQ(get_console_input_info)
    {
	req->handle = (obj_handle_t)hConIn;
	if (!wine_server_call_err( req )) ret = reply->history_size;
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************
 *		WINECON_SetHistorySize
 *
 *
 */
BOOL	WINECON_SetHistorySize(HANDLE hConIn, int size)
{
    BOOL	ret;

    SERVER_START_REQ(set_console_input_info)
    {
	req->handle = (obj_handle_t)hConIn;
	req->mask = SET_CONSOLE_INPUT_INFO_HISTORY_SIZE;
	req->history_size = size;
	ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************
 *		WINECON_GetHistoryMode
 *
 *
 */
int	WINECON_GetHistoryMode(HANDLE hConIn)
{
    int	ret = 0;

    SERVER_START_REQ(get_console_input_info)
    {
	req->handle = (obj_handle_t)hConIn;
	if (!wine_server_call_err( req )) ret = reply->history_mode;
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************
 *		WINECON_SetHistoryMode
 *
 *
 */
BOOL	WINECON_SetHistoryMode(HANDLE hConIn, int mode)
{
    BOOL	ret;

    SERVER_START_REQ(set_console_input_info)
    {
	req->handle = (obj_handle_t)hConIn;
	req->mask = SET_CONSOLE_INPUT_INFO_HISTORY_MODE;
	req->history_mode = mode;
	ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************
 *		WINECON_GetConsoleTitle
 *
 *
 */
BOOL WINECON_GetConsoleTitle(HANDLE hConIn, WCHAR* buffer, size_t len)
{
    BOOL ret;

    if (len < sizeof(WCHAR)) return FALSE;

    SERVER_START_REQ( get_console_input_info )
    {
        req->handle = (obj_handle_t)hConIn;
        wine_server_set_reply( req, buffer, len - sizeof(WCHAR) );
        if ((ret = !wine_server_call_err( req )))
        {
            len = wine_server_reply_size( reply );
            buffer[len / sizeof(WCHAR)] = 0;
        }
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************
 *		WINECON_SetEditionMode
 *
 *
 */
static BOOL WINECON_SetEditionMode(HANDLE hConIn, int edition_mode)
{
    BOOL ret;

    SERVER_START_REQ( set_console_input_info )
    {
        req->handle = (obj_handle_t)hConIn;
        req->mask = SET_CONSOLE_INPUT_INFO_EDITION_MODE;
        req->edition_mode = edition_mode;
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************
 *		WINECON_GrabChanges
 *
 * A change occurs, try to figure out which
 */
int	WINECON_GrabChanges(struct inner_data* data)
{
    struct console_renderer_event	evts[256];
    int	i, num, ev_found;
    HANDLE h;

    SERVER_START_REQ( get_console_renderer_events )
    {
        wine_server_set_reply( req, evts, sizeof(evts) );
        req->handle = (obj_handle_t)data->hSynchro;
        if (!wine_server_call_err( req )) num = wine_server_reply_size(reply) / sizeof(evts[0]);
        else num = 0;
    }
    SERVER_END_REQ;
    if (!num) {WINE_WARN("hmm renderer signaled but no events available\n"); return 1;}

    /* FIXME: should do some event compression here (cursor pos, update) */
    /* step 1: keep only last cursor pos event */
    ev_found = -1;
    for (i = num - 1; i >= 0; i--)
    {
        if (evts[i].event == CONSOLE_RENDERER_CURSOR_POS_EVENT)
        {
            if (ev_found != -1)
		evts[i].event = CONSOLE_RENDERER_NONE_EVENT;
	    ev_found = i;
        }
    }
    /* step 2: manage update events */
    ev_found = -1;
    for (i = 0; i < num; i++)
    {
	if (evts[i].event == CONSOLE_RENDERER_NONE_EVENT ||
	    evts[i].event == CONSOLE_RENDERER_CURSOR_POS_EVENT ||
	    evts[i].event == CONSOLE_RENDERER_CURSOR_GEOM_EVENT) continue;
	if (evts[i].event != CONSOLE_RENDERER_UPDATE_EVENT)
        {
	    ev_found = -1;
	    continue;
	}

	if (ev_found != -1 &&  /* Only 2 cases where they can NOT merge */
	    !(evts[i       ].u.update.bottom + 1 < evts[ev_found].u.update.top ||
	      evts[ev_found].u.update.bottom + 1 < evts[i       ].u.update.top))
	{
	    evts[i].u.update.top    = min(evts[i       ].u.update.top,
					  evts[ev_found].u.update.top);
	    evts[i].u.update.bottom = max(evts[i       ].u.update.bottom,
					  evts[ev_found].u.update.bottom);
	    evts[ev_found].event = CONSOLE_RENDERER_NONE_EVENT;
        }
	ev_found = i;
    }

    WINE_TRACE("Events:");
    for (i = 0; i < num; i++)
    {
	switch (evts[i].event)
	{
	case CONSOLE_RENDERER_NONE_EVENT:
	    WINE_TRACE(" NOP");
	    break;
	case CONSOLE_RENDERER_TITLE_EVENT:
	    WINE_TRACE(" title()");
	    data->fnSetTitle(data);
	    break;
	case CONSOLE_RENDERER_ACTIVE_SB_EVENT:
	    SERVER_START_REQ( open_console )
	    {
		req->from    = (int)data->hConIn;
		req->access  = GENERIC_READ | GENERIC_WRITE;
		req->share   = FILE_SHARE_READ | FILE_SHARE_WRITE;
		req->inherit = FALSE;
		h = wine_server_call_err( req ) ? 0 : (HANDLE)reply->handle;
	    }
	    SERVER_END_REQ;
	    WINE_TRACE(" active(%d)", (int)h);
	    if (h)
	    {
		CloseHandle(data->hConOut);
		data->hConOut = h;
	    }
	    break;
	case CONSOLE_RENDERER_SB_RESIZE_EVENT:
	    if (data->curcfg.sb_width != evts[i].u.resize.width ||
		data->curcfg.sb_height != evts[i].u.resize.height || !data->cells)
	    {
		WINE_TRACE(" resize(%d,%d)", evts[i].u.resize.width, evts[i].u.resize.height);
		data->curcfg.sb_width  = evts[i].u.resize.width;
		data->curcfg.sb_height = evts[i].u.resize.height;

		if (data->cells) 
		    data->cells = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, data->cells,
					  data->curcfg.sb_width * data->curcfg.sb_height * sizeof(CHAR_INFO));
		else 
		    data->cells = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
					  data->curcfg.sb_width * data->curcfg.sb_height * sizeof(CHAR_INFO));

		if (!data->cells) WINECON_Fatal("OOM\n");
		data->fnResizeScreenBuffer(data);
		data->fnComputePositions(data);
	    }
	    break;
	case CONSOLE_RENDERER_UPDATE_EVENT:
	    WINE_TRACE(" update(%d,%d)", evts[i].u.update.top, evts[i].u.update.bottom);
	    WINECON_FetchCells(data, evts[i].u.update.top, evts[i].u.update.bottom);
	    break;
	case CONSOLE_RENDERER_CURSOR_POS_EVENT:
	    if (evts[i].u.cursor_pos.x != data->cursor.X || evts[i].u.cursor_pos.y != data->cursor.Y)
	    {
		WINE_TRACE(" curs-pos(%d,%d)",evts[i].u.cursor_pos.x, evts[i].u.cursor_pos.y);
		data->cursor.X = evts[i].u.cursor_pos.x;
		data->cursor.Y = evts[i].u.cursor_pos.y;
		data->fnPosCursor(data);
	    }
	    break;
	case CONSOLE_RENDERER_CURSOR_GEOM_EVENT:
	    if (evts[i].u.cursor_geom.size != data->curcfg.cursor_size ||
		evts[i].u.cursor_geom.visible != data->curcfg.cursor_visible)
	    {
		WINE_TRACE(" curs-geom(%d,%d)",
                           evts[i].u.cursor_geom.size, evts[i].u.cursor_geom.visible);
		data->fnShapeCursor(data, evts[i].u.cursor_geom.size,
				    evts[i].u.cursor_geom.visible, FALSE);
	    }
	    break;
	case CONSOLE_RENDERER_DISPLAY_EVENT:
	    if (evts[i].u.display.left != data->curcfg.win_pos.X)
	    {
		WINE_TRACE(" h-scroll(%d)", evts[i].u.display.left);
		data->fnScroll(data, evts[i].u.display.left, TRUE);
		data->fnPosCursor(data);
	    }
	    if (evts[i].u.display.top != data->curcfg.win_pos.Y)
	    {
		WINE_TRACE(" v-scroll(%d)", evts[i].u.display.top);
		data->fnScroll(data, evts[i].u.display.top, FALSE);
		data->fnPosCursor(data);
	    }
	    if (evts[i].u.display.width != data->curcfg.win_width ||
		evts[i].u.display.height != data->curcfg.win_height)
	    {
		WINE_TRACE(" win-size(%d,%d)", evts[i].u.display.width, evts[i].u.display.height);
		data->curcfg.win_width = evts[i].u.display.width;
		data->curcfg.win_height = evts[i].u.display.height;
		data->fnComputePositions(data);
	    }
	    break;
	case CONSOLE_RENDERER_EXIT_EVENT:
	    WINE_TRACE(". Exit!!\n");
	    return 0;
	default:
	    WINE_FIXME("Unknown event type (%d)\n", evts[i].event);
	}
    }

    WINE_TRACE(".\n");
    return 1;
}

/******************************************************************
 *		WINECON_SetConfig
 *
 * Apply to data all the configuration elements from cfg. This includes modification
 * of server side equivalent and visual parts.
 * If force is FALSE, only the changed items are modified.
 */
void     WINECON_SetConfig(struct inner_data* data,
                           const struct config_data* cfg, BOOL force)
{
    if (force || data->curcfg.cursor_size != cfg->cursor_size ||
        data->curcfg.cursor_visible != cfg->cursor_visible)
    {
        CONSOLE_CURSOR_INFO cinfo;
        cinfo.dwSize = cfg->cursor_size;
        /* <FIXME>: this hack is needed to pass thru the invariant test operation on server side
         * (no notification is sent when invariant operation is requested
         */
        cinfo.bVisible = !cfg->cursor_visible;
        SetConsoleCursorInfo(data->hConOut, &cinfo);
        /* </FIXME> */
        cinfo.bVisible = cfg->cursor_visible;
        /* this shall update (through notif) curcfg */
        SetConsoleCursorInfo(data->hConOut, &cinfo);
    }
    if (force || data->curcfg.history_size != cfg->history_size)
    {
        data->curcfg.history_size = cfg->history_size;
        WINECON_SetHistorySize(data->hConIn, cfg->history_size);
    }
    if (force || data->curcfg.history_nodup != cfg->history_nodup)
    {
        data->curcfg.history_nodup = cfg->history_nodup;
        WINECON_SetHistoryMode(data->hConIn, cfg->history_nodup);
    }
    data->curcfg.menu_mask = cfg->menu_mask;
    data->curcfg.quick_edit = cfg->quick_edit;
    if (force || 1 /* FIXME: font info has changed */)
    {
        data->fnSetFont(data, cfg->face_name, cfg->cell_height, cfg->font_weight);
    }
    if (force || data->curcfg.def_attr != cfg->def_attr)
    {
        data->curcfg.def_attr = cfg->def_attr;
        SetConsoleTextAttribute(data->hConOut, cfg->def_attr);
    }
    /* now let's look at the window / sb size changes...
     * since the server checks that sb is always bigger than window, 
     * we have to take care of doing the operations in the right order
     */
    /* a set of macros to make things easier to read 
     * The Test<A><B> macros test if the <A> (width/height) needs to be changed 
     * for <B> (window / ScreenBuffer) 
     * The Change<A><B> actually modify the <B> dimension of <A>.
     */
#define TstSBfWidth()   (force || data->curcfg.sb_width != cfg->sb_width)
#define TstWinWidth()   (force || data->curcfg.win_width != cfg->win_width)

#define ChgSBfWidth()   do {c.X = cfg->sb_width; \
                            c.Y = data->curcfg.sb_height;\
                            SetConsoleScreenBufferSize(data->hConOut, c);\
                        } while (0)
#define ChgWinWidth()   do {pos.Left = pos.Top = 0; \
                            pos.Right = cfg->win_width - 1; \
                            pos.Bottom = data->curcfg.win_height - 1; \
                            SetConsoleWindowInfo(data->hConOut, FALSE, &pos);\
                        } while (0)
#define TstSBfHeight()  (force || data->curcfg.sb_height != cfg->sb_height)
#define TstWinHeight()  (force || data->curcfg.win_height != cfg->win_height)

/* since we're going to apply height after width is done, we use width as defined 
 * in cfg, and not in data->curcfg because if won't be updated yet */
#define ChgSBfHeight()  do {c.X = cfg->sb_width; c.Y = cfg->sb_height; \
                            SetConsoleScreenBufferSize(data->hConOut, c); \
                        } while (0)
#define ChgWinHeight()  do {pos.Left = pos.Top = 0; \
                            pos.Right = cfg->win_width - 1; \
                            pos.Bottom = cfg->win_height - 1; \
                            SetConsoleWindowInfo(data->hConOut, FALSE, &pos);\
                        } while (0)

    do
    {
        COORD       c;
        SMALL_RECT  pos;

        if (TstSBfWidth())            
        {
            if (TstWinWidth())
            {
                /* we're changing both at the same time, do it in the right order */
                if (cfg->sb_width >= data->curcfg.win_width)
                {
                    ChgSBfWidth(); ChgWinWidth();
                }
                else
                {
                    ChgWinWidth(); ChgSBfWidth();
                }
            }
            else ChgSBfWidth();
        }
        else if (TstWinWidth()) ChgWinWidth();
        if (TstSBfHeight())
        {
            if (TstWinHeight())
            {
                if (cfg->sb_height >= data->curcfg.win_height)
                {
                    ChgSBfHeight(); ChgWinHeight();
                }
                else
                {
                    ChgWinHeight(); ChgSBfHeight();
                }
            }
            else ChgSBfHeight();
        }
        else if (TstWinHeight()) ChgWinHeight();
    } while (0);
#undef TstSBfWidth
#undef TstWinWidth
#undef ChgSBfWidth
#undef ChgWinWidth
#undef TstSBfHeight
#undef TstWinHeight
#undef ChgSBfHeight
#undef ChgWinHeight

    data->curcfg.exit_on_die = cfg->exit_on_die;
    if (force || data->curcfg.edition_mode != cfg->edition_mode)
    {
        data->curcfg.edition_mode = cfg->edition_mode;
        WINECON_SetEditionMode(data->hConIn, cfg->edition_mode);
    }
    /* we now need to gather all events we got from the operations above,
     * in order to get data correctly updated
     */
    WINECON_GrabChanges(data);
}

/******************************************************************
 *		WINECON_Delete
 *
 * Destroy wineconsole internal data
 */
static void WINECON_Delete(struct inner_data* data)
{
    if (!data) return;

    if (data->fnDeleteBackend)  data->fnDeleteBackend(data);
    if (data->hConIn)		CloseHandle(data->hConIn);
    if (data->hConOut)		CloseHandle(data->hConOut);
    if (data->hSynchro)		CloseHandle(data->hSynchro);
    if (data->cells)		HeapFree(GetProcessHeap(), 0, data->cells);
    HeapFree(GetProcessHeap(), 0, data);
}

/******************************************************************
 *		WINECON_Init
 *
 * Initialisation part I. Creation of server object (console input and
 * active screen buffer)
 */
static struct inner_data* WINECON_Init(HINSTANCE hInst, DWORD pid, LPCWSTR appname,
                                       enum init_return (*backend)(struct inner_data*))
{
    struct inner_data*	data = NULL;
    DWORD		ret;
    struct config_data  cfg;
    STARTUPINFOW        si;

    data = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*data));
    if (!data) return 0;

    GetStartupInfo(&si);

    if (pid == 0)
    {
        if (!si.lpTitle) WINECON_Fatal("Should have a title set");
        appname = si.lpTitle;
    }

    /* load settings */
    WINECON_RegLoad(appname, &cfg);

    /* some overrides */
    if (pid == 0)
    {
        if (si.dwFlags & STARTF_USECOUNTCHARS)
        {
            cfg.sb_width  = si.dwXCountChars;
            cfg.sb_height = si.dwYCountChars;
        }
        if (si.dwFlags & STARTF_USEFILLATTRIBUTE)
            cfg.def_attr = si.dwFillAttribute;
        /* should always be defined */
    }

    /* the handles here are created without the whistles and bells required by console
     * (mainly because wineconsole doesn't need it)
     * - they are not inheritable
     * - hConIn is not synchronizable
     */
    SERVER_START_REQ(alloc_console)
    {
        req->access     = GENERIC_READ | GENERIC_WRITE;
        req->inherit    = FALSE;
        req->pid        = pid;

        ret = !wine_server_call_err( req );
        data->hConIn = (HANDLE)reply->handle_in;
	data->hSynchro = (HANDLE)reply->event;
    }
    SERVER_END_REQ;
    if (!ret) goto error;
    WINE_TRACE("using hConIn %p, hSynchro event %p\n", data->hConIn, data->hSynchro);

    SERVER_START_REQ( set_console_input_info )
    {
        req->handle = (obj_handle_t)data->hConIn;
        req->mask = SET_CONSOLE_INPUT_INFO_TITLE;
        wine_server_add_data( req, appname, lstrlenW(appname) * sizeof(WCHAR) );
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    if (!ret) goto error;

    SERVER_START_REQ(create_console_output)
    {
        req->handle_in = (obj_handle_t)data->hConIn;
        req->access    = GENERIC_WRITE|GENERIC_READ;
        req->share     = FILE_SHARE_READ|FILE_SHARE_WRITE;
        req->inherit   = FALSE;
        ret = !wine_server_call_err( req );
        data->hConOut  = (HANDLE)reply->handle_out;
    }
    SERVER_END_REQ;
    if (!ret) goto error;
    WINE_TRACE("using hConOut %p\n", data->hConOut);

    /* filling data->curcfg from cfg */
 retry:
    switch ((*backend)(data))
    {
    case init_success:
        WINECON_SetConfig(data, &cfg, TRUE);
        data->curcfg.registry = cfg.registry;
        WINECON_DumpConfig("fint", &data->curcfg);
        return data;
    case init_failed:
        break;
    case init_not_supported:
        if (backend == WCCURSES_InitBackend)
        {
            WINE_ERR("(n)curses was not found at configuration time.\n"
                     "If you want (n)curses support, please install relevant packages.\n"
                     "Now forcing user backend instead of (n)curses.\n");
            backend = WCUSER_InitBackend;
            goto retry;
        }
        break;
    }

 error:
    WINE_ERR("failed to init.\n");

    WINECON_Delete(data);
    return NULL;
}

/******************************************************************
 *		WINECON_Spawn
 *
 * Spawn the child process when invoked with wineconsole foo bar
 */
static BOOL WINECON_Spawn(struct inner_data* data, LPWSTR cmdLine)
{
    PROCESS_INFORMATION	info;
    STARTUPINFO		startup;
    BOOL		done;

    /* we're in the case wineconsole <exe> <options>... spawn the new process */
    memset(&startup, 0, sizeof(startup));
    startup.cb          = sizeof(startup);
    startup.dwFlags     = STARTF_USESTDHANDLES;

    /* the attributes of wineconsole's handles are not adequate for inheritance, so
     * get them with the correct attributes before process creation
     */
    if (!DuplicateHandle(GetCurrentProcess(), data->hConIn,  GetCurrentProcess(),
			 &startup.hStdInput, GENERIC_READ|GENERIC_WRITE|SYNCHRONIZE, TRUE, 0) ||
	!DuplicateHandle(GetCurrentProcess(), data->hConOut, GetCurrentProcess(),
			 &startup.hStdOutput, GENERIC_READ|GENERIC_WRITE, TRUE, 0) ||
	!DuplicateHandle(GetCurrentProcess(), data->hConOut, GetCurrentProcess(),
                         &startup.hStdError, GENERIC_READ|GENERIC_WRITE, TRUE, 0))
    {
	WINE_ERR("Can't dup handles\n");
	/* no need to delete handles, we're exiting the programm anyway */
	return FALSE;
    }

    done = CreateProcess(NULL, cmdLine, NULL, NULL, TRUE, 0L, NULL, NULL, &startup, &info);

    /* we no longer need the handles passed to the child for the console */
    CloseHandle(startup.hStdInput);
    CloseHandle(startup.hStdOutput);
    CloseHandle(startup.hStdError);

    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);

    return done;
}

struct wc_init {
    LPCSTR              ptr;
    enum {from_event, from_process_name} mode;
    enum init_return    (*backend)(struct inner_data*);
    HANDLE              event;
};

/******************************************************************
 *		 WINECON_ParseOptions
 *
 *
 */
static BOOL WINECON_ParseOptions(const char* lpCmdLine, struct wc_init* wci)
{
    memset(wci, 0, sizeof(*wci));
    wci->ptr = lpCmdLine;
    wci->mode = from_process_name;
    wci->backend = WCCURSES_InitBackend;

    for (;;)
    {
        while (*wci->ptr == ' ' || *wci->ptr == '\t') wci->ptr++;
        if (wci->ptr[0] != '-') break;
        if (strncmp(wci->ptr, "--use-event=", 12) == 0)
        {
            char*           end;
            wci->event = (HANDLE)strtol(wci->ptr + 12, &end, 10);
            if (end == wci->ptr + 12) return FALSE;
            wci->mode = from_event;
            wci->ptr = end;
            wci->backend = WCUSER_InitBackend;
        }
        else if (strncmp(wci->ptr, "--backend=", 10) == 0)
        {
            if (strncmp(wci->ptr + 10, "user", 4) == 0)
            {
                wci->backend = WCUSER_InitBackend;
                wci->ptr += 14;
            }
            else if (strncmp(wci->ptr + 10, "curses", 6) == 0)
            {
                wci->ptr += 16;
            }
            else
                return FALSE;
        }
        else
            return FALSE;
    }

    return TRUE;
}

/******************************************************************
 *		WinMain
 *
 * wineconsole can either be started as:
 *	wineconsole --use-event=<int>	used when a new console is created (AllocConsole)
 *	wineconsole <pgm> <arguments>	used to start the program <pgm> from the command line in
 *					a freshly created console
 * --backend=(curses|user) can also be used to select the desired backend
 */
int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, INT nCmdShow)
{
    struct inner_data*	data;
    int			ret = 1;
    struct wc_init      wci;

    if (!WINECON_ParseOptions(lpCmdLine, &wci))
    {
        WINE_ERR("Wrong command line options\n");
        return 0;
    }

    switch (wci.mode)
    {
    case from_event:
        /* case of wineconsole <evt>, signal process that created us that we're up and running */
        if (!(data = WINECON_Init(hInst, 0, NULL, wci.backend))) return 0;
	ret = SetEvent(wci.event);
	if (!ret) WINE_ERR("SetEvent failed.\n");
        break;
    case from_process_name:
        {
            WCHAR           buffer[256];

            MultiByteToWideChar(CP_ACP, 0, wci.ptr, -1, buffer, sizeof(buffer) / sizeof(buffer[0]));

            if (!(data = WINECON_Init(hInst, GetCurrentProcessId(), buffer, wci.backend)))
                return 0;
            ret = WINECON_Spawn(data, buffer);
            if (!ret)
                WINE_MESSAGE("wineconsole: spawning client program failed (%s), invalid/missing command line arguments ?\n", wine_dbgstr_w(buffer));
        }
        break;
    default:
        return 0;
    }

    if (ret)
    {
	WINE_TRACE("calling MainLoop.\n");
	ret = data->fnMainLoop(data);
    }

    WINECON_Delete(data);

    return ret;
}
