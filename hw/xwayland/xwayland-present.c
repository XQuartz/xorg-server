/*
 * Copyright © 2018 Roman Gilg
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

#include <xwayland-config.h>

#include <windowstr.h>
#include <present.h>

#include "xwayland-present.h"
#include "xwayland-screen.h"
#include "xwayland-window.h"
#include "xwayland-pixmap.h"
#include "glamor.h"

/*
 * When not flipping let Present copy with 60fps.
 * When flipping wait on frame_callback, otherwise
 * the surface is not visible, in this case update
 * with long interval.
 */
#define TIMER_LEN_COPY      17  // ~60fps
#define TIMER_LEN_FLIP    1000  // 1fps

static uint64_t present_wnmd_event_id;

static DevPrivateKeyRec xwl_present_window_private_key;

static struct xwl_present_window *
xwl_present_window_priv(WindowPtr window)
{
    return dixGetPrivate(&window->devPrivates,
                         &xwl_present_window_private_key);
}

static struct xwl_present_window *
xwl_present_window_get_priv(WindowPtr window)
{
    struct xwl_present_window *xwl_present_window = xwl_present_window_priv(window);

    if (xwl_present_window == NULL) {
        xwl_present_window = calloc (1, sizeof (struct xwl_present_window));
        if (!xwl_present_window)
            return NULL;

        xwl_present_window->window = window;
        xwl_present_window->msc = 1;
        xwl_present_window->ust = GetTimeInMicros();

        xorg_list_init(&xwl_present_window->frame_callback_list);
        xorg_list_init(&xwl_present_window->wait_list);
        xorg_list_init(&xwl_present_window->release_list);
        xorg_list_init(&xwl_present_window->exec_queue);
        xorg_list_init(&xwl_present_window->flip_queue);
        xorg_list_init(&xwl_present_window->idle_queue);

        dixSetPrivate(&window->devPrivates,
                      &xwl_present_window_private_key,
                      xwl_present_window);
    }

    return xwl_present_window;
}

static void
xwl_present_free_timer(struct xwl_present_window *xwl_present_window)
{
    TimerFree(xwl_present_window->frame_timer);
    xwl_present_window->frame_timer = NULL;
}

static CARD32
xwl_present_timer_callback(OsTimerPtr timer,
                           CARD32 time,
                           void *arg);

static inline Bool
xwl_present_has_pending_events(struct xwl_present_window *xwl_present_window)
{
    return !!xwl_present_window->sync_flip ||
           !xorg_list_is_empty(&xwl_present_window->wait_list);
}

static void
xwl_present_reset_timer(struct xwl_present_window *xwl_present_window)
{
    if (xwl_present_has_pending_events(xwl_present_window)) {
        CARD32 timeout;

        if (!xorg_list_is_empty(&xwl_present_window->frame_callback_list))
            timeout = TIMER_LEN_FLIP;
        else
            timeout = TIMER_LEN_COPY;

        xwl_present_window->frame_timer = TimerSet(xwl_present_window->frame_timer,
                                                   0, timeout,
                                                   &xwl_present_timer_callback,
                                                   xwl_present_window);
    } else {
        xwl_present_free_timer(xwl_present_window);
    }
}


static void
present_wnmd_execute(present_vblank_ptr vblank, uint64_t ust, uint64_t crtc_msc);

static int
present_wnmd_queue_vblank(ScreenPtr screen,
                          WindowPtr window,
                          RRCrtcPtr crtc,
                          uint64_t event_id,
                          uint64_t msc)
{
    present_screen_priv_ptr screen_priv = present_screen_priv(screen);
    return (*screen_priv->wnmd_info->queue_vblank) (window, crtc, event_id, msc);
}

static uint32_t
present_wnmd_query_capabilities(present_screen_priv_ptr screen_priv)
{
    return screen_priv->wnmd_info->capabilities;
}

static RRCrtcPtr
present_wnmd_get_crtc(present_screen_priv_ptr screen_priv, WindowPtr window)
{
    return (*screen_priv->wnmd_info->get_crtc)(window);
}

static int
present_wnmd_get_ust_msc(ScreenPtr screen, WindowPtr window, uint64_t *ust, uint64_t *msc)
{
    present_screen_priv_ptr screen_priv = present_screen_priv(screen);
    return (*screen_priv->wnmd_info->get_ust_msc)(window, ust, msc);
}

/*
 * When the wait fence or previous flip is completed, it's time
 * to re-try the request
 */
static void
present_wnmd_re_execute(present_vblank_ptr vblank)
{
    uint64_t ust = 0, crtc_msc = 0;

    (void) present_wnmd_get_ust_msc(vblank->screen, vblank->window, &ust, &crtc_msc);
    present_wnmd_execute(vblank, ust, crtc_msc);
}

