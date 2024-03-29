/*
 * Debugger memory handling
 *
 * Copyright 1993 Eric Youngdale
 * Copyright 1995 Alexandre Julliard
 * Copyright 2000 Eric Pouech
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
#include <string.h>

#include "debugger.h"
#include "winbase.h"

#ifdef __i386__
#define IS_VM86_MODE() (DEBUG_context.EFlags & V86_FLAG)
#endif

static	void	DEBUG_Die(const char* msg)
{
   DEBUG_Printf(msg);
   exit(1);
}

void* DBG_alloc(size_t size)
{
   void *res = malloc(size ? size : 1);
   if (res == NULL)
      DEBUG_Die("Memory exhausted.\n");
   memset(res, 0, size);
   return res;
}

void* DBG_realloc(void *ptr, size_t size)
{
   void* res = realloc(ptr, size);
   if ((res == NULL) && size)
      DEBUG_Die("Memory exhausted.\n");
   return res;
}

char* DBG_strdup(const char *str)
{
   char *res = strdup(str);
   if (!res)
      DEBUG_Die("Memory exhausted.\n");
   return res;
}

void DBG_free(void *ptr)
{
    free(ptr);
}

enum dbg_mode DEBUG_GetSelectorType( WORD sel )
{
#ifdef __i386__
    LDT_ENTRY	le;

    if (IS_VM86_MODE()) return MODE_VM86;
    if (sel == 0) return MODE_32;
    if (GetThreadSelectorEntry( DEBUG_CurrThread->handle, sel, &le))
        return le.HighWord.Bits.Default_Big ? MODE_32 : MODE_16;
    /* selector doesn't exist */
    return MODE_INVALID;
#else
    return MODE_32;
#endif
}
#ifdef __i386__
void DEBUG_FixAddress( DBG_ADDR *addr, DWORD def)
{
   if (addr->seg == 0xffffffff) addr->seg = def;
   if (DEBUG_IsSelectorSystem(addr->seg)) addr->seg = 0;
}

/* Determine if sel is a system selector (i.e. not managed by Wine) */
BOOL	DEBUG_IsSelectorSystem(WORD sel)
{
    if (IS_VM86_MODE()) return FALSE;  /* no system selectors in vm86 mode */
    return !(sel & 4) || ((sel >> 3) < 17);
}
#endif /* __i386__ */

DWORD DEBUG_ToLinear( const DBG_ADDR *addr )
{
#ifdef __i386__
   LDT_ENTRY	le;

   if (IS_VM86_MODE()) return (DWORD)(LOWORD(addr->seg) << 4) + addr->off;

   if (DEBUG_IsSelectorSystem(addr->seg))
      return addr->off;

   if (GetThreadSelectorEntry( DEBUG_CurrThread->handle, addr->seg, &le)) {
      return (le.HighWord.Bits.BaseHi << 24) + (le.HighWord.Bits.BaseMid << 16) + le.BaseLow + addr->off;
   }
   return 0;
#else
   return addr->off;
#endif
}

void DEBUG_GetCurrentAddress( DBG_ADDR *addr )
{
#ifdef __i386__
    addr->seg  = DEBUG_context.SegCs;

    if (DEBUG_IsSelectorSystem(addr->seg))
       addr->seg = 0;
    addr->off  = DEBUG_context.Eip;
#elif defined(__sparc__)
    addr->seg = 0;
    addr->off = DEBUG_context.pc;
#elif defined(__powerpc__)
    addr->seg = 0;
    addr->off = DEBUG_context.Iar;
#else
#	error You must define GET_IP for this CPU
#endif
}

void	DEBUG_InvalAddr( const DBG_ADDR* addr )
{
   DEBUG_Printf("*** Invalid address ");
   DEBUG_PrintAddress(addr, DEBUG_CurrThread ? DEBUG_CurrThread->dbg_mode : MODE_32, FALSE);
   DEBUG_Printf("\n");
   if (DBG_IVAR(ExtDbgOnInvalidAddress)) DEBUG_ExternalDebugger();
}

void	DEBUG_InvalLinAddr( void* addr )
{
   DBG_ADDR address;

   address.seg = 0;
   address.off = (unsigned long)addr;
   DEBUG_InvalAddr( &address );
}

