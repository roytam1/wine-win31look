/*
 * DOS devices
 *
 * Copyright 1999 Ove K�ven
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
#include <string.h>
#include "wine/winbase16.h"
#include "dosexe.h"
#include "wine/debug.h"

#include "pshpack1.h"

typedef struct {
  BYTE ljmp1;
  RMCBPROC strategy;
  BYTE ljmp2;
  RMCBPROC interrupt;
} WINEDEV_THUNK;

typedef struct {
  BYTE size; /* length of header + data */
  BYTE unit; /* unit (block devices only) */
  BYTE command;
  WORD status;
  BYTE reserved[8];
} REQUEST_HEADER;

typedef struct {
  REQUEST_HEADER hdr;
  BYTE media; /* media descriptor from BPB */
  SEGPTR buffer;
  WORD count; /* byte/sector count */
  WORD sector; /* starting sector (block devices) */
  DWORD volume; /* volume ID (block devices) */
} REQ_IO;

typedef struct {
  REQUEST_HEADER hdr;
  BYTE data;
} REQ_SAFEINPUT;

typedef struct
{
    DWORD next_dev;
    WORD  attr;
    WORD  strategy;
    WORD  interrupt;
    char  name[8];
} DOS_DEVICE_HEADER;

/* Warning: need to return LOL ptr w/ offset 0 (&ptr_first_DPB) to programs ! */
typedef struct _DOS_LISTOFLISTS
{
    WORD  CX_Int21_5e01;        /* -24d contents of CX from INT 21/AX=5E01h */
    WORD  LRU_count_FCB_cache;  /* -22d */
    WORD  LRU_count_FCB_open;   /* -20d */
    DWORD OEM_func_handler;     /* -18d OEM function of INT 21/AH=F8h */
    WORD  INT21_offset;         /* -14d offset in DOS CS of code to return from INT 21 call */
    WORD  sharing_retry_count;  /* -12d */
    WORD  sharing_retry_delay;  /* -10d */
    DWORD ptr_disk_buf;         /* -8d ptr to current disk buf */
    WORD  offs_unread_CON;      /* -4d pointer in DOS data segment of unread CON input */
    WORD  seg_first_MCB;        /* -2d */
    DWORD ptr_first_DPB;        /* 00 */
    DWORD ptr_first_SysFileTable; /* 04 */
    DWORD ptr_clock_dev_hdr;    /* 08 */
    DWORD ptr_CON_dev_hdr;      /* 0C */
    WORD  max_byte_per_sec;     /* 10 maximum bytes per sector of any block device */
    DWORD ptr_disk_buf_info;    /* 12 */
    DWORD ptr_array_CDS;        /* 16 current directory structure */
    DWORD ptr_sys_FCB;          /* 1A */
    WORD  nr_protect_FCB;       /* 1E */
    BYTE  nr_block_dev;         /* 20 */
    BYTE  nr_avail_drive_letters; /* 21 */
    DOS_DEVICE_HEADER NUL_dev;  /* 22 */
    BYTE  nr_drives_JOINed;     /* 34 */
    WORD  ptr_spec_prg_names;   /* 35 */
    DWORD ptr_SETVER_prg_list;  /* 37 */
    WORD DOS_HIGH_A20_func_offs;/* 3B */
    WORD PSP_last_exec;         /* 3D if DOS in HMA: PSP of program executed last; if DOS low: 0000h */
    WORD BUFFERS_val;           /* 3F */
    WORD BUFFERS_nr_lookahead;  /* 41 */
    BYTE boot_drive;            /* 43 */
    BYTE flag_DWORD_moves;      /* 44 01h for 386+, 00h otherwise */
    WORD size_extended_mem;     /* 45 size of extended mem in KB */
    SEGPTR wine_rm_lol;         /* -- wine: Real mode pointer to LOL */
    SEGPTR wine_pm_lol;         /* -- wine: Protected mode pointer to LOL */
} DOS_LISTOFLISTS;

#include "poppack.h"

#define CON_BUFFER 128

