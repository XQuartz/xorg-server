/*
 *Copyright (C) 1994-2000 The XFree86 Project, Inc. All Rights Reserved.
 *
 *Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 *"Software"), to deal in the Software without restriction, including
 *without limitation the rights to use, copy, modify, merge, publish,
 *distribute, sublicense, and/or sell copies of the Software, and to
 *permit persons to whom the Software is furnished to do so, subject to
 *the following conditions:
 *
 *The above copyright notice and this permission notice shall be
 *included in all copies or substantial portions of the Software.
 *
 *THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *NONINFRINGEMENT. IN NO EVENT SHALL THE XFREE86 PROJECT BE LIABLE FOR
 *ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 *CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *Except as contained in this notice, the name of the XFree86 Project
 *shall not be used in advertising or otherwise to promote the sale, use
 *or other dealings in this Software without prior written authorization
 *from the XFree86 Project.
 *
 * Authors:	Dakshinamurthy Karra
 *		Suhaib M Siddiqi
 *		Peter Busch
 *		Harold L Hunt II
 */
/* $XFree86: xc/programs/Xserver/hw/xwin/wincursor.c,v 1.5 2002/07/05 09:19:26 alanh Exp $ */

#include "win.h"
#include "winmsg.h"
#include <cursorstr.h>
#include <mipointrst.h>
#include <servermd.h>


#ifndef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif

#define BYTE_COUNT(x) (((x) + 7) / 8)
#if 1
# define WIN_DEBUG_MSG winDebug
#else
# define WIN_DEBUG_MSG(...)
#endif

/*
 * Local function prototypes
 */

static void
winPointerWarpCursor (ScreenPtr pScreen, int x, int y);

static Bool
winCursorOffScreen (ScreenPtr *ppScreen, int *x, int *y);

static void
winCrossScreen (ScreenPtr pScreen, Bool fEntering);

miPointerScreenFuncRec g_winPointerCursorFuncs =
{
  winCursorOffScreen,
  winCrossScreen,
  winPointerWarpCursor
};


static void
winPointerWarpCursor (ScreenPtr pScreen, int x, int y)
{
  winScreenPriv(pScreen);
  RECT			rcClient;
  static Bool		s_fInitialWarp = TRUE;

  /* Discard first warp call */
  if (s_fInitialWarp)
    {
      /* First warp moves mouse to center of window, just ignore it */

      /* Don't ignore subsequent warps */
      s_fInitialWarp = FALSE;

      winErrorFVerb (2, "winPointerWarpCursor - Discarding first warp: %d %d\n",
	      x, y);
      
      return;
    }

  /* Only update the Windows cursor position if we are active */
  if (pScreenPriv->hwndScreen == GetForegroundWindow ())
    {
      /* Get the client area coordinates */
      GetClientRect (pScreenPriv->hwndScreen, &rcClient);
      
      /* Translate the client area coords to screen coords */
      MapWindowPoints (pScreenPriv->hwndScreen,
		       HWND_DESKTOP,
		       (LPPOINT)&rcClient,
		       2);
      
      /* 
       * Update the Windows cursor position so that we don't
       * immediately warp back to the current position.
       */
      SetCursorPos (rcClient.left + x, rcClient.top + y);
    }

  /* Call the mi warp procedure to do the actual warping in X. */
  miPointerWarpCursor (pScreen, x, y);
}

static Bool
winCursorOffScreen (ScreenPtr *ppScreen, int *x, int *y)
{
  return FALSE;
}

static void
winCrossScreen (ScreenPtr pScreen, Bool fEntering)
{
}

static unsigned char
reverse(unsigned char c)
{
  int i;
  unsigned char ret = 0;
  for (i = 0; i < 8; ++i)
    {
      ret |= ((c >> i)&1) << (7 - i);
    }
  return ret;
}
/*
 * Convert X cursor to Windows cursor
 * FIXME: Perhaps there are more smart code
 */
