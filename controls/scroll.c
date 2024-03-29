/*
 * Scrollbar control
 *
 * Copyright 1993 Martin Ayotte
 * Copyright 1994, 1996 Alexandre Julliard
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
#include "wine/winuser16.h"
#include "controls.h"
#include "win.h"
#include "wine/debug.h"
#include "user.h"
#include "message.h"

WINE_DEFAULT_DEBUG_CHANNEL(scroll);

typedef struct
{
    INT   curVal;   /* Current scroll-bar value */
    INT   minVal;   /* Minimum scroll-bar value */
    INT   maxVal;   /* Maximum scroll-bar value */
    INT   page;     /* Page size of scroll bar (Win32) */
    UINT  flags;    /* EnableScrollBar flags */
} SCROLLBAR_INFO, *LPSCROLLBAR_INFO;


static HBITMAP hUpArrow;
static HBITMAP hDnArrow;
static HBITMAP hLfArrow;
static HBITMAP hRgArrow;
static HBITMAP hUpArrowD;
static HBITMAP hDnArrowD;
static HBITMAP hLfArrowD;
static HBITMAP hRgArrowD;
static HBITMAP hUpArrowI;
static HBITMAP hDnArrowI;
static HBITMAP hLfArrowI;
static HBITMAP hRgArrowI;

#define TOP_ARROW(flags,pressed) \
   (((flags)&ESB_DISABLE_UP) ? hUpArrowI : ((pressed) ? hUpArrowD:hUpArrow))
#define BOTTOM_ARROW(flags,pressed) \
   (((flags)&ESB_DISABLE_DOWN) ? hDnArrowI : ((pressed) ? hDnArrowD:hDnArrow))
#define LEFT_ARROW(flags,pressed) \
   (((flags)&ESB_DISABLE_LEFT) ? hLfArrowI : ((pressed) ? hLfArrowD:hLfArrow))
#define RIGHT_ARROW(flags,pressed) \
   (((flags)&ESB_DISABLE_RIGHT) ? hRgArrowI : ((pressed) ? hRgArrowD:hRgArrow))


  /* Minimum size of the rectangle between the arrows */
#define SCROLL_MIN_RECT  4

  /* Minimum size of the thumb in pixels */
#define SCROLL_MIN_THUMB 6

  /* Overlap between arrows and thumb */
#define SCROLL_ARROW_THUMB_OVERLAP ((TWEAK_WineLook == WIN31_LOOK) ? 1 : 0)

  /* Delay (in ms) before first repetition when holding the button down */
#define SCROLL_FIRST_DELAY   200

  /* Delay (in ms) between scroll repetitions */
#define SCROLL_REPEAT_DELAY  50

  /* Scroll timer id */
#define SCROLL_TIMER   0

  /* Scroll-bar hit testing */
enum SCROLL_HITTEST
{
    SCROLL_NOWHERE,      /* Outside the scroll bar */
    SCROLL_TOP_ARROW,    /* Top or left arrow */
    SCROLL_TOP_RECT,     /* Rectangle between the top arrow and the thumb */
    SCROLL_THUMB,        /* Thumb rectangle */
    SCROLL_BOTTOM_RECT,  /* Rectangle between the thumb and the bottom arrow */
    SCROLL_BOTTOM_ARROW  /* Bottom or right arrow */
};

 /* What to do after SCROLL_SetScrollInfo() */
#define SA_SSI_HIDE		0x0001
#define SA_SSI_SHOW		0x0002
#define SA_SSI_REFRESH		0x0004
#define SA_SSI_REPAINT_ARROWS	0x0008

 /* Thumb-tracking info */
static HWND SCROLL_TrackingWin = 0;
static INT  SCROLL_TrackingBar = 0;
static INT  SCROLL_TrackingPos = 0;
static INT  SCROLL_TrackingVal = 0;
 /* Hit test code of the last button-down event */
static enum SCROLL_HITTEST SCROLL_trackHitTest;
static BOOL SCROLL_trackVertical;

 /* Is the moving thumb being displayed? */
static BOOL SCROLL_MovingThumb = FALSE;

 /* Local functions */
static BOOL SCROLL_ShowScrollBar( HWND hwnd, INT nBar,
				    BOOL fShowH, BOOL fShowV );
static INT SCROLL_SetScrollInfo( HWND hwnd, INT nBar,
				   const SCROLLINFO *info, INT *action );
static void SCROLL_DrawInterior_9x( HWND hwnd, HDC hdc, INT nBar,
				    RECT *rect, INT arrowSize,
				    INT thumbSize, INT thumbPos,
				    UINT flags, BOOL vertical,
				    BOOL top_selected, BOOL bottom_selected );
static LRESULT WINAPI ScrollBarWndProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );


/*********************************************************************
 * scrollbar class descriptor
 */
const struct builtin_class_descr SCROLL_builtin_class =
{
    "ScrollBar",            /* name */
    CS_DBLCLKS | CS_VREDRAW | CS_HREDRAW | CS_PARENTDC, /* style  */
    NULL,                   /* procA (winproc is Unicode only) */
    ScrollBarWndProc,       /* procW */
    sizeof(SCROLLBAR_INFO), /* extra */
    IDC_ARROW,              /* cursor */
    0                       /* brush */
};
    
/***********************************************************************
 *           SCROLL_ScrollInfoValid
 *
 *  Determine if the supplied SCROLLINFO struct is valid.
 */
inline static BOOL SCROLL_ScrollInfoValid(
LPSCROLLINFO info /* [in] The SCROLLINFO struct to be tested */)
{
    return !(info->fMask & ~(SIF_ALL | SIF_DISABLENOSCROLL)
        || (info->cbSize != sizeof(*info)
            && info->cbSize != sizeof(*info) - sizeof(info->nTrackPos)));
}


/***********************************************************************
 *           SCROLL_LoadBitmaps
 */
static void SCROLL_LoadBitmaps(void)
{
    hUpArrow  = LoadBitmapA( 0, MAKEINTRESOURCEA(OBM_UPARROW) );
    hDnArrow  = LoadBitmapA( 0, MAKEINTRESOURCEA(OBM_DNARROW) );
    hLfArrow  = LoadBitmapA( 0, MAKEINTRESOURCEA(OBM_LFARROW) );
    hRgArrow  = LoadBitmapA( 0, MAKEINTRESOURCEA(OBM_RGARROW) );
    hUpArrowD = LoadBitmapA( 0, MAKEINTRESOURCEA(OBM_UPARROWD) );
    hDnArrowD = LoadBitmapA( 0, MAKEINTRESOURCEA(OBM_DNARROWD) );
    hLfArrowD = LoadBitmapA( 0, MAKEINTRESOURCEA(OBM_LFARROWD) );
    hRgArrowD = LoadBitmapA( 0, MAKEINTRESOURCEA(OBM_RGARROWD) );
    hUpArrowI = LoadBitmapA( 0, MAKEINTRESOURCEA(OBM_UPARROWI) );
    hDnArrowI = LoadBitmapA( 0, MAKEINTRESOURCEA(OBM_DNARROWI) );
    hLfArrowI = LoadBitmapA( 0, MAKEINTRESOURCEA(OBM_LFARROWI) );
    hRgArrowI = LoadBitmapA( 0, MAKEINTRESOURCEA(OBM_RGARROWI) );
}


/***********************************************************************
 *           SCROLL_GetScrollBarInfo
 */
static SCROLLBAR_INFO *SCROLL_GetScrollBarInfo( HWND hwnd, INT nBar )
{
    SCROLLBAR_INFO *infoPtr;
    WND *wndPtr = WIN_FindWndPtr( hwnd );

    if (!wndPtr) return NULL;
    switch(nBar)
    {
        case SB_HORZ: infoPtr = (SCROLLBAR_INFO *)wndPtr->pHScroll; break;
        case SB_VERT: infoPtr = (SCROLLBAR_INFO *)wndPtr->pVScroll; break;
        case SB_CTL:  infoPtr = (SCROLLBAR_INFO *)wndPtr->wExtra; break;
        default:
            WIN_ReleaseWndPtr( wndPtr );
            return NULL;
    }

    if (!infoPtr)  /* Create the info structure if needed */
    {
        if ((infoPtr = HeapAlloc( GetProcessHeap(), 0, sizeof(SCROLLBAR_INFO) )))
        {
            infoPtr->minVal = infoPtr->curVal = infoPtr->page = 0;
            infoPtr->maxVal = 100;
            infoPtr->flags  = ESB_ENABLE_BOTH;
            if (nBar == SB_HORZ) wndPtr->pHScroll = infoPtr;
            else wndPtr->pVScroll = infoPtr;
        }
        if (!hUpArrow) SCROLL_LoadBitmaps();
    }
    WIN_ReleaseWndPtr( wndPtr );
    return infoPtr;
}


/***********************************************************************
 *           SCROLL_GetScrollBarRect
 *
 * Compute the scroll bar rectangle, in drawing coordinates (i.e. client
 * coords for SB_CTL, window coords for SB_VERT and SB_HORZ).
 * 'arrowSize' returns the width or height of an arrow (depending on
 * the orientation of the scrollbar), 'thumbSize' returns the size of
 * the thumb, and 'thumbPos' returns the position of the thumb
 * relative to the left or to the top.
 * Return TRUE if the scrollbar is vertical, FALSE if horizontal.
 */
