/*
 * Common rootless definitions and code
 */
/*
 * Copyright (c) 2001 Greg Parker. All Rights Reserved.
 * Copyright (c) 2002-2003 Torrey T. Lyons. All Rights Reserved.
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written authorization.
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <stddef.h>             /* For NULL */
#include <limits.h>             /* For CHAR_BIT */

#include "rootlessCommon.h"
#include "colormapst.h"

unsigned int rootless_CopyBytes_threshold = 0;
unsigned int rootless_CopyWindow_threshold = 0;
int rootlessGlobalOffsetX = 0;
int rootlessGlobalOffsetY = 0;

RegionRec rootlessHugeRoot = { {-32767, -32767, 32767, 32767}, NULL };

/* Following macro from miregion.c */

/*  true iff two Boxes overlap */
#define EXTENTCHECK(r1,r2) \
      (!( ((r1)->x2 <= (r2)->x1)  || \
          ((r1)->x1 >= (r2)->x2)  || \
          ((r1)->y2 <= (r2)->y1)  || \
          ((r1)->y1 >= (r2)->y2) ) )

/*
 * TopLevelParent
 *  Returns the top-level parent of pWindow.
 *  The root is the top-level parent of itself, even though the root is
 *  not otherwise considered to be a top-level window.
 */
WindowPtr
TopLevelParent(WindowPtr pWindow)
{
    WindowPtr top;

    if (IsRoot(pWindow))
        return pWindow;

    top = pWindow;
    while (top && !IsTopLevel(top))
        top = top->parent;

    return top;
}

/*
 * IsFramedWindow
 *  Returns TRUE if this window is visible inside a frame
 *  (e.g. it is visible and has a top-level or root parent)
 */
Bool
IsFramedWindow(WindowPtr pWin)
{
    WindowPtr top;

    if (!dixPrivateKeyRegistered(&rootlessWindowPrivateKeyRec))
        return FALSE;

    if (!pWin->realized)
        return FALSE;
    top = TopLevelParent(pWin);

    return (top && WINREC(top));
}

/*
 * RootlessWindowIsScreenBacked
 *  Returns TRUE if pWin draws into the screen pixmap.  Framed and redirected
 *  windows have buffers of their own; anything else still points at the screen
 *  pixmap fbCreateWindow installed.  Note that comparing pixmaps would not
 *  answer this: framed windows hold the screen pixmap while not mid-draw.
 */
Bool
RootlessWindowIsScreenBacked(WindowPtr pWin)
{
    WindowPtr top;

#ifdef COMPOSITE
    if (pWin->redirectDraw != RedirectDrawNone)
        return FALSE;
#endif

    if (!dixPrivateKeyRegistered(&rootlessWindowPrivateKeyRec))
        return FALSE;

    /* Not IsFramedWindow(): an unrealized window still has its frame. */
    top = TopLevelParent(pWin);

    return (top == NULL || WINREC(top) == NULL);
}

/*
 * RootlessGetScreenPixmapBox
 *  Store the screen pixmap's extent in pBox, in the coordinates fb resolves
 *  against.  Returns FALSE if there is no screen pixmap, or it is empty.
 */
Bool
RootlessGetScreenPixmapBox(ScreenPtr pScreen, BoxPtr pBox)
{
    PixmapPtr pPix = (*pScreen->GetScreenPixmap) (pScreen);

    if (pPix == NULL)
        return FALSE;

    pBox->x1 = pPix->screen_x;
    pBox->y1 = pPix->screen_y;
    pBox->x2 = pPix->screen_x + pPix->drawable.width;
    pBox->y2 = pPix->screen_y + pPix->drawable.height;

    return (pBox->x2 > pBox->x1 && pBox->y2 > pBox->y1);
}

/*
 * RootlessBoundRegionToScreenPixmap
 *  Restrict pRegion to what a window drawing into the screen pixmap may reach.
 *  That pixmap is one scanline, and fb bounds its addresses by the clip alone,
 *  so an unbounded clip indexes outside the allocation in either direction.
 */
void
RootlessBoundRegionToScreenPixmap(WindowPtr pWin, RegionPtr pRegion)
{
    RegionRec bound;
    BoxRec box;

    if (!RootlessWindowIsScreenBacked(pWin))
        return;

    /* Nothing to bound against, so draw nothing. */
    if (!RootlessGetScreenPixmapBox(pWin->drawable.pScreen, &box)) {
        RegionEmpty(pRegion);
        return;
    }

    RegionInit(&bound, &box, 1);
    RegionIntersect(pRegion, pRegion, &bound);
    RegionUninit(&bound);
}

