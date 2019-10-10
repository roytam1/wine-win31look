/*
 * Functions to use the XRender extension
 *
 * Copyright 2001, 2002 Huw D M Davies for CodeWeavers
 *
 * Some parts also:
 * Copyright 2000 Keith Packard, member of The XFree86 Project, Inc.
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

#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "wownt32.h"
#include "x11drv.h"
#include "gdi.h"
#include "wine/library.h"
#include "wine/unicode.h"
#include "wine/debug.h"

static BOOL X11DRV_XRender_Installed = FALSE;
int using_client_side_fonts = FALSE;

WINE_DEFAULT_DEBUG_CHANNEL(xrender);

#ifdef HAVE_X11_EXTENSIONS_XRENDER_H

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

static XRenderPictFormat *screen_format; /* format of screen */
static XRenderPictFormat *mono_format; /* format of mono bitmap */

typedef struct
{
    LOGFONTW lf;
    SIZE     devsize;  /* size in device coords */
    DWORD    hash;
} LFANDSIZE;

#define INITIAL_REALIZED_BUF_SIZE 128

typedef enum { AA_None, AA_Grey, AA_RGB, AA_BGR, AA_VRGB, AA_VBGR } AA_Type;

typedef struct
{
    LFANDSIZE lfsz;
    AA_Type aa;
    GlyphSet glyphset;
    XRenderPictFormat *font_format;
    int nrealized;
    BOOL *realized;
    void **bitmaps;
    XGlyphInfo *gis;
    UINT count;
    INT next;
} gsCacheEntry;

struct tagXRENDERINFO
{
    int                cache_index;
    Picture            pict;
    Picture            tile_pict;
    Pixmap             tile_xpm;
    COLORREF           lastTextColor;
};


static gsCacheEntry *glyphsetCache = NULL;
static DWORD glyphsetCacheSize = 0;
static INT lastfree = -1;
static INT mru = -1;

#define INIT_CACHE_SIZE 10

static int antialias = 1;

/* some default values just in case */
#ifndef SONAME_LIBX11
#define SONAME_LIBX11 "libX11.so"
#endif
#ifndef SONAME_LIBXEXT
#define SONAME_LIBXEXT "libXext.so"
#endif
#ifndef SONAME_LIBXRENDER
#define SONAME_LIBXRENDER "libXrender.so"
#endif

static void *xrender_handle;

#define MAKE_FUNCPTR(f) static typeof(f) * p##f;
MAKE_FUNCPTR(XRenderAddGlyphs)
MAKE_FUNCPTR(XRenderCompositeString8)
MAKE_FUNCPTR(XRenderCompositeString16)
MAKE_FUNCPTR(XRenderCompositeString32)
MAKE_FUNCPTR(XRenderCreateGlyphSet)
MAKE_FUNCPTR(XRenderCreatePicture)
MAKE_FUNCPTR(XRenderFillRectangle)
MAKE_FUNCPTR(XRenderFindFormat)
MAKE_FUNCPTR(XRenderFindVisualFormat)
MAKE_FUNCPTR(XRenderFreeGlyphSet)
MAKE_FUNCPTR(XRenderFreePicture)
MAKE_FUNCPTR(XRenderSetPictureClipRectangles)
MAKE_FUNCPTR(XRenderQueryExtension)
#undef MAKE_FUNCPTR

static CRITICAL_SECTION xrender_cs;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &xrender_cs,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { 0, (DWORD)(__FILE__ ": xrender_cs") }
};
static CRITICAL_SECTION xrender_cs = { &critsect_debug, -1, 0, 0, 0, 0 };


/***********************************************************************
 *   X11DRV_XRender_Init
 *
 * Let's see if our XServer has the extension available
 *
 */
void X11DRV_XRender_Init(void)
{
    int error_base, event_base, i;
    XRenderPictFormat pf;

    if (client_side_with_render &&
	wine_dlopen(SONAME_LIBX11, RTLD_NOW|RTLD_GLOBAL, NULL, 0) &&
	wine_dlopen(SONAME_LIBXEXT, RTLD_NOW|RTLD_GLOBAL, NULL, 0) && 
	(xrender_handle = wine_dlopen(SONAME_LIBXRENDER, RTLD_NOW, NULL, 0)))
    {

#define LOAD_FUNCPTR(f) if((p##f = wine_dlsym(xrender_handle, #f, NULL, 0)) == NULL) goto sym_not_found;
LOAD_FUNCPTR(XRenderAddGlyphs)
LOAD_FUNCPTR(XRenderCompositeString8)
LOAD_FUNCPTR(XRenderCompositeString16)
LOAD_FUNCPTR(XRenderCompositeString32)
LOAD_FUNCPTR(XRenderCreateGlyphSet)
LOAD_FUNCPTR(XRenderCreatePicture)
LOAD_FUNCPTR(XRenderFillRectangle)
LOAD_FUNCPTR(XRenderFindFormat)
LOAD_FUNCPTR(XRenderFindVisualFormat)
LOAD_FUNCPTR(XRenderFreeGlyphSet)
LOAD_FUNCPTR(XRenderFreePicture)
LOAD_FUNCPTR(XRenderSetPictureClipRectangles)
LOAD_FUNCPTR(XRenderQueryExtension)
#undef LOAD_FUNCPTR

        wine_tsx11_lock();
        if(pXRenderQueryExtension(gdi_display, &event_base, &error_base)) {
            X11DRV_XRender_Installed = TRUE;
            TRACE("Xrender is up and running error_base = %d\n", error_base);
            screen_format = pXRenderFindVisualFormat(gdi_display, visual);
            if(!screen_format) { /* This fails in buggy versions of libXrender.so */
                wine_tsx11_unlock();
                WINE_MESSAGE(
                    "Wine has detected that you probably have a buggy version\n"
                    "of libXrender.so .  Because of this client side font rendering\n"
                    "will be disabled.  Please upgrade this library.\n");
                X11DRV_XRender_Installed = FALSE;
                return;
            }
            pf.type = PictTypeDirect;
            pf.depth = 1;
            pf.direct.alpha = 0;
            pf.direct.alphaMask = 1;
            mono_format = pXRenderFindFormat(gdi_display, PictFormatType |
                                             PictFormatDepth | PictFormatAlpha |
                                             PictFormatAlphaMask, &pf, 0);
            if(!mono_format) {
                ERR("mono_format == NULL?\n");
                X11DRV_XRender_Installed = FALSE;
            }
        }
        wine_tsx11_unlock();
    }

sym_not_found:
    if(X11DRV_XRender_Installed || client_side_with_core)
    {
	glyphsetCache = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
				  sizeof(*glyphsetCache) * INIT_CACHE_SIZE);

	glyphsetCacheSize = INIT_CACHE_SIZE;
	lastfree = 0;
	for(i = 0; i < INIT_CACHE_SIZE; i++) {
	  glyphsetCache[i].next = i + 1;
	  glyphsetCache[i].count = -1;
	}
	glyphsetCache[i-1].next = -1;
	using_client_side_fonts = 1;

	if(!X11DRV_XRender_Installed) {
	    TRACE("Xrender is not available on your XServer, client side rendering with the core protocol instead.\n");
	    if(screen_depth <= 8 || !client_side_antialias_with_core)
	        antialias = 0;
	} else {
	    if(screen_depth <= 8 || !client_side_antialias_with_render)
	        antialias = 0;
	}
    }
    else TRACE("Using X11 core fonts\n");
}

