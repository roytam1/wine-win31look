/*
 * X11 tablet driver
 *
 * Copyright 2003 CodeWeavers (Aric Stewart)
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

#include <stdlib.h>

#include "windef.h"
#include "x11drv.h"
#include "wine/library.h"
#include "wine/debug.h"
#include "wintab.h"

WINE_DEFAULT_DEBUG_CHANNEL(wintab32);
WINE_DECLARE_DEBUG_CHANNEL(event);

typedef struct tagWTI_CURSORS_INFO
{
    CHAR   NAME[256];
        /* a displayable zero-terminated string containing the name of the
         * cursor.
         */
    BOOL    ACTIVE;
        /* whether the cursor is currently connected. */
    WTPKT   PKTDATA;
        /* a bit mask indicating the packet data items supported when this
         * cursor is connected.
         */
    BYTE    BUTTONS;
        /* the number of buttons on this cursor. */
    BYTE    BUTTONBITS;
        /* the number of bits of raw button data returned by the hardware.*/
    CHAR   BTNNAMES[1024]; /* FIXME: make this dynamic */
        /* a list of zero-terminated strings containing the names of the
         * cursor's buttons. The number of names in the list is the same as the
         * number of buttons on the cursor. The names are separated by a single
         * zero character; the list is terminated by two zero characters.
         */
    BYTE    BUTTONMAP[32];
        /* a 32 byte array of logical button numbers, one for each physical
         * button.
         */
    BYTE    SYSBTNMAP[32];
        /* a 32 byte array of button action codes, one for each logical
         * button.
         */
    BYTE    NPBUTTON;
        /* the physical button number of the button that is controlled by normal
         * pressure.
         */
    UINT    NPBTNMARKS[2];
        /* an array of two UINTs, specifying the button marks for the normal
         * pressure button. The first UINT contains the release mark; the second
         * contains the press mark.
         */
    UINT    *NPRESPONSE;
        /* an array of UINTs describing the pressure response curve for normal
         * pressure.
         */
    BYTE    TPBUTTON;
        /* the physical button number of the button that is controlled by
         * tangential pressure.
         */
    UINT    TPBTNMARKS[2];
        /* an array of two UINTs, specifying the button marks for the tangential
         * pressure button. The first UINT contains the release mark; the second
         * contains the press mark.
         */
    UINT    *TPRESPONSE;
        /* an array of UINTs describing the pressure response curve for
         * tangential pressure.
         */
    DWORD   PHYSID;
         /* a manufacturer-specific physical identifier for the cursor. This
          * value will distinguish the physical cursor from others on the same
          * device. This physical identifier allows applications to bind
          * functions to specific physical cursors, even if category numbers
          * change and multiple, otherwise identical, physical cursors are
          * present.
          */
    UINT    MODE;
        /* the cursor mode number of this cursor type, if this cursor type has
         * the CRC_MULTIMODE capability.
         */
    UINT    MINPKTDATA;
        /* the minimum set of data available from a physical cursor in this
         * cursor type, if this cursor type has the CRC_AGGREGATE capability.
         */
    UINT    MINBUTTONS;
        /* the minimum number of buttons of physical cursors in the cursor type,
         * if this cursor type has the CRC_AGGREGATE capability.
         */
    UINT    CAPABILITIES;
        /* flags indicating cursor capabilities, as defined below:
            CRC_MULTIMODE
                Indicates this cursor type describes one of several modes of a
                single physical cursor. Consecutive cursor type categories
                describe the modes; the CSR_MODE data item gives the mode number
                of each cursor type.
            CRC_AGGREGATE
                Indicates this cursor type describes several physical cursors
                that cannot be distinguished by software.
            CRC_INVERT
                Indicates this cursor type describes the physical cursor in its
                inverted orientation; the previous consecutive cursor type
                category describes the normal orientation.
         */
    UINT    TYPE;
        /* Manufacturer Unique id for the item type */
} WTI_CURSORS_INFO, *LPWTI_CURSORS_INFO;


typedef struct tagWTI_DEVICES_INFO
{
    CHAR   NAME[256];
        /* a displayable null- terminated string describing the device,
         * manufacturer, and revision level.
         */
    UINT    HARDWARE;
        /* flags indicating hardware and driver capabilities, as defined
         * below:
            HWC_INTEGRATED:
                Indicates that the display and digitizer share the same surface.
            HWC_TOUCH
                Indicates that the cursor must be in physical contact with the
                device to report position.
            HWC_HARDPROX
                Indicates that device can generate events when the cursor is
                entering and leaving the physical detection range.
            HWC_PHYSID_CURSORS
                Indicates that device can uniquely identify the active cursor in
                hardware.
         */
    UINT    NCSRTYPES;
        /* the number of supported cursor types.*/
    UINT    FIRSTCSR;
        /* the first cursor type number for the device. */
    UINT    PKTRATE;
        /* the maximum packet report rate in Hertz. */
    WTPKT   PKTDATA;
        /* a bit mask indicating which packet data items are always available.*/
    WTPKT   PKTMODE;
        /* a bit mask indicating which packet data items are physically
         * relative, i.e., items for which the hardware can only report change,
         * not absolute measurement.
         */
    WTPKT   CSRDATA;
        /* a bit mask indicating which packet data items are only available when
         * certain cursors are connected. The individual cursor descriptions
         * must be consulted to determine which cursors return which data.
         */
    INT     XMARGIN;
    INT     YMARGIN;
    INT     ZMARGIN;
        /* the size of tablet context margins in tablet native coordinates, in
         * the x, y, and z directions, respectively.
         */
    AXIS    X;
    AXIS    Y;
    AXIS    Z;
        /* the tablet's range and resolution capabilities, in the x, y, and z
         * axes, respectively.
         */
    AXIS    NPRESSURE;
    AXIS    TPRESSURE;
        /* the tablet's range and resolution capabilities, for the normal and
         * tangential pressure inputs, respectively.
         */
    AXIS    ORIENTATION[3];
        /* a 3-element array describing the tablet's orientation range and
         * resolution capabilities.
         */
    AXIS    ROTATION[3];
        /* a 3-element array describing the tablet's rotation range and
         * resolution capabilities.
         */
    CHAR   PNPID[256];
        /* a null-terminated string containing the devices Plug and Play ID.*/
}   WTI_DEVICES_INFO, *LPWTI_DEVICES_INFO;

