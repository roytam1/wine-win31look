/*
 * OLEFONT test program
 *
 * Copyright 2003 Marcus Meissner
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <time.h>

#include <wine/test.h>
#include <windef.h>
#include <winbase.h>
#include <winuser.h>
#include <wingdi.h>
#include <winnls.h>
#include <winerror.h>
#include <winnt.h>

#include <wtypes.h>
#include <olectl.h>

static HMODULE hOleaut32;

static HRESULT (WINAPI *pOleCreateFontIndirect)(LPFONTDESC,REFIID,LPVOID*);

START_TEST(olefont)
{
	LPVOID pvObj = NULL;
	HRESULT hres;
	IFont*	font = NULL;

	hOleaut32 = LoadLibraryA("oleaut32.dll");    
	pOleCreateFontIndirect = (void*)GetProcAddress(hOleaut32, "OleCreateFontIndirect");
	if (!pOleCreateFontIndirect)
	    return;

	hres = pOleCreateFontIndirect(NULL, &IID_IFont, &pvObj);
	font = pvObj;

	ok(hres == S_OK,"OCFI (NULL,..) does not return 0, but 0x%08lx\n",hres);
	ok(font != NULL,"OCFI (NULL,..) returns NULL, instead of !NULL\n");

	pvObj = NULL;
	hres = IFont_QueryInterface( font, &IID_IFont, &pvObj);

	ok(hres == S_OK,"IFont_QI does not return S_OK, but 0x%08lx\n", hres);
	ok(pvObj != NULL,"IFont_QI does return NULL, instead of a ptr\n");
}
