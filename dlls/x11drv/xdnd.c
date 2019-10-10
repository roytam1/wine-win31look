/*
 * XDND handler code
 *
 * Copyright 2003 Ulrich Czekalla
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

#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "wownt32.h"

#include "x11drv.h"
#include "shlobj.h"  /* DROPFILES */

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(xdnd);

/* Maximum wait time for selection notify */
#define SELECTION_RETRIES 500  /* wait for .1 seconds */
#define SELECTION_WAIT    1000 /* us */

typedef struct tagXDNDDATA
{
    int cf_win;
    Atom cf_xdnd;
    void *data;
    unsigned int size;
    struct tagXDNDDATA *next;
} XDNDDATA, *LPXDNDDATA;

static LPXDNDDATA XDNDData = NULL;
static POINT XDNDxy = { 0, 0 };

static void X11DRV_XDND_InsertXDNDData(int property, int format, void* data, unsigned int len);
static int X11DRV_XDND_DeconstructTextPlain(int property, void* data, int len);
static int X11DRV_XDND_DeconstructTextHTML(int property, void* data, int len);
static int X11DRV_XDND_MapFormat(unsigned int property, unsigned char *data, int len);
static void X11DRV_XDND_ResolveProperty(Display *display, Window xwin, Time tm,
    Atom *types, unsigned long *count);
static void X11DRV_XDND_SendDropFiles(HWND hwnd);
static void X11DRV_XDND_FreeDragDropOp(void);
static unsigned int X11DRV_XDND_UnixToDos(char** lpdest, char* lpsrc, int len);
static DROPFILES* X11DRV_XDND_BuildDropFiles(char* filename, unsigned int len, POINT pt);

static CRITICAL_SECTION xdnd_cs;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &xdnd_cs,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { 0, (DWORD)(__FILE__ ": xdnd_cs") }
};
static CRITICAL_SECTION xdnd_cs = { &critsect_debug, -1, 0, 0, 0, 0 };

/**************************************************************************
 * X11DRV_XDND_Event
 *
 * Entry point for X11 XDND events. Returns FALSE if event is not handled.
 */
