/* $TOG: InitOutput.c /main/20 1998/02/10 13:23:56 kaleb $ */
/*

Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/Xserver/hw/xwin/InitOutput.c,v 1.35 2003/10/08 11:13:02 eich Exp $ */

#include "win.h"
#include "winmsg.h"
#include "winconfig.h"
#include "winprefs.h"


/*
 * References to external symbols
 */

extern int			g_iNumScreens;
extern winScreenInfo		g_ScreenInfo[];
extern int			g_iLastScreen;
extern Bool			g_fInitializedDefaultScreens;
extern FILE			*g_pfLog;
extern Bool			g_fUnicodeClipboard;
extern Bool			g_fXdmcpEnabled;
extern int			g_iScreenPrivateIndex;
extern int			g_fdMessageQueue;
extern const char *		g_pszQueryHost;
extern HINSTANCE		g_hInstance;

int		g_iLogVerbose = 4;
char *		g_pszLogFile = WIN_LOG_FNAME;
Bool		g_fLogInited = FALSE;

extern HMODULE			g_hmodDirectDraw;
extern FARPROC			g_fpDirectDrawCreate;
extern FARPROC			g_fpDirectDrawCreateClipper;
  
extern HMODULE			g_hmodCommonControls;
extern FARPROC			g_fpTrackMouseEvent;
  
  
/*
 * Function prototypes
 */

#ifdef DDXOSVERRORF
void OsVendorVErrorF (const char *pszFormat, va_list va_args);
#endif

#if defined(DDXOSRESET)
void OsVendorReset ();
#endif

void winInitializeDefaultScreens (void);


/*
 * For the depth 24 pixmap we default to 32 bits per pixel, but
 * we change this pixmap format later if we detect that the display
 * is going to be running at 24 bits per pixel.
 *
 * FIXME: On second thought, don't DIBs only support 32 bits per pixel?
 * DIBs are the underlying bitmap used for DirectDraw surfaces, so it
 * seems that all pixmap formats with depth 24 would be 32 bits per pixel.
 * Confirm whether depth 24 DIBs can have 24 bits per pixel, then remove/keep
 * the bits per pixel adjustment and update this comment to reflect the
 * situation.  Harold Hunt - 2002/07/02
 */

static PixmapFormatRec g_PixmapFormats[] = {
  { 1,    1,      BITMAP_SCANLINE_PAD },
  { 4,    8,      BITMAP_SCANLINE_PAD },
  { 8,    8,      BITMAP_SCANLINE_PAD },
  { 15,   16,     BITMAP_SCANLINE_PAD },
  { 16,   16,     BITMAP_SCANLINE_PAD },
  { 24,   32,     BITMAP_SCANLINE_PAD },
#ifdef RENDER
  { 32,   32,     BITMAP_SCANLINE_PAD }
#endif
};

const int NUMFORMATS = sizeof (g_PixmapFormats) / sizeof (g_PixmapFormats[0]);

#if defined(DDXOSRESET)
extern pthread_t		g_ptClipboardProc;
extern HWND			g_hwndClipboard;
extern Bool			g_fClipboard;

/*
 * Called right before KillAllClients when the server is going to reset,
 * allows us to shutdown our seperate threads cleanly.
 */

void
OsVendorReset ()
{
  int			i;

  ErrorF ("OsVendorReset - Hello\n");

  /* Close down clipboard resources */
  if (g_fClipboard)
    {
      /* Synchronously destroy the clipboard window */
      if (g_hwndClipboard != NULL)
	SendMessage (g_hwndClipboard, WM_DESTROY, 0, 0);
      
      /* Wait for the clipboard thread to exit */
      pthread_join (g_ptClipboardProc, NULL);

      ErrorF ("OsVendorReset - Clipboard thread has exited.\n");
    }
}
#endif


/* See Porting Layer Definition - p. 57 */
void
ddxGiveUp()
{
#if CYGDEBUG
  winErrorFVerb (2, "ddxGiveUp\n");
#endif

  /* Notify the worker threads we're exiting */
  winDeinitClipboard ();
  winDeinitMultiWindowWM ();

  /* Close our handle to our message queue */
  if (g_fdMessageQueue != WIN_FD_INVALID)
    {
      /* Close /dev/windows */
      close (g_fdMessageQueue);

      /* Set the file handle to invalid */
      g_fdMessageQueue = WIN_FD_INVALID;
    }

  if (!g_fLogInited) {
    LogInit(g_pszLogFile, NULL);
    g_fLogInited = TRUE;
  }  
  LogClose();

  /*
   * At this point we aren't creating any new screens, so
   * we are guaranteed to not need the DirectDraw functions.
   */
  if (g_hmodDirectDraw != NULL)
    {
      FreeLibrary (g_hmodDirectDraw);
      g_hmodDirectDraw = NULL;
      g_fpDirectDrawCreate = NULL;
      g_fpDirectDrawCreateClipper = NULL;
    }

  /* Unload our TrackMouseEvent funtion pointer */
  if (g_hmodCommonControls != NULL)
    {
      FreeLibrary (g_hmodCommonControls);
      g_hmodCommonControls = NULL;
      g_fpTrackMouseEvent = (FARPROC) (void (*)(void))NoopDDA;
    }
  
  /* Tell Windows that we want to end the app */
  PostQuitMessage (0);
}


