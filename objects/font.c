/*
 * GDI font objects
 *
 * Copyright 1993 Alexandre Julliard
 *           1997 Alex Korobka
 * Copyright 2002,2003 Shachar Shemesh
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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "winerror.h"
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "wownt32.h"
#include "gdi.h"
#include "gdi_private.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(font);
WINE_DECLARE_DEBUG_CHANNEL(gdi);

  /* Device -> World size conversion */

/* Performs a device to world transformation on the specified width (which
 * is in integer format).
 */
static inline INT INTERNAL_XDSTOWS(DC *dc, INT width)
{
    FLOAT floatWidth;

    /* Perform operation with floating point */
    floatWidth = (FLOAT)width * dc->xformVport2World.eM11;
    /* Round to integers */
    return GDI_ROUND(floatWidth);
}

/* Performs a device to world transformation on the specified size (which
 * is in integer format).
 */
static inline INT INTERNAL_YDSTOWS(DC *dc, INT height)
{
    FLOAT floatHeight;

    /* Perform operation with floating point */
    floatHeight = (FLOAT)height * dc->xformVport2World.eM22;
    /* Round to integers */
    return GDI_ROUND(floatHeight);
}


static HGDIOBJ FONT_SelectObject( HGDIOBJ handle, void *obj, HDC hdc );
static INT FONT_GetObject16( HGDIOBJ handle, void *obj, INT count, LPVOID buffer );
static INT FONT_GetObjectA( HGDIOBJ handle, void *obj, INT count, LPVOID buffer );
static INT FONT_GetObjectW( HGDIOBJ handle, void *obj, INT count, LPVOID buffer );
static BOOL FONT_DeleteObject( HGDIOBJ handle, void *obj );

static const struct gdi_obj_funcs font_funcs =
{
    FONT_SelectObject,  /* pSelectObject */
    FONT_GetObject16,   /* pGetObject16 */
    FONT_GetObjectA,    /* pGetObjectA */
    FONT_GetObjectW,    /* pGetObjectW */
    NULL,               /* pUnrealizeObject */
    FONT_DeleteObject   /* pDeleteObject */
};

#define ENUM_UNICODE	0x00000001
#define ENUM_CALLED     0x00000002

typedef struct
{
    GDIOBJHDR   header;
    LOGFONTW    logfont;
} FONTOBJ;

typedef struct
{
  LPLOGFONT16           lpLogFontParam;
  FONTENUMPROC16        lpEnumFunc;
  LPARAM                lpData;

  LPNEWTEXTMETRICEX16   lpTextMetric;
  LPENUMLOGFONTEX16     lpLogFont;
  SEGPTR                segTextMetric;
  SEGPTR                segLogFont;
  DWORD                 dwFlags;
  HDC                   hdc;
  DC                   *dc;
  PHYSDEV               physDev;
} fontEnum16;

typedef struct
{
  LPLOGFONTW          lpLogFontParam;
  FONTENUMPROCW       lpEnumFunc;
  LPARAM              lpData;
  DWORD               dwFlags;
  HDC                 hdc;
  DC                 *dc;
  PHYSDEV             physDev;
} fontEnum32;

/*
 *  For TranslateCharsetInfo
 */
#define FS(x) {{0,0,0,0},{0x1<<(x),0}}
#define MAXTCIINDEX 32
static CHARSETINFO FONT_tci[MAXTCIINDEX] = {
  /* ANSI */
  { ANSI_CHARSET, 1252, FS(0)},
  { EASTEUROPE_CHARSET, 1250, FS(1)},
  { RUSSIAN_CHARSET, 1251, FS(2)},
  { GREEK_CHARSET, 1253, FS(3)},
  { TURKISH_CHARSET, 1254, FS(4)},
  { HEBREW_CHARSET, 1255, FS(5)},
  { ARABIC_CHARSET, 1256, FS(6)},
  { BALTIC_CHARSET, 1257, FS(7)},
  { VIETNAMESE_CHARSET, 1258, FS(8)},
  /* reserved by ANSI */
  { DEFAULT_CHARSET, 0, FS(0)},
  { DEFAULT_CHARSET, 0, FS(0)},
  { DEFAULT_CHARSET, 0, FS(0)},
  { DEFAULT_CHARSET, 0, FS(0)},
  { DEFAULT_CHARSET, 0, FS(0)},
  { DEFAULT_CHARSET, 0, FS(0)},
  { DEFAULT_CHARSET, 0, FS(0)},
  /* ANSI and OEM */
  { THAI_CHARSET,  874,  FS(16)},
  { SHIFTJIS_CHARSET, 932, FS(17)},
  { GB2312_CHARSET, 936, FS(18)},
  { HANGEUL_CHARSET, 949, FS(19)},
  { CHINESEBIG5_CHARSET, 950, FS(20)},
  { JOHAB_CHARSET, 1361, FS(21)},
  /* reserved for alternate ANSI and OEM */
  { DEFAULT_CHARSET, 0, FS(0)},
  { DEFAULT_CHARSET, 0, FS(0)},
  { DEFAULT_CHARSET, 0, FS(0)},
  { DEFAULT_CHARSET, 0, FS(0)},
  { DEFAULT_CHARSET, 0, FS(0)},
  { DEFAULT_CHARSET, 0, FS(0)},
  { DEFAULT_CHARSET, 0, FS(0)},
  { DEFAULT_CHARSET, 0, FS(0)},
  /* reserved for system */
  { DEFAULT_CHARSET, 0, FS(0)},
  { SYMBOL_CHARSET, CP_SYMBOL, FS(31)},
};

/***********************************************************************
 *              LOGFONT conversion functions.
 */
static void FONT_LogFontWTo16( const LOGFONTW* font32, LPLOGFONT16 font16 )
{
    font16->lfHeight = font32->lfHeight;
    font16->lfWidth = font32->lfWidth;
    font16->lfEscapement = font32->lfEscapement;
    font16->lfOrientation = font32->lfOrientation;
    font16->lfWeight = font32->lfWeight;
    font16->lfItalic = font32->lfItalic;
    font16->lfUnderline = font32->lfUnderline;
    font16->lfStrikeOut = font32->lfStrikeOut;
    font16->lfCharSet = font32->lfCharSet;
    font16->lfOutPrecision = font32->lfOutPrecision;
    font16->lfClipPrecision = font32->lfClipPrecision;
    font16->lfQuality = font32->lfQuality;
    font16->lfPitchAndFamily = font32->lfPitchAndFamily;
    WideCharToMultiByte( CP_ACP, 0, font32->lfFaceName, -1,
                         font16->lfFaceName, LF_FACESIZE, NULL, NULL );
    font16->lfFaceName[LF_FACESIZE-1] = 0;
}

static void FONT_LogFont16ToW( const LOGFONT16 *font16, LPLOGFONTW font32 )
{
    font32->lfHeight = font16->lfHeight;
    font32->lfWidth = font16->lfWidth;
    font32->lfEscapement = font16->lfEscapement;
    font32->lfOrientation = font16->lfOrientation;
    font32->lfWeight = font16->lfWeight;
    font32->lfItalic = font16->lfItalic;
    font32->lfUnderline = font16->lfUnderline;
    font32->lfStrikeOut = font16->lfStrikeOut;
    font32->lfCharSet = font16->lfCharSet;
    font32->lfOutPrecision = font16->lfOutPrecision;
    font32->lfClipPrecision = font16->lfClipPrecision;
    font32->lfQuality = font16->lfQuality;
    font32->lfPitchAndFamily = font16->lfPitchAndFamily;
    MultiByteToWideChar( CP_ACP, 0, font16->lfFaceName, -1, font32->lfFaceName, LF_FACESIZE );
    font32->lfFaceName[LF_FACESIZE-1] = 0;
}

static void FONT_LogFontAToW( const LOGFONTA *fontA, LPLOGFONTW fontW )
{
    memcpy(fontW, fontA, sizeof(LOGFONTA) - LF_FACESIZE);
    MultiByteToWideChar(CP_ACP, 0, fontA->lfFaceName, -1, fontW->lfFaceName,
			LF_FACESIZE);
}

static void FONT_LogFontWToA( const LOGFONTW *fontW, LPLOGFONTA fontA )
{
    memcpy(fontA, fontW, sizeof(LOGFONTA) - LF_FACESIZE);
    WideCharToMultiByte(CP_ACP, 0, fontW->lfFaceName, -1, fontA->lfFaceName,
			LF_FACESIZE, NULL, NULL);
}

static void FONT_EnumLogFontExWTo16( const ENUMLOGFONTEXW *fontW, LPENUMLOGFONTEX16 font16 )
{
    FONT_LogFontWTo16( (LPLOGFONTW)fontW, (LPLOGFONT16)font16);

    WideCharToMultiByte( CP_ACP, 0, fontW->elfFullName, -1,
			 font16->elfFullName, LF_FULLFACESIZE, NULL, NULL );
    font16->elfFullName[LF_FULLFACESIZE-1] = '\0';
    WideCharToMultiByte( CP_ACP, 0, fontW->elfStyle, -1,
			 font16->elfStyle, LF_FACESIZE, NULL, NULL );
    font16->elfStyle[LF_FACESIZE-1] = '\0';
    WideCharToMultiByte( CP_ACP, 0, fontW->elfScript, -1,
			 font16->elfScript, LF_FACESIZE, NULL, NULL );
    font16->elfScript[LF_FACESIZE-1] = '\0';
}

static void FONT_EnumLogFontExWToA( const ENUMLOGFONTEXW *fontW, LPENUMLOGFONTEXA fontA )
{
    FONT_LogFontWToA( (LPLOGFONTW)fontW, (LPLOGFONTA)fontA);

    WideCharToMultiByte( CP_ACP, 0, fontW->elfFullName, -1,
			 fontA->elfFullName, LF_FULLFACESIZE, NULL, NULL );
    fontA->elfFullName[LF_FULLFACESIZE-1] = '\0';
    WideCharToMultiByte( CP_ACP, 0, fontW->elfStyle, -1,
			 fontA->elfStyle, LF_FACESIZE, NULL, NULL );
    fontA->elfStyle[LF_FACESIZE-1] = '\0';
    WideCharToMultiByte( CP_ACP, 0, fontW->elfScript, -1,
			 fontA->elfScript, LF_FACESIZE, NULL, NULL );
    fontA->elfScript[LF_FACESIZE-1] = '\0';
}

/***********************************************************************
 *              TEXTMETRIC conversion functions.
 */
