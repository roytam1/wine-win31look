/*
 *	PostScript driver builtin font functions
 *
 *	Copyright 2002  Huw D M Davies for CodeWeavers
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
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "wingdi.h"
#include "winspool.h"

#include "psdrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(psdrv);


/***********************************************************************
 *           is_stock_font
 */
inline static BOOL is_stock_font( HFONT font )
{
    int i;
    for (i = OEM_FIXED_FONT; i <= DEFAULT_GUI_FONT; i++)
    {
        if (i != DEFAULT_PALETTE && font == GetStockObject(i)) return TRUE;
    }
    return FALSE;
}


/*******************************************************************************
 *  ScaleFont
 *
 *  Scale builtin font to requested lfHeight
 *
 */
inline static float Round(float f)
{
    return (f > 0) ? (f + 0.5) : (f - 0.5);
}

static VOID ScaleFont(const AFM *afm, LONG lfHeight, PSFONT *font,
		      TEXTMETRICW *tm)
{
    const WINMETRICS	*wm = &(afm->WinMetrics);
    USHORT  	    	usUnitsPerEm, usWinAscent, usWinDescent;
    SHORT   	    	sAscender, sDescender, sLineGap, sAvgCharWidth;

    TRACE("'%s' %li\n", afm->FontName, lfHeight);

    if (lfHeight < 0)   	    	    	    	/* match em height */
    {
        font->fontinfo.Builtin.scale = - ((float)lfHeight / (float)(wm->usUnitsPerEm));
    }
    else    	    	    	    	    	    	/* match cell height */
    {
    	font->fontinfo.Builtin.scale = (float)lfHeight /
	    	(float)(wm->usWinAscent + wm->usWinDescent);
    }

    font->size = (INT)Round(font->fontinfo.Builtin.scale * (float)wm->usUnitsPerEm);

    usUnitsPerEm = (USHORT)Round((float)(wm->usUnitsPerEm) * font->fontinfo.Builtin.scale);
    sAscender = (SHORT)Round((float)(wm->sAscender) * font->fontinfo.Builtin.scale);
    sDescender = (SHORT)Round((float)(wm->sDescender) * font->fontinfo.Builtin.scale);
    sLineGap = (SHORT)Round((float)(wm->sLineGap) * font->fontinfo.Builtin.scale);
    usWinAscent = (USHORT)Round((float)(wm->usWinAscent) * font->fontinfo.Builtin.scale);
    usWinDescent = (USHORT)Round((float)(wm->usWinDescent) * font->fontinfo.Builtin.scale);
    sAvgCharWidth = (SHORT)Round((float)(wm->sAvgCharWidth) * font->fontinfo.Builtin.scale);

    tm->tmAscent = (LONG)usWinAscent;
    tm->tmDescent = (LONG)usWinDescent;
    tm->tmHeight = tm->tmAscent + tm->tmDescent;

    tm->tmInternalLeading = tm->tmHeight - (LONG)usUnitsPerEm;
    if (tm->tmInternalLeading < 0)
        tm->tmInternalLeading = 0;

    tm->tmExternalLeading =
    	    (LONG)(sAscender - sDescender + sLineGap) - tm->tmHeight;
    if (tm->tmExternalLeading < 0)
    	tm->tmExternalLeading = 0;

    tm->tmAveCharWidth = (LONG)sAvgCharWidth;

    tm->tmWeight = afm->Weight;
    tm->tmItalic = (afm->ItalicAngle != 0.0);
    tm->tmUnderlined = 0;
    tm->tmStruckOut = 0;
    tm->tmFirstChar = (WCHAR)(afm->Metrics[0].UV);
    tm->tmLastChar = (WCHAR)(afm->Metrics[afm->NumofMetrics - 1].UV);
    tm->tmDefaultChar = 0x001f;     	/* Win2K does this - FIXME? */
    tm->tmBreakChar = tm->tmFirstChar;	    	/* should be 'space' */

    tm->tmPitchAndFamily = TMPF_DEVICE | TMPF_VECTOR;
    if (!afm->IsFixedPitch)
    	tm->tmPitchAndFamily |= TMPF_FIXED_PITCH;   /* yes, it's backwards */
    if (wm->usUnitsPerEm != 1000)
    	tm->tmPitchAndFamily |= TMPF_TRUETYPE;

    tm->tmCharSet = ANSI_CHARSET;   	/* FIXME */
    tm->tmOverhang = 0;

    /*
     *	This is kludgy.  font->scale is used in several places in the driver
     *	to adjust PostScript-style metrics.  Since these metrics have been
     *	"normalized" to an em-square size of 1000, font->scale needs to be
     *	similarly adjusted..
     */

    font->fontinfo.Builtin.scale *= (float)wm->usUnitsPerEm / 1000.0;

    tm->tmMaxCharWidth = (LONG)Round(
    	    (afm->FontBBox.urx - afm->FontBBox.llx) * font->fontinfo.Builtin.scale);

    font->underlinePosition = afm->UnderlinePosition * font->fontinfo.Builtin.scale;
    font->underlineThickness = afm->UnderlineThickness * font->fontinfo.Builtin.scale;
    font->strikeoutPosition = tm->tmAscent / 2;
    font->strikeoutThickness = font->underlineThickness;

    TRACE("Selected PS font '%s' size %d weight %ld.\n", afm->FontName,
    	    font->size, tm->tmWeight );
    TRACE("H = %ld As = %ld Des = %ld IL = %ld EL = %ld\n", tm->tmHeight,
    	    tm->tmAscent, tm->tmDescent, tm->tmInternalLeading,
    	    tm->tmExternalLeading);
}


