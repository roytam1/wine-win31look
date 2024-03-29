/*
 * Utility functions.
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
#include <windows.h>
#include <unistd.h>
#include <errno.h>

#include "winetest.h"

void *xmalloc (size_t len)
{
    void *p = malloc (len);

    if (!p) report (R_FATAL, "Out of memory.");
    return p;
}

void *xrealloc (void *op, size_t len)
{
    void *p = realloc (op, len);

    if (!p) report (R_FATAL, "Out of memory.");
    return p;
}

char *vstrfmtmake (size_t *lenp, const char *fmt, va_list ap)
{
    size_t size = 1000;
    char *p, *q;
    int n;

    p = malloc (size);
    if (!p) return NULL;
    while (1) {
        n = vsnprintf (p, size, fmt, ap);
        if (n < 0) size *= 2;   /* Windows */
        else if ((unsigned)n >= size) size = n+1; /* glibc */
        else break;
        q = realloc (p, size);
        if (!q) {
          free (p);
          return NULL;
       }
       p = q;
    }
    if (lenp) *lenp = n;
    return p;
}

char *vstrmake (size_t *lenp, va_list ap)
{
    const char *fmt;

    fmt = va_arg (ap, const char*);
    return vstrfmtmake (lenp, fmt, ap);
}

char *strmake (size_t *lenp, ...)
{
    va_list ap;
    char *p;

    va_start (ap, lenp);
    p = vstrmake (lenp, ap);
    if (!p) report (R_FATAL, "Out of memory.");
    va_end (ap);
    return p;
}

void xprintf (const char *fmt, ...)
{
    va_list ap;
    size_t size;
    ssize_t written;
    char *buffer, *head;

    va_start (ap, fmt);
    buffer = vstrfmtmake (&size, fmt, ap);
    head = buffer;
    va_end (ap);
    while ((written = write (1, head, size)) != size) {
        if (written == -1)
            report (R_FATAL, "Can't write logs: %d", errno);
        head += written;
        size -= written;
    }
    free (buffer);
}

char *
badtagchar (char *tag)
{
    while (*tag)
        if (('a'<=*tag && *tag<='z') ||
            ('A'<=*tag && *tag<='Z') ||
            ('0'<=*tag && *tag<='9') ||
            *tag=='-' || *tag=='.')
            tag++;
        else return tag;
    return NULL;
}
