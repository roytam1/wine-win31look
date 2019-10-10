/*
 * BIOS interrupt 11h handler
 *
 * Copyright 1996 Alexandre Julliard
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "dosexe.h"
#include "wine/unicode.h"
#include "wine/debug.h"


/**********************************************************************
 *	    DOSVM_Int11Handler (WINEDOS16.117)
 *
 * Handler for int 11h (get equipment list).
 *
 *
 * Borrowed from Ralph Brown's interrupt lists:
 *
 *   bits 15-14: number of parallel devices
 *   bit     13: [Conv] Internal modem
 *   bit     12: reserved
 *   bits 11- 9: number of serial devices
 *   bit      8: reserved
 *   bits  7- 6: number of diskette drives minus one
 *   bits  5- 4: Initial video mode:
 *                 00b = EGA,VGA,PGA
 *                 01b = 40 x 25 color
 *                 10b = 80 x 25 color
 *                 11b = 80 x 25 mono
 *   bit      3: reserved
 *   bit      2: [PS] =1 if pointing device
 *               [non-PS] reserved
 *   bit      1: =1 if math co-processor
 *   bit      0: =1 if diskette available for boot
 *
 *
 * Currently the only of these bits correctly set are:
 *
 *   bits 15-14   } Added by William Owen Smith,
 *   bits 11-9    } wos@dcs.warwick.ac.uk
 *   bits 7-6
 *   bit  2       (always set)  ( bit 2 = 4 )
 *   bit  1       } Robert 'Admiral' Coeyman
 *                  All *nix systems either have a math processor or
 *		     emulate one.
 */
void WINAPI DOSVM_Int11Handler( CONTEXT86 *context )
{
    int diskdrives = 0;
    int parallelports = 0;
    int serialports = 0;
    int x;

    if (GetDriveTypeA("A:\\") == DRIVE_REMOVABLE) diskdrives++;
    if (GetDriveTypeA("B:\\") == DRIVE_REMOVABLE) diskdrives++;
    if (diskdrives) diskdrives--;

    for (x=0; x < 9; x++)
    {        
        HKEY hkey;
        char option[10];
        char temp[256];

        /* serial port name */
        strcpy( option, "COMx" );
        option[3] = '1' + x;
        option[4] = '\0';

        /* default value */
        strcpy( temp, "*" );

        if (!RegOpenKeyA(HKEY_LOCAL_MACHINE, 
                         "Software\\Wine\\Wine\\Config\\serialports", 
                         &hkey))
        {
            DWORD type;
            DWORD count = sizeof(temp);
            RegQueryValueExA( hkey, option, 0, &type, temp, &count );
            RegCloseKey( hkey );
        }

        if (strcmp(temp, "*") && *temp != '\0')
            serialports++;

        /* parallel port name */
        strcpy( option, "LPTx" );
        option[3] = '1' + x;
        option[4] = '\0';

        /* default value */
        strcpy( temp, "*" );

        if (!RegOpenKeyA(HKEY_LOCAL_MACHINE,
                         "Software\\Wine\\Wine\\Config\\parallelports", 
                         &hkey))
        {
            DWORD type;
            DWORD count = sizeof(temp);
            RegQueryValueExA( hkey, option, 0, &type, temp, &count );
            RegCloseKey( hkey );
        }

        if (strcmp(temp, "*") && *temp != '\0')
            parallelports++;
    }

    if (serialports > 7) /* 3 bits -- maximum value = 7 */
        serialports = 7;

    if (parallelports > 3) /* 2 bits -- maximum value = 3 */
        parallelports = 3;

    SET_AX( context, 
            (diskdrives << 6) | (serialports << 9) | (parallelports << 14) | 0x06 );
}
