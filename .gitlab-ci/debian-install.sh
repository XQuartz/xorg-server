#!/bin/bash

set -e
set -o xtrace

# Packages which are needed by this script, but not for the xserver build
EPHEMERAL="
	git
	libxkbcommon-dev
	x11-utils
	x11-xserver-utils
	xauth
	xvfb
	"

apt-get install -y \
	$EPHEMERAL \
	autoconf \
	automake \
	bison \
	build-essential \
	ca-certificates \
	ccache \
	flex \
	libaudit-dev \
	libbsd-dev \
	libdbus-1-dev \
	libdmx-dev \
	libdrm-dev \
	libegl1-mesa-dev \
	libepoxy-dev \
	libgbm-dev \
	libgcrypt-dev \
	libgl1-mesa-dev \
	libglx-mesa0 \
	libnvidia-egl-wayland-dev \
	libpciaccess-dev \
	libpixman-1-dev \
	libselinux1-dev \
	libsystemd-dev \
	libtool \
	libudev-dev \
	libunwind-dev \
	libwayland-dev \
	libx11-dev \
	libx11-xcb-dev \
	libxau-dev \
	libxaw7-dev \
	libxcb-glx0-dev \
	libxcb-icccm4-dev \
	libxcb-image0-dev \
	libxcb-keysyms1-dev \
	libxcb-randr0-dev \
	libxcb-render-util0-dev \
	libxcb-render0-dev \
	libxcb-shape0-dev \
	libxcb-shm0-dev \
	libxcb-util0-dev \
	libxcb-xf86dri0-dev \
	libxcb-xkb-dev \
	libxcb-xv0-dev \
	libxcb1-dev \
	libxdmcp-dev \
	libxext-dev \
	libxfixes-dev \
	libxfont-dev \
	libxi-dev \
	libxinerama-dev \
	libxkbfile-dev \
	libxmu-dev \
	libxmuu-dev \
	libxpm-dev \
	libxrender-dev \
	libxres-dev \
	libxshmfence-dev \
	libxt-dev \
	libxtst-dev \
	libxv-dev \
	mesa-common-dev \
	meson \
	nettle-dev \
	pkg-config \
	python3-mako \
	python3-numpy \
	python3-six \
	x11-xkb-utils \
	x11proto-dev \
	xfonts-utils \
	xkb-data \
	xtrans-dev \
	xutils-dev

cd /root

# Xwayland requires wayland-protocols >= 1.18, but Debian buster has 1.17 only
git clone https://gitlab.freedesktop.org/wayland/wayland-protocols.git --depth 1 --branch=1.18
cd wayland-protocols
./autogen.sh
make -j${FDO_CI_CONCURRENT:-4} install
cd ..
rm -rf wayland-protocols

git clone https://gitlab.freedesktop.org/mesa/piglit.git --depth 1

git clone https://gitlab.freedesktop.org/xorg/test/xts --depth 1
cd xts
./autogen.sh
xvfb-run make -j${FDO_CI_CONCURRENT:-4}
cd ..

git clone https://gitlab.freedesktop.org/xorg/test/rendercheck --depth 1
cd rendercheck
meson build
ninja -j${FDO_CI_CONCURRENT:-4} -C build install
cd ..

rm -rf piglit/.git xts/.git piglit/tests/spec/ rendercheck/

echo '[xts]' > piglit/piglit.conf
echo 'path=/root/xts' >> piglit/piglit.conf

find -name \*.a -o -name \*.o -o -name \*.c -o -name \*.h -o -name \*.la\* | xargs rm
strip xts/xts5/*/.libs/*

apt-get purge -y \
	$EPHEMERAL

apt-get autoremove -y --purge
