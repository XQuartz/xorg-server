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
 * Authors:	Kensuke Matsuzaki
 *		Earle F. Philhower, III
 *		Harold L Hunt II
 */
/*
 * Look at hw/darwin/quartz/xpr/xprFrame.c and hw/darwin/quartz/cr/crFrame.c
 */
#include "win.h"
#include <winuser.h>
#define _WINDOWSWM_SERVER_
#include "windowswmstr.h"
#include "dixevents.h"
#include "winmultiwindowclass.h"
#include "winprefs.h"


/*
 * Constant defines
 */

#define MOUSE_POLLING_INTERVAL		500

#define ULW_COLORKEY	0x00000001
#define ULW_ALPHA	0x00000002
#define ULW_OPAQUE	0x00000004
#define AC_SRC_ALPHA	0x01

/*
 * Global variables
 */

Bool			g_fNoConfigureWindow = FALSE;


/*
 * External global variables
 */

extern HICON		g_hiconX;


/*
 * Internal function to get the DIB format that is compatible with the screen
 * Fixme: Share code with winshadgdi.c
 */

static
Bool
winWin32RootlessQueryDIBFormat (win32RootlessWindowPtr pRLWinPriv, BITMAPINFOHEADER *pbmih)
{
  HBITMAP		hbmp;
#if CYGMULTIWINDOW_DEBUG
  LPDWORD		pdw = NULL;
#endif
  
  /* Create a memory bitmap compatible with the screen */
  hbmp = CreateCompatibleBitmap (pRLWinPriv->hdcScreen, 1, 1);
  if (hbmp == NULL)
    {
      ErrorF ("winWin32RootlessQueryDIBFormat - CreateCompatibleBitmap failed\n");
      return FALSE;
    }
  
  /* Initialize our bitmap info header */
  ZeroMemory (pbmih, sizeof (BITMAPINFOHEADER) + 256 * sizeof (RGBQUAD));
  pbmih->biSize = sizeof (BITMAPINFOHEADER);

  /* Get the biBitCount */
  if (!GetDIBits (pRLWinPriv->hdcScreen,
		  hbmp,
		  0, 1,
		  NULL,
		  (BITMAPINFO*) pbmih,
		  DIB_RGB_COLORS))
    {
      ErrorF ("winWin32RootlessQueryDIBFormat - First call to GetDIBits failed\n");
      DeleteObject (hbmp);
      return FALSE;
    }

#if CYGMULTIWINDOW_DEBUG
  /* Get a pointer to bitfields */
  pdw = (DWORD*) ((CARD8*)pbmih + sizeof (BITMAPINFOHEADER));

  ErrorF ("winWin32RootlessQueryDIBFormat - First call masks: %08x %08x %08x\n",
	  (unsigned int)pdw[0], (unsigned int)pdw[1], (unsigned int)pdw[2]);
#endif

  /* Get optimal color table, or the optimal bitfields */
  if (!GetDIBits (pRLWinPriv->hdcScreen,
		  hbmp,
		  0, 1,
		  NULL,
		  (BITMAPINFO*)pbmih,
		  DIB_RGB_COLORS))
    {
      ErrorF ("winWin32RootlessQueryDIBFormat - Second call to GetDIBits "
	      "failed\n");
      DeleteObject (hbmp);
      return FALSE;
    }

  /* Free memory */
  DeleteObject (hbmp);
  
  return TRUE;
}

static HRGN
winWin32RootlessCreateRgnFromRegion (RegionPtr pShape)
{
  int		nRects;
  BoxPtr	pRects, pEnd;
  HRGN		hRgn, hRgnRect;

  if (pShape == NULL) return NULL;

  nRects = REGION_NUM_RECTS(pShape);
  pRects = REGION_RECTS(pShape);
  
  hRgn = CreateRectRgn (0, 0, 0, 0);
  if (hRgn == NULL)
    {
      ErrorF ("winReshape - Initial CreateRectRgn (%d, %d, %d, %d) "
	      "failed: %d\n",
	      0, 0, 0, 0, (int) GetLastError ());
    }

  /* Loop through all rectangles in the X region */
  for (pEnd = pRects + nRects; pRects < pEnd; pRects++)
    {
      /* Create a Windows region for the X rectangle */
      hRgnRect = CreateRectRgn (pRects->x1,
				pRects->y1,
				pRects->x2,
				pRects->y2);
      if (hRgnRect == NULL)
	{
	  ErrorF ("winReshape - Loop CreateRectRgn (%d, %d, %d, %d) "
		  "failed: %d\n",
		  pRects->x1,
		  pRects->y1,
		  pRects->x2,
		  pRects->y2,
		  (int) GetLastError ());
	}
      
      /* Merge the Windows region with the accumulated region */
      if (CombineRgn (hRgn, hRgn, hRgnRect, RGN_OR) == ERROR)
	{
	  ErrorF ("winReshape - CombineRgn () failed: %d\n",
		  (int) GetLastError ());
	}
      
      /* Delete the temporary Windows region */
      DeleteObject (hRgnRect);
    }
  
  return hRgn;
}

