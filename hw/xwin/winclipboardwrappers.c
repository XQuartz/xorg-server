/*
 *Copyright (C) 2003-2004 Harold L Hunt II All Rights Reserved.
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
 *NONINFRINGEMENT. IN NO EVENT SHALL HAROLD L HUNT II BE LIABLE FOR
 *ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 *CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *Except as contained in this notice, the name of Harold L Hunt II
 *shall not be used in advertising or otherwise to promote the sale, use
 *or other dealings in this Software without prior written authorization
 *from Harold L Hunt II.
 *
 * Authors:	Harold L Hunt II
 */

#include "win.h"
#include "dixstruct.h"


/*
 * Local function prototypes
 */

DISPATCH_PROC(winProcEstablishConnection);
DISPATCH_PROC(winProcQueryTree);
DISPATCH_PROC(winProcSetSelectionOwner);

/*
 * References to external symbols
 */

extern Bool		g_fUnicodeSupport;
extern int		g_iNumScreens;
extern unsigned int	g_uiAuthDataLen;
extern char		*g_pAuthData;
extern Bool		g_fXdmcpEnabled;
extern Bool		g_fClipboardLaunched;

extern winDispatchProcPtr	winProcEstablishConnectionOrig;
extern winDispatchProcPtr	winProcQueryTreeOrig;
extern winDispatchProcPtr	winProcSetSelectionOwnerOrig;



/*
 * Wrapper for internal QueryTree function.
 * Hides the clipboard client when it is the only client remaining.
 */

int
winProcQueryTree (ClientPtr client)
{
  int			i;
  int			iReturn;

  /*
   * This procedure is only used for initialization.
   * We can unwrap the original procedure at this point
   * so that this function is no longer called until the
   * server resets and the function is wrapped again.
   */
  ProcVector[X_QueryTree] = winProcQueryTreeOrig;

  /*
   * Call original function and bail if it fails.
   * NOTE: We must do this first, since we need XdmcpOpenDisplay
   * to be called before we initialize our clipboard client.
   */
  iReturn = (*winProcQueryTreeOrig) (client);
  if (iReturn != 0)
    {
      ErrorF ("winProcQueryTree - ProcQueryTree failed, bailing.\n");
      return iReturn;
    }

  /* If the clipboard client has already been started, abort */
  if (g_fClipboardLaunched)
    {
      ErrorF ("winProcQueryTree - Clipboard client already "
	      "launched, returning.\n");
      return iReturn;
    }

  /* Walk the list of screens */
  for (i = 0; i < g_iNumScreens; i++)
    {
      ScreenPtr		pScreen = g_ScreenInfo[i].pScreen;
      winScreenPriv(pScreen);

      /* Startup the clipboard client if clipboard mode is being used */
      if (g_fXdmcpEnabled
	  && g_ScreenInfo[i].fClipboard)
	{
	  /*
	   * NOTE: The clipboard client is started here for a reason:
	   * 1) Assume you are using XDMCP (e.g. XWin -query %hostname%)
	   * 2) If the clipboard client attaches during X Server startup,
	   *    then it becomes the "magic client" that causes the X Server
	   *    to reset if it exits.
	   * 3) XDMCP calls KillAllClients when it starts up.
	   * 4) The clipboard client is a client, so it is killed.
	   * 5) The clipboard client is the "magic client", so the X Server
	   *    resets itself.
	   * 6) This repeats ad infinitum.
	   * 7) We avoid this by waiting until at least one client (could
	   *    be XDM, could be another client) connects, which makes it
	   *    almost certain that the clipboard client will not connect
	   *    until after XDM when using XDMCP.
	   * 8) Unfortunately, there is another problem.
	   * 9) XDM walks the list of windows with XQueryTree,
	   *    killing any client it finds with a window.
	   * 10)Thus, when using XDMCP we wait until the first call
	   *    to ProcQueryTree before we startup the clipboard client.
	   *    This should prevent XDM from finding the clipboard client,
	   *    since it has not yet created a window.
	   * 11)Startup when not using XDMCP is handled in
	   *    winProcEstablishConnection.
	   */
	  
	  /* Create the clipboard client thread */
	  if (!winInitClipboard (&pScreenPriv->ptClipboardProc,
				 &pScreenPriv->fClipboardStarted,
				 &pScreenPriv->hwndClipboard,
				 &pScreenPriv->pClipboardDisplay,
				 &pScreenPriv->iClipboardWindow,
				 &pScreenPriv->hwndClipboardNextViewer,
				 &pScreenPriv->fCBCInitialized,
				 &pScreenPriv->atomLastOwnedSelection,
				 g_ScreenInfo[i].dwScreen))
	    {
	      ErrorF ("winProcQueryTree - winClipboardInit "
		      "failed.\n");
	      return iReturn;
	    }

	  ErrorF ("winProcQueryTree - winInitClipboard returned.\n");
	}
    }

  /* Flag that clipboard client has been launched */
  g_fClipboardLaunched = TRUE;

  return iReturn;
}