static BOOL SCROLL_GetScrollBarRect( HWND hwnd, INT nBar, RECT *lprect,
                                       INT *arrowSize, INT *thumbSize,
                                       INT *thumbPos )
{
    INT pixels;
    BOOL vertical;
    WND *wndPtr = WIN_FindWndPtr( hwnd );

    switch(nBar)
    {
      case SB_HORZ:
        lprect->left   = wndPtr->rectClient.left - wndPtr->rectWindow.left;
        lprect->top    = wndPtr->rectClient.bottom - wndPtr->rectWindow.top;
        lprect->right  = wndPtr->rectClient.right - wndPtr->rectWindow.left;
        lprect->bottom = lprect->top + GetSystemMetrics(SM_CYHSCROLL);
	if(wndPtr->dwStyle & WS_BORDER) {
	  lprect->left--;
	  lprect->right++;
	} else if(wndPtr->dwStyle & WS_VSCROLL)
	  lprect->right++;
        vertical = FALSE;
	break;

      case SB_VERT:
        if((wndPtr->dwExStyle & WS_EX_LEFTSCROLLBAR) != 0)
            lprect->left   = wndPtr->rectClient.left - wndPtr->rectWindow.left - GetSystemMetrics(SM_CXVSCROLL);
        else
            lprect->left   = wndPtr->rectClient.right - wndPtr->rectWindow.left;
        lprect->top    = wndPtr->rectClient.top - wndPtr->rectWindow.top;
        lprect->right  = lprect->left + GetSystemMetrics(SM_CXVSCROLL);
        lprect->bottom = wndPtr->rectClient.bottom - wndPtr->rectWindow.top;
	if(wndPtr->dwStyle & WS_BORDER) {
	  lprect->top--;
	  lprect->bottom++;
	} else if(wndPtr->dwStyle & WS_HSCROLL)
	  lprect->bottom++;
        vertical = TRUE;
	break;

      case SB_CTL:
	GetClientRect( hwnd, lprect );
        vertical = ((wndPtr->dwStyle & SBS_VERT) != 0);
	break;

    default:
        WIN_ReleaseWndPtr(wndPtr);
        return FALSE;
    }

    if (vertical) pixels = lprect->bottom - lprect->top;
    else pixels = lprect->right - lprect->left;

    if (pixels <= 2*GetSystemMetrics(SM_CXVSCROLL) + SCROLL_MIN_RECT)
    {
        if (pixels > SCROLL_MIN_RECT)
            *arrowSize = (pixels - SCROLL_MIN_RECT) / 2;
        else
            *arrowSize = 0;
        *thumbPos = *thumbSize = 0;
    }
    else
    {
        SCROLLBAR_INFO *info = SCROLL_GetScrollBarInfo( hwnd, nBar );

        *arrowSize = GetSystemMetrics(SM_CXVSCROLL);
        pixels -= (2 * (GetSystemMetrics(SM_CXVSCROLL) - SCROLL_ARROW_THUMB_OVERLAP));

        if (info->page)
        {
	    *thumbSize = MulDiv(pixels,info->page,(info->maxVal-info->minVal+1));
            if (*thumbSize < SCROLL_MIN_THUMB) *thumbSize = SCROLL_MIN_THUMB;
        }
        else *thumbSize = GetSystemMetrics(SM_CXVSCROLL);

        if (((pixels -= *thumbSize ) < 0) ||
            ((info->flags & ESB_DISABLE_BOTH) == ESB_DISABLE_BOTH))
        {
            /* Rectangle too small or scrollbar disabled -> no thumb */
            *thumbPos = *thumbSize = 0;
        }
        else
        {
            INT max = info->maxVal - max( info->page-1, 0 );
            if (info->minVal >= max)
                *thumbPos = *arrowSize - SCROLL_ARROW_THUMB_OVERLAP;
            else
                *thumbPos = *arrowSize - SCROLL_ARROW_THUMB_OVERLAP
		  + MulDiv(pixels, (info->curVal-info->minVal),(max - info->minVal));
        }
    }
    WIN_ReleaseWndPtr(wndPtr);
    return vertical;
}


/***********************************************************************
 *           SCROLL_GetThumbVal
 *
 * Compute the current scroll position based on the thumb position in pixels
 * from the top of the scroll-bar.
 */
static UINT SCROLL_GetThumbVal( SCROLLBAR_INFO *infoPtr, RECT *rect,
                                  BOOL vertical, INT pos )
{
    INT thumbSize;
    INT pixels = vertical ? rect->bottom-rect->top : rect->right-rect->left;

    if ((pixels -= 2*(GetSystemMetrics(SM_CXVSCROLL) - SCROLL_ARROW_THUMB_OVERLAP)) <= 0)
        return infoPtr->minVal;

    if (infoPtr->page)
    {
        thumbSize = MulDiv(pixels,infoPtr->page,(infoPtr->maxVal-infoPtr->minVal+1));
        if (thumbSize < SCROLL_MIN_THUMB) thumbSize = SCROLL_MIN_THUMB;
    }
    else thumbSize = GetSystemMetrics(SM_CXVSCROLL);

    if ((pixels -= thumbSize) <= 0) return infoPtr->minVal;

    pos = max( 0, pos - (GetSystemMetrics(SM_CXVSCROLL) - SCROLL_ARROW_THUMB_OVERLAP) );
    if (pos > pixels) pos = pixels;

    if (!infoPtr->page) pos *= infoPtr->maxVal - infoPtr->minVal;
    else pos *= infoPtr->maxVal - infoPtr->minVal - infoPtr->page + 1;
    return infoPtr->minVal + ((pos + pixels / 2) / pixels);
}

/***********************************************************************
 *           SCROLL_PtInRectEx
 */
static BOOL SCROLL_PtInRectEx( LPRECT lpRect, POINT pt, BOOL vertical )
{
    RECT rect = *lpRect;

    if (vertical)
    {
	rect.left -= lpRect->right - lpRect->left;
	rect.right += lpRect->right - lpRect->left;
    }
    else
    {
	rect.top -= lpRect->bottom - lpRect->top;
	rect.bottom += lpRect->bottom - lpRect->top;
    }
    return PtInRect( &rect, pt );
}

/***********************************************************************
 *           SCROLL_ClipPos
 */
static POINT SCROLL_ClipPos( LPRECT lpRect, POINT pt )
{
    if( pt.x < lpRect->left )
	pt.x = lpRect->left;
    else
    if( pt.x > lpRect->right )
	pt.x = lpRect->right;

    if( pt.y < lpRect->top )
	pt.y = lpRect->top;
    else
    if( pt.y > lpRect->bottom )
	pt.y = lpRect->bottom;

    return pt;
}


/***********************************************************************
 *           SCROLL_HitTest
 *
 * Scroll-bar hit testing (don't confuse this with WM_NCHITTEST!).
 */
static enum SCROLL_HITTEST SCROLL_HitTest( HWND hwnd, INT nBar,
                                           POINT pt, BOOL bDragging )
{
    INT arrowSize, thumbSize, thumbPos;
    RECT rect;

    BOOL vertical = SCROLL_GetScrollBarRect( hwnd, nBar, &rect,
                                           &arrowSize, &thumbSize, &thumbPos );

    if ( (bDragging && !SCROLL_PtInRectEx( &rect, pt, vertical )) ||
	 (!PtInRect( &rect, pt )) ) return SCROLL_NOWHERE;

    if (vertical)
    {
        if (pt.y < rect.top + arrowSize) return SCROLL_TOP_ARROW;
        if (pt.y >= rect.bottom - arrowSize) return SCROLL_BOTTOM_ARROW;
        if (!thumbPos) return SCROLL_TOP_RECT;
        pt.y -= rect.top;
        if (pt.y < thumbPos) return SCROLL_TOP_RECT;
        if (pt.y >= thumbPos + thumbSize) return SCROLL_BOTTOM_RECT;
    }
    else  /* horizontal */
    {
        if (pt.x < rect.left + arrowSize) return SCROLL_TOP_ARROW;
        if (pt.x >= rect.right - arrowSize) return SCROLL_BOTTOM_ARROW;
        if (!thumbPos) return SCROLL_TOP_RECT;
        pt.x -= rect.left;
        if (pt.x < thumbPos) return SCROLL_TOP_RECT;
        if (pt.x >= thumbPos + thumbSize) return SCROLL_BOTTOM_RECT;
    }
    return SCROLL_THUMB;
}


/***********************************************************************
 *           SCROLL_DrawArrows
 *
 * Draw the scroll bar arrows.
 */
static void SCROLL_DrawArrows_9x( HDC hdc, SCROLLBAR_INFO *infoPtr,
                                  RECT *rect, INT arrowSize, BOOL vertical,
                                  BOOL top_pressed, BOOL bottom_pressed )
{
  RECT r;

  r = *rect;
  if( vertical )
    r.bottom = r.top + arrowSize;
  else
    r.right = r.left + arrowSize;

  DrawFrameControl( hdc, &r, DFC_SCROLL,
		    (vertical ? DFCS_SCROLLUP : DFCS_SCROLLLEFT)
		    | (top_pressed ? (DFCS_PUSHED | DFCS_FLAT) : 0 )
		    | (infoPtr->flags&ESB_DISABLE_LTUP ? DFCS_INACTIVE : 0 ) );

  r = *rect;
  if( vertical )
    r.top = r.bottom-arrowSize;
  else
    r.left = r.right-arrowSize;

  DrawFrameControl( hdc, &r, DFC_SCROLL,
		    (vertical ? DFCS_SCROLLDOWN : DFCS_SCROLLRIGHT)
		    | (bottom_pressed ? (DFCS_PUSHED | DFCS_FLAT) : 0 )
		    | (infoPtr->flags&ESB_DISABLE_RTDN ? DFCS_INACTIVE : 0) );
}

static void SCROLL_DrawArrows_31( HDC hdc, SCROLLBAR_INFO *infoPtr,
				  RECT *rect, INT arrowSize, BOOL vertical,
				  BOOL top_pressed, BOOL bottom_pressed )
{
    HDC hdcMem = CreateCompatibleDC( hdc );
    HBITMAP hbmpPrev = SelectObject( hdcMem, vertical ?
                                    TOP_ARROW(infoPtr->flags, top_pressed)
                                    : LEFT_ARROW(infoPtr->flags, top_pressed));

    SetStretchBltMode( hdc, STRETCH_DELETESCANS );
    StretchBlt( hdc, rect->left, rect->top,
                  vertical ? rect->right-rect->left : arrowSize,
                  vertical ? arrowSize : rect->bottom-rect->top,
                  hdcMem, 0, 0,
                  GetSystemMetrics(SM_CXVSCROLL),GetSystemMetrics(SM_CYHSCROLL),
                  SRCCOPY );

    SelectObject( hdcMem, vertical ?
                    BOTTOM_ARROW( infoPtr->flags, bottom_pressed )
                    : RIGHT_ARROW( infoPtr->flags, bottom_pressed ) );
    if (vertical)
        StretchBlt( hdc, rect->left, rect->bottom - arrowSize,
                      rect->right - rect->left, arrowSize,
                      hdcMem, 0, 0,
                      GetSystemMetrics(SM_CXVSCROLL),GetSystemMetrics(SM_CYHSCROLL),
                      SRCCOPY );
    else
        StretchBlt( hdc, rect->right - arrowSize, rect->top,
                      arrowSize, rect->bottom - rect->top,
                      hdcMem, 0, 0,
                      GetSystemMetrics(SM_CXVSCROLL), GetSystemMetrics(SM_CYHSCROLL),
                      SRCCOPY );
    SelectObject( hdcMem, hbmpPrev );
    DeleteDC( hdcMem );
}

