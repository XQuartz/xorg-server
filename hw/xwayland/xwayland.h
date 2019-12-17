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
#include <input.h>
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

#ifdef GLAMOR_HAS_GBM
struct xwl_present_window {
    struct xwl_screen *xwl_screen;
    struct xwl_present_event *sync_flip;
    WindowPtr window;
    struct xorg_list frame_callback_list;

    uint64_t msc;
    uint64_t ust;

    OsTimerPtr frame_timer;

    struct wl_callback *sync_callback;

    struct xorg_list event_list;
    struct xorg_list release_queue;
};

struct xwl_present_event {
    uint64_t event_id;
    uint64_t target_msc;

    Bool abort;
    Bool pending;
    Bool buffer_released;

    struct xwl_present_window *xwl_present_window;
    struct wl_buffer *buffer;

    struct xorg_list list;
};
#endif

#define MODIFIER_META 0x01

struct xwl_touch {
    struct xwl_window *window;
    int32_t id;
    int x, y;
    struct xorg_list link_touch;
};

struct xwl_pointer_warp_emulator {
    struct xwl_seat *xwl_seat;
    struct xwl_window *locked_window;
    struct zwp_locked_pointer_v1 *locked_pointer;
};

struct xwl_cursor {
    void (* update_proc) (struct xwl_cursor *);
    struct wl_surface *surface;
    struct wl_callback *frame_cb;
    Bool needs_update;
};

struct xwl_seat {
    DeviceIntPtr pointer;
    DeviceIntPtr relative_pointer;
    DeviceIntPtr keyboard;
    DeviceIntPtr touch;
    DeviceIntPtr stylus;
    DeviceIntPtr eraser;
    DeviceIntPtr puck;
    struct xwl_screen *xwl_screen;
    struct wl_seat *seat;
    struct wl_pointer *wl_pointer;
    struct zwp_relative_pointer_v1 *wp_relative_pointer;
    struct wl_keyboard *wl_keyboard;
    struct wl_touch *wl_touch;
    struct zwp_tablet_seat_v2 *tablet_seat;
    struct wl_array keys;
    struct xwl_window *focus_window;
    struct xwl_window *tablet_focus_window;
    uint32_t id;
    uint32_t pointer_enter_serial;
    struct xorg_list link;
    CursorPtr x_cursor;
    struct xwl_cursor cursor;
    WindowPtr last_xwindow;

    struct xorg_list touches;

    size_t keymap_size;
    char *keymap;
    struct wl_surface *keyboard_focus;

    struct xorg_list axis_discrete_pending;
    struct xorg_list sync_pending;

    struct xwl_pointer_warp_emulator *pointer_warp_emulator;

    struct xwl_window *cursor_confinement_window;
    struct zwp_confined_pointer_v1 *confined_pointer;

    struct {
        Bool has_absolute;
        wl_fixed_t x;
        wl_fixed_t y;

        Bool has_relative;
        double dx;
        double dy;
        double dx_unaccel;
        double dy_unaccel;
    } pending_pointer_event;

    struct xorg_list tablets;
    struct xorg_list tablet_tools;
    struct xorg_list tablet_pads;
    struct zwp_xwayland_keyboard_grab_v1 *keyboard_grab;
};

struct xwl_tablet {
    struct xorg_list link;
    struct zwp_tablet_v2 *tablet;
    struct xwl_seat *seat;
};

struct xwl_tablet_tool {
    struct xorg_list link;
    struct zwp_tablet_tool_v2 *tool;
    struct xwl_seat *seat;

    DeviceIntPtr xdevice;
    uint32_t proximity_in_serial;
    double x;
    double y;
    uint32_t pressure;
    double tilt_x;
    double tilt_y;
    double rotation;
    double slider;

    uint32_t buttons_now,
             buttons_prev;

    int32_t wheel_clicks;

    struct xwl_cursor cursor;
};

struct xwl_tablet_pad_ring {
    unsigned int index;
    struct xorg_list link;
    struct xwl_tablet_pad_group *group;
    struct zwp_tablet_pad_ring_v2 *ring;
};

struct xwl_tablet_pad_strip {
    unsigned int index;
    struct xorg_list link;
    struct xwl_tablet_pad_group *group;
    struct zwp_tablet_pad_strip_v2 *strip;
};

