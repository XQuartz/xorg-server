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

#include "win.h"
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


/*
 * Function prototypes
 */

#ifdef DDXOSVERRORF
void OsVendorVErrorF (const char *pszFormat, va_list va_args);
#endif


/*
 * Process arguments on the command line
 */

void
winInitializeDefaultScreens (void)
{
  int                   i;
  DWORD			dwWidth, dwHeight;

  /* Bail out early if default screens have already been initialized */

  if (g_fInitializedDefaultScreens)
    return;

  /* Zero the memory used for storing the screen info */
  ZeroMemory (g_ScreenInfo, MAXSCREENS * sizeof (winScreenInfo));

  /* Get default width and height */
  /*
   * NOTE: These defaults will cause the window to cover only
   * the primary monitor in the case that we have multiple monitors.
   */
  dwWidth = GetSystemMetrics (SM_CXSCREEN);
  dwHeight = GetSystemMetrics (SM_CYSCREEN);

  ErrorF ("winInitializeDefaultScreens - w %ld h %ld\n", dwWidth, dwHeight);

  /* Set a default DPI, if no parameter was passed */
  if (monitorResolution == 0)
    monitorResolution = WIN_DEFAULT_DPI;

  for (i = 0; i < MAXSCREENS; ++i)
    {
      g_ScreenInfo[i].dwScreen = i;
      g_ScreenInfo[i].dwWidth  = dwWidth;
      g_ScreenInfo[i].dwHeight = dwHeight;
      g_ScreenInfo[i].dwUserWidth  = dwWidth;
      g_ScreenInfo[i].dwUserHeight = dwHeight;
      g_ScreenInfo[i].fUserGaveHeightAndWidth
	=  WIN_DEFAULT_USER_GAVE_HEIGHT_AND_WIDTH;
      g_ScreenInfo[i].dwBPP = WIN_DEFAULT_BPP;
      g_ScreenInfo[i].dwClipUpdatesNBoxes = WIN_DEFAULT_CLIP_UPDATES_NBOXES;
      g_ScreenInfo[i].fEmulatePseudo = WIN_DEFAULT_EMULATE_PSEUDO;
      g_ScreenInfo[i].dwRefreshRate = WIN_DEFAULT_REFRESH;
      g_ScreenInfo[i].pfb = NULL;
      g_ScreenInfo[i].fFullScreen = FALSE;
      g_ScreenInfo[i].fDecoration = TRUE;
      g_ScreenInfo[i].fRootless = FALSE;
      g_ScreenInfo[i].fMultiWindow = FALSE;
      g_ScreenInfo[i].fMultipleMonitors = FALSE;
      g_ScreenInfo[i].fClipboard = FALSE;
      g_ScreenInfo[i].fLessPointer = FALSE;
      g_ScreenInfo[i].fScrollbars = FALSE;
      g_ScreenInfo[i].fNoTrayIcon = FALSE;
      g_ScreenInfo[i].iE3BTimeout = WIN_E3B_OFF;
      g_ScreenInfo[i].dwWidth_mm = (dwWidth / WIN_DEFAULT_DPI)
	* 25.4;
      g_ScreenInfo[i].dwHeight_mm = (dwHeight / WIN_DEFAULT_DPI)
	* 25.4;
      g_ScreenInfo[i].fUseWinKillKey = WIN_DEFAULT_WIN_KILL;
      g_ScreenInfo[i].fUseUnixKillKey = WIN_DEFAULT_UNIX_KILL;
      g_ScreenInfo[i].fIgnoreInput = FALSE;
      g_ScreenInfo[i].fExplicitScreen = FALSE;
    }

  /* Signal that the default screens have been initialized */
  g_fInitializedDefaultScreens = TRUE;

  ErrorF ("winInitializeDefaultScreens - Returning\n");
}


/* See Porting Layer Definition - p. 57 */
/*
 * INPUT
 * argv: pointer to an array of null-terminated strings, one for
 *   each token in the X Server command line; the first token
 *   is 'XWin.exe', or similar.
 * argc: a count of the number of tokens stored in argv.
 * i: a zero-based index into argv indicating the current token being
 *   processed.
 *
 * OUTPUT
 * return: return the number of tokens processed correctly.
 *
 * NOTE
 * When looking for n tokens, check that i + n is less than argc.  Or,
 *   you may check if i is greater than or equal to argc, in which case
 *   you should display the UseMsg () and return 0.
 */

