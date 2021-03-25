/*
 * Copyright 2005-2006 Luc Verhaegen.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_XORG_CONFIG_H
#include <xorg-config.h>
#endif

#include "xf86.h"
#include "xf86Modes.h"

#include <string.h>
#include <libxcvt/libxcvt.h>

/*
 * Generate a CVT standard mode from HDisplay, VDisplay and VRefresh.
 *
 * These calculations are stolen from the CVT calculation spreadsheet written
 * by Graham Loveridge. He seems to be claiming no copyright and there seems to
 * be no license attached to this. He apparently just wants to see his name
 * mentioned.
 *
 * This file can be found at http://www.vesa.org/Public/CVT/CVTd6r1.xls
 *
 * Comments and structure corresponds to the comments and structure of the xls.
 * This should ease importing of future changes to the standard (not very
 * likely though).
 *
 * About margins; i'm sure that they are to be the bit between HDisplay and
 * HBlankStart, HBlankEnd and HTotal, VDisplay and VBlankStart, VBlankEnd and
 * VTotal, where the overscan colour is shown. FB seems to call _all_ blanking
 * outside sync "margin" for some reason. Since we prefer seeing proper
 * blanking instead of the overscan colour, and since the Crtc* values will
 * probably get altered after us, we will disable margins altogether. With
 * these calculations, Margins will plainly expand H/VDisplay, and we don't
 * want that. -- libv
 *
 */
DisplayModePtr
xf86CVTMode(int HDisplay, int VDisplay, float VRefresh, Bool Reduced,
            Bool Interlaced)
{
    struct libxcvt_mode_info *libxcvt_mode_info;
    DisplayModeRec *Mode = xnfcalloc(1, sizeof(DisplayModeRec));

    libxcvt_mode_info =
        libxcvt_gen_mode_info(HDisplay, VDisplay, VRefresh, Reduced, Interlaced);

    Mode->VDisplay   = libxcvt_mode_info->vdisplay;
    Mode->HDisplay   = libxcvt_mode_info->hdisplay;
    Mode->Clock      = libxcvt_mode_info->dot_clock;
    Mode->HSyncStart = libxcvt_mode_info->hsync_start;
    Mode->HSyncEnd   = libxcvt_mode_info->hsync_end;
    Mode->HTotal     = libxcvt_mode_info->htotal;
    Mode->VSyncStart = libxcvt_mode_info->vsync_start;
    Mode->VSyncEnd   = libxcvt_mode_info->vsync_end;
    Mode->VTotal     = libxcvt_mode_info->vtotal;
    Mode->VRefresh   = libxcvt_mode_info->vrefresh;
    Mode->Flags      = libxcvt_mode_info->mode_flags;

    free(libxcvt_mode_info);

    return Mode;
}