static BOOL fontcmp(LFANDSIZE *p1, LFANDSIZE *p2)
{
  if(p1->hash != p2->hash) return TRUE;
  if(memcmp(&p1->devsize, &p2->devsize, sizeof(p1->devsize))) return TRUE;
  if(memcmp(&p1->lf, &p2->lf, offsetof(LOGFONTW, lfFaceName))) return TRUE;
  return strcmpW(p1->lf.lfFaceName, p2->lf.lfFaceName);
}

#if 0
static void walk_cache(void)
{
  int i;

  EnterCriticalSection(&xrender_cs);
  for(i=mru; i >= 0; i = glyphsetCache[i].next)
    TRACE("item %d\n", i);
  LeaveCriticalSection(&xrender_cs);
}
#endif

static int LookupEntry(LFANDSIZE *plfsz)
{
  int i, prev_i = -1;

  for(i = mru; i >= 0; i = glyphsetCache[i].next) {
    TRACE("%d\n", i);
    if(glyphsetCache[i].count == -1) { /* reached free list so stop */
      i = -1;
      break;
    }

    if(!fontcmp(&glyphsetCache[i].lfsz, plfsz)) {
      glyphsetCache[i].count++;
      if(prev_i >= 0) {
	glyphsetCache[prev_i].next = glyphsetCache[i].next;
	glyphsetCache[i].next = mru;
	mru = i;
      }
      TRACE("found font in cache %d\n", i);
      return i;
    }
    prev_i = i;
  }
  TRACE("font not in cache\n");
  return -1;
}

static void FreeEntry(int entry)
{
    int i;

    if(glyphsetCache[entry].glyphset) {
	wine_tsx11_lock();
	pXRenderFreeGlyphSet(gdi_display, glyphsetCache[entry].glyphset);
	wine_tsx11_unlock();
	glyphsetCache[entry].glyphset = 0;
    }
    if(glyphsetCache[entry].nrealized) {
	HeapFree(GetProcessHeap(), 0, glyphsetCache[entry].realized);
	glyphsetCache[entry].realized = NULL;
	if(glyphsetCache[entry].bitmaps) {
	    for(i = 0; i < glyphsetCache[entry].nrealized; i++)
		if(glyphsetCache[entry].bitmaps[i])
		    HeapFree(GetProcessHeap(), 0, glyphsetCache[entry].bitmaps[i]);
	    HeapFree(GetProcessHeap(), 0, glyphsetCache[entry].bitmaps);
	    glyphsetCache[entry].bitmaps = NULL;
	    HeapFree(GetProcessHeap(), 0, glyphsetCache[entry].gis);
	    glyphsetCache[entry].gis = NULL;
	}
	glyphsetCache[entry].nrealized = 0;
    }
}

static int AllocEntry(void)
{
  int best = -1, prev_best = -1, i, prev_i = -1;

  if(lastfree >= 0) {
    assert(glyphsetCache[lastfree].count == -1);
    glyphsetCache[lastfree].count = 1;
    best = lastfree;
    lastfree = glyphsetCache[lastfree].next;
    assert(best != mru);
    glyphsetCache[best].next = mru;
    mru = best;

    TRACE("empty space at %d, next lastfree = %d\n", mru, lastfree);
    return mru;
  }

  for(i = mru; i >= 0; i = glyphsetCache[i].next) {
    if(glyphsetCache[i].count == 0) {
      best = i;
      prev_best = prev_i;
    }
    prev_i = i;
  }

  if(best >= 0) {
    TRACE("freeing unused glyphset at cache %d\n", best);
    FreeEntry(best);
    glyphsetCache[best].count = 1;
    if(prev_best >= 0) {
      glyphsetCache[prev_best].next = glyphsetCache[best].next;
      glyphsetCache[best].next = mru;
      mru = best;
    } else {
      assert(mru == best);
    }
    return mru;
  }

  TRACE("Growing cache\n");
  
  if (glyphsetCache)
    glyphsetCache = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			      glyphsetCache,
			      (glyphsetCacheSize + INIT_CACHE_SIZE)
			      * sizeof(*glyphsetCache));
  else
    glyphsetCache = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			      (glyphsetCacheSize + INIT_CACHE_SIZE)
			      * sizeof(*glyphsetCache));

  for(best = i = glyphsetCacheSize; i < glyphsetCacheSize + INIT_CACHE_SIZE;
      i++) {
    glyphsetCache[i].next = i + 1;
    glyphsetCache[i].count = -1;
  }
  glyphsetCache[i-1].next = -1;
  glyphsetCacheSize += INIT_CACHE_SIZE;

  lastfree = glyphsetCache[best].next;
  glyphsetCache[best].count = 1;
  glyphsetCache[best].next = mru;
  mru = best;
  TRACE("new free cache slot at %d\n", mru);
  return mru;
}

static int GetCacheEntry(LFANDSIZE *plfsz)
{
    XRenderPictFormat pf;
    int ret;
    gsCacheEntry *entry;

    if((ret = LookupEntry(plfsz)) != -1) return ret;

    ret = AllocEntry();
    entry = glyphsetCache + ret;
    entry->lfsz = *plfsz;
    assert(entry->nrealized == 0);

    if(antialias)
        entry->aa = AA_Grey;
    else
        entry->aa = AA_None;

    if(X11DRV_XRender_Installed) {
        switch(entry->aa) {
	case AA_Grey:
	    pf.depth = 8;
	    pf.direct.alphaMask = 0xff;
	    break;

	default:
	    ERR("aa = %d - not implemented\n", entry->aa);
	case AA_None:
	    pf.depth = 1;
	    pf.direct.alphaMask = 1;
	    break;
	}

	pf.type = PictTypeDirect;
	pf.direct.alpha = 0;

	wine_tsx11_lock();
	entry->font_format = pXRenderFindFormat(gdi_display,
						PictFormatType |
						PictFormatDepth |
						PictFormatAlpha |
						PictFormatAlphaMask,
						&pf, 0);

	entry->glyphset = pXRenderCreateGlyphSet(gdi_display, entry->font_format);
	wine_tsx11_unlock();
    }
    return ret;
}

