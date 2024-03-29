/*
 *	Font metric functions common to Type 1 (AFM) and TrueType font files.
 *  	Functions specific to Type 1 and TrueType fonts are in type1afm.c and
 *  	truetype.c respectively.
 *
 *	Copyright 1998  Huw D M Davies
 *  	Copyright 2001  Ian Pilcher
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

#include <string.h>

#include "psdrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(psdrv);

/* ptr to fonts for which we have afm files */
FONTFAMILY *PSDRV_AFMFontList = NULL;


/***********************************************************
 *
 *	PSDRV_FreeAFMList
 *
 * Frees the family and afmlistentry structures in list head
 */
void PSDRV_FreeAFMList( FONTFAMILY *head )
{
    AFMLISTENTRY *afmle, *nexta;
    FONTFAMILY *family, *nextf;

    for(nextf = family = head; nextf; family = nextf) {
        for(nexta = afmle = family->afmlist; nexta; afmle = nexta) {
	    nexta = afmle->next;
	    HeapFree( PSDRV_Heap, 0, afmle );
	}
        nextf = family->next;
	HeapFree( PSDRV_Heap, 0, family );
    }
    return;
}


/***********************************************************
 *
 *	PSDRV_FindAFMinList
 * Returns ptr to an AFM if name (which is a PS font name) exists in list
 * headed by head.
 */
const AFM *PSDRV_FindAFMinList(FONTFAMILY *head, LPCSTR name)
{
    FONTFAMILY *family;
    AFMLISTENTRY *afmle;

    for(family = head; family; family = family->next) {
        for(afmle = family->afmlist; afmle; afmle = afmle->next) {
	    if(!strcmp(afmle->afm->FontName, name))
	        return afmle->afm;
	}
    }
    return NULL;
}

/***********************************************************
 *
 *	PSDRV_AddAFMtoList
 *
 *  Adds an afm to the list whose head is pointed to by head. Creates new
 *  family node if necessary and always creates a new AFMLISTENTRY.
 *
 *  Returns FALSE for memory allocation error; returns TRUE, but sets *p_added
 *  to FALSE, for duplicate.
 */
BOOL PSDRV_AddAFMtoList(FONTFAMILY **head, const AFM *afm, BOOL *p_added)
{
    FONTFAMILY *family = *head;
    FONTFAMILY **insert = head;
    AFMLISTENTRY *tmpafmle, *newafmle;

    newafmle = HeapAlloc(PSDRV_Heap, HEAP_ZERO_MEMORY,
			   sizeof(*newafmle));
    if (newafmle == NULL)
    	return FALSE;

    newafmle->afm = afm;

    while(family) {
        if(!strcmp(family->FamilyName, afm->FamilyName))
	    break;
	insert = &(family->next);
	family = family->next;
    }

    if(!family) {
        family = HeapAlloc(PSDRV_Heap, HEAP_ZERO_MEMORY,
			   sizeof(*family));
	if (family == NULL) {
	    HeapFree(PSDRV_Heap, 0, newafmle);
	    return FALSE;
	}
	*insert = family;
	if (!(family->FamilyName = HeapAlloc(PSDRV_Heap, 0, strlen(afm->FamilyName)+1 ))) {
	    HeapFree(PSDRV_Heap, 0, family);
	    HeapFree(PSDRV_Heap, 0, newafmle);
	    return FALSE;
	}
	strcpy( family->FamilyName, afm->FamilyName );
	family->afmlist = newafmle;
	*p_added = TRUE;
	return TRUE;
    }
    else {
    	tmpafmle = family->afmlist;
	while (tmpafmle) {
	    if (!strcmp(tmpafmle->afm->FontName, afm->FontName)) {
	    	WARN("Ignoring duplicate FontName '%s'\n", afm->FontName);
		HeapFree(PSDRV_Heap, 0, newafmle);
		*p_added = FALSE;
		return TRUE;	    	    	    /* not a fatal error */
	    }
	    tmpafmle = tmpafmle->next;
	}
    }

    tmpafmle = family->afmlist;
    while(tmpafmle->next)
        tmpafmle = tmpafmle->next;

    tmpafmle->next = newafmle;

    *p_added = TRUE;
    return TRUE;
}


/***********************************************************
 *
 *	PSDRV_DumpFontList
 *
 */