static void
present_wnmd_flip_try_ready(WindowPtr window)
{
    struct xwl_present_window *xwl_present_window = xwl_present_window_priv(window);
    present_vblank_ptr      vblank;

    xorg_list_for_each_entry(vblank, &xwl_present_window->flip_queue, event_queue) {
        if (vblank->queued) {
            present_wnmd_re_execute(vblank);
            return;
        }
    }
}

static void
present_wnmd_free_idle_vblank(present_vblank_ptr vblank)
{
    present_pixmap_idle(vblank->pixmap, vblank->window, vblank->serial, vblank->idle_fence);
    present_vblank_destroy(vblank);
}

/*
 * Free any left over idle vblanks
 */
static void
present_wnmd_free_idle_vblanks(WindowPtr window)
{
    struct xwl_present_window *xwl_present_window = xwl_present_window_priv(window);
    present_vblank_ptr              vblank, tmp;

    xorg_list_for_each_entry_safe(vblank, tmp, &xwl_present_window->idle_queue, event_queue) {
        present_wnmd_free_idle_vblank(vblank);
    }

    if (xwl_present_window->flip_active) {
        present_wnmd_free_idle_vblank(xwl_present_window->flip_active);
        xwl_present_window->flip_active = NULL;
    }
}

static WindowPtr
present_wnmd_toplvl_pixmap_window(WindowPtr window)
{
    ScreenPtr       screen = window->drawable.pScreen;
    PixmapPtr       pixmap = (*screen->GetWindowPixmap)(window);
    WindowPtr       w = window;
    WindowPtr       next_w;

    while(w->parent) {
        next_w = w->parent;
        if ( (*screen->GetWindowPixmap)(next_w) != pixmap) {
            break;
        }
        w = next_w;
    }
    return w;
}

static void
present_wnmd_flips_stop(WindowPtr window)
{
    struct xwl_present_window *xwl_present_window = xwl_present_window_priv(window);
    present_screen_priv_ptr screen_priv = present_screen_priv(window->drawable.pScreen);

    assert (!xwl_present_window->flip_pending);

    (*screen_priv->wnmd_info->flips_stop) (window);

    present_wnmd_free_idle_vblanks(window);
    present_wnmd_flip_try_ready(window);
}

static void
present_wnmd_flip_notify_vblank(present_vblank_ptr vblank, uint64_t ust, uint64_t crtc_msc)
{
    WindowPtr                   window = vblank->window;
    struct xwl_present_window *xwl_present_window = xwl_present_window_priv(window);

    DebugPresent(("\tn %" PRIu64 " %p %" PRIu64 " %" PRIu64 ": %08" PRIx32 " -> %08" PRIx32 "\n",
                  vblank->event_id, vblank, vblank->exec_msc, vblank->target_msc,
                  vblank->pixmap ? vblank->pixmap->drawable.id : 0,
                  vblank->window ? vblank->window->drawable.id : 0));

    assert (vblank == xwl_present_window->flip_pending);

    xorg_list_del(&vblank->event_queue);

    if (xwl_present_window->flip_active) {
        if (xwl_present_window->flip_active->flip_idler)
            present_wnmd_free_idle_vblank(xwl_present_window->flip_active);
        else
            /* Put the previous flip in the idle_queue and wait for further notice from
             * the Wayland compositor
             */
            xorg_list_append(&xwl_present_window->flip_active->event_queue, &xwl_present_window->idle_queue);
    }

    xwl_present_window->flip_active = vblank;
    xwl_present_window->flip_pending = NULL;

    present_vblank_notify(vblank, PresentCompleteKindPixmap, PresentCompleteModeFlip, ust, crtc_msc);

    if (vblank->abort_flip)
        present_wnmd_flips_stop(window);

    present_wnmd_flip_try_ready(window);
}

static void
present_wnmd_event_notify(WindowPtr window, uint64_t event_id, uint64_t ust, uint64_t msc)
{
    struct xwl_present_window *xwl_present_window = xwl_present_window_priv(window);
    present_window_priv_ptr     window_priv = present_window_priv(window);
    present_vblank_ptr          vblank;

    if (!window_priv)
        return;
    if (!event_id)
        return;

    DebugPresent(("\te %" PRIu64 " ust %" PRIu64 " msc %" PRIu64 "\n", event_id, ust, msc));
    xorg_list_for_each_entry(vblank, &xwl_present_window->exec_queue, event_queue) {
        if (event_id == vblank->event_id) {
            present_wnmd_execute(vblank, ust, msc);
            return;
        }
    }
    xorg_list_for_each_entry(vblank, &xwl_present_window->flip_queue, event_queue) {
        if (vblank->event_id == event_id) {
            assert(vblank->queued);
            present_wnmd_execute(vblank, ust, msc);
            return;
        }
    }

    /* Copies which were executed but need their completion event sent */
    xorg_list_for_each_entry(vblank, &xwl_present_window->idle_queue, event_queue) {
        if (vblank->event_id == event_id) {
            present_execute_post(vblank, ust, msc);
            return;
        }
    }
}