static void dec_ref_cache(int index)
{
    assert(index >= 0);
    TRACE("dec'ing entry %d to %d\n", index, glyphsetCache[index].count - 1);
    assert(glyphsetCache[index].count > 0);
    glyphsetCache[index].count--;
}

static void lfsz_calc_hash(LFANDSIZE *plfsz)
{
  DWORD hash = 0, *ptr;
  int i;

  hash ^= plfsz->devsize.cx;
  hash ^= plfsz->devsize.cy;
  for(i = 0, ptr = (DWORD*)&plfsz->lf; i < 7; i++, ptr++)
    hash ^= *ptr;
  for(i = 0, ptr = (DWORD*)&plfsz->lf.lfFaceName; i < LF_FACESIZE/2; i++, ptr++) {
    WCHAR *pwc = (WCHAR *)ptr;
    if(!*pwc) break;
    hash ^= *ptr;
    pwc++;
    if(!*pwc) break;
  }
  plfsz->hash = hash;
  return;
}

/***********************************************************************
 *   X11DRV_XRender_Finalize
 */
void X11DRV_XRender_Finalize(void)
{
    int i;

    EnterCriticalSection(&xrender_cs);
    for(i = mru; i >= 0; i = glyphsetCache[i].next)
	FreeEntry(i);
    LeaveCriticalSection(&xrender_cs);
}


/***********************************************************************
 *   X11DRV_XRender_SelectFont
 */
BOOL X11DRV_XRender_SelectFont(X11DRV_PDEVICE *physDev, HFONT hfont)
{
    LFANDSIZE lfsz;

    GetObjectW(hfont, sizeof(lfsz.lf), &lfsz.lf);
    TRACE("h=%ld w=%ld weight=%ld it=%d charset=%d name=%s\n",
	  lfsz.lf.lfHeight, lfsz.lf.lfWidth, lfsz.lf.lfWeight,
	  lfsz.lf.lfItalic, lfsz.lf.lfCharSet, debugstr_w(lfsz.lf.lfFaceName));
    lfsz.devsize.cx = X11DRV_XWStoDS( physDev, lfsz.lf.lfWidth );
    lfsz.devsize.cy = X11DRV_YWStoDS( physDev, lfsz.lf.lfHeight );
    lfsz_calc_hash(&lfsz);

    EnterCriticalSection(&xrender_cs);
    if(!physDev->xrender) {
        physDev->xrender = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
				     sizeof(*physDev->xrender));
	physDev->xrender->cache_index = -1;
    }
    else if(physDev->xrender->cache_index != -1)
        dec_ref_cache(physDev->xrender->cache_index);
    physDev->xrender->cache_index = GetCacheEntry(&lfsz);
    LeaveCriticalSection(&xrender_cs);
    return 0;
}

/***********************************************************************
 *   X11DRV_XRender_DeleteDC
 */
void X11DRV_XRender_DeleteDC(X11DRV_PDEVICE *physDev)
{
    wine_tsx11_lock();
    if(physDev->xrender->tile_pict)
        pXRenderFreePicture(gdi_display, physDev->xrender->tile_pict);

    if(physDev->xrender->tile_xpm)
        XFreePixmap(gdi_display, physDev->xrender->tile_xpm);

    if(physDev->xrender->pict) {
	TRACE("freeing pict = %lx dc = %p\n", physDev->xrender->pict, physDev->hdc);
        pXRenderFreePicture(gdi_display, physDev->xrender->pict);
    }
    wine_tsx11_unlock();

    EnterCriticalSection(&xrender_cs);
    if(physDev->xrender->cache_index != -1)
        dec_ref_cache(physDev->xrender->cache_index);
    LeaveCriticalSection(&xrender_cs);

    HeapFree(GetProcessHeap(), 0, physDev->xrender);
    physDev->xrender = NULL;
    return;
}

/***********************************************************************
 *   X11DRV_XRender_UpdateDrawable
 *
 * This gets called from X11DRV_SetDrawable and X11DRV_SelectBitmap.
 * It deletes the pict when the drawable changes.
 */
void X11DRV_XRender_UpdateDrawable(X11DRV_PDEVICE *physDev)
{
    if(physDev->xrender->pict) {
        TRACE("freeing pict %08lx from dc %p drawable %08lx\n", physDev->xrender->pict,
              physDev->hdc, physDev->drawable);
        wine_tsx11_lock();
        XFlush(gdi_display);
        pXRenderFreePicture(gdi_display, physDev->xrender->pict);
        wine_tsx11_unlock();
    }
    physDev->xrender->pict = 0;
    return;
}

/************************************************************************
 *   UploadGlyph
 *
 * Helper to ExtTextOut.  Must be called inside xrender_cs
 */
