INCLUDES = 					\
	@KDRIVE_INCS@				\
	$(XSERVER_CFLAGS)


noinst_LIBRARIES = liblinux.a

liblinux_a_SOURCES = 	\
	agp.c		\
	bus.c		\
	keyboard.c	\
	linux.c		\
	mouse.c		\
	ms.c		\
	ps2.c

if TSLIB
liblinux_a_SOURCES += tslib.c
endif

if H3600_TS
liblinux_a_SOURCES += ts.c
endif
