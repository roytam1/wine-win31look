/*
 * IFSMGR VxD implementation
 *
 * Copyright 1998 Marcus Meissner
 * Copyright 1998 Ulrich Weigand
 * Copyright 1998 Patrik Stridvall
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

/* NOTES
 *   These ioctls are used by 'MSNET32.DLL'.
 *
 *   I have been unable to uncover any documentation about the ioctls so
 *   the implementation of the cases IFS_IOCTL_21 and IFS_IOCTL_2F are
 *   based on reasonable guesses on information found in the Windows 95 DDK.
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(vxd);

extern void WINAPI CallBuiltinHandler( CONTEXT86 *context, BYTE intnum );  /* from winedos */

/*
 * IFSMgr DeviceIO service
 */

#define IFS_IOCTL_21                100
#define IFS_IOCTL_2F                101
#define IFS_IOCTL_GET_RES           102
#define IFS_IOCTL_GET_NETPRO_NAME_A 103

struct win32apireq {
        unsigned long   ar_proid;
        unsigned long   ar_eax;
        unsigned long   ar_ebx;
        unsigned long   ar_ecx;
        unsigned long   ar_edx;
        unsigned long   ar_esi;
        unsigned long   ar_edi;
        unsigned long   ar_ebp;
        unsigned short  ar_error;
        unsigned short  ar_pad;
};

static void win32apieq_2_CONTEXT(struct win32apireq *pIn,CONTEXT86 *pCxt)
{
        memset(pCxt,0,sizeof(*pCxt));

        pCxt->ContextFlags=CONTEXT86_INTEGER|CONTEXT86_CONTROL;
        pCxt->Eax = pIn->ar_eax;
        pCxt->Ebx = pIn->ar_ebx;
        pCxt->Ecx = pIn->ar_ecx;
        pCxt->Edx = pIn->ar_edx;
        pCxt->Esi = pIn->ar_esi;
        pCxt->Edi = pIn->ar_edi;

        /* FIXME: Only partial CONTEXT86_CONTROL */
        pCxt->Ebp = pIn->ar_ebp;

        /* FIXME: pIn->ar_proid ignored */
        /* FIXME: pIn->ar_error ignored */
        /* FIXME: pIn->ar_pad ignored */
}

static void CONTEXT_2_win32apieq(CONTEXT86 *pCxt,struct win32apireq *pOut)
{
        memset(pOut,0,sizeof(struct win32apireq));

        pOut->ar_eax = pCxt->Eax;
        pOut->ar_ebx = pCxt->Ebx;
        pOut->ar_ecx = pCxt->Ecx;
        pOut->ar_edx = pCxt->Edx;
        pOut->ar_esi = pCxt->Esi;
        pOut->ar_edi = pCxt->Edi;

        /* FIXME: Only partial CONTEXT86_CONTROL */
        pOut->ar_ebp = pCxt->Ebp;

        /* FIXME: pOut->ar_proid ignored */
        /* FIXME: pOut->ar_error ignored */
        /* FIXME: pOut->ar_pad ignored */
}

/***********************************************************************
 *           DeviceIoControl   (IFSMGR.VXD.@)
 */
BOOL WINAPI IFSMGR_DeviceIoControl(DWORD dwIoControlCode, LPVOID lpvInBuffer, DWORD cbInBuffer,
                                  LPVOID lpvOutBuffer, DWORD cbOutBuffer,
                                  LPDWORD lpcbBytesReturned,
                                  LPOVERLAPPED lpOverlapped)
{
    TRACE("(%ld,%p,%ld,%p,%ld,%p,%p): stub\n",
          dwIoControlCode, lpvInBuffer,cbInBuffer, lpvOutBuffer,cbOutBuffer,
          lpcbBytesReturned, lpOverlapped);

    switch (dwIoControlCode)
    {
    case IFS_IOCTL_21:
    case IFS_IOCTL_2F:
        {
            CONTEXT86 cxt;
            struct win32apireq *pIn=(struct win32apireq *) lpvInBuffer;
            struct win32apireq *pOut=(struct win32apireq *) lpvOutBuffer;

            TRACE( "Control '%s': "
                   "proid=0x%08lx, eax=0x%08lx, ebx=0x%08lx, ecx=0x%08lx, "
                   "edx=0x%08lx, esi=0x%08lx, edi=0x%08lx, ebp=0x%08lx, "
                   "error=0x%04x, pad=0x%04x\n",
                   (dwIoControlCode==IFS_IOCTL_21)?"IFS_IOCTL_21":"IFS_IOCTL_2F",
                   pIn->ar_proid, pIn->ar_eax, pIn->ar_ebx, pIn->ar_ecx,
                   pIn->ar_edx, pIn->ar_esi, pIn->ar_edi, pIn->ar_ebp,
                   pIn->ar_error, pIn->ar_pad );

            win32apieq_2_CONTEXT(pIn,&cxt);

            if(dwIoControlCode==IFS_IOCTL_21)
                CallBuiltinHandler( &cxt, 0x21 );
            else
                CallBuiltinHandler( &cxt, 0x2f );

            CONTEXT_2_win32apieq(&cxt,pOut);
            return TRUE;
        }
    case IFS_IOCTL_GET_RES:
        FIXME( "Control 'IFS_IOCTL_GET_RES' not implemented\n");
        return FALSE;
    case IFS_IOCTL_GET_NETPRO_NAME_A:
        FIXME( "Control 'IFS_IOCTL_GET_NETPRO_NAME_A' not implemented\n");
        return FALSE;
    default:
        FIXME( "Control %ld not implemented\n", dwIoControlCode);
        return FALSE;
    }
}