/***********************************************************************
 *           DEBUG_ReadMemory
 *
 * Read a memory value.
 */
/* FIXME: this function is now getting closer and closer to
 * DEBUG_ExprGetValue. They should be merged...
 */
int DEBUG_ReadMemory( const DBG_VALUE* val )
{
    int		value = 0; /* to clear any unused byte */
    int		os = DEBUG_GetObjectSize(val->type);

    assert(sizeof(value) >= os);

    /* FIXME: only works on little endian systems */

    if (val->cookie == DV_TARGET) {
       DBG_ADDR	addr = val->addr;
       void*	lin;

#ifdef __i386__
       DEBUG_FixAddress( &addr, DEBUG_context.SegDs );
#endif
       lin = (void*)DEBUG_ToLinear( &addr );

       DEBUG_READ_MEM_VERBOSE(lin, &value, os);
    } else {
       if (val->addr.off)
	  memcpy(&value, (void*)val->addr.off, os);
    }
    return value;
}


/***********************************************************************
 *           DEBUG_WriteMemory
 *
 * Store a value in memory.
 */
void DEBUG_WriteMemory( const DBG_VALUE* val, int value )
{
    int		os = DEBUG_GetObjectSize(val->type);

    assert(sizeof(value) >= os);

    /* FIXME: only works on little endian systems */

    if (val->cookie == DV_TARGET) {
       DBG_ADDR addr = val->addr;
       void*	lin;

#ifdef __i386__
       DEBUG_FixAddress( &addr, DEBUG_context.SegDs );
#endif
       lin = (void*)DEBUG_ToLinear( &addr );
       DEBUG_WRITE_MEM_VERBOSE(lin, &value, os);
    } else {
       memcpy((void*)val->addr.off, &value, os);
    }
}

/***********************************************************************
 *           DEBUG_GrabAddress
 *
 * Get the address from a value
 */
BOOL DEBUG_GrabAddress( DBG_VALUE* value, BOOL fromCode )
{
    assert(value->cookie == DV_TARGET || value->cookie == DV_HOST);

#ifdef __i386__
    DEBUG_FixAddress( &value->addr,
		      (fromCode) ? DEBUG_context.SegCs : DEBUG_context.SegDs);
#endif

    /*
     * Dereference pointer to get actual memory address we need to be
     * reading.  We will use the same segment as what we have already,
     * and hope that this is a sensible thing to do.
     */
    if (value->type != NULL) {
        if (value->type == DEBUG_GetBasicType(DT_BASIC_CONST_INT)) {
	    /*
	     * We know that we have the actual offset stored somewhere
	     * else in 32-bit space.  Grab it, and we
	     * should be all set.
	     */
	    unsigned int  seg2 = value->addr.seg;
	    value->addr.seg = 0;
	    value->addr.off = DEBUG_GetExprValue(value, NULL);
	    value->addr.seg = seg2;
	} else {
	    struct datatype	* testtype;

	    if (DEBUG_TypeDerefPointer(value, &testtype) == 0)
	        return FALSE;
	    if (testtype != NULL || value->type == DEBUG_GetBasicType(DT_BASIC_CONST_INT))
	        value->addr.off = DEBUG_GetExprValue(value, NULL);
	}
    } else if (!value->addr.seg && !value->addr.off) {
        DEBUG_Printf("Invalid expression\n");
	return FALSE;
    }
    return TRUE;
}

/***********************************************************************
 *           DEBUG_ExamineMemory
 *
 * Implementation of the 'x' command.
 */
