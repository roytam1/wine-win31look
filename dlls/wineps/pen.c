/*
 *	PostScript pen handling
 *
 *	Copyright 1998  Huw D M Davies
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "psdrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(psdrv);

static char PEN_dash[]       = "50 30";     /* -----   -----   -----  */
static char PEN_dot[]        = "20";      /* --  --  --  --  --  -- */
static char PEN_dashdot[]    = "40 30 20 30";  /* ----   --   ----   --  */
static char PEN_dashdotdot[] = "40 20 20 20 20 20"; /* ----  --  --  ----  */
static char PEN_alternate[]  = "1";

/***********************************************************************
 *           SelectPen   (WINEPS.@)
 */
HPEN PSDRV_SelectPen( PSDRV_PDEVICE *physDev, HPEN hpen )
{
    LOGPEN logpen;

    if (!GetObjectA( hpen, sizeof(logpen), &logpen )) return 0;

    TRACE("hpen = %p colour = %08lx\n", hpen, logpen.lopnColor);

    physDev->pen.width = PSDRV_XWStoDS(physDev, logpen.lopnWidth.x);
    if(physDev->pen.width < 0)
        physDev->pen.width = -physDev->pen.width;

    PSDRV_CreateColor(physDev, &physDev->pen.color, logpen.lopnColor);
    physDev->pen.style = logpen.lopnStyle & PS_STYLE_MASK;

    switch(physDev->pen.style) {
    case PS_DASH:
	physDev->pen.dash = PEN_dash;
	break;

    case PS_DOT:
	physDev->pen.dash = PEN_dot;
	break;

    case PS_DASHDOT:
	physDev->pen.dash = PEN_dashdot;
	break;

    case PS_DASHDOTDOT:
	physDev->pen.dash = PEN_dashdotdot;
	break;

    case PS_ALTERNATE:
	physDev->pen.dash = PEN_alternate;
	break;

    default:
	physDev->pen.dash = NULL;
    }

    if ((physDev->pen.width > 1) && (physDev->pen.dash != NULL)) {
	physDev->pen.style = PS_SOLID;
         physDev->pen.dash = NULL;
    }

    physDev->pen.set = FALSE;
    return hpen;
}


/**********************************************************************
 *
 *	PSDRV_SetPen
 *
 */
BOOL PSDRV_SetPen(PSDRV_PDEVICE *physDev)
{
    if (physDev->pen.style != PS_NULL) {
	PSDRV_WriteSetColor(physDev, &physDev->pen.color);

	if(!physDev->pen.set) {
	    PSDRV_WriteSetPen(physDev);
	    physDev->pen.set = TRUE;
	}
    }

    return TRUE;
}