static void SCROLL_DrawArrows( HDC hdc, SCROLLBAR_INFO *infoPtr,
			       RECT *rect, INT arrowSize, BOOL vertical,
			       BOOL top_pressed, BOOL bottom_pressed )
{
  if( TWEAK_WineLook == WIN31_LOOK )
    SCROLL_DrawArrows_31( hdc, infoPtr, rect, arrowSize,
			  vertical, top_pressed,bottom_pressed );
  else
    SCROLL_DrawArrows_9x( hdc, infoPtr, rect, arrowSize,
			  vertical, top_pressed,bottom_pressed );
}


/***********************************************************************
 *           SCROLL_DrawMovingThumb
 *
 * Draw the moving thumb rectangle.
 */
static void SCROLL_DrawMovingThumb_31( HDC hdc, RECT *rect, BOOL vertical,
				       INT arrowSize, INT thumbSize )
{
    RECT r = *rect;
    if (vertical)
    {
        r.top += SCROLL_TrackingPos;
        if (r.top < rect->top + arrowSize - SCROLL_ARROW_THUMB_OVERLAP)
	    r.top = rect->top + arrowSize - SCROLL_ARROW_THUMB_OVERLAP;
        if (r.top + thumbSize >
	               rect->bottom - (arrowSize - SCROLL_ARROW_THUMB_OVERLAP))
            r.top = rect->bottom - (arrowSize - SCROLL_ARROW_THUMB_OVERLAP)
	                                                          - thumbSize;
        r.bottom = r.top + thumbSize;
    }
    else
    {
        r.left += SCROLL_TrackingPos;
        if (r.left < rect->left + arrowSize - SCROLL_ARROW_THUMB_OVERLAP)
	    r.left = rect->left + arrowSize - SCROLL_ARROW_THUMB_OVERLAP;
        if (r.left + thumbSize >
	               rect->right - (arrowSize - SCROLL_ARROW_THUMB_OVERLAP))
            r.left = rect->right - (arrowSize - SCROLL_ARROW_THUMB_OVERLAP)
	                                                          - thumbSize;
        r.right = r.left + thumbSize;
    }

    DrawFocusRect( hdc, &r );
    SCROLL_MovingThumb = !SCROLL_MovingThumb;
}

static void SCROLL_DrawMovingThumb_9x( HDC hdc, RECT *rect, BOOL vertical,
				       INT arrowSize, INT thumbSize )
{
  INT pos = SCROLL_TrackingPos;
  INT max_size;

  if( vertical )
    max_size = rect->bottom - rect->top;
  else
    max_size = rect->right - rect->left;

  max_size -= (arrowSize-SCROLL_ARROW_THUMB_OVERLAP) + thumbSize;

  if( pos < (arrowSize-SCROLL_ARROW_THUMB_OVERLAP) )
    pos = (arrowSize-SCROLL_ARROW_THUMB_OVERLAP);
  else if( pos > max_size )
    pos = max_size;

  SCROLL_DrawInterior_9x( SCROLL_TrackingWin, hdc, SCROLL_TrackingBar,
			  rect, arrowSize, thumbSize, pos,
			  0, vertical, FALSE, FALSE );

  SCROLL_MovingThumb = !SCROLL_MovingThumb;
}

static void SCROLL_DrawMovingThumb( HDC hdc, RECT *rect, BOOL vertical,
				    INT arrowSize, INT thumbSize )
{
  if( TWEAK_WineLook == WIN31_LOOK )
    SCROLL_DrawMovingThumb_31( hdc, rect, vertical, arrowSize, thumbSize );
  else
    SCROLL_DrawMovingThumb_9x( hdc, rect, vertical, arrowSize, thumbSize );
}

/***********************************************************************
 *           SCROLL_DrawInterior
 *
 * Draw the scroll bar interior (everything except the arrows).
 */
static void SCROLL_DrawInterior_9x( HWND hwnd, HDC hdc, INT nBar,
				    RECT *rect, INT arrowSize,
				    INT thumbSize, INT thumbPos,
				    UINT flags, BOOL vertical,
				    BOOL top_selected, BOOL bottom_selected )
{
    RECT r;
    HPEN hSavePen;
    HBRUSH hSaveBrush,hBrush;

    /* Only scrollbar controls send WM_CTLCOLORSCROLLBAR.
     * The window-owned scrollbars need to call DEFWND_ControlColor
     * to correctly setup default scrollbar colors
     */
    if (nBar == SB_CTL)
    {
      hBrush = (HBRUSH)SendMessageA( GetParent(hwnd), WM_CTLCOLORSCROLLBAR,
				     (WPARAM)hdc,(LPARAM)hwnd);
    }
    else
    {
      hBrush = DEFWND_ControlColor( hdc, CTLCOLOR_SCROLLBAR );
    }

    hSavePen = SelectObject( hdc, SYSCOLOR_GetPen(COLOR_WINDOWFRAME) );
    hSaveBrush = SelectObject( hdc, hBrush );

    /* Calculate the scroll rectangle */
    r = *rect;
    if (vertical)
    {
        r.top    += arrowSize - SCROLL_ARROW_THUMB_OVERLAP;
        r.bottom -= (arrowSize - SCROLL_ARROW_THUMB_OVERLAP);
    }
    else
    {
        r.left  += arrowSize - SCROLL_ARROW_THUMB_OVERLAP;
        r.right -= (arrowSize - SCROLL_ARROW_THUMB_OVERLAP);
    }

    /* Draw the scroll rectangles and thumb */
    if (!thumbPos)  /* No thumb to draw */
    {
        PatBlt( hdc, r.left, r.top,
                     r.right - r.left, r.bottom - r.top,
                     PATCOPY );

        /* cleanup and return */
        SelectObject( hdc, hSavePen );
        SelectObject( hdc, hSaveBrush );
        return;
    }

    if (vertical)
    {
        PatBlt( hdc, r.left, r.top,
                  r.right - r.left,
                  thumbPos - (arrowSize - SCROLL_ARROW_THUMB_OVERLAP),
                  top_selected ? 0x0f0000 : PATCOPY );
        r.top += thumbPos - (arrowSize - SCROLL_ARROW_THUMB_OVERLAP);
        PatBlt( hdc, r.left, r.top + thumbSize,
                  r.right - r.left,
                  r.bottom - r.top - thumbSize,
                  bottom_selected ? 0x0f0000 : PATCOPY );
        r.bottom = r.top + thumbSize;
    }
    else  /* horizontal */
    {
        PatBlt( hdc, r.left, r.top,
                  thumbPos - (arrowSize - SCROLL_ARROW_THUMB_OVERLAP),
                  r.bottom - r.top,
                  top_selected ? 0x0f0000 : PATCOPY );
        r.left += thumbPos - (arrowSize - SCROLL_ARROW_THUMB_OVERLAP);
        PatBlt( hdc, r.left + thumbSize, r.top,
                  r.right - r.left - thumbSize,
                  r.bottom - r.top,
                  bottom_selected ? 0x0f0000 : PATCOPY );
        r.right = r.left + thumbSize;
    }

    /* Draw the thumb */
    DrawEdge( hdc, &r, EDGE_RAISED, BF_RECT | BF_MIDDLE  );

    /* cleanup */
    SelectObject( hdc, hSavePen );
    SelectObject( hdc, hSaveBrush );
}