Bool
RootlessResolveColormap(ScreenPtr pScreen, int first_color,
                        int n_colors, uint32_t * colors)
{
    int last, i;
    ColormapPtr map;

    map = RootlessGetColormap(pScreen);
    if (map == NULL || map->class != PseudoColor)
        return FALSE;

    last = min(map->pVisual->ColormapEntries, first_color + n_colors);
    for (i = max(0, first_color); i < last; i++) {
        Entry *ent = map->red + i;
        uint16_t red, green, blue;

        if (!ent->refcnt)
            continue;
        if (ent->fShared) {
            red = ent->co.shco.red->color;
            green = ent->co.shco.green->color;
            blue = ent->co.shco.blue->color;
        }
        else {
            red = ent->co.local.red;
            green = ent->co.local.green;
            blue = ent->co.local.blue;
        }

        colors[i - first_color] = (0xFF000000UL
                                   | ((uint32_t) red & 0xff00) << 8
                                   | (green & 0xff00)
                                   | (blue >> 8));
    }

    return TRUE;
}

unsigned long RootlessWID(WindowPtr pWindow) {
    WindowPtr top = TopLevelParent(pWindow);
    RootlessWindowRec *winRec;

    if (top == NULL) {
        return 0;
    }
    winRec = WINREC(top);
    if (winRec == NULL) {
        return 0;
    }

    return (unsigned long)(uintptr_t)winRec->wid;
}

/*
 * RootlessStartDrawing
 *  Prepare a window for direct access to its backing buffer.
 *  Each top-level parent has a Pixmap representing its backing buffer,
 *  which all of its children inherit.
 */
void
RootlessStartDrawing(WindowPtr pWindow)
{
    ScreenPtr pScreen = pWindow->drawable.pScreen;
    WindowPtr top = TopLevelParent(pWindow);
    RootlessWindowRec *winRec;
    PixmapPtr curPixmap;

    if (top == NULL) {
        RL_DEBUG_MSG("RootlessStartDrawing is a no-op because top == NULL.\n");
        return;
    }
    winRec = WINREC(top);
    if (winRec == NULL) {
        RL_DEBUG_MSG("RootlessStartDrawing is a no-op because winRec == NULL.\n");
        return;
    }

    // Make sure the window's top-level parent is prepared for drawing.
    if (!winRec->is_drawing) {
        int bw = wBorderWidth(top);

        SCREENREC(pScreen)->imp->StartDrawing(winRec->wid, &winRec->pixelData,
                                              &winRec->bytesPerRow);

        winRec->pixmap =
            GetScratchPixmapHeader(pScreen, winRec->width, winRec->height,
                                   top->drawable.depth,
                                   top->drawable.bitsPerPixel,
                                   winRec->bytesPerRow, winRec->pixelData);

        RL_DEBUG_MSG("GetScratchPixmapHeader gave us %p %p (%d,%d %dx%d %d) for wid=%lu\n",
                     winRec->pixmap, winRec->pixmap->devPrivate.ptr, winRec->pixmap->drawable.x,
                     winRec->pixmap->drawable.y, winRec->pixmap->drawable.width, winRec->pixmap->drawable.height,
                     winRec->pixmap->drawable.bitsPerPixel, RootlessWID(pWindow));

        SetPixmapBaseToScreen(winRec->pixmap,
                              top->drawable.x - bw, top->drawable.y - bw);

        RL_DEBUG_MSG("After SetPixmapBaseToScreen(%d %d %d): %p (%d,%d %dx%d %d) for wid=%lu\n",
                     top->drawable.x, top->drawable.y, bw, winRec->pixmap->devPrivate.ptr, winRec->pixmap->drawable.x,
                     winRec->pixmap->drawable.y, winRec->pixmap->drawable.width, winRec->pixmap->drawable.height,
                     winRec->pixmap->drawable.bitsPerPixel, RootlessWID(pWindow));

        winRec->is_drawing = TRUE;
    } else {
        RL_DEBUG_MSG("Skipped call to xprStartDrawing (wid: %lu) because winRec->is_drawing says we already did.\n", RootlessWID(pWindow));
    }

    curPixmap = pScreen->GetWindowPixmap(pWindow);
    if (curPixmap == winRec->pixmap) {
        RL_DEBUG_MSG("Window %p already has winRec->pixmap %p; not pushing\n",
                     pWindow, winRec->pixmap);
    }
    else {
        PixmapPtr oldPixmap =
            dixLookupPrivate(&pWindow->devPrivates,
                             rootlessWindowOldPixmapPrivateKey);

        RL_DEBUG_MSG("curPixmap is %p %p for wid=%lu\n", curPixmap, curPixmap ? curPixmap->devPrivate.ptr : NULL, RootlessWID(pWindow));
        RL_DEBUG_MSG("oldPixmap is %p %p for wid=%lu\n", oldPixmap, oldPixmap ? oldPixmap->devPrivate.ptr : NULL, RootlessWID(pWindow));

        if (oldPixmap != NULL) {
            if (oldPixmap == curPixmap)
                RL_DEBUG_MSG
                    ("Window %p's curPixmap %p is the same as its oldPixmap; strange\n",
                     pWindow, curPixmap);
            else
                RL_DEBUG_MSG("Window %p's existing oldPixmap %p being lost!\n",
                             pWindow, oldPixmap);
        }
        dixSetPrivate(&pWindow->devPrivates, rootlessWindowOldPixmapPrivateKey,
                      curPixmap);
        pScreen->SetWindowPixmap(pWindow, winRec->pixmap);
    }
}