/*
 * Wrapper for internal EstablishConnection function.
 * Initializes internal clients that must not be started until
 * an external client has connected.
 */

int
winProcEstablishConnection (ClientPtr client)
{
  int			i;
  int			iReturn;
  static int		s_iCallCount = 0;
  static unsigned long	s_ulServerGeneration = 0;

  ErrorF ("winProcEstablishConnection - Hello\n");

  /* Watch for server reset */
  if (s_ulServerGeneration != serverGeneration)
    {
      /* Save new generation number */
      s_ulServerGeneration = serverGeneration;

      /* Reset call count */
      s_iCallCount = 0;
    }

  /* Increment call count */
  ++s_iCallCount;

  /* Wait for second call when Xdmcp is enabled */
  if (g_fXdmcpEnabled
      && !g_fClipboardLaunched
      && s_iCallCount < 3)
    {
      ErrorF ("winProcEstablishConnection - Xdmcp enabled, waiting to "
	      "start clipboard client until third call.\n");
      return (*winProcEstablishConnectionOrig) (client);
    }

  /*
   * This procedure is only used for initialization.
   * We can unwrap the original procedure at this point
   * so that this function is no longer called until the
   * server resets and the function is wrapped again.
   */
  InitialVector[2] = winProcEstablishConnectionOrig;

  /*
   * Call original function and bail if it fails.
   * NOTE: We must do this first, since we need XdmcpOpenDisplay
   * to be called before we initialize our clipboard client.
   */
  iReturn = (*winProcEstablishConnectionOrig) (client);
  if (iReturn != 0)
    {
      ErrorF ("winProcEstablishConnection - ProcEstablishConnection "
	      "failed, bailing.\n");
      return iReturn;
    }

  /* Clear original function pointer */
  winProcEstablishConnectionOrig = NULL;

  /* If the clipboard client has already been started, abort */
  if (g_fClipboardLaunched)
    {
      ErrorF ("winProcEstablishConnection - Clipboard client already "
	      "launched, returning.\n");
      return iReturn;
    }

  /* Walk the list of screens */
  for (i = 0; i < g_iNumScreens; i++)
    {
      ScreenPtr		pScreen = g_ScreenInfo[i].pScreen;
      winScreenPriv(pScreen);

      /* Startup the clipboard client if clipboard mode is being used */
      if (g_ScreenInfo[i].fClipboard)
	{
	  /*
	   * NOTE: The clipboard client is started here for a reason:
	   * 1) Assume you are using XDMCP (e.g. XWin -query %hostname%)
	   * 2) If the clipboard client attaches during X Server startup,
	   *    then it becomes the "magic client" that causes the X Server
	   *    to reset if it exits.
	   * 3) XDMCP calls KillAllClients when it starts up.
	   * 4) The clipboard client is a client, so it is killed.
	   * 5) The clipboard client is the "magic client", so the X Server
	   *    resets itself.
	   * 6) This repeats ad infinitum.
	   * 7) We avoid this by waiting until at least one client (could
	   *    be XDM, could be another client) connects, which makes it
	   *    almost certain that the clipboard client will not connect
	   *    until after XDM when using XDMCP.
	   * 8) Unfortunately, there is another problem.
	   * 9) XDM walks the list of windows with XQueryTree,
	   *    killing any client it finds with a window.
	   * 10)Thus, when using XDMCP we wait until the second call
	   *    to ProcEstablishCeonnection before we startup the clipboard
	   *    client.  This should prevent XDM from finding the clipboard
	   *    client, since it has not yet created a window.
	   */
	  
	  /* Create the clipboard client thread */
	  if (!winInitClipboard (&pScreenPriv->ptClipboardProc,
				 &pScreenPriv->fClipboardStarted,
				 &pScreenPriv->hwndClipboard,
				 &pScreenPriv->pClipboardDisplay,
				 &pScreenPriv->iClipboardWindow,
				 &pScreenPriv->hwndClipboardNextViewer,
				 &pScreenPriv->fCBCInitialized,
				 &pScreenPriv->atomLastOwnedSelection,
				 g_ScreenInfo[i].dwScreen))
	    {
	      ErrorF ("winProcEstablishConnection - winClipboardInit "
		      "failed.\n");
	      return iReturn;
	    }

	  ErrorF ("winProcEstablishConnection - winInitClipboard returned.\n");
	}
    }

  /* Flag that clipboard client has been launched */
  g_fClipboardLaunched = TRUE;

  return iReturn;
}