static void SCROLL_DrawInterior( HWND hwnd, HDC hdc, INT nBar,
                                 RECT *rect, INT arrowSize,
                                 INT thumbSize, INT thumbPos,
                                 UINT flags, BOOL vertical,
                                 BOOL top_selected, BOOL bottom_selected )
{
    RECT r;
    HPEN hSavePen;
    HBRUSH hSaveBrush,hBrush;
    BOOL Save_SCROLL_MovingThumb = SCROLL_MovingThumb;

    if (Save_SCROLL_MovingThumb &&
        (SCROLL_TrackingWin == hwnd) &&
        (SCROLL_TrackingBar == nBar))
        SCROLL_DrawMovingThumb( hdc, rect, vertical, arrowSize, thumbSize );

      /* Select the correct brush and pen */

    if (TWEAK_WineLook == WIN31_LOOK && (flags & ESB_DISABLE_BOTH) == ESB_DISABLE_BOTH)
    {
        /* This ought to be the color of the parent window */
        hBrush = GetSysColorBrush(COLOR_WINDOW);
    }
    else
    {
        /* Only scrollbar controls send WM_CTLCOLORSCROLLBAR.
         * The window-owned scrollbars need to call DEFWND_ControlColor
         * to correctly setup default scrollbar colors
         */
        if (nBar == SB_CTL) {
            hBrush = (HBRUSH)SendMessageA( GetParent(hwnd), WM_CTLCOLORSCROLLBAR,
                                           (WPARAM)hdc,(LPARAM)hwnd);
        } else {
            hBrush = DEFWND_ControlColor( hdc, CTLCOLOR_SCROLLBAR );
        }
    }
    hSavePen = SelectObject( hdc, SYSCOLOR_GetPen(COLOR_WINDOWFRAME) );
    hSaveBrush = SelectObject( hdc, hBrush );

      /* Calculate the scroll rectangle */

    r = *rect;
    if (vertical)
    {
        r.top    += arrowSize - SCROLL_ARROW_THUMB_OVERLAP;
        r.bottom -= (arrowSize - SCROLL_ARROW_THUMB_OVERLAP);
    }
    else
    {
        r.left  += arrowSize - SCROLL_ARROW_THUMB_OVERLAP;
        r.right -= (arrowSize - SCROLL_ARROW_THUMB_OVERLAP);
    }

      /* Draw the scroll bar frame */

	/* Only draw outline if Win 3.1.  Mar 24, 1999 - Ronald B. Cemer */
    if (TWEAK_WineLook == WIN31_LOOK)
	Rectangle( hdc, r.left, r.top, r.right, r.bottom );

      /* Draw the scroll rectangles and thumb */

    if (!thumbPos)  /* No thumb to draw */
    {
        INT offset = (TWEAK_WineLook > WIN31_LOOK) ? 0 : 1;

        PatBlt( hdc, r.left+offset, r.top+offset,
                     r.right - r.left - 2*offset, r.bottom - r.top - 2*offset,
                     PATCOPY );

        /* cleanup and return */
        SelectObject( hdc, hSavePen );
        SelectObject( hdc, hSaveBrush );
        return;
    }

    if (vertical)
    {
        INT offset = (TWEAK_WineLook == WIN31_LOOK) ? 1 : 0;

        PatBlt( hdc, r.left + offset, r.top + offset,
                  r.right - r.left - offset*2,
                  thumbPos - (arrowSize - SCROLL_ARROW_THUMB_OVERLAP) - offset,
                  top_selected ? 0x0f0000 : PATCOPY );
        r.top += thumbPos - (arrowSize - SCROLL_ARROW_THUMB_OVERLAP);
        PatBlt( hdc, r.left + offset, r.top + thumbSize,
                  r.right - r.left - offset*2,
                  r.bottom - r.top - thumbSize - offset,
                  bottom_selected ? 0x0f0000 : PATCOPY );
        r.bottom = r.top + thumbSize;
    }
    else  /* horizontal */
    {
        INT offset = (TWEAK_WineLook == WIN31_LOOK) ? 1 : 0;

        PatBlt( hdc, r.left + offset, r.top + offset,
                  thumbPos - (arrowSize - SCROLL_ARROW_THUMB_OVERLAP),
                  r.bottom - r.top - offset*2,
                  top_selected ? 0x0f0000 : PATCOPY );
        r.left += thumbPos - (arrowSize - SCROLL_ARROW_THUMB_OVERLAP);
        PatBlt( hdc, r.left + thumbSize, r.top + offset,
                  r.right - r.left - thumbSize - offset,
                  r.bottom - r.top - offset*2,
                  bottom_selected ? 0x0f0000 : PATCOPY );
        r.right = r.left + thumbSize;
    }

      /* Draw the thumb */

    SelectObject( hdc, GetSysColorBrush(COLOR_BTNFACE) );
    if (TWEAK_WineLook == WIN31_LOOK)
    {
	Rectangle( hdc, r.left, r.top, r.right, r.bottom );
	r.top++, r.left++;
    }
    else
    {
	Rectangle( hdc, r.left+1, r.top+1, r.right-1, r.bottom-1 );
    }
    DrawEdge( hdc, &r, EDGE_RAISED, BF_RECT );

    if (Save_SCROLL_MovingThumb &&
        (SCROLL_TrackingWin == hwnd) &&
        (SCROLL_TrackingBar == nBar))
        SCROLL_DrawMovingThumb( hdc, rect, vertical, arrowSize, thumbSize );

    /* cleanup */
    SelectObject( hdc, hSavePen );
    SelectObject( hdc, hSaveBrush );
}


/***********************************************************************
 *           SCROLL_DrawScrollBar
 *
 * Redraw the whole scrollbar.
 */
void SCROLL_DrawScrollBar( HWND hwnd, HDC hdc, INT nBar,
			   BOOL arrows, BOOL interior )
{
    INT arrowSize, thumbSize, thumbPos;
    RECT rect;
    BOOL vertical;
    WND *wndPtr = WIN_FindWndPtr( hwnd );
    SCROLLBAR_INFO *infoPtr = SCROLL_GetScrollBarInfo( hwnd, nBar );
    BOOL Save_SCROLL_MovingThumb = SCROLL_MovingThumb;

    if (!wndPtr || !infoPtr ||
        ((nBar == SB_VERT) && !(wndPtr->dwStyle & WS_VSCROLL)) ||
        ((nBar == SB_HORZ) && !(wndPtr->dwStyle & WS_HSCROLL))) goto END;
    if (!WIN_IsWindowDrawable( hwnd, FALSE )) goto END;
    hwnd = wndPtr->hwndSelf;  /* make it a full handle */

    vertical = SCROLL_GetScrollBarRect( hwnd, nBar, &rect,
                                        &arrowSize, &thumbSize, &thumbPos );

    /* do not draw if the scrollbar rectangle is empty */
    if(IsRectEmpty(&rect))
      goto END;

    if (Save_SCROLL_MovingThumb &&
        (SCROLL_TrackingWin == hwnd) &&
        (SCROLL_TrackingBar == nBar))
        SCROLL_DrawMovingThumb( hdc, &rect, vertical, arrowSize, thumbSize );

      /* Draw the arrows */

    if (arrows && arrowSize)
    {
	if( vertical == SCROLL_trackVertical && GetCapture() == hwnd )
	    SCROLL_DrawArrows( hdc, infoPtr, &rect, arrowSize, vertical,
			       (SCROLL_trackHitTest == SCROLL_TOP_ARROW),
			       (SCROLL_trackHitTest == SCROLL_BOTTOM_ARROW) );
	else
	    SCROLL_DrawArrows( hdc, infoPtr, &rect, arrowSize, vertical,
							       FALSE, FALSE );
    }
    if( interior )
	SCROLL_DrawInterior( hwnd, hdc, nBar, &rect, arrowSize, thumbSize,
                         thumbPos, infoPtr->flags, vertical, FALSE, FALSE );

    if (Save_SCROLL_MovingThumb &&
        (SCROLL_TrackingWin == hwnd) &&
        (SCROLL_TrackingBar == nBar))
        SCROLL_DrawMovingThumb( hdc, &rect, vertical, arrowSize, thumbSize );

    /* if scroll bar has focus, reposition the caret */
    if(hwnd==GetFocus() && (nBar==SB_CTL))
    {
        if (!vertical)
        {
            SetCaretPos(thumbPos+1, rect.top+1);
        }
        else
        {
            SetCaretPos(rect.top+1, thumbPos+1);
        }
    }

END:
    WIN_ReleaseWndPtr(wndPtr);
}

/***********************************************************************
 *           SCROLL_DrawSizeGrip
 *
 *  Draw the size grip.
 */
static void SCROLL_DrawSizeGrip( HWND hwnd,  HDC hdc)
{
    RECT rc;

    GetClientRect( hwnd, &rc );
    FillRect( hdc, &rc, GetSysColorBrush(COLOR_SCROLLBAR) );
    rc.left = max( rc.left, rc.right - GetSystemMetrics(SM_CXVSCROLL) - 1 );
    rc.top  = max( rc.top, rc.bottom - GetSystemMetrics(SM_CYHSCROLL) - 1 );
    DrawFrameControl( hdc, &rc, DFC_SCROLL, DFCS_SCROLLSIZEGRIP );
}


/***********************************************************************
 *           SCROLL_RefreshScrollBar
 *
 * Repaint the scroll bar interior after a SetScrollRange() or
 * SetScrollPos() call.
 */
static void SCROLL_RefreshScrollBar( HWND hwnd, INT nBar,
				     BOOL arrows, BOOL interior )
{
    HDC hdc = GetDCEx( hwnd, 0,
                           DCX_CACHE | ((nBar == SB_CTL) ? 0 : DCX_WINDOW) );
    if (!hdc) return;

    SCROLL_DrawScrollBar( hwnd, hdc, nBar, arrows, interior );
    ReleaseDC( hwnd, hdc );
}


/***********************************************************************
 *           SCROLL_HandleKbdEvent
 *
 * Handle a keyboard event (only for SB_CTL scrollbars with focus).
 */
static void SCROLL_HandleKbdEvent(
HWND hwnd /* [in] Handle of window with scrollbar(s) */,
WPARAM wParam /* [in] Variable input including enable state */,
LPARAM lParam /* [in] Variable input including input point */)
{
    TRACE("hwnd=%p wParam=%d lParam=%ld\n", hwnd, wParam, lParam);

    /* hide caret on first KEYDOWN to prevent flicker */
    if ((lParam & PFD_DOUBLEBUFFER_DONTCARE) == 0)
        HideCaret(hwnd);

    switch(wParam)
    {
    case VK_PRIOR: wParam = SB_PAGEUP; break;
    case VK_NEXT:  wParam = SB_PAGEDOWN; break;
    case VK_HOME:  wParam = SB_TOP; break;
    case VK_END:   wParam = SB_BOTTOM; break;
    case VK_UP:    wParam = SB_LINEUP; break;
    case VK_DOWN:  wParam = SB_LINEDOWN; break;
    default: return;
    }
    SendMessageW(GetParent(hwnd),
        ((GetWindowLongA( hwnd, GWL_STYLE ) & SBS_VERT) ?
            WM_VSCROLL : WM_HSCROLL), wParam, (LPARAM)hwnd);
}


/***********************************************************************
 *           SCROLL_HandleScrollEvent
 *
 * Handle a mouse or timer event for the scrollbar.
 * 'pt' is the location of the mouse event in client (for SB_CTL) or
 * windows coordinates.
 */