static void
InitWin32RootlessEngine (win32RootlessWindowPtr pRLWinPriv)
{
  pRLWinPriv->hdcScreen = GetDC (pRLWinPriv->hWnd);
  pRLWinPriv->hdcShadow = CreateCompatibleDC (pRLWinPriv->hdcScreen);
  pRLWinPriv->hbmpShadow = NULL;

  /* Allocate bitmap info header */
  pRLWinPriv->pbmihShadow = (BITMAPINFOHEADER*) malloc (sizeof (BITMAPINFOHEADER)
							+ 256 * sizeof (RGBQUAD));
  if (pRLWinPriv->pbmihShadow == NULL)
    {
      ErrorF ("winWin32RootlessCreateFrame - malloc () failed\n");
      return;
    }
  
  /* Query the screen format */
  winWin32RootlessQueryDIBFormat (pRLWinPriv,
				  pRLWinPriv->pbmihShadow);
}

Bool
winWin32RootlessCreateFrame (RootlessWindowPtr pFrame, ScreenPtr pScreen,
			     int newX, int newY, RegionPtr pShape)
{
#define CLASS_NAME_LENGTH 512
  Bool				fResult = TRUE;
  win32RootlessWindowPtr	pRLWinPriv;
  WNDCLASS			wc;
  char                  	pszClass[CLASS_NAME_LENGTH], pszWindowID[12];
  HICON                	 	hIcon;
  char				*res_name, *res_class, *res_role;
  static int			s_iWindowID = 0;
 
#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessCreateFrame %d %d - %d %d\n",
	  newX, newY, pFrame->width, pFrame->height);
#endif

  pRLWinPriv = (win32RootlessWindowPtr) malloc (sizeof (win32RootlessWindowRec));
  pRLWinPriv->pFrame = pFrame;
  pRLWinPriv->pfb = NULL;
  pRLWinPriv->hbmpShadow = NULL;
  pRLWinPriv->hdcShadow = NULL;
  pRLWinPriv->hdcScreen = NULL;
  pRLWinPriv->fResized = TRUE;
  pRLWinPriv->fClose = FALSE;
  pRLWinPriv->fRestackingNow = FALSE;
  pRLWinPriv->fDestroyed = FALSE;
  pRLWinPriv->fMovingOrSizing = FALSE;
  
  // Store the implementation private frame ID
  pFrame->wid = (RootlessFrameID) pRLWinPriv;


  /* Load default X icon in case it's not ready yet */
  if (!g_hiconX)
    g_hiconX = (HICON)winOverrideDefaultIcon();
  
  if (!g_hiconX)
    g_hiconX = LoadIcon (g_hInstance, MAKEINTRESOURCE(IDI_XWIN));
  
  /* Try and get the icon from WM_HINTS */
  hIcon = winXIconToHICON (pFrame->win);
  
  /* Use default X icon if no icon loaded from WM_HINTS */
  if (!hIcon)
    hIcon = g_hiconX;

  /* Set standard class name prefix so we can identify window easily */
  strncpy (pszClass, WINDOW_CLASS_X, strlen (WINDOW_CLASS_X));

  if (winMultiWindowGetClassHint (pFrame->win, &res_name, &res_class))
    {
      strncat (pszClass, "-", 1);
      strncat (pszClass, res_name, CLASS_NAME_LENGTH - strlen (pszClass));
      strncat (pszClass, "-", 1);
      strncat (pszClass, res_class, CLASS_NAME_LENGTH - strlen (pszClass));
      
      /* Check if a window class is provided by the WM_WINDOW_ROLE property,
       * if not use the WM_CLASS information.
       * For further information see:
       * http://tronche.com/gui/x/icccm/sec-5.html
       */ 
      if (winMultiWindowGetWindowRole (pFrame->win, &res_role) )
	{
	  strcat (pszClass, "-");
	  strcat (pszClass, res_role);
	  free (res_role);
	}

      free (res_name);
      free (res_class);
    }

  /* Add incrementing window ID to make unique class name */
  sprintf (pszWindowID, "-%x", s_iWindowID++);
  strcat (pszClass, pszWindowID);