static HCURSOR
winLoadCursor (ScreenPtr pScreen, CursorPtr pCursor, int screen)
{
  winScreenPriv(pScreen);
  HCURSOR hCursor = NULL;
  unsigned char *pAnd;
  unsigned char *pXor;
  int nCX, nCY;
  int nBytes;
  double dForeY;
  double dBackY;
  BOOL fReverse;
  HBITMAP hAnd, hXor;
  ICONINFO ii;
  unsigned char *pColor, *pCur;
  int x, y;
  unsigned char bit;
  BITMAP bm;
  HDC hDC;
  int bpp, planes;
  unsigned short fg16, bg16;
  BITMAPV4HEADER bi;
  unsigned long *lpBits;

  /* We can use only White and Black, so calc brightness of color */
  dForeY = pCursor->foreRed*0.299 + pCursor->foreGreen*.587 + pCursor->foreBlue*.114;
  dBackY = pCursor->backRed*0.299 + pCursor->backGreen*.587 + pCursor->backBlue*.114;
  fReverse = dForeY < dBackY;
  
  if (pScreenPriv->cursor.sm_cx < pCursor->bits->width || 
      pScreenPriv->cursor.sm_cy < pCursor->bits->height)
    {
      winErrorFVerb (2, "winLoadCursor - Windows requires %dx%d cursor\n"
	      "\tbut X requires %dx%d\n",
	      pScreenPriv->cursor.sm_cx, pScreenPriv->cursor.sm_cy,
	      pCursor->bits->width, pCursor->bits->height);
    }

  /* round up to 8 pixel boundary so we can convert whole bytes */
  nBytes = BYTE_COUNT(pScreenPriv->cursor.sm_cx) * pScreenPriv->cursor.sm_cy;

  nCX = MIN(pScreenPriv->cursor.sm_cx, pCursor->bits->width);
  nCY = MIN(pScreenPriv->cursor.sm_cy, pCursor->bits->height);

  WIN_DEBUG_MSG("winLoadCursor: Win32: %dx%d X11: %dx%d\n", 
          pScreenPriv->cursor.sm_cx, pScreenPriv->cursor.sm_cy,
          pCursor->bits->width, pCursor->bits->height);

  pAnd = malloc (nBytes);
  memset (pAnd, 0xFF, nBytes);
  pXor = malloc (nBytes);
  memset (pXor, 0x00, nBytes);

  if (pCursor->bits->emptyMask)
    {
      int y;
      for (y = 0; y < nCY; ++y)
	{
	  int x;
	  int xmax = BYTE_COUNT(nCX);
	  for (x = 0; x < xmax; ++x)
	    {
	      int nWinPix = BYTE_COUNT(pScreenPriv->cursor.sm_cx) * y + x;
	      int nXPix = BYTE_COUNT(pCursor->bits->width) * y + x;
	      pAnd[nWinPix] = 0;
	      if (fReverse)
		{
		  pXor[nWinPix] = reverse (~pCursor->bits->source[nXPix]);
		}
	      else
		{
		  pXor[nWinPix] = reverse (pCursor->bits->source[nXPix]);
		}
	    }
	}
    }
  else
    {
      int y;
      for (y = 0; y < nCY; ++y)
	{
	  int x;
	  int xmax = BYTE_COUNT(nCX);
	  for (x = 0; x < xmax; ++x)
	    {
	      int nWinPix = BYTE_COUNT(pScreenPriv->cursor.sm_cx) * y + x;
	      int nXPix = BitmapBytePad(pCursor->bits->width) * y + x;

	      pAnd[nWinPix] = reverse (~pCursor->bits->mask[nXPix]);
	      if (fReverse)
		{
		  pXor[nWinPix] = reverse (~pCursor->bits->source[nXPix] & pCursor->bits->mask[nXPix]);
		}
	      else
		{
		  pXor[nWinPix] = reverse (pCursor->bits->source[nXPix] & pCursor->bits->mask[nXPix]);
		}
	    }
	}
    }

  /* See if we can make a real colored cursor instead of BnW */
  hDC = GetDC (GetDesktopWindow ());
  planes = GetDeviceCaps (hDC, PLANES);
  bpp = GetDeviceCaps (hDC, BITSPIXEL);
  ReleaseDC (GetDesktopWindow (), hDC);

  hXor = NULL;
  hAnd = NULL;
  hCursor = NULL;
  lpBits = NULL;

  /* We have a truecolor alpha-blended cursor and can use it! */
  if (bpp>=24 && planes==1 && pCursor->bits->argb) 
    {
      hAnd = CreateBitmap (pScreenPriv->cursor.sm_cx, pScreenPriv->cursor.sm_cy, 1, 1, pAnd);
      if (hAnd)
	{
	  memset (&bi, 0, sizeof (BITMAPV4HEADER));
	  bi.bV4Size = sizeof(BITMAPV4HEADER);
	  bi.bV4Width = pScreenPriv->cursor.sm_cx;
	  bi.bV4Height = pScreenPriv->cursor.sm_cy;
	  bi.bV4Planes = 1;
	  bi.bV4BitCount = 32;
	  bi.bV4V4Compression = BI_BITFIELDS;
	  bi.bV4RedMask = 0x00FF0000;
	  bi.bV4GreenMask = 0x0000FF00;
	  bi.bV4BlueMask = 0x000000FF;
	  bi.bV4AlphaMask = 0xFF000000; 
	  
	  hDC = GetDC(NULL);
	  if (hDC)
	    {
	      hXor = CreateDIBSection (hDC, (BITMAPINFO *)&bi, DIB_RGB_COLORS,
				       (void **)&lpBits, NULL, 0);
	      ReleaseDC (NULL, hDC);
	    }
	  
	  if (hXor && lpBits)
	    {
	      memset (lpBits, 0, 4*pScreenPriv->cursor.sm_cx*pScreenPriv->cursor.sm_cy);
	      for (y=0; y<nCY; y++)
		{
		  unsigned long *src, *dst;
		  src = &(pCursor->bits->argb[y * pCursor->bits->width]);
		  /*DIBs are upside down */
		  dst = &(lpBits[(pScreenPriv->cursor.sm_cy-y-1) * pScreenPriv->cursor.sm_cx]);
		  memcpy (dst, src, 4*nCX);
		}
	    }
	  else
	    {
	      DeleteObject (hAnd);
	      hAnd = NULL;
	    }
	} /* End if hAnd */
    } /* End if-truecolor-icon */

  /* Try and make a bicolor icon */
  if (bpp>=15 && planes==1 && !hXor && !hAnd)
    {
      if (bpp==15)
	bpp = 16;

      fg16 = (((pCursor->foreBlue>>10)&0x1f)<<10) | (((pCursor->foreRed>>10)&0x1f)<<5) | (((pCursor->foreGreen>>10)&0x1f)<<0);
      bg16 = (((pCursor->backBlue>>10)&0x1f)<<10) | (((pCursor->backRed>>10)&0x1f)<<5) | (((pCursor->backGreen>>10)&0x1f)<<0);
      
      hAnd = CreateBitmap (pScreenPriv->cursor.sm_cx, pScreenPriv->cursor.sm_cy, 1, 1, pAnd);
      pColor = (unsigned char *) calloc (pScreenPriv->cursor.sm_cx * pScreenPriv->cursor.sm_cy, bpp/8);
      if (!pColor)
	{
	  ErrorF ("winLoadCursor - Unable to allocate pColor.  Bailing.\n");
	  exit (-1);
	}
      pCur = pColor;
      for (y=0; y<pScreenPriv->cursor.sm_cy; y++)
	{
	  for (x=0; x<pScreenPriv->cursor.sm_cx; x++)
	    {
	      if (x>=nCX || y>=nCY) /* Outside of X11 icon bounds */
		{
		  switch (bpp)
		    {
		    case 32:
		      (*pCur++) = 0; /* Fall-thru */
		    case 24:
		      (*pCur++) = 0; /* Fall-thru */
		    case 16:
		      (*pCur++) = 0;
		      (*pCur++) = 0;
		      break;
		    }
		}
	      else /* Within X11 icon bounds */
		{
		  bit = pAnd[(y*pScreenPriv->cursor.sm_cx)/8 + (x/8)];
		  bit = bit & (1<<(7-(x&7)));
		  if (!bit) /* Within the cursor mask? */
		    {
		      int nXPix = BitmapBytePad(pCursor->bits->width) * y + (x/8);
		      bit = ~reverse(~pCursor->bits->source[nXPix] & pCursor->bits->mask[nXPix]);
		      bit = bit & (1<<(7-(x&7)));
		      if (bit) /* Draw foreground */
			{
			  switch (bpp)
			    {
			    case 32:
			    case 24:
			      (*pCur++) = pCursor->foreBlue>>8;
			      (*pCur++) = pCursor->foreGreen>>8;
			      (*pCur++) = pCursor->foreRed>>8;
			      if (bpp==32)
				(*pCur++) = 0;
			      break;
			    case 16:
			      *(pCur++) = (fg16 >> 8) & 255;
			      *(pCur++) = (fg16 & 255);
			      break;
			    }
			}
		      else /* Draw background */
			{
			  switch (bpp)
			    {
			    case 32:
			    case 24:
			      (*pCur++) = pCursor->backBlue>>8;
			      (*pCur++) = pCursor->backGreen>>8;
			      (*pCur++) = pCursor->backRed>>8;
			      if (bpp==32)
				(*pCur++) = 0;
			      break;
			    case 16:
			      *(pCur++) = (bg16 >> 8) & 255;
			      *(pCur++) = (bg16 & 255);
			      break;
			    }
			}
		    } 
		  else /* Outside the cursor mask */
		    {
		      switch (bpp)
			{
			case 32:
			  (*pCur++) = 0; /* Fall-thru */
			case 24:
			  (*pCur++) = 0; /* Fall-thru */
			case 16:
			  (*pCur++) = 0;
			  (*pCur++) = 0;
			  break;
			}
		    }
		}
	    }
	}
      /* Make a bitmap from the color bits */
      bm.bmType = 0;
      bm.bmWidth = pScreenPriv->cursor.sm_cx;
      bm.bmHeight = pScreenPriv->cursor.sm_cy;
      bm.bmWidthBytes = pScreenPriv->cursor.sm_cx*(bpp/8);
      bm.bmPlanes = 1;
      bm.bmBitsPixel = bpp;
      bm.bmBits = (void *)pColor;
      if (hAnd)
	hXor = CreateBitmapIndirect (&bm);
      free (pColor);
    } /* End if-bicolor-icon */

  /* If one of the previous two methods gave us the bitmaps we need, make a cursor */
  if (hXor && hAnd)
    {
      ii.fIcon = FALSE;
      ii.xHotspot = pCursor->bits->xhot;
      ii.yHotspot = pCursor->bits->yhot;
      ii.hbmMask = hAnd;
      ii.hbmColor = hXor;
      hCursor = (HCURSOR) CreateIconIndirect( &ii );
    }
  if (hAnd)
    DeleteObject (hAnd);
  if (hXor)
    DeleteObject (hXor);

  if (!hCursor)
    {
      /* We couldn't make a color cursor for this screen, use
	 black and white instead */
      hCursor = CreateCursor (g_hInstance,
			      pCursor->bits->xhot, pCursor->bits->yhot,
			      pScreenPriv->cursor.sm_cx, pScreenPriv->cursor.sm_cy,
			      pAnd, pXor);
    }
  free (pAnd);
  free (pXor);

  if (hCursor == NULL)
    {
      LPVOID lpMsgBuf;
      
      /* Display a fancy error message */
      FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		     FORMAT_MESSAGE_FROM_SYSTEM | 
		     FORMAT_MESSAGE_IGNORE_INSERTS,
		     NULL,
		     GetLastError (),
		     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		     (LPTSTR) &lpMsgBuf,
		     0, NULL);
      
      winErrorFVerb (2, "winLoadCursor - CreateCursor failed\n"
	      "\t%s\n", (LPSTR)lpMsgBuf);
      LocalFree (lpMsgBuf);
    }
  return hCursor;
}

