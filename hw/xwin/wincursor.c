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

  hCursor = CreateCursor (g_hInstance,
			  pCursor->bits->xhot, pCursor->bits->yhot,
			  pScreenPriv->cursor.sm_cx, pScreenPriv->cursor.sm_cy,
			  pAnd, pXor);

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