int X11DRV_XDND_Event(HWND hWnd, XClientMessageEvent *event)
{
    int isXDNDMsg = 1;

    TRACE("0x%p\n", hWnd);

    if (event->message_type == x11drv_atom(XdndEnter))
    {
        Atom *xdndtypes;
        unsigned long count = 0;

        TRACE("XDNDEnter: ver(%ld) check-XdndTypeList(%ld) data=%ld,%ld,%ld,%ld,%ld\n",
            (event->data.l[1] & 0xFF000000) >> 24, (event->data.l[1] & 1),
             event->data.l[0], event->data.l[1], event->data.l[2],
             event->data.l[3], event->data.l[4]);

        /* If the source supports more than 3 data types we retrieve
         * the entire list. */
        if (event->data.l[1] & 1)
        {
            Atom acttype;
            int actfmt;
            unsigned long bytesret;

            /* Request supported formats from source window */
            wine_tsx11_lock();
            XGetWindowProperty(event->display, event->data.l[0], x11drv_atom(XdndTypeList),
                               0, 65535, FALSE, AnyPropertyType, &acttype, &actfmt, &count,
                               &bytesret, (unsigned char**)&xdndtypes);
            wine_tsx11_unlock();
        }
        else
        {
            count = 3;
            xdndtypes = &event->data.l[2];
        }

        if (TRACE_ON(xdnd))
        {
            unsigned int i = 0;

            wine_tsx11_lock();
            for (; i < count; i++)
            {
                if (xdndtypes[i] != 0)
                {
                    char * pn = XGetAtomName(event->display, xdndtypes[i]);
                    TRACE("XDNDEnterAtom %ld: %s\n", xdndtypes[i], pn);
                    XFree(pn);
                }
            }
            wine_tsx11_unlock();
        }

        /* Do a one-time data read and cache results */
        X11DRV_XDND_ResolveProperty(event->display, event->window,
            event->data.l[1], xdndtypes, &count);

        if (event->data.l[1] & 1)
            XFree(xdndtypes);
    }
    else if (event->message_type == x11drv_atom(XdndPosition))
    {
        XClientMessageEvent e;
        int accept = 0; /* Assume we're not accepting */

        XDNDxy.x = event->data.l[2] >> 16;
        XDNDxy.y = event->data.l[2] & 0xFFFF;

        /* FIXME: Notify OLE of DragEnter. Result determines if we accept */

        if (GetWindowLongW( hWnd, GWL_EXSTYLE ) & WS_EX_ACCEPTFILES)
            accept = 1;

        TRACE("XDNDPosition. action req: %ld accept(%d) at x(%ld),y(%ld)\n",
                 event->data.l[4], accept, XDNDxy.x, XDNDxy.y);

        /*
         * Let source know if we're accepting the drop by
         * sending a status message.
         */
        e.type = ClientMessage;
        e.display = event->display;
        e.window = event->data.l[0];
        e.message_type = x11drv_atom(XdndStatus);
        e.format = 32;
        e.data.l[0] = event->window;
        e.data.l[1] = accept;
        e.data.l[2] = 0; /* Empty Rect */
        e.data.l[3] = 0; /* Empty Rect */
        if (accept)
            e.data.l[4] = event->data.l[4];
        else
            e.data.l[4] = None;
        wine_tsx11_lock();
        XSendEvent(event->display, event->data.l[0], False, NoEventMask, (XEvent*)&e);
        wine_tsx11_unlock();

        /* FIXME: if drag accepted notify OLE of DragOver */
    }
    else if (event->message_type == x11drv_atom(XdndDrop))
    {
        XClientMessageEvent e;

        TRACE("XDNDDrop\n");

        /* If we have a HDROP type we send a WM_ACCEPTFILES.*/
        if (GetWindowLongW( hWnd, GWL_EXSTYLE ) & WS_EX_ACCEPTFILES)
            X11DRV_XDND_SendDropFiles( hWnd );

        /* FIXME: Notify OLE of Drop */
        X11DRV_XDND_FreeDragDropOp();

        /* Tell the target we are finished. */
        memset(&e, 0, sizeof(e));
        e.type = ClientMessage;
        e.display = event->display;
        e.window = event->data.l[0];
        e.message_type = x11drv_atom(XdndFinished);
        e.format = 32;
        e.data.l[0] = event->window;
        wine_tsx11_lock();
        XSendEvent(event->display, event->data.l[0], False, NoEventMask, (XEvent*)&e);
        wine_tsx11_unlock();
    }
    else if (event->message_type == x11drv_atom(XdndLeave))
    {
        TRACE("DND Operation canceled\n");

        X11DRV_XDND_FreeDragDropOp();

        /* FIXME: Notify OLE of DragLeave */
    }
    else /* Not an XDND message */
        isXDNDMsg = 0;

    return isXDNDMsg;
}


/**************************************************************************
 * X11DRV_XDND_ResolveProperty
 *
 * Resolve all MIME types to windows clipboard formats. All data is cached.
 */
static void X11DRV_XDND_ResolveProperty(Display *display, Window xwin, Time tm,
                                        Atom *types, unsigned long *count)
{
    unsigned int i, j;
    BOOL res;
    XEvent xe;
    Atom acttype;
    int actfmt;
    unsigned long bytesret, icount;
    int entries = 0;
    unsigned char* data = NULL;

    TRACE("count(%ld)\n", *count);

    X11DRV_XDND_FreeDragDropOp(); /* Clear previously cached data */

    for (i = 0; i < *count; i++)
    {
        TRACE("requesting atom %ld from xwin %ld\n", types[i], xwin);

        if (types[i] == 0)
            continue;

        wine_tsx11_lock();
        XConvertSelection(display, x11drv_atom(XdndSelection), types[i],
                          x11drv_atom(XdndTarget), xwin, /*tm*/CurrentTime);
        wine_tsx11_unlock();

        /*
         * Wait for SelectionNotify
         */
        for (j = 0; j < SELECTION_RETRIES; j++)
        {
            wine_tsx11_lock();
            res = XCheckTypedWindowEvent(display, xwin, SelectionNotify, &xe);
            wine_tsx11_unlock();
            if (res && xe.xselection.selection == x11drv_atom(XdndSelection)) break;

            usleep(SELECTION_WAIT);
        }

        if (xe.xselection.property == None)
            continue;

        wine_tsx11_lock();
        XGetWindowProperty(display, xwin, x11drv_atom(XdndTarget), 0, 65535, FALSE,
            AnyPropertyType, &acttype, &actfmt, &icount, &bytesret, &data);
        wine_tsx11_unlock();

        entries += X11DRV_XDND_MapFormat(types[i], data, icount * (actfmt / 8));
        wine_tsx11_lock();
        XFree(data);
        wine_tsx11_unlock();
    }

    *count = entries;
}