/****************************************************************************
 *  PSDRV_SelectBuiltinFont
 *
 *  Set up physDev->font for a builtin font
 *
 */
BOOL PSDRV_SelectBuiltinFont(PSDRV_PDEVICE *physDev, HFONT hfont,
			     LOGFONTW *plf, LPSTR FaceName)
{
    AFMLISTENTRY *afmle;
    FONTFAMILY *family;
    BOOL bd = FALSE, it = FALSE;
    LONG height;

    TRACE("Trying to find facename '%s'\n", FaceName);

    /* Look for a matching font family */
    for(family = physDev->pi->Fonts; family; family = family->next) {
        if(!strcasecmp(FaceName, family->FamilyName))
	    break;
    }

    if(!family) {
	/* Fallback for Window's font families to common PostScript families */
	if(!strcmp(FaceName, "Arial"))
	    strcpy(FaceName, "Helvetica");
	else if(!strcmp(FaceName, "System"))
	    strcpy(FaceName, "Helvetica");
	else if(!strcmp(FaceName, "Times New Roman"))
	    strcpy(FaceName, "Times");
	else if(!strcmp(FaceName, "Courier New"))
	    strcpy(FaceName, "Courier");

	for(family = physDev->pi->Fonts; family; family = family->next) {
	    if(!strcmp(FaceName, family->FamilyName))
		break;
	}
    }
    /* If all else fails, use the first font defined for the printer */
    if(!family)
        family = physDev->pi->Fonts;

    TRACE("Got family '%s'\n", family->FamilyName);

    if(plf->lfItalic)
        it = TRUE;
    if(plf->lfWeight > 550)
        bd = TRUE;

    for(afmle = family->afmlist; afmle; afmle = afmle->next) {
        if( (bd == (afmle->afm->Weight == FW_BOLD)) &&
	    (it == (afmle->afm->ItalicAngle != 0.0)) )
	        break;
    }
    if(!afmle)
        afmle = family->afmlist; /* not ideal */

    TRACE("Got font '%s'\n", afmle->afm->FontName);

    physDev->font.fontloc = Builtin;
    physDev->font.fontinfo.Builtin.afm = afmle->afm;

    height = plf->lfHeight;
    /* stock fonts ignore the mapping mode */
    if (!is_stock_font( hfont )) {
        POINT pts[2];
	pts[0].x = pts[0].y = pts[1].x = 0;
	pts[1].y = height;
	LPtoDP(physDev->hdc, pts, 2);
	height = pts[1].y - pts[0].y;
    }
    ScaleFont(physDev->font.fontinfo.Builtin.afm, height,
	      &(physDev->font), &(physDev->font.fontinfo.Builtin.tm));


    /* Does anyone know if these are supposed to be reversed like this? */

    physDev->font.fontinfo.Builtin.tm.tmDigitizedAspectX = physDev->logPixelsY;
    physDev->font.fontinfo.Builtin.tm.tmDigitizedAspectY = physDev->logPixelsX;

    return TRUE;
}

