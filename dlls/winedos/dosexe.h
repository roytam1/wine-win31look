/*
 * DOS EXE loader
 *
 * Copyright 1998 Ove K�ven
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

#ifndef __WINE_DOSEXE_H
#define __WINE_DOSEXE_H

#include <stdarg.h>

#include "windef.h"
#include "wine/windef16.h"
#include "winbase.h"
#include "winnt.h"     /* for PCONTEXT */
#include "wincon.h"    /* for MOUSE_EVENT_RECORD */
#include "miscemu.h"

#define MAX_DOS_DRIVES  26

struct _DOSEVENT;

/* amount of space reserved for relay stack */
#define DOSVM_RELAY_DATA_SIZE 4096

/* various real-mode code stubs */
struct DPMI_segments
{
    WORD wrap_seg;
    WORD xms_seg;
    WORD dpmi_seg;
    WORD dpmi_sel;
    WORD int48_sel;
    WORD int16_sel;
    WORD relay_code_sel;
    WORD relay_data_sel;
};

/* 48-bit segmented pointers for DOS DPMI32 */
typedef struct {
  WORD  selector;
  DWORD offset;
} SEGPTR48, FARPROC48;

#define DOSCONF_MEM_HIGH        0x0001
#define DOSCONF_MEM_UMB         0x0002
#define DOSCONF_NUMLOCK         0x0004
#define DOSCONF_KEYB_CONV       0x0008

typedef struct {
    char lastdrive;
    int brk_flag;
    int files;
    int stacks_nr;
    int stacks_sz;
    int buf;
    int buf2;
    int fcbs;
    int flags;
    char *shell;
    char *country;
} DOSCONF;

typedef void (*DOSRELAY)(CONTEXT86*,void*);
typedef void (WINAPI *RMCBPROC)(CONTEXT86*);
typedef void (WINAPI *INTPROC)(CONTEXT86*);

#define DOS_PRIORITY_REALTIME 0  /* IRQ0 */
#define DOS_PRIORITY_KEYBOARD 1  /* IRQ1 */
#define DOS_PRIORITY_VGA      2  /* IRQ9 */
#define DOS_PRIORITY_MOUSE    5  /* IRQ12 */
#define DOS_PRIORITY_SERIAL   10 /* IRQ4 */

extern WORD DOSVM_psp;     /* psp of current DOS task */
extern WORD DOSVM_retval;  /* return value of previous DOS task */
extern struct DPMI_segments *DOSVM_dpmi_segments;

#if defined(linux) && defined(__i386__) && defined(HAVE_SYS_VM86_H)
# define MZ_SUPPORTED
#endif /* linux-i386 */

/*
 * Declare some CONTEXT86.EFlags bits.
 * IF_MASK is only pushed into real mode stack.
 */
#define V86_FLAG 0x00020000
#define IF_MASK  0x00000200
#define VIF_MASK 0x00080000
#define VIP_MASK 0x00100000

#define ADD_LOWORD(dw,val)  ((dw) = ((dw) & 0xffff0000) | LOWORD((DWORD)(dw)+(val)))

#define PTR_REAL_TO_LIN(seg,off) ((void*)(((unsigned int)(seg) << 4) + LOWORD(off)))

/* NOTE: Interrupts might get called from four modes: real mode, 16-bit,
 *       32-bit segmented (DPMI32) and 32-bit linear (via DeviceIoControl).
 *       For automatic conversion of pointer
 *       parameters, interrupt handlers should use CTX_SEG_OFF_TO_LIN with
 *       the contents of a segment register as second and the contents of
 *       a *32-bit* general register as third parameter, e.g.
 *          CTX_SEG_OFF_TO_LIN( context, DS_reg(context), EDX_reg(context) )
 *       This will generate a linear pointer in all three cases:
 *         Real-Mode:   Seg*16 + LOWORD(Offset)
 *         16-bit:      convert (Seg, LOWORD(Offset)) to linear
 *         32-bit segmented: convert (Seg, Offset) to linear
 *         32-bit linear:    use Offset as linear address (DeviceIoControl!)
 *
 *       Real-mode is recognized by checking the V86 bit in the flags register,
 *       32-bit linear mode is recognized by checking whether 'seg' is
 *       a system selector (0 counts also as 32-bit segment) and 32-bit
 *       segmented mode is recognized by checking whether 'seg' is 32-bit
 *       selector which is neither system selector nor zero.
 */
