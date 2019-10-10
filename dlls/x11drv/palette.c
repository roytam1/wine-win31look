/*
 * X11DRV OEM bitmap objects
 *
 * Copyright 1994, 1995 Alexandre Julliard
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

#include <stdlib.h>
#include <string.h>

#include "gdi.h"
#include "windef.h"
#include "winreg.h"
#include "x11drv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(palette);

/* Palette indexed mode:
 *	logical palette -> mapping -> pixel
 *
 *
 * Windows needs contiguous color space ( from 0 to n ) but
 * it is possible only with the private colormap. Otherwise we
 * have to map DC palette indices to real pixel values. With
 * private colormaps it boils down to the identity mapping. The
 * other special case is when we have a fixed color visual with
 * the screendepth > 8 - we abandon palette mappings altogether
 * because pixel values can be calculated without X server
 * assistance.
 *
 * Windows palette manager is described in the
 * http://premium.microsoft.com/msdn/library/techart/f30/f34/f40/d4d/sa942.htm
 */

static PALETTEENTRY *COLOR_sysPal; /* current system palette */

static int COLOR_gapStart = 256;
static int COLOR_gapEnd = -1;
static int COLOR_gapFilled = 0;
static int COLOR_max = 256;

Colormap X11DRV_PALETTE_PaletteXColormap = 0;
UINT16   X11DRV_PALETTE_PaletteFlags     = 0;

typedef struct {
    int shift;
    int scale;
    int max;
} ColorShifts;

/* initialize to zero to handle abortive X11DRV_PALETTE_VIRTUAL visuals */
static ColorShifts X11DRV_PALETTE_PRed   = {0,0,0};
static ColorShifts X11DRV_PALETTE_LRed   = {0,0,0};
static ColorShifts X11DRV_PALETTE_PGreen = {0,0,0};
static ColorShifts X11DRV_PALETTE_LGreen = {0,0,0};
static ColorShifts X11DRV_PALETTE_PBlue  = {0,0,0};
static ColorShifts X11DRV_PALETTE_LBlue  = {0,0,0};
static int X11DRV_PALETTE_Graymax        = 0;

static int palette_size;

/* First free dynamic color cell, 0 = full palette, -1 = fixed palette */
static int           X11DRV_PALETTE_firstFree = 0;
static unsigned char X11DRV_PALETTE_freeList[256];

/**********************************************************************/

   /* Map an EGA index (0..15) to a pixel value in the system color space.  */

int X11DRV_PALETTE_mapEGAPixel[16];

/**********************************************************************/

#define NB_COLORCUBE_START_INDEX	63
#define NB_PALETTE_EMPTY_VALUE          -1

/* Maps entry in the system palette to X pixel value */
int *X11DRV_PALETTE_PaletteToXPixel = NULL;

/* Maps pixel to the entry in the system palette */
int *X11DRV_PALETTE_XPixelToPalette = NULL;

/**********************************************************************/

static BOOL X11DRV_PALETTE_BuildPrivateMap( const PALETTEENTRY *sys_pal_template );
static BOOL X11DRV_PALETTE_BuildSharedMap( const PALETTEENTRY *sys_pal_template );
static void X11DRV_PALETTE_ComputeShifts(unsigned long maskbits, ColorShifts *physical, ColorShifts *to_logical);
static void X11DRV_PALETTE_FillDefaultColors( const PALETTEENTRY *sys_pal_template );
static void X11DRV_PALETTE_FormatSystemPalette(void);
static BOOL X11DRV_PALETTE_CheckSysColor( const PALETTEENTRY *sys_pal_template, COLORREF c);
static int X11DRV_PALETTE_LookupSystemXPixel(COLORREF col);


/***********************************************************************
 *           COLOR_Init
 *
 * Initialize color management.
 */