BOOL PSDRV_WriteSetBuiltinFont(PSDRV_PDEVICE *physDev)
{
    return PSDRV_WriteSetFont(physDev,
			      physDev->font.fontinfo.Builtin.afm->FontName,
			      physDev->font.size, physDev->font.escapement);
}

BOOL PSDRV_WriteBuiltinGlyphShow(PSDRV_PDEVICE *physDev, LPCWSTR str, INT count)
{
    int i;
    LPCSTR name;

    for (i = 0; i < count; ++i)
    {
	name = PSDRV_UVMetrics(str[i], physDev->font.fontinfo.Builtin.afm)->N->sz;

	PSDRV_WriteGlyphShow(physDev, name);
    }

    return TRUE;
}

/***********************************************************************
 *           PSDRV_GetTextMetrics
 */
BOOL PSDRV_GetTextMetrics(PSDRV_PDEVICE *physDev, TEXTMETRICW *metrics)
{
    assert(physDev->font.fontloc == Builtin);

    memcpy(metrics, &(physDev->font.fontinfo.Builtin.tm),
	   sizeof(physDev->font.fontinfo.Builtin.tm));
    return TRUE;
}

/******************************************************************************
 *  	PSDRV_UVMetrics
 *
 *  Find the AFMMETRICS for a given UV.  Returns first glyph in the font
 *  (space?) if the font does not have a glyph for the given UV.
 */
static int MetricsByUV(const void *a, const void *b)
{
    return (int)(((const AFMMETRICS *)a)->UV - ((const AFMMETRICS *)b)->UV);
}

const AFMMETRICS *PSDRV_UVMetrics(LONG UV, const AFM *afm)
{
    AFMMETRICS	    	key;
    const AFMMETRICS	*needle;

    /*
     *	Ugly work-around for symbol fonts.  Wine is sending characters which
     *	belong in the Unicode private use range (U+F020 - U+F0FF) as ASCII
     *	characters (U+0020 - U+00FF).
     */

    if ((afm->Metrics->UV & 0xff00) == 0xf000 && UV < 0x100)
    	UV |= 0xf000;

    key.UV = UV;

    needle = bsearch(&key, afm->Metrics, afm->NumofMetrics, sizeof(AFMMETRICS),
	    MetricsByUV);

    if (needle == NULL)
    {
    	WARN("No glyph for U+%.4lX in %s\n", UV, afm->FontName);
    	needle = afm->Metrics;
    }

    return needle;
}

/***********************************************************************
 *           PSDRV_GetTextExtentPoint
 */
BOOL PSDRV_GetTextExtentPoint(PSDRV_PDEVICE *physDev, LPCWSTR str, INT count, LPSIZE size)
{
    int     	    i;
    float   	    width = 0.0;

    assert(physDev->font.fontloc == Builtin);

    TRACE("%s %i\n", debugstr_wn(str, count), count);

    for (i = 0; i < count && str[i] != '\0'; ++i)
	width += PSDRV_UVMetrics(str[i], physDev->font.fontinfo.Builtin.afm)->WX;

    size->cx = width * physDev->font.fontinfo.Builtin.scale;
    size->cy = physDev->font.fontinfo.Builtin.tm.tmHeight;

    TRACE("cx=%li cy=%li\n", size->cx, size->cy);

    return TRUE;
}

/***********************************************************************
 *           PSDRV_GetCharWidth
 */
