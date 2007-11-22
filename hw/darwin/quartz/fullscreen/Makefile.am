noinst_LIBRARIES = libfullscreen.a
AM_CFLAGS = $(XORG_CFLAGS)
AM_CPPFLAGS = $(XORG_INCS) \
	-I$(srcdir) -I$(srcdir)/.. -I$(srcdir)/../.. \
	-I$(top_srcdir)/miext/damage \
	-I$(top_srcdir)/miext/shadow \
	-I$(top_srcdir)/mi

libfullscreen_a_SOURCES = \
	fullscreen.c \
	quartzCursor.c

EXTRA_DIST = \
	quartzCursor.h