#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winCreateWindowsWindow - Creating class: %s\n", pszClass);
#endif

  /* Setup our window class */
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = winWin32RootlessWindowProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = g_hInstance;
  wc.hIcon = hIcon;
  wc.hCursor = 0;
  wc.hbrBackground = (HBRUSH) GetStockObject (WHITE_BRUSH);
  wc.lpszMenuName = NULL;
  wc.lpszClassName = pszClass;
  RegisterClass (&wc);

  /* Create the window */
  g_fNoConfigureWindow = TRUE;
  pRLWinPriv->hWnd = CreateWindowExA (WS_EX_TOOLWINDOW,		/* Extended styles */
				      pszClass,			/* Class name */
				      WINDOW_TITLE_X,		/* Window name */
				      WS_POPUP | WS_CLIPCHILDREN,
				      newX,			/* Horizontal position */
				      newY,			/* Vertical position */
				      pFrame->width,		/* Right edge */ 
				      pFrame->height,		/* Bottom edge */
				      (HWND) NULL,		/* No parent or owner window */
				      (HMENU) NULL,		/* No menu */
				      GetModuleHandle (NULL),	/* Instance handle */
				      pRLWinPriv);		/* ScreenPrivates */
  if (pRLWinPriv->hWnd == NULL)
    {
      ErrorF ("winWin32RootlessCreateFrame - CreateWindowExA () failed: %d\n",
	      (int) GetLastError ());
      fResult = FALSE;
    }

#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessCreateFrame - ShowWindow\n");
#endif

  //ShowWindow (pRLWinPriv->hWnd, SW_SHOWNOACTIVATE);
  g_fNoConfigureWindow = FALSE;
  
  if (pShape != NULL)
    {
      winWin32RootlessReshapeFrame (pFrame->wid, pShape);
    }

#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessCreateFrame - (%08x) %08x\n",
	  (int) pFrame->wid, (int) pRLWinPriv->hWnd);
#if 0
  {
   WindowPtr		pWin2 = NULL;
   win32RootlessWindowPtr pRLWinPriv2 = NULL;

   /* Check if the Windows window property for our X window pointer is valid */
   if ((pWin2 = (WindowPtr)GetProp (pRLWinPriv->hWnd, WIN_WINDOW_PROP)) != NULL)
     {
       pRLWinPriv2 = (win32RootlessWindowPtr) RootlessFrameForWindow (pWin2, FALSE);
     }
   ErrorF ("winWin32RootlessCreateFrame2 (%08x) %08x\n",
	   pRLWinPriv2, pRLWinPriv2->hWnd);
   if (pRLWinPriv != pRLWinPriv2 || pRLWinPriv->hWnd != pRLWinPriv2->hWnd)
     {
       ErrorF ("Error param missmatch\n");
     }
 }
#endif
#endif
  return fResult;
}

void
winWin32RootlessDestroyFrame (RootlessFrameID wid)
{
  win32RootlessWindowPtr pRLWinPriv = (win32RootlessWindowPtr) wid;
#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessDestroyFrame (%08x) %08x\n",
	  (int) pRLWinPriv, (int) pRLWinPriv->hWnd);
#if 0
 {
   WindowPtr		pWin2 = NULL;
   win32RootlessWindowPtr pRLWinPriv2 = NULL;

   /* Check if the Windows window property for our X window pointer is valid */
   if ((pWin2 = (WindowPtr)GetProp (pRLWinPriv->hWnd, WIN_WINDOW_PROP)) != NULL)
     {
       pRLWinPriv2 = (win32RootlessWindowPtr) RootlessFrameForWindow (pWin2, FALSE);
     }
   ErrorF ("winWin32RootlessDestroyFrame2 (%08x) %08x\n",
	   pRLWinPriv2, pRLWinPriv2->hWnd);
   if (pRLWinPriv != pRLWinPriv2 || pRLWinPriv->hWnd != pRLWinPriv2->hWnd)
     {
       ErrorF ("Error param missmatch\n");
       *(int*)0 = 1;//raise exseption
     }
 }
