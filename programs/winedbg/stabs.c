/* -*- tab-width: 8; c-basic-offset: 4 -*- */

/*
 * File stabs.c - read stabs information from the wine executable itself.
 *
 * Copyright (C) 1996, Eric Youngdale.
 *		 1999-2003 Eric Pouech
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
 *
 *
 * Maintenance Information
 * -----------------------
 *
 * For documentation on the stabs format see for example
 *   The "stabs" debug format
 *     by Julia Menapace, Jim Kingdon, David Mackenzie
 *     of Cygnus Support
 *     available (hopefully) from http:\\sources.redhat.com\gdb\onlinedocs
 */

#include "config.h"

#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdio.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

#include "debugger.h"

#if defined(__svr4__) || defined(__sun)
#define __ELF__
#endif

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winedbg_stabs);

#ifndef N_UNDF
#define N_UNDF		0x00
#endif

#ifndef STN_UNDEF
# define STN_UNDEF	0
#endif

#define N_GSYM		0x20
#define N_FUN		0x24
#define N_STSYM		0x26
#define N_LCSYM		0x28
#define N_MAIN		0x2a
#define N_ROSYM		0x2c
#define N_OPT		0x3c
#define N_RSYM		0x40
#define N_SLINE		0x44
#define N_SO		0x64
#define N_LSYM		0x80
#define N_BINCL		0x82
#define N_SOL		0x84
#define N_PSYM		0xa0
#define N_EINCL		0xa2
#define N_LBRAC		0xc0
#define N_EXCL		0xc2
#define N_RBRAC		0xe0

struct stab_nlist {
  union {
    char *n_name;
    struct stab_nlist *n_next;
    long n_strx;
  } n_un;
  unsigned char n_type;
  char n_other;
  short n_desc;
  unsigned long n_value;
};

static void stab_strcpy(char * dest, int sz, const char * source)
{
  /*
   * A strcpy routine that stops when we hit the ':' character.
   * Faster than copying the whole thing, and then nuking the
   * ':'.
   */
  while(*source != '\0' && *source != ':' && sz-- > 0)
      *dest++ = *source++;
  *dest = '\0';
  assert(sz > 0);
}

typedef struct {
   char*		name;
   unsigned long	value;
   int			idx;
   struct datatype**	vector;
   int			nrofentries;
} include_def;

#define MAX_INCLUDES	5120

static	include_def* 	include_defs = NULL;
static	int	     	num_include_def = 0;
static  int		num_alloc_include_def = 0;
static	int		cu_include_stack[MAX_INCLUDES];
static	int		cu_include_stk_idx = 0;
static  struct datatype**	cu_vector = NULL;
static  int 		cu_nrofentries = 0;

static
int
DEBUG_CreateInclude(const char* file, unsigned long val)
{
  if (num_include_def == num_alloc_include_def)
    {
      num_alloc_include_def += 256;
      include_defs = DBG_realloc(include_defs, sizeof(include_defs[0])*num_alloc_include_def);
      memset(include_defs+num_include_def, 0, sizeof(include_defs[0])*256);
    }
  include_defs[num_include_def].name = DBG_strdup(file);
  include_defs[num_include_def].value = val;
  include_defs[num_include_def].vector = NULL;
  include_defs[num_include_def].nrofentries = 0;

  return num_include_def++;
}

static
int
DEBUG_FindInclude(const char* file, unsigned long val)
{
  int		i;

  for (i = 0; i < num_include_def; i++)
    {
      if (val == include_defs[i].value &&
	  strcmp(file, include_defs[i].name) == 0)
	return i;
    }
  return -1;
}

static
int
DEBUG_AddInclude(int idx)
{
  ++cu_include_stk_idx;

  /* is this happen, just bump MAX_INCLUDES */
  /* we could also handle this as another dynarray */
  assert(cu_include_stk_idx < MAX_INCLUDES);

  cu_include_stack[cu_include_stk_idx] = idx;
  return cu_include_stk_idx;
}

static
void
DEBUG_ResetIncludes(void)
{
  /*
   * The datatypes that we would need to use are reset when
   * we start a new file. (at least the ones in filenr == 0
   */
  cu_include_stk_idx = 0;/* keep 0 as index for the .c file itself */
  memset(cu_vector, 0, sizeof(cu_vector[0]) * cu_nrofentries);
}

static
void
DEBUG_FreeIncludes(void)
{
  int	i;

  DEBUG_ResetIncludes();

  for (i = 0; i < num_include_def; i++)
    {
      DBG_free(include_defs[i].name);
      DBG_free(include_defs[i].vector);
    }
  DBG_free(include_defs);
  include_defs = NULL;
  num_include_def = 0;
  num_alloc_include_def = 0;
  DBG_free(cu_vector);
  cu_vector = NULL;
  cu_nrofentries = 0;
}

