SUBDIRS = man

bin_PROGRAMS = Xvfb

AM_CFLAGS = -DHAVE_DIX_CONFIG_H \
            $(XVFBMODULES_CFLAGS) \
            -I$(top_srcdir)/glamor/ \
            -I$(top_srcdir)/dri3/ \
            $(LIBDRM_CFLAGS) \
            $(GBM_CFLAGS) \
	    $(DIX_CFLAGS)

SRCS =	InitInput.c \
	InitOutput.c \
	$(top_srcdir)/mi/miinitext.c

Xvfb_SOURCES = $(SRCS)

XVFB_LIBS = \
	$(glamor_lib) \
        @XVFB_LIBS@ \
	$(MAIN_LIB) \
	$(XSERVER_LIBS) \
	$(top_builddir)/Xi/libXistubs.la

Xvfb_LDADD = $(XVFB_LIBS) $(XVFB_SYS_LIBS) $(XSERVER_SYS_LIBS)
Xvfb_DEPENDENCIES = $(XVFB_LIBS)
Xvfb_LDFLAGS = $(LD_EXPORT_SYMBOLS_FLAG)

if GLAMOR_EGL
Xvfb_SOURCES += $(top_srcdir)/glamor/glamor_egl.c
glamor_lib = $(top_builddir)/glamor/libglamor.la

Xvfb_LDADD += $(GLAMOR_LIBS) $(GBM_LIBS) $(LIBDRM_LIBS) -lEGL -lGL
Xvfb_DEPENDENCIES = $(glamor_lib) $(XVFB_LIBS)
endif

relink:
	$(AM_V_at)rm -f Xvfb$(EXEEXT) && $(MAKE) Xvfb$(EXEEXT)