enum strategy { SYSTEM_STRATEGY_NUL, SYSTEM_STRATEGY_CON, NB_SYSTEM_STRATEGIES };

static void *strategy_data[NB_SYSTEM_STRATEGIES];

#define NONEXT ((DWORD)-1)

#define ATTR_STDIN     0x0001
#define ATTR_STDOUT    0x0002
#define ATTR_NUL       0x0004
#define ATTR_CLOCK     0x0008
#define ATTR_FASTCON   0x0010
#define ATTR_RAW       0x0020
#define ATTR_NOTEOF    0x0040
#define ATTR_DEVICE    0x0080
#define ATTR_REMOVABLE 0x0800
#define ATTR_NONIBM    0x2000 /* block devices */
#define ATTR_UNTILBUSY 0x2000 /* char devices */
#define ATTR_IOCTL     0x4000
#define ATTR_CHAR      0x8000

#define CMD_INIT       0
#define CMD_MEDIACHECK 1 /* block devices */
#define CMD_BUILDBPB   2 /* block devices */
#define CMD_INIOCTL    3
#define CMD_INPUT      4 /* read data */
#define CMD_SAFEINPUT  5 /* "non-destructive input no wait", char devices */
#define CMD_INSTATUS   6 /* char devices */
#define CMD_INFLUSH    7 /* char devices */
#define CMD_OUTPUT     8 /* write data */
#define CMD_SAFEOUTPUT 9 /* write data with verify */
#define CMD_OUTSTATUS 10 /* char devices */
#define CMD_OUTFLUSH  11 /* char devices */
#define CMD_OUTIOCTL  12
#define CMD_DEVOPEN   13
#define CMD_DEVCLOSE  14
#define CMD_REMOVABLE 15 /* block devices */
#define CMD_UNTILBUSY 16 /* output until busy */

#define STAT_MASK  0x00FF
#define STAT_DONE  0x0100
#define STAT_BUSY  0x0200
#define STAT_ERROR 0x8000

#define LJMP 0xea


/* prototypes */
static void WINAPI nul_strategy(CONTEXT86*ctx);
static void WINAPI nul_interrupt(CONTEXT86*ctx);
static void WINAPI con_strategy(CONTEXT86*ctx);
static void WINAPI con_interrupt(CONTEXT86*ctx);

/* devices */
typedef struct
{
    char name[8];
    WORD attr;
    RMCBPROC strategy;
    RMCBPROC interrupt;
} WINEDEV;

static WINEDEV devs[] =
{
  { "NUL     ",
    ATTR_CHAR|ATTR_NUL|ATTR_DEVICE,
    nul_strategy, nul_interrupt },

  { "CON     ",
    ATTR_CHAR|ATTR_STDIN|ATTR_STDOUT|ATTR_FASTCON|ATTR_NOTEOF|ATTR_DEVICE,
    con_strategy, con_interrupt }
};

#define NR_DEVS (sizeof(devs)/sizeof(WINEDEV))

/* DOS data segment */
typedef struct
{
    DOS_LISTOFLISTS    lol;
    DOS_DEVICE_HEADER  dev[NR_DEVS-1];
    WINEDEV_THUNK      thunk[NR_DEVS];
    REQ_IO             req;
    BYTE               buffer[CON_BUFFER];

} DOS_DATASEG;

#define DOS_DATASEG_OFF(xxx) FIELD_OFFSET(DOS_DATASEG, xxx)

DWORD DOS_LOLSeg;

static struct _DOS_LISTOFLISTS * DOSMEM_LOL()
{
    return PTR_REAL_TO_LIN(HIWORD(DOS_LOLSeg),0);
}


/* the device implementations */
static void do_lret(CONTEXT86*ctx)
{
  WORD *stack = CTX_SEG_OFF_TO_LIN(ctx, ctx->SegSs, ctx->Esp);

  ctx->Eip   = *(stack++);
  ctx->SegCs = *(stack++);
  ctx->Esp  += 2*sizeof(WORD);
}