static void PSDRV_DumpFontList(void)
{
    FONTFAMILY      *family;
    AFMLISTENTRY    *afmle;

    for(family = PSDRV_AFMFontList; family; family = family->next) {
        TRACE("Family '%s'\n", family->FamilyName);
	for(afmle = family->afmlist; afmle; afmle = afmle->next)
	{
#if 0
	    INT i;
#endif

	    TRACE("\tFontName '%s' (%i glyphs) - '%s' encoding:\n",
	    	    afmle->afm->FontName, afmle->afm->NumofMetrics,
		    afmle->afm->EncodingScheme);

	    /* Uncomment to regenerate font data; see afm2c.c */

	    /* PSDRV_AFM2C(afmle->afm); */

#if 0
	    for (i = 0; i < afmle->afm->NumofMetrics; ++i)
	    {
	    	TRACE("\t\tU+%.4lX; C %i; N '%s'\n", afmle->afm->Metrics[i].UV,
		    	afmle->afm->Metrics[i].C, afmle->afm->Metrics[i].N->sz);
	    }
#endif
	}
    }
    return;
}


/*******************************************************************************
 *  PSDRV_CalcAvgCharWidth
 *
 *  Calculate WinMetrics.sAvgCharWidth for a Type 1 font.  Can also be used on
 *  TrueType fonts, if font designer set OS/2:xAvgCharWidth to zero.
 *
 *  Tries to use formula in TrueType specification; falls back to simple mean
 *  if any lowercase latin letter (or space) is not present.
 */
inline static SHORT MeanCharWidth(const AFM *afm)
{
    float   w = 0.0;
    int     i;

    for (i = 0; i < afm->NumofMetrics; ++i)
    	w += afm->Metrics[i].WX;

    w /= afm->NumofMetrics;

    return (SHORT)(w + 0.5);
}

static const struct { LONG UV; int weight; } UVweight[27] =
{
    { 0x0061,  64 }, { 0x0062,  14 }, { 0x0063,  27 }, { 0x0064,  35 },
    { 0x0065, 100 }, { 0x0066,  20 }, { 0x0067,  14 }, { 0x0068,  42 },
    { 0x0069,  63 }, { 0x006a,   3 }, { 0x006b,   6 }, { 0x006c,  35 },
    { 0x006d,  20 }, { 0x006e,  56 }, { 0x006f,  56 }, { 0x0070,  17 },
    { 0x0071,   4 }, { 0x0072,  49 }, { 0x0073,  56 }, { 0x0074,  71 },
    { 0x0075,  31 }, { 0x0076,  10 }, { 0x0077,  18 }, { 0x0078,   3 },
    { 0x0079,  18 }, { 0x007a,   2 }, { 0x0020, 166 }
};

SHORT PSDRV_CalcAvgCharWidth(const AFM *afm)
{
    float   w = 0.0;
    int     i;

    for (i = 0; i < 27; ++i)
    {
    	const AFMMETRICS    *afmm;

	afmm = PSDRV_UVMetrics(UVweight[i].UV, afm);
	if (afmm->UV != UVweight[i].UV)     /* UVMetrics returns first glyph */
	    return MeanCharWidth(afm);	    /*   in font if UV is missing    */

	w += afmm->WX * (float)(UVweight[i].weight);
    }

    w /= 1000.0;

    return (SHORT)(w + 0.5);
}


/*******************************************************************************
 *  AddBuiltinAFMs
 *
 */

static BOOL AddBuiltinAFMs()
{
    const AFM *const	*afm = PSDRV_BuiltinAFMs;

    while (*afm != NULL)
    {
    	BOOL	added;

    	if (PSDRV_AddAFMtoList(&PSDRV_AFMFontList, *afm, &added) == FALSE)
	    return FALSE;

	if (added == FALSE)
	    TRACE("Ignoring built-in font %s\n", (*afm)->FontName);

	++afm;
    }

    return TRUE;
}


/***********************************************************
 *
 *	PSDRV_GetFontMetrics
 *
 * Parses all afm files listed in [afmdirs] and [TrueType Font Directories]
 * sections of Wine configuration file.  Adds built-in data last, so it can
 * be overridden by user-supplied AFM or TTF files.
 *
 * If this function fails, PSDRV_Init will destroy PSDRV_Heap, so don't worry
 * about freeing all the memory that's been allocated.
 */

BOOL PSDRV_GetFontMetrics(void)
{
    if (PSDRV_GlyphListInit() != 0)
    	return FALSE;

    if (PSDRV_GetType1Metrics() == FALSE)
    	return FALSE;

#ifdef HAVE_FREETYPE
    if (PSDRV_GetTrueTypeMetrics() == FALSE)
    	return FALSE;
#endif

    if (AddBuiltinAFMs() == FALSE)
    	return FALSE;

    PSDRV_IndexGlyphList(); 	    /* Enable fast searching of glyph names */

    PSDRV_DumpFontList();

    return TRUE;
}
