noinst_LIBRARIES = libdarwinShared.a
libdarwin_XINPUT_SRCS = darwinXinput.c

# -O2 inexplicably causes quartzKeyboard.o to break?!
AM_CFLAGS = @XORG_CFLAGS@
INCLUDES = @XORG_INCS@ -I../../miext/rootless

DEFS = @DEFS@ -DUSE_NEW_CLUT

if XQUARTZ
XQUARTZ_SUBDIRS = quartz
endif

SUBDIRS = \
	  iokit \
	  $(XQUARTZ_SUBDIRS) \
	  utils \
	  .

libdarwinShared_a_SOURCES = darwin.c \
			  darwinEvents.c \
			  darwinKeyboard.c \
			  $(darwin_XINPUT_SRCS) 

if XQUARTZ
bin_PROGRAMS = Xquartz
else
bin_PROGRAMS = XDarwin
endif

XDarwin_SOURCES = \
                  $(top_srcdir)/fb/fbcmap.c \
                  $(top_srcdir)/mi/miinitext.c \
                  $(top_srcdir)/Xi/stubs.c

Xquartz_SOURCES = \
                  $(top_srcdir)/fb/fbcmap.c \
                  $(top_srcdir)/mi/miinitext.c \
                  $(top_srcdir)/Xi/stubs.c \
                  apple/X11Application.m \
                  apple/X11Controller.m \
                  quartz/applewm.c \
                  quartz/keysym2ucs.c \
                  quartz/pseudoramiX.c \
                  quartz/quartz.c \
                  quartz/quartzAudio.c \
                  quartz/quartzCocoa.m \
                  quartz/quartzKeyboard.c \
                  quartz/quartzPasteboard.c \
                  quartz/quartzStartup.c \
                  quartz/xpr/appledri.c \
                  quartz/xpr/dri.c \
                  quartz/xpr/xprAppleWM.c \
                  quartz/xpr/xprCursor.c \
                  quartz/xpr/xprFrame.c \
                  quartz/xpr/xprScreen.c \
                  quartz/xpr/x-hash.c \
                  quartz/xpr/x-hook.c \
                  quartz/xpr/x-list.c

XDarwin_LDADD = \
		$(top_builddir)/dix/dixfonts.lo \
		$(top_builddir)/dix/libdix.la \
		$(top_builddir)/os/libos.la \
		./libdarwinShared.a \
		./iokit/libiokit.a \
		$(top_builddir)/dix/libxpstubs.la \
		$(top_builddir)/miext/shadow/libshadow.la \
		$(top_builddir)/fb/libfb.la \
		$(top_builddir)/composite/libcomposite.la \
		$(top_builddir)/damageext/libdamageext.la \
		$(top_builddir)/miext/damage/libdamage.la \
		$(top_builddir)/xfixes/libxfixes.la \
		$(top_builddir)/miext/cw/libcw.la \
		$(top_builddir)/Xext/libXext.la \
		$(top_builddir)/xkb/libxkb.la \
		$(top_builddir)/xkb/libxkbstubs.la \
		$(top_builddir)/Xi/libXi.la \
		$(top_builddir)/dbe/libdbe.la \
		$(top_builddir)/record/librecord.la \
		$(top_builddir)/XTrap/libxtrap.la \
		$(XGLX_LIBS) \
		$(top_builddir)/miext/rootless/librootless.la \
		$(top_builddir)/miext/rootless/safeAlpha/libsafeAlpha.la \
		$(top_builddir)/miext/rootless/accel/librlAccel.la \
		$(XSERVER_LIBS) -lXplugin