struct xwl_tablet_pad_group {
    struct xorg_list link;
    struct xwl_tablet_pad *pad;
    struct zwp_tablet_pad_group_v2 *group;

    struct xorg_list pad_group_ring_list;
    struct xorg_list pad_group_strip_list;
};

struct xwl_tablet_pad {
    struct xorg_list link;
    struct zwp_tablet_pad_v2 *pad;
    struct xwl_seat *seat;

    DeviceIntPtr xdevice;

    unsigned int nbuttons;
    struct xorg_list pad_group_list;
};

struct xwl_output {
    struct xorg_list link;
    struct wl_output *output;
    struct zxdg_output_v1 *xdg_output;
    uint32_t server_output_id;
    struct xwl_screen *xwl_screen;
    RROutputPtr randr_output;
    RRCrtcPtr randr_crtc;
    int32_t x, y, width, height, refresh;
    Rotation rotation;
    Bool wl_output_done;
    Bool xdg_output_done;
};

/* Per client per output emulated randr/vidmode resolution info. */
struct xwl_emulated_mode {
    uint32_t server_output_id;
    int32_t width;
    int32_t height;
    Bool from_vidmode;
};

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

Bool xwl_screen_init_cursor(struct xwl_screen *xwl_screen);

struct xwl_screen *xwl_screen_get(ScreenPtr screen);
Bool xwl_screen_has_resolution_change_emulation(struct xwl_screen *xwl_screen);
struct xwl_output *xwl_screen_get_first_output(struct xwl_screen *xwl_screen);
void xwl_screen_check_resolution_change_emulation(struct xwl_screen *xwl_screen);

void xwl_tablet_tool_set_cursor(struct xwl_tablet_tool *tool);
void xwl_seat_set_cursor(struct xwl_seat *xwl_seat);

void xwl_seat_destroy(struct xwl_seat *xwl_seat);

void xwl_seat_clear_touch(struct xwl_seat *xwl_seat, WindowPtr window);

void xwl_seat_emulate_pointer_warp(struct xwl_seat *xwl_seat,
                                   struct xwl_window *xwl_window,
                                   SpritePtr sprite,
                                   int x, int y);

void xwl_seat_destroy_pointer_warp_emulator(struct xwl_seat *xwl_seat);

void xwl_seat_cursor_visibility_changed(struct xwl_seat *xwl_seat);

void xwl_seat_confine_pointer(struct xwl_seat *xwl_seat,
                              struct xwl_window *xwl_window);
void xwl_seat_unconfine_pointer(struct xwl_seat *xwl_seat);

Bool xwl_screen_init_output(struct xwl_screen *xwl_screen);

struct xwl_output *xwl_output_create(struct xwl_screen *xwl_screen,
                                     uint32_t id);

void xwl_output_destroy(struct xwl_output *xwl_output);

void xwl_output_remove(struct xwl_output *xwl_output);

struct xwl_emulated_mode *xwl_output_get_emulated_mode_for_client(
                            struct xwl_output *xwl_output, ClientPtr client);

RRModePtr xwl_output_find_mode(struct xwl_output *xwl_output,
                               int32_t width, int32_t height);
void xwl_output_set_emulated_mode(struct xwl_output *xwl_output,
                                  ClientPtr client, RRModePtr mode,
                                  Bool from_vidmode);
void xwl_output_set_window_randr_emu_props(struct xwl_screen *xwl_screen,
                                           WindowPtr window);

RRModePtr xwayland_cvt(int HDisplay, int VDisplay,
                       float VRefresh, Bool Reduced, Bool Interlaced);

#ifdef XWL_HAS_GLAMOR

#ifdef GLAMOR_HAS_GBM
void xwl_present_frame_callback(struct xwl_present_window *xwl_present_window);
Bool xwl_present_init(ScreenPtr screen);
void xwl_present_cleanup(WindowPtr window);
void xwl_present_unrealize_window(WindowPtr window);
#endif /* GLAMOR_HAS_GBM */

#endif /* XWL_HAS_GLAMOR */

void xwl_screen_release_tablet_manager(struct xwl_screen *xwl_screen);

void xwl_screen_init_xdg_output(struct xwl_screen *xwl_screen);

#ifdef XF86VIDMODE
void xwlVidModeExtensionInit(void);
#endif

#ifdef GLXEXT
#include "glx_extinit.h"
extern __GLXprovider glamor_provider;
#endif

#endif