#endif
#endif

  pRLWinPriv->fClose = TRUE;
  pRLWinPriv->fDestroyed = TRUE;
  /* Destroy the Windows window */
  DestroyWindow (pRLWinPriv->hWnd);

#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessDestroyFrame - done\n");
#endif
}

void
winWin32RootlessMoveFrame (RootlessFrameID wid, ScreenPtr pScreen, int iNewX, int iNewY)
{
  win32RootlessWindowPtr pRLWinPriv = (win32RootlessWindowPtr) wid;
  RECT rcNew;
  DWORD dwExStyle;
  DWORD dwStyle;
  int iX, iY, iWidth, iHeight;

#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessMoveFrame (%08x) (%d %d)\n", (int) pRLWinPriv, iNewX, iNewY);
#endif

  /* Get the Windows window style and extended style */
  dwExStyle = GetWindowLongPtr (pRLWinPriv->hWnd, GWL_EXSTYLE);
  dwStyle = GetWindowLongPtr (pRLWinPriv->hWnd, GWL_STYLE);

  /* Get the X and Y location of the X window */
  iX = iNewX + GetSystemMetrics (SM_XVIRTUALSCREEN);
  iY = iNewY + GetSystemMetrics (SM_YVIRTUALSCREEN);

  /* Get the height and width of the X window */
  iWidth = pRLWinPriv->pFrame->width;
  iHeight = pRLWinPriv->pFrame->height;

  /* Store the origin, height, and width in a rectangle structure */
  SetRect (&rcNew, iX, iY, iX + iWidth, iY + iHeight);

  /*
   * Calculate the required size of the Windows window rectangle,
   * given the size of the Windows window client area.
   */
  AdjustWindowRectEx (&rcNew, dwStyle, FALSE, dwExStyle);

  g_fNoConfigureWindow = TRUE;
  SetWindowPos (pRLWinPriv->hWnd, NULL, rcNew.left, rcNew.top, 0, 0,
		SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
  g_fNoConfigureWindow = FALSE;
}

void
winWin32RootlessResizeFrame (RootlessFrameID wid, ScreenPtr pScreen,
			     int iNewX, int iNewY,
			     unsigned int uiNewWidth, unsigned int uiNewHeight,
			     unsigned int uiGravity)
{
  win32RootlessWindowPtr pRLWinPriv = (win32RootlessWindowPtr) wid;
  RECT rcNew;
  RECT rcOld;
  DWORD dwExStyle;
  DWORD dwStyle;
  int iX, iY;
  
#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessResizeFrame (%08x) (%d %d)-(%d %d)\n",
	  (int) pRLWinPriv, iNewX, iNewY, uiNewWidth, uiNewHeight);
#endif

  pRLWinPriv->fResized = TRUE;

  /* Get the Windows window style and extended style */
  dwExStyle = GetWindowLongPtr (pRLWinPriv->hWnd, GWL_EXSTYLE);
  dwStyle = GetWindowLongPtr (pRLWinPriv->hWnd, GWL_STYLE);

  /* Get the X and Y location of the X window */
  iX = iNewX + GetSystemMetrics (SM_XVIRTUALSCREEN);
  iY = iNewY + GetSystemMetrics (SM_YVIRTUALSCREEN);

  /* Store the origin, height, and width in a rectangle structure */
  SetRect (&rcNew, iX, iY, iX + uiNewWidth, iY + uiNewHeight);

  /*
   * Calculate the required size of the Windows window rectangle,
   * given the size of the Windows window client area.
   */
  AdjustWindowRectEx (&rcNew, dwStyle, FALSE, dwExStyle);

  /* Get a rectangle describing the old Windows window */
  GetWindowRect (pRLWinPriv->hWnd, &rcOld);

  /* Check if the old rectangle and new rectangle are the same */
  if (!EqualRect (&rcNew, &rcOld))
    {

      g_fNoConfigureWindow = TRUE;
      MoveWindow (pRLWinPriv->hWnd,
		  rcNew.left, rcNew.top,
		  rcNew.right - rcNew.left, rcNew.bottom - rcNew.top,
		  TRUE);
      g_fNoConfigureWindow = FALSE;
    }
}

