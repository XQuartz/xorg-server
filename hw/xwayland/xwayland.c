/*
 * Copyright © 2011-2014 Intel Corporation
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

#include <stdio.h>

#include <X11/Xatom.h>
#include <selection.h>
#include <micmap.h>
#include <misyncshm.h>
#include <compositeext.h>
#include <compint.h>
#include <glx_extinit.h>
#include <os.h>
#include <xserver_poll.h>
#include <propertyst.h>

#include "xwayland.h"
#include "xwayland-cursor.h"
#include "xwayland-glamor.h"
#include "xwayland-input.h"
#include "xwayland-output.h"
#include "xwayland-pixmap.h"
#include "xwayland-present.h"
#include "xwayland-shm.h"
#include "xwayland-window-buffers.h"
#include "xwayland-window.h"

#ifdef XF86VIDMODE
#include <X11/extensions/xf86vmproto.h>
_X_EXPORT Bool noXFree86VidModeExtension;
#endif

void
ddxGiveUp(enum ExitCode error)
{
}

void
OsVendorInit(void)
{
    if (serverGeneration == 1)
        ForceClockId(CLOCK_MONOTONIC);
}

void
OsVendorFatalError(const char *f, va_list args)
{
}

#if defined(DDXBEFORERESET)
void
ddxBeforeReset(void)
{
    return;
}
#endif

#if INPUTTHREAD
/** This function is called in Xserver/os/inputthread.c when starting
    the input thread. */
void
ddxInputThreadInit(void)
{
}
#endif

 _X_NORETURN
static void _X_ATTRIBUTE_PRINTF(1, 2)
xwl_give_up(const char *f, ...)
{
    va_list args;

    va_start(args, f);
    VErrorFSigSafe(f, args);
    va_end(args);

    CloseWellKnownConnections();
    OsCleanup(TRUE);
    fflush(stderr);
    exit(1);
}

void
ddxUseMsg(void)
{
    ErrorF("-rootless              run rootless, requires wm support\n");
    ErrorF("-wm fd                 create X client for wm on given fd\n");
    ErrorF("-initfd fd             add given fd as a listen socket for initialization clients\n");
    ErrorF("-listenfd fd           add given fd as a listen socket\n");
    ErrorF("-listen fd             deprecated, use \"-listenfd\" instead\n");
    ErrorF("-eglstream             use eglstream backend for nvidia GPUs\n");
}

static int init_fd = -1;
static int wm_fd = -1;
static int listen_fds[5] = { -1, -1, -1, -1, -1 };
static int listen_fd_count = 0;

static void
xwl_add_listen_fd(int argc, char *argv[], int i)
{
    NoListenAll = TRUE;
    if (listen_fd_count == ARRAY_SIZE(listen_fds))
        FatalError("Too many -listen arguments given, max is %zu\n",
                   ARRAY_SIZE(listen_fds));

    listen_fds[listen_fd_count++] = atoi(argv[i + 1]);
}

int
ddxProcessArgument(int argc, char *argv[], int i)
{
    if (strcmp(argv[i], "-rootless") == 0) {
        return 1;
    }
    else if (strcmp(argv[i], "-listen") == 0) {
        CHECK_FOR_REQUIRED_ARGUMENTS(1);

        /* Not an FD */
        if (!isdigit(*argv[i + 1]))
            return 0;

        LogMessage(X_WARNING, "Option \"-listen\" for file descriptors is deprecated\n"
                              "Please use \"-listenfd\" instead.\n");

        xwl_add_listen_fd (argc, argv, i);
        return 2;
    }
    else if (strcmp(argv[i], "-listenfd") == 0) {
        CHECK_FOR_REQUIRED_ARGUMENTS(1);

        xwl_add_listen_fd (argc, argv, i);
        return 2;
    }
    else if (strcmp(argv[i], "-wm") == 0) {
        CHECK_FOR_REQUIRED_ARGUMENTS(1);
        wm_fd = atoi(argv[i + 1]);
        return 2;
    }
    else if (strcmp(argv[i], "-initfd") == 0) {
        CHECK_FOR_REQUIRED_ARGUMENTS(1);
        init_fd = atoi(argv[i + 1]);
        return 2;
    }
    else if (strcmp(argv[i], "-shm") == 0) {
        return 1;
    }
    else if (strcmp(argv[i], "-eglstream") == 0) {
        return 1;
    }

    return 0;
}

static DevPrivateKeyRec xwl_client_private_key;
static DevPrivateKeyRec xwl_screen_private_key;

