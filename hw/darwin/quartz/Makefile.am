noinst_LIBRARIES = libXQuartz.a

AM_CFLAGS = @XORG_CFLAGS@
AM_OBJCFLAGS = @XORG_CFLAGS@
AM_CPPFLAGS = -I$(srcdir) -I$(srcdir)/.. @XORG_INCS@ -DXBINDIR=\"${bindir}\" -DINXQUARTZ -DHAS_KL_API -DHAVE_XORG_CONFIG_H

SUBDIRS = cr fullscreen xpr

libXQuartz_a_SOURCES = \
		  ../apple/X11Application.m \
		  ../apple/X11Controller.m \
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
	applewmExt.h \
	keysym2ucs.h \
	pseudoramiX.h \
	quartzAudio.h \
	quartzCommon.h \
	quartzCursor.c \
	quartzCursor.h \
	quartz.h \
	quartzPasteboard.h