static void
present_wnmd_flip_notify(WindowPtr window, uint64_t event_id, uint64_t ust, uint64_t msc)
{
    struct xwl_present_window *xwl_present_window = xwl_present_window_priv(window);
    present_vblank_ptr          vblank;

    xorg_list_for_each_entry(vblank, &xwl_present_window->flip_queue, event_queue) {
        if (vblank->event_id == event_id) {
            assert(!vblank->queued);
            assert(vblank->window);
            present_wnmd_flip_notify_vblank(vblank, ust, msc);
            return;
        }
    }
}

static void
present_wnmd_idle_notify(WindowPtr window, uint64_t event_id)
{
    struct xwl_present_window *xwl_present_window = xwl_present_window_priv(window);
    present_vblank_ptr          vblank;

    if (xwl_present_window->flip_active && xwl_present_window->flip_active->event_id == event_id) {
        /* Active flip is allowed to become idle directly when it becomes unactive again. */
        xwl_present_window->flip_active->flip_idler = TRUE;
        return;
    }

    xorg_list_for_each_entry(vblank, &xwl_present_window->idle_queue, event_queue) {
        if (vblank->event_id == event_id) {
            present_wnmd_free_idle_vblank(vblank);
            return;
        }
    }

    xorg_list_for_each_entry(vblank, &xwl_present_window->flip_queue, event_queue) {
        if (vblank->event_id == event_id) {
            vblank->flip_idler = TRUE;
            return;
        }
    }
}

static Bool
present_wnmd_check_flip(RRCrtcPtr           crtc,
                        WindowPtr           window,
                        PixmapPtr           pixmap,
                        Bool                sync_flip,
                        RegionPtr           valid,
                        int16_t             x_off,
                        int16_t             y_off,
                        PresentFlipReason   *reason)
{
    ScreenPtr               screen = window->drawable.pScreen;
    present_screen_priv_ptr screen_priv = present_screen_priv(screen);
    WindowPtr               toplvl_window = present_wnmd_toplvl_pixmap_window(window);

    if (reason)
        *reason = PRESENT_FLIP_REASON_UNKNOWN;

    if (!screen_priv)
        return FALSE;

    if (!screen_priv->wnmd_info)
        return FALSE;

    if (!crtc)
        return FALSE;

    /* Check to see if the driver supports flips at all */
    if (!screen_priv->wnmd_info->flip)
        return FALSE;

    /* Source pixmap must align with window exactly */
    if (x_off || y_off)
        return FALSE;

    /* Valid area must contain window (for simplicity for now just never flip when one is set). */
    if (valid)
        return FALSE;

    /* Flip pixmap must have same dimensions as window */
    if (window->drawable.width != pixmap->drawable.width ||
            window->drawable.height != pixmap->drawable.height)
        return FALSE;

    /* Window must be same region as toplevel window */
    if ( !RegionEqual(&window->winSize, &toplvl_window->winSize) )
        return FALSE;

    /* Can't flip if window clipped by children */
    if (!RegionEqual(&window->clipList, &window->winSize))
        return FALSE;

    /* Ask the driver for permission */
    if (screen_priv->wnmd_info->check_flip2) {
        if (!(*screen_priv->wnmd_info->check_flip2) (crtc, window, pixmap, sync_flip, reason)) {
            DebugPresent(("\td %08" PRIx32 " -> %08" PRIx32 "\n",
                          window->drawable.id, pixmap ? pixmap->drawable.id : 0));
            return FALSE;
        }
    }

    return TRUE;
}

/*
 * 'window' is being reconfigured. Check to see if it is involved
 * in flipping and clean up as necessary.
 */
static void
present_wnmd_check_flip_window (WindowPtr window)
{
    struct xwl_present_window *xwl_present_window = xwl_present_window_priv(window);
    present_window_priv_ptr window_priv = present_window_priv(window);
    present_vblank_ptr      flip_pending;
    present_vblank_ptr      flip_active;
    present_vblank_ptr      vblank;
    PresentFlipReason       reason;

    /* If this window hasn't ever been used with Present, it can't be
     * flipping
     */
    if (!xwl_present_window || !window_priv)
        return;

    flip_pending = xwl_present_window->flip_pending;
    flip_active = xwl_present_window->flip_active;

    if (flip_pending) {
        if (!present_wnmd_check_flip(flip_pending->crtc, flip_pending->window, flip_pending->pixmap,
                                flip_pending->sync_flip, flip_pending->valid, 0, 0, NULL))
            xwl_present_window->flip_pending->abort_flip = TRUE;
    } else if (flip_active) {
        if (!present_wnmd_check_flip(flip_active->crtc, flip_active->window, flip_active->pixmap,
                                     flip_active->sync_flip, flip_active->valid, 0, 0, NULL))
            present_wnmd_flips_stop(window);
    }

    /* Now check any queued vblanks */
    xorg_list_for_each_entry(vblank, &window_priv->vblank, window_list) {
        if (vblank->queued && vblank->flip &&
                !present_wnmd_check_flip(vblank->crtc, window, vblank->pixmap,
                                         vblank->sync_flip, vblank->valid, 0, 0, &reason)) {
            vblank->flip = FALSE;
            vblank->reason = reason;
        }
    }
}