static void FONT_TextMetricWToA(const TEXTMETRICW *ptmW, LPTEXTMETRICA ptmA )
{
    ptmA->tmHeight = ptmW->tmHeight;
    ptmA->tmAscent = ptmW->tmAscent;
    ptmA->tmDescent = ptmW->tmDescent;
    ptmA->tmInternalLeading = ptmW->tmInternalLeading;
    ptmA->tmExternalLeading = ptmW->tmExternalLeading;
    ptmA->tmAveCharWidth = ptmW->tmAveCharWidth;
    ptmA->tmMaxCharWidth = ptmW->tmMaxCharWidth;
    ptmA->tmWeight = ptmW->tmWeight;
    ptmA->tmOverhang = ptmW->tmOverhang;
    ptmA->tmDigitizedAspectX = ptmW->tmDigitizedAspectX;
    ptmA->tmDigitizedAspectY = ptmW->tmDigitizedAspectY;
    ptmA->tmFirstChar = ptmW->tmFirstChar > 255 ? 255 : ptmW->tmFirstChar;
    ptmA->tmLastChar = ptmW->tmLastChar > 255 ? 255 : ptmW->tmLastChar;
    ptmA->tmDefaultChar = ptmW->tmDefaultChar > 255 ? 255 : ptmW->tmDefaultChar;
    ptmA->tmBreakChar = ptmW->tmBreakChar > 255 ? 255 : ptmW->tmBreakChar;
    ptmA->tmItalic = ptmW->tmItalic;
    ptmA->tmUnderlined = ptmW->tmUnderlined;
    ptmA->tmStruckOut = ptmW->tmStruckOut;
    ptmA->tmPitchAndFamily = ptmW->tmPitchAndFamily;
    ptmA->tmCharSet = ptmW->tmCharSet;
}


static void FONT_NewTextMetricExWTo16(const NEWTEXTMETRICEXW *ptmW, LPNEWTEXTMETRICEX16 ptm16 )
{
    ptm16->ntmTm.tmHeight = ptmW->ntmTm.tmHeight;
    ptm16->ntmTm.tmAscent = ptmW->ntmTm.tmAscent;
    ptm16->ntmTm.tmDescent = ptmW->ntmTm.tmDescent;
    ptm16->ntmTm.tmInternalLeading = ptmW->ntmTm.tmInternalLeading;
    ptm16->ntmTm.tmExternalLeading = ptmW->ntmTm.tmExternalLeading;
    ptm16->ntmTm.tmAveCharWidth = ptmW->ntmTm.tmAveCharWidth;
    ptm16->ntmTm.tmMaxCharWidth = ptmW->ntmTm.tmMaxCharWidth;
    ptm16->ntmTm.tmWeight = ptmW->ntmTm.tmWeight;
    ptm16->ntmTm.tmOverhang = ptmW->ntmTm.tmOverhang;
    ptm16->ntmTm.tmDigitizedAspectX = ptmW->ntmTm.tmDigitizedAspectX;
    ptm16->ntmTm.tmDigitizedAspectY = ptmW->ntmTm.tmDigitizedAspectY;
    ptm16->ntmTm.tmFirstChar = ptmW->ntmTm.tmFirstChar > 255 ? 255 : ptmW->ntmTm.tmFirstChar;
    ptm16->ntmTm.tmLastChar = ptmW->ntmTm.tmLastChar > 255 ? 255 : ptmW->ntmTm.tmLastChar;
    ptm16->ntmTm.tmDefaultChar = ptmW->ntmTm.tmDefaultChar > 255 ? 255 : ptmW->ntmTm.tmDefaultChar;
    ptm16->ntmTm.tmBreakChar = ptmW->ntmTm.tmBreakChar > 255 ? 255 : ptmW->ntmTm.tmBreakChar;
    ptm16->ntmTm.tmItalic = ptmW->ntmTm.tmItalic;
    ptm16->ntmTm.tmUnderlined = ptmW->ntmTm.tmUnderlined;
    ptm16->ntmTm.tmStruckOut = ptmW->ntmTm.tmStruckOut;
    ptm16->ntmTm.tmPitchAndFamily = ptmW->ntmTm.tmPitchAndFamily;
    ptm16->ntmTm.tmCharSet = ptmW->ntmTm.tmCharSet;
    ptm16->ntmTm.ntmFlags = ptmW->ntmTm.ntmFlags;
    ptm16->ntmTm.ntmSizeEM = ptmW->ntmTm.ntmSizeEM;
    ptm16->ntmTm.ntmCellHeight = ptmW->ntmTm.ntmCellHeight;
    ptm16->ntmTm.ntmAvgWidth = ptmW->ntmTm.ntmAvgWidth;
    memcpy(&ptm16->ntmFontSig, &ptmW->ntmFontSig, sizeof(FONTSIGNATURE));
}

static void FONT_NewTextMetricExWToA(const NEWTEXTMETRICEXW *ptmW, NEWTEXTMETRICEXA *ptmA )
{
    FONT_TextMetricWToA((LPTEXTMETRICW)ptmW, (LPTEXTMETRICA)ptmA);
    ptmA->ntmTm.ntmFlags = ptmW->ntmTm.ntmFlags;
    ptmA->ntmTm.ntmSizeEM = ptmW->ntmTm.ntmSizeEM;
    ptmA->ntmTm.ntmCellHeight = ptmW->ntmTm.ntmCellHeight;
    ptmA->ntmTm.ntmAvgWidth = ptmW->ntmTm.ntmAvgWidth;
    memcpy(&ptmA->ntmFontSig, &ptmW->ntmFontSig, sizeof(FONTSIGNATURE));
}

/***********************************************************************
 *           CreateFontIndirectA   (GDI32.@)
 */
HFONT WINAPI CreateFontIndirectA( const LOGFONTA *plfA )
{
    LOGFONTW lfW;

    if (plfA) {
	FONT_LogFontAToW( plfA, &lfW );
	return CreateFontIndirectW( &lfW );
     } else
	return CreateFontIndirectW( NULL );

}

/***********************************************************************
 *           CreateFontIndirectW   (GDI32.@)
 */
HFONT WINAPI CreateFontIndirectW( const LOGFONTW *plf )
{
    HFONT hFont = 0;

    if (plf)
    {
        FONTOBJ* fontPtr;
	if ((fontPtr = GDI_AllocObject( sizeof(FONTOBJ), FONT_MAGIC,
					(HGDIOBJ *)&hFont, &font_funcs )))
	{
            static const WCHAR ItalicW[] = {' ','I','t','a','l','i','c','\0'};
            static const WCHAR BoldW[]   = {' ','B','o','l','d','\0'};
            WCHAR *pFaceNameItalicSuffix, *pFaceNameBoldSuffix;
            WCHAR* pFaceNameSuffix = NULL;

	    memcpy( &fontPtr->logfont, plf, sizeof(LOGFONTW) );

	    TRACE("(%ld %ld %ld %ld %x %d %x %d %d) %s %s %s %s => %p\n",
                  plf->lfHeight, plf->lfWidth,
                  plf->lfEscapement, plf->lfOrientation,
                  plf->lfPitchAndFamily,
		  plf->lfOutPrecision, plf->lfClipPrecision,
		  plf->lfQuality, plf->lfCharSet,
                  debugstr_w(plf->lfFaceName),
                  plf->lfWeight > 400 ? "Bold" : "",
                  plf->lfItalic ? "Italic" : "",
                  plf->lfUnderline ? "Underline" : "", hFont);

	    if (plf->lfEscapement != plf->lfOrientation) {
	      /* this should really depend on whether GM_ADVANCED is set */
	      fontPtr->logfont.lfOrientation = fontPtr->logfont.lfEscapement;
	      WARN("orientation angle %f set to "
                   "escapement angle %f for new font %p\n",
                   plf->lfOrientation/10., plf->lfEscapement/10., hFont);
	    }

            pFaceNameItalicSuffix = strstrW(fontPtr->logfont.lfFaceName, ItalicW);
            if (pFaceNameItalicSuffix) {
                fontPtr->logfont.lfItalic = TRUE;
                pFaceNameSuffix = pFaceNameItalicSuffix;
            }

            pFaceNameBoldSuffix = strstrW(fontPtr->logfont.lfFaceName, BoldW);
            if (pFaceNameBoldSuffix) {
                if (fontPtr->logfont.lfWeight < FW_BOLD) {
                    fontPtr->logfont.lfWeight = FW_BOLD;
                }
                if (!pFaceNameSuffix ||
                    (pFaceNameBoldSuffix < pFaceNameSuffix)) {
                    pFaceNameSuffix = pFaceNameBoldSuffix;
                }
            }

            if (pFaceNameSuffix) *pFaceNameSuffix = 0;

	    GDI_ReleaseObj( hFont );
	}
    }
    else WARN("(NULL) => NULL\n");

    return hFont;
}

/*************************************************************************
 *           CreateFontA    (GDI32.@)
 */
HFONT WINAPI CreateFontA( INT height, INT width, INT esc,
                              INT orient, INT weight, DWORD italic,
                              DWORD underline, DWORD strikeout, DWORD charset,
                              DWORD outpres, DWORD clippres, DWORD quality,
                              DWORD pitch, LPCSTR name )
{
    LOGFONTA logfont;

    logfont.lfHeight = height;
    logfont.lfWidth = width;
    logfont.lfEscapement = esc;
    logfont.lfOrientation = orient;
    logfont.lfWeight = weight;
    logfont.lfItalic = italic;
    logfont.lfUnderline = underline;
    logfont.lfStrikeOut = strikeout;
    logfont.lfCharSet = charset;
    logfont.lfOutPrecision = outpres;
    logfont.lfClipPrecision = clippres;
    logfont.lfQuality = quality;
    logfont.lfPitchAndFamily = pitch;

    if (name)
	lstrcpynA(logfont.lfFaceName,name,sizeof(logfont.lfFaceName));
    else
	logfont.lfFaceName[0] = '\0';

    return CreateFontIndirectA( &logfont );
}

/*************************************************************************
 *           CreateFontW    (GDI32.@)
 */
HFONT WINAPI CreateFontW( INT height, INT width, INT esc,
                              INT orient, INT weight, DWORD italic,
                              DWORD underline, DWORD strikeout, DWORD charset,
                              DWORD outpres, DWORD clippres, DWORD quality,
                              DWORD pitch, LPCWSTR name )
{
    LOGFONTW logfont;

    logfont.lfHeight = height;
    logfont.lfWidth = width;
    logfont.lfEscapement = esc;
    logfont.lfOrientation = orient;
    logfont.lfWeight = weight;
    logfont.lfItalic = italic;
    logfont.lfUnderline = underline;
    logfont.lfStrikeOut = strikeout;
    logfont.lfCharSet = charset;
    logfont.lfOutPrecision = outpres;
    logfont.lfClipPrecision = clippres;
    logfont.lfQuality = quality;
    logfont.lfPitchAndFamily = pitch;

    if (name)
	lstrcpynW(logfont.lfFaceName, name,
		  sizeof(logfont.lfFaceName) / sizeof(WCHAR));
    else
	logfont.lfFaceName[0] = '\0';

    return CreateFontIndirectW( &logfont );
}


/***********************************************************************
 *           FONT_SelectObject
 *
 * If the driver supports vector fonts we create a gdi font first and
 * then call the driver to give it a chance to supply its own device
 * font.  If the driver wants to do this it returns TRUE and we can
 * delete the gdi font, if the driver wants to use the gdi font it
 * should return FALSE, to signal an error return GDI_ERROR.  For
 * drivers that don't support vector fonts they must supply their own
 * font.
 */
