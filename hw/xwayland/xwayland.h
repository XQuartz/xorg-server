/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the
 * copyright holders not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifndef XWAYLAND_H
#define XWAYLAND_H

#include <xwayland-config.h>

#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <wayland-client.h>

#include <X11/X.h>

#include <fb.h>
#include <dix.h>
#include <randrstr.h>
#include <exevents.h>

#include "relative-pointer-unstable-v1-client-protocol.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "tablet-unstable-v2-client-protocol.h"
#include "xwayland-keyboard-grab-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "viewporter-client-protocol.h"

#include "xwayland-types.h"
#include "xwayland-output.h"
#include "xwayland-glamor.h"

struct xwl_format {
    uint32_t format;
    int num_modifiers;
    uint64_t *modifiers;
};

struct xwl_screen {
    int width;
    int height;
    int depth;
    ScreenPtr screen;
    int expecting_event;
    enum RootClipMode root_clip_mode;

    int rootless;
    int glamor;
    int present;

    CreateScreenResourcesProcPtr CreateScreenResources;
    CloseScreenProcPtr CloseScreen;
    RealizeWindowProcPtr RealizeWindow;
    UnrealizeWindowProcPtr UnrealizeWindow;
    DestroyWindowProcPtr DestroyWindow;
    XYToWindowProcPtr XYToWindow;
    SetWindowPixmapProcPtr SetWindowPixmap;
    ResizeWindowProcPtr ResizeWindow;

    struct xorg_list output_list;
    struct xorg_list seat_list;
    struct xorg_list damage_window_list;
    struct xorg_list window_list;

    int wayland_fd;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_registry *input_registry;
    struct wl_compositor *compositor;
    struct zwp_tablet_manager_v2 *tablet_manager;
    struct wl_shm *shm;
    struct wl_shell *shell;
    struct zwp_relative_pointer_manager_v1 *relative_pointer_manager;
    struct zwp_pointer_constraints_v1 *pointer_constraints;
    struct zwp_xwayland_keyboard_grab_manager_v1 *wp_grab;
    struct zxdg_output_manager_v1 *xdg_output_manager;
    struct wp_viewporter *viewporter;
    uint32_t serial;

#define XWL_FORMAT_ARGB8888 (1 << 0)
#define XWL_FORMAT_XRGB8888 (1 << 1)
#define XWL_FORMAT_RGB565   (1 << 2)

    int prepare_read;
    int wait_flush;

    uint32_t num_formats;
    struct xwl_format *formats;
    void *egl_display, *egl_context;

    struct xwl_egl_backend gbm_backend;
    struct xwl_egl_backend eglstream_backend;
    /* pointer to the current backend for creating pixmaps on wayland */
    struct xwl_egl_backend *egl_backend;

    struct glamor_context *glamor_ctx;

    Atom allow_commits_prop;
};

#define MODIFIER_META 0x01

/* Apps which use randr/vidmode to change the mode when going fullscreen,
 * usually change the mode of only a single monitor, so this should be plenty.
 */
#define XWL_CLIENT_MAX_EMULATED_MODES 16

struct xwl_client {
    struct xwl_emulated_mode emulated_modes[XWL_CLIENT_MAX_EMULATED_MODES];
};

struct xwl_client *xwl_client_get(ClientPtr client);

void xwl_sync_events (struct xwl_screen *xwl_screen);
void xwl_surface_damage(struct xwl_screen *xwl_screen,
                        struct wl_surface *surface,
                        int32_t x, int32_t y, int32_t width, int32_t height);

struct xwl_screen *xwl_screen_get(ScreenPtr screen);
Bool xwl_screen_has_resolution_change_emulation(struct xwl_screen *xwl_screen);
struct xwl_output *xwl_screen_get_first_output(struct xwl_screen *xwl_screen);
void xwl_screen_check_resolution_change_emulation(struct xwl_screen *xwl_screen);

RRModePtr xwayland_cvt(int HDisplay, int VDisplay,
                       float VRefresh, Bool Reduced, Bool Interlaced);

#ifdef XF86VIDMODE
void xwlVidModeExtensionInit(void);
#endif

#ifdef GLXEXT
#include "glx_extinit.h"
extern __GLXprovider glamor_provider;
#endif

#endif