#define CTX_SEG_OFF_TO_LIN(context,seg,off) \
    (ISV86(context) ? PTR_REAL_TO_LIN((seg),(off)) : wine_ldt_get_ptr((seg),(off)))

#define INT_BARF(context,num) \
    ERR( "int%x: unknown/not implemented parameters:\n" \
                     "int%x: AX %04x, BX %04x, CX %04x, DX %04x, " \
                     "SI %04x, DI %04x, DS %04x, ES %04x\n", \
             (num), (num), LOWORD((context)->Eax), LOWORD((context)->Ebx), \
             LOWORD((context)->Ecx), LOWORD((context)->Edx), LOWORD((context)->Esi), \
             LOWORD((context)->Edi), (WORD)(context)->SegDs, (WORD)(context)->SegEs )

/* Macros for easier access to i386 context registers */

#define AX_reg(context)      ((WORD)(context)->Eax)
#define BX_reg(context)      ((WORD)(context)->Ebx)
#define CX_reg(context)      ((WORD)(context)->Ecx)
#define DX_reg(context)      ((WORD)(context)->Edx)
#define SI_reg(context)      ((WORD)(context)->Esi)
#define DI_reg(context)      ((WORD)(context)->Edi)

#define AL_reg(context)      ((BYTE)(context)->Eax)
#define AH_reg(context)      ((BYTE)((context)->Eax >> 8))
#define BL_reg(context)      ((BYTE)(context)->Ebx)
#define BH_reg(context)      ((BYTE)((context)->Ebx >> 8))
#define CL_reg(context)      ((BYTE)(context)->Ecx)
#define CH_reg(context)      ((BYTE)((context)->Ecx >> 8))
#define DL_reg(context)      ((BYTE)(context)->Edx)
#define DH_reg(context)      ((BYTE)((context)->Edx >> 8))

#define SET_CFLAG(context)   ((context)->EFlags |= 0x0001)
#define RESET_CFLAG(context) ((context)->EFlags &= ~0x0001)
#define SET_ZFLAG(context)   ((context)->EFlags |= 0x0040)
#define RESET_ZFLAG(context) ((context)->EFlags &= ~0x0040)
#define ISV86(context)       ((context)->EFlags & 0x00020000)

#define SET_AX(context,val)  ((void)((context)->Eax = ((context)->Eax & ~0xffff) | (WORD)(val)))
#define SET_BX(context,val)  ((void)((context)->Ebx = ((context)->Ebx & ~0xffff) | (WORD)(val)))
#define SET_CX(context,val)  ((void)((context)->Ecx = ((context)->Ecx & ~0xffff) | (WORD)(val)))
#define SET_DX(context,val)  ((void)((context)->Edx = ((context)->Edx & ~0xffff) | (WORD)(val)))
#define SET_SI(context,val)  ((void)((context)->Esi = ((context)->Esi & ~0xffff) | (WORD)(val)))
#define SET_DI(context,val)  ((void)((context)->Edi = ((context)->Edi & ~0xffff) | (WORD)(val)))

#define SET_AL(context,val)  ((void)((context)->Eax = ((context)->Eax & ~0xff) | (BYTE)(val)))
#define SET_BL(context,val)  ((void)((context)->Ebx = ((context)->Ebx & ~0xff) | (BYTE)(val)))
#define SET_CL(context,val)  ((void)((context)->Ecx = ((context)->Ecx & ~0xff) | (BYTE)(val)))
#define SET_DL(context,val)  ((void)((context)->Edx = ((context)->Edx & ~0xff) | (BYTE)(val)))

#define SET_AH(context,val)  ((void)((context)->Eax = ((context)->Eax & ~0xff00) | (((BYTE)(val)) << 8)))
#define SET_BH(context,val)  ((void)((context)->Ebx = ((context)->Ebx & ~0xff00) | (((BYTE)(val)) << 8)))
#define SET_CH(context,val)  ((void)((context)->Ecx = ((context)->Ecx & ~0xff00) | (((BYTE)(val)) << 8)))
#define SET_DH(context,val)  ((void)((context)->Edx = ((context)->Edx & ~0xff00) | (((BYTE)(val)) << 8)))

/* module.c */
extern void WINAPI MZ_LoadImage( LPCSTR filename, HANDLE hFile );
extern BOOL WINAPI MZ_Exec( CONTEXT86 *context, LPCSTR filename, BYTE func, LPVOID paramblk );
extern void WINAPI MZ_Exit( CONTEXT86 *context, BOOL cs_psp, WORD retval );
extern BOOL WINAPI MZ_Current( void );
extern void WINAPI MZ_AllocDPMITask( void );
extern void WINAPI MZ_RunInThread( PAPCFUNC proc, ULONG_PTR arg );
extern BOOL DOSVM_IsWin16(void);