static
struct datatype**
DEBUG_FileSubNr2StabEnum(int filenr, int subnr)
{
  struct datatype** ret;

  WINE_TRACE("creating type id for (%d,%d)\n", filenr, subnr);

  /* FIXME: I could perhaps create a dummy include_def for each compilation
   * unit which would allow not to handle those two cases separately
   */
  if (filenr == 0)
    {
      if (cu_nrofentries <= subnr)
	{
	  cu_vector = DBG_realloc(cu_vector, sizeof(cu_vector[0])*(subnr+1));
	  memset(cu_vector+cu_nrofentries, 0, sizeof(cu_vector[0])*(subnr+1-cu_nrofentries));
	  cu_nrofentries = subnr + 1;
	}
      ret = &cu_vector[subnr];
    }
  else
    {
      include_def*	idef;

      assert(filenr <= cu_include_stk_idx);

      idef = &include_defs[cu_include_stack[filenr]];

      if (idef->nrofentries <= subnr)
	{
	  idef->vector = DBG_realloc(idef->vector, sizeof(idef->vector[0])*(subnr+1));
	  memset(idef->vector + idef->nrofentries, 0, sizeof(idef->vector[0])*(subnr+1-idef->nrofentries));
	  idef->nrofentries = subnr + 1;
	}
      ret = &idef->vector[subnr];
    }
  WINE_TRACE("(%d,%d) is %p\n",filenr,subnr,ret);
  return ret;
}

static
struct datatype**
DEBUG_ReadTypeEnum(LPCSTR *x)
{
    int filenr,subnr;

    if (**x=='(') {
	(*x)++;					/* '(' */
	filenr=strtol(*x,(char**)x,10);	        /* <int> */
	(*x)++;					/* ',' */
	subnr=strtol(*x,(char**)x,10);		/* <int> */
	(*x)++;					/* ')' */
    } else {
    	filenr = 0;
	subnr = strtol(*x,(char**)x,10);        /* <int> */
    }
    return DEBUG_FileSubNr2StabEnum(filenr,subnr);
}

/*#define PTS_DEBUG*/
struct ParseTypedefData
{
    const char*		ptr;
    char		buf[1024];
    int			idx;
#ifdef PTS_DEBUG
    struct PTS_Error 
    {
        char*               ptr;
        unsigned            line;
    } errors[16];
    int                 err_idx;
#endif
};

#ifdef PTS_DEBUG
static void PTS_Push(struct ParseTypedefData* ptd, unsigned line)
{
    assert(ptd->err_idx < sizeof(ptd->errors) / sizeof(ptd->errors[0]));
    ptd->errors[ptd->err_idx].line = line;
    ptd->errors[ptd->err_idx].ptr = ptd->ptr;
    ptd->err_idx++;
}
#define PTS_ABORTIF(ptd, t) do { if (t) { PTS_Push((ptd), __LINE__); return -1;} } while (0)
#else
#define PTS_ABORTIF(ptd, t) do { if (t) return -1; } while (0)
#endif

static int DEBUG_PTS_ReadTypedef(struct ParseTypedefData* ptd, const char* typename,
				 struct datatype** dt);

static int DEBUG_PTS_ReadID(struct ParseTypedefData* ptd)
{
    const char*	        first = ptd->ptr;
    unsigned int	len;

    PTS_ABORTIF(ptd, (ptd->ptr = strchr(ptd->ptr, ':')) == NULL);
    len = ptd->ptr - first;
    PTS_ABORTIF(ptd, len >= sizeof(ptd->buf) - ptd->idx);
    memcpy(ptd->buf + ptd->idx, first, len);
    ptd->buf[ptd->idx + len] = '\0';
    ptd->idx += len + 1;
    ptd->ptr++; /* ':' */
    return 0;
}

static int DEBUG_PTS_ReadNum(struct ParseTypedefData* ptd, int* v)
{
    char*	last;

    *v = strtol(ptd->ptr, &last, 10);
    PTS_ABORTIF(ptd, last == ptd->ptr);
    ptd->ptr = last;
    return 0;
}

static int DEBUG_PTS_ReadTypeReference(struct ParseTypedefData* ptd,
				       int* filenr, int* subnr)
{
    if (*ptd->ptr == '(') {
	/* '(' <int> ',' <int> ')' */
	ptd->ptr++;
	PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, filenr) == -1);
	PTS_ABORTIF(ptd, *ptd->ptr++ != ',');
	PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, subnr) == -1);
	PTS_ABORTIF(ptd, *ptd->ptr++ != ')');
    } else {
    	*filenr = 0;
	PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, subnr) == -1);
    }
    return 0;
}

static int DEBUG_PTS_ReadRange(struct ParseTypedefData* ptd, struct datatype** dt,
			       int* lo, int* hi)
{
    /* type ';' <int> ';' <int> ';' */
    PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, dt) == -1);
    PTS_ABORTIF(ptd, *ptd->ptr++ != ';');	/* ';' */
    PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, lo) == -1);
    PTS_ABORTIF(ptd, *ptd->ptr++ != ';');	/* ';' */
    PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, hi) == -1);
    PTS_ABORTIF(ptd, *ptd->ptr++ != ';');	/* ';' */
    return 0;
}