/**************************************************************************
 * X11DRV_XDND_InsertXDNDData
 *
 * Cache available XDND property
 */
static void X11DRV_XDND_InsertXDNDData(int property, int format, void* data, unsigned int len)
{
    LPXDNDDATA current = (LPXDNDDATA) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(XDNDDATA));

    if (current)
    {
        EnterCriticalSection(&xdnd_cs);
        current->next = XDNDData;
        current->cf_xdnd = property;
        current->cf_win = format;
        current->data = data;
        current->size = len;
        XDNDData = current;
        LeaveCriticalSection(&xdnd_cs);
    }
}


/**************************************************************************
 * X11DRV_XDND_MapFormat
 *
 * Map XDND MIME format to windows clipboard format.
 */
static int X11DRV_XDND_MapFormat(unsigned int property, unsigned char *data, int len)
{
    void* xdata;
    int count = 0;

    TRACE("%d: %s\n", property, data);

    /* Always include the raw type */
    xdata = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, len);
    memcpy(xdata, data, len);
    X11DRV_XDND_InsertXDNDData(property, property, xdata, len);
    count++;

    if (property == x11drv_atom(text_plain))
        count += X11DRV_XDND_DeconstructTextPlain(property, data, len);
    else if (property == x11drv_atom(text_html))
        count += X11DRV_XDND_DeconstructTextHTML(property, data, len);

    return count;
}


/**************************************************************************
 * X11DRV_XDND_DeconstructTextPlain
 *
 * Interpret text/plain Data and add records to <dndfmt> linked list
 */
static int X11DRV_XDND_DeconstructTextPlain(int property, void* data, int len)
{
    char *p = (char*) data;
    char* dostext;
    int count = 0;

    /* Always suppply plain text */
    X11DRV_XDND_UnixToDos(&dostext, (char*)data, len);
    X11DRV_XDND_InsertXDNDData(property, CF_TEXT, dostext, strlen(dostext));
    count++;

    TRACE("CF_TEXT (%d): %s\n", CF_TEXT, dostext);

    /* Check for additional mappings */
    while (*p != '\0' && *p != ':') /* Advance to end of protocol */
        p++;

    if (*p == ':')
    {
        if (!strncasecmp(data, "http", 4))
        {
            X11DRV_XDND_InsertXDNDData(property, RegisterClipboardFormatA("UniformResourceLocator"),
                strdup(dostext), strlen(dostext));
                count++;

            TRACE("UniformResourceLocator: %s\n", dostext);
        }
        else if (!strncasecmp(data, "file", 4))
        {
            DROPFILES* pdf;

            pdf = X11DRV_XDND_BuildDropFiles(p+1, len - 5, XDNDxy);
            if (pdf)
            {
                unsigned int size = HeapSize(GetProcessHeap(), 0, pdf);

                X11DRV_XDND_InsertXDNDData(property, CF_HDROP, pdf, size);
                count++;
            }

            TRACE("CF_HDROP: %p\n", pdf);
        }
    }

    return count;
}


/**************************************************************************
 * X11DRV_XDND_DeconstructTextHTML
 *
 * Interpret text/html data and add records to <dndfmt> linked list
 */