struct xwl_client *
xwl_client_get(ClientPtr client)
{
    return dixLookupPrivate(&client->devPrivates, &xwl_client_private_key);
}

struct xwl_screen *
xwl_screen_get(ScreenPtr screen)
{
    return dixLookupPrivate(&screen->devPrivates, &xwl_screen_private_key);
}

static Bool
xwl_screen_has_viewport_support(struct xwl_screen *xwl_screen)
{
    return wl_compositor_get_version(xwl_screen->compositor) >=
                            WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION &&
           xwl_screen->viewporter != NULL;
}

Bool
xwl_screen_has_resolution_change_emulation(struct xwl_screen *xwl_screen)
{
    /* Resolution change emulation is only supported in rootless mode and
     * it requires viewport support.
     */
    return xwl_screen->rootless && xwl_screen_has_viewport_support(xwl_screen);
}

/* Return the output @ 0x0, falling back to the first output in the list */
struct xwl_output *
xwl_screen_get_first_output(struct xwl_screen *xwl_screen)
{
    struct xwl_output *xwl_output;

    xorg_list_for_each_entry(xwl_output, &xwl_screen->output_list, link) {
        if (xwl_output->x == 0 && xwl_output->y == 0)
            return xwl_output;
    }

    if (xorg_list_is_empty(&xwl_screen->output_list))
        return NULL;

    return xorg_list_first_entry(&xwl_screen->output_list, struct xwl_output, link);
}

static void
xwl_property_callback(CallbackListPtr *pcbl, void *closure,
                      void *calldata)
{
    ScreenPtr screen = closure;
    PropertyStateRec *rec = calldata;
    struct xwl_screen *xwl_screen;
    struct xwl_window *xwl_window;

    if (rec->win->drawable.pScreen != screen)
        return;

    xwl_window = xwl_window_get(rec->win);
    if (!xwl_window)
        return;

    xwl_screen = xwl_screen_get(screen);

    if (rec->prop->propertyName == xwl_screen->allow_commits_prop)
        xwl_window_update_property(xwl_window, rec);
}

static Bool
xwl_close_screen(ScreenPtr screen)
{
    struct xwl_screen *xwl_screen = xwl_screen_get(screen);
    struct xwl_output *xwl_output, *next_xwl_output;
    struct xwl_seat *xwl_seat, *next_xwl_seat;

    DeleteCallback(&PropertyStateCallback, xwl_property_callback, screen);

    xorg_list_for_each_entry_safe(xwl_output, next_xwl_output,
                                  &xwl_screen->output_list, link)
        xwl_output_destroy(xwl_output);

    xorg_list_for_each_entry_safe(xwl_seat, next_xwl_seat,
                                  &xwl_screen->seat_list, link)
        xwl_seat_destroy(xwl_seat);

    xwl_screen_release_tablet_manager(xwl_screen);

    RemoveNotifyFd(xwl_screen->wayland_fd);

    wl_display_disconnect(xwl_screen->display);

    screen->CloseScreen = xwl_screen->CloseScreen;
    free(xwl_screen);

    return screen->CloseScreen(screen);
}

static struct xwl_seat *
xwl_screen_get_default_seat(struct xwl_screen *xwl_screen)
{
    if (xorg_list_is_empty(&xwl_screen->seat_list))
        return NULL;

    return container_of(xwl_screen->seat_list.prev,
                        struct xwl_seat,
                        link);
}

static void
xwl_cursor_warped_to(DeviceIntPtr device,
                     ScreenPtr screen,
                     ClientPtr client,
                     WindowPtr window,
                     SpritePtr sprite,
                     int x, int y)
{
    struct xwl_screen *xwl_screen = xwl_screen_get(screen);
    struct xwl_seat *xwl_seat = device->public.devicePrivate;
    struct xwl_window *xwl_window;
    WindowPtr focus;

    if (!xwl_seat)
        xwl_seat = xwl_screen_get_default_seat(xwl_screen);

    if (!window)
        window = XYToWindow(sprite, x, y);

    xwl_window = xwl_window_from_window(window);
    if (!xwl_window && xwl_seat->focus_window) {
        focus = xwl_seat->focus_window->window;

        /* Warps on non wl_surface backed Windows are only allowed
         * as long as the pointer stays within the focus window.
         */
        if (x >= focus->drawable.x &&
            y >= focus->drawable.y &&
            x < focus->drawable.x + focus->drawable.width &&
            y < focus->drawable.y + focus->drawable.height) {
            if (!window) {
                DebugF("Warp relative to pointer, assuming pointer focus\n");
                xwl_window = xwl_seat->focus_window;
            } else if (window == screen->root) {
                DebugF("Warp on root window, assuming pointer focus\n");
                xwl_window = xwl_seat->focus_window;
            }
        }
    }
    if (!xwl_window)
        return;

    xwl_seat_emulate_pointer_warp(xwl_seat, xwl_window, sprite, x, y);
}