void DEBUG_ExamineMemory( const DBG_VALUE *_value, int count, char format )
{
    DBG_VALUE		  value = *_value;
    int			  i;
    unsigned char	* pnt;

    if (!DEBUG_GrabAddress(&value, (format == 'i'))) return;

    if (format != 'i' && count > 1)
    {
        DEBUG_PrintAddress( &value.addr, DEBUG_CurrThread->dbg_mode, FALSE );
        DEBUG_Printf(": ");
    }

    pnt = (void*)DEBUG_ToLinear( &value.addr );

    switch(format)
    {
         case 'u':
                if (count == 1) count = 256;
                DEBUG_nchar += DEBUG_PrintStringW(&value.addr, count);
		DEBUG_Printf("\n");
		return;
        case 's':
		if (count == 1) count = 256;
                DEBUG_nchar += DEBUG_PrintStringA(&value.addr, count);
		DEBUG_Printf("\n");
		return;
	case 'i':
		while (count-- && DEBUG_DisassembleInstruction( &value.addr ));
		return;
        case 'g':
                while (count--)
                {
                    GUID guid;
                    if (!DEBUG_READ_MEM_VERBOSE(pnt, &guid, sizeof(guid))) break;
                    DEBUG_Printf("{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\n",
                                 guid.Data1, guid.Data2, guid.Data3,
                                 guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                                 guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7] );
                    pnt += sizeof(guid);
                    value.addr.off += sizeof(guid);
                    if (count)
                    {
                        DEBUG_PrintAddress( &value.addr, DEBUG_CurrThread->dbg_mode, FALSE );
                        DEBUG_Printf(": ");
                    }
                }
                return;

#define DO_DUMP2(_t,_l,_f,_vv) { \
	        _t _v; \
		for(i=0; i<count; i++) { \
                    if (!DEBUG_READ_MEM_VERBOSE(pnt, &_v, sizeof(_t))) break; \
                    DEBUG_Printf(_f,(_vv)); \
                    pnt += sizeof(_t); value.addr.off += sizeof(_t); \
                    if ((i % (_l)) == (_l)-1) { \
                        DEBUG_Printf("\n"); \
                        DEBUG_PrintAddress( &value.addr, DEBUG_CurrThread->dbg_mode, FALSE );\
                        DEBUG_Printf(": ");\
                    } \
		} \
		DEBUG_Printf("\n"); \
        } \
	return
#define DO_DUMP(_t,_l,_f) DO_DUMP2(_t,_l,_f,_v)

        case 'x': DO_DUMP(int, 4, " %8.8x");
	case 'd': DO_DUMP(unsigned int, 4, " %10d");
	case 'w': DO_DUMP(unsigned short, 8, " %04x");
        case 'c': DO_DUMP2(char, 32, " %c", (_v < 0x20) ? ' ' : _v);
	case 'b': DO_DUMP2(char, 16, " %02x", (_v) & 0xff);
	}
}

#define CHARBUFSIZE 16

/******************************************************************
 *		DEBUG_PrintStringA
 *
 * Prints on channel chnl, the string starting at address in target
 * address space. The string stops when either len chars (if <> -1)
 * have been printed, or the '\0' char is printed
 */
int  DEBUG_PrintStringA(const DBG_ADDR* address, int len)
{
    char*       lin = (void*)DEBUG_ToLinear(address);
    char        ch[CHARBUFSIZE+1];
    int         written = 0;

    if (len == -1) len = 32767; /* should be big enough */

    while (written < len)
    {
        int to_write = min(CHARBUFSIZE, len - written );
        if (!DEBUG_READ_MEM_VERBOSE(lin, ch, to_write)) break;
        ch[to_write] = '\0';  /* protect from displaying junk */
        to_write = lstrlenA(ch);
        DEBUG_OutputA(ch, to_write);
        lin += to_write;
        written += to_write;
        if (to_write < CHARBUFSIZE) break;
    }
    return written; /* number of actually written chars */
}

int  DEBUG_PrintStringW(const DBG_ADDR* address, int len)
{
    WCHAR*      lin = (void*)DEBUG_ToLinear(address);
    WCHAR       ch[CHARBUFSIZE+1];
    int         written = 0;

    if (len == -1) len = 32767; /* should be big enough */

    while  (written < len)
    {
        int to_write = min(CHARBUFSIZE, len - written );
        if (!DEBUG_READ_MEM_VERBOSE(lin, ch, to_write * sizeof(WCHAR))) break;
        ch[to_write] = 0;  /* protect from displaying junk */
        to_write = lstrlenW(ch);
        DEBUG_OutputW(ch, to_write);
        lin += to_write;
        written += to_write;
        if (to_write < CHARBUFSIZE) break;
    }
    return written; /* number of actually written chars */
}