typedef struct tagWTPACKET {
        HCTX pkContext;
        UINT pkStatus;
        LONG pkTime;
        WTPKT pkChanged;
        UINT pkSerialNumber;
        UINT pkCursor;
        DWORD pkButtons;
        DWORD pkX;
        DWORD pkY;
        DWORD pkZ;
        UINT pkNormalPressure;
        UINT pkTangentPressure;
        ORIENTATION pkOrientation;
        ROTATION pkRotation; /* 1.1 */
} WTPACKET, *LPWTPACKET;


#ifdef HAVE_X11_EXTENSIONS_XINPUT_H

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>

static int           motion_type = -1;
static int           button_press_type = -1;
static int           button_release_type = -1;
static int           key_press_type = -1;
static int           key_release_type = -1;
static int           proximity_in_type = -1;
static int           proximity_out_type = -1;

static HWND          hwndTabletDefault;
static WTPACKET      gMsgPacket;
static DWORD         gSerial;
static INT           button_state[10];

#define             CURSORMAX 10

static LOGCONTEXTA      gSysContext;
static WTI_DEVICES_INFO gSysDevice;
static WTI_CURSORS_INFO gSysCursor[CURSORMAX];
static INT              gNumCursors;


#ifndef SONAME_LIBXI
#define SONAME_LIBXI "libXi.so"
#endif

/* XInput stuff */
static void *xinput_handle;

#define MAKE_FUNCPTR(f) static typeof(f) * p##f;
MAKE_FUNCPTR(XListInputDevices)
MAKE_FUNCPTR(XOpenDevice)
MAKE_FUNCPTR(XQueryDeviceState)
MAKE_FUNCPTR(XGetDeviceButtonMapping)
MAKE_FUNCPTR(XCloseDevice)
MAKE_FUNCPTR(XSelectExtensionEvent)
MAKE_FUNCPTR(XFreeDeviceState)
#undef MAKE_FUNCPTR

static INT X11DRV_XInput_Init(void)
{
    xinput_handle = wine_dlopen(SONAME_LIBXI, RTLD_NOW, NULL, 0);
    if (xinput_handle)
    {
#define LOAD_FUNCPTR(f) if((p##f = wine_dlsym(xinput_handle, #f, NULL, 0)) == NULL) goto sym_not_found;
        LOAD_FUNCPTR(XListInputDevices)
        LOAD_FUNCPTR(XOpenDevice)
        LOAD_FUNCPTR(XGetDeviceButtonMapping)
        LOAD_FUNCPTR(XCloseDevice)
        LOAD_FUNCPTR(XSelectExtensionEvent)
        LOAD_FUNCPTR(XQueryDeviceState)
        LOAD_FUNCPTR(XFreeDeviceState)
#undef LOAD_FUNCPTR
        return 1;
    }
sym_not_found:
    return 0;
}

static int Tablet_ErrorHandler(Display *dpy, XErrorEvent *event, void* arg)
{
    return 1;
}