/*
 * Clean up any pending or current flips for this window
 */
static void
present_wnmd_clear_window_flip(WindowPtr window)
{
    struct xwl_present_window *xwl_present_window = xwl_present_window_get_priv(window);
    present_vblank_ptr          vblank, tmp;

    xorg_list_for_each_entry_safe(vblank, tmp, &xwl_present_window->flip_queue, event_queue) {
        present_pixmap_idle(vblank->pixmap, vblank->window, vblank->serial, vblank->idle_fence);
        present_vblank_destroy(vblank);
    }

    xorg_list_for_each_entry_safe(vblank, tmp, &xwl_present_window->idle_queue, event_queue) {
        present_pixmap_idle(vblank->pixmap, vblank->window, vblank->serial, vblank->idle_fence);
        present_vblank_destroy(vblank);
    }

    vblank = xwl_present_window->flip_active;
    if (vblank) {
        present_pixmap_idle(vblank->pixmap, vblank->window, vblank->serial, vblank->idle_fence);
        present_vblank_destroy(vblank);
    }
    xwl_present_window->flip_active = NULL;
}

static void
present_wnmd_cancel_flip(WindowPtr window)
{
    struct xwl_present_window *xwl_present_window = xwl_present_window_priv(window);

    if (xwl_present_window->flip_pending)
        xwl_present_window->flip_pending->abort_flip = TRUE;
    else if (xwl_present_window->flip_active)
        present_wnmd_flips_stop(window);
}

static void
present_wnmd_update_window_crtc(WindowPtr window, RRCrtcPtr crtc, uint64_t new_msc)
{
    present_window_priv_ptr window_priv = present_get_window_priv(window, TRUE);

    /* Crtc unchanged, no offset. */
    if (crtc == window_priv->crtc)
        return;

    /* No crtc earlier to offset against, just set the crtc. */
    if (window_priv->crtc == PresentCrtcNeverSet) {
        window_priv->msc_offset = 0;
        window_priv->crtc = crtc;
        return;
    }

    /* In window-mode the last correct msc-offset is always kept
     * in window-priv struct because msc is saved per window and
     * not per crtc as in screen-mode.
     */
    window_priv->msc_offset += new_msc - window_priv->msc;
    window_priv->crtc = crtc;
}

static int
present_wnmd_pixmap(WindowPtr window,
                    PixmapPtr pixmap,
                    CARD32 serial,
                    RegionPtr valid,
                    RegionPtr update,
                    int16_t x_off,
                    int16_t y_off,
                    RRCrtcPtr target_crtc,
                    SyncFence *wait_fence,
                    SyncFence *idle_fence,
                    uint32_t options,
                    uint64_t target_window_msc,
                    uint64_t divisor,
                    uint64_t remainder,
                    present_notify_ptr notifies,
                    int num_notifies)
{
    uint64_t                    ust = 0;
    uint64_t                    target_msc;
    uint64_t                    crtc_msc = 0;
    int                         ret;
    present_vblank_ptr          vblank, tmp;
    ScreenPtr                   screen = window->drawable.pScreen;
    struct xwl_present_window *xwl_present_window = xwl_present_window_get_priv(window);
    present_window_priv_ptr     window_priv = present_get_window_priv(window, TRUE);
    present_screen_priv_ptr     screen_priv = present_screen_priv(screen);

    if (!window_priv)
        return BadAlloc;

    target_crtc = present_wnmd_get_crtc(screen_priv, window);

    ret = present_wnmd_get_ust_msc(screen, window, &ust, &crtc_msc);

    present_wnmd_update_window_crtc(window, target_crtc, crtc_msc);

    if (ret == Success) {
        /* Stash the current MSC away in case we need it later
         */
        window_priv->msc = crtc_msc;
    }

    target_msc = present_get_target_msc(target_window_msc + window_priv->msc_offset,
                                        crtc_msc,
                                        divisor,
                                        remainder,
                                        options);

    /*
     * Look for a matching presentation already on the list...
     */

    if (!update && pixmap) {
        xorg_list_for_each_entry_safe(vblank, tmp, &window_priv->vblank, window_list) {

            if (!vblank->pixmap)
                continue;

            if (!vblank->queued)
                continue;

            if (vblank->target_msc != target_msc)
                continue;

            present_vblank_scrap(vblank);
            if (vblank->flip_ready)
                present_wnmd_re_execute(vblank);
        }
    }

    vblank = present_vblank_create(window,
                                   pixmap,
                                   serial,
                                   valid,
                                   update,
                                   x_off,
                                   y_off,
                                   target_crtc,
                                   wait_fence,
                                   idle_fence,
                                   options,
                                   screen_priv->wnmd_info->capabilities,
                                   notifies,
                                   num_notifies,
                                   target_msc,
                                   crtc_msc);
    if (!vblank)
        return BadAlloc;

    vblank->event_id = ++present_wnmd_event_id;

    /* WNMD presentations always complete (at least) one frame after they
     * are executed
     */
    vblank->exec_msc = vblank->target_msc - 1;

    xorg_list_append(&vblank->event_queue, &xwl_present_window->exec_queue);
    vblank->queued = TRUE;
    if (crtc_msc < vblank->exec_msc) {
        if (present_wnmd_queue_vblank(screen, window, target_crtc, vblank->event_id, vblank->exec_msc) == Success) {
            return Success;
        }
        DebugPresent(("present_queue_vblank failed\n"));
    }

    present_wnmd_execute(vblank, ust, crtc_msc);
    return Success;
}


