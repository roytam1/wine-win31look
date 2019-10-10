/*
 * Copyright 2000 Marcus Meissner
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

#include <string.h>
#include "ddraw_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ddraw);

/* *************************************
      16 / 15 bpp to palettized 8 bpp
   ************************************* */
static void pixel_convert_16_to_8(void *src, void *dst, DWORD width, DWORD height, LONG pitch, IDirectDrawPaletteImpl* palette) {
    unsigned char  *c_src = (unsigned char  *) src;
    unsigned short *c_dst = (unsigned short *) dst;
    int y;

    if (palette != NULL) {
	const unsigned int * pal = (unsigned int *) palette->screen_palents;

	for (y = height; y--; ) {
#if defined(__i386__) && defined(__GNUC__)
	    /* gcc generates slightly inefficient code for the the copy/lookup,
	     * it generates one excess memory access (to pal) per pixel. Since
	     * we know that pal is not modified by the memory write we can
	     * put it into a register and reduce the number of memory accesses
	     * from 4 to 3 pp. There are two xor eax,eax to avoid pipeline
	     * stalls. (This is not guaranteed to be the fastest method.)
	     */
	    __asm__ __volatile__(
	    "xor %%eax,%%eax\n"
	    "1:\n"
	    "    lodsb\n"
	    "    movw (%%edx,%%eax,4),%%ax\n"
	    "    stosw\n"
	    "	   xor %%eax,%%eax\n"
	    "    loop 1b\n"
	    : "=S" (c_src), "=D" (c_dst)
	    : "S" (c_src), "D" (c_dst) , "c" (width), "d" (pal)
	    : "eax", "cc", "memory"
	    );
	    c_src+=(pitch-width);
#else
	    unsigned char * srclineend = c_src+width;
	    while (c_src < srclineend)
		*c_dst++ = pal[*c_src++];
	    c_src+=(pitch-width);
#endif
	}
    } else {
	FIXME("No palette set...\n");
	memset(dst, 0, width * height * 2);
    }
}
static void palette_convert_16_to_8(
	LPPALETTEENTRY palent, void *screen_palette, DWORD start, DWORD count
) {
    int i;
    unsigned int *pal = (unsigned int *) screen_palette;

    for (i = 0; i < count; i++)
	pal[start + i] = (((((unsigned short) palent[i].peRed) & 0xF8) << 8) |
			  ((((unsigned short) palent[i].peBlue) & 0xF8) >> 3) |
			  ((((unsigned short) palent[i].peGreen) & 0xFC) << 3));
}

static void palette_convert_15_to_8(
	LPPALETTEENTRY palent, void *screen_palette, DWORD start, DWORD count
) {
    int i;
    unsigned int *pal = (unsigned int *) screen_palette;

    for (i = 0; i < count; i++)
	pal[start + i] = (((((unsigned short) palent[i].peRed) & 0xF8) << 7) |
			  ((((unsigned short) palent[i].peBlue) & 0xF8) >> 3) |
			  ((((unsigned short) palent[i].peGreen) & 0xF8) << 2));
}

/* *************************************
      24 to palettized 8 bpp
   ************************************* */
static void pixel_convert_24_to_8(
	void *src, void *dst, DWORD width, DWORD height, LONG pitch,
	IDirectDrawPaletteImpl* palette
) {
    unsigned char  *c_src = (unsigned char  *) src;
    unsigned char *c_dst = (unsigned char *) dst;
    int y;

    if (palette != NULL) {
	const unsigned int *pal = (unsigned int *) palette->screen_palents;

	for (y = height; y--; ) {
	    unsigned char * srclineend = c_src+width;
	    while (c_src < srclineend ) {
		register long pixel = pal[*c_src++];
		*c_dst++ = pixel;
		*c_dst++ = pixel>>8;
		*c_dst++ = pixel>>16;
	    }
	    c_src+=(pitch-width);
	}
    } else {
	FIXME("No palette set...\n");
	memset(dst, 0, width * height * 3);
    }
}

/* *************************************
      32 bpp to palettized 8 bpp
   ************************************* */
static void pixel_convert_32_to_8(
	void *src, void *dst, DWORD width, DWORD height, LONG pitch,
	IDirectDrawPaletteImpl* palette
) {
    unsigned char  *c_src = (unsigned char  *) src;
    unsigned int *c_dst = (unsigned int *) dst;
    int y;

    if (palette != NULL) {
	const unsigned int *pal = (unsigned int *) palette->screen_palents;

	for (y = height; y--; ) {
#if defined(__i386__) && defined(__GNUC__)
	    /* See comment in pixel_convert_16_to_8 */
	    __asm__ __volatile__(
	    "xor %%eax,%%eax\n"
	    "1:\n"
	    "    lodsb\n"
	    "    movl (%%edx,%%eax,4),%%eax\n"
	    "    stosl\n"
	    "	   xor %%eax,%%eax\n"
	    "    loop 1b\n"
	    : "=S" (c_src), "=D" (c_dst)
	    : "S" (c_src), "D" (c_dst) , "c" (width), "d" (pal)
	    : "eax", "cc", "memory"
	    );
	    c_src+=(pitch-width);
#else
	    unsigned char * srclineend = c_src+width;
	    while (c_src < srclineend )
		*c_dst++ = pal[*c_src++];
	    c_src+=(pitch-width);
#endif
	}
    } else {
	FIXME("No palette set...\n");
	memset(dst, 0, width * height * 4);
    }
}