static inline int DEBUG_PTS_ReadMethodInfo(struct ParseTypedefData* ptd)
{
    struct datatype* dt;
    char*   tmp;
    char    mthd;

    do
    {
        /* get type of return value */
        PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, &dt) == -1);
        if (*ptd->ptr == ';') ptd->ptr++;

        /* get types of parameters */
        if (*ptd->ptr == ':')
        {
            PTS_ABORTIF(ptd, !(tmp = strchr(ptd->ptr + 1, ';')));
            ptd->ptr = tmp + 1;
        }
        PTS_ABORTIF(ptd, !(*ptd->ptr >= '0' && *ptd->ptr <= '9'));
        ptd->ptr++;
        PTS_ABORTIF(ptd, !(ptd->ptr[0] >= 'A' && *ptd->ptr <= 'D'));
        mthd = *++ptd->ptr;
        PTS_ABORTIF(ptd, mthd != '.' && mthd != '?' && mthd != '*');
        ptd->ptr++;
        if (mthd == '*')
        {
            int              ofs;
            struct datatype* dt;

            PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, &ofs) == -1);
            PTS_ABORTIF(ptd, *ptd->ptr++ != ';');
            PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, &dt) == -1);
            PTS_ABORTIF(ptd, *ptd->ptr++ != ';');
        }
    } while (*ptd->ptr != ';');
    ptd->ptr++;

    return 0;
}

static inline int DEBUG_PTS_ReadAggregate(struct ParseTypedefData* ptd,
                                          struct datatype* sdt)
{
    int			sz, ofs;
    struct datatype*	adt;
    struct datatype*	dt = NULL;
    int			idx;
    int			doadd;

    PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, &sz) == -1);

    doadd = DEBUG_SetStructSize(sdt, sz);
    if (*ptd->ptr == '!') /* C++ inheritence */
    {
        int     num_classes;
        char    tmp[256];

        ptd->ptr++;
        PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, &num_classes) == -1);
        PTS_ABORTIF(ptd, *ptd->ptr++ != ',');
        while (--num_classes >= 0)
        {
            ptd->ptr += 2; /* skip visibility and inheritence */
            PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, &ofs) == -1);
            PTS_ABORTIF(ptd, *ptd->ptr++ != ',');

            PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, &adt) == -1);

            snprintf(tmp, sizeof(tmp), "__inherited_class_%s", DEBUG_GetName(adt));
            /* FIXME: DEBUG_GetObjectSize will not always work, especially when adt
             * has just been seen as a forward definition and not the real stuff yet.
             * As we don't use much the size of members in structs, this may not
             * be much of a problem
             */
            if (doadd) DEBUG_AddStructElement(sdt, tmp, adt, ofs, DEBUG_GetObjectSize(adt) * 8); 
            PTS_ABORTIF(ptd, *ptd->ptr++ != ';');
        }
        
    }
    /* if the structure has already been filled, just redo the parsing
     * but don't store results into the struct
     * FIXME: there's a quite ugly memory leak in there...
     */

    /* Now parse the individual elements of the structure/union. */
    while (*ptd->ptr != ';') 
    {
	/* agg_name : type ',' <int:offset> ',' <int:size> */
	idx = ptd->idx;

        if (ptd->ptr[0] == '$' && ptd->ptr[1] == 'v')
        {
            int                 x;

            if (ptd->ptr[2] == 'f')
            {
                /* C++ virtual method table */
                ptd->ptr += 3;
                DEBUG_ReadTypeEnum(&ptd->ptr);
                PTS_ABORTIF(ptd, *ptd->ptr++ != ':');
                PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, &dt) == -1);
                PTS_ABORTIF(ptd, *ptd->ptr++ != ',');
                PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, &x) == -1);
                PTS_ABORTIF(ptd, *ptd->ptr++ != ';');
                ptd->idx = idx;
                continue;
            }
            else if (ptd->ptr[2] == 'b')
            {
                ptd->ptr += 3;
                PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, &dt) == -1);
                PTS_ABORTIF(ptd, *ptd->ptr++ != ':');
                PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, &dt) == -1);
                PTS_ABORTIF(ptd, *ptd->ptr++ != ',');
                PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, &x) == -1);
                PTS_ABORTIF(ptd, *ptd->ptr++ != ';');
                ptd->idx = idx;
                continue;
            }
        }

	PTS_ABORTIF(ptd, DEBUG_PTS_ReadID(ptd) == -1);
        /* Ref. TSDF R2.130 Section 7.4.  When the field name is a method name
         * it is followed by two colons rather than one.
         */
        if (*ptd->ptr == ':')
        {
            ptd->ptr++; 
            DEBUG_PTS_ReadMethodInfo(ptd);
            ptd->idx = idx;
            continue;
        }
        else
        {
            /* skip C++ member protection /0 /1 or /2 */
            if (*ptd->ptr == '/') ptd->ptr += 2;
        }
	PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, &adt) == -1);

        switch (*ptd->ptr++)
        {
        case ',':
            PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, &ofs) == -1);
            PTS_ABORTIF(ptd, *ptd->ptr++ != ',');
            PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, &sz) == -1);
            PTS_ABORTIF(ptd, *ptd->ptr++ != ';');

            if (doadd) DEBUG_AddStructElement(sdt, ptd->buf + idx, adt, ofs, sz);
            break;
        case ':':
            {
                char* tmp;
                /* method parameters... terminated by ';' */
                PTS_ABORTIF(ptd, !(tmp = strchr(ptd->ptr, ';')));
                ptd->ptr = tmp + 1;
            }
            break;
        default:
            PTS_ABORTIF(ptd, TRUE);
        }
	ptd->idx = idx;
    }
    PTS_ABORTIF(ptd, *ptd->ptr++ != ';');
    if (*ptd->ptr == '~')
    {
        ptd->ptr++;
        PTS_ABORTIF(ptd, *ptd->ptr++ != '%');
        PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, &dt) == -1);
        PTS_ABORTIF(ptd, *ptd->ptr++ != ';');
    }
    return 0;
}

