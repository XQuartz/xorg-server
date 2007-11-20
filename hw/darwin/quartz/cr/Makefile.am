noinst_LIBRARIES = libcr.a
AM_CFLAGS = $(XORG_CFLAGS)
AM_OBJCFLAGS = $(XORG_CFLAGS)
AM_CPPFLAGS = -DDEFER_NSWINDOW \
	   $(XORG_INCS) \
	   -I../fullscreen \
	   -I$(srcdir) -I$(srcdir)/.. -I$(srcdir)/../.. \
	   -I$(top_srcdir)/mi \
	   -I$(top_srcdir)/miext/rootless \
	   -I$(top_srcdir)/miext/rootless/safeAlpha \
	   -I$(top_srcdir)/render \
	   $(PIXMAN_CFLAGS)

libcr_a_SOURCES = crAppleWM.m \
	           crFrame.m \
	           crScreen.m \
	           ../fullscreen/quartzCursor.c \
	           XView.m

EXTRA_DIST = \
	cr.h \
	XView.h 