int X11DRV_PALETTE_Init(void)
{
    int	mask, white, black;
    int monoPlane;
    PALETTEENTRY sys_pal_template[NB_RESERVED_COLORS];

    TRACE("initializing palette manager...\n");

    white = WhitePixel( gdi_display, DefaultScreen(gdi_display) );
    black = BlackPixel( gdi_display, DefaultScreen(gdi_display) );
    monoPlane = 1;
    for( mask = 1; !((white & mask)^(black & mask)); mask <<= 1 )
	 monoPlane++;
    X11DRV_PALETTE_PaletteFlags = (white & mask) ? X11DRV_PALETTE_WHITESET : 0;
    palette_size = visual->map_entries;

    switch(visual->class)
    {
    case DirectColor:
	X11DRV_PALETTE_PaletteFlags |= X11DRV_PALETTE_VIRTUAL;
    case GrayScale:
    case PseudoColor:
    {
	HKEY hkey;
	BOOL private_color_map = FALSE;
	if(!RegOpenKeyA(HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\x11drv", &hkey))
	{
	    char buffer[20];
	    DWORD type, count = sizeof(buffer);
	    if(!RegQueryValueExA(hkey, "PrivateColorMap", 0, &type, buffer, &count))
            {
                char ch = buffer[0];
                private_color_map = (ch == 'y' || ch == 'Y' || ch == 't' || ch == 'T' || ch == '1');
            }
	    RegCloseKey(hkey);
	}

        wine_tsx11_lock();
	if (private_color_map)
	{
	    XSetWindowAttributes win_attr;

	    X11DRV_PALETTE_PaletteXColormap = XCreateColormap( gdi_display, root_window,
                                                               visual, AllocAll );
	    if (X11DRV_PALETTE_PaletteXColormap)
	    {
	        X11DRV_PALETTE_PaletteFlags |= (X11DRV_PALETTE_PRIVATE | X11DRV_PALETTE_WHITESET);

		monoPlane = 1;
		for( white = palette_size - 1; !(white & 1); white >>= 1 )
		     monoPlane++;

	        if( root_window != DefaultRootWindow(gdi_display) )
	        {
		    win_attr.colormap = X11DRV_PALETTE_PaletteXColormap;
		    XChangeWindowAttributes( gdi_display, root_window, CWColormap, &win_attr );
		}
	    }
	} else {
	  X11DRV_PALETTE_PaletteXColormap = XCreateColormap(gdi_display, root_window,
							    visual, AllocNone);
	}
        wine_tsx11_unlock();
        break;
    }

    case StaticGray:
        wine_tsx11_lock();
        X11DRV_PALETTE_PaletteXColormap = XCreateColormap(gdi_display, root_window,
                                                          visual, AllocNone);
	X11DRV_PALETTE_PaletteFlags |= X11DRV_PALETTE_FIXED;
        X11DRV_PALETTE_Graymax = (1 << screen_depth)-1;
        wine_tsx11_unlock();
        break;

    case TrueColor:
	X11DRV_PALETTE_PaletteFlags |= X11DRV_PALETTE_VIRTUAL;
    case StaticColor: {
    	int *depths,nrofdepths;
	/* FIXME: hack to detect XFree32 XF_VGA16 ... We just have
	 * depths 1 and 4
	 */
        wine_tsx11_lock();
        depths = XListDepths(gdi_display,DefaultScreen(gdi_display),&nrofdepths);
	if ((nrofdepths==2) && ((depths[0]==4) || depths[1]==4)) {
	    monoPlane = 1;
	    for( white = palette_size - 1; !(white & 1); white >>= 1 )
	        monoPlane++;
    	    X11DRV_PALETTE_PaletteFlags = (white & mask) ? X11DRV_PALETTE_WHITESET : 0;
            X11DRV_PALETTE_PaletteXColormap = XCreateColormap(gdi_display, root_window,
                                                              visual, AllocNone);
	}
        else
        {
            X11DRV_PALETTE_PaletteXColormap = XCreateColormap(gdi_display, root_window,
                                                              visual, AllocNone);
            X11DRV_PALETTE_PaletteFlags |= X11DRV_PALETTE_FIXED;
            X11DRV_PALETTE_ComputeShifts(visual->red_mask, &X11DRV_PALETTE_PRed, &X11DRV_PALETTE_LRed);
            X11DRV_PALETTE_ComputeShifts(visual->green_mask, &X11DRV_PALETTE_PGreen, &X11DRV_PALETTE_LGreen);
            X11DRV_PALETTE_ComputeShifts(visual->blue_mask, &X11DRV_PALETTE_PBlue, &X11DRV_PALETTE_LBlue);
        }
        XFree(depths);
        wine_tsx11_unlock();
        break;
      }
    }

    TRACE(" visual class %i (%i)\n",  visual->class, monoPlane);

    GetPaletteEntries( GetStockObject(DEFAULT_PALETTE), 0, NB_RESERVED_COLORS, sys_pal_template );

    if( X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_VIRTUAL )
    {
        palette_size = 0;
    }
    else
    {
        if (X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_PRIVATE)
            X11DRV_PALETTE_BuildPrivateMap( sys_pal_template );
        else
            X11DRV_PALETTE_BuildSharedMap( sys_pal_template );

        /* Build free list */

        if( X11DRV_PALETTE_firstFree != -1 )
            X11DRV_PALETTE_FormatSystemPalette();

        X11DRV_PALETTE_FillDefaultColors( sys_pal_template );
        palette_size = visual->map_entries;
    }

    return palette_size;
}

/***********************************************************************
 *           X11DRV_PALETTE_Cleanup
 *
 * Free external colors we grabbed in the FillDefaultPalette()
 */
void X11DRV_PALETTE_Cleanup(void)
{
  if( COLOR_gapFilled )
  {
      wine_tsx11_lock();
      XFreeColors(gdi_display, X11DRV_PALETTE_PaletteXColormap,
		  (unsigned long*)(X11DRV_PALETTE_PaletteToXPixel + COLOR_gapStart),
		  COLOR_gapFilled, 0);
      wine_tsx11_unlock();
  }
}

/***********************************************************************
 *		X11DRV_PALETTE_ComputeShifts
 *
 * Calculate conversion parameters for direct mapped visuals
 */
static void X11DRV_PALETTE_ComputeShifts(unsigned long maskbits, ColorShifts *physical, ColorShifts *to_logical)
{
    int i;

    if (maskbits==0)
    {
        physical->shift=0;
        physical->scale=0;
        physical->max=0;
        to_logical->shift=0;
        to_logical->scale=0;
        to_logical->max=0;
        return;
    }

    for(i=0;!(maskbits&1);i++)
        maskbits >>= 1;

    physical->shift = i;
    physical->max = maskbits;

    for(i=0;maskbits!=0;i++)
        maskbits >>= 1;
    physical->scale = i;

    if (physical->scale>8)
    {
        /* On FreeBSD, VNC's default 32bpp mode is bgrabb (ffc00000,3ff800,7ff)!
         * So we adjust the shifts to also normalize the color fields to
         * the Win32 standard of 8 bits per color.
         */
        to_logical->shift=physical->shift+(physical->scale-8);
        to_logical->scale=8;
        to_logical->max=0xff;
    } else {
        to_logical->shift=physical->shift;
        to_logical->scale=physical->scale;
        to_logical->max=physical->max;
    }
}

/***********************************************************************
 *           X11DRV_PALETTE_BuildPrivateMap
 *
 * Allocate colorcells and initialize mapping tables.
 */
static BOOL X11DRV_PALETTE_BuildPrivateMap( const PALETTEENTRY *sys_pal_template )
{
    /* Private colormap - identity mapping */

    XColor color;
    int i;

    if((COLOR_sysPal = (PALETTEENTRY*)HeapAlloc(GetProcessHeap(), 0, sizeof(PALETTEENTRY)*palette_size)) == NULL) {
        WARN("Can not allocate system palette\n");
        return FALSE;
    }

    TRACE("Building private map - %i palette entries\n", palette_size);

      /* Allocate system palette colors */

    wine_tsx11_lock();
    for( i=0; i < palette_size; i++ )
    {
       if( i < NB_RESERVED_COLORS/2 )
       {
         color.red   = sys_pal_template[i].peRed * 65535 / 255;
         color.green = sys_pal_template[i].peGreen * 65535 / 255;
         color.blue  = sys_pal_template[i].peBlue * 65535 / 255;
	 COLOR_sysPal[i] = sys_pal_template[i];
         COLOR_sysPal[i].peFlags |= PC_SYS_USED;
       }
       else if( i >= palette_size - NB_RESERVED_COLORS/2 )
       {
	 int j = NB_RESERVED_COLORS + i - palette_size;
         color.red   = sys_pal_template[j].peRed * 65535 / 255;
         color.green = sys_pal_template[j].peGreen * 65535 / 255;
         color.blue  = sys_pal_template[j].peBlue * 65535 / 255;
	 COLOR_sysPal[i] = sys_pal_template[j];
         COLOR_sysPal[i].peFlags |= PC_SYS_USED;
       }

       color.flags = DoRed | DoGreen | DoBlue;
       color.pixel = i;
       XStoreColor(gdi_display, X11DRV_PALETTE_PaletteXColormap, &color);

       /* Set EGA mapping if color is from the first or last eight */

       if (i < 8)
           X11DRV_PALETTE_mapEGAPixel[i] = color.pixel;
       else if (i >= palette_size - 8 )
           X11DRV_PALETTE_mapEGAPixel[i - (palette_size - 16)] = color.pixel;
    }
    wine_tsx11_unlock();

    X11DRV_PALETTE_XPixelToPalette = X11DRV_PALETTE_PaletteToXPixel = NULL;

    COLOR_gapStart = 256; COLOR_gapEnd = -1;

    X11DRV_PALETTE_firstFree = (palette_size > NB_RESERVED_COLORS)?NB_RESERVED_COLORS/2 : -1;

    return FALSE;
}

/***********************************************************************
 *           X11DRV_PALETTE_BuildSharedMap
 *
 * Allocate colorcells and initialize mapping tables.
 */
static BOOL X11DRV_PALETTE_BuildSharedMap( const PALETTEENTRY *sys_pal_template )
{
   XColor		color;
   unsigned long        sysPixel[NB_RESERVED_COLORS];
   unsigned long*	pixDynMapping = NULL;
   unsigned long	plane_masks[1];
   int			i, j, warn = 0;
   int			diff, r, g, b, max = 256, bp = 0, wp = 1;
   int			step = 1;
   int			defaultCM_max_copy;
   Colormap		defaultCM;
   XColor		defaultColors[256];
   HKEY hkey;

   defaultCM_max_copy = 128;
   COLOR_max = 256;

   if(!RegOpenKeyA(HKEY_LOCAL_MACHINE, "Software\\Wine\\Wine\\Config\\x11drv", &hkey))
   {
	char buffer[20];
	DWORD type, count;

	count = sizeof(buffer);
	if(!RegQueryValueExA(hkey, "CopyDefaultColors", 0, &type, buffer, &count))
	    defaultCM_max_copy = atoi(buffer);

	count = sizeof(buffer);
	if(!RegQueryValueExA(hkey, "AllocSystemColors", 0, &type, buffer, &count))
	    COLOR_max = atoi(buffer);

	RegCloseKey(hkey);
   }

   /* Copy the first bunch of colors out of the default colormap to prevent
    * colormap flashing as much as possible.  We're likely to get the most
    * important Window Manager colors, etc in the first 128 colors */
   defaultCM = DefaultColormap( gdi_display, DefaultScreen(gdi_display) );

   for (i = 0; i < defaultCM_max_copy; i++)
       defaultColors[i].pixel = (long) i;
   wine_tsx11_lock();
   XQueryColors(gdi_display, defaultCM, &defaultColors[0], defaultCM_max_copy);
   for (i = 0; i < defaultCM_max_copy; i++)
       XAllocColor( gdi_display, X11DRV_PALETTE_PaletteXColormap, &defaultColors[i] );

   if (COLOR_max > 256) COLOR_max = 256;
   else if (COLOR_max < 20) COLOR_max = 20;
   TRACE("%d colors configured.\n", COLOR_max);

   TRACE("Building shared map - %i palette entries\n", palette_size);

   /* Be nice and allocate system colors as read-only */

   for( i = 0; i < NB_RESERVED_COLORS; i++ )
     {
        color.red   = sys_pal_template[i].peRed * 65535 / 255;
        color.green = sys_pal_template[i].peGreen * 65535 / 255;
        color.blue  = sys_pal_template[i].peBlue * 65535 / 255;
        color.flags = DoRed | DoGreen | DoBlue;

        if (!XAllocColor( gdi_display, X11DRV_PALETTE_PaletteXColormap, &color ))
        {
	     XColor	best, c;

             if( !warn++ )
	     {
		  WARN("Not enough colors for the full system palette.\n");

	          bp = BlackPixel(gdi_display, DefaultScreen(gdi_display));
	          wp = WhitePixel(gdi_display, DefaultScreen(gdi_display));

	          max = (0xffffffff)>>(32 - screen_depth);
	          if( max > 256 )
	          {
	  	      step = max/256;
		      max = 256;
	          }
	     }

	     /* reinit color (XAllocColor() may change it)
	      * and map to the best shared colorcell */

             color.red   = sys_pal_template[i].peRed * 65535 / 255;
             color.green = sys_pal_template[i].peGreen * 65535 / 255;
             color.blue  = sys_pal_template[i].peBlue * 65535 / 255;

	     best.pixel = best.red = best.green = best.blue = 0;
	     for( c.pixel = 0, diff = 0x7fffffff; c.pixel < max; c.pixel += step )
	     {
		XQueryColor(gdi_display, X11DRV_PALETTE_PaletteXColormap, &c);
		r = (c.red - color.red)>>8;
		g = (c.green - color.green)>>8;
		b = (c.blue - color.blue)>>8;
		r = r*r + g*g + b*b;
		if( r < diff ) { best = c; diff = r; }
	     }

	     if( XAllocColor(gdi_display, X11DRV_PALETTE_PaletteXColormap, &best) )
		 color.pixel = best.pixel;
	     else color.pixel = (i < NB_RESERVED_COLORS/2)? bp : wp;
        }

        sysPixel[i] = color.pixel;

        TRACE("syscolor(%lx) -> pixel %i\n",
		      *(COLORREF*)(sys_pal_template+i), (int)color.pixel);

        /* Set EGA mapping if color in the first or last eight */

        if (i < 8)
            X11DRV_PALETTE_mapEGAPixel[i] = color.pixel;
        else if (i >= NB_RESERVED_COLORS - 8 )
            X11DRV_PALETTE_mapEGAPixel[i - (NB_RESERVED_COLORS-16)] = color.pixel;
     }
   wine_tsx11_unlock();

   /* now allocate changeable set */

   if( !(X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_FIXED) )
     {
	int c_min = 0, c_max = palette_size, c_val;

	TRACE("Dynamic colormap... \n");

	/* let's become the first client that actually follows
	 * X guidelines and does binary search...
	 */

	if((pixDynMapping = (unsigned long*)HeapAlloc(GetProcessHeap(), 0, sizeof(long)*palette_size)) == NULL) {
	    WARN("Out of memory while building system palette.\n");
	    return FALSE;
        }

        wine_tsx11_lock();
	/* comment this out if you want to debug palette init */
	XGrabServer(gdi_display);

        while( c_max - c_min > 0 )
          {
             c_val = (c_max + c_min)/2 + (c_max + c_min)%2;

             if( !XAllocColorCells(gdi_display, X11DRV_PALETTE_PaletteXColormap, False,
                                   plane_masks, 0, pixDynMapping, c_val) )
                 c_max = c_val - 1;
             else
               {
                 XFreeColors(gdi_display, X11DRV_PALETTE_PaletteXColormap, pixDynMapping, c_val, 0);
                 c_min = c_val;
               }
          }

	if( c_min > COLOR_max - NB_RESERVED_COLORS)
	    c_min = COLOR_max - NB_RESERVED_COLORS;

	c_min = (c_min/2) + (c_min/2);		/* need even set for split palette */

	if( c_min > 0 )
	  if( !XAllocColorCells(gdi_display, X11DRV_PALETTE_PaletteXColormap, False,
                                plane_masks, 0, pixDynMapping, c_min) )
	    {
	      WARN("Inexplicable failure during colorcell allocation.\n");
	      c_min = 0;
	    }

        palette_size = c_min + NB_RESERVED_COLORS;

	XUngrabServer(gdi_display);
        wine_tsx11_unlock();

	TRACE("adjusted size %i colorcells\n", palette_size);
     }
   else if( X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_VIRTUAL )
	{
          /* virtual colorspace - ToPhysical takes care of
           * color translations but we have to allocate full palette
	   * to maintain compatibility
	   */
	  palette_size = 256;
	  TRACE("Virtual colorspace - screendepth %i\n", screen_depth);
	}
   else palette_size = NB_RESERVED_COLORS;	/* system palette only - however we can alloc a bunch
			                 * of colors and map to them */

   TRACE("Shared system palette uses %i colors.\n", palette_size);

   /* set gap to account for pixel shortage. It has to be right in the center
    * of the system palette because otherwise raster ops get screwed. */

   if( palette_size >= 256 )
     { COLOR_gapStart = 256; COLOR_gapEnd = -1; }
   else
     { COLOR_gapStart = palette_size/2; COLOR_gapEnd = 255 - palette_size/2; }

   X11DRV_PALETTE_firstFree = ( palette_size > NB_RESERVED_COLORS &&
		      (X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_VIRTUAL || !(X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_FIXED)) )
		     ? NB_RESERVED_COLORS/2 : -1;

   COLOR_sysPal = (PALETTEENTRY*)HeapAlloc(GetProcessHeap(),0,sizeof(PALETTEENTRY)*256);
   if(COLOR_sysPal == NULL) {
       ERR("Can not allocate system palette!\n");
       return FALSE;
   }

   /* setup system palette entry <-> pixel mappings and fill in 20 fixed entries */

   if (screen_depth <= 8)
   {
       X11DRV_PALETTE_XPixelToPalette = HeapAlloc( GetProcessHeap(), 0, 256 * sizeof(int) );
       if(X11DRV_PALETTE_XPixelToPalette == NULL) {
           ERR("Out of memory: XPixelToPalette!\n");
           return FALSE;
       }
       for( i = 0; i < 256; i++ )
           X11DRV_PALETTE_XPixelToPalette[i] = NB_PALETTE_EMPTY_VALUE;
   }

   /* for hicolor visuals PaletteToPixel mapping is used to skip
    * RGB->pixel calculation in X11DRV_PALETTE_ToPhysical().
    */

   X11DRV_PALETTE_PaletteToXPixel = (int*)HeapAlloc(GetProcessHeap(),0,sizeof(int)*256);
   if(X11DRV_PALETTE_PaletteToXPixel == NULL) {
       ERR("Out of memory: PaletteToXPixel!\n");
       return FALSE;
   }

   for( i = j = 0; i < 256; i++ )
   {
      if( i >= COLOR_gapStart && i <= COLOR_gapEnd )
      {
         X11DRV_PALETTE_PaletteToXPixel[i] = NB_PALETTE_EMPTY_VALUE;
         COLOR_sysPal[i].peFlags = 0;	/* mark as unused */
         continue;
      }

      if( i < NB_RESERVED_COLORS/2 )
      {
        X11DRV_PALETTE_PaletteToXPixel[i] = sysPixel[i];
        COLOR_sysPal[i] = sys_pal_template[i];
        COLOR_sysPal[i].peFlags |= PC_SYS_USED;
      }
      else if( i >= 256 - NB_RESERVED_COLORS/2 )
      {
        X11DRV_PALETTE_PaletteToXPixel[i] = sysPixel[(i + NB_RESERVED_COLORS) - 256];
        COLOR_sysPal[i] = sys_pal_template[(i + NB_RESERVED_COLORS) - 256];
        COLOR_sysPal[i].peFlags |= PC_SYS_USED;
      }
      else if( pixDynMapping )
             X11DRV_PALETTE_PaletteToXPixel[i] = pixDynMapping[j++];
           else
             X11DRV_PALETTE_PaletteToXPixel[i] = i;

      TRACE("index %i -> pixel %i\n", i, X11DRV_PALETTE_PaletteToXPixel[i]);

      if( X11DRV_PALETTE_XPixelToPalette )
          X11DRV_PALETTE_XPixelToPalette[X11DRV_PALETTE_PaletteToXPixel[i]] = i;
   }

   if( pixDynMapping ) HeapFree(GetProcessHeap(), 0, pixDynMapping);

   return TRUE;
}

/***********************************************************************
 *      Colormap Initialization
 */
static void X11DRV_PALETTE_FillDefaultColors( const PALETTEENTRY *sys_pal_template )
{
 /* initialize unused entries to what Windows uses as a color
  * cube - based on Greg Kreider's code.
  */

  int i = 0, idx = 0;
  int red, no_r, inc_r;
  int green, no_g, inc_g;
  int blue, no_b, inc_b;

  if (palette_size <= NB_RESERVED_COLORS)
  	return;
  while (i*i*i < (palette_size - NB_RESERVED_COLORS)) i++;
  no_r = no_g = no_b = --i;
  if ((no_r * (no_g+1) * no_b) < (palette_size - NB_RESERVED_COLORS)) no_g++;
  if ((no_r * no_g * (no_b+1)) < (palette_size - NB_RESERVED_COLORS)) no_b++;
  inc_r = (255 - NB_COLORCUBE_START_INDEX)/no_r;
  inc_g = (255 - NB_COLORCUBE_START_INDEX)/no_g;
  inc_b = (255 - NB_COLORCUBE_START_INDEX)/no_b;

  wine_tsx11_lock();

  idx = X11DRV_PALETTE_firstFree;

  if( idx != -1 )
    for (blue = NB_COLORCUBE_START_INDEX; blue < 256 && idx; blue += inc_b )
     for (green = NB_COLORCUBE_START_INDEX; green < 256 && idx; green += inc_g )
      for (red = NB_COLORCUBE_START_INDEX; red < 256 && idx; red += inc_r )
      {
	 /* weird but true */

	 if( red == NB_COLORCUBE_START_INDEX && green == red && blue == green ) continue;

         COLOR_sysPal[idx].peRed = red;
         COLOR_sysPal[idx].peGreen = green;
         COLOR_sysPal[idx].peBlue = blue;

	 /* set X color */

	 if( X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_VIRTUAL )
	 {
            if (X11DRV_PALETTE_PRed.max != 255) no_r = (red * X11DRV_PALETTE_PRed.max) / 255;
            if (X11DRV_PALETTE_PGreen.max != 255) no_g = (green * X11DRV_PALETTE_PGreen.max) / 255;
            if (X11DRV_PALETTE_PBlue.max != 255) no_b = (blue * X11DRV_PALETTE_PBlue.max) / 255;

            X11DRV_PALETTE_PaletteToXPixel[idx] = (no_r << X11DRV_PALETTE_PRed.shift) | (no_g << X11DRV_PALETTE_PGreen.shift) | (no_b << X11DRV_PALETTE_PBlue.shift);
	 }
	 else if( !(X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_FIXED) )
	 {
	   XColor color;
	   color.pixel = (X11DRV_PALETTE_PaletteToXPixel)? X11DRV_PALETTE_PaletteToXPixel[idx] : idx;
	   color.red = COLOR_sysPal[idx].peRed << 8;
	   color.green = COLOR_sysPal[idx].peGreen << 8;
	   color.blue =  COLOR_sysPal[idx].peBlue << 8;
	   color.flags = DoRed | DoGreen | DoBlue;
	   XStoreColor(gdi_display, X11DRV_PALETTE_PaletteXColormap, &color);
	 }
	 idx = X11DRV_PALETTE_freeList[idx];
      }

  /* try to fill some entries in the "gap" with
   * what's already in the colormap - they will be
   * mappable to but not changeable. */

  if( COLOR_gapStart < COLOR_gapEnd && X11DRV_PALETTE_XPixelToPalette )
  {
    XColor	xc;
    int		r, g, b, max;

    max = COLOR_max - (256 - (COLOR_gapEnd - COLOR_gapStart));
    for ( i = 0, idx = COLOR_gapStart; i < 256 && idx <= COLOR_gapEnd; i++ )
      if( X11DRV_PALETTE_XPixelToPalette[i] == NB_PALETTE_EMPTY_VALUE )
	{
	  xc.pixel = i;

	  XQueryColor(gdi_display, X11DRV_PALETTE_PaletteXColormap, &xc);
	  r = xc.red>>8; g = xc.green>>8; b = xc.blue>>8;

	  if( xc.pixel < 256 && X11DRV_PALETTE_CheckSysColor( sys_pal_template, RGB(r, g, b)) &&
	      XAllocColor(gdi_display, X11DRV_PALETTE_PaletteXColormap, &xc) )
	  {
	     X11DRV_PALETTE_XPixelToPalette[xc.pixel] = idx;
	     X11DRV_PALETTE_PaletteToXPixel[idx] = xc.pixel;
           *(COLORREF*)(COLOR_sysPal + idx) = RGB(r, g, b);
	     COLOR_sysPal[idx++].peFlags |= PC_SYS_USED;
	     if( --max <= 0 ) break;
	  }
	}
    COLOR_gapFilled = idx - COLOR_gapStart;
  }
  wine_tsx11_unlock();
}


/***********************************************************************
 *           X11DRV_IsSolidColor
 *
 * Check whether 'color' can be represented with a solid color.
 */
BOOL X11DRV_IsSolidColor( COLORREF color )
{
    int i;
    const PALETTEENTRY *pEntry = COLOR_sysPal;

    if (color & 0xff000000) return TRUE;  /* indexed color */

    if (!color || (color == 0xffffff)) return TRUE;  /* black or white */

    if (X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_VIRTUAL) return TRUE;  /* no palette */

    for (i = 0; i < palette_size ; i++, pEntry++)
    {
        if( i < COLOR_gapStart || i > COLOR_gapEnd )
            if ((GetRValue(color) == pEntry->peRed) &&
                (GetGValue(color) == pEntry->peGreen) &&
                (GetBValue(color) == pEntry->peBlue)) return TRUE;
    }
    return FALSE;
}


/***********************************************************************
 *           X11DRV_PALETTE_ToLogical
 *
 * Return RGB color for given X pixel.
 */
COLORREF X11DRV_PALETTE_ToLogical(int pixel)
{
    XColor color;

#if 0
    /* truecolor visual */

    if (screen_depth >= 24) return pixel;
#endif

    /* check for hicolor visuals first */

    if ( (X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_FIXED) && !X11DRV_PALETTE_Graymax )
    {
         color.red = (pixel >> X11DRV_PALETTE_LRed.shift) & X11DRV_PALETTE_LRed.max;
         if (X11DRV_PALETTE_LRed.scale<8)
             color.red=  color.red   << (8-X11DRV_PALETTE_LRed.scale) |
                         color.red   >> (2*X11DRV_PALETTE_LRed.scale-8);
         color.green = (pixel >> X11DRV_PALETTE_LGreen.shift) & X11DRV_PALETTE_LGreen.max;
         if (X11DRV_PALETTE_LGreen.scale<8)
             color.green=color.green << (8-X11DRV_PALETTE_LGreen.scale) |
                         color.green >> (2*X11DRV_PALETTE_LGreen.scale-8);
         color.blue = (pixel >> X11DRV_PALETTE_LBlue.shift) & X11DRV_PALETTE_LBlue.max;
         if (X11DRV_PALETTE_LBlue.scale<8)
             color.blue= color.blue  << (8-X11DRV_PALETTE_LBlue.scale)  |
                         color.blue  >> (2*X11DRV_PALETTE_LBlue.scale-8);
                 return RGB(color.red,color.green,color.blue);
    }

    /* check if we can bypass X */

    if ((screen_depth <= 8) && (pixel < 256) &&
        !(X11DRV_PALETTE_PaletteFlags & (X11DRV_PALETTE_VIRTUAL | X11DRV_PALETTE_FIXED)) ) {
         return  ( *(COLORREF*)(COLOR_sysPal +
		   ((X11DRV_PALETTE_XPixelToPalette)?X11DRV_PALETTE_XPixelToPalette[pixel]:pixel)) ) & 0x00ffffff;
    }

    wine_tsx11_lock();
    color.pixel = pixel;
    XQueryColor(gdi_display, X11DRV_PALETTE_PaletteXColormap, &color);
    wine_tsx11_unlock();
    return RGB(color.red >> 8, color.green >> 8, color.blue >> 8);
}


/***********************************************************************
 *	     X11DRV_SysPaletteLookupPixel
 */
static int X11DRV_SysPaletteLookupPixel( COLORREF col, BOOL skipReserved )
{
    int i, best = 0, diff = 0x7fffffff;
    int r,g,b;

    for( i = 0; i < palette_size && diff ; i++ )
    {
        if( !(COLOR_sysPal[i].peFlags & PC_SYS_USED) ||
            (skipReserved && COLOR_sysPal[i].peFlags  & PC_SYS_RESERVED) )
            continue;

        r = COLOR_sysPal[i].peRed - GetRValue(col);
        g = COLOR_sysPal[i].peGreen - GetGValue(col);
        b = COLOR_sysPal[i].peBlue - GetBValue(col);

        r = r*r + g*g + b*b;

        if( r < diff ) { best = i; diff = r; }
    }
    return best;
}


/***********************************************************************
 *           X11DRV_PALETTE_ToPhysical
 *
 * Return the physical color closest to 'color'.
 */
int X11DRV_PALETTE_ToPhysical( X11DRV_PDEVICE *physDev, COLORREF color )
{
    WORD 		 index = 0;
    HPALETTE hPal = physDev ? GetCurrentObject(physDev->hdc, OBJ_PAL ) : GetStockObject(DEFAULT_PALETTE);
    unsigned char	 spec_type = color >> 24;
    PALETTEOBJ* 	 palPtr = (PALETTEOBJ *) GDI_GetObjPtr( hPal, PALETTE_MAGIC );

    /* palPtr can be NULL when DC is being destroyed */
    if( !palPtr ) return 0;

    if ( X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_FIXED )
    {
        /* there is no colormap limitation; we are going to have to compute
         * the pixel value from the visual information stored earlier
	 */

	unsigned 	long red, green, blue;
	unsigned 	idx = 0;

	switch(spec_type)
        {
          case 1: /* PALETTEINDEX */

            if( (idx = color & 0xffff) >= palPtr->logpalette.palNumEntries)
            {
                WARN("RGB(%lx) : idx %d is out of bounds, assuming black\n", color, idx);
		GDI_ReleaseObj( hPal );
                return 0;
            }

            if( palPtr->mapping )
	    {
                int ret = palPtr->mapping[idx];
		GDI_ReleaseObj( hPal );
		return ret;
	    }
	    color = *(COLORREF*)(palPtr->logpalette.palPalEntry + idx);
	    break;

	  default:
	    color &= 0xffffff;
	    /* fall through to RGB */

	  case 0: /* RGB */
	    if (physDev && (physDev->depth == 1) )
	    {
		GDI_ReleaseObj( hPal );
		return (((color >> 16) & 0xff) +
			((color >> 8) & 0xff) + (color & 0xff) > 255*3/2) ? 1 : 0;
	    }

	}

        red = GetRValue(color); green = GetGValue(color); blue = GetBValue(color);

	if (X11DRV_PALETTE_Graymax)
        {
	    /* grayscale only; return scaled value */
	    GDI_ReleaseObj( hPal );
            return ( (red * 30 + green * 59 + blue * 11) * X11DRV_PALETTE_Graymax) / 25500;
	}
	else
        {
	    /* scale each individually and construct the TrueColor pixel value */
	    if (X11DRV_PALETTE_PRed.scale < 8)
		red = red >> (8-X11DRV_PALETTE_PRed.scale);
	    else if (X11DRV_PALETTE_PRed.scale > 8)
		red =   red   << (X11DRV_PALETTE_PRed.scale-8) |
                        red   >> (16-X11DRV_PALETTE_PRed.scale);
	    if (X11DRV_PALETTE_PGreen.scale < 8)
		green = green >> (8-X11DRV_PALETTE_PGreen.scale);
	    else if (X11DRV_PALETTE_PGreen.scale > 8)
		green = green << (X11DRV_PALETTE_PGreen.scale-8) |
                        green >> (16-X11DRV_PALETTE_PGreen.scale);
	    if (X11DRV_PALETTE_PBlue.scale < 8)
		blue =  blue  >> (8-X11DRV_PALETTE_PBlue.scale);
	    else if (X11DRV_PALETTE_PBlue.scale > 8)
		blue =  blue  << (X11DRV_PALETTE_PBlue.scale-8) |
                        blue  >> (16-X11DRV_PALETTE_PBlue.scale);

	    GDI_ReleaseObj( hPal );
            return (red << X11DRV_PALETTE_PRed.shift) | (green << X11DRV_PALETTE_PGreen.shift) | (blue << X11DRV_PALETTE_PBlue.shift);
        }
    }
    else
    {

	if( !palPtr->mapping )
            WARN("Palette %p is not realized\n", hPal);

	switch(spec_type)	/* we have to peruse DC and system palette */
    	{
	    default:
		color &= 0xffffff;
		/* fall through to RGB */

       	    case 0:  /* RGB */
		if (physDev && (physDev->depth == 1) )
		{
		    GDI_ReleaseObj( hPal );
		    return (((color >> 16) & 0xff) +
			    ((color >> 8) & 0xff) + (color & 0xff) > 255*3/2) ? 1 : 0;
		}

	    	index = X11DRV_SysPaletteLookupPixel( color, FALSE);
                if (X11DRV_PALETTE_PaletteToXPixel) index = X11DRV_PALETTE_PaletteToXPixel[index];

		/* TRACE(palette,"RGB(%lx) -> pixel %i\n", color, index);
		 */
	    	break;
       	    case 1:  /* PALETTEINDEX */
		index = color & 0xffff;

	        if( index >= palPtr->logpalette.palNumEntries )
		    WARN("RGB(%lx) : index %i is out of bounds\n", color, index);
		else if( palPtr->mapping ) index = palPtr->mapping[index];

		/*  TRACE(palette,"PALETTEINDEX(%04x) -> pixel %i\n", (WORD)color, index);
		 */
		break;
            case 2:  /* PALETTERGB */
                index = GetNearestPaletteIndex( hPal, color );
                if (palPtr->mapping) index = palPtr->mapping[index];
		/* TRACE(palette,"PALETTERGB(%lx) -> pixel %i\n", color, index);
		 */
		break;
	}
    }

    GDI_ReleaseObj( hPal );
    return index;
}

/***********************************************************************
 *           X11DRV_PALETTE_LookupSystemXPixel
 */
static int X11DRV_PALETTE_LookupSystemXPixel(COLORREF col)
{
 int            i, best = 0, diff = 0x7fffffff;
 int            size = palette_size;
 int            r,g,b;

 for( i = 0; i < size && diff ; i++ )
    {
      if( i == NB_RESERVED_COLORS/2 )
      {
      	int newi = size - NB_RESERVED_COLORS/2;
	if (newi>i) i=newi;
      }

      r = COLOR_sysPal[i].peRed - GetRValue(col);
      g = COLOR_sysPal[i].peGreen - GetGValue(col);
      b = COLOR_sysPal[i].peBlue - GetBValue(col);

      r = r*r + g*g + b*b;

      if( r < diff ) { best = i; diff = r; }
    }

 return (X11DRV_PALETTE_PaletteToXPixel)? X11DRV_PALETTE_PaletteToXPixel[best] : best;
}

/***********************************************************************
 *           X11DRV_PALETTE_FormatSystemPalette
 */
static void X11DRV_PALETTE_FormatSystemPalette(void)
{
 /* Build free list so we'd have an easy way to find
  * out if there are any available colorcells.
  */

  int i, j = X11DRV_PALETTE_firstFree = NB_RESERVED_COLORS/2;

  COLOR_sysPal[j].peFlags = 0;
  for( i = NB_RESERVED_COLORS/2 + 1 ; i < 256 - NB_RESERVED_COLORS/2 ; i++ )
    if( i < COLOR_gapStart || i > COLOR_gapEnd )
      {
	COLOR_sysPal[i].peFlags = 0;  /* unused tag */
	X11DRV_PALETTE_freeList[j] = i;	  /* next */
        j = i;
      }
  X11DRV_PALETTE_freeList[j] = 0;
}

/***********************************************************************
 *           X11DRV_PALETTE_CheckSysColor
 */
static BOOL X11DRV_PALETTE_CheckSysColor( const PALETTEENTRY *sys_pal_template, COLORREF c)
{
  int i;
  for( i = 0; i < NB_RESERVED_COLORS; i++ )
       if( c == (*(COLORREF*)(sys_pal_template + i) & 0x00ffffff) )
	   return 0;
  return 1;
}


/***********************************************************************
 *	     X11DRV_LookupSysPaletteExact
 */
static int X11DRV_LookupSysPaletteExact( COLORREF col )
{
    int i;
    BYTE r = GetRValue(col), g = GetGValue(col), b = GetBValue(col);
    for( i = 0; i < palette_size; i++ )
    {
        if( COLOR_sysPal[i].peFlags & PC_SYS_USED )  /* skips gap */
            if( COLOR_sysPal[i].peRed == r &&
                COLOR_sysPal[i].peGreen == g &&
                COLOR_sysPal[i].peBlue == b )
                return i;
    }
    return -1;
}


/***********************************************************************
 *           X11DRV_PALETTE_SetMapping
 *
 * Set the color-mapping table for selected palette.
 * Return number of entries which mapping has changed.
 */
static UINT X11DRV_PALETTE_SetMapping( PALETTEOBJ* palPtr, UINT uStart, UINT uNum, BOOL mapOnly )
{
    char flag;
    int  prevMapping = (palPtr->mapping) ? 1 : 0;
    int  index;
    UINT iRemapped = 0;
    int* mapping;

    /* reset dynamic system palette entries */

    if( !mapOnly && X11DRV_PALETTE_firstFree != -1)
         X11DRV_PALETTE_FormatSystemPalette();

    /* initialize palette mapping table */
    if (palPtr->mapping) 
	mapping = HeapReAlloc( GetProcessHeap(), 0, palPtr->mapping,
                           sizeof(int)*palPtr->logpalette.palNumEntries);
    else 
	mapping = HeapAlloc( GetProcessHeap(), 0, 
                           sizeof(int)*palPtr->logpalette.palNumEntries);

    if(mapping == NULL) {
        ERR("Can not allocate new mapping -- memory exausted!\n");
        return 0;
    }
    palPtr->mapping = mapping;

    if (uStart >= palPtr->logpalette.palNumEntries) return 0;

    if (uStart + uNum > palPtr->logpalette.palNumEntries)
        uNum = palPtr->logpalette.palNumEntries - uStart;

    for( uNum += uStart; uStart < uNum; uStart++ )
    {
        index = -1;
        flag = PC_SYS_USED;

        switch( palPtr->logpalette.palPalEntry[uStart].peFlags & 0x07 )
        {
	case PC_EXPLICIT:   /* palette entries are indices into system palette */
            index = *(WORD*)(palPtr->logpalette.palPalEntry + uStart);
            if( index > 255 || (index >= COLOR_gapStart && index <= COLOR_gapEnd) )
            {
                WARN("PC_EXPLICIT: idx %d out of system palette, assuming black.\n", index);
                index = 0;
            }
            break;

	case PC_RESERVED:   /* forbid future mappings to this entry */
            flag |= PC_SYS_RESERVED;

            /* fall through */
	default:	    /* try to collapse identical colors */
            index = X11DRV_LookupSysPaletteExact(*(COLORREF*)(palPtr->logpalette.palPalEntry + uStart));
            /* fall through */
	case PC_NOCOLLAPSE:
            if( index < 0 )
            {
                if( X11DRV_PALETTE_firstFree > 0 && !(X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_FIXED) )
                {
                    XColor color;
                    index = X11DRV_PALETTE_firstFree;  /* ought to be available */
                    X11DRV_PALETTE_firstFree = X11DRV_PALETTE_freeList[index];

                    color.pixel = (X11DRV_PALETTE_PaletteToXPixel) ? X11DRV_PALETTE_PaletteToXPixel[index] : index;
                    color.red = palPtr->logpalette.palPalEntry[uStart].peRed << 8;
                    color.green = palPtr->logpalette.palPalEntry[uStart].peGreen << 8;
                    color.blue = palPtr->logpalette.palPalEntry[uStart].peBlue << 8;
                    color.flags = DoRed | DoGreen | DoBlue;
                    wine_tsx11_lock();
                    XStoreColor(gdi_display, X11DRV_PALETTE_PaletteXColormap, &color);
                    wine_tsx11_unlock();

                    COLOR_sysPal[index] = palPtr->logpalette.palPalEntry[uStart];
                    COLOR_sysPal[index].peFlags = flag;
		    X11DRV_PALETTE_freeList[index] = 0;

                    if( X11DRV_PALETTE_PaletteToXPixel ) index = X11DRV_PALETTE_PaletteToXPixel[index];
                    break;
                }
                else if ( X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_VIRTUAL )
                {
                    index = X11DRV_PALETTE_ToPhysical( NULL, 0x00ffffff &
                             *(COLORREF*)(palPtr->logpalette.palPalEntry + uStart));
                    break;
                }

                /* we have to map to existing entry in the system palette */

                index = X11DRV_SysPaletteLookupPixel( *(COLORREF*)(palPtr->logpalette.palPalEntry + uStart), TRUE);
            }
	    palPtr->logpalette.palPalEntry[uStart].peFlags |= PC_SYS_USED;

            if( X11DRV_PALETTE_PaletteToXPixel ) index = X11DRV_PALETTE_PaletteToXPixel[index];
            break;
        }

        if( !prevMapping || palPtr->mapping[uStart] != index ) iRemapped++;
        palPtr->mapping[uStart] = index;

        TRACE("entry %i (%lx) -> pixel %i\n", uStart,
				*(COLORREF*)(palPtr->logpalette.palPalEntry + uStart), index);

    }
    return iRemapped;
}

/***********************************************************************
 *              GetSystemPaletteEntries   (X11DRV.@)
 */
UINT X11DRV_GetSystemPaletteEntries( X11DRV_PDEVICE *physDev, UINT start, UINT count,
                                     LPPALETTEENTRY entries )
{
    UINT i;

    if (!entries) return palette_size;
    if (start >= palette_size) return 0;
    if (start + count >= palette_size) count = palette_size - start;

    for (i = 0; i < count; i++)
    {
        entries[i].peRed   = COLOR_sysPal[start + i].peRed;
        entries[i].peGreen = COLOR_sysPal[start + i].peGreen;
        entries[i].peBlue  = COLOR_sysPal[start + i].peBlue;
        entries[i].peFlags = 0;
        TRACE("\tidx(%02x) -> RGB(%08lx)\n", start + i, *(COLORREF*)(entries + i) );
    }
    return count;
}


/***********************************************************************
 *              GetNearestColor   (X11DRV.@)
 */
COLORREF X11DRV_GetNearestColor( X11DRV_PDEVICE *physDev, COLORREF color )
{
    unsigned char spec_type = color >> 24;
    COLORREF nearest;

    if (!palette_size) return color;

    if (spec_type == 1 || spec_type == 2)
    {
        /* we need logical palette for PALETTERGB and PALETTEINDEX colorrefs */

        UINT index;
        PALETTEENTRY entry;
        HPALETTE hpal = GetCurrentObject( physDev->hdc, OBJ_PAL );

        if (!hpal) hpal = GetStockObject( DEFAULT_PALETTE );

        if (spec_type == 2) /* PALETTERGB */
            index = GetNearestPaletteIndex( hpal, color );
        else  /* PALETTEINDEX */
            index = LOWORD(color);

        if (!GetPaletteEntries( hpal, index, 1, &entry ))
        {
            WARN("RGB(%lx) : idx %d is out of bounds, assuming NULL\n", color, index );
            if (!GetPaletteEntries( hpal, 0, 1, &entry )) return CLR_INVALID;
        }
        color = RGB( entry.peRed,  entry.peGreen, entry.peBlue );
    }
    color &= 0x00ffffff;
    nearest = (0x00ffffff & *(COLORREF*)(COLOR_sysPal + X11DRV_SysPaletteLookupPixel(color, FALSE)));

    TRACE("(%06lx): returning %06lx\n", color, nearest );
    return nearest;
}


/***********************************************************************
 *              RealizePalette    (X11DRV.@)
 */
UINT X11DRV_RealizePalette( X11DRV_PDEVICE *physDev, HPALETTE hpal, BOOL primary )
{
    UINT ret;
    PALETTEOBJ *palPtr;

    if (X11DRV_PALETTE_PaletteFlags & X11DRV_PALETTE_VIRTUAL) return 0;

    if (!(palPtr = GDI_GetObjPtr( hpal, PALETTE_MAGIC ))) return 0;
    ret = X11DRV_PALETTE_SetMapping( palPtr, 0, palPtr->logpalette.palNumEntries, !primary );
    GDI_ReleaseObj( hpal );
    return ret;
}


/***********************************************************************
 *              RealizeDefaultPalette    (X11DRV.@)
 */
UINT X11DRV_RealizeDefaultPalette( X11DRV_PDEVICE *physDev )
{
    UINT ret = 0;

    if (palette_size && GetObjectType(physDev->hdc) != OBJ_MEMDC)
    {
        PALETTEOBJ*  palPtr = GDI_GetObjPtr( GetStockObject(DEFAULT_PALETTE), PALETTE_MAGIC );
        if (palPtr)
        {
            /* lookup is needed to account for SetSystemPaletteUse() stuff */
            int i, index;

            for( i = 0; i < 20; i++ )
            {
                index = X11DRV_PALETTE_LookupSystemXPixel(*(COLORREF*)(palPtr->logpalette.palPalEntry + i));
                /* mapping is allocated in COLOR_InitPalette() */
                if( index != palPtr->mapping[i] )
                {
                    palPtr->mapping[i]=index;
                    ret++;
                }
            }
            GDI_ReleaseObj( GetStockObject(DEFAULT_PALETTE) );
        }
    }
    return ret;
}
