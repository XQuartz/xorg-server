noinst_LIBRARIES = libcr.a
AM_CFLAGS = $(XORG_CFLAGS)
AM_OBJCFLAGS = $(XORG_CFLAGS)
AM_CPPFLAGS = $(XORG_INCS) \
	-DDEFER_NSWINDOW \
	-I../fullscreen \
	-I$(srcdir) -I$(srcdir)/.. -I$(srcdir)/../.. \
	-I$(top_srcdir)/mi \
	-I$(top_srcdir)/miext/rootless \
	-I$(top_srcdir)/miext/rootless/safeAlpha \
	-I$(top_srcdir)/render

libcr_a_SOURCES = \
	crAppleWM.m \
	crFrame.m \
	crScreen.m \
	../fullscreen/quartzCursor.c \
	XView.m

EXTRA_DIST = \
	cr.h \
	XView.h 
