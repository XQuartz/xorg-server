noinst_LTLIBRARIES = libhurd.la

libhurd_la_SOURCES = hurd_init.c hurd_mmap.c \
		hurd_mouse.c hurd_video.c hurd_io.c hurd_kbd.c \
		$(srcdir)/../shared/VTsw_noop.c \
		$(srcdir)/../shared/posix_tty.c \
		$(srcdir)/../shared/libc_wrapper.c \
		$(srcdir)/../shared/stdResource.c \
		$(srcdir)/../shared/stdPci.c \
		$(srcdir)/../shared/sigiostubs.c \
		$(srcdir)/../shared/pm_noop.c \
		$(srcdir)/../shared/kmod_noop.c \
		$(srcdir)/../shared/agp_noop.c \
		$(srcdir)/../shared/at_scancode.c

AM_CFLAGS = -DUSESTDRES -DHAVE_SYSV_IPC $(XORG_CFLAGS)

INCLUDES = $(XORG_INCS)
