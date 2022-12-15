#!/bin/bash

set "$(dirname "$0")"/X11.bin "${@}"

if [ ! -f "${HOME}/Library/Preferences/org.xquartz.X11.plist" ] ; then
    # Try migrating preferences
    if [ -f "${HOME}/Library/Preferences/org.macosforge.xquartz.X11.plist" ] ; then
        cp "${HOME}/Library/Preferences/org.macosforge.xquartz.X11.plist" "${HOME}/Library/Preferences/org.xquartz.X11.plist"
    elif [ -f "${HOME}/Library/Preferences/org.x.X11.plist" ] ; then
        cp "${HOME}/Library/Preferences/org.x.X11.plist" "${HOME}/Library/Preferences/org.xquartz.X11.plist"
    elif [ -f "${HOME}/Library/Preferences/com.apple.X11.plist" ] ; then
        cp "${HOME}/Library/Preferences/com.apple.X11.plist" "${HOME}/Library/Preferences/org.xquartz.X11.plist"
    fi
fi

if [ -x ~/.x11run ]; then
	exec ~/.x11run "${@}"
fi

export DYLD_LIBRARY_PATH=/tmp/Xplugin.dst/usr/lib

case $(basename "${SHELL}") in
	bash)          exec -l "${SHELL}" --login -c 'exec "${@}"' - "${@}" ;;
	ksh|sh|zsh)    exec -l "${SHELL}" -c 'exec "${@}"' - "${@}" ;;
	csh|tcsh)      exec -l "${SHELL}" -c 'exec $argv:q' "${@}" ;;
	es|rc)         exec -l "${SHELL}" -l -c 'exec $*' "${@}" ;;
	*)             exec    "${@}" ;;
esac