/*
===========================================================================

 Pointer sprite functions

===========================================================================
*/

/*
 * winRealizeCursor
 *  Convert the X cursor representation to native format if possible.
 */
static Bool
winRealizeCursor (ScreenPtr pScreen, CursorPtr pCursor)
{
  WIN_DEBUG_MSG("winRealizeCursor: cursor=%p\n", pCursor); 

  if(pCursor == NULL || pCursor->bits == NULL)
    return FALSE;
  
  /* FIXME: cache ARGB8888 representation? */

  return TRUE;
}


/*
 * winUnrealizeCursor
 *  Free the storage space associated with a realized cursor.
 */
static Bool
winUnrealizeCursor(ScreenPtr pScreen, CursorPtr pCursor)
{
  WIN_DEBUG_MSG("winUnrealizeCursor: cursor=%p\n", pCursor); 
  return TRUE;
}


/*
 * winSetCursor
 *  Set the cursor sprite and position.
 */
static void
winSetCursor (ScreenPtr pScreen, CursorPtr pCursor, int x, int y)
{
  POINT ptCurPos, ptTemp;
  HWND  hwnd;
  RECT  rcClient;
  BOOL  bInhibit;
  winScreenPriv(pScreen);
  WIN_DEBUG_MSG("winSetCursor: cursor=%p\n", pCursor); 
  
  /* Inhibit changing the cursor if the mouse is not in a client area */
  bInhibit = FALSE;
  if (GetCursorPos (&ptCurPos))
    {
      hwnd = WindowFromPoint (ptCurPos);
      if (hwnd)
	{
	  if (GetClientRect (hwnd, &rcClient))
	    {
	      ptTemp.x = rcClient.left;
	      ptTemp.y = rcClient.top;
	      if (ClientToScreen (hwnd, &ptTemp))
		{
		  rcClient.left = ptTemp.x;
		  rcClient.top = ptTemp.y;
		  ptTemp.x = rcClient.right;
		  ptTemp.y = rcClient.bottom;
		  if (ClientToScreen (hwnd, &ptTemp))
		    {
		      rcClient.right = ptTemp.x;
		      rcClient.bottom = ptTemp.y;
		      if (!PtInRect (&rcClient, ptCurPos))
			bInhibit = TRUE;
		    }
		}
	    }
	}
    }

  if (pCursor == NULL)
    {
      if (pScreenPriv->cursor.visible)
	{
	  if (!bInhibit)
	    ShowCursor (FALSE);
	  pScreenPriv->cursor.visible = FALSE;
	}
    }
  else
    {
      if (pScreenPriv->cursor.handle)
	{
	  if (!bInhibit)
	    SetCursor (NULL);
	  DestroyCursor (pScreenPriv->cursor.handle);
	  pScreenPriv->cursor.handle = NULL;
	}
      pScreenPriv->cursor.handle =
	winLoadCursor (pScreen, pCursor, pScreen->myNum);
      WIN_DEBUG_MSG("winSetCursor: handle=%p\n", pScreenPriv->cursor.handle); 

      if (!bInhibit)
	SetCursor (pScreenPriv->cursor.handle);

      if (!pScreenPriv->cursor.visible)
	{
	  if (!bInhibit)
	    ShowCursor (TRUE);
	  pScreenPriv->cursor.visible = TRUE;
	}
    }
}


