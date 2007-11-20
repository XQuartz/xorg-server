noinst_LIBRARIES = libxpr.a
AM_CFLAGS =  $(XORG_CFLAGS) $(PIXMAN_CFLAGS)
AM_CPPFLAGS = $(XORG_INCS) \
	   -DHAVE_XORG_CONFIG_H \
	   -I$(srcdir) -I$(srcdir)/.. -I$(srcdir)/../.. \
	   -I$(top_srcdir)/fb \
	   -I$(top_srcdir)/mi \
	   -I$(top_srcdir)/miext \
	   -I$(top_srcdir)/miext/damage \
	   -I$(top_srcdir)/miext/rootless \
	   -I$(top_srcdir)/miext/rootless/safeAlpha \
	   -I$(top_srcdir)/render

libxpr_a_SOURCES = \
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
	dri.h \
	dristruct.h \
	appledri.h \
	appledristr.h \
	x-hash.h \
	x-hook.h \
	x-list.h \
	Xplugin.h \
	xpr.h
	