static void
xwl_cursor_confined_to(DeviceIntPtr device,
                       ScreenPtr screen,
                       WindowPtr window)
{
    struct xwl_screen *xwl_screen = xwl_screen_get(screen);
    struct xwl_seat *xwl_seat = device->public.devicePrivate;
    struct xwl_window *xwl_window;

    if (!xwl_seat)
        xwl_seat = xwl_screen_get_default_seat(xwl_screen);

    /* xwl_seat hasn't been setup yet, don't do anything just yet */
    if (!xwl_seat)
        return;

    if (window == screen->root) {
        xwl_seat_unconfine_pointer(xwl_seat);
        return;
    }

    xwl_window = xwl_window_from_window(window);
    if (!xwl_window && xwl_seat->focus_window) {
        /* Allow confining on InputOnly windows, but only if the geometry
         * is the same than the focus window.
         */
        if (window->drawable.class == InputOnly) {
            DebugF("Confine on InputOnly window, assuming pointer focus\n");
            xwl_window = xwl_seat->focus_window;
        }
    }
    if (!xwl_window)
        return;

    xwl_seat_confine_pointer(xwl_seat, xwl_window);
}

void
xwl_screen_check_resolution_change_emulation(struct xwl_screen *xwl_screen)
{
    struct xwl_window *xwl_window;

    xorg_list_for_each_entry(xwl_window, &xwl_screen->window_list, link_window)
        xwl_window_check_resolution_change_emulation(xwl_window);
}

static void
xwl_screen_post_damage(struct xwl_screen *xwl_screen)
{
    struct xwl_window *xwl_window, *next_xwl_window;

    xorg_list_for_each_entry_safe(xwl_window, next_xwl_window,
                                  &xwl_screen->damage_window_list, link_damage) {
        /* If we're waiting on a frame callback from the server,
         * don't attach a new buffer. */
        if (xwl_window->frame_callback)
            continue;

        if (!xwl_window->allow_commits)
            continue;

#ifdef XWL_HAS_GLAMOR
        if (xwl_screen->glamor && !xwl_glamor_allow_commits(xwl_window))
            continue;
#endif

        xwl_window_post_damage(xwl_window);
    }
}

static void
registry_global(void *data, struct wl_registry *registry, uint32_t id,
                const char *interface, uint32_t version)
{
    struct xwl_screen *xwl_screen = data;

    if (strcmp(interface, "wl_compositor") == 0) {
        uint32_t request_version = 1;

        if (version >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION)
            request_version = WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION;

        xwl_screen->compositor =
            wl_registry_bind(registry, id, &wl_compositor_interface, request_version);
    }
    else if (strcmp(interface, "wl_shm") == 0) {
        xwl_screen->shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
    }
    else if (strcmp(interface, "wl_shell") == 0) {
        xwl_screen->shell =
            wl_registry_bind(registry, id, &wl_shell_interface, 1);
    }
    else if (strcmp(interface, "wl_output") == 0 && version >= 2) {
        if (xwl_output_create(xwl_screen, id))
            xwl_screen->expecting_event++;
    }
    else if (strcmp(interface, "zxdg_output_manager_v1") == 0) {
        /* We support xdg-output from version 1 to version 3 */
        version = min(version, 3);
        xwl_screen->xdg_output_manager =
            wl_registry_bind(registry, id, &zxdg_output_manager_v1_interface, version);
        xwl_screen_init_xdg_output(xwl_screen);
    }
    else if (strcmp(interface, "wp_viewporter") == 0) {
        xwl_screen->viewporter = wl_registry_bind(registry, id, &wp_viewporter_interface, 1);
    }
#ifdef XWL_HAS_GLAMOR
    else if (xwl_screen->glamor) {
        xwl_glamor_init_wl_registry(xwl_screen, registry, id, interface,
                                    version);
    }
#endif
}

static void
global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    struct xwl_screen *xwl_screen = data;
    struct xwl_output *xwl_output, *tmp_xwl_output;

    xorg_list_for_each_entry_safe(xwl_output, tmp_xwl_output,
                                  &xwl_screen->output_list, link) {
        if (xwl_output->server_output_id == name) {
            xwl_output_remove(xwl_output);
            break;
        }
    }
}