BOOL PSDRV_GetCharWidth(PSDRV_PDEVICE *physDev, UINT firstChar, UINT lastChar, LPINT buffer)
{
    UINT    	    i;

    assert(physDev->font.fontloc == Builtin);

    TRACE("U+%.4X U+%.4X\n", firstChar, lastChar);

    if (lastChar > 0xffff || firstChar > lastChar)
    {
    	SetLastError(ERROR_INVALID_PARAMETER);
    	return FALSE;
    }

    for (i = firstChar; i <= lastChar; ++i)
    {
        *buffer = floor( PSDRV_UVMetrics(i, physDev->font.fontinfo.Builtin.afm)->WX
                         * physDev->font.fontinfo.Builtin.scale + 0.5 );
	TRACE("U+%.4X: %i\n", i, *buffer);
	++buffer;
    }

    return TRUE;
}


/***********************************************************************
 *           PSDRV_GetFontMetric
 */
static UINT PSDRV_GetFontMetric(HDC hdc, const AFM *afm,
    	NEWTEXTMETRICEXW *ntmx, ENUMLOGFONTEXW *elfx)
{
    /* ntmx->ntmTm is NEWTEXTMETRICW; compatible w/ TEXTMETRICW per Win32 doc */

    TEXTMETRICW     *tm = (TEXTMETRICW *)&(ntmx->ntmTm);
    LOGFONTW	    *lf = &(elfx->elfLogFont);
    PSFONT  	    font;

    memset(ntmx, 0, sizeof(*ntmx));
    memset(elfx, 0, sizeof(*elfx));

    ScaleFont(afm, -(LONG)(afm->WinMetrics.usUnitsPerEm), &font, tm);

    lf->lfHeight = tm->tmHeight;
    lf->lfWidth = tm->tmAveCharWidth;
    lf->lfWeight = tm->tmWeight;
    lf->lfItalic = tm->tmItalic;
    lf->lfCharSet = tm->tmCharSet;

    lf->lfPitchAndFamily = (afm->IsFixedPitch) ? FIXED_PITCH : VARIABLE_PITCH;

    MultiByteToWideChar(CP_ACP, 0, afm->FamilyName, -1, lf->lfFaceName,
    	    LF_FACESIZE);

    return DEVICE_FONTTYPE;
}

/***********************************************************************
 *           PSDRV_EnumDeviceFonts
 */
BOOL PSDRV_EnumDeviceFonts( PSDRV_PDEVICE *physDev, LPLOGFONTW plf,
			    FONTENUMPROCW proc, LPARAM lp )
{
    ENUMLOGFONTEXW	lf;
    NEWTEXTMETRICEXW	tm;
    BOOL	  	b, bRet = 0;
    AFMLISTENTRY	*afmle;
    FONTFAMILY		*family;
    char                FaceName[LF_FACESIZE];

    if( plf->lfFaceName[0] ) {
        WideCharToMultiByte(CP_ACP, 0, plf->lfFaceName, -1,
			  FaceName, sizeof(FaceName), NULL, NULL);
        TRACE("lfFaceName = '%s'\n", FaceName);
        for(family = physDev->pi->Fonts; family; family = family->next) {
            if(!strncmp(FaceName, family->FamilyName,
			strlen(family->FamilyName)))
	        break;
	}
	if(family) {
	    for(afmle = family->afmlist; afmle; afmle = afmle->next) {
	        TRACE("Got '%s'\n", afmle->afm->FontName);
		if( (b = (*proc)( &lf.elfLogFont, (TEXTMETRICW *)&tm,
			PSDRV_GetFontMetric( physDev->hdc, afmle->afm, &tm, &lf ),
				  lp )) )
		     bRet = b;
		else break;
	    }
	}
    } else {

        TRACE("lfFaceName = NULL\n");
        for(family = physDev->pi->Fonts; family; family = family->next) {
	    afmle = family->afmlist;
	    TRACE("Got '%s'\n", afmle->afm->FontName);
	    if( (b = (*proc)( &lf.elfLogFont, (TEXTMETRICW *)&tm,
		   PSDRV_GetFontMetric( physDev->hdc, afmle->afm, &tm, &lf ),
			      lp )) )
	        bRet = b;
	    else break;
	}
    }
    return bRet;
}