static void do_strategy(CONTEXT86*ctx, int id, int extra)
{
  REQUEST_HEADER *hdr = CTX_SEG_OFF_TO_LIN(ctx, ctx->SegEs, ctx->Ebx);
  void **hdr_ptr = strategy_data[id];

  if (!hdr_ptr) {
    hdr_ptr = calloc(1,sizeof(void *)+extra);
    strategy_data[id] = hdr_ptr;
  }
  *hdr_ptr = hdr;
  do_lret(ctx);
}

static REQUEST_HEADER * get_hdr(int id, void**extra)
{
  void **hdr_ptr = strategy_data[id];
  if (extra)
    *extra = hdr_ptr ? (void*)(hdr_ptr+1) : (void *)NULL;
  return hdr_ptr ? *hdr_ptr : (void *)NULL;
}

static void WINAPI nul_strategy(CONTEXT86*ctx)
{
  do_strategy(ctx, SYSTEM_STRATEGY_NUL, 0);
}

static void WINAPI nul_interrupt(CONTEXT86*ctx)
{
  REQUEST_HEADER *hdr = get_hdr(SYSTEM_STRATEGY_NUL, NULL);
  /* eat everything and recycle nothing */
  switch (hdr->command) {
  case CMD_INPUT:
    ((REQ_IO*)hdr)->count = 0;
    hdr->status = STAT_DONE;
    break;
  case CMD_SAFEINPUT:
    hdr->status = STAT_DONE|STAT_BUSY;
    break;
  default:
    hdr->status = STAT_DONE;
  }
  do_lret(ctx);
}

static void WINAPI con_strategy(CONTEXT86*ctx)
{
  do_strategy(ctx, SYSTEM_STRATEGY_CON, sizeof(int));
}