static void SCROLL_HandleScrollEvent( HWND hwnd, INT nBar, UINT msg, POINT pt)
{
      /* Previous mouse position for timer events */
    static POINT prevPt;
      /* Thumb position when tracking started. */
    static UINT trackThumbPos;
      /* Position in the scroll-bar of the last button-down event. */
    static INT lastClickPos;
      /* Position in the scroll-bar of the last mouse event. */
    static INT lastMousePos;

    enum SCROLL_HITTEST hittest;
    HWND hwndOwner, hwndCtl;
    BOOL vertical;
    INT arrowSize, thumbSize, thumbPos;
    RECT rect;
    HDC hdc;

    SCROLLBAR_INFO *infoPtr = SCROLL_GetScrollBarInfo( hwnd, nBar );
    if (!infoPtr) return;
    if ((SCROLL_trackHitTest == SCROLL_NOWHERE) && (msg != WM_LBUTTONDOWN))
		  return;

    if (nBar == SB_CTL && (GetWindowLongW( hwnd, GWL_STYLE ) & (SBS_SIZEGRIP | SBS_SIZEBOX)))
    {
        switch(msg)
        {
            case WM_LBUTTONDOWN:  /* Initialise mouse tracking */
                HideCaret(hwnd);  /* hide caret while holding down LBUTTON */
                SetCapture( hwnd );
                prevPt = pt;
                SCROLL_trackHitTest  = hittest = SCROLL_THUMB;
                break;
            case WM_MOUSEMOVE:
                GetClientRect(GetParent(GetParent(hwnd)),&rect);
                prevPt = pt;
                break;
            case WM_LBUTTONUP:
                ReleaseCapture();
                SCROLL_trackHitTest  = hittest = SCROLL_NOWHERE;
                if (hwnd==GetFocus()) ShowCaret(hwnd);
                break;
            case WM_SYSTIMER:
                pt = prevPt;
                break;
        }
        return;
    }

    hdc = GetDCEx( hwnd, 0, DCX_CACHE | ((nBar == SB_CTL) ? 0 : DCX_WINDOW));
    vertical = SCROLL_GetScrollBarRect( hwnd, nBar, &rect,
                                        &arrowSize, &thumbSize, &thumbPos );
    hwndOwner = (nBar == SB_CTL) ? GetParent(hwnd) : hwnd;
    hwndCtl   = (nBar == SB_CTL) ? hwnd : 0;

    switch(msg)
    {
      case WM_LBUTTONDOWN:  /* Initialise mouse tracking */
          HideCaret(hwnd);  /* hide caret while holding down LBUTTON */
          SCROLL_trackVertical = vertical;
          SCROLL_trackHitTest  = hittest = SCROLL_HitTest( hwnd, nBar, pt, FALSE );
          lastClickPos  = vertical ? (pt.y - rect.top) : (pt.x - rect.left);
          lastMousePos  = lastClickPos;
          trackThumbPos = thumbPos;
          prevPt = pt;
          if (nBar == SB_CTL && (GetWindowLongA(hwnd, GWL_STYLE) & WS_TABSTOP)) SetFocus( hwnd );
          SetCapture( hwnd );
          break;

      case WM_MOUSEMOVE:
          hittest = SCROLL_HitTest( hwnd, nBar, pt, TRUE );
          prevPt = pt;
          break;

      case WM_LBUTTONUP:
          hittest = SCROLL_NOWHERE;
          ReleaseCapture();
          /* if scrollbar has focus, show back caret */
          if (hwnd==GetFocus()) ShowCaret(hwnd);
          break;

      case WM_SYSTIMER:
          pt = prevPt;
          hittest = SCROLL_HitTest( hwnd, nBar, pt, FALSE );
          break;

      default:
          return;  /* Should never happen */
    }

    TRACE("Event: hwnd=%p bar=%d msg=%s pt=%ld,%ld hit=%d\n",
          hwnd, nBar, SPY_GetMsgName(msg,hwnd), pt.x, pt.y, hittest );

    switch(SCROLL_trackHitTest)
    {
    case SCROLL_NOWHERE:  /* No tracking in progress */
        break;

    case SCROLL_TOP_ARROW:
        SCROLL_DrawArrows( hdc, infoPtr, &rect, arrowSize, vertical,
                           (hittest == SCROLL_trackHitTest), FALSE );
        if (hittest == SCROLL_trackHitTest)
        {
            if ((msg == WM_LBUTTONDOWN) || (msg == WM_SYSTIMER))
            {
                SendMessageA( hwndOwner, vertical ? WM_VSCROLL : WM_HSCROLL,
                                SB_LINEUP, (LPARAM)hwndCtl );
	    }

	    SetSystemTimer( hwnd, SCROLL_TIMER, (msg == WM_LBUTTONDOWN) ?
			    SCROLL_FIRST_DELAY : SCROLL_REPEAT_DELAY,
			    (TIMERPROC)0 );
        }
        else KillSystemTimer( hwnd, SCROLL_TIMER );
        break;

    case SCROLL_TOP_RECT:
        SCROLL_DrawInterior( hwnd, hdc, nBar, &rect, arrowSize, thumbSize,
                             thumbPos, infoPtr->flags, vertical,
                             (hittest == SCROLL_trackHitTest), FALSE );
        if (hittest == SCROLL_trackHitTest)
        {
            if ((msg == WM_LBUTTONDOWN) || (msg == WM_SYSTIMER))
            {
                SendMessageA( hwndOwner, vertical ? WM_VSCROLL : WM_HSCROLL,
                                SB_PAGEUP, (LPARAM)hwndCtl );
            }
            SetSystemTimer( hwnd, SCROLL_TIMER, (msg == WM_LBUTTONDOWN) ?
                              SCROLL_FIRST_DELAY : SCROLL_REPEAT_DELAY,
                              (TIMERPROC)0 );
        }
        else KillSystemTimer( hwnd, SCROLL_TIMER );
        break;

    case SCROLL_THUMB:
        if (msg == WM_LBUTTONDOWN)
        {
            SCROLL_TrackingWin = hwnd;
            SCROLL_TrackingBar = nBar;
            SCROLL_TrackingPos = trackThumbPos + lastMousePos - lastClickPos;
            SCROLL_TrackingVal = SCROLL_GetThumbVal( infoPtr, &rect,
                                                        vertical,
                                                        SCROLL_TrackingPos );
	    if (!SCROLL_MovingThumb)
		SCROLL_DrawMovingThumb(hdc, &rect, vertical, arrowSize, thumbSize);
        }
        else if (msg == WM_LBUTTONUP)
        {
	    if (SCROLL_MovingThumb)
		SCROLL_DrawMovingThumb(hdc, &rect, vertical, arrowSize, thumbSize);
            SCROLL_TrackingWin = 0;
            SCROLL_DrawInterior( hwnd, hdc, nBar, &rect, arrowSize, thumbSize,
                                 thumbPos, infoPtr->flags, vertical,
                                 FALSE, FALSE );
        }
        else  /* WM_MOUSEMOVE */
        {
            UINT pos;

            if (!SCROLL_PtInRectEx( &rect, pt, vertical )) pos = lastClickPos;
            else
	    {
		pt = SCROLL_ClipPos( &rect, pt );
		pos = vertical ? (pt.y - rect.top) : (pt.x - rect.left);
	    }
            if ( (pos != lastMousePos) || (!SCROLL_MovingThumb) )
            {
		if (SCROLL_MovingThumb)
		    SCROLL_DrawMovingThumb( hdc, &rect, vertical,
                                        arrowSize, thumbSize );
                lastMousePos = pos;
                SCROLL_TrackingPos = trackThumbPos + pos - lastClickPos;
                SCROLL_TrackingVal = SCROLL_GetThumbVal( infoPtr, &rect,
                                                         vertical,
                                                         SCROLL_TrackingPos );
                SendMessageA( hwndOwner, vertical ? WM_VSCROLL : WM_HSCROLL,
                                MAKEWPARAM( SB_THUMBTRACK, SCROLL_TrackingVal),
                                (LPARAM)hwndCtl );
		if (!SCROLL_MovingThumb)
		    SCROLL_DrawMovingThumb( hdc, &rect, vertical,
                                        arrowSize, thumbSize );
            }
        }
        break;

    case SCROLL_BOTTOM_RECT:
        SCROLL_DrawInterior( hwnd, hdc, nBar, &rect, arrowSize, thumbSize,
                             thumbPos, infoPtr->flags, vertical,
                             FALSE, (hittest == SCROLL_trackHitTest) );
        if (hittest == SCROLL_trackHitTest)
        {
            if ((msg == WM_LBUTTONDOWN) || (msg == WM_SYSTIMER))
            {
                SendMessageA( hwndOwner, vertical ? WM_VSCROLL : WM_HSCROLL,
                                SB_PAGEDOWN, (LPARAM)hwndCtl );
            }
            SetSystemTimer( hwnd, SCROLL_TIMER, (msg == WM_LBUTTONDOWN) ?
                              SCROLL_FIRST_DELAY : SCROLL_REPEAT_DELAY,
                              (TIMERPROC)0 );
        }
        else KillSystemTimer( hwnd, SCROLL_TIMER );
        break;

    case SCROLL_BOTTOM_ARROW:
        SCROLL_DrawArrows( hdc, infoPtr, &rect, arrowSize, vertical,
                           FALSE, (hittest == SCROLL_trackHitTest) );
        if (hittest == SCROLL_trackHitTest)
        {
            if ((msg == WM_LBUTTONDOWN) || (msg == WM_SYSTIMER))
            {
                SendMessageA( hwndOwner, vertical ? WM_VSCROLL : WM_HSCROLL,
                                SB_LINEDOWN, (LPARAM)hwndCtl );
	    }

	    SetSystemTimer( hwnd, SCROLL_TIMER, (msg == WM_LBUTTONDOWN) ?
			    SCROLL_FIRST_DELAY : SCROLL_REPEAT_DELAY,
			    (TIMERPROC)0 );
        }
        else KillSystemTimer( hwnd, SCROLL_TIMER );
        break;
    }

    if (msg == WM_LBUTTONDOWN)
    {

        if (hittest == SCROLL_THUMB)
        {
            UINT val = SCROLL_GetThumbVal( infoPtr, &rect, vertical,
                                 trackThumbPos + lastMousePos - lastClickPos );
            SendMessageA( hwndOwner, vertical ? WM_VSCROLL : WM_HSCROLL,
                            MAKEWPARAM( SB_THUMBTRACK, val ), (LPARAM)hwndCtl );
        }
    }

    if (msg == WM_LBUTTONUP)
    {
	hittest = SCROLL_trackHitTest;
	SCROLL_trackHitTest = SCROLL_NOWHERE;  /* Terminate tracking */

        if (hittest == SCROLL_THUMB)
        {
            UINT val = SCROLL_GetThumbVal( infoPtr, &rect, vertical,
                                 trackThumbPos + lastMousePos - lastClickPos );
            SendMessageA( hwndOwner, vertical ? WM_VSCROLL : WM_HSCROLL,
                            MAKEWPARAM( SB_THUMBPOSITION, val ), (LPARAM)hwndCtl );
        }
        SendMessageA( hwndOwner, vertical ? WM_VSCROLL : WM_HSCROLL,
                          SB_ENDSCROLL, (LPARAM)hwndCtl );
    }

    ReleaseDC( hwnd, hdc );
}