/* See Porting Layer Definition - p. 57 */
void
AbortDDX (void)
{
#if CYGDEBUG
  winErrorFVerb (2, "AbortDDX\n");
#endif
  ddxGiveUp ();
}


void
OsVendorInit (void)
{
  /* Re-initialize global variables on server reset */
  winInitializeGlobals ();

#ifdef DDXOSVERRORF
  if (!OsVendorVErrorFProc)
    OsVendorVErrorFProc = OsVendorVErrorF;
#endif

  if (!g_fLogInited) {
    LogInit(g_pszLogFile, NULL);
    g_fLogInited = TRUE;
  }  
  LogSetParameter(XLOG_FLUSH, 1);
  LogSetParameter(XLOG_VERBOSITY, g_iLogVerbose);

  /* Add a default screen if no screens were specified */
  if (g_iNumScreens == 0)
    {
      winErrorFVerb (2, "OsVendorInit - Creating bogus screen 0\n");

      /* 
       * We need to initialize default screens if no arguments
       * were processed.  Otherwise, the default screens would
       * already have been initialized by ddxProcessArgument ().
       */
      winInitializeDefaultScreens ();

      /*
       * Add a screen 0 using the defaults set by 
       * winInitializeDefaultScreens () and any additional parameters
       * processed by ddxProcessArgument ().
       */
      g_iNumScreens = 1;
      g_iLastScreen = 0;

      /* We have to flag this as an explicit screen, even though it isn't */
      g_ScreenInfo[0].fExplicitScreen = TRUE;
    }
}


/* See Porting Layer Definition - p. 57 */
void
ddxUseMsg (void)
{
  ErrorF ("-depth bits_per_pixel\n"
	  "\tSpecify an optional bitdepth to use in fullscreen mode\n"
	  "\twith a DirectDraw engine.\n");

  ErrorF ("-emulate3buttons [timeout]\n"
	  "\tEmulate 3 button mouse with an optional timeout in\n"
	  "\tmilliseconds.\n");

  ErrorF ("-engine engine_type_id\n"
	  "\tOverride the server's automatically selected engine type:\n"
	  "\t\t1 - Shadow GDI\n"
	  "\t\t2 - Shadow DirectDraw\n"
	  "\t\t4 - Shadow DirectDraw4 Non-Locking\n"
	  "\t\t16 - Native GDI - experimental\n");

  ErrorF ("-fullscreen\n"
	  "\tRun the server in fullscreen mode.\n");
  
  ErrorF ("-refresh rate_in_Hz\n"
	  "\tSpecify an optional refresh rate to use in fullscreen mode\n"
	  "\twith a DirectDraw engine.\n");

  ErrorF ("-screen scr_num [width height]\n"
	  "\tEnable screen scr_num and optionally specify a width and\n"
	  "\theight for that screen.\n");

  ErrorF ("-lesspointer\n"
	  "\tHide the windows mouse pointer when it is over an inactive\n"
          "\tCygwin/X window.  This prevents ghost cursors appearing where\n"
	  "\tthe Windows cursor is drawn overtop of the X cursor\n");

  ErrorF ("-nodecoration\n"
          "\tDo not draw a window border, title bar, etc.  Windowed\n"
	  "\tmode only.\n");

  ErrorF ("-rootless\n"
	  "\tEXPERIMENTAL: Run the server in rootless mode.\n");

  ErrorF ("-pseudorootless\n"
	  "\tEXPERIMENTAL: Run the server in pseudo-rootless mode.\n");

  ErrorF ("-multiwindow\n"
	  "\tEXPERIMENTAL: Run the server in multi-window mode.\n");

  ErrorF ("-multiplemonitors\n"
	  "\tEXPERIMENTAL: Use the entire virtual screen if multiple\n"
	  "\tmonitors are present.\n");

  ErrorF ("-clipboard\n"
	  "\tEXPERIMENTAL: Run the clipboard integration module.\n");

  ErrorF ("-scrollbars\n"
	  "\tIn windowed mode, allow screens bigger than the Windows desktop.\n"
	  "\tMoreover, if the window has decorations, one can now resize\n"
	  "\tit.\n");

  ErrorF ("-[no]trayicon\n"
          "\tDo not create a tray icon.  Default is to create one\n"
	  "\ticon per screen.  You can globally disable tray icons with\n"
	  "\t-notrayicon, then enable it for specific screens with\n"
	  "\t-trayicon for those screens.\n");

  ErrorF ("-clipupdates num_boxes\n"
	  "\tUse a clipping region to constrain shadow update blits to\n"
	  "\tthe updated region when num_boxes, or more, are in the\n"
	  "\tupdated region.  Currently supported only by `-engine 1'.\n");

  ErrorF ("-emulatepseudo\n"
	  "\tCreate a depth 8 PseudoColor visual when running in\n"
	  "\tdepths 15, 16, 24, or 32, collectively known as TrueColor\n"
	  "\tdepths.  The PseudoColor visual does not have correct colors,\n"
	  "\tand it may crash, but it at least allows you to run your\n"
	  "\tapplication in TrueColor modes.\n");

  ErrorF ("-[no]unixkill\n"
          "\tCtrl+Alt+Backspace exits the X Server.\n");

  ErrorF ("-[no]winkill\n"
          "\tAlt+F4 exits the X Server.\n");

  ErrorF ("-xf86config\n"
          "\tSpecify a configuration file.\n");

  ErrorF ("-keyboard\n"
	  "\tSpecify a keyboard device from the configuration file.\n");

  ErrorF ("-nounicodeclipboard\n"
	  "\tDo not use Unicode clipboard even if NT-based platform.\n");

  /* TODO: new options */ 
}