static const struct wl_registry_listener registry_listener = {
    registry_global,
    global_remove
};

static void
xwl_read_events (struct xwl_screen *xwl_screen)
{
    int ret;

    if (xwl_screen->wait_flush)
        return;

    ret = wl_display_read_events(xwl_screen->display);
    if (ret == -1)
        xwl_give_up("failed to read Wayland events: %s\n", strerror(errno));

    xwl_screen->prepare_read = 0;

    ret = wl_display_dispatch_pending(xwl_screen->display);
    if (ret == -1)
        xwl_give_up("failed to dispatch Wayland events: %s\n", strerror(errno));
}

static int
xwl_display_pollout (struct xwl_screen *xwl_screen, int timeout)
{
    struct pollfd poll_fd;

    poll_fd.fd = wl_display_get_fd(xwl_screen->display);
    poll_fd.events = POLLOUT;

    return xserver_poll(&poll_fd, 1, timeout);
}

static void
xwl_dispatch_events (struct xwl_screen *xwl_screen)
{
    int ret = 0;
    int ready;

    if (xwl_screen->wait_flush)
        goto pollout;

    while (xwl_screen->prepare_read == 0 &&
           wl_display_prepare_read(xwl_screen->display) == -1) {
        ret = wl_display_dispatch_pending(xwl_screen->display);
        if (ret == -1)
            xwl_give_up("failed to dispatch Wayland events: %s\n",
                       strerror(errno));
    }

    xwl_screen->prepare_read = 1;

pollout:
    ready = xwl_display_pollout(xwl_screen, 5);
    if (ready == -1 && errno != EINTR)
        xwl_give_up("error polling on XWayland fd: %s\n", strerror(errno));

    if (ready > 0)
        ret = wl_display_flush(xwl_screen->display);

    if (ret == -1 && errno != EAGAIN)
        xwl_give_up("failed to write to XWayland fd: %s\n", strerror(errno));

    xwl_screen->wait_flush = (ready == 0 || ready == -1 || ret == -1);
}

static void
socket_handler(int fd, int ready, void *data)
{
    struct xwl_screen *xwl_screen = data;

    xwl_read_events (xwl_screen);
}

static void
wakeup_handler(void *data, int err)
{
}

static void
block_handler(void *data, void *timeout)
{
    struct xwl_screen *xwl_screen = data;

    xwl_screen_post_damage(xwl_screen);
    xwl_dispatch_events (xwl_screen);
}

void
xwl_sync_events (struct xwl_screen *xwl_screen)
{
    xwl_dispatch_events (xwl_screen);
    xwl_read_events (xwl_screen);
}

static CARD32
add_client_fd(OsTimerPtr timer, CARD32 time, void *arg)
{
    if (!AddClientOnOpenFD(wm_fd))
        FatalError("Failed to add wm client\n");

    TimerFree(timer);

    return 0;
}

static void
listen_on_fds(void)
{
    int i;

    for (i = 0; i < listen_fd_count; i++)
        ListenOnOpenFD(listen_fds[i], FALSE);
}

static void
wm_selection_callback(CallbackListPtr *p, void *data, void *arg)
{
    SelectionInfoRec *info = arg;
    struct xwl_screen *xwl_screen = data;
    static const char atom_name[] = "WM_S0";
    static Atom atom_wm_s0;

    if (atom_wm_s0 == None)
        atom_wm_s0 = MakeAtom(atom_name, strlen(atom_name), TRUE);
    if (info->selection->selection != atom_wm_s0 ||
        info->kind != SelectionSetOwner)
        return;

    listen_on_fds();

    DeleteCallback(&SelectionCallback, wm_selection_callback, xwl_screen);
}