static HGDIOBJ FONT_SelectObject( HGDIOBJ handle, void *obj, HDC hdc )
{
    HGDIOBJ ret = 0;
    DC *dc = DC_GetDCPtr( hdc );

    if (!dc) return 0;

    if (dc->hFont != handle || dc->gdiFont == NULL)
    {
        if(GetDeviceCaps(dc->hSelf, TEXTCAPS) & TC_VA_ABLE)
            dc->gdiFont = WineEngCreateFontInstance(dc, handle);
    }

    if (dc->funcs->pSelectFont) ret = dc->funcs->pSelectFont( dc->physDev, handle, dc->gdiFont );

    if (ret && dc->gdiFont) dc->gdiFont = 0;

    if (ret == HGDI_ERROR)
        ret = 0; /* SelectObject returns 0 on error */
    else
    {
        ret = dc->hFont;
        dc->hFont = handle;
    }
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           FONT_GetObject16
 */
static INT FONT_GetObject16( HGDIOBJ handle, void *obj, INT count, LPVOID buffer )
{
    FONTOBJ *font = obj;
    LOGFONT16 lf16;

    FONT_LogFontWTo16( &font->logfont, &lf16 );

    if (count > sizeof(LOGFONT16)) count = sizeof(LOGFONT16);
    memcpy( buffer, &lf16, count );
    return count;
}

/***********************************************************************
 *           FONT_GetObjectA
 */
static INT FONT_GetObjectA( HGDIOBJ handle, void *obj, INT count, LPVOID buffer )
{
    FONTOBJ *font = obj;
    LOGFONTA lfA;

    if(!buffer)
        return sizeof(lfA);
    FONT_LogFontWToA( &font->logfont, &lfA );

    if (count > sizeof(lfA)) count = sizeof(lfA);
    memcpy( buffer, &lfA, count );
    return count;
}

/***********************************************************************
 *           FONT_GetObjectW
 */
static INT FONT_GetObjectW( HGDIOBJ handle, void *obj, INT count, LPVOID buffer )
{
    FONTOBJ *font = obj;
    if(!buffer)
        return sizeof(LOGFONTW);
    if (count > sizeof(LOGFONTW)) count = sizeof(LOGFONTW);
    memcpy( buffer, &font->logfont, count );
    return count;
}


/***********************************************************************
 *           FONT_DeleteObject
 */
static BOOL FONT_DeleteObject( HGDIOBJ handle, void *obj )
{
    WineEngDestroyFontInstance( handle );
    return GDI_FreeObject( handle, obj );
}


/***********************************************************************
 *              FONT_EnumInstance16
 *
 * Called by the device driver layer to pass font info
 * down to the application.
 *
 * Note: plf is really an ENUMLOGFONTEXW, and ptm is a NEWTEXTMETRICEXW.
 *       We have to use other types because of the FONTENUMPROCW definition.
 */
static INT CALLBACK FONT_EnumInstance16( const LOGFONTW *plf, const TEXTMETRICW *ptm,
                                         DWORD fType, LPARAM lp )
{
    fontEnum16 *pfe = (fontEnum16*)lp;
    INT ret = 1;
    DC *dc;

    if( pfe->lpLogFontParam->lfCharSet == DEFAULT_CHARSET ||
        pfe->lpLogFontParam->lfCharSet == plf->lfCharSet )
    {
        WORD args[7];
        DWORD result;

        FONT_EnumLogFontExWTo16((const ENUMLOGFONTEXW *)plf, pfe->lpLogFont);
        FONT_NewTextMetricExWTo16((const NEWTEXTMETRICEXW *)ptm, pfe->lpTextMetric);
        pfe->dwFlags |= ENUM_CALLED;
        GDI_ReleaseObj( pfe->hdc );  /* release the GDI lock */

        args[6] = SELECTOROF(pfe->segLogFont);
        args[5] = OFFSETOF(pfe->segLogFont);
        args[4] = SELECTOROF(pfe->segTextMetric);
        args[3] = OFFSETOF(pfe->segTextMetric);
        args[2] = fType;
        args[1] = HIWORD(pfe->lpData);
        args[0] = LOWORD(pfe->lpData);
        WOWCallback16Ex( (DWORD)pfe->lpEnumFunc, WCB16_PASCAL, sizeof(args), args, &result );
        ret = LOWORD(result);

        /* get the lock again and make sure the DC is still valid */
        dc = DC_GetDCPtr( pfe->hdc );
        if (!dc || dc != pfe->dc || dc->physDev != pfe->physDev)
        {
            if (dc) GDI_ReleaseObj( pfe->hdc );
            pfe->hdc = 0;  /* make sure we don't try to release it later on */
            ret = 0;
        }
    }
    return ret;
}

/***********************************************************************
 *              FONT_EnumInstance
 *
 * Note: plf is really an ENUMLOGFONTEXW, and ptm is a NEWTEXTMETRICEXW.
 *       We have to use other types because of the FONTENUMPROCW definition.
 */
static INT CALLBACK FONT_EnumInstance( const LOGFONTW *plf, const TEXTMETRICW *ptm,
                                       DWORD fType, LPARAM lp )
{
    fontEnum32 *pfe = (fontEnum32*)lp;
    INT ret = 1;
    DC *dc;

    /* lfCharSet is at the same offset in both LOGFONTA and LOGFONTW */
    if( pfe->lpLogFontParam->lfCharSet == DEFAULT_CHARSET ||
        pfe->lpLogFontParam->lfCharSet == plf->lfCharSet )
    {
	/* convert font metrics */
        ENUMLOGFONTEXA logfont;
        NEWTEXTMETRICEXA tmA;

        pfe->dwFlags |= ENUM_CALLED;
        if (!(pfe->dwFlags & ENUM_UNICODE))
        {
            FONT_EnumLogFontExWToA( (const ENUMLOGFONTEXW *)plf, &logfont);
            FONT_NewTextMetricExWToA( (const NEWTEXTMETRICEXW *)ptm, &tmA );
            plf = (LOGFONTW *)&logfont.elfLogFont;
            ptm = (TEXTMETRICW *)&tmA;
        }
        GDI_ReleaseObj( pfe->hdc );  /* release the GDI lock */

        ret = pfe->lpEnumFunc( plf, ptm, fType, pfe->lpData );

        /* get the lock again and make sure the DC is still valid */
        dc = DC_GetDCPtr( pfe->hdc );
        if (!dc || dc != pfe->dc || dc->physDev != pfe->physDev)
        {
            if (dc) GDI_ReleaseObj( pfe->hdc );
            pfe->hdc = 0;  /* make sure we don't try to release it later on */
            ret = 0;
        }
    }
    return ret;
}

/***********************************************************************
 *              EnumFontFamiliesEx	(GDI.613)
 */
INT16 WINAPI EnumFontFamiliesEx16( HDC16 hDC, LPLOGFONT16 plf,
                                   FONTENUMPROC16 efproc, LPARAM lParam,
                                   DWORD dwFlags)
{
    fontEnum16 fe16;
    INT16	ret = 1, ret2;
    DC* 	dc = DC_GetDCPtr( HDC_32(hDC) );
    NEWTEXTMETRICEX16 tm16;
    ENUMLOGFONTEX16 lf16;
    LOGFONTW lfW;
    BOOL enum_gdi_fonts;

    if (!dc) return 0;
    FONT_LogFont16ToW(plf, &lfW);

    fe16.hdc = HDC_32(hDC);
    fe16.dc = dc;
    fe16.physDev = dc->physDev;
    fe16.lpLogFontParam = plf;
    fe16.lpEnumFunc = efproc;
    fe16.lpData = lParam;
    fe16.lpTextMetric = &tm16;
    fe16.lpLogFont = &lf16;
    fe16.segTextMetric = MapLS( &tm16 );
    fe16.segLogFont = MapLS( &lf16 );
    fe16.dwFlags = 0;

    enum_gdi_fonts = GetDeviceCaps(fe16.hdc, TEXTCAPS) & TC_VA_ABLE;

    if (!dc->funcs->pEnumDeviceFonts && !enum_gdi_fonts)
    {
        ret = 0;
        goto done;
    }

    if (enum_gdi_fonts)
        ret = WineEngEnumFonts( &lfW, FONT_EnumInstance16, (LPARAM)&fe16 );
    fe16.dwFlags &= ~ENUM_CALLED;
    if (ret && dc->funcs->pEnumDeviceFonts) {
	ret2 = dc->funcs->pEnumDeviceFonts( dc->physDev, &lfW, FONT_EnumInstance16, (LPARAM)&fe16 );
	if(fe16.dwFlags & ENUM_CALLED) /* update ret iff a font gets enumed */
	    ret = ret2;
    }
done:
    UnMapLS( fe16.segTextMetric );
    UnMapLS( fe16.segLogFont );
    if (fe16.hdc) GDI_ReleaseObj( fe16.hdc );
    return ret;
}

/***********************************************************************
 *		FONT_EnumFontFamiliesEx
 */
static INT FONT_EnumFontFamiliesEx( HDC hDC, LPLOGFONTW plf,
				    FONTENUMPROCW efproc,
				    LPARAM lParam, DWORD dwUnicode)
{
    INT ret = 1, ret2;
    DC *dc = DC_GetDCPtr( hDC );
    fontEnum32 fe32;
    BOOL enum_gdi_fonts;

    if (!dc) return 0;

    TRACE("lfFaceName = %s lfCharset = %d\n", debugstr_w(plf->lfFaceName),
	  plf->lfCharSet);
    fe32.lpLogFontParam = plf;
    fe32.lpEnumFunc = efproc;
    fe32.lpData = lParam;
    fe32.dwFlags = dwUnicode;
    fe32.hdc = hDC;
    fe32.dc = dc;
    fe32.physDev = dc->physDev;

    enum_gdi_fonts = GetDeviceCaps(hDC, TEXTCAPS) & TC_VA_ABLE;

    if (!dc->funcs->pEnumDeviceFonts && !enum_gdi_fonts)
    {
        ret = 0;
        goto done;
    }

    if (enum_gdi_fonts)
        ret = WineEngEnumFonts( plf, FONT_EnumInstance, (LPARAM)&fe32 );
    fe32.dwFlags &= ~ENUM_CALLED;
    if (ret && dc->funcs->pEnumDeviceFonts) {
	ret2 = dc->funcs->pEnumDeviceFonts( dc->physDev, plf, FONT_EnumInstance, (LPARAM)&fe32 );
	if(fe32.dwFlags & ENUM_CALLED) /* update ret iff a font gets enumed */
	    ret = ret2;
    }
 done:
    if (fe32.hdc) GDI_ReleaseObj( fe32.hdc );
    return ret;
}

/***********************************************************************
 *              EnumFontFamiliesExW	(GDI32.@)
 */
INT WINAPI EnumFontFamiliesExW( HDC hDC, LPLOGFONTW plf,
                                    FONTENUMPROCW efproc,
                                    LPARAM lParam, DWORD dwFlags )
{
    return  FONT_EnumFontFamiliesEx( hDC, plf, efproc, lParam, ENUM_UNICODE );
}

/***********************************************************************
 *              EnumFontFamiliesExA	(GDI32.@)
 */
INT WINAPI EnumFontFamiliesExA( HDC hDC, LPLOGFONTA plf,
                                    FONTENUMPROCA efproc,
                                    LPARAM lParam, DWORD dwFlags)
{
    LOGFONTW lfW;
    FONT_LogFontAToW( plf, &lfW );

    return FONT_EnumFontFamiliesEx( hDC, &lfW, (FONTENUMPROCW)efproc, lParam, 0);
}

/***********************************************************************
 *              EnumFontFamilies	(GDI.330)
 */
INT16 WINAPI EnumFontFamilies16( HDC16 hDC, LPCSTR lpFamily,
                                 FONTENUMPROC16 efproc, LPARAM lpData )
{
    LOGFONT16	lf;

    lf.lfCharSet = DEFAULT_CHARSET;
    if( lpFamily ) lstrcpynA( lf.lfFaceName, lpFamily, LF_FACESIZE );
    else lf.lfFaceName[0] = '\0';

    return EnumFontFamiliesEx16( hDC, &lf, efproc, lpData, 0 );
}

/***********************************************************************
 *              EnumFontFamiliesA	(GDI32.@)
 */
INT WINAPI EnumFontFamiliesA( HDC hDC, LPCSTR lpFamily,
                                  FONTENUMPROCA efproc, LPARAM lpData )
{
    LOGFONTA	lf;

    lf.lfCharSet = DEFAULT_CHARSET;
    if( lpFamily ) lstrcpynA( lf.lfFaceName, lpFamily, LF_FACESIZE );
    else lf.lfFaceName[0] = lf.lfFaceName[1] = '\0';

    return EnumFontFamiliesExA( hDC, &lf, efproc, lpData, 0 );
}

/***********************************************************************
 *              EnumFontFamiliesW	(GDI32.@)
 */
INT WINAPI EnumFontFamiliesW( HDC hDC, LPCWSTR lpFamily,
                                  FONTENUMPROCW efproc, LPARAM lpData )
{
    LOGFONTW  lf;

    lf.lfCharSet = DEFAULT_CHARSET;
    if( lpFamily ) lstrcpynW( lf.lfFaceName, lpFamily, LF_FACESIZE );
    else lf.lfFaceName[0] = 0;

    return EnumFontFamiliesExW( hDC, &lf, efproc, lpData, 0 );
}

/***********************************************************************
 *              EnumFonts		(GDI.70)
 */
INT16 WINAPI EnumFonts16( HDC16 hDC, LPCSTR lpName, FONTENUMPROC16 efproc,
                          LPARAM lpData )
{
    return EnumFontFamilies16( hDC, lpName, efproc, lpData );
}

/***********************************************************************
 *              EnumFontsA		(GDI32.@)
 */
INT WINAPI EnumFontsA( HDC hDC, LPCSTR lpName, FONTENUMPROCA efproc,
                           LPARAM lpData )
{
    return EnumFontFamiliesA( hDC, lpName, efproc, lpData );
}

/***********************************************************************
 *              EnumFontsW		(GDI32.@)
 */
INT WINAPI EnumFontsW( HDC hDC, LPCWSTR lpName, FONTENUMPROCW efproc,
                           LPARAM lpData )
{
    return EnumFontFamiliesW( hDC, lpName, efproc, lpData );
}


/***********************************************************************
 *           GetTextCharacterExtra    (GDI32.@)
 */
INT WINAPI GetTextCharacterExtra( HDC hdc )
{
    INT ret;
    DC *dc = DC_GetDCPtr( hdc );
    if (!dc) return 0x80000000;
    ret = dc->charExtra;
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           SetTextCharacterExtra    (GDI32.@)
 */
INT WINAPI SetTextCharacterExtra( HDC hdc, INT extra )
{
    INT prev;
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return 0x80000000;
    if (dc->funcs->pSetTextCharacterExtra)
        prev = dc->funcs->pSetTextCharacterExtra( dc->physDev, extra );
    else
    {
        prev = dc->charExtra;
        dc->charExtra = extra;
    }
    GDI_ReleaseObj( hdc );
    return prev;
}


/***********************************************************************
 *           SetTextJustification    (GDI32.@)
 */
BOOL WINAPI SetTextJustification( HDC hdc, INT extra, INT breaks )
{
    BOOL ret = TRUE;
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return FALSE;
    if (dc->funcs->pSetTextJustification)
        ret = dc->funcs->pSetTextJustification( dc->physDev, extra, breaks );
    else
    {
        extra = abs((extra * dc->vportExtX + dc->wndExtX / 2) / dc->wndExtX);
        if (!extra) breaks = 0;
        if (breaks)
        {
            dc->breakExtra = extra / breaks;
            dc->breakRem   = extra - (breaks * dc->breakExtra);
        }
        else
        {
            dc->breakExtra = 0;
            dc->breakRem   = 0;
        }
    }
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           GetTextFaceA    (GDI32.@)
 */
INT WINAPI GetTextFaceA( HDC hdc, INT count, LPSTR name )
{
    INT res = GetTextFaceW(hdc, 0, NULL);
    LPWSTR nameW = HeapAlloc( GetProcessHeap(), 0, res * 2 );
    GetTextFaceW( hdc, res, nameW );

    if (name)
    {
        if (count && !WideCharToMultiByte( CP_ACP, 0, nameW, -1, name, count, NULL, NULL))
            name[count-1] = 0;
        res = strlen(name);
    }
    else
        res = WideCharToMultiByte( CP_ACP, 0, nameW, -1, NULL, 0, NULL, NULL);
    HeapFree( GetProcessHeap(), 0, nameW );
    return res;
}

/***********************************************************************
 *           GetTextFaceW    (GDI32.@)
 */
INT WINAPI GetTextFaceW( HDC hdc, INT count, LPWSTR name )
{
    FONTOBJ *font;
    INT     ret = 0;

    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return 0;

    if(dc->gdiFont)
        ret = WineEngGetTextFace(dc->gdiFont, count, name);
    else if ((font = (FONTOBJ *) GDI_GetObjPtr( dc->hFont, FONT_MAGIC )))
    {
        if (name)
        {
            lstrcpynW( name, font->logfont.lfFaceName, count );
            ret = strlenW(name);
        }
        else ret = strlenW(font->logfont.lfFaceName) + 1;
        GDI_ReleaseObj( dc->hFont );
    }
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           GetTextExtentPoint32A    (GDI32.@)
 */
BOOL WINAPI GetTextExtentPoint32A( HDC hdc, LPCSTR str, INT count,
                                     LPSIZE size )
{
    BOOL ret = FALSE;
    INT wlen;
    LPWSTR p = FONT_mbtowc(hdc, str, count, &wlen, NULL);

    if (p) {
	ret = GetTextExtentPoint32W( hdc, p, wlen, size );
	HeapFree( GetProcessHeap(), 0, p );
    }

    TRACE("(%p %s %d %p): returning %ld x %ld\n",
          hdc, debugstr_an (str, count), count, size, size->cx, size->cy );
    return ret;
}


/***********************************************************************
 * GetTextExtentPoint32W [GDI32.@]
 *
 * Computes width/height for a string.
 *
 * Computes width and height of the specified string.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI GetTextExtentPoint32W(
    HDC hdc,     /* [in]  Handle of device context */
    LPCWSTR str,   /* [in]  Address of text string */
    INT count,   /* [in]  Number of characters in string */
    LPSIZE size) /* [out] Address of structure for string size */
{
    BOOL ret = FALSE;
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return FALSE;

    if(dc->gdiFont)
        ret = WineEngGetTextExtentPoint(dc->gdiFont, str, count, size);
    else if(dc->funcs->pGetTextExtentPoint)
        ret = dc->funcs->pGetTextExtentPoint( dc->physDev, str, count, size );

    if (ret)
    {
	size->cx = abs(INTERNAL_XDSTOWS(dc, size->cx));
	size->cy = abs(INTERNAL_YDSTOWS(dc, size->cy));
        size->cx += count * dc->charExtra + dc->breakRem;
    }

    GDI_ReleaseObj( hdc );

    TRACE("(%p %s %d %p): returning %ld x %ld\n",
          hdc, debugstr_wn (str, count), count, size, size->cx, size->cy );
    return ret;
}

/***********************************************************************
 * GetTextExtentPointI [GDI32.@]
 *
 * Computes width and height of the array of glyph indices.
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI GetTextExtentPointI(
    HDC hdc,     /* [in]  Handle of device context */
    const WORD *indices,   /* [in]  Address of glyph index array */
    INT count,   /* [in]  Number of glyphs in array */
    LPSIZE size) /* [out] Address of structure for string size */
{
    BOOL ret = FALSE;
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return FALSE;

    if(dc->gdiFont) {
        ret = WineEngGetTextExtentPointI(dc->gdiFont, indices, count, size);
	size->cx = abs(INTERNAL_XDSTOWS(dc, size->cx));
	size->cy = abs(INTERNAL_YDSTOWS(dc, size->cy));
        size->cx += count * dc->charExtra;
    }
    else if(dc->funcs->pGetTextExtentPoint) {
        FIXME("calling GetTextExtentPoint\n");
        ret = dc->funcs->pGetTextExtentPoint( dc->physDev, (LPCWSTR)indices, count, size );
    }

    GDI_ReleaseObj( hdc );

    TRACE("(%p %p %d %p): returning %ld x %ld\n",
          hdc, indices, count, size, size->cx, size->cy );
    return ret;
}


/***********************************************************************
 *           GetTextExtentPointA    (GDI32.@)
 */
BOOL WINAPI GetTextExtentPointA( HDC hdc, LPCSTR str, INT count,
                                          LPSIZE size )
{
    TRACE("not bug compatible.\n");
    return GetTextExtentPoint32A( hdc, str, count, size );
}

/***********************************************************************
 *           GetTextExtentPointW   (GDI32.@)
 */
BOOL WINAPI GetTextExtentPointW( HDC hdc, LPCWSTR str, INT count,
                                          LPSIZE size )
{
    TRACE("not bug compatible.\n");
    return GetTextExtentPoint32W( hdc, str, count, size );
}


/***********************************************************************
 *           GetTextExtentExPointA    (GDI32.@)
 */
BOOL WINAPI GetTextExtentExPointA( HDC hdc, LPCSTR str, INT count,
				   INT maxExt, LPINT lpnFit,
				   LPINT alpDx, LPSIZE size )
{
    BOOL ret;
    INT wlen;
    LPWSTR p = FONT_mbtowc( hdc, str, count, &wlen, NULL);
    ret = GetTextExtentExPointW( hdc, p, wlen, maxExt, lpnFit, alpDx, size);
    if (lpnFit) *lpnFit = WideCharToMultiByte(CP_ACP,0,p,*lpnFit,NULL,0,NULL,NULL);
    HeapFree( GetProcessHeap(), 0, p );
    return ret;
}


/***********************************************************************
 *           GetTextExtentExPointW    (GDI32.@)
 *
 * Return the size of the string as it would be if it was output properly by
 * e.g. TextOut.
 *
 * This should include
 * - Intercharacter spacing
 * - justification spacing (not yet done)
 * - kerning? see below
 *
 * Kerning.  Since kerning would be carried out by the rendering code it should
 * be done by the driver.  However they don't support it yet.  Also I am not
 * yet persuaded that (certainly under Win95) any kerning is actually done.
 *
 * str: According to MSDN this should be null-terminated.  That is not true; a
 *      null will not terminate it early.
 * size: Certainly under Win95 this appears buggy or weird if *lpnFit is less
 *       than count.  I have seen it be either the size of the full string or
 *       1 less than the size of the full string.  I have not seen it bear any
 *       resemblance to the portion that would fit.
 * lpnFit: What exactly is fitting?  Stupidly, in my opinion, it includes the
 *         trailing intercharacter spacing and any trailing justification.
 *
 * FIXME
 * Currently we do this by measuring each character etc.  We should do it by
 * passing the request to the driver, perhaps by extending the
 * pGetTextExtentPoint function to take the alpDx argument.  That would avoid
 * thinking about kerning issues and rounding issues in the justification.
 */

BOOL WINAPI GetTextExtentExPointW( HDC hdc, LPCWSTR str, INT count,
				   INT maxExt, LPINT lpnFit,
				   LPINT alpDx, LPSIZE size )
{
    int index, nFit, extent;
    SIZE tSize;
    BOOL ret = FALSE;

    TRACE("(%p, %s, %d)\n",hdc,debugstr_wn(str,count),maxExt);

    size->cx = size->cy = nFit = extent = 0;
    for(index = 0; index < count; index++)
    {
 	if(!GetTextExtentPoint32W( hdc, str, 1, &tSize )) goto done;
        /* GetTextExtentPoint includes intercharacter spacing. */
        /* FIXME - justification needs doing yet.  Remember that the base
         * data will not be in logical coordinates.
         */
	extent += tSize.cx;
	if( !lpnFit || extent <= maxExt )
        /* It is allowed to be equal. */
        {
	    nFit++;
	    if( alpDx ) alpDx[index] = extent;
        }
	if( tSize.cy > size->cy ) size->cy = tSize.cy;
	str++;
    }
    size->cx = extent;
    if(lpnFit) *lpnFit = nFit;
    ret = TRUE;

    TRACE("returning %d %ld x %ld\n",nFit,size->cx,size->cy);

done:
    return ret;
}

/***********************************************************************
 *           GetTextMetricsA    (GDI32.@)
 */
BOOL WINAPI GetTextMetricsA( HDC hdc, TEXTMETRICA *metrics )
{
    TEXTMETRICW tm32;

    if (!GetTextMetricsW( hdc, &tm32 )) return FALSE;
    FONT_TextMetricWToA( &tm32, metrics );
    return TRUE;
}

/***********************************************************************
 *           GetTextMetricsW    (GDI32.@)
 */
BOOL WINAPI GetTextMetricsW( HDC hdc, TEXTMETRICW *metrics )
{
    BOOL ret = FALSE;
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return FALSE;

    if (dc->gdiFont)
        ret = WineEngGetTextMetrics(dc->gdiFont, metrics);
    else if (dc->funcs->pGetTextMetrics)
        ret = dc->funcs->pGetTextMetrics( dc->physDev, metrics );

    if (ret)
    {
    /* device layer returns values in device units
     * therefore we have to convert them to logical */

#define WDPTOLP(x) ((x<0)?					\
		(-abs(INTERNAL_XDSTOWS(dc, (x)))):		\
		(abs(INTERNAL_XDSTOWS(dc, (x)))))
#define HDPTOLP(y) ((y<0)?					\
		(-abs(INTERNAL_YDSTOWS(dc, (y)))):		\
		(abs(INTERNAL_YDSTOWS(dc, (y)))))

    metrics->tmHeight           = HDPTOLP(metrics->tmHeight);
    metrics->tmAscent           = HDPTOLP(metrics->tmAscent);
    metrics->tmDescent          = HDPTOLP(metrics->tmDescent);
    metrics->tmInternalLeading  = HDPTOLP(metrics->tmInternalLeading);
    metrics->tmExternalLeading  = HDPTOLP(metrics->tmExternalLeading);
    metrics->tmAveCharWidth     = WDPTOLP(metrics->tmAveCharWidth);
    metrics->tmMaxCharWidth     = WDPTOLP(metrics->tmMaxCharWidth);
    metrics->tmOverhang         = WDPTOLP(metrics->tmOverhang);
        ret = TRUE;
#undef WDPTOLP
#undef HDPTOLP
    TRACE("text metrics:\n"
          "    Weight = %03li\t FirstChar = %i\t AveCharWidth = %li\n"
          "    Italic = % 3i\t LastChar = %i\t\t MaxCharWidth = %li\n"
          "    UnderLined = %01i\t DefaultChar = %i\t Overhang = %li\n"
          "    StruckOut = %01i\t BreakChar = %i\t CharSet = %i\n"
          "    PitchAndFamily = %02x\n"
          "    --------------------\n"
          "    InternalLeading = %li\n"
          "    Ascent = %li\n"
          "    Descent = %li\n"
          "    Height = %li\n",
          metrics->tmWeight, metrics->tmFirstChar, metrics->tmAveCharWidth,
          metrics->tmItalic, metrics->tmLastChar, metrics->tmMaxCharWidth,
          metrics->tmUnderlined, metrics->tmDefaultChar, metrics->tmOverhang,
          metrics->tmStruckOut, metrics->tmBreakChar, metrics->tmCharSet,
          metrics->tmPitchAndFamily,
          metrics->tmInternalLeading,
          metrics->tmAscent,
          metrics->tmDescent,
          metrics->tmHeight );
    }
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 * GetOutlineTextMetrics [GDI.308]  Gets metrics for TrueType fonts.
 *
 * NOTES
 *    lpOTM should be LPOUTLINETEXTMETRIC
 *
 * RETURNS
 *    Success: Non-zero or size of required buffer
 *    Failure: 0
 */
UINT16 WINAPI GetOutlineTextMetrics16(
    HDC16 hdc,    /* [in]  Handle of device context */
    UINT16 cbData, /* [in]  Size of metric data array */
    LPOUTLINETEXTMETRIC16 lpOTM)  /* [out] Address of metric data array */
{
    FIXME("(%04x,%04x,%p): stub\n", hdc,cbData,lpOTM);
    return 0;
}


/***********************************************************************
 *		GetOutlineTextMetricsA (GDI32.@)
 * Gets metrics for TrueType fonts.
 *
 * NOTES
 *    If the supplied buffer isn't big enough Windows partially fills it up to
 *    its given length and returns that length.
 *
 * RETURNS
 *    Success: Non-zero or size of required buffer
 *    Failure: 0
 */
UINT WINAPI GetOutlineTextMetricsA(
    HDC hdc,    /* [in]  Handle of device context */
    UINT cbData, /* [in]  Size of metric data array */
    LPOUTLINETEXTMETRICA lpOTM)  /* [out] Address of metric data array */
{
    char buf[512], *ptr;
    UINT ret, needed;
    OUTLINETEXTMETRICW *lpOTMW = (OUTLINETEXTMETRICW *)buf;
    OUTLINETEXTMETRICA *output = lpOTM;
    INT left, len;

    if((ret = GetOutlineTextMetricsW(hdc, 0, NULL)) == 0)
        return 0;
    if(ret > sizeof(buf))
	lpOTMW = HeapAlloc(GetProcessHeap(), 0, ret);
    GetOutlineTextMetricsW(hdc, ret, lpOTMW);

    needed = sizeof(OUTLINETEXTMETRICA);
    if(lpOTMW->otmpFamilyName)
        needed += WideCharToMultiByte(CP_ACP, 0,
	   (WCHAR*)((char*)lpOTMW + (ptrdiff_t)lpOTMW->otmpFamilyName), -1,
				      NULL, 0, NULL, NULL);
    if(lpOTMW->otmpFaceName)
        needed += WideCharToMultiByte(CP_ACP, 0,
	   (WCHAR*)((char*)lpOTMW + (ptrdiff_t)lpOTMW->otmpFaceName), -1,
				      NULL, 0, NULL, NULL);
    if(lpOTMW->otmpStyleName)
        needed += WideCharToMultiByte(CP_ACP, 0,
	   (WCHAR*)((char*)lpOTMW + (ptrdiff_t)lpOTMW->otmpStyleName), -1,
				      NULL, 0, NULL, NULL);
    if(lpOTMW->otmpFullName)
        needed += WideCharToMultiByte(CP_ACP, 0,
	   (WCHAR*)((char*)lpOTMW + (ptrdiff_t)lpOTMW->otmpFullName), -1,
				      NULL, 0, NULL, NULL);

    if(!lpOTM) {
        ret = needed;
	goto end;
    }

    TRACE("needed = %d\n", needed);
    if(needed > cbData)
        /* Since the supplied buffer isn't big enough, we'll alloc one
           that is and memcpy the first cbData bytes into the lpOTM at
           the end. */
        output = HeapAlloc(GetProcessHeap(), 0, needed);

    ret = output->otmSize = min(needed, cbData);
    FONT_TextMetricWToA( &lpOTMW->otmTextMetrics, &output->otmTextMetrics );
    output->otmFiller = 0;
    output->otmPanoseNumber = lpOTMW->otmPanoseNumber;
    output->otmfsSelection = lpOTMW->otmfsSelection;
    output->otmfsType = lpOTMW->otmfsType;
    output->otmsCharSlopeRise = lpOTMW->otmsCharSlopeRise;
    output->otmsCharSlopeRun = lpOTMW->otmsCharSlopeRun;
    output->otmItalicAngle = lpOTMW->otmItalicAngle;
    output->otmEMSquare = lpOTMW->otmEMSquare;
    output->otmAscent = lpOTMW->otmAscent;
    output->otmDescent = lpOTMW->otmDescent;
    output->otmLineGap = lpOTMW->otmLineGap;
    output->otmsCapEmHeight = lpOTMW->otmsCapEmHeight;
    output->otmsXHeight = lpOTMW->otmsXHeight;
    output->otmrcFontBox = lpOTMW->otmrcFontBox;
    output->otmMacAscent = lpOTMW->otmMacAscent;
    output->otmMacDescent = lpOTMW->otmMacDescent;
    output->otmMacLineGap = lpOTMW->otmMacLineGap;
    output->otmusMinimumPPEM = lpOTMW->otmusMinimumPPEM;
    output->otmptSubscriptSize = lpOTMW->otmptSubscriptSize;
    output->otmptSubscriptOffset = lpOTMW->otmptSubscriptOffset;
    output->otmptSuperscriptSize = lpOTMW->otmptSuperscriptSize;
    output->otmptSuperscriptOffset = lpOTMW->otmptSuperscriptOffset;
    output->otmsStrikeoutSize = lpOTMW->otmsStrikeoutSize;
    output->otmsStrikeoutPosition = lpOTMW->otmsStrikeoutPosition;
    output->otmsUnderscoreSize = lpOTMW->otmsUnderscoreSize;
    output->otmsUnderscorePosition = lpOTMW->otmsUnderscorePosition;


    ptr = (char*)(output + 1);
    left = needed - sizeof(*output);

    if(lpOTMW->otmpFamilyName) {
        output->otmpFamilyName = (LPSTR)(ptr - (char*)output);
	len = WideCharToMultiByte(CP_ACP, 0,
	     (WCHAR*)((char*)lpOTMW + (ptrdiff_t)lpOTMW->otmpFamilyName), -1,
				  ptr, left, NULL, NULL);
	left -= len;
	ptr += len;
    } else
        output->otmpFamilyName = 0;

    if(lpOTMW->otmpFaceName) {
        output->otmpFaceName = (LPSTR)(ptr - (char*)output);
	len = WideCharToMultiByte(CP_ACP, 0,
	     (WCHAR*)((char*)lpOTMW + (ptrdiff_t)lpOTMW->otmpFaceName), -1,
				  ptr, left, NULL, NULL);
	left -= len;
	ptr += len;
    } else
        output->otmpFaceName = 0;

    if(lpOTMW->otmpStyleName) {
        output->otmpStyleName = (LPSTR)(ptr - (char*)output);
	len = WideCharToMultiByte(CP_ACP, 0,
	     (WCHAR*)((char*)lpOTMW + (ptrdiff_t)lpOTMW->otmpStyleName), -1,
				  ptr, left, NULL, NULL);
	left -= len;
	ptr += len;
    } else
        output->otmpStyleName = 0;

    if(lpOTMW->otmpFullName) {
        output->otmpFullName = (LPSTR)(ptr - (char*)output);
	len = WideCharToMultiByte(CP_ACP, 0,
	     (WCHAR*)((char*)lpOTMW + (ptrdiff_t)lpOTMW->otmpFullName), -1,
				  ptr, left, NULL, NULL);
	left -= len;
    } else
        output->otmpFullName = 0;

    assert(left == 0);

    if(output != lpOTM) {
        memcpy(lpOTM, output, cbData);
        HeapFree(GetProcessHeap(), 0, output);
    }

end:
    if(lpOTMW != (OUTLINETEXTMETRICW *)buf)
        HeapFree(GetProcessHeap(), 0, lpOTMW);

    return ret;
}


/***********************************************************************
 *           GetOutlineTextMetricsW [GDI32.@]
 */
UINT WINAPI GetOutlineTextMetricsW(
    HDC hdc,    /* [in]  Handle of device context */
    UINT cbData, /* [in]  Size of metric data array */
    LPOUTLINETEXTMETRICW lpOTM)  /* [out] Address of metric data array */
{
    DC *dc = DC_GetDCPtr( hdc );
    OUTLINETEXTMETRICW *output = lpOTM;
    UINT ret;

    TRACE("(%p,%d,%p)\n", hdc, cbData, lpOTM);
    if(!dc) return 0;

    if(dc->gdiFont) {
        ret = WineEngGetOutlineTextMetrics(dc->gdiFont, cbData, output);
        if(lpOTM && ret) {
            if(ret > cbData) {
                output = HeapAlloc(GetProcessHeap(), 0, ret);
                WineEngGetOutlineTextMetrics(dc->gdiFont, ret, output);
            }

#define WDPTOLP(x) ((x<0)?					\
		(-abs(INTERNAL_XDSTOWS(dc, (x)))):		\
		(abs(INTERNAL_XDSTOWS(dc, (x)))))
#define HDPTOLP(y) ((y<0)?					\
		(-abs(INTERNAL_YDSTOWS(dc, (y)))):		\
		(abs(INTERNAL_YDSTOWS(dc, (y)))))

	    output->otmTextMetrics.tmHeight           = HDPTOLP(output->otmTextMetrics.tmHeight);
	    output->otmTextMetrics.tmAscent           = HDPTOLP(output->otmTextMetrics.tmAscent);
	    output->otmTextMetrics.tmDescent          = HDPTOLP(output->otmTextMetrics.tmDescent);
	    output->otmTextMetrics.tmInternalLeading  = HDPTOLP(output->otmTextMetrics.tmInternalLeading);
	    output->otmTextMetrics.tmExternalLeading  = HDPTOLP(output->otmTextMetrics.tmExternalLeading);
	    output->otmTextMetrics.tmAveCharWidth     = WDPTOLP(output->otmTextMetrics.tmAveCharWidth);
	    output->otmTextMetrics.tmMaxCharWidth     = WDPTOLP(output->otmTextMetrics.tmMaxCharWidth);
	    output->otmTextMetrics.tmOverhang         = WDPTOLP(output->otmTextMetrics.tmOverhang);
	    output->otmAscent = HDPTOLP(output->otmAscent);
	    output->otmDescent = HDPTOLP(output->otmDescent);
	    output->otmLineGap = HDPTOLP(output->otmLineGap);
	    output->otmsCapEmHeight = HDPTOLP(output->otmsCapEmHeight);
	    output->otmsXHeight = HDPTOLP(output->otmsXHeight);
	    output->otmrcFontBox.top = HDPTOLP(output->otmrcFontBox.top);
	    output->otmrcFontBox.bottom = HDPTOLP(output->otmrcFontBox.bottom);
	    output->otmrcFontBox.left = WDPTOLP(output->otmrcFontBox.left);
	    output->otmrcFontBox.right = WDPTOLP(output->otmrcFontBox.right);
	    output->otmMacAscent = HDPTOLP(output->otmMacAscent);
	    output->otmMacDescent = HDPTOLP(output->otmMacDescent);
	    output->otmMacLineGap = HDPTOLP(output->otmMacLineGap);
	    output->otmptSubscriptSize.x = WDPTOLP(output->otmptSubscriptSize.x);
	    output->otmptSubscriptSize.y = HDPTOLP(output->otmptSubscriptSize.y);
	    output->otmptSubscriptOffset.x = WDPTOLP(output->otmptSubscriptOffset.x);
	    output->otmptSubscriptOffset.y = HDPTOLP(output->otmptSubscriptOffset.y);
	    output->otmptSuperscriptSize.x = WDPTOLP(output->otmptSuperscriptSize.x);
	    output->otmptSuperscriptSize.y = HDPTOLP(output->otmptSuperscriptSize.y);
	    output->otmptSuperscriptOffset.x = WDPTOLP(output->otmptSuperscriptOffset.x);
	    output->otmptSuperscriptOffset.y = HDPTOLP(output->otmptSuperscriptOffset.y);
	    output->otmsStrikeoutSize = HDPTOLP(output->otmsStrikeoutSize);
	    output->otmsStrikeoutPosition = HDPTOLP(output->otmsStrikeoutPosition);
	    output->otmsUnderscoreSize = HDPTOLP(output->otmsUnderscoreSize);
	    output->otmsUnderscorePosition = HDPTOLP(output->otmsUnderscorePosition);
#undef WDPTOLP
#undef HDPTOLP
            if(output != lpOTM) {
                memcpy(lpOTM, output, cbData);
                HeapFree(GetProcessHeap(), 0, output);
                ret = cbData;
            }
	}
    }

    else { /* This stuff was in GetOutlineTextMetricsA, I've moved it here
	      but really this should just be a return 0. */

        ret = sizeof(*lpOTM);
        if (lpOTM) {
	    if(cbData < ret)
	        ret = 0;
	    else {
	        memset(lpOTM, 0, ret);
		lpOTM->otmSize = sizeof(*lpOTM);
		GetTextMetricsW(hdc, &lpOTM->otmTextMetrics);
		/*
		  Further fill of the structure not implemented,
		  Needs real values for the structure members
		*/
	    }
	}
    }
    GDI_ReleaseObj(hdc);
    return ret;
}