#ifdef DDXTIME /* from ServerOSDefines */
CARD32
GetTimeInMillis (void)
{
  return GetTickCount ();
}
#endif /* DDXTIME */


/* See Porting Layer Definition - p. 20 */
/*
 * Do any global initialization, then initialize each screen.
 * 
 * NOTE: We use ddxProcessArgument, so we don't need to touch argc and argv
 */

void
InitOutput (ScreenInfo *screenInfo, int argc, char *argv[])
{
  int		i;
  int		iMaxConsecutiveScreen = 0;

#if CYGDEBUG
  winErrorFVerb (2, "InitOutput\n");
#endif

  /* Try to read the XF86Config-style configuration file */
  if (!winReadConfigfile ())
    winErrorFVerb (1, "InitOutput - Error reading config file\n");

  /* Setup global screen info parameters */
  screenInfo->imageByteOrder = IMAGE_BYTE_ORDER;
  screenInfo->bitmapScanlinePad = BITMAP_SCANLINE_PAD;
  screenInfo->bitmapScanlineUnit = BITMAP_SCANLINE_UNIT;
  screenInfo->bitmapBitOrder = BITMAP_BIT_ORDER;
  screenInfo->numPixmapFormats = NUMFORMATS;
  
  /* Describe how we want common pixmap formats padded */
  for (i = 0; i < NUMFORMATS; i++)
    {
      screenInfo->formats[i] = g_PixmapFormats[i];
    }

  /* Load pointers to DirectDraw functions */
  winGetDDProcAddresses ();
  
  /* Detect supported engines */
  winDetectSupportedEngines ();

  /* Load common controls library */
  g_hmodCommonControls = LoadLibraryEx ("comctl32.dll", NULL, 0);

  /* Load TrackMouseEvent function pointer */  
  g_fpTrackMouseEvent = GetProcAddress (g_hmodCommonControls,
					 "_TrackMouseEvent");
  if (g_fpTrackMouseEvent == NULL)
    {
      winErrorFVerb (1, "InitOutput - Could not get pointer to function\n"
	      "\t_TrackMouseEvent in comctl32.dll.  Try installing\n"
	      "\tInternet Explorer 3.0 or greater if you have not\n"
	      "\talready.\n");

      /* Free the library since we won't need it */
      FreeLibrary (g_hmodCommonControls);
      g_hmodCommonControls = NULL;

      /* Set function pointer to point to no operation function */
      g_fpTrackMouseEvent = (FARPROC) (void (*)(void))NoopDDA;
    }

  /*
   * Check for a malformed set of -screen parameters.
   * Examples of malformed parameters:
   *	XWin -screen 1
   *	XWin -screen 0 -screen 2
   *	XWin -screen 1 -screen 2
   */
  for (i = 0; i < MAXSCREENS; i++)
    {
      if (g_ScreenInfo[i].fExplicitScreen)
	iMaxConsecutiveScreen = i + 1;
    }
  winErrorFVerb (2, "InitOutput - g_iNumScreens: %d iMaxConsecutiveScreen: %d\n",
	  g_iNumScreens, iMaxConsecutiveScreen);
  if (g_iNumScreens < iMaxConsecutiveScreen)
    FatalError ("InitOutput - Malformed set of screen parameter(s).  "
		"Screens must be specified consecutively starting with "
		"screen 0.  That is, you cannot have only a screen 1, nor "
		"could you have screen 0 and screen 2.  You instead must have "
		"screen 0, or screen 0 and screen 1, respectively.  Of "
		"you can specify as many screens as you want from 0 up to "
		"%d.\n", MAXSCREENS - 1);

  /* Store the instance handle */
  g_hInstance = GetModuleHandle (NULL);

  /* Initialize each screen */
  for (i = 0; i < g_iNumScreens; i++)
    {
      /* Initialize the screen */
      if (-1 == AddScreen (winScreenInit, argc, argv))
	{
	  FatalError ("InitOutput - Couldn't add screen %d", i);
	}
    }

  /* Load preferences from XWinrc file */
  LoadPreferences();

  /* Generate a cookie used by internal clients for authorization */
  if (g_fXdmcpEnabled)
    winGenerateAuthorization ();

#if CYGDEBUG || YES
  winErrorFVerb (2, "InitOutput - Returning.\n");
#endif
}
