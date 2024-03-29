/*
 *  Clock (winclock.h)
 *
 *  Copyright 1998 by Marcel Baur <mbaur@g26.ethz.ch>
 *  This file is essentially rolex.c by Jim Peterson.
 *  Please see my winclock.c and/or his rolex.c for references.
 *
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

void AnalogClock(HDC dc, int X, int Y, BOOL bSeconds);
HFONT SizeFont(HDC dc, int x, int y, BOOL bSeconds, const LOGFONT* font);
void DigitalClock(HDC dc, int X, int Y, BOOL bSeconds, HFONT font);