static int
RestorePreDrawingPixmapVisitor(WindowPtr pWindow, void *data)
{
    RootlessWindowRec *winRec = (RootlessWindowRec *) data;
    ScreenPtr pScreen = pWindow->drawable.pScreen;
    PixmapPtr exPixmap = pScreen->GetWindowPixmap(pWindow);
    PixmapPtr oldPixmap =
        dixLookupPrivate(&pWindow->devPrivates,
                         rootlessWindowOldPixmapPrivateKey);
    if (oldPixmap == NULL) {
        if (exPixmap == winRec->pixmap)
            RL_DEBUG_MSG
                ("Window %p appears to be in drawing mode (ex-pixmap %p equals winRec->pixmap, which is being freed) but has no oldPixmap!\n",
                 pWindow, exPixmap);
    }
    else {
        if (exPixmap != winRec->pixmap)
            RL_DEBUG_MSG
                ("Window %p appears to be in drawing mode (oldPixmap %p) but ex-pixmap %p not winRec->pixmap %p!\n",
                 pWindow, oldPixmap, exPixmap, winRec->pixmap);
        if (oldPixmap == winRec->pixmap)
            RL_DEBUG_MSG
                ("Window %p's oldPixmap %p is winRec->pixmap, which has just been freed!\n",
                 pWindow, oldPixmap);
        pScreen->SetWindowPixmap(pWindow, oldPixmap);
        dixSetPrivate(&pWindow->devPrivates, rootlessWindowOldPixmapPrivateKey,
                      NULL);
    }
    return WT_WALKCHILDREN;
}

/*
 * RootlessStopDrawingFrame
 *  Stop drawing to the given frame's backing buffer. If flush is true, damaged regions are flushed to the screen.
 *  winRec must not be NULL.
 *
 *  Unlike RootlessStopDrawing(), the frame is named directly instead of being found by walking up from a window.
 *  Callers need that once a framed window has been reparented below another framed window, because TopLevelParent()
 *  then resolves either to a different frame or to no frame at all rather than to the one this record describes.
 *
 *  This deliberately does not process is_reorder_pending; see RootlessStopDrawing().
 */
void
RootlessStopDrawingFrame(RootlessWindowPtr winRec, Bool flush)
{
    WindowPtr pWin = winRec->win;
    ScreenPtr pScreen = pWin->drawable.pScreen;

    if (winRec->is_drawing) {
        SCREENREC(pScreen)->imp->StopDrawing(winRec->wid, flush);

        FreeScratchPixmapHeader(winRec->pixmap);
        TraverseTree(pWin, RestorePreDrawingPixmapVisitor, (void *) winRec);
        winRec->pixmap = NULL;

        winRec->is_drawing = FALSE;
    }
    else if (flush) {
        SCREENREC(pScreen)->imp->UpdateRegion(winRec->wid, NULL);
    }
}

/*
 * RootlessStopDrawing
 *  Stop drawing to a window's backing buffer. If flush is true,
 *  damaged regions are flushed to the screen.
 */
void
RootlessStopDrawing(WindowPtr pWindow, Bool flush)
{
    WindowPtr top = TopLevelParent(pWindow);
    RootlessWindowRec *winRec;

    if (top == NULL)
        return;
    winRec = WINREC(top);
    if (winRec == NULL)
        return;

    RootlessStopDrawingFrame(winRec, flush);

    /* Reorder the window the caller named rather than the frame's owner.  Clearing the flag consumes it either way,
     * but RootlessReorderWindow() itself does nothing unless pWindow is the window that owns the frame.  This is
     * long-standing behavior; every caller that passes flush reorders a frame-owning window.
     */
    if (flush && winRec->is_reorder_pending) {
        winRec->is_reorder_pending = FALSE;
        RootlessReorderWindow(pWindow);
    }
}

/*
 * RootlessDamageRegion
 *  Mark a damaged region as requiring redisplay to screen.
 *  pRegion is in GLOBAL coordinates.
 */