/***********************************************************************
 *           SCROLL_TrackScrollBar
 *
 * Track a mouse button press on a scroll-bar.
 * pt is in screen-coordinates for non-client scroll bars.
 */
void SCROLL_TrackScrollBar( HWND hwnd, INT scrollbar, POINT pt )
{
    MSG msg;
    INT xoffset = 0, yoffset = 0;

    if (scrollbar != SB_CTL)
    {
        WND *wndPtr = WIN_GetPtr( hwnd );
        if (!wndPtr || wndPtr == WND_OTHER_PROCESS) return;
        xoffset = wndPtr->rectClient.left - wndPtr->rectWindow.left;
        yoffset = wndPtr->rectClient.top - wndPtr->rectWindow.top;
        WIN_ReleasePtr( wndPtr );
        ScreenToClient( hwnd, &pt );
        pt.x += xoffset;
        pt.y += yoffset;
    }

    SCROLL_HandleScrollEvent( hwnd, scrollbar, WM_LBUTTONDOWN, pt );

    do
    {
        if (!GetMessageW( &msg, 0, 0, 0 )) break;
        if (CallMsgFilterW( &msg, MSGF_SCROLLBAR )) continue;
        switch(msg.message)
        {
        case WM_LBUTTONUP:
        case WM_MOUSEMOVE:
        case WM_SYSTIMER:
            pt.x = (short)LOWORD(msg.lParam) + xoffset;
            pt.y = (short)HIWORD(msg.lParam) + yoffset;
            SCROLL_HandleScrollEvent( hwnd, scrollbar, msg.message, pt );
            break;
        default:
            TranslateMessage( &msg );
            DispatchMessageW( &msg );
            break;
        }
        if (!IsWindow( hwnd ))
        {
            ReleaseCapture();
            break;
        }
    } while (msg.message != WM_LBUTTONUP);
}


/***********************************************************************
 *           SCROLL_CreateScrollBar
 *
 *  Create a scroll bar
 */
static void SCROLL_CreateScrollBar(
HWND hwnd /* [in] Handle of window with scrollbar(s) */,
LPCREATESTRUCTW lpCreate /* [in] The style and place of the scroll bar */)
{
    LPSCROLLBAR_INFO info = SCROLL_GetScrollBarInfo(hwnd, SB_CTL);
    if (!info) return;

    TRACE("hwnd=%p lpCreate=%p\n", hwnd, lpCreate);

    if (lpCreate->style & WS_DISABLED)
    {
        info->flags = ESB_DISABLE_BOTH;
        TRACE("Created WS_DISABLED scrollbar\n");
    }


    if (lpCreate->style & (SBS_SIZEGRIP | SBS_SIZEBOX))
    {
        if (lpCreate->style & SBS_SIZEBOXTOPLEFTALIGN)
            MoveWindow( hwnd, lpCreate->x, lpCreate->y, GetSystemMetrics(SM_CXVSCROLL)+1,
                        GetSystemMetrics(SM_CYHSCROLL)+1, FALSE );
        else if(lpCreate->style & SBS_SIZEBOXBOTTOMRIGHTALIGN)
            MoveWindow( hwnd, lpCreate->x+lpCreate->cx-GetSystemMetrics(SM_CXVSCROLL)-1, 
                        lpCreate->y+lpCreate->cy-GetSystemMetrics(SM_CYHSCROLL)-1,
                        GetSystemMetrics(SM_CXVSCROLL)+1,
                        GetSystemMetrics(SM_CYHSCROLL)+1, FALSE );
    }
    else if (lpCreate->style & SBS_VERT)
    {
        if (lpCreate->style & SBS_LEFTALIGN)
            MoveWindow( hwnd, lpCreate->x, lpCreate->y,
                        GetSystemMetrics(SM_CXVSCROLL)+1, lpCreate->cy, FALSE );
        else if (lpCreate->style & SBS_RIGHTALIGN)
            MoveWindow( hwnd,
                        lpCreate->x+lpCreate->cx-GetSystemMetrics(SM_CXVSCROLL)-1,
                        lpCreate->y,
                        GetSystemMetrics(SM_CXVSCROLL)+1, lpCreate->cy, FALSE );
    }
    else  /* SBS_HORZ */
    {
        if (lpCreate->style & SBS_TOPALIGN)
            MoveWindow( hwnd, lpCreate->x, lpCreate->y,
                        lpCreate->cx, GetSystemMetrics(SM_CYHSCROLL)+1, FALSE );
        else if (lpCreate->style & SBS_BOTTOMALIGN)
            MoveWindow( hwnd,
                        lpCreate->x,
                        lpCreate->y+lpCreate->cy-GetSystemMetrics(SM_CYHSCROLL)-1,
                        lpCreate->cx, GetSystemMetrics(SM_CYHSCROLL)+1, FALSE );
    }
}


/*************************************************************************
 *           SCROLL_GetScrollInfo
 *
 *  Internal helper for the API function
 */
static BOOL SCROLL_GetScrollInfo(
HWND hwnd /* [in] Handle of window with scrollbar(s) */,
INT nBar /* [in] One of SB_HORZ, SB_VERT, or SB_CTL */,
LPSCROLLINFO info /* [in/out] (fMask specifies which values to retrieve) */)
{
    LPSCROLLBAR_INFO infoPtr;

    /* handle invalid data structure */
    if (!SCROLL_ScrollInfoValid(info)
        || !(infoPtr = SCROLL_GetScrollBarInfo(hwnd, nBar)))
            return FALSE;

    /* fill in the desired scroll info structure */
    if (info->fMask & SIF_PAGE) info->nPage = infoPtr->page;
    if (info->fMask & SIF_POS) info->nPos = infoPtr->curVal;
    if ((info->fMask & SIF_TRACKPOS) && (info->cbSize == sizeof(*info)))
        info->nTrackPos = (SCROLL_TrackingWin == WIN_GetFullHandle(hwnd)) ? SCROLL_TrackingVal : infoPtr->curVal;
    if (info->fMask & SIF_RANGE)
    {
        info->nMin = infoPtr->minVal;
        info->nMax = infoPtr->maxVal;
    }

    return (info->fMask & SIF_ALL) != 0;
}


/*************************************************************************
 *           SCROLL_GetScrollPos
 *
 *  Internal helper for the API function
 */
static INT SCROLL_GetScrollPos(
HWND hwnd /* [in] Handle of window with scrollbar(s) */,
INT nBar /* [in] One of SB_HORZ, SB_VERT, or SB_CTL */)
{
    LPSCROLLBAR_INFO infoPtr = SCROLL_GetScrollBarInfo(hwnd, nBar);
    return infoPtr ? infoPtr->curVal: 0;
}


/*************************************************************************
 *           SCROLL_GetScrollRange
 *
 *  Internal helper for the API function
 *
 * RETURNS STD
 */
static BOOL SCROLL_GetScrollRange(
HWND hwnd, /* [in] Handle of window */
INT nBar, /* [in] One of SB_HORZ, SB_VERT, or SB_CTL  */
LPINT lpMin, /* [out] Where to store minimum value */
LPINT lpMax /* [out] Where to store maximum value */)
{
    LPSCROLLBAR_INFO infoPtr = SCROLL_GetScrollBarInfo(hwnd, nBar);

    if (lpMin) *lpMin = infoPtr ? infoPtr->minVal : 0;
    if (lpMax) *lpMax = infoPtr ? infoPtr->maxVal : 0;

    return infoPtr ? TRUE : FALSE;
}



/***********************************************************************
 *           ScrollBarWndProc
 */
