INCLUDES = 					\
	@KDRIVE_INCS@				\
	$(XSERVER_CFLAGS)

noinst_LIBRARIES = libfbdev.a

bin_PROGRAMS = Xfbdev

libfbdev_a_SOURCES = \
	fbdev.c

Xfbdev_SOURCES = \
	fbinit.c

Xfbdev_LDADD = 						\
	libfbdev.a					\
	$(KDRIVE_LIBS)					\
	$(XSERVER_LIBS)

Xfbdev_DEPENDENCIES =					\
	libfbdev.a					\
	$(KDRIVE_LIBS)

if TSLIB
Xfbdev_LDADD += -lts
endif