static void
xwl_present_release_pixmap(struct xwl_present_event *event)
{
    if (!event->pixmap)
        return;

    xwl_pixmap_del_buffer_release_cb(event->pixmap);
    dixDestroyPixmap(event->pixmap, event->pixmap->drawable.id);
    event->pixmap = NULL;
}

static void
xwl_present_free_event(struct xwl_present_event *event)
{
    if (!event)
        return;

    xwl_present_release_pixmap(event);
    xorg_list_del(&event->list);
    free(event);
}

void
xwl_present_cleanup(WindowPtr window)
{
    struct xwl_present_window *xwl_present_window = xwl_present_window_priv(window);
    struct xwl_present_event *event, *tmp;

    if (!xwl_present_window)
        return;

    xorg_list_del(&xwl_present_window->frame_callback_list);

    if (xwl_present_window->sync_callback) {
        wl_callback_destroy(xwl_present_window->sync_callback);
        xwl_present_window->sync_callback = NULL;
    }

    /* Clear remaining events */
    xorg_list_for_each_entry_safe(event, tmp, &xwl_present_window->wait_list, list)
        xwl_present_free_event(event);

    xwl_present_free_event(xwl_present_window->sync_flip);

    xorg_list_for_each_entry_safe(event, tmp, &xwl_present_window->release_list, list)
        xwl_present_free_event(event);

    /* Clear timer */
    xwl_present_free_timer(xwl_present_window);

    /* Remove from privates so we don't try to access it later */
    dixSetPrivate(&window->devPrivates,
                  &xwl_present_window_private_key,
                  NULL);

    free(xwl_present_window);
}

static void
xwl_present_buffer_release(void *data)
{
    struct xwl_present_event *event = data;

    if (!event)
        return;

    xwl_present_release_pixmap(event);

    if (!event->abort)
        present_wnmd_idle_notify(event->xwl_present_window->window, event->event_id);

    if (!event->pending)
        xwl_present_free_event(event);
}

static void
xwl_present_msc_bump(struct xwl_present_window *xwl_present_window)
{
    uint64_t msc = ++xwl_present_window->msc;
    struct xwl_present_event    *event, *tmp;

    xwl_present_window->ust = GetTimeInMicros();

    event = xwl_present_window->sync_flip;
    xwl_present_window->sync_flip = NULL;
    if (event) {
        event->pending = FALSE;

        present_wnmd_flip_notify(xwl_present_window->window, event->event_id,
                                 xwl_present_window->ust, msc);

        if (!event->pixmap) {
            /* If the buffer was already released, clean up now */
            xwl_present_free_event(event);
        } else {
            xorg_list_add(&event->list, &xwl_present_window->release_list);
        }
    }

    xorg_list_for_each_entry_safe(event, tmp,
                                  &xwl_present_window->wait_list,
                                  list) {
        if (event->target_msc <= msc) {
            present_wnmd_event_notify(xwl_present_window->window,
                                      event->event_id,
                                      xwl_present_window->ust,
                                      msc);
            xwl_present_free_event(event);
        }
    }
}

static CARD32
xwl_present_timer_callback(OsTimerPtr timer,
                           CARD32 time,
                           void *arg)
{
    struct xwl_present_window *xwl_present_window = arg;

    /* If we were expecting a frame callback for this window, it didn't arrive
     * in a second. Stop listening to it to avoid double-bumping the MSC
     */
    xorg_list_del(&xwl_present_window->frame_callback_list);

    xwl_present_msc_bump(xwl_present_window);
    xwl_present_reset_timer(xwl_present_window);

    return 0;
}

void
xwl_present_frame_callback(struct xwl_present_window *xwl_present_window)
{
    xorg_list_del(&xwl_present_window->frame_callback_list);

    xwl_present_msc_bump(xwl_present_window);

    /* we do not need the timer anymore for this frame,
     * reset it for potentially the next one
     */
    xwl_present_reset_timer(xwl_present_window);
}

