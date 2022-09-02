#!/bin/bash

set -e
set -o xtrace

meson -Dc_args="-fno-common" -Dprefix=/usr -Dxephyr=true -Dwerror=true $MESON_EXTRA_OPTIONS build/

ninja -j${FDO_CI_CONCURRENT:-4} -C build/ dist

export PIGLIT_DIR=/root/piglit XTEST_DIR=/root/xts
ninja -j${FDO_CI_CONCURRENT:-4} -C build/ test