static void palette_convert_24_to_8(
	LPPALETTEENTRY palent, void *screen_palette, DWORD start, DWORD count
) {
    int i;
    unsigned int *pal = (unsigned int *) screen_palette;

    for (i = 0; i < count; i++)
	pal[start + i] = ((((unsigned int) palent[i].peRed) << 16) |
			  (((unsigned int) palent[i].peGreen) << 8) |
			   ((unsigned int) palent[i].peBlue));
}

/* *************************************
      16 bpp to 15 bpp
   ************************************* */
static void pixel_convert_15_to_16(
	void *src, void *dst, DWORD width, DWORD height, LONG pitch,
	IDirectDrawPaletteImpl* palette
) {
    unsigned short *c_src = (unsigned short *) src;
    unsigned short *c_dst = (unsigned short *) dst;
    int y;

    for (y = height; y--; ) {
	unsigned short * srclineend = c_src+width;
	while (c_src < srclineend ) {
	    unsigned short val = *c_src++;
	    *c_dst++=((val&0xFFC0)>>1)|(val&0x001f);
	}
	c_src+=((pitch/2)-width);
    }
}

/* *************************************
      32 bpp to 16 bpp
   ************************************* */
static void pixel_convert_32_to_16(
	void *src, void *dst, DWORD width, DWORD height, LONG pitch,
	IDirectDrawPaletteImpl* palette
) {
    unsigned short *c_src = (unsigned short *) src;
    unsigned int *c_dst = (unsigned int *) dst;
    int y;

    for (y = height; y--; ) {
	unsigned short * srclineend = c_src+width;
	while (c_src < srclineend ) {
	    *c_dst++ = (((*c_src & 0xF800) << 8) |
			((*c_src & 0x07E0) << 5) |
			((*c_src & 0x001F) << 3));
	    c_src++;
	}
	c_src+=((pitch/2)-width);
    }
}

/* *************************************
      32 bpp to 24 bpp
   ************************************* */
static void pixel_convert_32_to_24(
	void *src, void *dst, DWORD width, DWORD height, LONG pitch,
	IDirectDrawPaletteImpl* palette
) {
    unsigned char *c_src = (unsigned char *) src;
    unsigned int *c_dst = (unsigned int *) dst;
    int y;

    for (y = height; y--; ) {
	unsigned char * srclineend = c_src+width*3;
	while (c_src < srclineend ) {
	    /* FIXME: wrong for big endian */
	    memcpy(c_dst,c_src,3);
	    c_src+=3;
	    c_dst++;
	}
	c_src+=pitch-width*3;
    }
}

/* *************************************
      16 bpp to 32 bpp
   ************************************* */
static void pixel_convert_16_to_32(
	void *src, void *dst, DWORD width, DWORD height, LONG pitch,
	IDirectDrawPaletteImpl* palette
) {
    unsigned int *c_src = (unsigned int *) src;
    unsigned short *c_dst = (unsigned short *) dst;
    int y;

    for (y = height; y--; ) {
	unsigned int * srclineend = c_src+width;
	while (c_src < srclineend ) {
	    *c_dst++ = (((*c_src & 0xF80000) >> 8) |
			((*c_src & 0x00FC00) >> 5) |
			((*c_src & 0x0000F8) >> 3));
	    c_src++;
	}
	c_src+=((pitch/4)-width);
    }
}

Convert ModeEmulations[8] = {
  { { 32, 24, 0x00FF0000, 0x0000FF00, 0x000000FF }, {  24, 24, 0xFF0000, 0x00FF00, 0x0000FF }, { pixel_convert_32_to_24, NULL } },
  { { 32, 24, 0x00FF0000, 0x0000FF00, 0x000000FF }, {  16, 16, 0xF800, 0x07E0, 0x001F }, { pixel_convert_32_to_16, NULL } },
  { { 32, 24, 0x00FF0000, 0x0000FF00, 0x000000FF }, {  8,  8, 0x00, 0x00, 0x00 }, { pixel_convert_32_to_8,  palette_convert_24_to_8 } },
  { { 24, 24,   0xFF0000,   0x00FF00,   0x0000FF }, {  8,  8, 0x00, 0x00, 0x00 }, { pixel_convert_24_to_8,  palette_convert_24_to_8 } },
  { { 16, 15,     0x7C00,     0x03E0,     0x001F }, {  16,16, 0xf800, 0x07e0, 0x001f }, { pixel_convert_15_to_16,  NULL } },
  { { 16, 16,     0xF800,     0x07E0,     0x001F }, {  8,  8, 0x00, 0x00, 0x00 }, { pixel_convert_16_to_8,  palette_convert_16_to_8 } },
  { { 16, 15,     0x7C00,     0x03E0,     0x001F }, {  8,  8, 0x00, 0x00, 0x00 }, { pixel_convert_16_to_8,  palette_convert_15_to_8 } },
  { { 16, 16,     0xF800,     0x07E0,     0x001F }, {  32, 24, 0x00FF0000, 0x0000FF00, 0x000000FF }, { pixel_convert_16_to_32, NULL } }
};
