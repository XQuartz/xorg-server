#! /bin/sh

export PKGCONFIG_PATH=/usr/X11/lib/pkgconfig:$PKGCONFIG_PATH
export ACLOCAL="aclocal -I /usr/X11/share/aclocal"

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir

autoreconf -v --install || exit 1
cd $ORIGDIR || exit $?

$srcdir/configure --prefix=/usr/X11 --enable-maintainer-mode "$@"
