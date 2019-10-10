/*
 * Copyright 1995 Thomas Sandford (tdgsandf@prds-grn.demon.co.uk)
 * Copyright 2003 Dimitrie O. Paun
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

#include <stdarg.h>

#include "ntstatus.h"
#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(powermgnt);

/******************************************************************************
 *           GetDevicePowerState   (KERNEL32.@)
 */
BOOL WINAPI GetDevicePowerState(HANDLE hDevice, BOOL* pfOn)
{
    FIXME("(hDevice %p pfOn %p): stub\n", hDevice, pfOn);
    return TRUE; /* no information */
}

/***********************************************************************
 *           GetSystemPowerStatus      (KERNEL32.@)
 */
BOOL WINAPI GetSystemPowerStatus(LPSYSTEM_POWER_STATUS sps_ptr)
{
    FIXME("(): stub, harmless.\n");
    return FALSE;   /* no power management support */
}

/***********************************************************************
 *           IsSystemResumeAutomatic   (KERNEL32.@)
 */
BOOL WINAPI IsSystemResumeAutomatic(void)
{
    FIXME("(): stub, harmless.\n");
    return FALSE;
}

/***********************************************************************
 *           RequestWakeupLatency      (KERNEL32.@)
 */
BOOL WINAPI RequestWakeupLatency(LATENCY_TIME latency)
{
    FIXME("(): stub, harmless.\n");
    return TRUE;
}

/***********************************************************************
 *           SetSystemPowerState      (KERNEL32.@)
 */
BOOL WINAPI SetSystemPowerState(BOOL suspend_or_hibernate,
                                  BOOL force_flag)
{
    FIXME("(): stub, harmless.\n");
    /* suspend_or_hibernate flag: w95 does not support
       this feature anyway */

    for ( ;0; )
    {
        if ( force_flag )
        {
        }
        else
        {
        }
    }
    return TRUE;
}

/***********************************************************************
 * SetThreadExecutionState (KERNEL32.@)
 *
 * Informs the system that activity is taking place for
 * power management purposes.
 */
EXECUTION_STATE WINAPI SetThreadExecutionState(EXECUTION_STATE flags)
{
    static EXECUTION_STATE current =
        ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED | ES_USER_PRESENT;
    EXECUTION_STATE old = current;

    FIXME("(0x%lx): stub, harmless.\n", flags);

    if (!(current & ES_CONTINUOUS) || (flags & ES_CONTINUOUS))
        current = flags;
    return old;
}