Xquartz_LDADD = \
		$(top_builddir)/dix/dixfonts.lo \
		$(top_builddir)/dix/libdix.la \
		$(top_builddir)/os/libos.la \
		./libdarwinShared.a \
		$(top_builddir)/dix/libxpstubs.la \
		$(top_builddir)/miext/shadow/libshadow.la \
		$(top_builddir)/fb/libfb.la \
		$(top_builddir)/mi/libmi.la \
		$(top_builddir)/composite/libcomposite.la \
		$(top_builddir)/damageext/libdamageext.la \
		$(top_builddir)/miext/damage/libdamage.la \
		$(top_builddir)/xfixes/libxfixes.la \
		$(top_builddir)/miext/cw/libcw.la \
		$(top_builddir)/Xext/libXext.la \
		$(top_builddir)/xkb/libxkb.la \
		$(top_builddir)/xkb/libxkbstubs.la \
		$(top_builddir)/Xi/libXi.la \
		$(top_builddir)/dbe/libdbe.la \
		$(top_builddir)/record/librecord.la \
		$(top_builddir)/XTrap/libxtrap.la \
		$(top_builddir)/miext/rootless/librootless.la \
		$(top_builddir)/miext/rootless/safeAlpha/libsafeAlpha.la \
		$(top_builddir)/miext/rootless/accel/librlAccel.la \
		$(DARWIN_LIBS) $(XSERVER_LIBS) -lXplugin

XDarwin_LDFLAGS =  \
                 -XCClinker -Objc \
                 -Wl,-u,_miDCInitialize \
                 -Wl,-framework,IOKit

Xquartz_LDFLAGS =  \
                 -XCClinker -Objc \
                 -Wl,-u,_miDCInitialize \
                 -Wl,-framework,Carbon \
	         -L/System/Library/Frameworks/OpenGL.framework/Libraries -lGL \
                 -Wl,-framework,OpenGL \
                 -Wl,-framework,Cocoa \
                 -Wl,-framework,CoreAudio \
                 -Wl,-framework,IOKit

XDarwin_CFLAGS = -DINXDARWIN
Xquartz_CFLAGS = -DINXQUARTZ -DHAS_CG_MACH_PORT -DHAS_KL_API  -DHAVE_XORG_CONFIG_H
Xquartz_OBJCFLAGS = -DINXQUARTZ -DHAS_CG_MACH_PORT -DHAS_KL_API  -DHAVE_XORG_CONFIG_H

if X11APP
bin_SCRIPTS = x11app x11launcher

x11app:
	cd apple && xcodebuild CFLAGS="$(XSERVERCFLAGS_CFLAGS)" LDFLAGS="$(XSERVERCFLAGS_LIBS)"

x11launcher:
	cd launcher && xcodebuild CFLAGS="$(XSERVERCFLAGS_CFLAGS)" LDFLAGS="$(XSERVERCFLAGS_LIBS)"

x11app-install-hook:
	cd apple && xcodebuild install DSTROOT=$(DESTDIR) INSTALL_PATH=$(prefixdir) DEPLOYMENT_LOCATION=YES SKIP_INSTALL=NO
	cd launcher && xcodebuild install DSTROOT=$(DESTDIR) INSTALL_PATH=$(APPLE_APPLICATIONS_DIR) DEPLOYMENT_LOCATION=YES SKIP_INSTALL=NO

HOOK_TARGETS = x11app-install-hook

endif

man1_MANS = XDarwin.man

install-data-hook: $(HOOK_TARGETS)

EXTRA_DIST = \
	darwin.c \
	darwinClut8.h \
	darwinEvents.c \
	darwin.h \
	darwinKeyboard.c \
	darwinKeyboard.h \
	darwinXinput.c \
	apple/X11Application.h \
	apple/X11Controller.h \
	apple/Info.plist \
	apple/X11.icns \
	apple/Xquartz.man \
	apple/bundle-main.c \
	apple/English.lproj/InfoPlist.strings \
	apple/English.lproj/Localizable.strings \
	apple/English.lproj/main.nib/classes.nib \
	apple/English.lproj/main.nib/info.nib \
	apple/English.lproj/main.nib/keyedobjects.nib \
	apple/X11.xcodeproj/project.pbxproj \
	XDarwin.man