/* dosvm.c */
extern void DOSVM_SendQueuedEvents( CONTEXT86 * );
extern void WINAPI DOSVM_AcknowledgeIRQ( CONTEXT86 * );
extern INT WINAPI DOSVM_Enter( CONTEXT86 *context );
extern void WINAPI DOSVM_Wait( CONTEXT86 * );
extern DWORD WINAPI DOSVM_Loop( HANDLE hThread );
extern void WINAPI DOSVM_QueueEvent( INT irq, INT priority, DOSRELAY relay, LPVOID data );
extern void WINAPI DOSVM_PIC_ioport_out( WORD port, BYTE val );
extern void WINAPI DOSVM_SetTimer( UINT ticks );
extern UINT WINAPI DOSVM_GetTimer( void );
extern BIOSDATA   *DOSVM_BiosData( void );

/* devices.c */
extern void DOSDEV_InstallDOSDevices(void);
extern DWORD DOSDEV_Console(void);
extern DWORD DOSDEV_FindCharDevice(char*name);
extern int DOSDEV_Peek(DWORD dev, BYTE*data);
extern int DOSDEV_Read(DWORD dev, DWORD buf, int buflen);
extern int DOSDEV_Write(DWORD dev, DWORD buf, int buflen, int verify);
extern int DOSDEV_IoctlRead(DWORD dev, DWORD buf, int buflen);
extern int DOSDEV_IoctlWrite(DWORD dev, DWORD buf, int buflen);
extern void DOSDEV_SetSharingRetry(WORD delay, WORD count);
extern SEGPTR DOSDEV_GetLOL(BOOL v86);

/* dma.c */
extern int DMA_Transfer(int channel,int reqlength,void* buffer);
extern void DMA_ioport_out( WORD port, BYTE val );
extern BYTE DMA_ioport_in( WORD port );

/* dosaspi.c */
void WINAPI DOSVM_ASPIHandler(CONTEXT86*);

/* dosconf.c */
DOSCONF *DOSCONF_GetConfig( void );

/* fpu.c */
extern void WINAPI DOSVM_Int34Handler(CONTEXT86*);
extern void WINAPI DOSVM_Int35Handler(CONTEXT86*);
extern void WINAPI DOSVM_Int36Handler(CONTEXT86*);
extern void WINAPI DOSVM_Int37Handler(CONTEXT86*);
extern void WINAPI DOSVM_Int38Handler(CONTEXT86*);
extern void WINAPI DOSVM_Int39Handler(CONTEXT86*);
extern void WINAPI DOSVM_Int3aHandler(CONTEXT86*);
extern void WINAPI DOSVM_Int3bHandler(CONTEXT86*);
extern void WINAPI DOSVM_Int3cHandler(CONTEXT86*);
extern void WINAPI DOSVM_Int3dHandler(CONTEXT86*);
extern void WINAPI DOSVM_Int3eHandler(CONTEXT86*);

/* himem.c */
extern void DOSVM_InitSegments(void);
extern LPVOID DOSVM_AllocUMB(DWORD);
extern LPVOID DOSVM_AllocCodeUMB(DWORD, WORD *, WORD *);
extern LPVOID DOSVM_AllocDataUMB(DWORD, WORD *, WORD *);

/* int09.c */
extern void WINAPI DOSVM_Int09Handler(CONTEXT86*);
extern void WINAPI DOSVM_Int09SendScan(BYTE scan,BYTE ascii);
extern BYTE WINAPI DOSVM_Int09ReadScan(BYTE*ascii);

/* int10.c */
extern void WINAPI DOSVM_Int10Handler(CONTEXT86*);
extern void WINAPI DOSVM_PutChar(BYTE ascii);

/* int11.c */
extern void WINAPI DOSVM_Int11Handler(CONTEXT86*);

/* int12.c */
extern void WINAPI DOSVM_Int12Handler(CONTEXT86*);

/* int13.c */
extern void WINAPI DOSVM_Int13Handler(CONTEXT86*);

/* int15.c */
extern void WINAPI DOSVM_Int15Handler(CONTEXT86*);

/* int1a.c */
extern void WINAPI DOSVM_Int1aHandler(CONTEXT86*);

