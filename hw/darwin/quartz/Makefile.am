noinst_LIBRARIES = libXQuartz.a

AM_CFLAGS = $(XSERVER_CFLAGS) $(DIX_CFLAGS)
AM_OBJCFLAGS = $(XSERVER_CFLAGS) $(DIX_CFLAGS)
AM_CPPFLAGS = \
	-I$(srcdir) -I$(srcdir)/.. \
	-I$(top_srcdir)/miext/rootless \
	-I$(top_srcdir)/miext/rootless/safeAlpha

libXQuartz_a_SOURCES = \
	X11Application.m \
	X11Controller.m \
	applewm.c \
	keysym2ucs.c \
	pseudoramiX.c \
	quartz.c \
	quartzAudio.c \
	quartzCocoa.m \
	quartzKeyboard.c \
	quartzPasteboard.c \
	quartzStartup.c \
	appledri.c \
	dri.c \
	xprAppleWM.c \
	xprCursor.c \
	xprFrame.c \
	xprScreen.c \
	x-hash.c \
	x-hook.c \
	x-list.c

EXTRA_DIST = \
	X11Application.h \
	X11Controller.h \
	applewmExt.h \
	keysym2ucs.h \
	pseudoramiX.h \
	quartzAudio.h \
	quartzCommon.h \
	quartzCursor.c \
	quartzCursor.h \
	quartz.h \
	quartzPasteboard.h \
	dri.h \
	dristruct.h \
	appledri.h \
	appledristr.h \
	x-hash.h \
	x-hook.h \
	x-list.h \
	Xplugin.h \
	xpr.h
