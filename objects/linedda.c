/*
 * LineDDA
 *
 * Copyright 1993 Bob Amstadt
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
#include <stdlib.h>
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"

/**********************************************************************
 *           LineDDA   (GDI32.@)
 */
BOOL WINAPI LineDDA(INT nXStart, INT nYStart, INT nXEnd, INT nYEnd,
                        LINEDDAPROC callback, LPARAM lParam )
{
    INT xadd = 1, yadd = 1;
    INT err,erradd;
    INT cnt;
    INT dx = nXEnd - nXStart;
    INT dy = nYEnd - nYStart;

    if (dx < 0)  {
      dx = -dx; xadd = -1;
    }
    if (dy < 0)  {
      dy = -dy; yadd = -1;
    }
    if (dx > dy) { /* line is "more horizontal" */
      err = 2*dy - dx; erradd = 2*dy - 2*dx;
      for(cnt = 0;cnt <= dx; cnt++) {
        callback(nXStart,nYStart,lParam);
	if (err > 0) {
	  nYStart += yadd;
	  err += erradd;
	} else  {
	  err += 2*dy;
	}
	nXStart += xadd;
      }
    } else  { /* line is "more vertical" */
      err = 2*dx - dy; erradd = 2*dx - 2*dy;
      for(cnt = 0;cnt <= dy; cnt++) {
	callback(nXStart,nYStart,lParam);
	if (err > 0) {
	  nXStart += xadd;
	  err += erradd;
	} else  {
	  err += 2*dx;
	}
	nYStart += yadd;
      }
    }
    return TRUE;
}