static inline int DEBUG_PTS_ReadEnum(struct ParseTypedefData* ptd, 
                                     struct datatype* edt)
{
    int			ofs;
    int			idx;

    while (*ptd->ptr != ';') {
	idx = ptd->idx;
	PTS_ABORTIF(ptd, DEBUG_PTS_ReadID(ptd) == -1);
	PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, &ofs) == -1);
	PTS_ABORTIF(ptd, *ptd->ptr++ != ',');
	DEBUG_AddStructElement(edt, ptd->buf + idx, NULL, ofs, 0);
	ptd->idx = idx;
    }
    ptd->ptr++;
    return 0;
}

static inline int DEBUG_PTS_ReadArray(struct ParseTypedefData* ptd, 
                                      struct datatype* adt)
{
    int			lo, hi;
    struct datatype*	rdt;

    /* ar<typeinfo_nodef>;<int>;<int>;<typeinfo> */

    PTS_ABORTIF(ptd, *ptd->ptr++ != 'r');
    /* FIXME: range type is lost, always assume int */
    PTS_ABORTIF(ptd, DEBUG_PTS_ReadRange(ptd, &rdt, &lo, &hi) == -1);
    PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, &rdt) == -1);

    DEBUG_SetArrayParams(adt, lo, hi, rdt);
    return 0;
}

static int DEBUG_PTS_ReadTypedef(struct ParseTypedefData* ptd, const char* typename,
				 struct datatype** ret_dt)
{
    int			idx, lo, hi, sz = -1;
    struct datatype*	new_dt = NULL;	/* newly created data type */
    struct datatype*	ref_dt;		/* referenced data type (pointer...) */
    struct datatype* 	dt1;		/* intermediate data type (scope is limited) */
    struct datatype* 	dt2;		/* intermediate data type: t1=t2=new_dt */
    int			filenr1, subnr1;

