/*
 * Copyright © 2011 Collabra Ltd.
 * Copyright © 2011 Red Hat, Inc.
 * Copyright © 2020 Povilas Kanapickas  <povilas@radix.lt>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include "inputstr.h"
#include "scrnintstr.h"
#include "dixgrabs.h"

#include "eventstr.h"
#include "exevents.h"
#include "exglobals.h"
#include "inpututils.h"
#include "eventconvert.h"
#include "windowstr.h"
#include "mi.h"

Bool
GestureInitGestureInfo(GestureInfoPtr gi)
{
    memset(gi, 0, sizeof(*gi));

    gi->sprite.spriteTrace = calloc(32, sizeof(*gi->sprite.spriteTrace));
    if (!gi->sprite.spriteTrace) {
        return FALSE;
    }
    gi->sprite.spriteTraceSize = 32;
    gi->sprite.spriteTrace[0] = screenInfo.screens[0]->root;
    gi->sprite.hot.pScreen = screenInfo.screens[0];
    gi->sprite.hotPhys.pScreen = screenInfo.screens[0];

    return TRUE;
}