static void WINAPI con_interrupt(CONTEXT86*ctx)
{
  int *scan;
  REQUEST_HEADER *hdr = get_hdr(SYSTEM_STRATEGY_CON,(void **)&scan);
  BIOSDATA *bios = DOSVM_BiosData();
  WORD CurOfs = bios->NextKbdCharPtr;
  DOS_LISTOFLISTS *lol = DOSMEM_LOL();
  DOS_DATASEG *dataseg = (DOS_DATASEG *)lol;
  BYTE *linebuffer = dataseg->buffer;
  BYTE *curbuffer = (lol->offs_unread_CON) ?
    (((BYTE*)dataseg) + lol->offs_unread_CON) : (BYTE*)NULL;
  DOS_DEVICE_HEADER *con = dataseg->dev;

  switch (hdr->command) {
  case CMD_INPUT:
    {
      REQ_IO *io = (REQ_IO *)hdr;
      WORD count = io->count, len = 0;
      BYTE *buffer = CTX_SEG_OFF_TO_LIN(ctx,
					SELECTOROF(io->buffer),
					(DWORD)OFFSETOF(io->buffer));

      hdr->status = STAT_BUSY;
      /* first, check whether we already have data in line buffer */
      if (curbuffer) {
	/* yep, copy as much as we can */
	BYTE data = 0;
	while ((len<count) && (data != '\r')) {
	  data = *curbuffer++;
	  buffer[len++] = data;
	}
	if (data == '\r') {
	  /* line buffer emptied */
	  lol->offs_unread_CON = 0;
	  curbuffer = NULL;
	  /* if we're not in raw mode, call it a day */
	  if (!(con->attr & ATTR_RAW)) {
	    hdr->status = STAT_DONE;
	    io->count = len;
	    break;
	  }
	} else {
	  /* still some data left */
	  lol->offs_unread_CON = curbuffer - (BYTE*)lol;
	  /* but buffer was filled, we're done */
	  hdr->status = STAT_DONE;
	  io->count = len;
	  break;
	}
      }

      /* if we're in raw mode, we just need to fill the buffer */
      if (con->attr & ATTR_RAW) {
	while (len<count) {
	  WORD data;

	  /* do we have a waiting scancode? */
	  if (*scan) {
	    /* yes, store scancode in buffer */
	    buffer[len++] = *scan;
	    *scan = 0;
	    if (len==count) break;
	  }

	  /* check for new keyboard input */
	  while (CurOfs == bios->FirstKbdCharPtr) {
	    /* no input available yet, so wait... */
	    DOSVM_Wait( ctx );
	  }
	  /* read from keyboard queue (call int16?) */
	  data = ((WORD*)bios)[CurOfs];
	  CurOfs += 2;
	  if (CurOfs >= bios->KbdBufferEnd) CurOfs = bios->KbdBufferStart;
	  bios->NextKbdCharPtr = CurOfs;
	  /* if it's an extended key, save scancode */
	  if (LOBYTE(data) == 0) *scan = HIBYTE(data);
	  /* store ASCII char in buffer */
	  buffer[len++] = LOBYTE(data);
	}
      } else {
	/* we're not in raw mode, so we need to do line input... */
	while (TRUE) {
	  WORD data;
	  /* check for new keyboard input */
	  while (CurOfs == bios->FirstKbdCharPtr) {
	    /* no input available yet, so wait... */
	    DOSVM_Wait( ctx );
	  }
	  /* read from keyboard queue (call int16?) */
	  data = ((WORD*)bios)[CurOfs];
	  CurOfs += 2;
	  if (CurOfs >= bios->KbdBufferEnd) CurOfs = bios->KbdBufferStart;
	  bios->NextKbdCharPtr = CurOfs;

	  if (LOBYTE(data) == '\r') {
	    /* it's the return key, we're done */
	    linebuffer[len++] = LOBYTE(data);
	    break;
	  }
	  else if (LOBYTE(data) >= ' ') {
	    /* a character */
	    if ((len+1)<CON_BUFFER) {
	      linebuffer[len] = LOBYTE(data);
	      WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), &linebuffer[len++], 1, NULL, NULL);
	    }
	    /* else beep, but I don't like noise */
	  }
	  else switch (LOBYTE(data)) {
	  case '\b':
	    if (len>0) {
	      len--;
	      WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), "\b \b", 3, NULL, NULL);
	    }
	    break;
	  }
	}
	if (len > count) {
	  /* save rest of line for later */
	  lol->offs_unread_CON = linebuffer - (BYTE*)lol + count;
	  len = count;
	}
	memcpy(buffer, linebuffer, len);
      }
      hdr->status = STAT_DONE;
      io->count = len;
    }
    break;
  case CMD_SAFEINPUT:
    if (curbuffer) {
      /* some line input waiting */
      hdr->status = STAT_DONE;
      ((REQ_SAFEINPUT*)hdr)->data = *curbuffer;
    }
    else if (con->attr & ATTR_RAW) {
      if (CurOfs == bios->FirstKbdCharPtr) {
	/* no input */
	hdr->status = STAT_DONE|STAT_BUSY;
      } else {
	/* some keyboard input waiting */
	hdr->status = STAT_DONE;
	((REQ_SAFEINPUT*)hdr)->data = ((BYTE*)bios)[CurOfs];
      }
    } else {
      /* no line input */
      hdr->status = STAT_DONE|STAT_BUSY;
    }
    break;
  case CMD_INSTATUS:
    if (curbuffer) {
      /* we have data */
      hdr->status = STAT_DONE;
    }
    else if (con->attr & ATTR_RAW) {
      if (CurOfs == bios->FirstKbdCharPtr) {
	/* no input */
	hdr->status = STAT_DONE|STAT_BUSY;
      } else {
	/* some keyboard input waiting */
	hdr->status = STAT_DONE;
      }
    } else {
      /* no line input */
      hdr->status = STAT_DONE|STAT_BUSY;
    }

    break;
  case CMD_INFLUSH:
    /* flush line and keyboard queue */
    lol->offs_unread_CON = 0;
    bios->NextKbdCharPtr = bios->FirstKbdCharPtr;
    break;
  case CMD_OUTPUT:
  case CMD_SAFEOUTPUT:
    {
      REQ_IO *io = (REQ_IO *)hdr;
      BYTE *buffer = CTX_SEG_OFF_TO_LIN(ctx,
					SELECTOROF(io->buffer),
					(DWORD)OFFSETOF(io->buffer));
      DWORD result = 0;
      WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buffer, io->count, &result, NULL);
      io->count = result;
      hdr->status = STAT_DONE;
    }
    break;
  default:
    hdr->status = STAT_DONE;
  }
  do_lret(ctx);
}

