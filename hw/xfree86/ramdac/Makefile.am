noinst_LTLIBRARIES = libramdac.la

libramdac_la_SOURCES = xf86RamDac.c xf86RamDacCmap.c \
                      xf86CursorRD.c xf86HWCurs.c

sdk_HEADERS = xf86Cursor.h xf86RamDac.h

EXTRA_DIST = xf86CursorPriv.h xf86RamDacPriv.h CURSOR.NOTES

AM_CFLAGS = $(DIX_CFLAGS) $(XORG_CFLAGS)
AM_CPPFLAGS = $(XORG_INCS)
