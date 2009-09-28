/*
 * Copyright (c) 2007, 2008 Apple Inc.
 * Copyright (c) 2004 Torrey T. Lyons. All Rights Reserved.
 * Copyright (c) 2002 Greg Parker. All Rights Reserved.
 *
 * Portions of this file are copied from Mesa's xf86glx.c,
 * which contains the following copyright:
 *
 * Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
 * All Rights Reserved.
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
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include "dri.h"

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLContext.h>

#include <GL/gl.h>
#include <GL/glxproto.h>
#include <windowstr.h>
#include <resource.h>
#include <GL/glxint.h>
#include <GL/glxtokens.h>
#include <scrnintstr.h>
#include <glxserver.h>
#include <glxscreens.h>
#include <glxdrawable.h>
#include <glxcontext.h>
#include <glxext.h>
#include <glxutil.h>
#include <glxscreens.h>
#include <GL/internal/glcore.h>

#include "capabilities.h"
#include "visualConfigs.h"
#include "darwinfb.h"

/* Based originally on code from indirect.c which was based on code from i830_dri.c. */
void setVisualConfigs(void) {
    int numConfigs = 0;
    __GLXvisualConfig *visualConfigs, *c;
    void **visualPrivates = NULL;
    struct glCapabilities caps;
    struct glCapabilitiesConfig *conf = NULL;
    int stereo, depth, aux, buffers, stencil, accum, color, msample;
    
    if(getGlCapabilities(&caps)) {
        ErrorF("error from getGlCapabilities()!\n");
        return;
    }
    
    /*
     conf->stereo is 0 or 1, but we need at least 1 iteration of the loop, 
     so we treat a true conf->stereo as 2.
     
     The depth size is 0 or 24.  Thus we do 2 iterations for that.
     
     conf->aux_buffers (when available/non-zero) result in 2 iterations instead of 1.
     
     conf->buffers indicates whether we have single or double buffering.
     
     conf->total_stencil_bit_depths
     
     conf->total_color_buffers indicates the RGB/RGBA color depths.
     
     conf->total_accum_buffers iterations for accum (with at least 1 if equal to 0) 
     
     conf->total_depth_buffer_depths 
     
     conf->multisample_buffers iterations (with at least 1 if equal to 0).  We add 1
     for the 0 multisampling config.
     
     */
    
    assert(NULL != caps.configurations);
    conf = caps.configurations;
    
    numConfigs = 0;
    
    for(conf = caps.configurations; conf; conf = conf->next) {
        if(conf->total_color_buffers <= 0)
            continue;
        
        numConfigs += (conf->stereo ? 2 : 1) 
	    * (conf->aux_buffers ? 2 : 1) 
	    * conf->buffers
	    * ((conf->total_stencil_bit_depths > 0) ? conf->total_stencil_bit_depths : 1)
	    * conf->total_color_buffers
	    * ((conf->total_accum_buffers > 0) ? conf->total_accum_buffers : 1)
	    * conf->total_depth_buffer_depths
	    * (conf->multisample_buffers + 1);
    }
    
    visualConfigs = xcalloc(sizeof(*visualConfigs), numConfigs);
    
    if(NULL == visualConfigs) {
        ErrorF("xcalloc failure when allocating visualConfigs\n");
        freeGlCapabilities(&caps);
        return;
    }
    
    visualPrivates = xcalloc(sizeof(void *), numConfigs);
    
    if(NULL == visualPrivates) {
        ErrorF("xcalloc failure when allocating visualPrivates");
        freeGlCapabilities(&caps);
        xfree(visualConfigs);
        return;
    }    
    
    c = visualConfigs; /* current buffer */
    for(conf = caps.configurations; conf; conf = conf->next) {
        for(stereo = 0; stereo < (conf->stereo ? 2 : 1); ++stereo) {
            for(aux = 0; aux < (conf->aux_buffers ? 2 : 1); ++aux) {
                for(buffers = 0; buffers < conf->buffers; ++buffers) {
                    for(stencil = 0; stencil < ((conf->total_stencil_bit_depths > 0) ? 
                                                conf->total_stencil_bit_depths : 1); ++stencil) {
                        for(color = 0; color < conf->total_color_buffers; ++color) {
                            for(accum = 0; accum < ((conf->total_accum_buffers > 0) ?
                                                    conf->total_accum_buffers : 1); ++accum) {
                                for(depth = 0; depth < conf->total_depth_buffer_depths; ++depth) {
                                    for(msample = 0; msample < (conf->multisample_buffers + 1); ++msample) {
                                        
                                        // Global
                                        c->vid = (VisualID)-1;
                                        c->class = GLX_TRUE_COLOR;

                                        c->rgba = true;
                                        
                                        c->level = 0;
                                        
                                        if(conf->accelerated) {
                                            c->visualRating = GLX_NONE;
                                        } else {
                                            c->visualRating = GLX_SLOW_VISUAL_EXT;
                                        }
                                        
                                        c->transparentPixel = GLX_NONE;
                                        c->transparentRed = GLX_NONE;
                                        c->transparentGreen = GLX_NONE;
                                        c->transparentBlue = GLX_NONE;
                                        c->transparentAlpha = GLX_NONE;
                                        c->transparentIndex = GLX_NONE;
                                        
                                        c->visualSelectGroup = 0;
                                        
                                        // Stereo
                                        c->stereo = stereo ? TRUE : FALSE;

                                        // Aux buffers
                                        c->auxBuffers = aux ? conf->aux_buffers : 0;
                                        
                                        // Double Buffered
                                        c->doubleBuffer = buffers ? TRUE : FALSE;

                                        // Stencil Buffer
                                        if(conf->total_stencil_bit_depths > 0) {
                                            c->stencilSize = conf->stencil_bit_depths[stencil];
                                        } else {
                                            c->stencilSize = 0;
                                        }
                                        
                                        // Color
                                        if(GLCAPS_COLOR_BUF_INVALID_VALUE != conf->color_buffers[color].a) {
                                            c->alphaSize = conf->color_buffers[color].a;
                                        } else {
                                            c->alphaSize = 0;
                                        }
                                        c->redSize   = conf->color_buffers[color].r;
                                        c->greenSize = conf->color_buffers[color].g;
                                        c->blueSize  = conf->color_buffers[color].b;
                                        
                                        c->bufferSize = c->alphaSize + c->redSize + c->greenSize + c->blueSize;

                                        c->alphaMask = AM_ARGB(c->alphaSize, c->redSize, c->greenSize, c->blueSize);
                                        c->redMask   = RM_ARGB(c->alphaSize, c->redSize, c->greenSize, c->blueSize);
                                        c->greenMask = GM_ARGB(c->alphaSize, c->redSize, c->greenSize, c->blueSize);
                                        c->blueMask  = BM_ARGB(c->alphaSize, c->redSize, c->greenSize, c->blueSize);
                                                                                
                                        // Accumulation Buffers
                                        if(conf->total_accum_buffers > 0) {
                                            c->accumRedSize = conf->accum_buffers[accum].r;
                                            c->accumGreenSize = conf->accum_buffers[accum].g;
                                            c->accumBlueSize = conf->accum_buffers[accum].b;
                                            if(GLCAPS_COLOR_BUF_INVALID_VALUE != conf->accum_buffers[accum].a) {
                                                c->accumAlphaSize = conf->accum_buffers[accum].a;
                                            } else {
                                                c->accumAlphaSize = 0;
                                            }
                                        } else {
                                            c->accumRedSize = 0;
                                            c->accumGreenSize = 0;
                                            c->accumBlueSize = 0;
                                            c->accumAlphaSize = 0;
                                        } 

                                        // Depth
                                        c->depthSize = conf->depth_buffers[depth];
                                        
                                        // MultiSample
                                        if(msample > 0) {
                                            c->multiSampleSize = conf->multisample_samples;
                                            c->nMultiSampleBuffers = conf->multisample_buffers;
                                        } else {
                                            c->multiSampleSize = 0;
                                            c->nMultiSampleBuffers = 0;
                                        }

                                        c = c + 1;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (c - visualConfigs != numConfigs) {
        FatalError("numConfigs calculation error in setVisualConfigs!  numConfigs is %d  i is %d\n", numConfigs, (int)(c - visualConfigs));
    }
    
    freeGlCapabilities(&caps);
    GlxSetVisualConfigs(numConfigs, visualConfigs, visualPrivates);
}