    /* things are a bit complicated because of the way the typedefs are stored inside
     * the file (we cannot keep the struct datatype** around, because address can
     * change when realloc is done, so we must call over and over
     * DEBUG_FileSubNr2StabEnum to keep the correct values around
     * (however, keeping struct datatype* is valid))
     */
    PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypeReference(ptd, &filenr1, &subnr1) == -1);

    while (*ptd->ptr == '=') {
	ptd->ptr++;
	PTS_ABORTIF(ptd, new_dt != NULL);

	/* first handle attribute if any */
	switch (*ptd->ptr) {
	case '@':
	    if (*++ptd->ptr == 's') {
		ptd->ptr++;
		if (DEBUG_PTS_ReadNum(ptd, &sz) == -1) {
		    WINE_ERR("Not an attribute... NIY\n");
		    ptd->ptr -= 2;
		    return -1;
		}
		PTS_ABORTIF(ptd, *ptd->ptr++ != ';');
	    }
	    break;
	}
	/* then the real definitions */
	switch (*ptd->ptr++) {
	case '*':
        case '&':
	    new_dt = DEBUG_NewDataType(DT_POINTER, NULL);
	    PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, &ref_dt) == -1);
	    DEBUG_SetPointerType(new_dt, ref_dt);
           break;
        case 'k': /* 'const' modifier */
        case 'B': /* 'volatile' modifier */
            /* just kinda ignore the modifier, I guess -gmt */
            PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, typename, &new_dt) == -1);
	    break;
	case '(':
	    ptd->ptr--;
            PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, typename, &new_dt) == -1);
	    break;
	case 'a':
	    new_dt = DEBUG_NewDataType(DT_ARRAY, NULL);
	    PTS_ABORTIF(ptd, DEBUG_PTS_ReadArray(ptd, new_dt) == -1);
	    break;
	case 'r':
	    new_dt = DEBUG_NewDataType(DT_BASIC, typename);
	    assert(!*DEBUG_FileSubNr2StabEnum(filenr1, subnr1));
	    *DEBUG_FileSubNr2StabEnum(filenr1, subnr1) = new_dt;
	    PTS_ABORTIF(ptd, DEBUG_PTS_ReadRange(ptd, &ref_dt, &lo, &hi) == -1);
	    /* should perhaps do more here... */
	    break;
	case 'f':
	    new_dt = DEBUG_NewDataType(DT_FUNC, NULL);
	    PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, &ref_dt) == -1);
	    DEBUG_SetPointerType(new_dt, ref_dt);
	    break;
	case 'e':
	    new_dt = DEBUG_NewDataType(DT_ENUM, NULL);
	    PTS_ABORTIF(ptd, DEBUG_PTS_ReadEnum(ptd, new_dt) == -1);
	    break;
	case 's':
	case 'u':
	    /* dt1 can have been already defined in a forward definition */
	    dt1 = *DEBUG_FileSubNr2StabEnum(filenr1, subnr1);
	    dt2 = DEBUG_TypeCast(DT_STRUCT, typename);
	    if (!dt1) {
		new_dt = DEBUG_NewDataType(DT_STRUCT, typename);
		/* we need to set it here, because a struct can hold a pointer
		 * to itself
		 */
		*DEBUG_FileSubNr2StabEnum(filenr1, subnr1) = new_dt;
	    } else {
		if (DEBUG_GetType(dt1) != DT_STRUCT) {
		    WINE_ERR("Forward declaration is not an aggregate\n");
		    return -1;
		}

		/* should check typename is the same too */
		new_dt = dt1;
	    }
	    PTS_ABORTIF(ptd, DEBUG_PTS_ReadAggregate(ptd, new_dt) == -1);
	    break;
	case 'x':
	    switch (*ptd->ptr++) {
	    case 'e':			lo = DT_ENUM;	break;
	    case 's':	case 'u':	lo = DT_STRUCT;	break;
	    default: return -1;
	    }

	    idx = ptd->idx;
	    PTS_ABORTIF(ptd, DEBUG_PTS_ReadID(ptd) == -1);
	    new_dt = DEBUG_NewDataType(lo, ptd->buf + idx);
	    ptd->idx = idx;
	    break;
	case '-':
            {
                enum debug_type_basic basic = DT_BASIC_LAST;

                PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, &lo) == -1);

                switch (lo)
                {
                case  1: basic = DT_BASIC_INT; break;
                case  2: basic = DT_BASIC_CHAR; break;
                case  3: basic = DT_BASIC_SHORTINT; break;
                case  4: basic = DT_BASIC_LONGINT; break;
                case  5: basic = DT_BASIC_UCHAR; break;
                case  6: basic = DT_BASIC_SCHAR; break;
                case  7: basic = DT_BASIC_USHORTINT; break;
                case  8: basic = DT_BASIC_UINT; break;
/*              case  9: basic = DT_BASIC_UINT";  */
                case 10: basic = DT_BASIC_ULONGINT; break;
                case 11: basic = DT_BASIC_VOID; break;
                case 12: basic = DT_BASIC_FLOAT; break;
                case 13: basic = DT_BASIC_DOUBLE; break;
                case 14: basic = DT_BASIC_LONGDOUBLE; break;
/*              case 15: basic = DT_BASIC_INT; break; */
                case 16:
                    switch (sz) {
                    case 32: basic = DT_BASIC_BOOL1; break;
                    case 16: basic = DT_BASIC_BOOL2; break;
                    case  8: basic = DT_BASIC_BOOL4; break;
                    }
                    break;
/*              case 17: basic = DT_BASIC_SHORT real; break; */
/*              case 18: basic = DT_BASIC_REAL; break; */
                case 25: basic = DT_BASIC_CMPLX_FLOAT; break;
                case 26: basic = DT_BASIC_CMPLX_DOUBLE; break;
/*              case 30: basic = DT_BASIC_wchar"; break; */
                case 31: basic = DT_BASIC_LONGLONGINT; break;
                case 32: basic = DT_BASIC_ULONGLONGINT; break;
                default:
                    PTS_ABORTIF(ptd, 1);
                }
                PTS_ABORTIF(ptd, !(new_dt = DEBUG_GetBasicType(basic)));
                PTS_ABORTIF(ptd, *ptd->ptr++ != ';');
            }
	    break;
        case '#':
	    new_dt = DEBUG_NewDataType(DT_FUNC, NULL);
            if (*ptd->ptr == '#')
            {
                ptd->ptr++;
                PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, &ref_dt) == -1);
                DEBUG_SetPointerType(new_dt, ref_dt);
            }
            else
            {
                struct datatype* cls_dt;
                struct datatype* pmt_dt;

                PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, &cls_dt) == -1);
                PTS_ABORTIF(ptd, *ptd->ptr++ != ',');
                PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, &ref_dt) == -1);
                DEBUG_SetPointerType(new_dt, ref_dt);
                while (*ptd->ptr == ',')
                {
                    ptd->ptr++;
                    PTS_ABORTIF(ptd, DEBUG_PTS_ReadTypedef(ptd, NULL, &pmt_dt) == -1);
                }
            }
            break;
        case 'R':
            {
                enum debug_type_basic basic = DT_BASIC_LAST;
                int type, len, unk;
                
                PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, &type) == -1);
                PTS_ABORTIF(ptd, *ptd->ptr++ != ';');	/* ';' */
                PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, &len) == -1);
                PTS_ABORTIF(ptd, *ptd->ptr++ != ';');	/* ';' */
                PTS_ABORTIF(ptd, DEBUG_PTS_ReadNum(ptd, &unk) == -1);
                PTS_ABORTIF(ptd, *ptd->ptr++ != ';');	/* ';' */

                switch (type)
                {
                case 1: basic = DT_BASIC_FLOAT; break;
                case 2: basic = DT_BASIC_DOUBLE; break;
                case 3: basic = DT_BASIC_CMPLX_FLOAT; break;
                case 4: basic = DT_BASIC_CMPLX_DOUBLE; break;
                case 5: basic = DT_BASIC_CMPLX_LONGDOUBLE; break;
                case 6: basic = DT_BASIC_LONGDOUBLE; break;
                default: PTS_ABORTIF(ptd, 1);
                }
                PTS_ABORTIF(ptd, !(new_dt = DEBUG_GetBasicType(basic)));
            }
            break;
	default:
	    WINE_ERR("Unknown type '%c'\n", ptd->ptr[-1]);
	    return -1;
	}
    }

    if (!new_dt)
    {
        /* is it a forward declaration that has been filled ? */
	new_dt = *DEBUG_FileSubNr2StabEnum(filenr1, subnr1);
        /* if not, this should then be a basic type: define it, or even void */
        if (!new_dt)
	    new_dt = DEBUG_NewDataType(DT_BASIC, typename);
    }            

    *DEBUG_FileSubNr2StabEnum(filenr1, subnr1) = *ret_dt = new_dt;

    if (typename && WINE_TRACE_ON(winedbg_stabs)) {
	DEBUG_Printf("Adding (%d,%d) %s => ", filenr1, subnr1, typename);
	DEBUG_PrintTypeCast(new_dt);
	DEBUG_Printf("\n");
    }

    return 0;
}