void X11DRV_LoadTabletInfo(HWND hwnddefault)
{
    struct x11drv_thread_data *data = x11drv_thread_data();
    int num_devices;
    int loop;
    int cursor_target;
    XDeviceInfo *devices;
    XDeviceInfo *target = NULL;
    BOOL    axis_read_complete= FALSE;

    XAnyClassPtr        any;
    XButtonInfoPtr      Button;
    XValuatorInfoPtr    Val;
    XAxisInfoPtr        Axis;

    XDevice *opendevice;

    if (!X11DRV_XInput_Init())
    {
        ERR("Unable to initialized the XInput library.\n");
        return;
    }

    hwndTabletDefault = hwnddefault;

    /* Do base initializaion */
    strcpy(gSysContext.lcName, "Wine Tablet Context");
    strcpy(gSysDevice.NAME,"Wine Tablet Device");

    gSysContext.lcOptions = CXO_SYSTEM | CXO_MESSAGES | CXO_CSRMESSAGES;
    gSysContext.lcLocks = CXL_INSIZE | CXL_INASPECT | CXL_MARGIN |
                               CXL_SENSITIVITY | CXL_SYSOUT;

    gSysContext.lcMsgBase= WT_DEFBASE;
    gSysContext.lcDevice = 0;
    gSysContext.lcPktData =
        PK_CONTEXT | PK_STATUS | PK_SERIAL_NUMBER| PK_TIME | PK_CURSOR |
        PK_BUTTONS |  PK_X | PK_Y | PK_NORMAL_PRESSURE | PK_ORIENTATION;
    gSysContext.lcMoveMask=
        PK_BUTTONS |  PK_X | PK_Y | PK_NORMAL_PRESSURE | PK_ORIENTATION;
    gSysContext.lcStatus = CXS_ONTOP;
    gSysContext.lcPktRate = 100;
    gSysContext.lcBtnDnMask = 0xffffffff;
    gSysContext.lcBtnUpMask = 0xffffffff;
    gSysContext.lcSensX = 65536;
    gSysContext.lcSensY = 65536;
    gSysContext.lcSensX = 65536;
    gSysContext.lcSensZ = 65536;
    gSysContext.lcSysSensX= 65536;
    gSysContext.lcSysSensY= 65536;

    /* Device Defaults */
    gSysDevice.HARDWARE = HWC_HARDPROX|HWC_PHYSID_CURSORS;
    gSysDevice.FIRSTCSR= 0;
    gSysDevice.PKTRATE = 100;
    gSysDevice.PKTDATA =
        PK_CONTEXT | PK_STATUS | PK_SERIAL_NUMBER| PK_TIME | PK_CURSOR |
        PK_BUTTONS |  PK_X | PK_Y | PK_NORMAL_PRESSURE | PK_ORIENTATION;
    strcpy(gSysDevice.PNPID,"non-pluginplay");

    wine_tsx11_lock();

    cursor_target = -1;
    devices = pXListInputDevices(data->display, &num_devices);
    if (!devices)
    {
        WARN("XInput Extenstions reported as not avalable\n");
        wine_tsx11_unlock();
        return;
    }
    for (loop=0; loop < num_devices; loop++)
    {
        int class_loop;

        TRACE("Trying device %i(%s)\n",loop,devices[loop].name);
        if (devices[loop].use == IsXExtensionDevice)
        {
            LPWTI_CURSORS_INFO cursor;

            TRACE("Is Extension Device\n");
            cursor_target++;
            target = &devices[loop];
            cursor = &gSysCursor[cursor_target];

            opendevice = pXOpenDevice(data->display,target->id);
            if (opendevice)
            {
                unsigned char map[32];
                int i;
                int shft = 0;

                X11DRV_expect_error(data->display,Tablet_ErrorHandler,NULL);
                pXGetDeviceButtonMapping(data->display, opendevice, map, 32);
                if (X11DRV_check_error())
                {
                    TRACE("No buttons, Non Tablet Device\n");
                    pXCloseDevice(data->display, opendevice);
                    cursor_target --;
                    continue;
                }

                for (i=0; i< cursor->BUTTONS; i++,shft++)
                {
                    cursor->BUTTONMAP[i] = map[i];
                    cursor->SYSBTNMAP[i] = (1<<shft);
                }
                pXCloseDevice(data->display, opendevice);
            }
            else
            {
                WARN("Unable to open device %s\n",target->name);
                cursor_target --;
                continue;
            }

            strcpy(cursor->NAME,target->name);

            cursor->ACTIVE = 1;
            cursor->PKTDATA = PK_TIME | PK_CURSOR | PK_BUTTONS |  PK_X | PK_Y |
                              PK_NORMAL_PRESSURE | PK_TANGENT_PRESSURE |
                              PK_ORIENTATION;

            cursor->PHYSID = cursor_target;
            cursor->NPBUTTON = 1;
            cursor->NPBTNMARKS[0] = 0 ;
            cursor->NPBTNMARKS[1] = 1 ;
            cursor->CAPABILITIES = 1;
            if (strcasecmp(cursor->NAME,"stylus")==0)
                cursor->TYPE = 0x4825;
            if (strcasecmp(cursor->NAME,"eraser")==0)
                cursor->TYPE = 0xc85a;


            any = (XAnyClassPtr) (target->inputclassinfo);

            for (class_loop = 0; class_loop < target->num_classes; class_loop++)
            {
                switch (any->class)
                {
                    case ValuatorClass:
                        if (!axis_read_complete)
                        {
                            Val = (XValuatorInfoPtr) any;
                            Axis = (XAxisInfoPtr) ((char *) Val + sizeof
                                (XValuatorInfo));

                            if (Val->num_axes>=1)
                            {
                                /* Axis 1 is X */
                                gSysDevice.X.axMin = Axis->min_value;
                                gSysDevice.X.axMax= Axis->max_value;
                                gSysDevice.X.axUnits = 1;
                                gSysDevice.X.axResolution = Axis->resolution;
                                gSysContext.lcInOrgX = Axis->min_value;
                                gSysContext.lcSysOrgX = Axis->min_value;
                                gSysContext.lcInExtX = Axis->max_value;
                                gSysContext.lcSysExtX = Axis->max_value;
                                Axis++;
                            }
                            if (Val->num_axes>=2)
                            {
                                /* Axis 2 is Y */
                                gSysDevice.Y.axMin = Axis->min_value;
                                gSysDevice.Y.axMax= Axis->max_value;
                                gSysDevice.Y.axUnits = 1;
                                gSysDevice.Y.axResolution = Axis->resolution;
                                gSysContext.lcInOrgY = Axis->min_value;
                                gSysContext.lcSysOrgY = Axis->min_value;
                                gSysContext.lcInExtY = Axis->max_value;
                                gSysContext.lcSysExtY = Axis->max_value;
                                Axis++;
                            }
                            if (Val->num_axes>=3)
                            {
                                /* Axis 3 is Normal Pressure */
                                gSysDevice.NPRESSURE.axMin = Axis->min_value;
                                gSysDevice.NPRESSURE.axMax= Axis->max_value;
                                gSysDevice.NPRESSURE.axUnits = 1;
                                gSysDevice.NPRESSURE.axResolution =
                                                        Axis->resolution;
                                Axis++;
                            }
                            if (Val->num_axes >= 5)
                            {
                                /* Axis 4 and 5 are X and Y tilt */
                                XAxisInfoPtr        XAxis = Axis;
                                Axis++;
                                if (max (abs(Axis->max_value),
                                         abs(XAxis->max_value)))
                                {
                                    gSysDevice.ORIENTATION[0].axMin = 0;
                                    gSysDevice.ORIENTATION[0].axMax = 3600;
                                    gSysDevice.ORIENTATION[0].axUnits = 1;
                                    gSysDevice.ORIENTATION[0].axResolution =
                                                                     235929600;
                                    gSysDevice.ORIENTATION[1].axMin = -1000;
                                    gSysDevice.ORIENTATION[1].axMax = 1000;
                                    gSysDevice.ORIENTATION[1].axUnits = 1;
                                    gSysDevice.ORIENTATION[1].axResolution =
                                                                     235929600;
                                    Axis++;
                                }
                            }
                            axis_read_complete = TRUE;
                        }
                        break;
                    case ButtonClass:
                    {
                        CHAR *ptr = cursor->BTNNAMES;
                        int i;

                        Button = (XButtonInfoPtr) any;
                        cursor->BUTTONS = Button->num_buttons;
                        for (i = 0; i < cursor->BUTTONS; i++)
                        {
                            strcpy(ptr,cursor->NAME);
                            ptr+=8;
                        }
                    }
                    break;
                }
                any = (XAnyClassPtr) ((char*) any + any->length);
            }
        }
    }
    wine_tsx11_unlock();
    gSysDevice.NCSRTYPES = cursor_target+1;
    gNumCursors = cursor_target+1;
}

