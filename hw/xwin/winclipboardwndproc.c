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
 * Authors:	Harold L Hunt II
 */
/* $XFree86: xc/programs/Xserver/hw/xwin/winclipboardwndproc.c,v 1.2 2003/07/29 21:25:16 dawes Exp $ */

#include "winclipboard.h"


/*
 * Constants
 */

#define WIN_CLIPBOARD_PROP	"cyg_clipboard_prop"


/*
 * References to external symbols
 */

extern Bool		g_fUnicodeSupport;


/* 
 * Local function prototypes
 */

static Bool
winLookForSelectionNotify (Display *pDisplay, XEvent *pEvent, XPointer pArg);


/*
 * Signal that we found a SelectionNotify event
 */

static Bool
winLookForSelectionNotify (Display *pDisplay, XEvent *pEvent, XPointer pArg)
{
  if (pEvent->type == SelectionNotify)
    return TRUE;
  
  return FALSE;
}


/*
 * Process a given Windows message
 */

LRESULT CALLBACK
winClipboardWindowProc (HWND hwnd, UINT message, 
			WPARAM wParam, LPARAM lParam)
{
  ClipboardWindowPropPtr	pWindowProp = GetProp (hwnd,
						       WIN_CLIPBOARD_PROP);

  /* Branch on message type */
  switch (message)
    {
    case WM_DESTROY:
      {
	ErrorF ("winClipboardWindowProc - WM_DESTROY\n");

	HWND	*phwndNextViewer = pWindowProp->phwndClipboardNextViewer;
	
	/* Remove ourselves from the clipboard chain */
	ChangeClipboardChain (hwnd, *phwndNextViewer);
	
	*phwndNextViewer = NULL;
	
	/* Free the window property data */
	free (pWindowProp);
	pWindowProp = NULL;
	SetProp (hwnd, WIN_CLIPBOARD_PROP, NULL);

	PostQuitMessage (0);
      }
      return 0;


    case WM_CREATE:
      {
	ErrorF ("winClipboardWindowProc - WM_CREATE\n");
	
	/* Fetch window data from creation data */
	pWindowProp = ((LPCREATESTRUCT) lParam)->lpCreateParams;
	
	/* Save data as a window property */
	SetProp (hwnd, WIN_CLIPBOARD_PROP, pWindowProp);

	/* Add ourselves to the clipboard viewer chain */
	*(pWindowProp->phwndClipboardNextViewer) = SetClipboardViewer (hwnd);
      }
      return 0;


    case WM_CHANGECBCHAIN:
      {
	HWND	*phwndNextViewer = pWindowProp->phwndClipboardNextViewer;

	if ((HWND) wParam == *phwndNextViewer)
	  *phwndNextViewer = (HWND) lParam;
	else if (*phwndNextViewer)
	  SendMessage (*phwndNextViewer, message, wParam, lParam);
      }
      return 0;


    case WM_DRAWCLIPBOARD:
      {
	HWND	*phwndNextViewer = pWindowProp->phwndClipboardNextViewer;
	Bool	*pfCBCInitialized = pWindowProp->pfCBCInitialized;
	Display *pDisplay = *(pWindowProp->ppClipboardDisplay);
	Window	iWindow = *(pWindowProp->piClipboardWindow);
	int	iReturn;

	/* Pass the message on the next window in the clipboard viewer chain */
	if (*phwndNextViewer)
	  SendMessage (*phwndNextViewer, message, 0, 0);
	
	/* Bail on first message */
	if (!*pfCBCInitialized)
	  {
	    ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
		    "Initializing - Returning.\n");
	    *pfCBCInitialized = TRUE;
	    return 0;
	  }
	
	/* Bail when clipboard is unowned */
	if (NULL == GetClipboardOwner ())
	  {
	    ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
		    "Clipboard is unowned.\n");
	    return 0;
	  }
	
	/* Bail when we still own the clipboard */
	if (hwnd == GetClipboardOwner ())
	  {
	    ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
		    "We own the clipboard, returning.\n");
	    return 0;
	  }

	/* Reassert ownership of PRIMARY */	  
	iReturn = XSetSelectionOwner (pDisplay,
				      XA_PRIMARY,
				      iWindow,
				      CurrentTime);
	if (iReturn == BadAtom || iReturn == BadWindow)
	  {
	    ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
		    "Could not reassert ownership of PRIMARY\n");
	  }
	else
	  {
	    ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
		    "Reasserted ownership of PRIMARY\n");
	  }
	
	/* Reassert ownership of the CLIPBOARD */	  
	iReturn = XSetSelectionOwner (pDisplay,
				      XInternAtom (pDisplay,
						   "CLIPBOARD",
						   FALSE),
				      iWindow,
				      CurrentTime);
	if (iReturn == BadAtom || iReturn == BadWindow)
	  {
	    ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
		    "Could not reassert ownership of CLIPBOARD\n");
	  }
	else
	  {
	    ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
		    "Reasserted ownership of CLIPBOARD\n");
	  }
	
	/* Flush the pending SetSelectionOwner event now */
	XFlush (pDisplay);
      }
      return 0;


    case WM_RENDERFORMAT:
    case WM_RENDERALLFORMATS:
      {
	XEvent	event;
	int	iReturn;
	Display *pDisplay = *(pWindowProp->ppClipboardDisplay);
	Window	iWindow = *(pWindowProp->piClipboardWindow);

#if 0
	ErrorF ("winClipboardWindowProc - WM_RENDER*FORMAT - Hello.\n");
#endif

	/* Request the selection contents */
	iReturn = XConvertSelection (pDisplay,
				     *(pWindowProp->patomLastOwnedSelection),
				     XInternAtom (pDisplay,
						  "COMPOUND_TEXT", False),
				     XInternAtom (pDisplay,
						  "CYGX_CUT_BUFFER", False),
				     iWindow,
				     CurrentTime);
	if (iReturn == BadAtom || iReturn == BadWindow)
	  {
	    ErrorF ("winClipboardWindowProc - WM_RENDER*FORMAT - "
		    "XConvertSelection () failed\n");
	    break;
	  }

	/* Wait for the SelectionNotify event */
	XPeekIfEvent (pDisplay, &event, winLookForSelectionNotify, NULL);

	/* Special handling for WM_RENDERALLFORMATS */
	if (message == WM_RENDERALLFORMATS)
	  {
	    /* We must open and empty the clipboard */
	    
	    if (!OpenClipboard (hwnd))
	      {
		ErrorF ("winClipboardWindowProc - WM_RENDER*FORMATS - "
			"OpenClipboard () failed: %08x\n",
			GetLastError ());
		break;
	      }
	    
	    if (!EmptyClipboard ())
	      {
		ErrorF ("winClipboardWindowProc - WM_RENDER*FORMATS - "
			"EmptyClipboard () failed: %08x\n",
		      GetLastError ());
		break;
	      }
	  }
	
	/* Process the SelectionNotify event */
	iReturn = winClipboardFlushXEvents (hwnd,
					    iWindow,
					    pDisplay,
					    g_fUnicodeSupport);
	if (WIN_XEVENTS_CONVERT == iReturn)
	  {
	    /*
	     * The selection was offered for conversion first, so we have
	     * to process a second SelectionNotify event to get the actual
	     * data in the selection.
	     */
	    
	    /* Wait for the second SelectionNotify event */
	    XPeekIfEvent (pDisplay, &event, winLookForSelectionNotify, NULL);
	    
	    winClipboardFlushXEvents (hwnd,
				      iWindow,
				      pDisplay,
				      g_fUnicodeSupport);
	  }

	/* Special handling for WM_RENDERALLFORMATS */
	if (message == WM_RENDERALLFORMATS)
	  {
	    /* We must close the clipboard */
	    
	    if (!CloseClipboard ())
	      {
	      ErrorF ("winClipboardWindowProc - WM_RENDERALLFORMATS - "
		      "CloseClipboard () failed: %08x\n",
		      GetLastError ());
	      break;
	      }
	  }

#if 0
	ErrorF ("winClipboardWindowProc - WM_RENDER*FORMAT - Returning.\n");
#endif
	return 0;
      }
    }

  /* Let Windows perform default processing for unhandled messages */
  return DefWindowProc (hwnd, message, wParam, lParam);
}


/*
 * Process any pending Windows messages
 */

BOOL
winClipboardFlushWindowsMessageQueue (HWND hwnd)
{
  MSG			msg;

  /* Flush the messaging window queue */
  /* NOTE: Do not pass the hwnd of our messaging window to PeekMessage,
   * as this will filter out many non-window-specific messages that
   * are sent to our thread, such as WM_QUIT.
   */
  while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
    {
      /* Dispatch the message if not WM_QUIT */
      if (msg.message == WM_QUIT)
	return FALSE;
      else
	DispatchMessage (&msg);
    }
  
  return TRUE;
}
