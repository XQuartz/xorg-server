#!/bin/bash

set -e
set -o xtrace

# Packages which are needed by this script, but not for the xserver build
EPHEMERAL="
	x11-utils
	x11-xserver-utils
	xauth
	xvfb
	"

apt update

apt-get install -y \
	$EPHEMERAL \
	autoconf \
	automake \
	bison \
	build-essential \
	ca-certificates \
	ccache \
	dpkg-dev \
	flex \
	gcc-mingw-w64-i686 \
	git \
	libaudit-dev \
	libbsd-dev \
	libcairo2 \
	libcairo2-dev \
	libdbus-1-dev \
	libdecor-0-dev \
	libdrm-dev \
	libegl1-mesa-dev \
	libepoxy-dev \
	libevdev2 \
	libexpat1 \
	libffi-dev \
	libgbm-dev \
	libgcrypt-dev \
	libgl1-mesa-dev \
	libgles2 \
	libglx-mesa0 \
	libinput10 \
	libinput-dev \
	libnvidia-egl-wayland-dev \
	libpango1.0-0 \
	libpango1.0-dev \
	libpixman-1-dev \
	libselinux1-dev \
	libspice-protocol-dev \
	libsystemd-dev \
	libtool \
	libudev-dev \
	libunwind-dev \
	libwayland-dev \
	libx11-dev \
	libx11-xcb-dev \
	libxau-dev \
	libxaw7-dev \
	libxcb-damage0-dev \
	libxcb-dri2-0-dev \
	libxcb-dri3-dev \
	libxcb-glx0-dev \
	libxcb-icccm4-dev \
	libxcb-image0-dev \
	libxcb-keysyms1-dev \
	libxcb-randr0-dev \
	libxcb-render-util0-dev \
	libxcb-render0-dev \
	libxcb-shape0-dev \
	libxcb-shm0-dev \
	libxcb-sync-dev \
	libxcb-util0-dev \
	libxcb-xf86dri0-dev \
	libxcb-xinput-dev \
	libxcb-xkb-dev \
	libxcb-xv0-dev \
	libxcb1-dev \
	libxcursor-dev \
	libxcvt-dev \
	libxdamage-dev \
	libxdmcp-dev \
	libxext-dev \
	libxfixes-dev \
	libxfont-dev \
	libxi-dev \
	libxinerama-dev \
	libxkbcommon0 \
	libxkbfile-dev \
	libxmu-dev \
	libxmuu-dev \
	libxpm-dev \
	libxrandr-dev \
	libxrender-dev \
	libxres-dev \
	libxshmfence-dev \
	libxss-dev \
	libxt-dev \
	libxtst-dev \
	libxv-dev \
	libxvmc-dev \
	libxxf86vm-dev \
	libz-mingw-w64-dev \
	linux-libc-dev \
	mesa-common-dev \
	meson \
	mingw-w64-tools \
	nettle-dev \
	pkg-config \
	python3-attr \
	python3-jinja2 \
	python3-pytest \
	python3-mako \
	python3-numpy \
	python3-six \
	valgrind \
	weston \
	x11-xkb-utils \
	xfonts-utils \
	xkb-data \
	xtrans-dev \
	xutils-dev

.gitlab-ci/cross-prereqs-build.sh i686-w64-mingw32

cd /root

# Xwayland requires drm 2.4.116 for drmSyncobjEventfd
# but Debian bookworm has only 2.4.114
git clone https://gitlab.freedesktop.org/mesa/drm --depth 1 --branch=libdrm-2.4.116
cd drm
meson _build
ninja -C _build -j${FDO_CI_CONCURRENT:-4} install
cd ..
rm -rf drm

# xserver requires xorgproto >= 2024.1 for XWAYLAND
# but Debian bookworm has only 2022.1
git clone https://gitlab.freedesktop.org/xorg/proto/xorgproto.git --depth 1 --branch=xorgproto-2024.1
pushd xorgproto
./autogen.sh
make -j${FDO_CI_CONCURRENT:-4} install
popd
rm -rf xorgproto

# xserver requires xtrans >= 1.5.1 to build with gcc 12
# but Debian bookworm has only 1.4.0
git clone https://gitlab.freedesktop.org/xorg/lib/libxtrans.git --depth 1 --branch=xtrans-1.6.0
pushd libxtrans
./autogen.sh
make -j${FDO_CI_CONCURRENT:-4} install
popd
rm -rf libxtrans

# Xorg prefers libpciaccess >= 0.19
# but Debian bookworm has only 0.17
git clone https://gitlab.freedesktop.org/xorg/lib/libpciaccess.git --depth 1 --branch=libpciaccess-0.19
pushd libpciaccess
meson setup _build
meson compile -C _build
meson install -C _build
popd
rm -rf libpciaccess

# wayland-protocols 1.38 requires either wayland-scanner 1.23 or a build with
# dtd_validation=false, but Debian bookworm has only 1.21 w/ dtd_validation=true
git clone https://gitlab.freedesktop.org/wayland/wayland.git --depth 1 --branch=1.26.0
cd wayland
meson -Dtests=false -Ddocumentation=false -Ddtd_validation=false _build
ninja -C _build -j${FDO_CI_CONCURRENT:-4} install
cd ..
rm -rf wayland

# Xwayland requires wayland-protocols >= 1.38, but Debian bookworm has 1.31 only
git clone https://gitlab.freedesktop.org/wayland/wayland-protocols.git --depth 1 --branch=1.38
cd wayland-protocols
meson _build
ninja -C _build -j${FDO_CI_CONCURRENT:-4} install
cd ..
rm -rf wayland-protocols

# Install libei for Xwayland, as Debian didn't add until trixie
git clone https://gitlab.freedesktop.org/libinput/libei.git --depth 1 --branch=1.0.0
cd libei
meson setup _build -Dtests=disabled -Ddocumentation=[] -Dliboeffis=enabled
ninja -C _build -j${FDO_CI_CONCURRENT:-4} install
cd ..
rm -rf libei

git clone https://gitlab.freedesktop.org/mesa/piglit.git
cd piglit
git checkout 265896c86f90cb72e8f218ba6a3617fca8b9a1e3
cd ..

git clone https://gitlab.freedesktop.org/xorg/test/xts
cd xts
git checkout 12a887c2c72c4258962b56ced7b0aec782f1ffed
./autogen.sh
xvfb-run make -j${FDO_CI_CONCURRENT:-4}
cd ..

git clone https://gitlab.freedesktop.org/xorg/test/rendercheck
cd rendercheck
git checkout 67a820621b1475ebfcf3d4f9d7f03a5fc3b9769a
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
