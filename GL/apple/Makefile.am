AM_CFLAGS = -I$(top_srcdir) \
	 -I$(top_srcdir)/GL/glx \
	 -I$(top_srcdir)/GL/include \
	 -I$(top_srcdir)/GL/mesa/glapi \
	 -I$(top_srcdir)/hw/darwin/quartz \
	 -I$(top_srcdir)/hw/darwin/quartz/xpr

noinst_LIBRARIES = libCGLcore.a 
libCGLcore_a_SOURCES = indirect.c \
		       $(top_srcdir)/hw/dmx/glxProxy/compsize.c