static void
xwl_present_sync_callback(void *data,
               struct wl_callback *callback,
               uint32_t time)
{
    struct xwl_present_event *event = data;
    struct xwl_present_window *xwl_present_window = event->xwl_present_window;

    wl_callback_destroy(xwl_present_window->sync_callback);
    xwl_present_window->sync_callback = NULL;

    event->pending = FALSE;

    if (!event->abort)
        present_wnmd_flip_notify(xwl_present_window->window, event->event_id,
                                 xwl_present_window->ust, xwl_present_window->msc);

    if (!event->pixmap)
        xwl_present_free_event(event);
}

static const struct wl_callback_listener xwl_present_sync_listener = {
    xwl_present_sync_callback
};

static RRCrtcPtr
xwl_present_get_crtc(WindowPtr present_window)
{
    struct xwl_present_window *xwl_present_window = xwl_present_window_get_priv(present_window);
    rrScrPrivPtr rr_private;

    if (xwl_present_window == NULL)
        return NULL;

    rr_private = rrGetScrPriv(present_window->drawable.pScreen);

    if (rr_private->numCrtcs == 0)
        return NULL;

    return rr_private->crtcs[0];
}

static int
xwl_present_get_ust_msc(WindowPtr present_window, uint64_t *ust, uint64_t *msc)
{
    struct xwl_present_window *xwl_present_window = xwl_present_window_get_priv(present_window);
    if (!xwl_present_window)
        return BadAlloc;

    *ust = xwl_present_window->ust;
    *msc = xwl_present_window->msc;

    return Success;
}

/*
 * Queue an event to report back to the Present extension when the specified
 * MSC has past
 */
static int
xwl_present_queue_vblank(WindowPtr present_window,
                         RRCrtcPtr crtc,
                         uint64_t event_id,
                         uint64_t msc)
{
    struct xwl_present_window *xwl_present_window = xwl_present_window_get_priv(present_window);
    struct xwl_window *xwl_window = xwl_window_from_window(present_window);
    struct xwl_present_event *event;

    event = malloc(sizeof *event);
    if (!event)
        return BadAlloc;

    event->event_id = event_id;
    event->pixmap = NULL;
    event->xwl_present_window = xwl_present_window;
    event->target_msc = msc;

    xorg_list_append(&event->list, &xwl_present_window->wait_list);

    /* If there's a pending frame callback, use that */
    if (xwl_window && xwl_window->frame_callback &&
        xorg_list_is_empty(&xwl_present_window->frame_callback_list)) {
        xorg_list_add(&xwl_present_window->frame_callback_list,
                      &xwl_window->frame_callback_list);
    }

    if ((xwl_window && xwl_window->frame_callback) ||
        !xwl_present_window->frame_timer)
        xwl_present_reset_timer(xwl_present_window);

    return Success;
}

/*
 * Remove a pending vblank event so that it is not reported
 * to the extension
 */
static void
xwl_present_abort_vblank(ScreenPtr screen,
                         WindowPtr present_window,
                         RRCrtcPtr crtc,
                         uint64_t event_id,
                         uint64_t msc)
{
    struct xwl_present_window *xwl_present_window = xwl_present_window_priv(present_window);
    struct xwl_present_event *event, *tmp;
    present_vblank_ptr vblank;

    if (xwl_present_window) {
        xorg_list_for_each_entry_safe(event, tmp, &xwl_present_window->wait_list, list) {
            if (event->event_id == event_id) {
                xwl_present_free_event(event);
                break;
            }
        }

        xorg_list_for_each_entry(event, &xwl_present_window->release_list, list) {
            if (event->event_id == event_id) {
                event->abort = TRUE;
                break;
            }
        }
    }

    xorg_list_for_each_entry(vblank, &xwl_present_window->exec_queue, event_queue) {
        if (vblank->event_id == event_id) {
            xorg_list_del(&vblank->event_queue);
            vblank->queued = FALSE;
            return;
        }
    }
    xorg_list_for_each_entry(vblank, &xwl_present_window->flip_queue, event_queue) {
        if (vblank->event_id == event_id) {
            xorg_list_del(&vblank->event_queue);
            vblank->queued = FALSE;
            return;
        }
    }
}

static void
xwl_present_flush(WindowPtr window)
{
    glamor_block_handler(window->drawable.pScreen);
}

static Bool
xwl_present_check_flip2(RRCrtcPtr crtc,
                        WindowPtr present_window,
                        PixmapPtr pixmap,
                        Bool sync_flip,
                        PresentFlipReason *reason)
{
    struct xwl_window *xwl_window = xwl_window_from_window(present_window);
    ScreenPtr screen = pixmap->drawable.pScreen;

    if (!xwl_window)
        return FALSE;

    if (!xwl_glamor_check_flip(pixmap))
        return FALSE;

    /* Can't flip if the window pixmap doesn't match the xwl_window parent
     * window's, e.g. because a client redirected this window or one of its
     * parents.
     */
    if (screen->GetWindowPixmap(xwl_window->window) != screen->GetWindowPixmap(present_window))
        return FALSE;

    /*
     * We currently only allow flips of windows, that have the same
     * dimensions as their xwl_window parent window. For the case of
     * different sizes subsurfaces are presumably the way forward.
     */
    if (!RegionEqual(&xwl_window->window->winSize, &present_window->winSize))
        return FALSE;

    return TRUE;
}