/***********************************************************************
 *           GetCharWidthW      (GDI32.@)
 *           GetCharWidth32W    (GDI32.@)
 */
BOOL WINAPI GetCharWidth32W( HDC hdc, UINT firstChar, UINT lastChar,
                               LPINT buffer )
{
    UINT i;
    BOOL ret = FALSE;
    DC * dc = DC_GetDCPtr( hdc );
    if (!dc) return FALSE;

    if (dc->gdiFont)
        ret = WineEngGetCharWidth( dc->gdiFont, firstChar, lastChar, buffer );
    else if (dc->funcs->pGetCharWidth)
        ret = dc->funcs->pGetCharWidth( dc->physDev, firstChar, lastChar, buffer);

    if (ret)
    {
        /* convert device units to logical */
        for( i = firstChar; i <= lastChar; i++, buffer++ )
            *buffer = INTERNAL_XDSTOWS(dc, *buffer);
        ret = TRUE;
    }
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           GetCharWidthA      (GDI32.@)
 *           GetCharWidth32A    (GDI32.@)
 */
BOOL WINAPI GetCharWidth32A( HDC hdc, UINT firstChar, UINT lastChar,
                               LPINT buffer )
{
    INT i, wlen, count = (INT)(lastChar - firstChar + 1);
    LPSTR str;
    LPWSTR wstr;
    BOOL ret = TRUE;

    if(count <= 0) return FALSE;

    str = HeapAlloc(GetProcessHeap(), 0, count);
    for(i = 0; i < count; i++)
	str[i] = (BYTE)(firstChar + i);

    wstr = FONT_mbtowc(hdc, str, count, &wlen, NULL);

    for(i = 0; i < wlen; i++)
    {
	if(!GetCharWidth32W(hdc, wstr[i], wstr[i], buffer))
	{
	    ret = FALSE;
	    break;
	}
	buffer++;
    }

    HeapFree(GetProcessHeap(), 0, str);
    HeapFree(GetProcessHeap(), 0, wstr);

    return ret;
}


/* FIXME: all following APIs ******************************************/


/***********************************************************************
 *           SetMapperFlags    (GDI32.@)
 */
DWORD WINAPI SetMapperFlags( HDC hDC, DWORD dwFlag )
{
    DC *dc = DC_GetDCPtr( hDC );
    DWORD ret = 0;
    if(!dc) return 0;
    if(dc->funcs->pSetMapperFlags)
        ret = dc->funcs->pSetMapperFlags( dc->physDev, dwFlag );
    else
        FIXME("(%p, 0x%08lx): stub - harmless\n", hDC, dwFlag);
    GDI_ReleaseObj( hDC );
    return ret;
}

/***********************************************************************
 *          GetAspectRatioFilterEx  (GDI.486)
 */
BOOL16 WINAPI GetAspectRatioFilterEx16( HDC16 hdc, LPSIZE16 pAspectRatio )
{
  FIXME("(%04x, %p): -- Empty Stub !\n", hdc, pAspectRatio);
  return FALSE;
}

/***********************************************************************
 *          GetAspectRatioFilterEx  (GDI32.@)
 */
BOOL WINAPI GetAspectRatioFilterEx( HDC hdc, LPSIZE pAspectRatio )
{
  FIXME("(%p, %p): -- Empty Stub !\n", hdc, pAspectRatio);
  return FALSE;
}


/***********************************************************************
 *           GetCharABCWidthsA   (GDI32.@)
 */
BOOL WINAPI GetCharABCWidthsA(HDC hdc, UINT firstChar, UINT lastChar,
                                  LPABC abc )
{
    INT i, wlen, count = (INT)(lastChar - firstChar + 1);
    LPSTR str;
    LPWSTR wstr;
    BOOL ret = TRUE;

    if(count <= 0) return FALSE;

    str = HeapAlloc(GetProcessHeap(), 0, count);
    for(i = 0; i < count; i++)
	str[i] = (BYTE)(firstChar + i);

    wstr = FONT_mbtowc(hdc, str, count, &wlen, NULL);

    for(i = 0; i < wlen; i++)
    {
	if(!GetCharABCWidthsW(hdc, wstr[i], wstr[i], abc))
	{
	    ret = FALSE;
	    break;
	}
	abc++;
    }

    HeapFree(GetProcessHeap(), 0, str);
    HeapFree(GetProcessHeap(), 0, wstr);

    return ret;
}


/******************************************************************************
 * GetCharABCWidthsW [GDI32.@]
 *
 * Retrieves widths of characters in range.
 *
 * PARAMS
 *    hdc       [I] Handle of device context
 *    firstChar [I] First character in range to query
 *    lastChar  [I] Last character in range to query
 *    abc       [O] Address of character-width structure
 *
 * NOTES
 *    Only works with TrueType fonts
 *
 * RETURNS
 *    Success: TRUE
 *    Failure: FALSE
 */
BOOL WINAPI GetCharABCWidthsW( HDC hdc, UINT firstChar, UINT lastChar,
                                   LPABC abc )
{
    DC *dc = DC_GetDCPtr(hdc);
    int	i;
    BOOL ret = FALSE;

    if(dc->gdiFont)
        ret = WineEngGetCharABCWidths( dc->gdiFont, firstChar, lastChar, abc );
    else
        FIXME(": stub\n");

    if (ret)
    {
        /* convert device units to logical */
        for( i = firstChar; i <= lastChar; i++, abc++ ) {
            abc->abcA = INTERNAL_XDSTOWS(dc, abc->abcA);
            abc->abcB = INTERNAL_XDSTOWS(dc, abc->abcB);
            abc->abcC = INTERNAL_XDSTOWS(dc, abc->abcC);
	}
        ret = TRUE;
    }

    GDI_ReleaseObj(hdc);
    return ret;
}


/***********************************************************************
 *           GetGlyphOutline    (GDI.309)
 */
DWORD WINAPI GetGlyphOutline16( HDC16 hdc, UINT16 uChar, UINT16 fuFormat,
                                LPGLYPHMETRICS16 lpgm, DWORD cbBuffer,
                                LPVOID lpBuffer, const MAT2 *lpmat2 )
{
    FIXME("(%04x, '%c', %04x, %p, %ld, %p, %p): stub\n",
	  hdc, uChar, fuFormat, lpgm, cbBuffer, lpBuffer, lpmat2 );
    return ~0UL; /* failure */
}


/***********************************************************************
 *           GetGlyphOutlineA    (GDI32.@)
 */
DWORD WINAPI GetGlyphOutlineA( HDC hdc, UINT uChar, UINT fuFormat,
                                 LPGLYPHMETRICS lpgm, DWORD cbBuffer,
                                 LPVOID lpBuffer, const MAT2 *lpmat2 )
{
    LPWSTR p = NULL;
    DWORD ret;
    UINT c;

    if(!(fuFormat & GGO_GLYPH_INDEX)) {
        p = FONT_mbtowc(hdc, (char*)&uChar, 1, NULL, NULL);
	c = p[0];
    } else
        c = uChar;
    ret = GetGlyphOutlineW(hdc, c, fuFormat, lpgm, cbBuffer, lpBuffer,
			   lpmat2);
    if(p)
        HeapFree(GetProcessHeap(), 0, p);
    return ret;
}

/***********************************************************************
 *           GetGlyphOutlineW    (GDI32.@)
 */
DWORD WINAPI GetGlyphOutlineW( HDC hdc, UINT uChar, UINT fuFormat,
                                 LPGLYPHMETRICS lpgm, DWORD cbBuffer,
                                 LPVOID lpBuffer, const MAT2 *lpmat2 )
{
    DC *dc = DC_GetDCPtr(hdc);
    DWORD ret;

    TRACE("(%p, %04x, %04x, %p, %ld, %p, %p)\n",
	  hdc, uChar, fuFormat, lpgm, cbBuffer, lpBuffer, lpmat2 );

    if(!dc) return GDI_ERROR;

    if(dc->gdiFont)
      ret = WineEngGetGlyphOutline(dc->gdiFont, uChar, fuFormat, lpgm,
				   cbBuffer, lpBuffer, lpmat2);
    else
      ret = GDI_ERROR;

    GDI_ReleaseObj(hdc);
    return ret;
}


/***********************************************************************
 *           CreateScalableFontResourceA   (GDI32.@)
 */
BOOL WINAPI CreateScalableFontResourceA( DWORD fHidden,
                                             LPCSTR lpszResourceFile,
                                             LPCSTR lpszFontFile,
                                             LPCSTR lpszCurrentPath )
{
    HANDLE f;

    /* fHidden=1 - only visible for the calling app, read-only, not
     * enumbered with EnumFonts/EnumFontFamilies
     * lpszCurrentPath can be NULL
     */
    FIXME("(%ld,%s,%s,%s): stub\n",
          fHidden, debugstr_a(lpszResourceFile), debugstr_a(lpszFontFile),
          debugstr_a(lpszCurrentPath) );

    /* If the output file already exists, return the ERROR_FILE_EXISTS error as specified in MSDN */
    if ((f = CreateFileA(lpszResourceFile, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)) != INVALID_HANDLE_VALUE) {
        CloseHandle(f);
        SetLastError(ERROR_FILE_EXISTS);
        return FALSE;
    }
    return FALSE; /* create failed */
}

/***********************************************************************
 *           CreateScalableFontResourceW   (GDI32.@)
 */
BOOL WINAPI CreateScalableFontResourceW( DWORD fHidden,
                                             LPCWSTR lpszResourceFile,
                                             LPCWSTR lpszFontFile,
                                             LPCWSTR lpszCurrentPath )
{
    FIXME("(%ld,%p,%p,%p): stub\n",
	  fHidden, lpszResourceFile, lpszFontFile, lpszCurrentPath );
    return FALSE; /* create failed */
}


/*************************************************************************
 *             GetRasterizerCaps   (GDI32.@)
 */
BOOL WINAPI GetRasterizerCaps( LPRASTERIZER_STATUS lprs, UINT cbNumBytes)
{
  lprs->nSize = sizeof(RASTERIZER_STATUS);
  lprs->wFlags = TT_AVAILABLE|TT_ENABLED;
  lprs->nLanguageID = 0;
  return TRUE;
}


/*************************************************************************
 *             GetKerningPairsA   (GDI32.@)
 */
DWORD WINAPI GetKerningPairsA( HDC hDC, DWORD cPairs,
                               LPKERNINGPAIR lpKerningPairs )
{
    return GetKerningPairsW( hDC, cPairs, lpKerningPairs );
}


/*************************************************************************
 *             GetKerningPairsW   (GDI32.@)
 */
DWORD WINAPI GetKerningPairsW( HDC hDC, DWORD cPairs,
                                 LPKERNINGPAIR lpKerningPairs )
{
    int i;
    FIXME("(%p,%ld,%p): almost empty stub!\n", hDC, cPairs, lpKerningPairs);
    for (i = 0; i < cPairs; i++)
        lpKerningPairs[i].iKernAmount = 0;
    return 0;
}

/*************************************************************************
 * TranslateCharsetInfo [GDI32.@]
 *
 * Fills a CHARSETINFO structure for a character set, code page, or
 * font. This allows making the correspondance between different labelings
 * (character set, Windows, ANSI, and OEM codepages, and Unicode ranges)
 * of the same encoding.
 *
 * Only one codepage will be set in lpCs->fs. If TCI_SRCFONTSIG is used,
 * only one codepage should be set in *lpSrc.
 *
 * RETURNS
 *   TRUE on success, FALSE on failure.
 *
 */
BOOL WINAPI TranslateCharsetInfo(
  LPDWORD lpSrc, /* [in]
       if flags == TCI_SRCFONTSIG: pointer to fsCsb of a FONTSIGNATURE
       if flags == TCI_SRCCHARSET: a character set value
       if flags == TCI_SRCCODEPAGE: a code page value
		 */
  LPCHARSETINFO lpCs, /* [out] structure to receive charset information */
  DWORD flags /* [in] determines interpretation of lpSrc */)
{
    int index = 0;
    switch (flags) {
    case TCI_SRCFONTSIG:
	while (!(*lpSrc>>index & 0x0001) && index<MAXTCIINDEX) index++;
      break;
    case TCI_SRCCODEPAGE:
      while ((UINT) (lpSrc) != FONT_tci[index].ciACP && index < MAXTCIINDEX) index++;
      break;
    case TCI_SRCCHARSET:
      while ((UINT) (lpSrc) != FONT_tci[index].ciCharset && index < MAXTCIINDEX) index++;
      break;
    default:
      return FALSE;
    }
    if (index >= MAXTCIINDEX || FONT_tci[index].ciCharset == DEFAULT_CHARSET) return FALSE;
    memcpy(lpCs, &FONT_tci[index], sizeof(CHARSETINFO));
    return TRUE;
}

/*************************************************************************
 *             GetFontLanguageInfo   (GDI32.@)
 */
DWORD WINAPI GetFontLanguageInfo(HDC hdc)
{
	FONTSIGNATURE fontsig;
	static const DWORD GCP_DBCS_MASK=0x003F0000,
		GCP_DIACRITIC_MASK=0x00000000,
		FLI_GLYPHS_MASK=0x00000000,
		GCP_GLYPHSHAPE_MASK=0x00000040,
		GCP_KASHIDA_MASK=0x00000000,
		GCP_LIGATE_MASK=0x00000000,
		GCP_USEKERNING_MASK=0x00000000,
		GCP_REORDER_MASK=0x00000060;

	DWORD result=0;

	GetTextCharsetInfo( hdc, &fontsig, 0 );
	/* We detect each flag we return using a bitmask on the Codepage Bitfields */

	if( (fontsig.fsCsb[0]&GCP_DBCS_MASK)!=0 )
		result|=GCP_DBCS;

	if( (fontsig.fsCsb[0]&GCP_DIACRITIC_MASK)!=0 )
		result|=GCP_DIACRITIC;

	if( (fontsig.fsCsb[0]&FLI_GLYPHS_MASK)!=0 )
		result|=FLI_GLYPHS;

	if( (fontsig.fsCsb[0]&GCP_GLYPHSHAPE_MASK)!=0 )
		result|=GCP_GLYPHSHAPE;

	if( (fontsig.fsCsb[0]&GCP_KASHIDA_MASK)!=0 )
		result|=GCP_KASHIDA;

	if( (fontsig.fsCsb[0]&GCP_LIGATE_MASK)!=0 )
		result|=GCP_LIGATE;

	if( (fontsig.fsCsb[0]&GCP_USEKERNING_MASK)!=0 )
		result|=GCP_USEKERNING;

	if( (fontsig.fsCsb[0]&GCP_REORDER_MASK)!=0 )
		result|=GCP_REORDER;

	return result;
}


/*************************************************************************
 * GetFontData [GDI32.@]
 *
 * Retrieve data for TrueType font.
 *
 * RETURNS
 *
 * success: Number of bytes returned
 * failure: GDI_ERROR
 *
 * NOTES
 *
 * Calls SetLastError()
 *
 */
DWORD WINAPI GetFontData(HDC hdc, DWORD table, DWORD offset,
    LPVOID buffer, DWORD length)
{
    DC *dc = DC_GetDCPtr(hdc);
    DWORD ret = GDI_ERROR;

    if(!dc) return GDI_ERROR;

    if(dc->gdiFont)
      ret = WineEngGetFontData(dc->gdiFont, table, offset, buffer, length);

    GDI_ReleaseObj(hdc);
    return ret;
}

/*************************************************************************
 * GetGlyphIndicesA [GDI32.@]
 */
DWORD WINAPI GetGlyphIndicesA(HDC hdc, LPCSTR lpstr, INT count,
			      LPWORD pgi, DWORD flags)
{
    DWORD ret;
    WCHAR *lpstrW;
    INT countW;

    TRACE("(%p, %s, %d, %p, 0x%lx)\n",
          hdc, debugstr_an(lpstr, count), count, pgi, flags);

    lpstrW = FONT_mbtowc(hdc, lpstr, count, &countW, NULL);
    ret = GetGlyphIndicesW(hdc, lpstrW, countW, pgi, flags);
    HeapFree(GetProcessHeap(), 0, lpstrW);

    return ret;
}

/*************************************************************************
 * GetGlyphIndicesW [GDI32.@]
 */
DWORD WINAPI GetGlyphIndicesW(HDC hdc, LPCWSTR lpstr, INT count,
			      LPWORD pgi, DWORD flags)
{
    DC *dc = DC_GetDCPtr(hdc);
    DWORD ret = GDI_ERROR;

    TRACE("(%p, %s, %d, %p, 0x%lx)\n",
          hdc, debugstr_wn(lpstr, count), count, pgi, flags);

    if(!dc) return GDI_ERROR;

    if(dc->gdiFont)
	ret = WineEngGetGlyphIndices(dc->gdiFont, lpstr, count, pgi, flags);

    GDI_ReleaseObj(hdc);
    return ret;
}

/*************************************************************************
 * GetCharacterPlacementA [GDI32.@]
 *
 * NOTES:
 *  the web browser control of ie4 calls this with dwFlags=0
 */
DWORD WINAPI
GetCharacterPlacementA(HDC hdc, LPCSTR lpString, INT uCount,
			 INT nMaxExtent, GCP_RESULTSA *lpResults,
			 DWORD dwFlags)
{
    WCHAR *lpStringW;
    INT uCountW;
    GCP_RESULTSW resultsW;
    DWORD ret;
    UINT font_cp;

    TRACE("%s, %d, %d, 0x%08lx\n",
          debugstr_an(lpString, uCount), uCount, nMaxExtent, dwFlags);

    /* both structs are equal in size */
    memcpy(&resultsW, lpResults, sizeof(resultsW));

    lpStringW = FONT_mbtowc(hdc, lpString, uCount, &uCountW, &font_cp);
    if(lpResults->lpOutString)
        resultsW.lpOutString = HeapAlloc(GetProcessHeap(), 0, sizeof(WCHAR)*uCountW);

    ret = GetCharacterPlacementW(hdc, lpStringW, uCountW, nMaxExtent, &resultsW, dwFlags);

    if(lpResults->lpOutString) {
        WideCharToMultiByte(font_cp, 0, resultsW.lpOutString, uCountW,
                            lpResults->lpOutString, uCount, NULL, NULL );
    }

    HeapFree(GetProcessHeap(), 0, lpStringW);
    HeapFree(GetProcessHeap(), 0, resultsW.lpOutString);

    return ret;
}

/*************************************************************************
 * GetCharacterPlacementW [GDI32.@]
 *
 *   Retrieve information about a string. This includes the width, reordering,
 *   Glyphing and so on.
 *
 * RETURNS
 *
 *   The width and height of the string if successful, 0 if failed.
 *
 * BUGS
 *
 *   All flags except GCP_REORDER are not yet implemented.
 *   Reordering is not 100% complient to the Windows BiDi method.
 *   Caret positioning is not yet implemented for BiDi.
 *   Classes are not yet implemented.
 *
 */
DWORD WINAPI
GetCharacterPlacementW(
		HDC hdc,		/* [in] Device context for which the rendering is to be done */
		LPCWSTR lpString,	/* [in] The string for which information is to be returned */
		INT uCount,		/* [in] Number of WORDS in string. */
		INT nMaxExtent,		/* [in] Maximum extent the string is to take (in HDC logical units) */
		GCP_RESULTSW *lpResults,/* [in/out] A pointer to a GCP_RESULTSW struct */
		DWORD dwFlags 		/* [in] Flags specifying how to process the string */
		)
{
    DWORD ret=0;
    SIZE size;
    UINT i, nSet;

    TRACE("%s, %d, %d, 0x%08lx\n",
          debugstr_wn(lpString, uCount), uCount, nMaxExtent, dwFlags);

    TRACE("lStructSize=%ld, lpOutString=%p, lpOrder=%p, lpDx=%p, lpCaretPos=%p\n"
	  "lpClass=%p, lpGlyphs=%p, nGlyphs=%u, nMaxFit=%d\n",
	    lpResults->lStructSize, lpResults->lpOutString, lpResults->lpOrder,
	    lpResults->lpDx, lpResults->lpCaretPos, lpResults->lpClass,
	    lpResults->lpGlyphs, lpResults->nGlyphs, lpResults->nMaxFit);

    if(dwFlags&(~GCP_REORDER))			FIXME("flags 0x%08lx ignored\n", dwFlags);
    if(lpResults->lpClass)	FIXME("classes not implemented\n");
    if (lpResults->lpCaretPos && (dwFlags & GCP_REORDER))
        FIXME("Caret positions for complex scripts not implemented\n");

	nSet = (UINT)uCount;
	if(nSet > lpResults->nGlyphs)
		nSet = lpResults->nGlyphs;

	/* return number of initialized fields */
	lpResults->nGlyphs = nSet;

	if((dwFlags&GCP_REORDER)==0 || !BidiAvail)
	{
		/* Treat the case where no special handling was requested in a fastpath way */
		/* copy will do if the GCP_REORDER flag is not set */
		if(lpResults->lpOutString)
                    strncpyW( lpResults->lpOutString, lpString, nSet );

		if(lpResults->lpOrder)
		{
			for(i = 0; i < nSet; i++)
				lpResults->lpOrder[i] = i;
		}
	} else
	{
            BIDI_Reorder( lpString, uCount, dwFlags, WINE_GCPW_FORCE_LTR, lpResults->lpOutString,
                          nSet, lpResults->lpOrder );
	}

	/* FIXME: Will use the placement chars */
	if (lpResults->lpDx)
	{
		int c;
		for (i = 0; i < nSet; i++)
		{
			if (GetCharWidth32W(hdc, lpString[i], lpString[i], &c))
				lpResults->lpDx[i]= c;
		}
	}

    if (lpResults->lpCaretPos && !(dwFlags & GCP_REORDER))
    {
        int pos = 0;
       
        lpResults->lpCaretPos[0] = 0;
        for (i = 1; i < nSet; i++)
            if (GetTextExtentPoint32W(hdc, &(lpString[i - 1]), 1, &size))
                lpResults->lpCaretPos[i] = (pos += size.cx);
    }
   
    if(lpResults->lpGlyphs)
	GetGlyphIndicesW(hdc, lpString, nSet, lpResults->lpGlyphs, 0);

    if (GetTextExtentPoint32W(hdc, lpString, uCount, &size))
      ret = MAKELONG(size.cx, size.cy);

    return ret;
}

/*************************************************************************
 *      GetCharABCWidthsFloatA [GDI32.@]
 */
BOOL WINAPI GetCharABCWidthsFloatA(HDC hdc, UINT iFirstChar, UINT iLastChar,
		                        LPABCFLOAT lpABCF)
{
       FIXME_(gdi)("GetCharABCWidthsFloatA, stub\n");
       return 0;
}

/*************************************************************************
 *      GetCharABCWidthsFloatW [GDI32.@]
 */
BOOL WINAPI GetCharABCWidthsFloatW(HDC hdc, UINT iFirstChar,
		                        UINT iLastChar, LPABCFLOAT lpABCF)
{
       FIXME_(gdi)("GetCharABCWidthsFloatW, stub\n");
       return 0;
}

/*************************************************************************
 *      GetCharWidthFloatA [GDI32.@]
 */
BOOL WINAPI GetCharWidthFloatA(HDC hdc, UINT iFirstChar,
		                    UINT iLastChar, PFLOAT pxBuffer)
{
       FIXME_(gdi)("GetCharWidthFloatA, stub\n");
       return 0;
}

/*************************************************************************
 *      GetCharWidthFloatW [GDI32.@]
 */
BOOL WINAPI GetCharWidthFloatW(HDC hdc, UINT iFirstChar,
		                    UINT iLastChar, PFLOAT pxBuffer)
{
       FIXME_(gdi)("GetCharWidthFloatW, stub\n");
       return 0;
}


/***********************************************************************
 *								       *
 *           Font Resource API					       *
 *								       *
 ***********************************************************************/

/***********************************************************************
 *           AddFontResourceA    (GDI32.@)
 */
INT WINAPI AddFontResourceA( LPCSTR str )
{
    return AddFontResourceExA( str, 0, NULL);
}

/***********************************************************************
 *           AddFontResourceW    (GDI32.@)
 */
INT WINAPI AddFontResourceW( LPCWSTR str )
{
    return AddFontResourceExW(str, 0, NULL);
}


/***********************************************************************
 *           AddFontResourceExA    (GDI32.@)
 */
INT WINAPI AddFontResourceExA( LPCSTR str, DWORD fl, PVOID pdv )
{
    DWORD len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    LPWSTR strW = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
    INT ret;

    MultiByteToWideChar(CP_ACP, 0, str, -1, strW, len);
    ret = AddFontResourceExW(strW, fl, pdv);
    HeapFree(GetProcessHeap(), 0, strW);
    return ret;
}

/***********************************************************************
 *           AddFontResourceExW    (GDI32.@)
 */
INT WINAPI AddFontResourceExW( LPCWSTR str, DWORD fl, PVOID pdv )
{
    return WineEngAddFontResourceEx(str, fl, pdv);
}

/***********************************************************************
 *           RemoveFontResourceA    (GDI32.@)
 */
BOOL WINAPI RemoveFontResourceA( LPCSTR str )
{
    return RemoveFontResourceExA(str, 0, 0);
}

/***********************************************************************
 *           RemoveFontResourceW    (GDI32.@)
 */
BOOL WINAPI RemoveFontResourceW( LPCWSTR str )
{
    return RemoveFontResourceExW(str, 0, 0);
}

/***********************************************************************
 *           RemoveFontResourceExA    (GDI32.@)
 */
BOOL WINAPI RemoveFontResourceExA( LPCSTR str, DWORD fl, PVOID pdv )
{
    DWORD len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    LPWSTR strW = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
    INT ret;

    MultiByteToWideChar(CP_ACP, 0, str, -1, strW, len);
    ret = RemoveFontResourceExW(strW, fl, pdv);
    HeapFree(GetProcessHeap(), 0, strW);
    return ret;
}

/***********************************************************************
 *           RemoveFontResourceExW    (GDI32.@)
 */
BOOL WINAPI RemoveFontResourceExW( LPCWSTR str, DWORD fl, PVOID pdv )
{
    return WineEngRemoveFontResourceEx(str, fl, pdv);
}

/***********************************************************************
 *           GetTextCharset    (GDI32.@)
 */
UINT WINAPI GetTextCharset(HDC hdc)
{
    /* MSDN docs say this is equivalent */
    return GetTextCharsetInfo(hdc, NULL, 0);
}

/***********************************************************************
 *           GetTextCharsetInfo    (GDI32.@)
 */
UINT WINAPI GetTextCharsetInfo(HDC hdc, LPFONTSIGNATURE fs, DWORD flags)
{
    UINT ret = DEFAULT_CHARSET;
    DC *dc = DC_GetDCPtr(hdc);

    if (!dc) goto done;

    if (dc->gdiFont)
        ret = WineEngGetTextCharsetInfo(dc->gdiFont, fs, flags);

    GDI_ReleaseObj(hdc);

done:
    if (ret == DEFAULT_CHARSET && fs)
        memset(fs, 0, sizeof(FONTSIGNATURE));
    return ret;
}