static int DEBUG_ParseTypedefStab(const char* ptr, const char* typename)
{
    struct ParseTypedefData	ptd;
    struct datatype*		dt;
    int				ret = -1;

    /* check for already existing definition */

    ptd.idx = 0;
#ifdef PTS_DEBUG
    ptd.err_idx = 0;
#endif
    for (ptd.ptr = ptr - 1; ;)
    {
        ptd.ptr = strchr(ptd.ptr + 1, ':');
        if (ptd.ptr == NULL || *++ptd.ptr != ':') break;
    }

    if (ptd.ptr)
    {
	if (*ptd.ptr != '(') ptd.ptr++;
        /* most of type definitions take one char, except Tt */
	if (*ptd.ptr != '(') ptd.ptr++;
	ret = DEBUG_PTS_ReadTypedef(&ptd, typename, &dt);
    }

    if (ret == -1 || *ptd.ptr) {
#ifdef PTS_DEBUG
        int     i;
	WINE_TRACE("Failure on %s\n", ptr);
        if (ret == -1)
        {
            for (i = 0; i < ptd.err_idx; i++)
            {
                WINE_TRACE("[%d]: line %d => %s\n", 
                           i, ptd.errors[i].line, ptd.errors[i].ptr);
            }
        }
        else
            WINE_TRACE("[0]: => %s\n", ptd.ptr);
            
#else
	WINE_ERR("Failure on %s at %s\n", ptr, ptd.ptr);
#endif
	return FALSE;
    }

    return TRUE;
}

static struct datatype*
DEBUG_ParseStabType(const char* stab)
{
    const char* c = stab - 1;

    /*
     * Look through the stab definition, and figure out what datatype
     * this represents.  If we have something we know about, assign the
     * type.
     * According to "The \"stabs\" debug format" (Rev 2.130) the name may be
     * a C++ name and contain double colons e.g. foo::bar::baz:t5=*6.
     */
    do
    {
        if ((c = strchr(c + 1, ':')) == NULL) return NULL;
    } while (*++c == ':');

    /*
     * The next characters say more about the type (i.e. data, function, etc)
     * of symbol.  Skip them.  (C++ for example may have Tt).
     * Actually this is a very weak description; I think Tt is the only
     * multiple combination we should see.
     */
    while (*c && *c != '(' && !isdigit(*c))
        c++;
    /*
     * The next is either an integer or a (integer,integer).
     * The DEBUG_ReadTypeEnum takes care that stab_types is large enough.
     */
    return *DEBUG_ReadTypeEnum(&c);
}