static void InitListOfLists(DOS_LISTOFLISTS *DOS_LOL)
{
/*
Output of DOS 6.22:

0133:0020                    6A 13-33 01 CC 00 33 01 59 00         j.3...3.Y.
0133:0030  70 00 00 00 72 02 00 02-6D 00 33 01 00 00 2E 05   p...r...m.3.....
0133:0040  00 00 FC 04 00 00 03 08-92 21 11 E0 04 80 C6 0D   .........!......
0133:0050  CC 0D 4E 55 4C 20 20 20-20 20 00 00 00 00 00 00   ..NUL     ......
0133:0060  00 4B BA C1 06 14 00 00-00 03 01 00 04 70 CE FF   .K...........p..
0133:0070  FF 00 00 00 00 00 00 00-00 01 00 00 0D 05 00 00   ................
0133:0080  00 FF FF 00 00 00 00 FE-00 00 F8 03 FF 9F 70 02   ..............p.
0133:0090  D0 44 C8 FD D4 44 C8 FD-D4 44 C8 FD D0 44 C8 FD   .D...D...D...D..
0133:00A0  D0 44 C8 FD D0 44                                 .D...D
*/
  DOS_LOL->CX_Int21_5e01		= 0x0;
  DOS_LOL->LRU_count_FCB_cache	= 0x0;
  DOS_LOL->LRU_count_FCB_open		= 0x0;
  DOS_LOL->OEM_func_handler		= -1; /* not available */
  DOS_LOL->INT21_offset		= 0x0;
  DOS_LOL->sharing_retry_count	= 3;
  DOS_LOL->sharing_retry_delay	= 1;
  DOS_LOL->ptr_disk_buf		= 0x0;
  DOS_LOL->offs_unread_CON		= 0x0;
  DOS_LOL->seg_first_MCB		= 0x0;
  DOS_LOL->ptr_first_DPB		= 0x0;
  DOS_LOL->ptr_first_SysFileTable	= 0x0;
  DOS_LOL->ptr_clock_dev_hdr		= 0x0;
  DOS_LOL->ptr_CON_dev_hdr		= 0x0;
  DOS_LOL->max_byte_per_sec		= 512;
  DOS_LOL->ptr_disk_buf_info		= 0x0;
  DOS_LOL->ptr_array_CDS		= 0x0;
  DOS_LOL->ptr_sys_FCB		= 0x0;
  DOS_LOL->nr_protect_FCB		= 0x0;
  DOS_LOL->nr_block_dev		= 0x0;
  DOS_LOL->nr_avail_drive_letters	= 26; /* A - Z */
  DOS_LOL->nr_drives_JOINed		= 0x0;
  DOS_LOL->ptr_spec_prg_names		= 0x0;
  DOS_LOL->ptr_SETVER_prg_list	= 0x0; /* no SETVER list */
  DOS_LOL->DOS_HIGH_A20_func_offs	= 0x0;
  DOS_LOL->PSP_last_exec		= 0x0;
  DOS_LOL->BUFFERS_val		= 99; /* maximum: 99 */
  DOS_LOL->BUFFERS_nr_lookahead	= 8; /* maximum: 8 */
  DOS_LOL->boot_drive			= 3; /* C: */
  DOS_LOL->flag_DWORD_moves		= 0x01; /* i386+ */
  DOS_LOL->size_extended_mem		= 0xf000; /* very high value */
}

