/*
 * DOS interrupt 34->3e handlers.  All FPU interrupt code should be
 * moved into this file.
 *
 * Copyright 2002 Robert 'Admiral' Coeyman
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

#include <stdlib.h>
#include "dosexe.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(int);

/*
 *  The actual work is done by a single routine.
 */

static void FPU_ModifyCode(CONTEXT86 *context, BYTE Opcode);


/**********************************************************************
 *          DOSVM_Int34Handler (WINEDOS16.152)
 *
 * Handler for int 34 (FLOATING POINT EMULATION - Opcode 0xd8).
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int34Handler(CONTEXT86 *context)
{
    TRACE("Int 0x34 called-- FP opcode 0xd8\n");
    FPU_ModifyCode(context, 0xd8);
}


/**********************************************************************
 *          DOSVM_Int35Handler (WINEDOS16.153)
 *
 * Handler for int 35 (FLOATING POINT EMULATION - Opcode 0xd9).
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int35Handler(CONTEXT86 *context)
{
    TRACE("Int 0x35 called-- FP opcode 0xd9\n");
    FPU_ModifyCode(context, 0xd9);
}


/**********************************************************************
 *          DOSVM_Int36Handler (WINEDOS16.154)
 *
 * Handler for int 36 (FLOATING POINT EMULATION - Opcode 0xda).
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int36Handler(CONTEXT86 *context)
{
    TRACE("Int 0x36 called-- FP opcode 0xda\n");
    FPU_ModifyCode(context, 0xda);
}


/**********************************************************************
 *          DOSVM_Int37Handler (WINEDOS16.155)
 *
 * Handler for int 37 (FLOATING POINT EMULATION - Opcode 0xdb).
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int37Handler(CONTEXT86 *context)
{
    TRACE("Int 0x37 called-- FP opcode 0xdb\n");
    FPU_ModifyCode(context, 0xdb);
}


/**********************************************************************
 *          DOSVM_Int38Handler (WINEDOS16.156)
 *
 * Handler for int 38 (FLOATING POINT EMULATION - Opcode 0xdc).
 *
 *  Between versions 3.0 and 5.01, the original PC-MOS API call that
 *  was here was moved to int 0xd4.
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int38Handler(CONTEXT86 *context)
{
    TRACE("Int 0x38 called-- FP opcode 0xdc\n");
    FPU_ModifyCode(context, 0xdc);
}


/**********************************************************************
 *          DOSVM_Int39Handler (WINEDOS16.157)
 *
 * Handler for int 39 (FLOATING POINT EMULATION - Opcode 0xdd).
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int39Handler(CONTEXT86 *context)
{
    TRACE("Int 0x39 called-- FP opcode 0xdd\n");
    FPU_ModifyCode(context, 0xdd);
}


/**********************************************************************
 *          DOSVM_Int3aHandler (WINEDOS16.158)
 *
 * Handler for int 3a (FLOATING POINT EMULATION - Opcode 0xde).
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int3aHandler(CONTEXT86 *context)
{
    TRACE("Int 0x3a called-- FP opcode 0xde\n");
    FPU_ModifyCode(context, 0xde);
}


/**********************************************************************
 *          DOSVM_Int3bHandler (WINEDOS16.159)
 *
 * Handler for int 3B (FLOATING POINT EMULATION - Opcode 0xdf).
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int3bHandler(CONTEXT86 *context)
{
    TRACE("Int 0x3b called-- FP opcode 0xdf\n");
    FPU_ModifyCode(context, 0xdf);
}


/**********************************************************************
 *          DOSVM_Int3cHandler (WINEDOS16.160)
 *
 * Handler for int 3C (FLOATING POINT EMULATION - INSTRUCTIONS WITH SEGMENT OVERRIDE).
 *
 *  Generated code is CD 3C xy mm ... (CD = int | 3C = this interrupt)
 *   xy is a modified ESC code and mm is the modR/M byte.
 *   xy byte seems to be encoded as ss011xxx  or ss000xxx
 *   ss= segment override.
 *     00 -> DS
 *     01 -> SS
 *     10 -> CS
 *     11 -> ES
 *
 *  11011xxx should be the opcode instruction.
 */
void WINAPI DOSVM_Int3cHandler(CONTEXT86 *context)
{
    FIXME("Int 3C NOT Implemented\n");
    INT_BARF(context, 0x3c);
}


/**********************************************************************
 *          DOSVM_Int3dHandler (WINEDOS16.161)
 *
 * Handler for int 3D (FLOATING POINT EMULATION - Standalone FWAIT).
 *
 *  Opcode 0x90 is a NOP.  It just fills space where the 3D was.
 */
void WINAPI DOSVM_Int3dHandler(CONTEXT86 *context)
{
    TRACE("Int 0x3d called-- Standalone FWAIT\n");
    FPU_ModifyCode(context, 0x90);
}


/**********************************************************************
 *          DOSVM_Int3eHandler (WINEDOS16.162)
 *
 * FLOATING POINT EMULATION -- Borland "Shortcut" call.
 *  The two bytes following the int 3E instruction are
 *  the subcode and a NOP ( 0x90 ), except for subcodes DC and DE
 *  where the second byte is the register count.
 *
 *  Direct access 4.0 modifies and does not restore this vector.
 *
 */
void WINAPI DOSVM_Int3eHandler(CONTEXT86 *context)
{
    FIXME("Int 3E NOT Implemented\n");
    INT_BARF(context, 0x3e);
}


/**********************************************************************
 *          FPU_ModifyCode
 *
 *   This is the function that inserts the 0x9b fwait instruction
 *   and the actual FPU opcode into the program.
 *           -A.C.
 *
 *               Code thanks to Ove Kaaven
 */
static void FPU_ModifyCode(CONTEXT86 *context, BYTE Opcode)
{
    BYTE *code = CTX_SEG_OFF_TO_LIN(context, context->SegCs, context->Eip);

    /*
     * All *NIX systems should have a real or kernel emulated FPU.
     */

    code[-2] = 0x9b;          /* The fwait instruction */
    code[-1] = Opcode;        /* Insert the opcode     */

    if ( ISV86(context) && LOWORD(context->Eip) < 2 )
        FIXME("Backed up over a real mode segment boundary in FPU code.\n");

    context->Eip -= 2; /* back up the return address 2 bytes */

    TRACE("Modified code in FPU int call to 0x9b 0x%x\n",Opcode);
}