enum DbgInfoLoad DEBUG_ParseStabs(const char* addr, void* load_offset,
				  unsigned int staboff, int stablen,
				  unsigned int strtaboff, int strtablen)
{
  struct name_hash*             curr_func = NULL;
  struct wine_locals*           curr_loc = NULL;
  struct name_hash*             curr_sym = NULL;
  char                          currpath[PATH_MAX];
  int                           i;
  int                           in_external_file = FALSE;
  int                           last_nso = -1;
  unsigned int                  len;
  DBG_VALUE	                new_value;
  int                           nstab;
  const char*                   ptr;
  char*                         stabbuff;
  unsigned int                  stabbufflen;
  const struct stab_nlist*      stab_ptr;
  const char*                   strs;
  int                           strtabinc;
  const char*                   subpath = NULL;
  char                          symname[4096];

  nstab = stablen / sizeof(struct stab_nlist);
  stab_ptr = (struct stab_nlist *) (addr + staboff);
  strs  = (char *) (addr + strtaboff);

  memset(currpath, 0, sizeof(currpath));

  /*
   * Allocate a buffer into which we can build stab strings for cases
   * where the stab is continued over multiple lines.
   */
  stabbufflen = 65536;
  stabbuff = (char *) DBG_alloc(stabbufflen);

  strtabinc = 0;
  stabbuff[0] = '\0';
  for (i = 0; i < nstab; i++, stab_ptr++)
    {
      ptr = strs + (unsigned int) stab_ptr->n_un.n_name;
      if( ptr[strlen(ptr) - 1] == '\\' )
        {
          /*
           * Indicates continuation.  Append this to the buffer, and go onto the
           * next record.  Repeat the process until we find a stab without the
           * '/' character, as this indicates we have the whole thing.
           */
          len = strlen(ptr);
          if( strlen(stabbuff) + len > stabbufflen )
            {
              stabbufflen += 65536;
              stabbuff = (char *) DBG_realloc(stabbuff, stabbufflen);
            }
          strncat(stabbuff, ptr, len - 1);
          continue;
        }
      else if( stabbuff[0] != '\0' )
        {
          strcat( stabbuff, ptr);
          ptr = stabbuff;
        }

      if( strchr(ptr, '=') != NULL )
        {
          /*
           * The stabs aren't in writable memory, so copy it over so we are
           * sure we can scribble on it.
           */
          if( ptr != stabbuff )
            {
              strcpy(stabbuff, ptr);
              ptr = stabbuff;
            }
          stab_strcpy(symname, sizeof(symname), ptr);
          if (!DEBUG_ParseTypedefStab(ptr, symname)) {
	    /* skip this definition */
	    stabbuff[0] = '\0';
	    continue;
	  }
        }

      switch(stab_ptr->n_type)
        {
        case N_GSYM:
          /*
           * These are useless with ELF.  They have no value, and you have to
           * read the normal symbol table to get the address.  Thus we
           * ignore them, and when we process the normal symbol table
           * we should do the right thing.
           *
           * With a.out or mingw, they actually do make some amount of sense.
           */
          new_value.type = DEBUG_ParseStabType(ptr);
          new_value.addr.seg = 0;
	  new_value.cookie = DV_TARGET;

          stab_strcpy(symname, sizeof(symname), ptr);
#ifdef __ELF__
          /* EPP used to be: new_value.addr.off = 0; */
          new_value.addr.off = (unsigned long)load_offset + stab_ptr->n_value;
          curr_sym = DEBUG_AddSymbol( symname, &new_value, currpath,
                                      SYM_WINE | SYM_DATA | SYM_INVALID );
#else
          new_value.addr.off = (unsigned long)load_offset + stab_ptr->n_value;
          curr_sym = DEBUG_AddSymbol( symname, &new_value, currpath,
                                      SYM_WINE | SYM_DATA );
#endif
          break;
        case N_RBRAC:
        case N_LBRAC:
          /*
           * We need to keep track of these so we get symbol scoping
           * right for local variables.  For now, we just ignore them.
           * The hooks are already there for dealing with this however,
           * so all we need to do is to keep count of the nesting level,
           * and find the RBRAC for each matching LBRAC.
           */
          break;
        case N_LCSYM:
        case N_STSYM:
          /*
           * These are static symbols and BSS symbols.
           */
          new_value.addr.seg = 0;
          new_value.type = DEBUG_ParseStabType(ptr);
          new_value.addr.off = (unsigned long)load_offset + stab_ptr->n_value;
	  new_value.cookie = DV_TARGET;

          stab_strcpy(symname, sizeof(symname), ptr);
          curr_sym = DEBUG_AddSymbol( symname, &new_value, currpath,
                                      SYM_WINE | SYM_DATA );
          break;
        case N_PSYM:
          /*
           * These are function parameters.
           */
          if( curr_func != NULL && !in_external_file )
            {
              stab_strcpy(symname, sizeof(symname), ptr);
              curr_loc = DEBUG_AddLocal( curr_func, 0,
                                         stab_ptr->n_value, 0, 0, symname );
              DEBUG_SetLocalSymbolType( curr_loc, DEBUG_ParseStabType(ptr) );
            }
          break;
        case N_RSYM:
          if( curr_func != NULL && !in_external_file )
            {
              stab_strcpy(symname, sizeof(symname), ptr);
              curr_loc = DEBUG_AddLocal( curr_func, stab_ptr->n_value + 1,
					 0, 0, 0, symname );
              DEBUG_SetLocalSymbolType( curr_loc, DEBUG_ParseStabType(ptr) );
            }
          break;
        case N_LSYM:
          if( curr_func != NULL && !in_external_file )
            {
              stab_strcpy(symname, sizeof(symname), ptr);
              curr_loc = DEBUG_AddLocal( curr_func, 0,
					 stab_ptr->n_value, 0, 0, symname );
	      DEBUG_SetLocalSymbolType( curr_loc, DEBUG_ParseStabType(ptr) );
            }
          break;
        case N_SLINE:
          /*
           * This is a line number.  These are always relative to the start
           * of the function (N_FUN), and this makes the lookup easier.
           */
          if( curr_func != NULL && !in_external_file )
            {
#ifdef __ELF__
              DEBUG_AddLineNumber(curr_func, stab_ptr->n_desc,
                                  stab_ptr->n_value);
#else
#if 0
              /*
               * This isn't right.  The order of the stabs is different under
               * a.out, and as a result we would end up attaching the line
               * number to the wrong function.
               */
              DEBUG_AddLineNumber(curr_func, stab_ptr->n_desc,
                                  stab_ptr->n_value - curr_func->addr.off);
#endif
#endif
            }
          break;
        case N_FUN:
          /*
           * First, clean up the previous function we were working on.
           */
          DEBUG_Normalize(curr_func);

          /*
           * For now, just declare the various functions.  Later
           * on, we will add the line number information and the
           * local symbols.
           */
          if( !in_external_file)
            {
              stab_strcpy(symname, sizeof(symname), ptr);
	      if (*symname)
		{
		  new_value.addr.seg = 0;
		  new_value.type = DEBUG_ParseStabType(ptr);
		  new_value.addr.off = (unsigned long)load_offset + stab_ptr->n_value;
		  new_value.cookie = DV_TARGET;
		  /*
		   * Copy the string to a temp buffer so we
		   * can kill everything after the ':'.  We do
		   * it this way because otherwise we end up dirtying
		   * all of the pages related to the stabs, and that
		   * sucks up swap space like crazy.
		   */
#ifdef __ELF__
		  curr_func = DEBUG_AddSymbol( symname, &new_value, currpath,
					       SYM_WINE | SYM_FUNC | SYM_INVALID );
#else
		  curr_func = DEBUG_AddSymbol( symname, &new_value, currpath,
					       SYM_WINE | SYM_FUNC );
#endif
		}
	      else
		{
		  /* some GCC seem to use a N_FUN "" to mark the end of a function */
		  curr_func = NULL;
		}
            }
          else
            {
              /*
               * Don't add line number information for this function
               * any more.
               */
              curr_func = NULL;
            }
          break;
        case N_SO:
          /*
           * This indicates a new source file.  Append the records
           * together, to build the correct path name.
           */
#ifndef __ELF__
          /*
           * With a.out, there is no NULL string N_SO entry at the end of
           * the file.  Thus when we find non-consecutive entries,
           * we consider that a new file is started.
           */
          if( last_nso < i-1 )
            {
              currpath[0] = '\0';
              DEBUG_Normalize(curr_func);
              curr_func = NULL;
            }
#endif

          if( *ptr == '\0' ) /* end of N_SO file */
            {
              /*
               * Nuke old path.
               */
              currpath[0] = '\0';
              DEBUG_Normalize(curr_func);
              curr_func = NULL;
            }
          else
            {
              if (*ptr != '/')
                strcat(currpath, ptr);
              else
                strcpy(currpath, ptr);
              subpath = ptr;
	      DEBUG_ResetIncludes();
            }
          last_nso = i;
          break;
        case N_SOL:
          /*
           * This indicates we are including stuff from an include file.
           * If this is the main source, enable the debug stuff, otherwise
           * ignore it.
           */
            in_external_file = !(subpath == NULL || strcmp(ptr, subpath) == 0);
            in_external_file = FALSE; /* FIXME EPP hack FIXME */;
          break;
        case N_UNDF:
          strs += strtabinc;
          strtabinc = stab_ptr->n_value;
          DEBUG_Normalize(curr_func);
          curr_func = NULL;
          break;
        case N_OPT:
          /*
           * Ignore this.  We don't care what it points to.
           */
          break;
        case N_BINCL:
	   DEBUG_AddInclude(DEBUG_CreateInclude(ptr, stab_ptr->n_value));
	   break;
        case N_EINCL:
	   break;
	case N_EXCL:
	   DEBUG_AddInclude(DEBUG_FindInclude(ptr, stab_ptr->n_value));
	   break;
        case N_MAIN:
          /*
           * Always ignore these.  GCC doesn't even generate them.
           */
          break;
        default:
          WINE_ERR("Unknown stab type 0x%02x\n", stab_ptr->n_type);
          break;
        }

      stabbuff[0] = '\0';

      WINE_TRACE("0x%02x %x %s\n", stab_ptr->n_type,
                 (unsigned int) stab_ptr->n_value,
                 strs + (unsigned int) stab_ptr->n_un.n_name);
    }

  DEBUG_FreeIncludes();

  return DIL_LOADED;
}

