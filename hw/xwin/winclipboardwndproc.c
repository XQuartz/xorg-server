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
extern void		*g_pClipboardDisplay;
extern Window		g_iClipboardWindow;
extern Atom		g_atomLastOwnedSelection;


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
  static HWND		s_hwndNextViewer;
  static Bool		s_fCBCInitialized;

  /* Branch on message type */
  switch (message)
    {
    case WM_DESTROY:
      {
#if 0
	ErrorF ("winClipboardWindowProc - WM_DESTROY\n");
#endif

	/* Remove ourselves from the clipboard chain */
	ChangeClipboardChain (hwnd, s_hwndNextViewer);
	
	s_hwndNextViewer = NULL;

	PostQuitMessage (0);
      }
      return 0;


    case WM_CREATE:
      {
#if 0
	ErrorF ("winClipboardWindowProc - WM_CREATE\n");
#endif
	
	/* Add ourselves to the clipboard viewer chain */
	s_hwndNextViewer = SetClipboardViewer (hwnd);
      }
      return 0;


    case WM_CHANGECBCHAIN:
      {
	if ((HWND) wParam == s_hwndNextViewer)
	  s_hwndNextViewer = (HWND) lParam;
	else if (s_hwndNextViewer)
	  SendMessage (s_hwndNextViewer, message,
		       wParam, lParam);
      }
      return 0;


    case WM_DRAWCLIPBOARD:
      {
	Display	*pDisplay = g_pClipboardDisplay;
	Window	iWindow = g_iClipboardWindow;
	int	iReturn;

	/* Pass the message on the next window in the clipboard viewer chain */
	if (s_hwndNextViewer)
	  SendMessage (s_hwndNextViewer, message, 0, 0);
	
	/* Bail on first message */
	if (!s_fCBCInitialized)
	  {
	    ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
		    "Initializing - Returning.\n");
	    s_fCBCInitialized = TRUE;
	    return 0;
	  }

	/*
	 * NOTE: We cannot bail out when NULL == GetClipboardOwner ()
	 * because some applications deal with the clipboard in a manner
	 * that causes the clipboard owner to be NULL when they are in
	 * fact taking ownership.  One example of this is the Win32
	 * native compile of emacs.
	 */
	
	/* Bail when we still own the clipboard */
	if (hwnd == GetClipboardOwner ())
	  {
#if 0
	    ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
		    "We own the clipboard, returning.\n");
#endif
	    return 0;
	  }

	/*
	 * Do not take ownership of the X11 selections when something
	 * other than CF_TEXT or CF_UNICODETEXT has been copied
	 * into the Win32 clipboard.
	 */
	if (!IsClipboardFormatAvailable (CF_TEXT)
	    && !IsClipboardFormatAvailable (CF_UNICODETEXT))
	  {
#if 0
	    ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
		    "Clipboard does not contain CF_TEXT nor "
		    "CF_UNICODETEXT.\n");
#endif

	    /*
	     * We need to make sure that the X Server has processed
	     * previous XSetSelectionOwner messages.
	     */
	    XSync (pDisplay, FALSE);
	    
	    /* Release PRIMARY selection if owned */
	    iReturn = XGetSelectionOwner (pDisplay, XA_PRIMARY);
	    if (iReturn == g_iClipboardWindow)
	      {
#if 0
		ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
			"PRIMARY selection is owned by us.\n");
#endif
		XSetSelectionOwner (pDisplay,
				    XA_PRIMARY,
				    None,
				    CurrentTime);
	      }
	    else if (BadWindow == iReturn || BadAtom == iReturn)
	      ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
		      "XGetSelection failed for PRIMARY: %d\n", iReturn);

	    /* Release CLIPBOARD selection if owned */
	    iReturn = XGetSelectionOwner (pDisplay,
					  XInternAtom (pDisplay,
						       "CLIPBOARD",
						       FALSE));
	    if (iReturn == g_iClipboardWindow)
	      {
#if 0
		ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
			"CLIPBOARD selection is owned by us.\n");
#endif
		XSetSelectionOwner (pDisplay,
				    XInternAtom (pDisplay,
						 "CLIPBOARD",
						 FALSE),
				    None,
				    CurrentTime);
	      }
	    else if (BadWindow == iReturn || BadAtom == iReturn)
	      ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
		      "XGetSelection failed for CLIPBOARD: %d\n", iReturn);

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
#if 0
	else
	  {
	    ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
		    "Reasserted ownership of PRIMARY\n");
	  }
#endif
	
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
#if 0
	else
	  {
	    ErrorF ("winClipboardWindowProc - WM_DRAWCLIPBOARD - "
		    "Reasserted ownership of CLIPBOARD\n");
	  }
#endif
	
	/* Flush the pending SetSelectionOwner event now */
	XFlush (pDisplay);
      }
      return 0;


    case WM_DESTROYCLIPBOARD:
      /*
       * NOTE: Intentionally do nothing.
       * Changes in the Win32 clipboard are handled by WM_DRAWCLIPBOARD
       * above.  We only process this message to conform to the specs
       * for delayed clipboard rendering in Win32.  You might think
       * that we need to release ownership of the X11 selections, but
       * we do not, because a WM_DRAWCLIPBOARD message will closely
       * follow this message and reassert ownership of the X11
       * selections, handling the issue for us.
       */
      return 0;


    case WM_RENDERFORMAT:
    case WM_RENDERALLFORMATS:
      {
	XEvent	event;
	int	iReturn;
	Display *pDisplay = g_pClipboardDisplay;
	Window	iWindow = g_iClipboardWindow;
	Bool	fConvertToUnicode;

#if 0
	ErrorF ("winClipboardWindowProc - WM_RENDER*FORMAT - Hello.\n");
#endif

	/* Flag whether to convert to Unicode or not */
	if (message == WM_RENDERALLFORMATS)
	  fConvertToUnicode = FALSE;
	else
	  fConvertToUnicode = g_fUnicodeSupport && (CF_UNICODETEXT == wParam);

	/* Request the selection contents */
	iReturn = XConvertSelection (pDisplay,
				     g_atomLastOwnedSelection,
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

	    /* Close clipboard if we have it open already */
	    if (GetOpenClipboardWindow () == hwnd)
	      {
		CloseClipboard ();
	      }	    

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
					    fConvertToUnicode);
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
				      fConvertToUnicode);
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