static int figure_deg(int x, int y)
{
    int rc;

    if (y != 0)
    {
        rc = (int) 10 * (atan( (FLOAT)abs(y) / (FLOAT)abs(x)) / (3.1415 / 180));
        if (y>0)
        {
            if (x>0)
                rc += 900;
            else
                rc = 2700 - rc;
        }
        else
        {
            if (x>0)
                rc = 900 - rc;
            else
                rc += 2700;
        }
    }
    else
    {
        if (x >= 0)
            rc = 900;
        else
            rc = 2700;
    }

    return rc;
}

static int get_button_state(int deviceid)
{
    return button_state[deviceid];
}

static void set_button_state(XID deviceid)
{
    struct x11drv_thread_data *data = x11drv_thread_data();
    XDevice *device;
    XDeviceState *state;
    XInputClass  *class;
    int loop;
    int rc = 0;

    wine_tsx11_lock();
    device = pXOpenDevice(data->display,deviceid);
    state = pXQueryDeviceState(data->display,device);

    if (state)
    {
        class = state->data;
        for (loop = 0; loop < state->num_classes; loop++)
        {
            if (class->class == ButtonClass)
            {
                int loop2;
                XButtonState *button_state =  (XButtonState*)class;
                for (loop2 = 1; loop2 <= button_state->num_buttons; loop2++)
                {
                    if (button_state->buttons[loop2 / 8] & (1 << (loop2 % 8)))
                    {
                        rc |= (1<<(loop2-1));
                    }
                }
            }
            class = (XInputClass *) ((char *) class + class->length);
        }
    }
    pXFreeDeviceState(state);
    wine_tsx11_unlock();
    button_state[deviceid] = rc;
}