void
winWin32RootlessRestackFrame (RootlessFrameID wid, RootlessFrameID nextWid)
{
  win32RootlessWindowPtr pRLWinPriv = (win32RootlessWindowPtr) wid;
  win32RootlessWindowPtr pRLNextWinPriv = (win32RootlessWindowPtr) nextWid;
  winScreenPriv(pRLWinPriv->pFrame->win->drawable.pScreen);
  HWND hWnd;
#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessRestackFrame (%08x)\n", (int) pRLWinPriv);
#endif

  if (pScreenPriv->fRestacking) return;

  pRLWinPriv->fRestackingNow = TRUE;

  /* Show window */    
  if(!IsWindowVisible (pRLWinPriv->hWnd))
    ShowWindow (pRLWinPriv->hWnd, SW_SHOWNA);

  if (pRLNextWinPriv == NULL)
    {
      //ErrorF ("Win %08x is top\n", pRLWinPriv);
      pScreenPriv->widTop = wid;
      SetWindowPos (pRLWinPriv->hWnd, HWND_TOP,
		    0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    }
  else
    {
      //ErrorF ("Win %08x is not top\n", pRLWinPriv);
      hWnd = GetNextWindow (pRLWinPriv->hWnd, GW_HWNDPREV);
      do
	{
	  if (hWnd == pRLNextWinPriv->hWnd)
	    {
	      SetWindowPos (pRLWinPriv->hWnd, pRLNextWinPriv->hWnd,
			    0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
	      break;
	    }
	  hWnd = GetNextWindow (hWnd, GW_HWNDPREV);
	}
      while (hWnd);
    }
  pRLWinPriv->fRestackingNow = FALSE;
}

void
winWin32RootlessReshapeFrame (RootlessFrameID wid, RegionPtr pShape)
{
  win32RootlessWindowPtr pRLWinPriv = (win32RootlessWindowPtr) wid;
  HRGN hRgn;
#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessReshapeFrame (%08x)\n", (int) pRLWinPriv);
#endif
  hRgn = winWin32RootlessCreateRgnFromRegion (pShape);
  SetWindowRgn (pRLWinPriv->hWnd, hRgn, TRUE);
}

void
winWin32RootlessUnmapFrame (RootlessFrameID wid)
{
  win32RootlessWindowPtr pRLWinPriv = (win32RootlessWindowPtr) wid;
#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessUnmapFrame (%08x)\n", (int) pRLWinPriv);
#endif

  g_fNoConfigureWindow = TRUE;
  //ShowWindow (pRLWinPriv->hWnd, SW_MINIMIZE);
  ShowWindow (pRLWinPriv->hWnd, SW_HIDE);
  g_fNoConfigureWindow = FALSE;
}

/*
 * Fixme: Code sharing with winshadgdi.c and other engine support
 */
void
winWin32RootlessStartDrawing (RootlessFrameID wid, char **pixelData, int *bytesPerRow)
{
  win32RootlessWindowPtr pRLWinPriv = (win32RootlessWindowPtr) wid;
  winPrivScreenPtr	pScreenPriv = NULL;
  winScreenInfo		*pScreenInfo = NULL;
  ScreenPtr		pScreen = NULL;
  DIBSECTION		dibsection;
  Bool			fReturn = TRUE;
  HDC			hdcNew;
  HBITMAP		hbmpNew;
#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessStartDrawing (%08x) %08x\n", (int) pRLWinPriv, pRLWinPriv->fDestroyed);
#endif

  if (!pRLWinPriv->fDestroyed)
    {
      pScreen = pRLWinPriv->pFrame->win->drawable.pScreen;
      if (pScreen) pScreenPriv = winGetScreenPriv(pScreen);
      if (pScreenPriv) pScreenInfo = pScreenPriv->pScreenInfo;
      
#if CYGMULTIWINDOW_DEBUG
      ErrorF ("pScreenPriv %08X\n", (int) pScreenPriv);
      ErrorF ("pScreenInfo %08X\n", (int) pScreenInfo);
#endif
      if (pRLWinPriv->hdcScreen == NULL)
	{
	  InitWin32RootlessEngine (pRLWinPriv);
	}
      
      if (pRLWinPriv->fResized)
	{
	  hdcNew = CreateCompatibleDC (pRLWinPriv->hdcScreen);
	  /* Describe shadow bitmap to be created */
	  pRLWinPriv->pbmihShadow->biWidth = pRLWinPriv->pFrame->width;//pRLWinPriv->dwWidth;
	  pRLWinPriv->pbmihShadow->biHeight = -pRLWinPriv->pFrame->height;//pRLWinPriv->dwHeight;
	  
	  /* Create a DI shadow bitmap with a bit pointer */
	  hbmpNew = CreateDIBSection (pRLWinPriv->hdcScreen,
				      (BITMAPINFO *) pRLWinPriv->pbmihShadow,
				      DIB_RGB_COLORS,
				      (VOID**) &pRLWinPriv->pfb,
				      NULL,
				      0);
	  if (hbmpNew == NULL || pRLWinPriv->pfb == NULL)
	    {
	      ErrorF ("winWin32RootlessStartDrawing - CreateDIBSection failed\n");
	      //return FALSE;
	    }
	  else
	    {
#if CYGMULTIWINDOW_DEBUG
	      ErrorF ("winWin32RootlessStartDrawing - Shadow buffer allocated\n");
#endif
	    }
	  
	  /* Get information about the bitmap that was allocated */
	  GetObject (hbmpNew, sizeof (dibsection), &dibsection);
	  
#if CYGMULTIWINDOW_DEBUG
	  /* Print information about bitmap allocated */
	  ErrorF ("winWin32RootlessStartDrawing - Dibsection width: %d height: %d "
		  "depth: %d size image: %d\n",
		  (unsigned int)dibsection.dsBmih.biWidth,
		  (unsigned int)dibsection.dsBmih.biHeight,
		  (unsigned int)dibsection.dsBmih.biBitCount,
		  (unsigned int)dibsection.dsBmih.biSizeImage);
#endif
	  
	  /* Select the shadow bitmap into the shadow DC */
	  SelectObject (hdcNew, hbmpNew);
	  
#if CYGMULTIWINDOW_DEBUG
	  ErrorF ("winWin32RootlessStartDrawing - Attempting a shadow blit\n");
#endif
	  
	  /* Blit from the old shadow to the new shadow */
	  fReturn = BitBlt (hdcNew,
			    0, 0,
			    pRLWinPriv->pFrame->width, pRLWinPriv->pFrame->height,
			    pRLWinPriv->hdcShadow,
			    0, 0,
			    SRCCOPY);
	  if (fReturn)
	    {
#if CYGMULTIWINDOW_DEBUG
	      ErrorF ("winWin32RootlessStartDrawing - Shadow blit success\n");
#endif
	    }
	  else
	    {
	      ErrorF ("winWin32RootlessStartDrawing - Shadow blit failure\n");
	    }
	  
	  /* Look for height weirdness */
	  if (dibsection.dsBmih.biHeight < 0)
	    {
	      /* FIXME: Figure out why biHeight is sometimes negative */
	      ErrorF ("winWin32RootlessStartDrawing - WEIRDNESS - biHeight "
		      "still negative: %d\n"
		      "winAllocateFBShadowGDI - WEIRDNESS - Flipping biHeight sign\n",
		      (int) dibsection.dsBmih.biHeight);
	      dibsection.dsBmih.biHeight = -dibsection.dsBmih.biHeight;
	    }
	  
	  pRLWinPriv->dwWidthBytes = dibsection.dsBm.bmWidthBytes;
	  
#if CYGMULTIWINDOW_DEBUG
	  ErrorF ("winWin32RootlessStartDrawing - bytesPerRow: %d\n",
		  (unsigned int)dibsection.dsBm.bmWidthBytes);
#endif
	  
	  /* Free the old shadow bitmap */
	  DeleteObject (pRLWinPriv->hdcShadow);
	  DeleteObject (pRLWinPriv->hbmpShadow);
	  
	  pRLWinPriv->hdcShadow = hdcNew;
	  pRLWinPriv->hbmpShadow = hbmpNew;
	  
	  pRLWinPriv->fResized = FALSE;
	}
#if CYGMULTIWINDOW_DEBUG
      ErrorF ("winWin32RootlessStartDrawing - 0x%08x %d\n",
	      (unsigned int)pRLWinPriv->pfb, (unsigned int)dibsection.dsBm.bmWidthBytes);
#endif
    }
  else
    {
      ErrorF ("winWin32RootlessStartDrawing - Already window was destoroyed \n"); 
    }
#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessStartDrawing - done (0x08x) 0x%08x %d\n",
	  (int) pRLWinPriv,
	  (unsigned int)pRLWinPriv->pfb, (unsigned int)pRLWinPriv->dwWidthBytes);
#endif
  *pixelData = pRLWinPriv->pfb;
  *bytesPerRow = pRLWinPriv->dwWidthBytes;
}

void
winWin32RootlessStopDrawing (RootlessFrameID wid, Bool fFlush)
{
#if 0
  win32RootlessWindowPtr pRLWinPriv = (win32RootlessWindowPtr) wid;
  BLENDFUNCTION bfBlend;
  SIZE szWin;
  POINT ptSrc;
#if CYGMULTIWINDOW_DEBUG || TRUE
  ErrorF ("winWin32RootlessStopDrawing (%08x)\n", pRLWinPriv);
#endif
  szWin.cx = pRLWinPriv->dwWidth;
  szWin.cy = pRLWinPriv->dwHeight;
  ptSrc.x = 0;
  ptSrc.y = 0;
  bfBlend.BlendOp = AC_SRC_OVER;
  bfBlend.BlendFlags = 0;
  bfBlend.SourceConstantAlpha = 255;
  bfBlend.AlphaFormat = AC_SRC_ALPHA;

  if (!UpdateLayeredWindow (pRLWinPriv->hWnd,
			    NULL, NULL, &szWin,
			    pRLWinPriv->hdcShadow, &ptSrc,
			    0, &bfBlend, ULW_ALPHA))
    {
      ErrorF ("winWin32RootlessStopDrawing - UpdateLayeredWindow failed\n");
    }
#endif
}

void
winWin32RootlessUpdateRegion (RootlessFrameID wid, RegionPtr pDamage)
{
  win32RootlessWindowPtr pRLWinPriv = (win32RootlessWindowPtr) wid;
#if 0
  BLENDFUNCTION bfBlend;
  SIZE szWin;
  POINT ptSrc;
#endif
#if CYGMULTIWINDOW_DEBUG && 0
  ErrorF ("winWin32RootlessUpdateRegion (%08x)\n", pRLWinPriv);
#endif
#if 0
  szWin.cx = pRLWinPriv->dwWidth;
  szWin.cy = pRLWinPriv->dwHeight;
  ptSrc.x = 0;
  ptSrc.y = 0;
  bfBlend.BlendOp = AC_SRC_OVER;
  bfBlend.BlendFlags = 0;
  bfBlend.SourceConstantAlpha = 255;
  bfBlend.AlphaFormat = AC_SRC_ALPHA;

  if (!UpdateLayeredWindow (pRLWinPriv->hWnd,
			    NULL, NULL, &szWin,
			    pRLWinPriv->hdcShadow, &ptSrc,
			    0, &bfBlend, ULW_ALPHA))
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
      
      ErrorF ("winWin32RootlessUpdateRegion - UpdateLayeredWindow failed: %s\n",
	      (LPSTR)lpMsgBuf);
      LocalFree (lpMsgBuf);
    }
#endif
  if (!g_fNoConfigureWindow) UpdateWindow (pRLWinPriv->hWnd);
}