void
RootlessDamageRegion(WindowPtr pWindow, RegionPtr pRegion)
{
    RootlessWindowRec *winRec;
    RegionRec clipped;
    WindowPtr pTop;
    BoxPtr b1, b2;

    RL_DEBUG_MSG("Damaged win %p\n", pWindow);

    pTop = TopLevelParent(pWindow);
    if (pTop == NULL)
        return;

    winRec = WINREC(pTop);
    if (winRec == NULL)
        return;

    /* We need to intersect the drawn region with the clip of the window
       to avoid marking places we didn't actually draw (which can cause
       problems when the window has an extra client-side backing store)

       But this is a costly operation and since we'll normally just be
       drawing inside the clip, go to some lengths to avoid the general
       case intersection. */

    b1 = RegionExtents(&pWindow->borderClip);
    b2 = RegionExtents(pRegion);

    if (EXTENTCHECK(b1, b2)) {
        /* Regions may overlap. */

        if (RegionNumRects(pRegion) == 1) {
            int in;

            /* Damaged region only has a single rect, so we can
               just compare that against the region */

            in = RegionContainsRect(&pWindow->borderClip, RegionRects(pRegion));
            if (in == rgnIN) {
                /* clip totally contains pRegion */

                SCREENREC(pWindow->drawable.pScreen)->imp->DamageRects(winRec->
                                                                       wid,
                                                                       RegionNumRects
                                                                       (pRegion),
                                                                       RegionRects
                                                                       (pRegion),
                                                                       -winRec->
                                                                       x,
                                                                       -winRec->
                                                                       y);

                RootlessQueueRedisplay(pTop->drawable.pScreen);
                goto out;
            }
            else if (in == rgnOUT) {
                /* clip doesn't contain pRegion */

                goto out;
            }
        }

        /* clip overlaps pRegion, need to intersect */

        RegionNull(&clipped);
        RegionIntersect(&clipped, &pWindow->borderClip, pRegion);

        SCREENREC(pWindow->drawable.pScreen)->imp->DamageRects(winRec->wid,
                                                               RegionNumRects
                                                               (&clipped),
                                                               RegionRects
                                                               (&clipped),
                                                               -winRec->x,
                                                               -winRec->y);

        RegionUninit(&clipped);

        RootlessQueueRedisplay(pTop->drawable.pScreen);
    }

 out:
#ifdef ROOTLESSDEBUG
    {
        BoxRec *box = RegionRects(pRegion), *end;
        int numBox = RegionNumRects(pRegion);

        for (end = box + numBox; box < end; box++) {
            RL_DEBUG_MSG("Damage rect: %i, %i, %i, %i\n",
                         box->x1, box->x2, box->y1, box->y2);
        }
    }
#endif
    return;
}

/*
 * RootlessDamageBox
 *  Mark a damaged box as requiring redisplay to screen.
 *  pRegion is in GLOBAL coordinates.
 */
void
RootlessDamageBox(WindowPtr pWindow, BoxPtr pBox)
{
    RegionRec region;

    RegionInit(&region, pBox, 1);

    RootlessDamageRegion(pWindow, &region);

    RegionUninit(&region);      /* no-op */
}

/*
 * RootlessDamageRect
 *  Mark a damaged rectangle as requiring redisplay to screen.
 *  (x, y, w, h) is in window-local coordinates.
 */
void
RootlessDamageRect(WindowPtr pWindow, int x, int y, int w, int h)
{
    BoxRec box;
    RegionRec region;

    x += pWindow->drawable.x;
    y += pWindow->drawable.y;

    box.x1 = x;
    box.x2 = x + w;
    box.y1 = y;
    box.y2 = y + h;

    RegionInit(&region, &box, 1);

    RootlessDamageRegion(pWindow, &region);

    RegionUninit(&region);      /* no-op */
}

/*
 * RootlessRedisplay
 *  Stop drawing and redisplay the damaged region of a window.
 */
void
RootlessRedisplay(WindowPtr pWindow)
{
    RootlessStopDrawing(pWindow, TRUE);
}

/*
 * RootlessRepositionWindows
 *  Reposition all windows on a screen to their correct positions.
 */
void
RootlessRepositionWindows(ScreenPtr pScreen)
{
    WindowPtr root = pScreen->root;
    WindowPtr win;

    if (root != NULL) {
        RootlessRepositionWindow(root);

        for (win = root->firstChild; win; win = win->nextSib) {
            if (WINREC(win) != NULL)
                RootlessRepositionWindow(win);
        }
    }
}

/*
 * RootlessRedisplayScreen
 *  Walk every window on a screen and redisplay the damaged regions.
 */
void
RootlessRedisplayScreen(ScreenPtr pScreen)
{
    WindowPtr root = pScreen->root;

    if (root != NULL) {
        WindowPtr win;

        RootlessRedisplay(root);
        for (win = root->firstChild; win; win = win->nextSib) {
            if (WINREC(win) != NULL) {
                RootlessRedisplay(win);
            }
        }
    }
}