int X11DRV_ProcessTabletEvent(HWND hwnd, XEvent *event)
{
    memset(&gMsgPacket,0,sizeof(WTPACKET));

    if(event->type ==  motion_type)
    {
        XDeviceMotionEvent *motion = (XDeviceMotionEvent *)event;

        TRACE_(event)("Received tablet motion event (%p)\n",hwnd);
        TRACE("Received tablet motion event (%p)\n",hwnd);
        gMsgPacket.pkTime = motion->time;
        gMsgPacket.pkSerialNumber = gSerial++;
        gMsgPacket.pkCursor = motion->deviceid;
        gMsgPacket.pkX = motion->axis_data[0];
        gMsgPacket.pkY = motion->axis_data[1];
        gMsgPacket.pkOrientation.orAzimuth =
                    figure_deg(motion->axis_data[3],motion->axis_data[4]);
        gMsgPacket.pkOrientation.orAltitude = 1000 - 15 * max
                    (abs(motion->axis_data[3]),abs(motion->axis_data[4]));
        gMsgPacket.pkNormalPressure = motion->axis_data[2];
        gMsgPacket.pkButtons = get_button_state(motion->deviceid);
        SendMessageW(hwndTabletDefault,WT_PACKET,0,(LPARAM)hwnd);
    }
    else if ((event->type == button_press_type)||(event->type ==
              button_release_type))
    {
        XDeviceButtonEvent *button = (XDeviceButtonEvent *) event;

        TRACE_(event)("Received tablet button event\n");
        TRACE("Received tablet button %s event\n", (event->type ==
                                button_press_type)?"press":"release");

        set_button_state(button->deviceid);
    }
    else if (event->type == key_press_type)
    {
        TRACE_(event)("Received tablet key press event\n");
        FIXME("Received tablet key press event\n");
    }
    else if (event->type == key_release_type)
    {
        TRACE_(event)("Received tablet key release event\n");
        FIXME("Received tablet key release event\n");
    }
    else if ((event->type == proximity_in_type) ||
             (event->type == proximity_out_type))
    {
        TRACE_(event)("Received tablet proximity event\n");
        TRACE("Received tablet proximity event\n");
        gMsgPacket.pkStatus = (event->type==proximity_out_type)?TPS_PROXIMITY:0;
        SendMessageW(hwndTabletDefault, WT_PROXIMITY,
                     (event->type==proximity_out_type)?0:1, (LPARAM)hwnd);
    }
    else
        return 0;

    return 1;
}

int X11DRV_AttachEventQueueToTablet(HWND hOwner)
{
    struct x11drv_thread_data *data = x11drv_thread_data();
    int             num_devices;
    int             loop;
    int             cur_loop;
    XDeviceInfo     *devices;
    XDeviceInfo     *target = NULL;
    XDevice         *the_device;
    XInputClassInfo *ip;
    XEventClass     event_list[7];
    Window          win = X11DRV_get_whole_window( hOwner );

    if (!win) return 0;

    TRACE("Creating context for window %p (%lx)  %i cursors\n", hOwner, win, gNumCursors);

    wine_tsx11_lock();
    devices = pXListInputDevices(data->display, &num_devices);

    for (cur_loop=0; cur_loop < gNumCursors; cur_loop++)
    {
        int    event_number=0;

        for (loop=0; loop < num_devices; loop ++)
            if (strcmp(devices[loop].name,gSysCursor[cur_loop].NAME)==0)
                target = &devices[loop];

        TRACE("Opening cursor %i id %i\n",cur_loop,(INT)target->id);

        the_device = pXOpenDevice(data->display, target->id);

        if (!the_device)
        {
            WARN("Unable to Open device\n");
            continue;
        }

        if (the_device->num_classes > 0)
        {
            for (ip = the_device->classes, loop=0; loop < target->num_classes;
                 ip++, loop++)
            {
                switch(ip->input_class)
                {
                    case KeyClass:
                        DeviceKeyPress(the_device, key_press_type,
                                       event_list[event_number]);
                        event_number++;
                        DeviceKeyRelease(the_device, key_release_type,
                                          event_list[event_number]);
                        event_number++;
                        break;
                    case ButtonClass:
                        DeviceButtonPress(the_device, button_press_type,
                                       event_list[event_number]);
                        event_number++;
                        DeviceButtonRelease(the_device, button_release_type,
                                            event_list[event_number]);
                        event_number++;
                        break;
                    case ValuatorClass:
                        DeviceMotionNotify(the_device, motion_type,
                                           event_list[event_number]);
                        event_number++;
                        ProximityIn(the_device, proximity_in_type,
                                 event_list[event_number]);
                        event_number++;
                        ProximityOut(the_device, proximity_out_type,
                                     event_list[event_number]);
                        event_number++;
                        break;
                    default:
                        ERR("unknown class\n");
                        break;
                }
            }
            if (pXSelectExtensionEvent(data->display, win, event_list, event_number))
            {
                ERR( "error selecting extended events\n");
                goto end;
            }
        }
    }

end:
    wine_tsx11_unlock();
    return 0;
}

int X11DRV_GetCurrentPacket(LPWTPACKET *packet)
{
    memcpy(packet,&gMsgPacket,sizeof(WTPACKET));
    return 1;
}


int static inline CopyTabletData(LPVOID target, LPVOID src, INT size)
{
    memcpy(target,src,size);
    return(size);
}

/***********************************************************************
 *		X11DRV_WTInfoA (X11DRV.@)
 */