static LRESULT WINAPI ScrollBarWndProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    if (!IsWindow( hwnd )) return 0;

    switch(message)
    {
    case WM_CREATE:
        SCROLL_CreateScrollBar(hwnd, (LPCREATESTRUCTW)lParam);
        if (!hUpArrow) SCROLL_LoadBitmaps();
        break;

    case WM_ENABLE:
        {
	    SCROLLBAR_INFO *infoPtr;
	    if ((infoPtr = SCROLL_GetScrollBarInfo( hwnd, SB_CTL )))
	    {
		infoPtr->flags = wParam ? ESB_ENABLE_BOTH : ESB_DISABLE_BOTH;
		SCROLL_RefreshScrollBar(hwnd, SB_CTL, TRUE, TRUE);
	    }
	}
	return 0;

    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
        {
	    POINT pt;
	    pt.x = (short)LOWORD(lParam);
	    pt.y = (short)HIWORD(lParam);
            SCROLL_TrackScrollBar( hwnd, SB_CTL, pt );
	}
        break;
    case WM_LBUTTONUP:
    case WM_MOUSEMOVE:
    case WM_SYSTIMER:
        {
            POINT pt;
            pt.x = (short)LOWORD(lParam);
            pt.y = (short)HIWORD(lParam);
            SCROLL_HandleScrollEvent( hwnd, SB_CTL, message, pt );
        }
        break;

    case WM_KEYDOWN:
        SCROLL_HandleKbdEvent(hwnd, wParam, lParam);
        break;

    case WM_KEYUP:
        ShowCaret(hwnd);
        break;

    case WM_SETFOCUS:
        {
            /* Create a caret when a ScrollBar get focus */
            RECT rect;
            int arrowSize, thumbSize, thumbPos, vertical;
            vertical = SCROLL_GetScrollBarRect( hwnd, SB_CTL, &rect,
                                                &arrowSize, &thumbSize, &thumbPos );
            if (!vertical)
            {
                CreateCaret(hwnd, (HBITMAP)1, thumbSize-2, rect.bottom-rect.top-2);
                SetCaretPos(thumbPos+1, rect.top+1);
            }
            else
            {
                CreateCaret(hwnd, (HBITMAP)1, rect.right-rect.left-2,thumbSize-2);
                SetCaretPos(rect.top+1, thumbPos+1);
            }
            ShowCaret(hwnd);
        }
        break;

    case WM_KILLFOCUS:
        {
            RECT rect;
            int arrowSize, thumbSize, thumbPos, vertical;
            vertical = SCROLL_GetScrollBarRect( hwnd, SB_CTL, &rect,&arrowSize, &thumbSize, &thumbPos );
            if (!vertical){
                rect.left=thumbPos+1;
                rect.right=rect.left+thumbSize;
            }
            else
            {
                rect.top=thumbPos+1;
                rect.bottom=rect.top+thumbSize;
            }
            HideCaret(hwnd);
            InvalidateRect(hwnd,&rect,0);
            DestroyCaret();
        }
        break;

    case WM_ERASEBKGND:
         return 1;

    case WM_GETDLGCODE:
         return DLGC_WANTARROWS; /* Windows returns this value */

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = wParam ? (HDC)wParam : BeginPaint(hwnd, &ps);
            if (GetWindowLongW( hwnd, GWL_STYLE ) & SBS_SIZEGRIP)
            {
                SCROLL_DrawSizeGrip( hwnd, hdc);
            }
            else if (GetWindowLongW( hwnd, GWL_STYLE ) & SBS_SIZEBOX)
            {
                RECT rc;
                GetClientRect( hwnd, &rc );
                FillRect( hdc, &rc, GetSysColorBrush(COLOR_SCROLLBAR) );
            }
            else
                SCROLL_DrawScrollBar( hwnd, hdc, SB_CTL, TRUE, TRUE );
            if (!wParam) EndPaint(hwnd, &ps);
        }
        break;

    case SBM_SETPOS16:
    case SBM_SETPOS:
        return SetScrollPos( hwnd, SB_CTL, wParam, (BOOL)lParam );

    case SBM_GETPOS16:
    case SBM_GETPOS:
       return SCROLL_GetScrollPos(hwnd, SB_CTL);

    case SBM_SETRANGE16:
        SetScrollRange( hwnd, SB_CTL, LOWORD(lParam), HIWORD(lParam),
                          wParam  /* FIXME: Is this correct? */ );
        return 0;

    case SBM_SETRANGE:
        {
            INT oldPos = SCROLL_GetScrollPos( hwnd, SB_CTL );
            SetScrollRange( hwnd, SB_CTL, wParam, lParam, FALSE );
            if (oldPos != SCROLL_GetScrollPos( hwnd, SB_CTL )) return oldPos;
        }
        return 0;

    case SBM_GETRANGE16:
        FIXME("don't know how to handle SBM_GETRANGE16 (wp=%04x,lp=%08lx)\n", wParam, lParam );
        return 0;

    case SBM_GETRANGE:
        return SCROLL_GetScrollRange(hwnd, SB_CTL, (LPINT)wParam, (LPINT)lParam);

    case SBM_ENABLE_ARROWS16:
    case SBM_ENABLE_ARROWS:
        return EnableScrollBar( hwnd, SB_CTL, wParam );

    case SBM_SETRANGEREDRAW:
        {
            INT oldPos = SCROLL_GetScrollPos( hwnd, SB_CTL );
            SetScrollRange( hwnd, SB_CTL, wParam, lParam, TRUE );
            if (oldPos != SCROLL_GetScrollPos( hwnd, SB_CTL )) return oldPos;
        }
        return 0;

    case SBM_SETSCROLLINFO:
        return SetScrollInfo( hwnd, SB_CTL, (SCROLLINFO *)lParam, wParam );

    case SBM_GETSCROLLINFO:
        return SCROLL_GetScrollInfo(hwnd, SB_CTL, (SCROLLINFO *)lParam);

    case 0x00e5:
    case 0x00e7:
    case 0x00e8:
    case 0x00eb:
    case 0x00ec:
    case 0x00ed:
    case 0x00ee:
    case 0x00ef:
        ERR("unknown Win32 msg %04x wp=%08x lp=%08lx\n",
		    message, wParam, lParam );
        break;

    default:
        if (message >= WM_USER)
            WARN("unknown msg %04x wp=%04x lp=%08lx\n",
			 message, wParam, lParam );
        return DefWindowProcW( hwnd, message, wParam, lParam );
    }
    return 0;
}


/*************************************************************************
 *           SetScrollInfo   (USER32.@)
 * SetScrollInfo can be used to set the position, upper bound,
 * lower bound, and page size of a scrollbar control.
 *
 * RETURNS
 *    Scrollbar position
 *
 * NOTE
 *    For 100 lines of text to be displayed in a window of 25 lines,
 *  one would for instance use info->nMin=0, info->nMax=75
 *  (corresponding to the 76 different positions of the window on
 *  the text), and info->nPage=25.
 */
INT WINAPI SetScrollInfo(
HWND hwnd /* [in] Handle of window whose scrollbar will be affected */,
INT nBar /* [in] One of SB_HORZ, SB_VERT, or SB_CTL */,
const SCROLLINFO *info /* [in] Specifies what to change and new values */,
BOOL bRedraw /* [in] Should scrollbar be redrawn afterwards ? */)
{
    INT action;
    INT retVal = SCROLL_SetScrollInfo( hwnd, nBar, info, &action );

    if( action & SA_SSI_HIDE )
	SCROLL_ShowScrollBar( hwnd, nBar, FALSE, FALSE );
    else
    {
	if( action & SA_SSI_SHOW )
	    if( SCROLL_ShowScrollBar( hwnd, nBar, TRUE, TRUE ) )
		return retVal; /* SetWindowPos() already did the painting */

	if( bRedraw )
	    SCROLL_RefreshScrollBar( hwnd, nBar, TRUE, TRUE );
	else if( action & SA_SSI_REPAINT_ARROWS )
	    SCROLL_RefreshScrollBar( hwnd, nBar, TRUE, FALSE );
    }
    return retVal;
}

INT SCROLL_SetScrollInfo( HWND hwnd, INT nBar,
			    const SCROLLINFO *info, INT *action  )
{
    /* Update the scrollbar state and set action flags according to
     * what has to be done graphics wise. */

    SCROLLBAR_INFO *infoPtr;
    UINT new_flags;
    BOOL bChangeParams = FALSE; /* don't show/hide scrollbar if params don't change */

   *action = 0;

    if (!(infoPtr = SCROLL_GetScrollBarInfo(hwnd, nBar))) return 0;
    if (info->fMask & ~(SIF_ALL | SIF_DISABLENOSCROLL)) return 0;
    if ((info->cbSize != sizeof(*info)) &&
        (info->cbSize != sizeof(*info)-sizeof(info->nTrackPos))) return 0;

    if (TRACE_ON(scroll))
    {
        TRACE("hwnd=%p bar=%d", hwnd, nBar);
        if (info->fMask & SIF_PAGE) TRACE( " page=%d", info->nPage );
        if (info->fMask & SIF_POS) TRACE( " pos=%d", info->nPos );
        if (info->fMask & SIF_RANGE) TRACE( " min=%d max=%d", info->nMin, info->nMax );
        TRACE("\n");
    }

    /* Set the page size */

    if (info->fMask & SIF_PAGE)
    {
	if( infoPtr->page != info->nPage )
	{
            infoPtr->page = info->nPage;
	   *action |= SA_SSI_REFRESH;
           bChangeParams = TRUE;
	}
    }

    /* Set the scroll pos */

    if (info->fMask & SIF_POS)
    {
	if( infoPtr->curVal != info->nPos )
	{
	    infoPtr->curVal = info->nPos;
	   *action |= SA_SSI_REFRESH;
	}
    }

    /* Set the scroll range */

    if (info->fMask & SIF_RANGE)
    {
        /* Invalid range -> range is set to (0,0) */
        if ((info->nMin > info->nMax) ||
            ((UINT)(info->nMax - info->nMin) >= 0x80000000))
        {
            infoPtr->minVal = 0;
            infoPtr->maxVal = 0;
            bChangeParams = TRUE;
        }
        else
        {
	    if( infoPtr->minVal != info->nMin ||
		infoPtr->maxVal != info->nMax )
	    {
                *action |= SA_SSI_REFRESH;
                infoPtr->minVal = info->nMin;
                infoPtr->maxVal = info->nMax;
                bChangeParams = TRUE;
	    }
        }
    }

    /* Make sure the page size is valid */

    if (infoPtr->page < 0) infoPtr->page = 0;
    else if (infoPtr->page > infoPtr->maxVal - infoPtr->minVal + 1 )
        infoPtr->page = infoPtr->maxVal - infoPtr->minVal + 1;

    /* Make sure the pos is inside the range */

    if (infoPtr->curVal < infoPtr->minVal)
        infoPtr->curVal = infoPtr->minVal;
    else if (infoPtr->curVal > infoPtr->maxVal - max( infoPtr->page-1, 0 ))
        infoPtr->curVal = infoPtr->maxVal - max( infoPtr->page-1, 0 );

    TRACE("    new values: page=%d pos=%d min=%d max=%d\n",
		 infoPtr->page, infoPtr->curVal,
		 infoPtr->minVal, infoPtr->maxVal );

    /* don't change the scrollbar state if SetScrollInfo
     * is just called with SIF_DISABLENOSCROLL
     */
    if(!(info->fMask & SIF_ALL)) goto done;

    /* Check if the scrollbar should be hidden or disabled */

    if (info->fMask & (SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL))
    {
        new_flags = infoPtr->flags;
        if (infoPtr->minVal >= infoPtr->maxVal - max( infoPtr->page-1, 0 ))
        {
            /* Hide or disable scroll-bar */
            if (info->fMask & SIF_DISABLENOSCROLL)
	    {
                new_flags = ESB_DISABLE_BOTH;
	       *action |= SA_SSI_REFRESH;
	    }
            else if ((nBar != SB_CTL) && bChangeParams)
	    {
		*action = SA_SSI_HIDE;
		goto done;
            }
        }
        else  /* Show and enable scroll-bar */
        {
	    new_flags = 0;
            if ((nBar != SB_CTL) && bChangeParams)
		*action |= SA_SSI_SHOW;
        }

        if (infoPtr->flags != new_flags) /* check arrow flags */
        {
            infoPtr->flags = new_flags;
           *action |= SA_SSI_REPAINT_ARROWS;
        }
    }

done:
    /* Return current position */

    return infoPtr->curVal;
}