static BOOL UploadGlyph(X11DRV_PDEVICE *physDev, int glyph)
{
    int buflen;
    char *buf;
    Glyph gid;
    GLYPHMETRICS gm;
    XGlyphInfo gi;
    gsCacheEntry *entry = glyphsetCache + physDev->xrender->cache_index;
    UINT ggo_format = GGO_GLYPH_INDEX;

    if(entry->nrealized <= glyph) {
        entry->nrealized = (glyph / 128 + 1) * 128;

	if (entry->realized)
	    entry->realized = HeapReAlloc(GetProcessHeap(),
				      HEAP_ZERO_MEMORY,
				      entry->realized,
				      entry->nrealized * sizeof(BOOL));
	else
	    entry->realized = HeapAlloc(GetProcessHeap(),
				      HEAP_ZERO_MEMORY,
				      entry->nrealized * sizeof(BOOL));

	if(entry->glyphset == 0) {
	  if (entry->bitmaps)
	    entry->bitmaps = HeapReAlloc(GetProcessHeap(),
				      HEAP_ZERO_MEMORY,
				      entry->bitmaps,
				      entry->nrealized * sizeof(entry->bitmaps[0]));
	  else
	    entry->bitmaps = HeapAlloc(GetProcessHeap(),
				      HEAP_ZERO_MEMORY,
				      entry->nrealized * sizeof(entry->bitmaps[0]));

	  if (entry->gis)
	    entry->gis = HeapReAlloc(GetProcessHeap(),
				   HEAP_ZERO_MEMORY,
				   entry->gis,
				   entry->nrealized * sizeof(entry->gis[0]));
	  else
	    entry->gis = HeapAlloc(GetProcessHeap(),
				   HEAP_ZERO_MEMORY,
				   entry->nrealized * sizeof(entry->gis[0]));
	}
    }

    switch(entry->aa) {
    case AA_Grey:
	ggo_format |= WINE_GGO_GRAY16_BITMAP;
	break;

    default:
        ERR("aa = %d - not implemented\n", entry->aa);
    case AA_None:
        ggo_format |= GGO_BITMAP;
	break;
    }

    buflen = GetGlyphOutlineW(physDev->hdc, glyph, ggo_format, &gm, 0, NULL,
			      NULL);
    if(buflen == GDI_ERROR) {
        LeaveCriticalSection(&xrender_cs);
	return FALSE;
    }

    entry->realized[glyph] = TRUE;

    buf = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, buflen);
    GetGlyphOutlineW(physDev->hdc, glyph, ggo_format, &gm, buflen, buf, NULL);

    TRACE("buflen = %d. Got metrics: %dx%d adv=%d,%d origin=%ld,%ld\n",
	  buflen,
	  gm.gmBlackBoxX, gm.gmBlackBoxY, gm.gmCellIncX, gm.gmCellIncY,
	  gm.gmptGlyphOrigin.x, gm.gmptGlyphOrigin.y);

    gi.width = gm.gmBlackBoxX;
    gi.height = gm.gmBlackBoxY;
    gi.x = -gm.gmptGlyphOrigin.x;
    gi.y = gm.gmptGlyphOrigin.y;
    gi.xOff = gm.gmCellIncX;
    gi.yOff = gm.gmCellIncY;

    if(TRACE_ON(xrender)) {
        int pitch, i, j;
	char output[300];
	unsigned char *line;

	if(entry->aa == AA_None) {
	    pitch = ((gi.width + 31) / 32) * 4;
	    for(i = 0; i < gi.height; i++) {
	        line = buf + i * pitch;
		output[0] = '\0';
		for(j = 0; j < pitch * 8; j++) {
	            strcat(output, (line[j / 8] & (1 << (7 - (j % 8)))) ? "#" : " ");
		}
		strcat(output, "\n");
		TRACE(output);
	    }
	} else {
	    char blks[] = " .:;!o*#";
	    char str[2];

	    str[1] = '\0';
	    pitch = ((gi.width + 3) / 4) * 4;
	    for(i = 0; i < gi.height; i++) {
	        line = buf + i * pitch;
		output[0] = '\0';
		for(j = 0; j < pitch; j++) {
		    str[0] = blks[line[j] >> 5];
		    strcat(output, str);
		}
		strcat(output, "\n");
		TRACE(output);
	    }
	}
    }

    if(entry->glyphset) {
        if(entry->aa == AA_None && BitmapBitOrder(gdi_display) != MSBFirst) {
	    unsigned char *byte = buf, c;
	    int i = buflen;

	    while(i--) {
	        c = *byte;

		/* magic to flip bit order */
		c = ((c << 1) & 0xaa) | ((c >> 1) & 0x55);
		c = ((c << 2) & 0xcc) | ((c >> 2) & 0x33);
		c = ((c << 4) & 0xf0) | ((c >> 4) & 0x0f);

		*byte++ = c;
	    }
	}
	gid = glyph;
        wine_tsx11_lock();
	pXRenderAddGlyphs(gdi_display, entry->glyphset, &gid, &gi, 1,
			  buf, buflen);
	wine_tsx11_unlock();
	HeapFree(GetProcessHeap(), 0, buf);
    } else {
        entry->bitmaps[glyph] = buf;
	memcpy(&entry->gis[glyph], &gi, sizeof(gi));
    }
    return TRUE;
}

static void SharpGlyphMono(X11DRV_PDEVICE *physDev, INT x, INT y,
			    void *bitmap, XGlyphInfo *gi)
{
    unsigned char   *srcLine = bitmap, *src;
    unsigned char   bits, bitsMask;
    int             width = gi->width;
    int             stride = ((width + 31) & ~31) >> 3;
    int             height = gi->height;
    int             w;
    int             xspan, lenspan;

    TRACE("%d, %d\n", x, y);
    x -= gi->x;
    y -= gi->y;
    while (height--)
    {
        src = srcLine;
        srcLine += stride;
        w = width;
        
        bitsMask = 0x80;    /* FreeType is always MSB first */
        bits = *src++;
        
        xspan = x;
        while (w)
        {
            if (bits & bitsMask)
            {
                lenspan = 0;
                do
                {
                    lenspan++;
                    if (lenspan == w)
                        break;
                    bitsMask = bitsMask >> 1;
                    if (!bitsMask)
                    {
                        bits = *src++;
                        bitsMask = 0x80;
                    }
                } while (bits & bitsMask);
                XFillRectangle (gdi_display, physDev->drawable, 
                                physDev->gc, xspan, y, lenspan, 1);
                xspan += lenspan;
                w -= lenspan;
            }
            else
            {
                do
                {
                    w--;
                    xspan++;
                    if (!w)
                        break;
                    bitsMask = bitsMask >> 1;
                    if (!bitsMask)
                    {
                        bits = *src++;
                        bitsMask = 0x80;
                    }
                } while (!(bits & bitsMask));
            }
        }
        y++;
    }
}

static void SharpGlyphGray(X11DRV_PDEVICE *physDev, INT x, INT y,
			    void *bitmap, XGlyphInfo *gi)
{
    unsigned char   *srcLine = bitmap, *src, bits;
    int             width = gi->width;
    int             stride = ((width + 3) & ~3);
    int             height = gi->height;
    int             w;
    int             xspan, lenspan;

    x -= gi->x;
    y -= gi->y;
    while (height--)
    {
        src = srcLine;
        srcLine += stride;
        w = width;
        
        bits = *src++;
        xspan = x;
        while (w)
        {
            if (bits >= 0x80)
            {
                lenspan = 0;
                do
                {
                    lenspan++;
                    if (lenspan == w)
                        break;
                    bits = *src++;
                } while (bits >= 0x80);
                XFillRectangle (gdi_display, physDev->drawable, 
                                physDev->gc, xspan, y, lenspan, 1);
                xspan += lenspan;
                w -= lenspan;
            }
            else
            {
                do
                {
                    w--;
                    xspan++;
                    if (!w)
                        break;
                    bits = *src++;
                } while (bits < 0x80);
            }
        }
        y++;
    }
}


static void ExamineBitfield (DWORD mask, int *shift, int *len)
{
    int s, l;

    s = 0;
    while ((mask & 1) == 0)
    {
        mask >>= 1;
        s++;
    }
    l = 0;
    while ((mask & 1) == 1)
    {
        mask >>= 1;
        l++;
    }
    *shift = s;
    *len = l;
}