static Bool
xwl_screen_init(ScreenPtr pScreen, int argc, char **argv)
{
    static const char allow_commits[] = "_XWAYLAND_ALLOW_COMMITS";
    struct xwl_screen *xwl_screen;
    Pixel red_mask, blue_mask, green_mask;
    int ret, bpc, green_bpc, i;
#ifdef XWL_HAS_GLAMOR
    Bool use_eglstreams = FALSE;
#endif

    xwl_screen = calloc(1, sizeof *xwl_screen);
    if (xwl_screen == NULL)
        return FALSE;

    if (!dixRegisterPrivateKey(&xwl_screen_private_key, PRIVATE_SCREEN, 0))
        return FALSE;
    if (!xwl_pixmap_init())
        return FALSE;
    if (!xwl_window_init())
        return FALSE;
    /* There are no easy to use new / delete client hooks, we could use a
     * ClientStateCallback, but it is easier to let the dix code manage the
     * memory for us. This will zero fill the initial xwl_client data.
     */
    if (!dixRegisterPrivateKey(&xwl_client_private_key, PRIVATE_CLIENT,
                               sizeof(struct xwl_client)))
        return FALSE;

    dixSetPrivate(&pScreen->devPrivates, &xwl_screen_private_key, xwl_screen);
    xwl_screen->screen = pScreen;

#ifdef XWL_HAS_GLAMOR
    xwl_screen->glamor = 1;
#endif

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-rootless") == 0) {
            xwl_screen->rootless = 1;
        }
        else if (strcmp(argv[i], "-shm") == 0) {
            xwl_screen->glamor = 0;
        }
        else if (strcmp(argv[i], "-eglstream") == 0) {
#ifdef XWL_HAS_EGLSTREAM
            use_eglstreams = TRUE;
#else
            ErrorF("xwayland glamor: this build does not have EGLStream support\n");
#endif
        }
    }

#ifdef XWL_HAS_GLAMOR
    if (xwl_screen->glamor)
        xwl_glamor_init_backends(xwl_screen, use_eglstreams);
#endif

    /* In rootless mode, we don't have any screen storage, and the only
     * rendering should be to redirected mode. */
    if (xwl_screen->rootless)
        xwl_screen->root_clip_mode = ROOT_CLIP_INPUT_ONLY;
    else
        xwl_screen->root_clip_mode = ROOT_CLIP_FULL;

    xorg_list_init(&xwl_screen->output_list);
    xorg_list_init(&xwl_screen->seat_list);
    xorg_list_init(&xwl_screen->damage_window_list);
    xorg_list_init(&xwl_screen->window_list);
    xwl_screen->depth = 24;

    xwl_screen->display = wl_display_connect(NULL);
    if (xwl_screen->display == NULL) {
        ErrorF("could not connect to wayland server\n");
        return FALSE;
    }

    if (!xwl_screen_init_output(xwl_screen))
        return FALSE;

    xwl_screen->expecting_event = 0;
    xwl_screen->registry = wl_display_get_registry(xwl_screen->display);
    wl_registry_add_listener(xwl_screen->registry,
                             &registry_listener, xwl_screen);
    ret = wl_display_roundtrip(xwl_screen->display);
    if (ret == -1) {
        ErrorF("could not connect to wayland server\n");
        return FALSE;
    }

    while (xwl_screen->expecting_event > 0)
        wl_display_roundtrip(xwl_screen->display);

    bpc = xwl_screen->depth / 3;
    green_bpc = xwl_screen->depth - 2 * bpc;
    blue_mask = (1 << bpc) - 1;
    green_mask = ((1 << green_bpc) - 1) << bpc;
    red_mask = blue_mask << (green_bpc + bpc);

    miSetVisualTypesAndMasks(xwl_screen->depth,
                             ((1 << TrueColor) | (1 << DirectColor)),
                             green_bpc, TrueColor,
                             red_mask, green_mask, blue_mask);

    miSetPixmapDepths();

    ret = fbScreenInit(pScreen, NULL,
                       xwl_screen->width, xwl_screen->height,
                       96, 96, 0,
                       BitsPerPixel(xwl_screen->depth));
    if (!ret)
        return FALSE;

    fbPictureInit(pScreen, 0, 0);

#ifdef HAVE_XSHMFENCE
    if (!miSyncShmScreenInit(pScreen))
        return FALSE;
#endif

    xwl_screen->wayland_fd = wl_display_get_fd(xwl_screen->display);
    SetNotifyFd(xwl_screen->wayland_fd, socket_handler, X_NOTIFY_READ, xwl_screen);
    RegisterBlockAndWakeupHandlers(block_handler, wakeup_handler, xwl_screen);

    pScreen->blackPixel = 0;
    pScreen->whitePixel = 1;

    ret = fbCreateDefColormap(pScreen);

    if (!xwl_screen_init_cursor(xwl_screen))
        return FALSE;

#ifdef XWL_HAS_GLAMOR
    if (xwl_screen->glamor) {
        xwl_glamor_select_backend(xwl_screen, use_eglstreams);

        if (xwl_screen->egl_backend == NULL || !xwl_glamor_init(xwl_screen)) {
           ErrorF("Failed to initialize glamor, falling back to sw\n");
           xwl_screen->glamor = 0;
        }
    }

    if (xwl_screen->glamor && xwl_screen->rootless)
        xwl_screen->present = xwl_present_init(pScreen);