/* int16.c */
extern void WINAPI DOSVM_Int16Handler(CONTEXT86*);
extern BOOL WINAPI DOSVM_Int16ReadChar( BYTE *, BYTE *, CONTEXT86 * );
extern int WINAPI DOSVM_Int16AddChar(BYTE ascii,BYTE scan);

/* int17.c */
extern void WINAPI DOSVM_Int17Handler(CONTEXT86*);

/* int19.c */
extern void WINAPI DOSVM_Int19Handler(CONTEXT86*);

/* int20.c */
extern void WINAPI DOSVM_Int20Handler(CONTEXT86*);

/* int21.c */
extern void WINAPI DOSVM_Int21Handler(CONTEXT86*);

/* int25.c */
BOOL DOSVM_RawRead( BYTE, DWORD, DWORD, BYTE *, BOOL );
void WINAPI DOSVM_Int25Handler( CONTEXT86 * );

/* int26.c */
BOOL DOSVM_RawWrite( BYTE, DWORD, DWORD, BYTE *, BOOL );
void WINAPI DOSVM_Int26Handler( CONTEXT86 * );

/* int29.c */
extern void WINAPI DOSVM_Int29Handler(CONTEXT86*);

/* int2a.c */
extern void WINAPI DOSVM_Int2aHandler(CONTEXT86*);

/* int2f.c */
extern void WINAPI DOSVM_Int2fHandler(CONTEXT86*);

/* int31.c */
extern void WINAPI DOSVM_Int31Handler(CONTEXT86*);
extern void WINAPI DOSVM_RawModeSwitchHandler(CONTEXT86*);
extern BOOL DOSVM_IsDos32(void);
extern FARPROC16 WINAPI DPMI_AllocInternalRMCB(RMCBPROC);
extern void WINAPI DPMI_FreeInternalRMCB(FARPROC16);
extern int DPMI_CallRMProc(CONTEXT86*,LPWORD,int,int);
extern BOOL DOSVM_CheckWrappers(CONTEXT86*);

/* int33.c */
extern void WINAPI DOSVM_Int33Handler(CONTEXT86*);
extern void WINAPI DOSVM_Int33Message(UINT,WPARAM,LPARAM);
extern void WINAPI DOSVM_Int33Console(MOUSE_EVENT_RECORD*);

/* int41.c */
extern void WINAPI DOSVM_Int41Handler(CONTEXT86*);

/* int4b.c */
extern void WINAPI DOSVM_Int4bHandler(CONTEXT86*);

/* int5c.c */
extern void WINAPI DOSVM_Int5cHandler(CONTEXT86*);

/* int67.c */
extern void WINAPI DOSVM_Int67Handler(CONTEXT86*);
extern void WINAPI EMS_Ioctl_Handler(CONTEXT86*);

/* interrupts.c */
extern void WINAPI DOSVM_CallBuiltinHandler( CONTEXT86 *, BYTE );
extern void WINAPI DOSVM_EmulateInterruptPM( CONTEXT86 *, BYTE );
extern BOOL WINAPI DOSVM_EmulateInterruptRM( CONTEXT86 *, BYTE );
extern FARPROC16   DOSVM_GetPMHandler16( BYTE );
extern FARPROC48   DOSVM_GetPMHandler48( BYTE );
extern FARPROC16   DOSVM_GetRMHandler( BYTE );
extern void        DOSVM_HardwareInterruptPM( CONTEXT86 *, BYTE );
extern void        DOSVM_HardwareInterruptRM( CONTEXT86 *, BYTE );
extern void        DOSVM_SetPMHandler16( BYTE, FARPROC16 );
extern void        DOSVM_SetPMHandler48( BYTE, FARPROC48 );
extern void        DOSVM_SetRMHandler( BYTE, FARPROC16 );

/* relay.c */
void DOSVM_RelayHandler( CONTEXT86 * );
void DOSVM_BuildCallFrame( CONTEXT86 *, DOSRELAY, LPVOID );

/* soundblaster.c */
extern void SB_ioport_out( WORD port, BYTE val );
extern BYTE SB_ioport_in( WORD port );

/* ppdev.c */
extern BOOL IO_pp_outp(int port, DWORD* res);
extern int IO_pp_inp(int port, DWORD* res);
extern char IO_pp_init(void);

/* timer.c */
extern void WINAPI DOSVM_Int08Handler(CONTEXT86*);

/* xms.c */
extern void WINAPI XMS_Handler(CONTEXT86*);

#endif /* __WINE_DOSEXE_H */