static DWORD GetField (DWORD pixel, int shift, int len)
{
    pixel = pixel & (((1 << (len)) - 1) << shift);
    pixel = pixel << (32 - (shift + len)) >> 24;
    while (len < 8)
    {
        pixel |= (pixel >> len);
        len <<= 1;
    }
    return pixel;
}


static DWORD PutField (DWORD pixel, int shift, int len)
{
    shift = shift - (8 - len);
    if (len <= 8)
        pixel &= (((1 << len) - 1) << (8 - len));
    if (shift < 0)
        pixel >>= -shift;
    else
        pixel <<= shift;
    return pixel;
}

static void SmoothGlyphGray(XImage *image, int x, int y, void *bitmap, XGlyphInfo *gi,
			    COLORREF color)
{
    int             r_shift, r_len;
    int             g_shift, g_len;
    int             b_shift, b_len;
    BYTE            *maskLine, *mask, m;
    int             maskStride;
    DWORD           pixel;
    int             width, height;
    int             w, tx;
    BYTE            src_r, src_g, src_b;

    src_r = GetRValue(color);
    src_g = GetGValue(color);
    src_b = GetBValue(color);

    x -= gi->x;
    y -= gi->y;
    width = gi->width;
    height = gi->height;

    maskLine = (unsigned char *) bitmap;
    maskStride = (width + 3) & ~3;

    ExamineBitfield (image->red_mask, &r_shift, &r_len);
    ExamineBitfield (image->green_mask, &g_shift, &g_len);
    ExamineBitfield (image->blue_mask, &b_shift, &b_len);
    for(; height--; y++)
    {
        mask = maskLine;
        maskLine += maskStride;
        w = width;
        tx = x;

	if(y < 0) continue;
	if(y >= image->height) break;

        for(; w--; tx++)
        {
	    if(tx >= image->width) break;

            m = *mask++;
	    if(tx < 0) continue;

	    if (m == 0xff)
	    {
	        pixel = (PutField ((src_r), r_shift, r_len) |
			 PutField ((src_g), g_shift, g_len) |
			 PutField ((src_b), b_shift, b_len));
		XPutPixel (image, tx, y, pixel);
	    }
	    else if (m)
	    {
	        BYTE r, g, b;

		pixel = XGetPixel (image, tx, y);

		r = GetField(pixel, r_shift, r_len);
		r = ((BYTE)~m * (WORD)r + (BYTE)m * (WORD)src_r) >> 8;
		g = GetField(pixel, g_shift, g_len);
		g = ((BYTE)~m * (WORD)g + (BYTE)m * (WORD)src_g) >> 8;
		b = GetField(pixel, b_shift, b_len);
		b = ((BYTE)~m * (WORD)b + (BYTE)m * (WORD)src_b) >> 8;

		pixel = (PutField (r, r_shift, r_len) |
			 PutField (g, g_shift, g_len) |
			 PutField (b, b_shift, b_len));
		XPutPixel (image, tx, y, pixel);
	    }
        }
    }
}

static int XRenderErrorHandler(Display *dpy, XErrorEvent *event, void *arg)
{
    return 1;
}

/***********************************************************************
 *   X11DRV_XRender_ExtTextOut
 */
