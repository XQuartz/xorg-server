#ifndef _WINCLIPBOARD_H_
#define _WINCLIPBOARD_H_
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
 * Authors:	Harold Hunt
 */
/* $XFree86: xc/programs/Xserver/hw/xwin/winclipboard.h,v 1.3 2003/10/02 13:30:10 eich Exp $ */

/* Standard library headers */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>
#include <setjmp.h>
#include <pthread.h>

/* X headers */
#include "X11/X.h"
#include "X11/Xatom.h"
/* NOTE: For some unknown reason, including Xproto.h solves
 * tons of problems with including windows.h.  Unknowns reasons
 * are usually bad, so someone should investigate this.
 */
#include "X11/Xproto.h"
#include "X11/Xutil.h"
#include "X11/Xlocale.h"

/* Fixups to prevent collisions between Windows and X headers */
#define ATOM			DWORD

/* Windows headers */
#ifndef XFree86Server
#define XFree86Server
#endif
#include <windows.h>
#undef XFree86Server


/* Clipboard module constants */
#define WIN_CLIPBOARD_WINDOW_CLASS		"xwinclip"
#define WIN_CLIPBOARD_WINDOW_TITLE		"xwinclip"
#define WIN_MSG_QUEUE_FNAME			"/dev/windows"
#define WIN_CONNECT_RETRIES			40
#define WIN_CONNECT_DELAY			4
#define WIN_JMP_OKAY				0
#define WIN_JMP_ERROR_IO			2
#define WIN_LOCAL_PROPERTY			"CYGX_CUT_BUFFER"
#define WIN_XEVENTS_SUCCESS			0
#define WIN_XEVENTS_SHUTDOWN			1
#define WIN_XEVENTS_CONVERT			2


/*
 * References to external symbols
 */

extern char *display;
extern void ErrorF (const char* /*f*/, ...);


/*
 * winclipboardinit.c
 */

Bool
winInitClipboard (void);

HWND
winClipboardCreateMessagingWindow (void);


/*
 * winclipboardtextconv.c
 */

void
winClipboardDOStoUNIX (char *pszData, int iLength);

void
winClipboardUNIXtoDOS (unsigned char **ppszData, int iLength);


/*
 * winclipboardthread.c
 */

void *
winClipboardProc (void);

void
winDeinitClipboard (void);


/*
 * winclipboardunicode.c
 */

Bool
winClipboardDetectUnicodeSupport (void);


/*
 * winclipboardwndproc.c
 */

BOOL
winClipboardFlushWindowsMessageQueue (HWND hwnd);

LRESULT CALLBACK
winClipboardWindowProc (HWND hwnd, UINT message, 
			WPARAM wParam, LPARAM lParam);


/*
 * winclipboardxevents.c
 */

int
winClipboardFlushXEvents (HWND hwnd,
			  int iWindow,
			  Display *pDisplay,
			  Bool fUnicodeSupport);
#endif