void DOSDEV_InstallDOSDevices(void)
{
  DOS_DATASEG *dataseg;
  WORD seg;
  WORD selector;
  unsigned int n;

  /* allocate DOS data segment or something */
  dataseg = DOSVM_AllocDataUMB( sizeof(DOS_DATASEG), &seg, &selector );

  DOS_LOLSeg = MAKESEGPTR( seg, 0 );
  DOSMEM_LOL()->wine_rm_lol = 
      MAKESEGPTR( seg, FIELD_OFFSET(DOS_LISTOFLISTS, ptr_first_DPB) );
  DOSMEM_LOL()->wine_pm_lol = 
      MAKESEGPTR( selector, FIELD_OFFSET(DOS_LISTOFLISTS, ptr_first_DPB) );

  /* initialize the magnificent List Of Lists */
  InitListOfLists(&dataseg->lol);

  /* Set up first device (NUL) */
  dataseg->lol.NUL_dev.next_dev  = MAKESEGPTR(seg, DOS_DATASEG_OFF(dev[0]));
  dataseg->lol.NUL_dev.attr      = devs[0].attr;
  dataseg->lol.NUL_dev.strategy  = DOS_DATASEG_OFF(thunk[0].ljmp1);
  dataseg->lol.NUL_dev.interrupt = DOS_DATASEG_OFF(thunk[0].ljmp2);
  memcpy(dataseg->lol.NUL_dev.name, devs[0].name, 8);

  /* Set up the remaining devices */
  for (n = 1; n < NR_DEVS; n++)
  {
    dataseg->dev[n-1].next_dev  = (n+1) == NR_DEVS ? NONEXT :
                                  MAKESEGPTR(seg, DOS_DATASEG_OFF(dev[n]));
    dataseg->dev[n-1].attr      = devs[n].attr;
    dataseg->dev[n-1].strategy  = DOS_DATASEG_OFF(thunk[n].ljmp1);
    dataseg->dev[n-1].interrupt = DOS_DATASEG_OFF(thunk[n].ljmp2);
    memcpy(dataseg->dev[n-1].name, devs[n].name, 8);
  }

  /* Set up thunks */
  for (n = 0; n < NR_DEVS; n++)
  {
    dataseg->thunk[n].ljmp1     = LJMP;
    dataseg->thunk[n].strategy  = (RMCBPROC)DPMI_AllocInternalRMCB(devs[n].strategy);
    dataseg->thunk[n].ljmp2     = LJMP;
    dataseg->thunk[n].interrupt = (RMCBPROC)DPMI_AllocInternalRMCB(devs[n].interrupt);
  }

  /* CON is device 1 */
  dataseg->lol.ptr_CON_dev_hdr = MAKESEGPTR(seg, DOS_DATASEG_OFF(dev[0]));
}

DWORD DOSDEV_Console(void)
{
  return DOSMEM_LOL()->ptr_CON_dev_hdr;
}

DWORD DOSDEV_FindCharDevice(char*name)
{
  SEGPTR cur_ptr = MAKESEGPTR(HIWORD(DOS_LOLSeg), FIELD_OFFSET(DOS_LISTOFLISTS,NUL_dev));
  DOS_DEVICE_HEADER *cur = PTR_REAL_TO_LIN(SELECTOROF(cur_ptr),OFFSETOF(cur_ptr));
  char dname[8];
  int cnt;

  /* get first 8 characters */
  strncpy(dname,name,8);
  /* if less than 8 characters, pad with spaces */
  for (cnt=0; cnt<8; cnt++)
    if (!dname[cnt]) dname[cnt]=' ';

  /* search for char devices with the right name */
  while (cur &&
	 ((!(cur->attr & ATTR_CHAR)) ||
	  memcmp(cur->name,dname,8))) {
    cur_ptr = cur->next_dev;
    if (cur_ptr == NONEXT) cur=NULL;
    else cur = PTR_REAL_TO_LIN(SELECTOROF(cur_ptr),OFFSETOF(cur_ptr));
  }
  return cur_ptr;
}