/*
 * winReallySetCursor
 *  Not needed for xpr. Cursor is set from the X server thread.
 */
void
winReallySetCursor ()
{
}


/*
 * QuartzMoveCursor
 *  Move the cursor. This is a noop for us.
 */
static void
winMoveCursor (ScreenPtr pScreen, int x, int y)
{
}


static miPointerSpriteFuncRec winSpriteFuncsRec = {
  winRealizeCursor,
  winUnrealizeCursor,
  winSetCursor,
  winMoveCursor
};


/*
===========================================================================

 Other screen functions

===========================================================================
*/

/*
 * winCursorQueryBestSize
 *  Handle queries for best cursor size
 */
static void
winCursorQueryBestSize (int class, unsigned short *width,
				     unsigned short *height, ScreenPtr pScreen)
{
  winScreenPriv(pScreen);
  
  if (class == CursorShape)
    {
      *width = pScreenPriv->cursor.sm_cx;
      *height = pScreenPriv->cursor.sm_cy;
    }
  else
    {
      if (pScreenPriv->cursor.QueryBestSize)
        (*pScreenPriv->cursor.QueryBestSize)(class, width, height, pScreen);
    }
}

/*
 * winInitCursor
 *  Initialize cursor support
 */
Bool
winInitCursor (ScreenPtr pScreen)
{
  winScreenPriv(pScreen);
  miPointerScreenPtr pPointPriv;
  /* override some screen procedures */
  pScreenPriv->cursor.QueryBestSize = pScreen->QueryBestSize;
  pScreen->QueryBestSize = winCursorQueryBestSize;
  
  pPointPriv = (miPointerScreenPtr) pScreen->devPrivates[miPointerScreenIndex].ptr;
  
  pScreenPriv->cursor.spriteFuncs = pPointPriv->spriteFuncs;
  pPointPriv->spriteFuncs = &winSpriteFuncsRec;

  pScreenPriv->cursor.handle = NULL;
  pScreenPriv->cursor.visible = FALSE;
  
  pScreenPriv->cursor.sm_cx = GetSystemMetrics (SM_CXCURSOR);
  pScreenPriv->cursor.sm_cy = GetSystemMetrics (SM_CYCURSOR);

  return TRUE;
}
