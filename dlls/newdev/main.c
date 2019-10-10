/*
 * NewDev
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

#include "windef.h"
#include "winerror.h"

#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(newdev);

/***********************************************************************
 *           InstallNewDevice (NEWDEV.@)
 */
BOOL WINAPI InstallNewDevice(HWND hwndParent, LPGUID ClassGuid, PDWORD pReboot)
{
    FIXME("Stub!\n");
    return TRUE;
}


/***********************************************************************
 *           UpdateDriverForPlugAndPlayDevicesA (NEWDEV.@)
 */
BOOL WINAPI UpdateDriverForPlugAndPlayDevicesA(HWND hwndParent, LPCSTR HardwareId,
    LPCSTR FullInfPath, DWORD InstallFlags, PBOOL bRebootRequired OPTIONAL)
{
    FIXME("Stub! %s %s 0x%08lx\n", HardwareId, FullInfPath, InstallFlags);
    return TRUE;
}


/***********************************************************************
 *           UpdateDriverForPlugAndPlayDevicesW (NEWDEV.@)
 */
BOOL WINAPI UpdateDriverForPlugAndPlayDevicesW(HWND hwndParent, LPCWSTR HardwareId,
    LPCWSTR FullInfPath, DWORD InstallFlags, PBOOL bRebootRequired OPTIONAL)
{
    FIXME("Stub! %s %s 0x%08lx\n", debugstr_w(HardwareId), debugstr_w(FullInfPath), InstallFlags);
    return TRUE;
}