void
winWin32RootlessDamageRects (RootlessFrameID wid, int nCount, const BoxRec *pRects,
			     int shift_x, int shift_y)
{
  win32RootlessWindowPtr pRLWinPriv = (win32RootlessWindowPtr) wid;
  const BoxRec *pEnd;
#if CYGMULTIWINDOW_DEBUG && 0
  ErrorF ("winWin32RootlessDamageRects (%08x, %d, %08x, %d, %d)\n",
	  pRLWinPriv, nCount, pRects, shift_x, shift_y);
#endif

  for (pEnd = pRects + nCount; pRects < pEnd; pRects++) {
        RECT rcDmg;
        rcDmg.left = pRects->x1 + shift_x;
        rcDmg.top = pRects->y1 + shift_y;
        rcDmg.right = pRects->x2 + shift_x;
        rcDmg.bottom = pRects->y2 + shift_y;

	InvalidateRect (pRLWinPriv->hWnd, &rcDmg, FALSE);
    }
}

void
winWin32RootlessRootlessSwitchWindow (RootlessWindowPtr pFrame, WindowPtr oldWin)
{
  win32RootlessWindowPtr pRLWinPriv = (win32RootlessWindowPtr) pFrame->wid;
#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessRootlessSwitchWindow (%08x) %08x\n",
	  (int) pRLWinPriv, (int) pRLWinPriv->hWnd);