/*
 * Wrapper for internal SetSelectionOwner function.
 * Grabs ownership of Windows clipboard when X11 clipboard owner changes.
 */

int
winProcSetSelectionOwner (ClientPtr client)
{
  DrawablePtr		pDrawable;
  ScreenPtr		pScreen = NULL;
  winPrivScreenPtr	pScreenPriv = NULL;
  HWND			hwndClipboard = NULL;
  WindowPtr		pWindow = None;
  REQUEST(xSetSelectionOwnerReq);
  
  REQUEST_SIZE_MATCH(xSetSelectionOwnerReq);

#if 0
  ErrorF ("winProcSetSelectionOwner - Hello.\n");
#endif

  /* Grab the Window from the request */
  if (stuff->window != None)
    {
      pWindow = (WindowPtr) SecurityLookupWindow (stuff->window, client,
						  SecurityReadAccess);
      
      if (!pWindow)
	{
	  ErrorF ("winProcSetSelectionOwner - Found BadWindow, aborting.\n");
	  goto winProcSetSelectionOwner_Done;
	}
    }
  else
    {
#if 0
      ErrorF ("winProcSetSelectionOwner - No window specified, aborting.\n");
#endif

      /* Abort if we don't have a drawable for the client */
      if ((pDrawable = client->lastDrawable) == NULL)
	{
	  ErrorF ("winProcSetSelectionOwner - Client has no "
		  "lastDrawable, aborting.\n");
	  goto winProcSetSelectionOwner_Done;
	}
      
      /* Abort if we don't have a screen for the drawable */
      if ((pScreen = pDrawable->pScreen) == NULL)
	{
	  ErrorF ("winProcSetSelectionOwner - Drawable has no screen, "
		  "aborting.\n");
	  goto winProcSetSelectionOwner_Done;
	}

      /* Abort if no screen privates */
      if ((pScreenPriv = winGetScreenPriv (pScreen)) == NULL)
	{
	  ErrorF ("winProcSetSelectionOwner - Screen has no privates, "
		  "aborting.\n");
	  goto winProcSetSelectionOwner_Done;
	}

      /* Abort if clipboard not completely initialized yet */
      if (!pScreenPriv->fClipboardStarted)
	{
	  ErrorF ("winProcSetSelectionOwner - Clipboard not yet started, "
		  "aborting.\n");
	  goto winProcSetSelectionOwner_Done;
	}

      /* Abort if WM_DRAWCLIPBOARD disowned the selection */
      if (pScreenPriv->iClipboardWindow == client->lastDrawableID)
	{
	  ErrorF ("winProcSetSelectionOwner - WM_DRAWCLIPBOARD disowned "
		  "the selection, aborting.\n");
	  goto winProcSetSelectionOwner_Done;
	}

      /* Check if we own the clipboard */
      if (pScreenPriv->hwndClipboard != NULL
	  && pScreenPriv->hwndClipboard == GetClipboardOwner ())
	{
	  ErrorF ("winProcSetSelectionOwner - We currently own the "
		  "clipboard, releasing ownership.\n");

	  /* Release ownership of the Windows clipboard */
	  OpenClipboard (NULL);
	  EmptyClipboard ();
	  CloseClipboard ();
	}

      goto winProcSetSelectionOwner_Done;
    }

  /* Abort if invalid selection */
  if (!ValidAtom (stuff->selection))
    {
      ErrorF ("winProcSetSelectionOwner - Found BadAtom, aborting.\n");
      goto winProcSetSelectionOwner_Done;
    }

  /* Cast Window to Drawable */
  pDrawable = (DrawablePtr) pWindow;


  /*
   * Get the screen pointer from the client pointer
   */

  /* Abort if we don't have a screen for the window */
  if ((pScreen = pDrawable->pScreen) == NULL)
    {
      ErrorF ("winProcSetSelectionOwner - Window has no screen.\n");
      goto winProcSetSelectionOwner_Done;
    }

  /* Abort if no screen privates */
  if ((pScreenPriv = winGetScreenPriv (pScreen)) == NULL)
    {
      ErrorF ("winProcSetSelectionOwner - Screen has no privates.\n");
      goto winProcSetSelectionOwner_Done;
    }

  /* Abort if clipboard not completely initialized yet */
  if (!pScreenPriv->fClipboardStarted)
    {
      ErrorF ("\nwinProcSetSelectionOwner - Clipboard not yet started.\n\n");
      goto winProcSetSelectionOwner_Done;
    }

#if 0
  ErrorF ("winProcSetSelectionOwner - "
	  "iWindow: %d client->lastDrawableID: %d target: %d\n",
	  pScreenPriv->iClipboardWindow, pDrawable->id, stuff->selection);
#endif

  /* Abort if no clipboard manager window */
  if (pScreenPriv->iClipboardWindow == 0)
    {
      ErrorF ("winProcSetSelectionOwner - No X clipboard window.\n");
      goto winProcSetSelectionOwner_Done;
    }

  /* Abort if clipboard manager is owning the selection */
  if (pDrawable->id == pScreenPriv->iClipboardWindow)
    {
      ErrorF ("winProcSetSelectionOwner - We changed ownership, "
	      "aborting.\n");
      goto winProcSetSelectionOwner_Done;
    }

  /* Abort if root window is taking ownership */
  if (pDrawable->id == 0)
    {
      ErrorF ("winProcSetSelectionOwner - Root window taking ownership, "
	      "aborting\n");
      goto winProcSetSelectionOwner_Done;
    }

  /* Abort if no clipboard window */
  if ((hwndClipboard = pScreenPriv->hwndClipboard) == NULL
      || !IsWindow (hwndClipboard))
    {
      ErrorF ("winProcSetSelectionOwner - No valid clipboard window "
	      "handle.\n");
      goto winProcSetSelectionOwner_Done;
    }

  /* Access the Windows clipboard */
  if (!OpenClipboard (hwndClipboard))
    {
      ErrorF ("winProcSetSelectionOwner - OpenClipboard () failed: %08x\n",
	      (int) GetLastError ());
      goto winProcSetSelectionOwner_Done;
    }

  /* Take ownership of the Windows clipboard */
  if (!EmptyClipboard ())
    {
      ErrorF ("winProcSetSelectionOwner - EmptyClipboard () failed: %08x\n",
	      (int) GetLastError ());
      goto winProcSetSelectionOwner_Done;
    }

  /* Advertise Unicode if we support it */
  if (g_fUnicodeSupport)
    SetClipboardData (CF_UNICODETEXT, NULL);

  /* Always advertise regular text */
  SetClipboardData (CF_TEXT, NULL);

  /* Save handle to last owned selection */
  pScreenPriv->atomLastOwnedSelection = stuff->selection;

  /* Release the clipboard */
  if (!CloseClipboard ())
    {
      ErrorF ("winProcSetSelectionOwner - CloseClipboard () failed: "
	      "%08x\n",
	      (int) GetLastError ());
      goto winProcSetSelectionOwner_Done;
    }

 winProcSetSelectionOwner_Done:
  return (*winProcSetSelectionOwnerOrig) (client);
}