static void DOSDEV_DoReq(void*req, DWORD dev)
{
  REQUEST_HEADER *hdr = (REQUEST_HEADER *)req;
  DOS_DEVICE_HEADER *dhdr;
  CONTEXT86 ctx;
  char *phdr;

  dhdr = PTR_REAL_TO_LIN(SELECTOROF(dev),OFFSETOF(dev));
  phdr = ((char*)DOSMEM_LOL()) + DOS_DATASEG_OFF(req);

  /* copy request to request scratch area */
  memcpy(phdr, req, hdr->size);

  /* prepare to call device driver */
  memset(&ctx, 0, sizeof(ctx));
  ctx.EFlags |= V86_FLAG;

  /* ES:BX points to request for strategy routine */
  ctx.SegEs = HIWORD(DOS_LOLSeg);
  ctx.Ebx   = DOS_DATASEG_OFF(req);

  /* call strategy routine */
  ctx.SegCs = SELECTOROF(dev);
  ctx.Eip   = dhdr->strategy;
  DPMI_CallRMProc(&ctx, 0, 0, 0);

  /* call interrupt routine */
  ctx.SegCs = SELECTOROF(dev);
  ctx.Eip   = dhdr->interrupt;
  DPMI_CallRMProc(&ctx, 0, 0, 0);

  /* completed, copy request back */
  memcpy(req, phdr, hdr->size);

  if (hdr->status & STAT_ERROR) {
    switch (hdr->status & STAT_MASK) {
    case 0x0F: /* invalid disk change */
      /* this error seems to fit the bill */
      SetLastError(ERROR_NOT_SAME_DEVICE);
      break;
    default:
      SetLastError((hdr->status & STAT_MASK) + 0x13);
      break;
    }
  }
}

static int DOSDEV_IO(unsigned cmd, DWORD dev, DWORD buf, int buflen)
{
  REQ_IO req;

  req.hdr.size=sizeof(req);
  req.hdr.unit=0; /* not dealing with block devices yet */
  req.hdr.command=cmd;
  req.hdr.status=STAT_BUSY;
  req.media=0; /* not dealing with block devices yet */
  req.buffer=buf;
  req.count=buflen;
  req.sector=0; /* block devices */
  req.volume=0; /* block devices */

  DOSDEV_DoReq(&req, dev);

  return req.count;
}

int DOSDEV_Peek(DWORD dev, BYTE*data)
{
  REQ_SAFEINPUT req;

  req.hdr.size=sizeof(req);
  req.hdr.unit=0; /* not dealing with block devices yet */
  req.hdr.command=CMD_SAFEINPUT;
  req.hdr.status=STAT_BUSY;
  req.data=0;

  DOSDEV_DoReq(&req, dev);

  if (req.hdr.status & STAT_BUSY) return 0;

  *data = req.data;
  return 1;
}

int DOSDEV_Read(DWORD dev, DWORD buf, int buflen)
{
  return DOSDEV_IO(CMD_INPUT, dev, buf, buflen);
}

int DOSDEV_Write(DWORD dev, DWORD buf, int buflen, int verify)
{
  return DOSDEV_IO(verify?CMD_SAFEOUTPUT:CMD_OUTPUT, dev, buf, buflen);
}

int DOSDEV_IoctlRead(DWORD dev, DWORD buf, int buflen)
{
  return DOSDEV_IO(CMD_INIOCTL, dev, buf, buflen);
}

int DOSDEV_IoctlWrite(DWORD dev, DWORD buf, int buflen)
{
  return DOSDEV_IO(CMD_OUTIOCTL, dev, buf, buflen);
}

void DOSDEV_SetSharingRetry(WORD delay, WORD count)
{
    DOSMEM_LOL()->sharing_retry_delay = delay;
    if (count) DOSMEM_LOL()->sharing_retry_count = count;
}

SEGPTR DOSDEV_GetLOL(BOOL v86)
{
    if (v86) return DOSMEM_LOL()->wine_rm_lol;
    else return DOSMEM_LOL()->wine_pm_lol;
}