/* Check if enough arguments are given for the option */
#define CHECK_ARGS(count) if (i + count >= argc) { UseMsg (); return 0; }

/* Compare the current option with the string. */ 
#define IS_OPTION(name) (strcmp (argv[i], name) == 0)

int
ddxProcessArgument (int argc, char *argv[], int i)
{
  static Bool		s_fBeenHere = FALSE;

  /* Initialize once */
  if (!s_fBeenHere)
    {
#ifdef DDXOSVERRORF
      /*
       * This initialises our hook into VErrorF () for catching log messages
       * that are generated before OsInit () is called.
       */
      OsVendorVErrorFProc = OsVendorVErrorF;

      /* Open log file if not yet open */
      if (g_pfLog == NULL)
	g_pfLog = fopen (WIN_LOG_FNAME, "w");
#endif

      s_fBeenHere = TRUE;

      /*
       * Initialize default screen settings.  We have to do this before
       * OsVendorInit () gets called, otherwise we will overwrite
       * settings changed by parameters such as -fullscreen, etc.
       */
      ErrorF ("ddxProcessArgument - Initializing default screens\n");
      winInitializeDefaultScreens ();
    }

#if CYGDEBUG
  ErrorF ("ddxProcessArgument - arg: %s\n", argv[i]);
#endif
  
  /*
   * Look for the '-screen scr_num [width height]' argument
   */
  if (strcmp (argv[i], "-screen") == 0)
    {
      int		iArgsProcessed = 1;
      int		nScreenNum;
      int		iWidth, iHeight;

#if CYGDEBUG
      ErrorF ("ddxProcessArgument - screen - argc: %d i: %d\n",
	      argc, i);
#endif

      /* Display the usage message if the argument is malformed */
      if (i + 1 >= argc)
	{
	  return 0;
	}
      
      /* Grab screen number */
      nScreenNum = atoi (argv[i + 1]);

      /* Validate the specified screen number */
      if (nScreenNum < 0 || nScreenNum >= MAXSCREENS)
        {
          ErrorF ("ddxProcessArgument - screen - Invalid screen number %d\n",
		  nScreenNum);
          UseMsg ();
	  return 0;
        }

      /* Look for 'WxD' or 'W D' */
      if (i + 2 < argc
	  && 2 == sscanf (argv[i + 2], "%dx%d",
			  (int *) &iWidth,
			  (int *) &iHeight))
	{
	  ErrorF ("ddxProcessArgument - screen - Found ``WxD'' arg\n");
	  iArgsProcessed = 3;
	  g_ScreenInfo[nScreenNum].fUserGaveHeightAndWidth = TRUE;
	  g_ScreenInfo[nScreenNum].dwWidth = iWidth;
	  g_ScreenInfo[nScreenNum].dwHeight = iHeight;
	  g_ScreenInfo[nScreenNum].dwUserWidth = iWidth;
	  g_ScreenInfo[nScreenNum].dwUserHeight = iHeight;
	}
      else if (i + 3 < argc
	       && 1 == sscanf (argv[i + 2], "%d",
			       (int *) &iWidth)
	       && 1 == sscanf (argv[i + 3], "%d",
			       (int *) &iHeight))
	{
	  ErrorF ("ddxProcessArgument - screen - Found ``W D'' arg\n");
	  iArgsProcessed = 4;
	  g_ScreenInfo[nScreenNum].fUserGaveHeightAndWidth = TRUE;
	  g_ScreenInfo[nScreenNum].dwWidth = iWidth;
	  g_ScreenInfo[nScreenNum].dwHeight = iHeight;
	  g_ScreenInfo[nScreenNum].dwUserWidth = iWidth;
	  g_ScreenInfo[nScreenNum].dwUserHeight = iHeight;
	}
      else
	{
	  ErrorF ("ddxProcessArgument - screen - Did not find size arg. "
		  "dwWidth: %ld dwHeight: %ld\n",
		  g_ScreenInfo[nScreenNum].dwWidth,
		  g_ScreenInfo[nScreenNum].dwHeight);
	  iArgsProcessed = 2;
	  g_ScreenInfo[nScreenNum].fUserGaveHeightAndWidth = FALSE;
	}

      /* Calculate the screen width and height in millimeters */
      if (g_ScreenInfo[nScreenNum].fUserGaveHeightAndWidth)
	{
	  g_ScreenInfo[nScreenNum].dwWidth_mm
	    = (g_ScreenInfo[nScreenNum].dwWidth
	       / monitorResolution) * 25.4;
	  g_ScreenInfo[nScreenNum].dwHeight_mm
	    = (g_ScreenInfo[nScreenNum].dwHeight
	       / monitorResolution) * 25.4;
	}

      /* Flag that this screen was explicity specified by the user */
      g_ScreenInfo[nScreenNum].fExplicitScreen = TRUE;

      /*
       * Keep track of the last screen number seen, as parameters seen
       * before a screen number apply to all screens, whereas parameters
       * seen after a screen number apply to that screen number only.
       */
      g_iLastScreen = nScreenNum;

      /* Keep a count of the number of screens */
      ++g_iNumScreens;

      return iArgsProcessed;
    }

  /*
   * Look for the '-engine n' argument
   */
  if (strcmp (argv[i], "-engine") == 0)
    {
      DWORD		dwEngine = 0;
      CARD8		c8OnBits = 0;
      
      /* Display the usage message if the argument is malformed */
      if (++i >= argc)
	{
	  UseMsg ();
	  return 0;
	}

      /* Grab the argument */
      dwEngine = atoi (argv[i]);

      /* Count the one bits in the engine argument */
      c8OnBits = winCountBits (dwEngine);

      /* Argument should only have a single bit on */
      if (c8OnBits != 1)
	{
	  UseMsg ();
	  return 0;
	}

      /* Is this parameter attached to a screen or global? */
      if (-1 == g_iLastScreen)
	{
	  int		j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].dwEnginePreferred = dwEngine;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].dwEnginePreferred = dwEngine;
	}
      
      /* Indicate that we have processed the argument */
      return 2;
    }

  /*
   * Look for the '-fullscreen' argument
   */
  if (strcmp (argv[i], "-fullscreen") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].fFullScreen = TRUE;

	      /*
	       * No scrollbars in fullscreen mode. Later, we may want to have
	       * a fullscreen with a bigger virtual screen?
	       */
	      g_ScreenInfo[j].fScrollbars = FALSE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].fFullScreen = TRUE;

	  /*
	   * No scrollbars in fullscreen mode. Later, we may want to have
	   * a fullscreen with a bigger virtual screen?
	   */
	  g_ScreenInfo[g_iLastScreen].fScrollbars = FALSE;
	}

      /* Indicate that we have processed this argument */
      return 1;
    }

  /*
   * Look for the '-lesspointer' argument
   */
  if (strcmp (argv[i], "-lesspointer") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].fLessPointer = TRUE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
          g_ScreenInfo[g_iLastScreen].fLessPointer = TRUE;
	}

      /* Indicate that we have processed this argument */
      return 1;
    }

  /*
   * Look for the '-nodecoration' argument
   */
  if (strcmp (argv[i], "-nodecoration") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].fDecoration = FALSE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].fDecoration = FALSE;
	}

      /* Indicate that we have processed this argument */
      return 1;
    }

  /*
   * Look for the '-rootless' argument
   */
  if (strcmp (argv[i], "-rootless") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].fRootless = TRUE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].fRootless = TRUE;
	}

      /* Indicate that we have processed this argument */
      return 1;
    }

  /*
   * Look for the '-multiwindow' argument
   */
  if (strcmp (argv[i], "-multiwindow") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].fMultiWindow = TRUE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].fMultiWindow = TRUE;
	}

      /* Indicate that we have processed this argument */
      return 1;
    }

  /*
   * Look for the '-multiplemonitors' argument
   */
  if (strcmp (argv[i], "-multiplemonitors") == 0
      || strcmp (argv[i], "-multimonitors") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].fMultipleMonitors = TRUE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].fMultipleMonitors = TRUE;
	}

      /* Indicate that we have processed this argument */
      return 1;
    }

  /*
   * Look for the '-scrollbars' argument
   */
  if (strcmp (argv[i], "-scrollbars") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      /* No scrollbar in fullscreen mode */
	      if (!g_ScreenInfo[j].fFullScreen)
		g_ScreenInfo[j].fScrollbars = TRUE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  if (!g_ScreenInfo[g_iLastScreen].fFullScreen)
	    {
	      /* No scrollbar in fullscreen mode */
	      g_ScreenInfo[g_iLastScreen].fScrollbars = TRUE;
	    }
	}

      /* Indicate that we have processed this argument */
      return 1;
    }



  /*
   * Look for the '-clipboard' argument
   */
  if (strcmp (argv[i], "-clipboard") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].fClipboard = TRUE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].fClipboard = TRUE;
	}

      /* Indicate that we have processed this argument */
      return 1;
    }

  /*
   * Look for the '-ignoreinput' argument
   */
  if (strcmp (argv[i], "-ignoreinput") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].fIgnoreInput = TRUE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].fIgnoreInput = TRUE;
	}

      /* Indicate that we have processed this argument */
      return 1;
    }

  /*
   * Look for the '-emulate3buttons' argument
   */
  if (strcmp (argv[i], "-emulate3buttons") == 0)
    {
      int	iArgsProcessed = 1;
      int	iE3BTimeout = WIN_DEFAULT_E3B_TIME;

      /* Grab the optional timeout value */
      if (i + 1 < argc
	  && 1 == sscanf (argv[i + 1], "%d",
			  &iE3BTimeout))
        {
	  /* Indicate that we have processed the next argument */
	  iArgsProcessed++;
        }
      else
	{
	  /*
	   * sscanf () won't modify iE3BTimeout if it doesn't find
	   * the specified format; however, I want to be explicit
	   * about setting the default timeout in such cases to
	   * prevent some programs (me) from getting confused.
	   */
	  iE3BTimeout = WIN_DEFAULT_E3B_TIME;
	}

      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].iE3BTimeout = iE3BTimeout;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].iE3BTimeout = iE3BTimeout;
	}

      /* Indicate that we have processed this argument */
      return iArgsProcessed;
    }

  /*
   * Look for the '-depth n' argument
   */
  if (strcmp (argv[i], "-depth") == 0)
    {
      DWORD		dwBPP = 0;
      
      /* Display the usage message if the argument is malformed */
      if (++i >= argc)
	{
	  UseMsg ();
	  return 0;
	}

      /* Grab the argument */
      dwBPP = atoi (argv[i]);

      /* Is this parameter attached to a screen or global? */
      if (-1 == g_iLastScreen)
	{
	  int		j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].dwBPP = dwBPP;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].dwBPP = dwBPP;
	}
      
      /* Indicate that we have processed the argument */
      return 2;
    }

  /*
   * Look for the '-refresh n' argument
   */
  if (strcmp (argv[i], "-refresh") == 0)
    {
      DWORD		dwRefreshRate = 0;
      
      /* Display the usage message if the argument is malformed */
      if (++i >= argc)
	{
	  UseMsg ();
	  return 0;
	}

      /* Grab the argument */
      dwRefreshRate = atoi (argv[i]);

      /* Is this parameter attached to a screen or global? */
      if (-1 == g_iLastScreen)
	{
	  int		j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].dwRefreshRate = dwRefreshRate;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].dwRefreshRate = dwRefreshRate;
	}
      
      /* Indicate that we have processed the argument */
      return 2;
    }

  /*
   * Look for the '-clipupdates num_boxes' argument
   */
  if (strcmp (argv[i], "-clipupdates") == 0)
    {
      DWORD		dwNumBoxes = 0;
      
      /* Display the usage message if the argument is malformed */
      if (++i >= argc)
	{
	  UseMsg ();
	  return 0;
	}

      /* Grab the argument */
      dwNumBoxes = atoi (argv[i]);

      /* Is this parameter attached to a screen or global? */
      if (-1 == g_iLastScreen)
	{
	  int		j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].dwClipUpdatesNBoxes = dwNumBoxes;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].dwClipUpdatesNBoxes = dwNumBoxes;
	}
      
      /* Indicate that we have processed the argument */
      return 2;
    }

  /*
   * Look for the '-emulatepseudo' argument
   */
  if (strcmp (argv[i], "-emulatepseudo") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].fEmulatePseudo = TRUE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
          g_ScreenInfo[g_iLastScreen].fEmulatePseudo = TRUE;
	}

      /* Indicate that we have processed this argument */
      return 1;
    }

  /*
   * Look for the '-nowinkill' argument
   */
  if (strcmp (argv[i], "-nowinkill") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].fUseWinKillKey = FALSE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].fUseWinKillKey = FALSE;
	}

      /* Indicate that we have processed this argument */
      return 1;
    }

  /*
   * Look for the '-winkill' argument
   */
  if (strcmp (argv[i], "-winkill") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].fUseWinKillKey = TRUE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].fUseWinKillKey = TRUE;
	}

      /* Indicate that we have processed this argument */
      return 1;
    }

  /*
   * Look for the '-nounixkill' argument
   */
  if (strcmp (argv[i], "-nounixkill") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].fUseUnixKillKey = FALSE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].fUseUnixKillKey = FALSE;
	}

      /* Indicate that we have processed this argument */
      return 1;
    }

  /*
   * Look for the '-unixkill' argument
   */
  if (strcmp (argv[i], "-unixkill") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].fUseUnixKillKey = TRUE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].fUseUnixKillKey = TRUE;
	}

      /* Indicate that we have processed this argument */
      return 1;
    }

  /*
   * Look for the '-notrayicon' argument
   */
  if (strcmp (argv[i], "-notrayicon") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].fNoTrayIcon = TRUE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].fNoTrayIcon = TRUE;
	}

      /* Indicate that we have processed this argument */
      return 1;
    }

  /*
   * Look for the '-trayicon' argument
   */
  if (strcmp (argv[i], "-trayicon") == 0)
    {
      /* Is this parameter attached to a screen or is it global? */
      if (-1 == g_iLastScreen)
	{
	  int			j;

	  /* Parameter is for all screens */
	  for (j = 0; j < MAXSCREENS; j++)
	    {
	      g_ScreenInfo[j].fNoTrayIcon = FALSE;
	    }
	}
      else
	{
	  /* Parameter is for a single screen */
	  g_ScreenInfo[g_iLastScreen].fNoTrayIcon = FALSE;
	}

      /* Indicate that we have processed this argument */
      return 1;
    }

  /*
   * Look for the '-fp' argument
   */
  if (IS_OPTION ("-fp"))
    {
      CHECK_ARGS (1);
      g_cmdline.fontPath = argv[++i];
      return 0; /* Let DIX parse this again */
    }

  /*
   * Look for the '-co' argument
   */
  if (IS_OPTION ("-co"))
    {
      CHECK_ARGS (1);
      g_cmdline.rgbPath = argv[++i];
      return 0; /* Let DIX parse this again */
    }

  /*
   * Look for the '-query' argument
   */
  if (IS_OPTION ("-query"))
    {
      CHECK_ARGS (1);
      g_fXdmcpEnabled = TRUE;
      g_pszQueryHost = argv[++i];
      return 0; /* Let DIX parse this again */
    }

  /*
   * Look for the '-indirect' or '-broadcast' arguments
   */
  if (IS_OPTION ("-indirect")
      || IS_OPTION ("-broadcast"))
    {
      g_fXdmcpEnabled = TRUE;
      return 0; /* Let DIX parse this again */
    }

  /*
   * Look for the '-xf86config' argument
   */
  if (IS_OPTION ("-xf86config"))
    {
      CHECK_ARGS (1);
      g_cmdline.configFile = argv[++i];
      return 2;
    }

  /*
   * Look for the '-keyboard' argument
   */
  if (IS_OPTION ("-keyboard"))
    {
      CHECK_ARGS (1);
      g_cmdline.keyboard = argv[++i];
      return 2;
    }

  /*
   * Look for the '-nounicodeclipboard' argument
   */
  if (IS_OPTION ("-nounicodeclipboard"))
    {
      g_fUnicodeClipboard = FALSE;
      /* Indicate that we have processed the argument */
      return 1;
    }

#ifdef XKB
  /*
   * Look for the '-kb' argument
   */
  if (IS_OPTION ("-kb"))
    {
      g_cmdline.noXkbExtension = TRUE;  
      return 0; /* Let DIX parse this again */
    }
#endif

  return 0;
}
