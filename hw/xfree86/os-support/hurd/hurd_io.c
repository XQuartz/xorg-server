/*
 * Copyright 1997,1998 by UCHIYAMA Yasushi
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of UCHIYAMA Yasushi not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  UCHIYAMA Yasushi makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * UCHIYAMA YASUSHI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL UCHIYAMA YASUSHI BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/hurd/hurd_io.c,v 1.8 2002/10/11 01:40:35 dawes Exp $ */

#define NEED_EVENTS
#ifdef HAVE_XORG_CONFIG_H
#include <xorg-config.h>
#endif

#include <X11/X.h>
#include <X11/Xproto.h>
#include "inputstr.h"
#include "scrnintstr.h"
#include "mipointer.h"

#include "compiler.h"

#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/file.h>
#include <assert.h>
#include <mach.h>
#include <sys/ioctl.h>

typedef unsigned short kev_type;		/* kd event type */
typedef unsigned char Scancode;

struct mouse_motion {		
    short mm_deltaX;		/* units? */
    short mm_deltaY;
};

typedef struct {
    kev_type type;			/* see below */
    struct timeval time;		/* timestamp */
    union {				/* value associated with event */
	boolean_t up;		/* MOUSE_LEFT .. MOUSE_RIGHT */
	Scancode sc;		/* KEYBD_EVENT */
	struct mouse_motion mmotion;	/* MOUSE_MOTION */
    } value;
} kd_event;

/* 
 * kd_event ID's.
 */
#define MOUSE_LEFT	1		/* mouse left button up/down */
#define MOUSE_MIDDLE	2
#define MOUSE_RIGHT	3
#define MOUSE_MOTION	4		/* mouse motion */
#define KEYBD_EVENT	5		/* key up/down */

/***********************************************************************
 * Keyboard
 **********************************************************************/
void 
xf86SoundKbdBell(int loudness,int pitch,int duration)
{
    return;
}

void 
xf86SetKbdLeds(int leds)
{
    return;
}

int 
xf86GetKbdLeds()
{
    return 0;
}

void 
xf86SetKbdRepeat(char rad)
{
    return;
}

void 
xf86KbdInit()
{
    return;
}
int
xf86KbdOn()
{
    int data = 1;
    if( ioctl( xf86Info.consoleFd, _IOW('k', 1, int),&data) < 0)
	FatalError("Cannot set event mode on keyboard (%s)\n",strerror(errno));
    return xf86Info.consoleFd;
}
int
xf86KbdOff()
{
    int data = 2;
    if( ioctl( xf86Info.consoleFd, _IOW('k', 1, int),&data) < 0)
	FatalError("can't reset keyboard mode (%s)\n",strerror(errno));
}

void
xf86KbdEvents()
{
    kd_event ke;
    while( read(xf86Info.consoleFd, &ke, sizeof(ke)) == sizeof(ke) )
	xf86PostKbdEvent(ke.value.sc);
}