UINT X11DRV_WTInfoA(UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
    int rc = 0;
    LPWTI_CURSORS_INFO  tgtcursor;
    TRACE("(%u, %u, %p)\n", wCategory, nIndex, lpOutput);

    switch(wCategory)
    {
        case 0:
            /* return largest necessary buffer */
            TRACE("%i cursors\n",gNumCursors);
            if (gNumCursors>0)
            {
                FIXME("Return proper size\n");
                return 200;
            }
            else
                return 0;
            break;
        case WTI_INTERFACE:
            switch (nIndex)
            {
                WORD version;
                case IFC_WINTABID:
                    strcpy(lpOutput,"Wine Wintab 1.1");
                    rc = 16;
                    break;
                case IFC_SPECVERSION:
                    version = (0x01) | (0x01 << 8);
                    rc = CopyTabletData(lpOutput, &version,sizeof(WORD));
                    break;
                case IFC_IMPLVERSION:
                    version = (0x00) | (0x01 << 8);
                    rc = CopyTabletData(lpOutput, &version,sizeof(WORD));
                    break;
                default:
                    FIXME("WTI_INTERFACE unhandled index %i\n",nIndex);
                    rc = 0;

            }
        case WTI_DEFSYSCTX:
        case WTI_DDCTXS:
        case WTI_DEFCONTEXT:
            switch (nIndex)
            {
                case 0:
                    memcpy(lpOutput, &gSysContext,
                            sizeof(LOGCONTEXTA));
                    rc = sizeof(LOGCONTEXTA);
                    break;
                case CTX_NAME:
                    rc = CopyTabletData(lpOutput, &gSysContext.lcName,
                         strlen(gSysContext.lcName)+1);
                    break;
                case CTX_OPTIONS:
                    rc = CopyTabletData(lpOutput, &gSysContext.lcOptions,
                                        sizeof(UINT));
                    break;
                case CTX_STATUS:
                    rc = CopyTabletData(lpOutput, &gSysContext.lcStatus,
                                        sizeof(UINT));
                    break;
                case CTX_LOCKS:
                    rc= CopyTabletData (lpOutput, &gSysContext.lcLocks,
                                        sizeof(UINT));
                    break;
                case CTX_MSGBASE:
                    rc = CopyTabletData(lpOutput, &gSysContext.lcMsgBase,
                                        sizeof(UINT));
                    break;
                case CTX_DEVICE:
                    rc = CopyTabletData(lpOutput, &gSysContext.lcDevice,
                                        sizeof(UINT));
                    break;
                case CTX_PKTRATE:
                    rc = CopyTabletData(lpOutput, &gSysContext.lcPktRate,
                                        sizeof(UINT));
                    break;
                case CTX_PKTMODE:
                    rc = CopyTabletData(lpOutput, &gSysContext.lcPktMode,
                                        sizeof(WTPKT));
                    break;
                case CTX_MOVEMASK:
                    rc = CopyTabletData(lpOutput, &gSysContext.lcMoveMask,
                                        sizeof(WTPKT));
                    break;
                case CTX_BTNDNMASK:
                    rc = CopyTabletData(lpOutput, &gSysContext.lcBtnDnMask,
                                        sizeof(DWORD));
                    break;
                case CTX_BTNUPMASK:
                    rc = CopyTabletData(lpOutput, &gSysContext.lcBtnUpMask,
                                        sizeof(DWORD));
                    break;
                case CTX_INORGX:
                    rc = CopyTabletData(lpOutput, &gSysContext.lcInOrgX,
                                        sizeof(LONG));
                    break;
                case CTX_INORGY:
                    rc = CopyTabletData(lpOutput, &gSysContext.lcInOrgY,
                                        sizeof(LONG));
                    break;
                case CTX_INORGZ:
                    rc = CopyTabletData(lpOutput, &gSysContext.lcInOrgZ,
                                        sizeof(LONG));
                    break;
                case CTX_INEXTX:
                    rc = CopyTabletData(lpOutput, &gSysContext.lcInExtX,
                                        sizeof(LONG));
                    break;
                case CTX_INEXTY:
                     rc = CopyTabletData(lpOutput, &gSysContext.lcInExtY,
                                        sizeof(LONG));
                    break;
                case CTX_INEXTZ:
                     rc = CopyTabletData(lpOutput, &gSysContext.lcInExtZ,
                                        sizeof(LONG));
                    break;
                case CTX_OUTORGX:
                     rc = CopyTabletData(lpOutput, &gSysContext.lcOutOrgX,
                                        sizeof(LONG));
                    break;
                case CTX_OUTORGY:
                      rc = CopyTabletData(lpOutput, &gSysContext.lcOutOrgY,
                                        sizeof(LONG));
                    break;
                case CTX_OUTORGZ:
                       rc = CopyTabletData(lpOutput, &gSysContext.lcOutOrgZ,
                                        sizeof(LONG));
                    break;
               case CTX_OUTEXTX:
                      rc = CopyTabletData(lpOutput, &gSysContext.lcOutExtX,
                                        sizeof(LONG));
                    break;
                case CTX_OUTEXTY:
                       rc = CopyTabletData(lpOutput, &gSysContext.lcOutExtY,
                                        sizeof(LONG));
                    break;
                case CTX_OUTEXTZ:
                       rc = CopyTabletData(lpOutput, &gSysContext.lcOutExtZ,
                                        sizeof(LONG));
                    break;
                case CTX_SENSX:
                        rc = CopyTabletData(lpOutput, &gSysContext.lcSensX,
                                        sizeof(LONG));
                    break;
                case CTX_SENSY:
                        rc = CopyTabletData(lpOutput, &gSysContext.lcSensY,
                                        sizeof(LONG));
                    break;
                case CTX_SENSZ:
                        rc = CopyTabletData(lpOutput, &gSysContext.lcSensZ,
                                        sizeof(LONG));
                    break;
                case CTX_SYSMODE:
                        rc = CopyTabletData(lpOutput, &gSysContext.lcSysMode,
                                        sizeof(LONG));
                    break;
                case CTX_SYSORGX:
                        rc = CopyTabletData(lpOutput, &gSysContext.lcSysOrgX,
                                        sizeof(LONG));
                    break;
                case CTX_SYSORGY:
                        rc = CopyTabletData(lpOutput, &gSysContext.lcSysOrgY,
                                        sizeof(LONG));
                    break;
                case CTX_SYSEXTX:
                        rc = CopyTabletData(lpOutput, &gSysContext.lcSysExtX,
                                        sizeof(LONG));
                    break;
                case CTX_SYSEXTY:
                        rc = CopyTabletData(lpOutput, &gSysContext.lcSysExtY,
                                        sizeof(LONG));
                    break;
                case CTX_SYSSENSX:
                        rc = CopyTabletData(lpOutput, &gSysContext.lcSysSensX,
                                        sizeof(LONG));
                    break;
                case CTX_SYSSENSY:
                       rc = CopyTabletData(lpOutput, &gSysContext.lcSysSensY,
                                        sizeof(LONG));
                    break;
                default:
                    FIXME("WTI_DEFSYSCTX unhandled index %i\n",nIndex);
                    rc = 0;
            }
            break;
        case WTI_CURSORS:
        case WTI_CURSORS+1:
        case WTI_CURSORS+2:
        case WTI_CURSORS+3:
        case WTI_CURSORS+4:
        case WTI_CURSORS+5:
        case WTI_CURSORS+6:
        case WTI_CURSORS+7:
        case WTI_CURSORS+8:
        case WTI_CURSORS+9:
        case WTI_CURSORS+10:
            tgtcursor = &gSysCursor[wCategory - WTI_CURSORS];
            switch (nIndex)
            {
                case CSR_NAME:
                    rc = CopyTabletData(lpOutput, &tgtcursor->NAME,
                                        strlen(tgtcursor->NAME)+1);
                    break;
                case CSR_ACTIVE:
                    rc = CopyTabletData(lpOutput,&tgtcursor->ACTIVE,
                                        sizeof(BOOL));
                    break;
                case CSR_PKTDATA:
                    rc = CopyTabletData(lpOutput,&tgtcursor->PKTDATA,
                                        sizeof(WTPKT));
                    break;
                case CSR_BUTTONS:
                    rc = CopyTabletData(lpOutput,&tgtcursor->BUTTONS,
                                        sizeof(BYTE));
                    break;
                case CSR_BUTTONBITS:
                    rc = CopyTabletData(lpOutput,&tgtcursor->BUTTONBITS,
                                        sizeof(BYTE));
                    break;
                case CSR_BTNNAMES:
                    FIXME("Button Names not returned correctly\n");
                    rc = CopyTabletData(lpOutput,&tgtcursor->BTNNAMES,
                                        strlen(tgtcursor->BTNNAMES)+1);
                    break;
                case CSR_BUTTONMAP:
                    rc = CopyTabletData(lpOutput,&tgtcursor->BUTTONMAP,
                                        sizeof(BYTE)*32);
                    break;
                case CSR_SYSBTNMAP:
                    rc = CopyTabletData(lpOutput,&tgtcursor->SYSBTNMAP,
                                        sizeof(BYTE)*32);
                    break;
                case CSR_NPBTNMARKS:
                    memcpy(lpOutput,&tgtcursor->NPBTNMARKS,sizeof(UINT)*2);
                    rc = sizeof(UINT)*2;
                    break;
                case CSR_NPBUTTON:
                    rc = CopyTabletData(lpOutput,&tgtcursor->NPBUTTON,
                                        sizeof(BYTE));
                    break;
                case CSR_NPRESPONSE:
                    FIXME("Not returning CSR_NPRESPONSE correctly\n");
                    rc = 0;
                    break;
                case CSR_TPBUTTON:
                    rc = CopyTabletData(lpOutput,&tgtcursor->TPBUTTON,
                                        sizeof(BYTE));
                    break;
                case CSR_TPBTNMARKS:
                    memcpy(lpOutput,&tgtcursor->TPBTNMARKS,sizeof(UINT)*2);
                    rc = sizeof(UINT)*2;
                    break;
                case CSR_TPRESPONSE:
                    FIXME("Not returning CSR_TPRESPONSE correctly\n");
                    rc = 0;
                    break;
                case CSR_PHYSID:
                {
                    DWORD id;
                    rc = CopyTabletData(&id,&tgtcursor->PHYSID,
                                        sizeof(DWORD));
                    id += (wCategory - WTI_CURSORS);
                    memcpy(lpOutput,&id,sizeof(DWORD));
                }
                    break;
                case CSR_MODE:
                    rc = CopyTabletData(lpOutput,&tgtcursor->MODE,sizeof(UINT));
                    break;
                case CSR_MINPKTDATA:
                    rc = CopyTabletData(lpOutput,&tgtcursor->MINPKTDATA,
                        sizeof(UINT));
                    break;
                case CSR_MINBUTTONS:
                    rc = CopyTabletData(lpOutput,&tgtcursor->MINBUTTONS,
                        sizeof(UINT));
                    break;
                case CSR_CAPABILITIES:
                    rc = CopyTabletData(lpOutput,&tgtcursor->CAPABILITIES,
                        sizeof(UINT));
                    break;
                case CSR_TYPE:
                    rc = CopyTabletData(lpOutput,&tgtcursor->TYPE,
                        sizeof(UINT));
                    break;
                default:
                    FIXME("WTI_CURSORS unhandled index %i\n",nIndex);
                    rc = 0;
            }
            break;
        case WTI_DEVICES:
            switch (nIndex)
            {
                case DVC_NAME:
                    rc = CopyTabletData(lpOutput,gSysDevice.NAME,
                                        strlen(gSysDevice.NAME)+1);
                    break;
                case DVC_HARDWARE:
                    rc = CopyTabletData(lpOutput,&gSysDevice.HARDWARE,
                                        sizeof(UINT));
                    break;
                case DVC_NCSRTYPES:
                    rc = CopyTabletData(lpOutput,&gSysDevice.NCSRTYPES,
                                        sizeof(UINT));
                    break;
                case DVC_FIRSTCSR:
                    rc = CopyTabletData(lpOutput,&gSysDevice.FIRSTCSR,
                                        sizeof(UINT));
                    break;
                case DVC_PKTRATE:
                    rc = CopyTabletData(lpOutput,&gSysDevice.PKTRATE,
                                        sizeof(UINT));
                    break;
                case DVC_PKTDATA:
                    rc = CopyTabletData(lpOutput,&gSysDevice.PKTDATA,
                                        sizeof(WTPKT));
                    break;
                case DVC_PKTMODE:
                    rc = CopyTabletData(lpOutput,&gSysDevice.PKTMODE,
                                        sizeof(WTPKT));
                    break;
                case DVC_CSRDATA:
                    rc = CopyTabletData(lpOutput,&gSysDevice.CSRDATA,
                                        sizeof(WTPKT));
                    break;
                case DVC_XMARGIN:
                    rc = CopyTabletData(lpOutput,&gSysDevice.XMARGIN,
                                        sizeof(INT));
                    break;
                case DVC_YMARGIN:
                    rc = CopyTabletData(lpOutput,&gSysDevice.YMARGIN,
                                        sizeof(INT));
                    break;
                case DVC_ZMARGIN:
                    rc = 0; /* unsupported */
                    /*
                    rc = CopyTabletData(lpOutput,&gSysDevice.ZMARGIN,
                                        sizeof(INT));
                    */
                    break;
                case DVC_X:
                    rc = CopyTabletData(lpOutput,&gSysDevice.X,
                                        sizeof(AXIS));
                    break;
                case DVC_Y:
                    rc = CopyTabletData(lpOutput,&gSysDevice.Y,
                                        sizeof(AXIS));
                    break;
                case DVC_Z:
                    rc = 0; /* unsupported */
                    /*
                    rc = CopyTabletData(lpOutput,&gSysDevice.Z,
                                        sizeof(AXIS));
                    */
                    break;
                case DVC_NPRESSURE:
                    rc = CopyTabletData(lpOutput,&gSysDevice.NPRESSURE,
                                        sizeof(AXIS));
                    break;
                case DVC_TPRESSURE:
                    rc = 0; /* unsupported */
                    /*
                    rc = CopyTabletData(lpOutput,&gSysDevice.TPRESSURE,
                                        sizeof(AXIS));
                    */
                    break;
                case DVC_ORIENTATION:
                    memcpy(lpOutput,&gSysDevice.ORIENTATION,sizeof(AXIS)*3);
                    rc = sizeof(AXIS)*3;
                    break;
                case DVC_ROTATION:
                    rc = 0; /* unsupported */
                    /*
                    memcpy(lpOutput,&gSysDevice.ROTATION,sizeof(AXIS)*3);
                    rc = sizeof(AXIS)*3;
                    */
                    break;
                case DVC_PNPID:
                    rc = CopyTabletData(lpOutput,gSysDevice.PNPID,
                                        strlen(gSysDevice.PNPID)+1);
                    break;
                default:
                    FIXME("WTI_DEVICES unhandled index %i\n",nIndex);
                    rc = 0;
            }
            break;
        default:
            FIXME("Unhandled Category %i\n",wCategory);
    }
    return rc;
}

#else /* HAVE_X11_EXTENSIONS_XINPUT_H */

int X11DRV_ProcessTabletEvent(HWND hwnd, XEvent *event)
{
    return 0;
}

/***********************************************************************
 *		AttachEventQueueToTablet (X11DRV.@)
 */
int X11DRV_AttachEventQueueToTablet(HWND hOwner)
{
    return 0;
}

/***********************************************************************
 *		GetCurrentPacket (X11DRV.@)
 */
int X11DRV_GetCurrentPacket(LPWTPACKET *packet)
{
    return 0;
}

/***********************************************************************
 *		LoadTabletInfo (X11DRV.@)
 */
void X11DRV_LoadTabletInfo(HWND hwnddefault)
{
}

/***********************************************************************
 *		WTInfoA (X11DRV.@)
 */
UINT X11DRV_WTInfoA(UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
    return 0;
}

#endif /* HAVE_X11_EXTENSIONS_XINPUT_H */