#endif
  pRLWinPriv->pFrame = pFrame;
  pRLWinPriv->fResized = TRUE;

#if CYGMULTIWINDOW_DEBUG
#if 0
 {
   WindowPtr		pWin2 = NULL;
   win32RootlessWindowPtr pRLWinPriv2 = NULL;

   /* Check if the Windows window property for our X window pointer is valid */
   if ((pWin2 = (WindowPtr)GetProp (pRLWinPriv->hWnd, WIN_WINDOW_PROP)) != NULL)
     {
       pRLWinPriv2 = (win32RootlessWindowPtr) RootlessFrameForWindow (pWin2, FALSE);
     }
   ErrorF ("winWin32RootlessSwitchFrame2 (%08x) %08x\n",
	   pRLWinPriv2, pRLWinPriv2->hWnd);
   if (pRLWinPriv != pRLWinPriv2 || pRLWinPriv->hWnd != pRLWinPriv2->hWnd)
     {
       ErrorF ("Error param missmatch\n");
     }
 }
#endif
#endif
}

void
winWin32RootlessCopyBytes (unsigned int width, unsigned int height,
			   const void *src, unsigned int srcRowBytes,
			   void *dst, unsigned int dstRowBytes)
{
#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessCopyBytes - Not implemented\n");
#endif
}