#endif

    if (!xwl_screen->glamor) {
        xwl_screen->CreateScreenResources = pScreen->CreateScreenResources;
        pScreen->CreateScreenResources = xwl_shm_create_screen_resources;
        pScreen->CreatePixmap = xwl_shm_create_pixmap;
        pScreen->DestroyPixmap = xwl_shm_destroy_pixmap;
    }

    xwl_screen->RealizeWindow = pScreen->RealizeWindow;
    pScreen->RealizeWindow = xwl_realize_window;

    xwl_screen->UnrealizeWindow = pScreen->UnrealizeWindow;
    pScreen->UnrealizeWindow = xwl_unrealize_window;

    xwl_screen->DestroyWindow = pScreen->DestroyWindow;
    pScreen->DestroyWindow = xwl_destroy_window;

    xwl_screen->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = xwl_close_screen;

    xwl_screen->ResizeWindow = pScreen->ResizeWindow;
    pScreen->ResizeWindow = xwl_resize_window;

    if (xwl_screen->rootless) {
        xwl_screen->SetWindowPixmap = pScreen->SetWindowPixmap;
        pScreen->SetWindowPixmap = xwl_window_set_window_pixmap;
    }

    pScreen->CursorWarpedTo = xwl_cursor_warped_to;
    pScreen->CursorConfinedTo = xwl_cursor_confined_to;

    xwl_screen->allow_commits_prop = MakeAtom(allow_commits,
                                              strlen(allow_commits),
                                              TRUE);
    if (xwl_screen->allow_commits_prop == BAD_RESOURCE)
        return FALSE;

    AddCallback(&PropertyStateCallback, xwl_property_callback, pScreen);

    wl_display_roundtrip(xwl_screen->display);
    while (xwl_screen->expecting_event)
        wl_display_roundtrip(xwl_screen->display);

    return ret;
}

_X_NORETURN
static void _X_ATTRIBUTE_PRINTF(1, 0)
xwl_log_handler(const char *format, va_list args)
{
    char msg[256];

    vsnprintf(msg, sizeof msg, format, args);
    FatalError("%s", msg);
}

static const ExtensionModule xwayland_extensions[] = {
#ifdef XF86VIDMODE
    { xwlVidModeExtensionInit, XF86VIDMODENAME, &noXFree86VidModeExtension },
#endif
};

void
InitOutput(ScreenInfo * screen_info, int argc, char **argv)
{
    int depths[] = { 1, 4, 8, 15, 16, 24, 32 };
    int bpp[] =    { 1, 8, 8, 16, 16, 32, 32 };
    int i;

    for (i = 0; i < ARRAY_SIZE(depths); i++) {
        screen_info->formats[i].depth = depths[i];
        screen_info->formats[i].bitsPerPixel = bpp[i];
        screen_info->formats[i].scanlinePad = BITMAP_SCANLINE_PAD;
    }

    screen_info->imageByteOrder = IMAGE_BYTE_ORDER;
    screen_info->bitmapScanlineUnit = BITMAP_SCANLINE_UNIT;
    screen_info->bitmapScanlinePad = BITMAP_SCANLINE_PAD;
    screen_info->bitmapBitOrder = BITMAP_BIT_ORDER;
    screen_info->numPixmapFormats = ARRAY_SIZE(depths);

    if (serverGeneration == 1)
        LoadExtensionList(xwayland_extensions,
                          ARRAY_SIZE(xwayland_extensions), FALSE);

    /* Cast away warning from missing printf annotation for
     * wl_log_func_t.  Wayland 1.5 will have the annotation, so we can
     * remove the cast and require that when it's released. */
    wl_log_set_handler_client((void *) xwl_log_handler);

    if (AddScreen(xwl_screen_init, argc, argv) == -1) {
        FatalError("Couldn't add screen\n");
    }

    xorgGlxCreateVendor();

    LocalAccessScopeUser();

    if (wm_fd >= 0 || init_fd >= 0) {
        if (wm_fd >= 0)
            TimerSet(NULL, 0, 1, add_client_fd, NULL);
        if (init_fd >= 0)
            ListenOnOpenFD(init_fd, FALSE);
        AddCallback(&SelectionCallback, wm_selection_callback, NULL);
    }
    else if (listen_fd_count > 0) {
        listen_on_fds();
    }
}
