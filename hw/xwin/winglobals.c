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


/*
 * General global variables
 */

int		g_iNumScreens = 0;
winScreenInfo	g_ScreenInfo[MAXSCREENS];
int		g_iLastScreen = -1;
int		g_fdMessageQueue = WIN_FD_INVALID;
int		g_iScreenPrivateIndex = -1;
int		g_iCmapPrivateIndex = -1;
int		g_iGCPrivateIndex = -1;
int		g_iPixmapPrivateIndex = -1;
int		g_iWindowPrivateIndex = -1;
unsigned long	g_ulServerGeneration = 0;
Bool		g_fInitializedDefaultScreens = FALSE;
FILE		*g_pfLog = NULL;
DWORD		g_dwEnginesSupported = 0;
HINSTANCE	g_hInstance = 0;
HWND		g_hDlgDepthChange = NULL;
HWND		g_hDlgExit = NULL;
const char *	g_pszQueryHost = NULL;
Bool		g_fUnicodeClipboard = TRUE;
Bool		g_fClipboard = FALSE;
Bool		g_fXdmcpEnabled = FALSE;


/*
 * Global variables for dynamically loaded libraries and
 * their function pointers
 */

HMODULE		g_hmodDirectDraw = NULL;
FARPROC		g_fpDirectDrawCreate = NULL;
FARPROC		g_fpDirectDrawCreateClipper = NULL;

HMODULE		g_hmodCommonControls = NULL;
FARPROC		g_fpTrackMouseEvent = (FARPROC) (void (*)())NoopDDA;


/*
 * Wrapped DIX functions
 */
winDispatchProcPtr	winProcEstablishConnectionOrig = NULL;
winDispatchProcPtr	winProcQueryTreeOrig = NULL;
winDispatchProcPtr	winProcSetSelectionOwnerOrig = NULL;


/*
 * Clipboard variables
 */

Bool			g_fClipboardLaunched = FALSE;
Bool			g_fClipboardStarted = FALSE;
pthread_t		g_ptClipboardProc;
Bool			g_fClipboardStarted;
HWND			g_hwndClipboard;
void			*g_pClipboardDisplay;
Window			g_iClipboardWindow;
Atom			g_atomLastOwnedSelection;


/*
 * Re-initialize global variables that are invalidated
 * by a server reset.
 */

void
winInitializeGlobals ()
{
  g_fClipboardLaunched = FALSE;
}
