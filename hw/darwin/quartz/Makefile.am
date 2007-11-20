noinst_LIBRARIES = libXQuartz.a

AM_CFLAGS = $(XORG_CFLAGS) $(PIXMAN_CFLAGS)
AM_OBJCFLAGS = $(XORG_CFLAGS) $(PIXMAN_CFLAGS)
AM_CPPFLAGS = $(XORG_INCS) -DXBINDIR=\"${bindir}\" -DHAS_KL_API -DHAVE_XORG_CONFIG_H \
	-I$(srcdir) -I$(srcdir)/.. \
	-I$(top_srcdir)/fb \
	-I$(top_srcdir)/mi \
	-I$(top_srcdir)/miext/rootless \
	-I$(top_srcdir)/render

SUBDIRS = cr fullscreen xpr

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
	          quartzStartup.c

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
	quartzPasteboard.h
