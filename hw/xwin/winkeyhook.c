/*
 *Copyright (C) 2004 Harold L Hunt II All Rights Reserved.
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
 * References to external symbols
 */

extern HHOOK			g_hhookKeyboardLL;
extern DWORD			g_dwCurrentThreadID;


/*
 * Function prototypes
 */

static LRESULT CALLBACK
winKeyboardMessageHookLL (int iCode, WPARAM wParam, LPARAM lParam);


/*
 * KeyboardMessageHook
 */

static LRESULT CALLBACK
winKeyboardMessageHookLL (int iCode, WPARAM wParam, LPARAM lParam)
{
  BOOL			fEatKeystroke = FALSE;
  PKBDLLHOOKSTRUCT	p = (PKBDLLHOOKSTRUCT) lParam;

  /* Swallow keystrokes only for our app */
  if (iCode == HC_ACTION)
    {
      switch (wParam)
	{
	case WM_KEYDOWN:  case WM_SYSKEYDOWN:
	case WM_KEYUP:    case WM_SYSKEYUP: 
	  p = (PKBDLLHOOKSTRUCT) lParam;

	  fEatKeystroke = 
	    (p->vkCode == VK_TAB) && ((p->flags & LLKHF_ALTDOWN) != 0);
	  break;
	}
    }
  
  return (fEatKeystroke ? 1 : CallNextHookEx (NULL, iCode, wParam, 
					      lParam));
}


/*
 * Attempt to install the keyboard hook, return FALSE if it was not installed
 */

Bool
winInstallKeyboardHookLL ()
{
  OSVERSIONINFO		osvi = {0};
  
  /* Get operating system version information */
  osvi.dwOSVersionInfoSize = sizeof (osvi);
  GetVersionEx (&osvi);

  /* Branch on platform ID */
  switch (osvi.dwPlatformId)
    {
    case VER_PLATFORM_WIN32_NT:
      /* Low-level is supported on NT 4.0 SP3+ only */
      /* TODO: Return FALSE on NT 4.0 with no SP, SP1, or SP2 */
      break;

    case VER_PLATFORM_WIN32_WINDOWS:
      /* Low-level hook is not supported on non-NT */
      return FALSE;
    }

  /* Install the hook only once */
  if (!g_hhookKeyboardLL)
    g_hhookKeyboardLL = SetWindowsHookEx (WH_KEYBOARD_LL,
					  winKeyboardMessageHookLL,
					  g_hInstance,
					  0);

  return TRUE;
}


/*
 * Remove the keyboard hook if it is installed
 */

void
winRemoveKeyboardHookLL ()
{
  if (g_hhookKeyboardLL)
    UnhookWindowsHookEx (g_hhookKeyboardLL);
  g_hhookKeyboardLL = NULL;
}