static Bool
xwl_present_flip(WindowPtr present_window,
                 RRCrtcPtr crtc,
                 uint64_t event_id,
                 uint64_t target_msc,
                 PixmapPtr pixmap,
                 Bool sync_flip,
                 RegionPtr damage)
{
    struct xwl_window           *xwl_window = xwl_window_from_window(present_window);
    struct xwl_present_window   *xwl_present_window = xwl_present_window_priv(present_window);
    BoxPtr                      damage_box;
    struct wl_buffer            *buffer;
    struct xwl_present_event    *event;

    if (!xwl_window)
        return FALSE;

    buffer = xwl_glamor_pixmap_get_wl_buffer(pixmap);
    if (!buffer) {
        ErrorF("present: Error getting buffer\n");
        return FALSE;
    }

    damage_box = RegionExtents(damage);

    event = malloc(sizeof *event);
    if (!event)
        return FALSE;

    pixmap->refcnt++;

    event->event_id = event_id;
    event->xwl_present_window = xwl_present_window;
    event->pixmap = pixmap;
    event->target_msc = target_msc;
    event->pending = TRUE;
    event->abort = FALSE;

    if (sync_flip) {
        xorg_list_init(&event->list);
        xwl_present_window->sync_flip = event;
    } else {
        xorg_list_add(&event->list, &xwl_present_window->release_list);
    }

    xwl_pixmap_set_buffer_release_cb(pixmap, xwl_present_buffer_release, event);

    /* We can flip directly to the main surface (full screen window without clips) */
    wl_surface_attach(xwl_window->surface, buffer, 0, 0);

    if (!xwl_window->frame_callback)
        xwl_window_create_frame_callback(xwl_window);

    if (xorg_list_is_empty(&xwl_present_window->frame_callback_list)) {
        xorg_list_add(&xwl_present_window->frame_callback_list,
                      &xwl_window->frame_callback_list);
    }

    /* Realign timer */
    xwl_present_reset_timer(xwl_present_window);

    xwl_surface_damage(xwl_window->xwl_screen, xwl_window->surface,
                       damage_box->x1 - present_window->drawable.x,
                       damage_box->y1 - present_window->drawable.y,
                       damage_box->x2 - damage_box->x1,
                       damage_box->y2 - damage_box->y1);

    wl_surface_commit(xwl_window->surface);

    if (!sync_flip) {
        xwl_present_window->sync_callback =
            wl_display_sync(xwl_window->xwl_screen->display);
        wl_callback_add_listener(xwl_present_window->sync_callback,
                                 &xwl_present_sync_listener,
                                 event);
    }

    wl_display_flush(xwl_window->xwl_screen->display);
    xwl_window->present_flipped = TRUE;
    return TRUE;
}

static void
xwl_present_flips_stop(WindowPtr window)
{
    struct xwl_present_window   *xwl_present_window = xwl_present_window_priv(window);

    /* Change back to the fast refresh rate */
    xwl_present_reset_timer(xwl_present_window);
}

/*
 * Once the required MSC has been reached, execute the pending request.
 *
 * For requests to actually present something, either blt contents to
 * the window pixmap or queue a window buffer swap on the backend.
 *
 * For requests to just get the current MSC/UST combo, skip that part and
 * go straight to event delivery.
 */