BOOL X11DRV_XRender_ExtTextOut( X11DRV_PDEVICE *physDev, INT x, INT y, UINT flags,
				const RECT *lprect, LPCWSTR wstr, UINT count,
				const INT *lpDx, INT breakExtra )
{
    XRenderColor col;
    int idx;
    TEXTMETRICW tm;
    RGNDATA *data;
    SIZE sz;
    RECT rc;
    BOOL done_extents = FALSE;
    INT width, xwidth, ywidth;
    double cosEsc, sinEsc;
    XGCValues xgcval;
    LOGFONTW lf;
    int render_op = PictOpOver;
    WORD *glyphs;
    POINT pt;
    gsCacheEntry *entry;
    BOOL retv = FALSE;
    HDC hdc = physDev->hdc;
    int textPixel, backgroundPixel;
    INT *deltas = NULL;
    INT char_extra;
    HRGN saved_region = 0;
    UINT align = GetTextAlign( hdc );
    COLORREF textColor = GetTextColor( hdc );

    TRACE("%p, %d, %d, %08x, %p, %s, %d, %p)\n", hdc, x, y, flags,
	  lprect, debugstr_wn(wstr, count), count, lpDx);

    if(flags & ETO_GLYPH_INDEX)
        glyphs = (LPWORD)wstr;
    else {
        glyphs = HeapAlloc(GetProcessHeap(), 0, count * sizeof(WCHAR));
        GetGlyphIndicesW(hdc, wstr, count, glyphs, 0);
    }

    if(lprect)
      TRACE("rect: %ld,%ld - %ld,%ld\n", lprect->left, lprect->top, lprect->right,
	    lprect->bottom);
    TRACE("align = %x bkmode = %x mapmode = %x\n", align, GetBkMode(hdc), GetMapMode(hdc));

    if(align & TA_UPDATECP)
    {
        GetCurrentPositionEx( hdc, &pt );
        x = pt.x;
        y = pt.y;
    }

    GetObjectW(GetCurrentObject(hdc, OBJ_FONT), sizeof(lf), &lf);
    if(lf.lfEscapement != 0) {
        cosEsc = cos(lf.lfEscapement * M_PI / 1800);
	sinEsc = sin(lf.lfEscapement * M_PI / 1800);
    } else {
        cosEsc = 1;
	sinEsc = 0;
    }

    if(flags & (ETO_CLIPPED | ETO_OPAQUE)) {
        if(!lprect) {
	    if(flags & ETO_CLIPPED) return FALSE;
	        GetTextExtentPointI(hdc, glyphs, count, &sz);
		done_extents = TRUE;
		rc.left = x;
		rc.top = y;
		rc.right = x + sz.cx;
		rc.bottom = y + sz.cy;
	} else {
	    rc = *lprect;
	}

	LPtoDP(hdc, (POINT*)&rc, 2);

	if(rc.left > rc.right) {INT tmp = rc.left; rc.left = rc.right; rc.right = tmp;}
	if(rc.top > rc.bottom) {INT tmp = rc.top; rc.top = rc.bottom; rc.bottom = tmp;}
    }

    xgcval.function = GXcopy;
    xgcval.background = physDev->backgroundPixel;
    xgcval.fill_style = FillSolid;
    wine_tsx11_lock();
    XChangeGC( gdi_display, physDev->gc, GCFunction | GCBackground | GCFillStyle, &xgcval );
    wine_tsx11_unlock();

    X11DRV_LockDIBSection( physDev, DIB_Status_GdiMod, FALSE );

    if(physDev->depth == 1) {
        if((textColor & 0xffffff) == 0) {
	    textPixel = 0;
	    backgroundPixel = 1;
	} else {
	    textPixel = 1;
	    backgroundPixel = 0;
	}
    } else {
        textPixel = physDev->textPixel;
	backgroundPixel = physDev->backgroundPixel;
    }

    if(flags & ETO_OPAQUE) {
        wine_tsx11_lock();
        XSetForeground( gdi_display, physDev->gc, backgroundPixel );
        XFillRectangle( gdi_display, physDev->drawable, physDev->gc,
                        physDev->org.x + rc.left, physDev->org.y + rc.top,
                        rc.right - rc.left, rc.bottom - rc.top );
        wine_tsx11_unlock();
    }

    if(count == 0) {
	retv =  TRUE;
        goto done;
    }

    pt.x = x;
    pt.y = y;
    LPtoDP(hdc, &pt, 1);
    x = pt.x;
    y = pt.y;

    TRACE("real x,y %d,%d\n", x, y);

    if((char_extra = GetTextCharacterExtra(hdc)) != 0) {
        INT i;
	SIZE tmpsz;
        deltas = HeapAlloc(GetProcessHeap(), 0, count * sizeof(INT));
	for(i = 0; i < count; i++) {
	    if(lpDx)
	        deltas[i] = lpDx[i] + char_extra;
	    else {
	        GetTextExtentPointI(hdc, glyphs + i, 1, &tmpsz);
		deltas[i] = tmpsz.cx;
	    }
	}
    } else if(lpDx)
        deltas = (INT*)lpDx;

    if(deltas) {
	width = 0;
	for(idx = 0; idx < count; idx++)
	    width += deltas[idx];
    } else {
	if(!done_extents) {
	    GetTextExtentPointI(hdc, glyphs, count, &sz);
	    done_extents = TRUE;
	}
	width = sz.cx;
    }
    width = X11DRV_XWStoDS(physDev, width);
    xwidth = width * cosEsc;
    ywidth = width * sinEsc;

    GetTextMetricsW(hdc, &tm);

    tm.tmAscent = X11DRV_YWStoDS(physDev, tm.tmAscent);
    tm.tmDescent = X11DRV_YWStoDS(physDev, tm.tmDescent);
    switch( align & (TA_LEFT | TA_RIGHT | TA_CENTER) ) {
    case TA_LEFT:
        if (align & TA_UPDATECP) {
	    pt.x = x + xwidth;
	    pt.y = y - ywidth;
	    DPtoLP(hdc, &pt, 1);
	    MoveToEx(hdc, pt.x, pt.y, NULL);
	}
	break;

    case TA_CENTER:
        x -= xwidth / 2;
	y += ywidth / 2;
	break;

    case TA_RIGHT:
        x -= xwidth;
	y += ywidth;
	if (align & TA_UPDATECP) {
	    pt.x = x;
	    pt.y = y;
	    DPtoLP(hdc, &pt, 1);
	    MoveToEx(hdc, pt.x, pt.y, NULL);
	}
	break;
    }

    switch( align & (TA_TOP | TA_BOTTOM | TA_BASELINE) ) {
    case TA_TOP:
        y += tm.tmAscent * cosEsc;
	x += tm.tmAscent * sinEsc;
	break;

    case TA_BOTTOM:
        y -= tm.tmDescent * cosEsc;
	x -= tm.tmDescent * sinEsc;
	break;

    case TA_BASELINE:
        break;
    }

    if (flags & ETO_CLIPPED)
    {
        HRGN clip_region;
        RECT clip_rect = *lprect;

        LPtoDP( hdc, (POINT *)&clip_rect, 2 );
        clip_region = CreateRectRgnIndirect( &clip_rect );
        /* make a copy of the current device region */
        saved_region = CreateRectRgn( 0, 0, 0, 0 );
        CombineRgn( saved_region, physDev->region, 0, RGN_COPY );
        X11DRV_SetDeviceClipping( physDev, saved_region, clip_region );
        DeleteObject( clip_region );
    }

    if(X11DRV_XRender_Installed) {
        if(!physDev->xrender->pict) {
	    XRenderPictureAttributes pa;
	    pa.subwindow_mode = IncludeInferiors;

	    wine_tsx11_lock();
	    physDev->xrender->pict = pXRenderCreatePicture(gdi_display,
							   physDev->drawable,
							   (physDev->depth == 1) ?
							   mono_format : screen_format,
							   CPSubwindowMode, &pa);
	    wine_tsx11_unlock();

	    TRACE("allocing pict = %lx dc = %p drawable = %08lx\n",
                  physDev->xrender->pict, hdc, physDev->drawable);
	} else {
	    TRACE("using existing pict = %lx dc = %p drawable = %08lx\n",
                  physDev->xrender->pict, hdc, physDev->drawable);
	}

	if ((data = X11DRV_GetRegionData( physDev->region, 0 )))
	{
	    wine_tsx11_lock();
	    pXRenderSetPictureClipRectangles( gdi_display, physDev->xrender->pict,
					      physDev->org.x, physDev->org.y,
					      (XRectangle *)data->Buffer, data->rdh.nCount );
	    wine_tsx11_unlock();
	    HeapFree( GetProcessHeap(), 0, data );
	}
    }

    if(GetBkMode(hdc) != TRANSPARENT) {
        if(!((flags & ETO_CLIPPED) && (flags & ETO_OPAQUE))) {
	    if(!(flags & ETO_OPAQUE) || x < rc.left || x + width >= rc.right ||
	       y - tm.tmAscent < rc.top || y + tm.tmDescent >= rc.bottom) {
                wine_tsx11_lock();
                XSetForeground( gdi_display, physDev->gc, backgroundPixel );
                XFillRectangle( gdi_display, physDev->drawable, physDev->gc,
                                physDev->org.x + x, physDev->org.y + y - tm.tmAscent,
                                width, tm.tmAscent + tm.tmDescent );
                wine_tsx11_unlock();
	    }
	}
    }

    if(X11DRV_XRender_Installed) {
        /* Create a 1x1 pixmap to tile over the font mask */
        if(!physDev->xrender->tile_xpm) {
	    XRenderPictureAttributes pa;

	    XRenderPictFormat *format = (physDev->depth == 1) ? mono_format : screen_format;
	    wine_tsx11_lock();
	    physDev->xrender->tile_xpm = XCreatePixmap(gdi_display,
						       physDev->drawable,
						       1, 1,
						       format->depth);
	    pa.repeat = True;
	    physDev->xrender->tile_pict = pXRenderCreatePicture(gdi_display,
								physDev->xrender->tile_xpm,
								format,
								CPRepeat, &pa);
	    wine_tsx11_unlock();
	    TRACE("Created pixmap of depth %d\n", format->depth);
	    /* init lastTextColor to something different from textColor */
	    physDev->xrender->lastTextColor = ~textColor;

	}

	if(textColor != physDev->xrender->lastTextColor) {
	    if(physDev->depth != 1) {
	      /* Map 0 -- 0xff onto 0 -- 0xffff */
	        col.red = GetRValue(textColor);
		col.red |= col.red << 8;
		col.green = GetGValue(textColor);
		col.green |= col.green << 8;
		col.blue = GetBValue(textColor);
		col.blue |= col.blue << 8;
		col.alpha = 0x0;
	    } else { /* for a 1bpp bitmap we always need a 1 in the tile */
	        col.red = col.green = col.blue = 0;
		col.alpha = 0xffff;
	    }
	    wine_tsx11_lock();
	    pXRenderFillRectangle(gdi_display, PictOpSrc,
				  physDev->xrender->tile_pict,
				  &col, 0, 0, 1, 1);
	    wine_tsx11_unlock();
	    physDev->xrender->lastTextColor = textColor;
	}

	/* FIXME the mapping of Text/BkColor onto 1 or 0 needs investigation.
	 */
	if((physDev->depth == 1) && (textPixel == 0))
	    render_op = PictOpOutReverse; /* This gives us 'black' text */
    }

    EnterCriticalSection(&xrender_cs);
    entry = glyphsetCache + physDev->xrender->cache_index;

    for(idx = 0; idx < count; idx++) {
        if(glyphs[idx] >= entry->nrealized || entry->realized[glyphs[idx]] == FALSE) {
	    UploadGlyph(physDev, glyphs[idx]);
	}
    }


    TRACE("Writing %s at %ld,%ld\n", debugstr_wn(wstr,count),
          physDev->org.x + x, physDev->org.y + y);

    if(X11DRV_XRender_Installed) {
        wine_tsx11_lock();
	if(!deltas)
	    pXRenderCompositeString16(gdi_display, render_op,
				      physDev->xrender->tile_pict,
				      physDev->xrender->pict,
				      entry->font_format, entry->glyphset,
				      0, 0, physDev->org.x + x, physDev->org.y + y,
				      glyphs, count);

	else {
	    INT offset = 0, xoff = 0, yoff = 0;
	    for(idx = 0; idx < count; idx++) {
	        pXRenderCompositeString16(gdi_display, render_op,
					  physDev->xrender->tile_pict,
					  physDev->xrender->pict,
					  entry->font_format, entry->glyphset,
					  0, 0, physDev->org.x + x + xoff,
					  physDev->org.y + y + yoff,
					  glyphs + idx, 1);
		offset += X11DRV_XWStoDS(physDev, deltas[idx]);
		xoff = offset * cosEsc;
		yoff = offset * -sinEsc;
	    }
	}
	wine_tsx11_unlock();

	if (lf.lfUnderline || lf.lfStrikeOut) {
	    int linePos;
	    unsigned int lineWidth;
	    UINT nMetricsSize = GetOutlineTextMetricsW(hdc, 0, NULL);
	    OUTLINETEXTMETRICW* otm = HeapAlloc(GetProcessHeap(), 0, nMetricsSize);
	    if (!otm) goto done;

	    GetOutlineTextMetricsW(hdc, nMetricsSize, otm);

            wine_tsx11_lock();
	    XSetForeground( gdi_display, physDev->gc, physDev->textPixel );

	    if (lf.lfUnderline) {
                linePos = X11DRV_YWStoDS(physDev, otm->otmsUnderscorePosition);
                lineWidth = X11DRV_YWStoDS(physDev, otm->otmsUnderscoreSize);

                XSetLineAttributes( gdi_display, physDev->gc, lineWidth,
                                    LineSolid, CapProjecting, JoinBevel );
                XDrawLine( gdi_display, physDev->drawable, physDev->gc,
                           physDev->org.x + x, physDev->org.y + y - linePos,
                           physDev->org.x + x + width, physDev->org.y + y - linePos );
	    }

	    if (lf.lfStrikeOut) { 
                linePos = X11DRV_YWStoDS(physDev, otm->otmsStrikeoutPosition);
                lineWidth = X11DRV_YWStoDS(physDev, otm->otmsStrikeoutSize);

                XSetLineAttributes( gdi_display, physDev->gc, lineWidth,
                                    LineSolid, CapProjecting, JoinBevel );
                XDrawLine( gdi_display, physDev->drawable, physDev->gc,
                           physDev->org.x + x, physDev->org.y + y - linePos,
                           physDev->org.x + x + width, physDev->org.y + y - linePos );
	    } 
            wine_tsx11_unlock();
	    HeapFree(GetProcessHeap(), 0, otm);
	}

    } else {
        INT offset = 0, xoff = 0, yoff = 0;
        wine_tsx11_lock();
	XSetForeground( gdi_display, physDev->gc, textPixel );

	if(entry->aa == AA_None) {
	    for(idx = 0; idx < count; idx++) {
	        SharpGlyphMono(physDev, physDev->org.x + x + xoff,
			       physDev->org.y + y + yoff,
			       entry->bitmaps[glyphs[idx]],
			       &entry->gis[glyphs[idx]]);
		if(deltas) {
		    offset += X11DRV_XWStoDS(physDev, deltas[idx]);
		    xoff = offset * cosEsc;
		    yoff = offset * -sinEsc;

		} else {
		    xoff += entry->gis[glyphs[idx]].xOff;
		    yoff += entry->gis[glyphs[idx]].yOff;
		}
	    }
	} else if(physDev->depth == 1) {
	    for(idx = 0; idx < count; idx++) {
	        SharpGlyphGray(physDev, physDev->org.x + x + xoff,
			       physDev->org.y + y + yoff,
			       entry->bitmaps[glyphs[idx]],
			       &entry->gis[glyphs[idx]]);
		if(deltas) {
		    offset += X11DRV_XWStoDS(physDev, deltas[idx]);
		    xoff = offset * cosEsc;
		    yoff = offset * -sinEsc;

		} else {
		    xoff += entry->gis[glyphs[idx]].xOff;
		    yoff += entry->gis[glyphs[idx]].yOff;
		}
		    
	    }
	} else {
	    XImage *image;
	    unsigned int w, h, dummy_uint;
	    Window dummy_window;
	    int dummy_int;
	    int image_x, image_y, image_off_x, image_off_y, image_w, image_h;
	    RECT extents = {0, 0, 0, 0};
	    POINT cur = {0, 0};
	    
	    
	    XGetGeometry(gdi_display, physDev->drawable, &dummy_window, &dummy_int, &dummy_int,
			 &w, &h, &dummy_uint, &dummy_uint);
	    TRACE("drawable %dx%d\n", w, h);

	    for(idx = 0; idx < count; idx++) {
	        if(extents.left > cur.x - entry->gis[glyphs[idx]].x)
		    extents.left = cur.x - entry->gis[glyphs[idx]].x;
		if(extents.top > cur.y - entry->gis[glyphs[idx]].y)
		    extents.top = cur.y - entry->gis[glyphs[idx]].y;
		if(extents.right < cur.x - entry->gis[glyphs[idx]].x + entry->gis[glyphs[idx]].width)
		    extents.right = cur.x - entry->gis[glyphs[idx]].x + entry->gis[glyphs[idx]].width;
		if(extents.bottom < cur.y - entry->gis[glyphs[idx]].y + entry->gis[glyphs[idx]].height)
		    extents.bottom = cur.y - entry->gis[glyphs[idx]].y + entry->gis[glyphs[idx]].height;
		if(deltas) {
		    offset += X11DRV_XWStoDS(physDev, deltas[idx]);
		    cur.x = offset * cosEsc;
		    cur.y = offset * -sinEsc;
		} else {
		    cur.x += entry->gis[glyphs[idx]].xOff;
		    cur.y += entry->gis[glyphs[idx]].yOff;
		}
	    }
	    TRACE("glyph extents %ld,%ld - %ld,%ld drawable x,y %ld,%ld\n", extents.left, extents.top,
		  extents.right, extents.bottom, physDev->org.x + x, physDev->org.y + y);

	    if(physDev->org.x + x + extents.left >= 0) {
	        image_x = physDev->org.x + x + extents.left;
		image_off_x = 0;
	    } else {
	        image_x = 0;
		image_off_x = physDev->org.x + x + extents.left;
	    }
	    if(physDev->org.y + y + extents.top >= 0) {
	        image_y = physDev->org.y + y + extents.top;
		image_off_y = 0;
	    } else {
	        image_y = 0;
		image_off_y = physDev->org.y + y + extents.top;
	    }
	    if(physDev->org.x + x + extents.right < w)
	        image_w = physDev->org.x + x + extents.right - image_x;
	    else
	        image_w = w - image_x;
	    if(physDev->org.y + y + extents.bottom < h)
	        image_h = physDev->org.y + y + extents.bottom - image_y;
	    else
	        image_h = h - image_y;

	    if(image_w <= 0 || image_h <= 0) goto no_image;

	    X11DRV_expect_error(gdi_display, XRenderErrorHandler, NULL);
	    image = XGetImage(gdi_display, physDev->drawable,
			      image_x, image_y, image_w, image_h,
			      AllPlanes, ZPixmap);
	    X11DRV_check_error();

	    TRACE("XGetImage(%p, %x, %d, %d, %d, %d, %lx, %x) depth = %d rets %p\n",
		  gdi_display, (int)physDev->drawable, image_x, image_y,
		  image_w, image_h, AllPlanes, ZPixmap,
		  physDev->depth, image);
	    if(!image) {
	        Pixmap xpm = XCreatePixmap(gdi_display, physDev->drawable, image_w, image_h,
					   physDev->depth);
		GC gc;
		XGCValues gcv;

		gcv.graphics_exposures = False;
		gc = XCreateGC(gdi_display, xpm, GCGraphicsExposures, &gcv);
		XCopyArea(gdi_display, physDev->drawable, xpm, gc, image_x, image_y,
			  image_w, image_h, 0, 0);
		XFreeGC(gdi_display, gc);
		X11DRV_expect_error(gdi_display, XRenderErrorHandler, NULL);
		image = XGetImage(gdi_display, xpm, 0, 0, image_w, image_h, AllPlanes,
				  ZPixmap);
		X11DRV_check_error();
		XFreePixmap(gdi_display, xpm);
	    }
	    if(!image) goto no_image;

	    image->red_mask = visual->red_mask;
	    image->green_mask = visual->green_mask;
	    image->blue_mask = visual->blue_mask;

	    offset = xoff = yoff = 0;
	    for(idx = 0; idx < count; idx++) {
	        SmoothGlyphGray(image, xoff + image_off_x - extents.left,
				yoff + image_off_y - extents.top,
				entry->bitmaps[glyphs[idx]],
				&entry->gis[glyphs[idx]],
				textColor);
		if(deltas) {
		    offset += X11DRV_XWStoDS(physDev, deltas[idx]);
		    xoff = offset * cosEsc;
		    yoff = offset * -sinEsc;
		} else {
		    xoff += entry->gis[glyphs[idx]].xOff;
		    yoff += entry->gis[glyphs[idx]].yOff;
		}
		    
	    }
	    XPutImage(gdi_display, physDev->drawable, physDev->gc, image, 0, 0,
		      image_x, image_y, image_w, image_h);
	    XDestroyImage(image);
	}
    no_image:
	wine_tsx11_unlock();
    }
    LeaveCriticalSection(&xrender_cs);

    if(deltas && deltas != lpDx)
        HeapFree(GetProcessHeap(), 0, deltas);

    if (flags & ETO_CLIPPED)
    {
        /* restore the device region */
        X11DRV_SetDeviceClipping( physDev, saved_region, 0 );
        DeleteObject( saved_region );
    }

    retv = TRUE;

done:
    X11DRV_UnlockDIBSection( physDev, TRUE );
    if(glyphs != wstr) HeapFree(GetProcessHeap(), 0, glyphs);
    return retv;
}

#else /* HAVE_X11_EXTENSIONS_XRENDER_H */

void X11DRV_XRender_Init(void)
{
    TRACE("XRender support not compiled in.\n");
    return;
}

void X11DRV_XRender_Finalize(void)
{
}

BOOL X11DRV_XRender_SelectFont(X11DRV_PDEVICE *physDev, HFONT hfont)
{
  assert(0);
  return FALSE;
}

void X11DRV_XRender_DeleteDC(X11DRV_PDEVICE *physDev)
{
  assert(0);
  return;
}

BOOL X11DRV_XRender_ExtTextOut( X11DRV_PDEVICE *physDev, INT x, INT y, UINT flags,
				const RECT *lprect, LPCWSTR wstr, UINT count,
				const INT *lpDx, INT breakExtra )
{
  assert(0);
  return FALSE;
}

void X11DRV_XRender_UpdateDrawable(X11DRV_PDEVICE *physDev)
{
  assert(0);
  return;
}

#endif /* HAVE_X11_EXTENSIONS_XRENDER_H */
