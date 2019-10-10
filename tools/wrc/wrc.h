/*
 * Main definitions and externals
 *
 * Copyright 1998 Bertho A. Stultiens (BS)
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

#ifndef __WRC_WRC_H
#define __WRC_WRC_H

#include <time.h>	/* For time_t */

#include "wrctypes.h"

#define WRC_MAJOR_VERSION	1
#define WRC_MINOR_VERSION	1
#define WRC_MICRO_VERSION	9
#define WRC_RELEASEDATE		"(31-Dec-2000)"

#define WRC_STRINGIZE(a)	#a
#define WRC_EXP_STRINGIZE(a)	WRC_STRINGIZE(a)
#define WRC_VERSIONIZE(a,b,c)	WRC_STRINGIZE(a) "." WRC_STRINGIZE(b) "." WRC_STRINGIZE(c)
#define WRC_VERSION		WRC_VERSIONIZE(WRC_MAJOR_VERSION, WRC_MINOR_VERSION, WRC_MICRO_VERSION)
#define WRC_FULLVERSION 	WRC_VERSION " " WRC_RELEASEDATE

/* From wrc.c */
extern int debuglevel;
#define DEBUGLEVEL_NONE		0x0000
#define DEBUGLEVEL_CHAT		0x0001
#define DEBUGLEVEL_DUMP		0x0002
#define DEBUGLEVEL_TRACE	0x0004
#define DEBUGLEVEL_PPMSG	0x0008
#define DEBUGLEVEL_PPLEX	0x0010
#define DEBUGLEVEL_PPTRACE	0x0020

extern int win32;
extern int create_res;
extern int extensions;
extern int create_s;
extern int pedantic;
extern int byteorder;
extern int preprocess_only;
extern int no_preprocess;

extern char *output_name;
extern char *input_name;
extern char *cmdline;
extern time_t now;

extern int line_number;
extern int char_number;

extern resource_t *resource_top;
extern language_t *currentlanguage;

#endif