static void
present_wnmd_execute(present_vblank_ptr vblank, uint64_t ust, uint64_t crtc_msc)
{
    WindowPtr               window = vblank->window;
    struct xwl_present_window *xwl_present_window = xwl_present_window_get_priv(window);
    present_window_priv_ptr window_priv = present_window_priv(window);

    if (present_execute_wait(vblank, crtc_msc))
        return;

    if (vblank->flip && vblank->pixmap && vblank->window) {
        if (xwl_present_window->flip_pending) {
            DebugPresent(("\tr %" PRIu64 " %p (pending %p)\n",
                          vblank->event_id, vblank,
                          xwl_present_window->flip_pending));
            xorg_list_del(&vblank->event_queue);
            xorg_list_append(&vblank->event_queue, &xwl_present_window->flip_queue);
            vblank->flip_ready = TRUE;
            return;
        }
    }

    xorg_list_del(&vblank->event_queue);
    xorg_list_del(&vblank->window_list);
    vblank->queued = FALSE;

    if (vblank->pixmap && vblank->window) {
        ScreenPtr screen = window->drawable.pScreen;

        if (vblank->flip) {
            RegionPtr damage;

            DebugPresent(("\tf %" PRIu64 " %p %" PRIu64 ": %08" PRIx32 " -> %08" PRIx32 "\n",
                          vblank->event_id, vblank, crtc_msc,
                          vblank->pixmap->drawable.id, vblank->window->drawable.id));

            /* Prepare to flip by placing it in the flip queue
             */
            xorg_list_add(&vblank->event_queue, &xwl_present_window->flip_queue);

            /* Set update region as damaged */
            if (vblank->update) {
                damage = RegionDuplicate(vblank->update);
                /* Translate update region to screen space */
                assert(vblank->x_off == 0 && vblank->y_off == 0);
                RegionTranslate(damage, window->drawable.x, window->drawable.y);
                RegionIntersect(damage, damage, &window->clipList);
            } else
                damage = RegionDuplicate(&window->clipList);

            /* Try to flip - the vblank is now pending
             */
            xwl_present_window->flip_pending = vblank;
            if (xwl_present_flip(vblank->window, vblank->crtc, vblank->event_id,
                                 vblank->target_msc, vblank->pixmap, vblank->sync_flip, damage)) {
                WindowPtr toplvl_window = present_wnmd_toplvl_pixmap_window(vblank->window);
                PixmapPtr old_pixmap = screen->GetWindowPixmap(window);

                /* Replace window pixmap with flip pixmap */
#ifdef COMPOSITE
                vblank->pixmap->screen_x = old_pixmap->screen_x;
                vblank->pixmap->screen_y = old_pixmap->screen_y;
#endif
                present_set_tree_pixmap(toplvl_window, old_pixmap, vblank->pixmap);
                vblank->pixmap->refcnt++;
                dixDestroyPixmap(old_pixmap, old_pixmap->drawable.id);

                /* Report damage */
                DamageDamageRegion(&vblank->window->drawable, damage);
                RegionDestroy(damage);
                return;
            }

            xorg_list_del(&vblank->event_queue);
            /* Flip failed. Clear the flip_pending field
              */
            xwl_present_window->flip_pending = NULL;
            vblank->flip = FALSE;
        }
        DebugPresent(("\tc %p %" PRIu64 ": %08" PRIx32 " -> %08" PRIx32 "\n",
                      vblank, crtc_msc, vblank->pixmap->drawable.id, vblank->window->drawable.id));

        present_wnmd_cancel_flip(window);

        present_execute_copy(vblank, crtc_msc);
        assert(!vblank->queued);

        if (present_wnmd_queue_vblank(screen, window, vblank->crtc,
                                      vblank->event_id, crtc_msc + 1)
            == Success) {
            xorg_list_add(&vblank->event_queue, &xwl_present_window->idle_queue);
            xorg_list_append(&vblank->window_list, &window_priv->vblank);

            return;
        }
    }

    present_execute_post(vblank, ust, crtc_msc);
}

void
xwl_present_unrealize_window(struct xwl_present_window *xwl_present_window)
{
    /* The pending frame callback may never be called, so drop it and shorten
     * the frame timer interval.
     */
    xorg_list_del(&xwl_present_window->frame_callback_list);
    xwl_present_reset_timer(xwl_present_window);
}

static present_wnmd_info_rec xwl_present_info = {
    .version = PRESENT_SCREEN_INFO_VERSION,
    .get_crtc = xwl_present_get_crtc,

    .get_ust_msc = xwl_present_get_ust_msc,
    .queue_vblank = xwl_present_queue_vblank,

    .capabilities = PresentCapabilityAsync,
    .check_flip2 = xwl_present_check_flip2,
    .flips_stop = xwl_present_flips_stop
};

Bool
xwl_present_init(ScreenPtr screen)
{
    struct xwl_screen *xwl_screen = xwl_screen_get(screen);
    present_screen_priv_ptr screen_priv;

    if (!xwl_screen->glamor || !xwl_screen->egl_backend)
        return FALSE;

    if (!present_screen_register_priv_keys())
        return FALSE;

    if (present_screen_priv(screen))
        return TRUE;

    screen_priv = present_screen_priv_init(screen);
    if (!screen_priv)
        return FALSE;

    if (!dixRegisterPrivateKey(&xwl_present_window_private_key, PRIVATE_WINDOW, 0))
        return FALSE;

    screen_priv->wnmd_info = &xwl_present_info;

    screen_priv->query_capabilities = present_wnmd_query_capabilities;
    screen_priv->get_crtc = present_wnmd_get_crtc;

    screen_priv->check_flip = present_wnmd_check_flip;
    screen_priv->check_flip_window = present_wnmd_check_flip_window;
    screen_priv->clear_window_flip = present_wnmd_clear_window_flip;

    screen_priv->present_pixmap = present_wnmd_pixmap;
    screen_priv->queue_vblank = present_wnmd_queue_vblank;
    screen_priv->flush = xwl_present_flush;
    screen_priv->re_execute = present_wnmd_re_execute;

    screen_priv->abort_vblank = xwl_present_abort_vblank;

    return TRUE;
}