/*************************************************************************
 *           GetScrollInfo   (USER32.@)
 *
 * GetScrollInfo can be used to retrieve the position, upper bound,
 * lower bound, and page size of a scrollbar control.
 *
 * RETURNS STD
 */
BOOL WINAPI GetScrollInfo(
HWND hwnd /* [in] Handle of window with scrollbar(s) */,
INT nBar /* [in] One of SB_HORZ, SB_VERT, or SB_CTL */,
LPSCROLLINFO info /* [in/out] (fMask specifies which values to retrieve) */)
{
    TRACE("hwnd=%p nBar=%d info=%p\n", hwnd, nBar, info);

    /* Refer SB_CTL requests to the window */
    if (nBar == SB_CTL)
        return SendMessageA(hwnd, SBM_GETSCROLLINFO, (WPARAM)0, (LPARAM)info);
    else
        return SCROLL_GetScrollInfo(hwnd, nBar, info);
}


/*************************************************************************
 *           SetScrollPos   (USER32.@)
 *
 * RETURNS
 *    Success: Scrollbar position
 *    Failure: 0
 *
 * REMARKS
 *    Note the ambiguity when 0 is returned.  Use GetLastError
 *    to make sure there was an error (and to know which one).
 */
INT WINAPI SetScrollPos(
HWND hwnd /* [in] Handle of window whose scrollbar will be affected */,
INT nBar /* [in] One of SB_HORZ, SB_VERT, or SB_CTL */,
INT nPos /* [in] New value */,
BOOL bRedraw /* [in] Should scrollbar be redrawn afterwards ? */ )
{
    SCROLLINFO info;
    SCROLLBAR_INFO *infoPtr;
    INT oldPos;

    if (!(infoPtr = SCROLL_GetScrollBarInfo( hwnd, nBar ))) return 0;
    oldPos      = infoPtr->curVal;
    info.cbSize = sizeof(info);
    info.nPos   = nPos;
    info.fMask  = SIF_POS;
    SetScrollInfo( hwnd, nBar, &info, bRedraw );
    return oldPos;
}


/*************************************************************************
 *           GetScrollPos   (USER32.@)
 *
 * RETURNS
 *    Success: Current position
 *    Failure: 0
 *
 * REMARKS
 *    There is ambiguity when 0 is returned.  Use GetLastError
 *    to make sure there was an error (and to know which one).
 */
INT WINAPI GetScrollPos(
HWND hwnd /* [in] Handle of window with scrollbar(s) */,
INT nBar /* [in] One of SB_HORZ, SB_VERT, or SB_CTL */)
{
    TRACE("hwnd=%p nBar=%d\n", hwnd, nBar);

    /* Refer SB_CTL requests to the window */
    if (nBar == SB_CTL)
        return SendMessageA(hwnd, SBM_GETPOS, (WPARAM)0, (LPARAM)0);
    else
        return SCROLL_GetScrollPos(hwnd, nBar);
}


/*************************************************************************
 *           SetScrollRange   (USER32.@)
 *
 * RETURNS STD
 */
BOOL WINAPI SetScrollRange(
HWND hwnd, /* [in] Handle of window whose scrollbar will be affected */
INT nBar, /* [in] One of SB_HORZ, SB_VERT, or SB_CTL */
INT minVal, /* [in] New minimum value */
INT maxVal, /* [in] New maximum value */
BOOL bRedraw /* [in] Should scrollbar be redrawn afterwards ? */)
{
    SCROLLINFO info;

    info.cbSize = sizeof(info);
    info.nMin   = minVal;
    info.nMax   = maxVal;
    info.fMask  = SIF_RANGE;
    SetScrollInfo( hwnd, nBar, &info, bRedraw );
    return TRUE;
}


/*************************************************************************
 *	     SCROLL_SetNCSbState
 *
 * Updates both scrollbars at the same time. Used by MDI CalcChildScroll().
 */
INT SCROLL_SetNCSbState(HWND hwnd, int vMin, int vMax, int vPos,
                        int hMin, int hMax, int hPos)
{
    INT vA, hA;
    SCROLLINFO vInfo, hInfo;

    vInfo.cbSize = hInfo.cbSize = sizeof(SCROLLINFO);
    vInfo.nMin   = vMin;
    vInfo.nMax   = vMax;
    vInfo.nPos   = vPos;
    hInfo.nMin   = hMin;
    hInfo.nMax   = hMax;
    hInfo.nPos   = hPos;
    vInfo.fMask  = hInfo.fMask = SIF_RANGE | SIF_POS;

    SCROLL_SetScrollInfo( hwnd, SB_VERT, &vInfo, &vA );
    SCROLL_SetScrollInfo( hwnd, SB_HORZ, &hInfo, &hA );

    if( !SCROLL_ShowScrollBar( hwnd, SB_BOTH,
			      (hA & SA_SSI_SHOW),(vA & SA_SSI_SHOW) ) )
    {
	/* SetWindowPos() wasn't called, just redraw the scrollbars if needed */
	if( vA & SA_SSI_REFRESH )
	    SCROLL_RefreshScrollBar( hwnd, SB_VERT, FALSE, TRUE );

	if( hA & SA_SSI_REFRESH )
	    SCROLL_RefreshScrollBar( hwnd, SB_HORZ, FALSE, TRUE );
    }
    return 0;
}


/*************************************************************************
 *           GetScrollRange   (USER32.@)
 *
 * RETURNS STD
 */
BOOL WINAPI GetScrollRange(
HWND hwnd /* [in] Handle of window with scrollbar(s) */,
INT nBar /* [in] One of SB_HORZ, SB_VERT, or SB_CTL  */,
LPINT lpMin /* [out] Where to store minimum value */,
LPINT lpMax /* [out] Where to store maximum value */)
{
    TRACE("hwnd=%p nBar=%d lpMin=%p lpMax=%p\n", hwnd, nBar, lpMin, lpMax);

    /* Refer SB_CTL requests to the window */
    if (nBar == SB_CTL)
        return SendMessageA(hwnd, SBM_GETRANGE, (WPARAM)lpMin, (LPARAM)lpMax);
    else
        return SCROLL_GetScrollRange(hwnd, nBar, lpMin, lpMax);
}


/*************************************************************************
 *           SCROLL_ShowScrollBar()
 *
 * Back-end for ShowScrollBar(). Returns FALSE if no action was taken.
 */
BOOL SCROLL_ShowScrollBar( HWND hwnd, INT nBar,
			     BOOL fShowH, BOOL fShowV )
{
    LONG style = GetWindowLongW( hwnd, GWL_STYLE );

    TRACE("hwnd=%p bar=%d horz=%d, vert=%d\n", hwnd, nBar, fShowH, fShowV );

    switch(nBar)
    {
    case SB_CTL:
        ShowWindow( hwnd, fShowH ? SW_SHOW : SW_HIDE );
        return TRUE;

    case SB_BOTH:
    case SB_HORZ:
        if (fShowH)
        {
            fShowH = !(style & WS_HSCROLL);
            style |= WS_HSCROLL;
        }
        else  /* hide it */
        {
            fShowH = (style & WS_HSCROLL);
            style &= ~WS_HSCROLL;
        }
        if( nBar == SB_HORZ ) {
            fShowV = FALSE;
            break;
        }
	/* fall through */

    case SB_VERT:
        if (fShowV)
        {
            fShowV = !(style & WS_VSCROLL);
            style |= WS_VSCROLL;
        }
	else  /* hide it */
        {
            fShowV = (style & WS_VSCROLL);
            style &= ~WS_VSCROLL;
        }
        if ( nBar == SB_VERT )
           fShowH = FALSE;
        break;

    default:
        return FALSE;  /* Nothing to do! */
    }

    if( fShowH || fShowV ) /* frame has been changed, let the window redraw itself */
    {
        WIN_SetStyle( hwnd, style );
	SetWindowPos( hwnd, 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE
                    | SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED );
        return TRUE;
    }
    return FALSE; /* no frame changes */
}


/*************************************************************************
 *           ShowScrollBar   (USER32.@)
 *
 * RETURNS STD
 */
BOOL WINAPI ShowScrollBar(
HWND hwnd, /* [in] Handle of window whose scrollbar(s) will be affected   */
INT nBar, /* [in] One of SB_HORZ, SB_VERT, SB_BOTH or SB_CTL */
BOOL fShow /* [in] TRUE = show, FALSE = hide  */)
{
    SCROLL_ShowScrollBar( hwnd, nBar, (nBar == SB_VERT) ? 0 : fShow,
                                      (nBar == SB_HORZ) ? 0 : fShow );
    return TRUE;
}


/*************************************************************************
 *           EnableScrollBar   (USER32.@)
 */
BOOL WINAPI EnableScrollBar( HWND hwnd, INT nBar, UINT flags )
{
    BOOL bFineWithMe;
    SCROLLBAR_INFO *infoPtr;

    TRACE("%p %d %d\n", hwnd, nBar, flags );

    flags &= ESB_DISABLE_BOTH;

    if (nBar == SB_BOTH)
    {
	if (!(infoPtr = SCROLL_GetScrollBarInfo( hwnd, SB_VERT ))) return FALSE;
	if (!(bFineWithMe = (infoPtr->flags == flags)) )
	{
	    infoPtr->flags = flags;
	    SCROLL_RefreshScrollBar( hwnd, SB_VERT, TRUE, TRUE );
	}
	nBar = SB_HORZ;
    }
    else
	bFineWithMe = TRUE;

    if (!(infoPtr = SCROLL_GetScrollBarInfo( hwnd, nBar ))) return FALSE;
    if (bFineWithMe && infoPtr->flags == flags) return FALSE;
    infoPtr->flags = flags;

    SCROLL_RefreshScrollBar( hwnd, nBar, TRUE, TRUE );
    return TRUE;
}