void
winWin32RootlessFillBytes (unsigned int width, unsigned int height, unsigned int value,
			   void *dst, unsigned int dstRowBytes)
{
#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessFillBytes - Not implemented\n");
#endif
}

int
winWin32RootlessCompositePixels (unsigned int width, unsigned int height, unsigned int function,
				 void *src[2], unsigned int srcRowBytes[2],
				 void *mask, unsigned int maskRowBytes,
				 void *dst[2], unsigned int dstRowBytes[2])
{
#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessCompositePixels - Not implemented\n");
#endif
  return 0;
}


void
winWin32RootlessCopyWindow (RootlessFrameID wid, int nDstRects, const BoxRec *pDstRects,
			    int nDx, int nDy)
{
  win32RootlessWindowPtr pRLWinPriv = (win32RootlessWindowPtr) wid;
  const BoxRec *pEnd;
  RECT rcDmg;
#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessCopyWindow (%08x, %d, %08x, %d, %d)\n",
	  (int) pRLWinPriv, nDstRects, (int) pDstRects, nDx, nDy);
#endif

  for (pEnd = pDstRects + nDstRects; pDstRects < pEnd; pDstRects++)
    {
#if CYGMULTIWINDOW_DEBUG
      ErrorF ("BitBlt (%d, %d, %d, %d) (%d, %d)\n",
	      pDstRects->x1, pDstRects->y1,
	      pDstRects->x2 - pDstRects->x1,
	      pDstRects->y2 - pDstRects->y1,
	      pDstRects->x1 + nDx,
	      pDstRects->y1 + nDy);
#endif

      if (!BitBlt (pRLWinPriv->hdcShadow,
		   pDstRects->x1, pDstRects->y1,
		   pDstRects->x2 - pDstRects->x1,
		   pDstRects->y2 - pDstRects->y1,
		   pRLWinPriv->hdcShadow,
		   pDstRects->x1 + nDx,  pDstRects->y1 + nDy,
		   SRCCOPY))
	{
	  ErrorF ("winWin32RootlessCopyWindow - BitBlt failed.\n");
	}
      
      rcDmg.left = pDstRects->x1;
      rcDmg.top = pDstRects->y1;
      rcDmg.right = pDstRects->x2;
      rcDmg.bottom = pDstRects->y2;
      
      InvalidateRect (pRLWinPriv->hWnd, &rcDmg, FALSE);
    }
#if CYGMULTIWINDOW_DEBUG
  ErrorF ("winWin32RootlessCopyWindow - done\n");
#endif
}
