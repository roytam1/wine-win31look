/*
 * winetest definitions
 *
 * Copyright 2003 Dimitrie O. Paun
 * Copyright 2003 Ferenc Wagner
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
 *
 */

#ifndef __WINETESTS_H
#define __WINETESTS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void fatal (const char* msg);
void warning (const char* msg);
void *xmalloc (size_t len);
void *xrealloc (void *op, size_t len);
void xprintf (const char *fmt, ...);
char *vstrmake (size_t *lenp, va_list ap);
char *strmake (size_t *lenp, ...);
char *badtagchar (char *tag);

int send_file (const char *name);

/* GUI definitions */

#include <windows.h>

enum report_type {
    R_STATUS = 0,
    R_PROGRESS,
    R_STEP,
    R_DELTA,
    R_DIR,
    R_OUT,
    R_WARNING,
    R_ERROR,
    R_FATAL,
    R_ASK,
    R_TEXTMODE,
    R_QUIET
};

int report (enum report_type t, ...);

#endif /* __WINETESTS_H */