static int X11DRV_XDND_DeconstructTextHTML(int property, void* data, int len)
{
    char* dostext;

    X11DRV_XDND_UnixToDos(&dostext, data, len);

    X11DRV_XDND_InsertXDNDData(property,
        RegisterClipboardFormatA("UniformResourceLocator"), dostext, strlen(dostext));

    TRACE("UniformResourceLocator: %s\n", dostext);

    return 1;
}


/**************************************************************************
 * X11DRV_XDND_SendDropFiles
 */
static void X11DRV_XDND_SendDropFiles(HWND hwnd)
{
    LPXDNDDATA current;

    EnterCriticalSection(&xdnd_cs);

    current = XDNDData;

    /* Find CF_HDROP type if any */
    while (current != NULL)
    {
        if (current->cf_win == CF_HDROP)
            break;
        current = current->next;
    }

    if (current != NULL)
    {
        DROPFILES *lpDrop = (DROPFILES*) current->data;

        if (lpDrop)
        {
            lpDrop->pt.x = XDNDxy.x;
            lpDrop->pt.y = XDNDxy.y;

            TRACE("Sending WM_DROPFILES: hWnd(0x%p) %p(%s)\n", hwnd,
                ((char*)lpDrop) + lpDrop->pFiles, ((char*)lpDrop) + lpDrop->pFiles);

            PostMessageA(hwnd, WM_DROPFILES, (WPARAM)lpDrop, 0L);
        }
    }

    LeaveCriticalSection(&xdnd_cs);
}


/**************************************************************************
 * X11DRV_XDND_FreeDragDropOp
 */
static void X11DRV_XDND_FreeDragDropOp(void)
{
    LPXDNDDATA next;
    LPXDNDDATA current;

    TRACE("\n");

    EnterCriticalSection(&xdnd_cs);

    current = XDNDData;

    /** Free data cache */
    while (current != NULL)
    {
        next = current->next;
        HeapFree(GetProcessHeap(), 0, current);
        current = next;
    }

    XDNDData = NULL;
    XDNDxy.x = XDNDxy.y = 0;

    LeaveCriticalSection(&xdnd_cs);
}



/**************************************************************************
 * X11DRV_XDND_BuildDropFiles
 */
static DROPFILES* X11DRV_XDND_BuildDropFiles(char* filename, unsigned int len, POINT pt)
{
    char* pfn;
    int pathlen;
    char path[MAX_PATH];
    DROPFILES *lpDrop = NULL;

    /* Advance to last starting slash */
    pfn = filename + 1;
    while (*pfn && (*pfn == '\\' || *pfn =='/'))
    {
        pfn++;
        filename++;
    }

    /* Remove any trailing \r or \n */
    while (*pfn)
    {
        if (*pfn == '\r' || *pfn == '\n')
        {
            *pfn = 0;
            break;
        }
        pfn++;
    }

    TRACE("%s\n", filename);

    pathlen = GetLongPathNameA(filename, path, MAX_PATH);
    if (pathlen)
    {
        lpDrop = (DROPFILES*) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
            sizeof(DROPFILES) + pathlen + 1);

        lpDrop->pFiles = sizeof(DROPFILES);
        lpDrop->pt.x = pt.x;
        lpDrop->pt.y = pt.y;
        lpDrop->fNC = 0;
        lpDrop->fWide = FALSE;

        strcpy(((char*)lpDrop)+lpDrop->pFiles, path);
    }

    TRACE("resolved %s\n", lpDrop ? filename : NULL);

    return lpDrop;
}


/**************************************************************************
 * X11DRV_XDND_UnixToDos
 */
static unsigned int X11DRV_XDND_UnixToDos(char** lpdest, char* lpsrc, int len)
{
    int i;
    unsigned int destlen, lines;

    for (i = 0, lines = 0; i <= len; i++)
    {
        if (lpsrc[i] == '\n')
            lines++;
    }

    destlen = len + lines + 1;

    if (lpdest)
    {
        char* lpstr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, destlen);
        for (i = 0, lines = 0; i <= len; i++)
        {
            if (lpsrc[i] == '\n')
                lpstr[++lines + i] = '\r';
            lpstr[lines + i] = lpsrc[i];
        }

        *lpdest = lpstr;
    }

    return lines;
}
