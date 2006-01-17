/*
 * Copyright © 2005  Red Hat, Inc.
 * (C) Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * RED HAT, INC, OR PRECISION INSIGHT AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
 * OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Kevin E. Martin <kevin@precisioninsight.com>
 *   Brian Paul <brian@precisioninsight.com>
 *   Kristian Høgsberg <krh@redhat.com>
 *
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <windowstr.h>
#include <os.h>

#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/time.h>

#include <GL/gl.h>
#include <GL/internal/dri_interface.h>

#define _XF86DRI_SERVER_
#include <drm_sarea.h>
#include <xf86drm.h>
#include <xf86dristr.h>
#include <xf86str.h>
#include <xf86.h>
#include <dri.h>

#define DRI_NEW_INTERFACE_ONLY
#include "glxserver.h"
#include "glxutil.h"
#include "glxloader.h"
#include "glcontextmodes.h"

#ifndef DEFAULT_DRIVER_DIR
/* this is normally defined in the Imakefile */
#define DEFAULT_DRIVER_DIR "/usr/lib/dri"
#endif

/*
** We keep a linked list of these structures, one per DRI device driver.
*/

/* Not sure if the DRIdriver struct is really necessary...  We could
 * just dlopen() the driver for every screen.  This won't cause the
 * library to be loaded again, and the libraries are ref-counted so
 * dlclose()'ing the driver for one screen won't close it for
 * another. /KRH */

struct __DRIdriverRec {
   const char *name;
   void *handle;
   PFNCREATENEWSCREENFUNC createNewScreenFunc;
   struct __DRIdriverRec *next;
};

static __DRIdriver *Drivers = NULL;



/**
 * Extract the ith directory path out of a colon-separated list of paths.  No
 * more than \c dirLen characters, including the terminating \c NUL, will be
 * written to \c dir.
 *
 * \param index  Index of path to extract (starting at zero)
 * \param paths  The colon-separated list of paths
 * \param dirLen Maximum length of result to store in \c dir
 * \param dir    Buffer to hold the extracted directory path
 *
 * \returns
 * The number of characters that would have been written to \c dir had there
 * been enough room.  This does not include the terminating \c NUL.  When
 * extraction fails, zero will be returned.
 * 
 * \todo
 * It seems like this function could be rewritten to use \c strchr.
 */
static size_t
ExtractDir(int index, const char *paths, int dirLen, char *dir)
{
   int i, len;
   const char *start, *end;

   /* find ith colon */
   start = paths;
   i = 0;
   while (i < index) {
      if (*start == ':') {
         i++;
         start++;
      }
      else if (*start == 0) {
         /* end of string and couldn't find ith colon */
         dir[0] = 0;
         return 0;
      }
      else {
         start++;
      }
   }

   while (*start == ':')
      start++;

   /* find next colon, or end of string */
   end = start + 1;
   while (*end != ':' && *end != 0) {
      end++;
   }

   /* copy string between <start> and <end> into result string */
   len = end - start;
   if (len > dirLen - 1)
      len = dirLen - 1;
   strncpy(dir, start, len);
   dir[len] = 0;

   return( end - start );
}


/**
 * Versioned name of the expected \c __driCreateNewScreen function.
 * 
 * The version of the last incompatible loader/driver inteface change is
 * appended to the name of the \c __driCreateNewScreen function.  This
 * prevents loaders from trying to load drivers that are too old.
 * 
 * \todo
 * Create a macro or something so that this is automatically updated.
 */
static const char createNewScreenName[] = "__driCreateNewScreen_20050727";


/**
 * Try to \c dlopen the named driver.
 *
 * This function adds the "_dri.so" suffix to the driver name and searches the
 * directories specified by the \c LIBGL_DRIVERS_PATH environment variable in
 * order to find the driver.
 *
 * \param driverName - a name like "tdfx", "i810", "mga", etc.
 *
 * \returns
 * A handle from \c dlopen, or \c NULL if driver file not found.
 */
static __DRIdriver *OpenDriver(const char *driverName, ScreenPtr pScreen)
{
   char *libPaths = NULL;
   char libDir[1000];
   int i;
   __DRIdriver *driver;

   /* First, search Drivers list to see if we've already opened this driver */
   for (driver = Drivers; driver; driver = driver->next) {
      if (strcmp(driver->name, driverName) == 0) {
         /* found it */
         return driver;
      }
   }

   /* We probably want to control this similar to how we handle the
    * -modulepath cmd line option.  For now, this is convenient
    * though. */
   libPaths = getenv("LIBGL_DRIVERS_PATH");
   if (!libPaths)
       libPaths = getenv("LIBGL_DRIVERS_DIR"); /* deprecated */
   if (!libPaths)
      libPaths = DEFAULT_DRIVER_DIR;

   for ( i = 0 ; ExtractDir(i, libPaths, sizeof libDir, libDir) != 0 ; i++ ) {
      char realDriverName[200];
      void *handle = NULL;

      snprintf(realDriverName, sizeof realDriverName,
	       "%s/%s_dri.so", libDir, driverName);

      xf86DrvMsg(pScreen->myNum, X_INFO,
		 "OpenDriver: trying %s\n", realDriverName);
      handle = dlopen(realDriverName, RTLD_NOW | RTLD_GLOBAL);

      if ( handle != NULL ) {
         /* allocate __DRIdriver struct */
         driver = (__DRIdriver *) __glXMalloc(sizeof(__DRIdriver));
         if (!driver)
            return NULL; /* out of memory! */
         /* init the struct */
         driver->name = Xstrdup(driverName);
         if (!driver->name) {
            Xfree(driver);
            return NULL; /* out of memory! */
         }

         driver->createNewScreenFunc = (PFNCREATENEWSCREENFUNC)
            dlsym(handle, createNewScreenName);

         if ( driver->createNewScreenFunc == NULL ) {
            /* If the driver doesn't have this symbol then something's
             * really, really wrong.
             */
	     xf86DrvMsg(pScreen->myNum, X_ERROR,
			"%s not defined in %s_dri.so!\n"
			"Your driver may be too old for this GLX module.\n",
			createNewScreenName, driverName);
            Xfree(driver);
            dlclose(handle);
            continue;
         }
         driver->handle = handle;
         /* put at head of linked list */
         driver->next = Drivers;
         Drivers = driver;

         return driver;
      }
      else {
	  xf86DrvMsg(pScreen->myNum, X_ERROR,
		     "dlopen %s failed (%s)\n", realDriverName, dlerror());
      }
   }

   xf86DrvMsg(pScreen->myNum, X_ERROR,
	      "unable to find driver: %s_dri.so\n", driverName);

   return NULL;
}


/*
 * Given a display pointer and screen number, determine the name of
 * the DRI driver for the screen. (I.e. "r128", "tdfx", etc).
 * Return True for success, False for failure.
 */
static Bool GetDriverName(ScreenPtr pScreen, char **driverName)
{
   int directCapable;
   Bool b;
   int driverMajor, driverMinor, driverPatch;

   *driverName = NULL;

   if (!DRIQueryDirectRenderingCapable(pScreen, &directCapable)) {
       xf86DrvMsg(pScreen->myNum, X_ERROR,
		  "DRIQueryDirectRenderingCapable failed\n");
      return False;
   }
   if (!directCapable) {
       xf86DrvMsg(pScreen->myNum, X_ERROR,
		  "DRIQueryDirectRenderingCapable returned false\n");
       return False;
   }

   b = DRIGetClientDriverName(pScreen, &driverMajor, &driverMinor,
			      &driverPatch, driverName);
   if (!b) {
       xf86DrvMsg(pScreen->myNum, X_ERROR,
		  "Cannot determine driver name for screen %d\n",
		  pScreen->myNum);
      return False;
   }

   xf86DrvMsg(pScreen->myNum, X_INFO,
	      "DRIGetClientDriverName: %d.%d.%d %s (screen %d)\n",
	      driverMajor, driverMinor, driverPatch,
	      *driverName, pScreen->myNum);

   return True;
}


/*
 * Given a display pointer and screen number, return a __DRIdriver handle.
 * Return NULL if anything goes wrong.
 */
static __DRIdriver *driGetDriver(ScreenPtr pScreen)
{
   char *driverName;

   if (GetDriverName(pScreen, &driverName)) {
      __DRIdriver *ret;
      ret = OpenDriver(driverName, pScreen);

      return ret;
   }

   return NULL;
}

static unsigned
filter_modes(__GLcontextModes **server_modes,
	     const __GLcontextModes *driver_modes)
{
    __GLcontextModes * m;
    __GLcontextModes ** prev_next;
    const __GLcontextModes * check;
    unsigned modes_count = 0;

    if ( driver_modes == NULL ) {
	fprintf(stderr, "libGL warning: 3D driver returned no fbconfigs.\n");
	return 0;
    }

    /* For each mode in server_modes, check to see if a matching mode exists
     * in driver_modes.  If not, then the mode is not available.
     */

    prev_next = server_modes;
    for ( m = *prev_next ; m != NULL ; m = *prev_next ) {
	GLboolean do_delete = GL_TRUE;

	for ( check = driver_modes ; check != NULL ; check = check->next ) {
	    if ( _gl_context_modes_are_same( m, check ) ) {
		do_delete = GL_FALSE;
		break;
	    }
	}

	/* The 3D has to support all the modes that match the GLX visuals
	 * sent from the X server.
	 */
	if ( do_delete && (m->visualID != 0) ) {
	    do_delete = GL_FALSE;

	    fprintf(stderr, "libGL warning: 3D driver claims to not support "
		    "visual 0x%02x\n", m->visualID);
	}

	if ( do_delete ) {
	    *prev_next = m->next;

	    m->next = NULL;
	    _gl_context_modes_destroy( m );
	}
	else {
	    modes_count++;
	    prev_next = & m->next;
	}
    }

    return modes_count;
}


static __DRIfuncPtr getProcAddress(const char *proc_name)
{
    return NULL;
}

static __DRIscreen *findScreen(__DRInativeDisplay *dpy, int scrn)
{
    return &__glXActiveScreens[scrn].driScreen;
}

static GLboolean windowExists(__DRInativeDisplay *dpy, __DRIid draw)
{
    WindowPtr pWin = (WindowPtr) LookupIDByType(draw, RT_WINDOW);

    return pWin == NULL ? GL_FALSE : GL_TRUE;
}

static GLboolean createContext(__DRInativeDisplay *dpy, int screen,
			       int configID, void *contextID,
			       drm_context_t *hw_context)
{
    XID fakeID;
    VisualPtr visual;
    int i;
    ScreenPtr pScreen;

    pScreen = screenInfo.screens[screen];

    /* Find the requested X visual */
    visual = pScreen->visuals;
    for (i = 0; i < pScreen->numVisuals; i++, visual++)
	if (visual->vid == configID)
	    break;
    if (i == pScreen->numVisuals)
	return GL_FALSE;

    fakeID = FakeClientID(0);
    *(XID *) contextID = fakeID;

    return DRICreateContext(pScreen, visual, fakeID, hw_context);
}

static GLboolean destroyContext(__DRInativeDisplay *dpy, int screen,
				__DRIid context)
{
    return DRIDestroyContext(screenInfo.screens[screen], context);
}

static GLboolean
createDrawable(__DRInativeDisplay *dpy, int screen,
	       __DRIid drawable, drm_drawable_t *hHWDrawable)
{
    DrawablePtr pDrawable;

    pDrawable = (DrawablePtr) LookupIDByClass(drawable, RC_DRAWABLE);
    if (!pDrawable)
	return GL_FALSE;

    return DRICreateDrawable(screenInfo.screens[screen],
			     drawable,
			     pDrawable,
			     hHWDrawable);
}

static GLboolean
destroyDrawable(__DRInativeDisplay *dpy, int screen, __DRIid drawable)
{
    DrawablePtr pDrawable;

    pDrawable = (DrawablePtr) LookupIDByClass(drawable, RC_DRAWABLE);
    if (!pDrawable)
	return GL_FALSE;

    return DRIDestroyDrawable(screenInfo.screens[screen],
			      drawable,
			      pDrawable);
}

static GLboolean
getDrawableInfo(__DRInativeDisplay *dpy, int screen,
		__DRIid drawable, unsigned int *index, unsigned int *stamp,
		int *x, int *y, int *width, int *height,
		int *numClipRects, drm_clip_rect_t **ppClipRects,
		int *backX, int *backY,
		int *numBackClipRects, drm_clip_rect_t **ppBackClipRects)
{
    DrawablePtr pDrawable;
    drm_clip_rect_t *pClipRects, *pBackClipRects;
    GLboolean retval;
    size_t size;

    pDrawable = (DrawablePtr) LookupIDByClass(drawable, RC_DRAWABLE);
    if (!pDrawable)
	return GL_FALSE;

    retval = DRIGetDrawableInfo(screenInfo.screens[screen],
				pDrawable, index, stamp,
				x, y, width, height,
				numClipRects, &pClipRects,
				backX, backY,
				numBackClipRects, &pBackClipRects);

    if (*numClipRects > 0) {
	size = sizeof (drm_clip_rect_t) * *numClipRects;
	*ppClipRects = __glXMalloc (size);
	if (*ppClipRects != NULL)
	    memcpy (*ppClipRects, pClipRects, size);
    }
    else {
      *ppClipRects = NULL;
    }
      
    if (*numBackClipRects > 0) {
	size = sizeof (drm_clip_rect_t) * *numBackClipRects;
	*ppBackClipRects = __glXMalloc (size);
	if (*ppBackClipRects != NULL)
	    memcpy (*ppBackClipRects, pBackClipRects, size);
    }
    else {
      *ppBackClipRects = NULL;
    }

    return GL_TRUE;
}

static int
getUST(int64_t *ust)
{
    struct timeval  tv;
    
    if (ust == NULL)
	return -EFAULT;

    if (gettimeofday(&tv, NULL) == 0) {
	ust[0] = (tv.tv_sec * 1000000) + tv.tv_usec;
	return 0;
    } else {
	return -errno;
    }
}

/* Table of functions exported by the loader to the driver. */
static const __DRIinterfaceMethods interface_methods = {
    getProcAddress,

    _gl_context_modes_create,
    _gl_context_modes_destroy,
      
    findScreen,
    windowExists,
      
    createContext,
    destroyContext,

    createDrawable,
    destroyDrawable,
    getDrawableInfo,

    getUST,
    NULL, /* glXGetMscRateOML, */
};

/**
 * Retrieves the verion of the internal libGL API in YYYYMMDD format.  This
 * might be used by the DRI drivers to determine how new libGL is at runtime.
 * Drivers should not call this function directly.  They should instead use
 * \c glXGetProcAddress to obtain a pointer to the function.
 * 
 * \returns An 8-digit decimal number representing the internal libGL API in
 *          YYYYMMDD format.
 * 
 * \sa glXGetProcAddress, PFNGLXGETINTERNALVERSIONPROC
 *
 * \since Internal API version 20021121.
 */
static int __glXGetInternalVersion(void)
{
    /* History:
     * 20021121 - Initial version
     * 20021128 - Added __glXWindowExists() function
     * 20021207 - Added support for dynamic GLX extensions,
     *            GLX_SGI_swap_control, GLX_SGI_video_sync,
     *            GLX_OML_sync_control, and GLX_MESA_swap_control.
     *            Never officially released.  Do NOT test against
     *            this version.  Use 20030317 instead.
     * 20030317 - Added support GLX_SGIX_fbconfig,
     *            GLX_MESA_swap_frame_usage, GLX_OML_swap_method,
     *            GLX_{ARB,SGIS}_multisample, and
     *            GLX_SGIX_visual_select_group.
     * 20030606 - Added support for GLX_SGI_make_current_read.
     * 20030813 - Made support for dynamic extensions multi-head aware.
     * 20030818 - Added support for GLX_MESA_allocate_memory in place of the
     *            deprecated GLX_NV_vertex_array_range & GLX_MESA_agp_offset
     *            interfaces.
     * 20031201 - Added support for the first round of DRI interface changes.
     *            Do NOT test against this version!  It has binary
     *            compatibility bugs, use 20040317 instead.
     * 20040317 - Added the 'mode' field to __DRIcontextRec.
     * 20040415 - Added support for bindContext3 and unbindContext3.
     * 20040602 - Add __glXGetDrawableInfo.  I though that was there
     *            months ago. :(
     * 20050727 - Gut all the old interfaces.  This breaks compatability with
     *            any DRI driver built to any previous version.
     */
    return 20050727;
}

/**
 * Perform the required libGL-side initialization and call the client-side
 * driver's \c __driCreateNewScreen function.
 * 
 * \param dpy    Display pointer.
 * \param scrn   Screen number on the display.
 * \param psc    DRI screen information.
 * \param driDpy DRI display information.
 * \param createNewScreen  Pointer to the client-side driver's
 *               \c __driCreateNewScreen function.
 * \returns A pointer to the \c __DRIscreenPrivate structure returned by
 *          the client-side driver on success, or \c NULL on failure.
 * 
 * \todo This function needs to be modified to remove context-modes from the
 *       list stored in the \c __GLXscreenConfigsRec to match the list
 *       returned by the client-side driver.
 */
static void *
CallCreateNewScreen(ScreenPtr pScreen, __DRIscreen *psc,
		    PFNCREATENEWSCREENFUNC createNewScreen)
{
    drm_handle_t hSAREA;
    drmAddress pSAREA = MAP_FAILED;
    char *BusID;
    __DRIversion   ddx_version;
    __DRIversion   dri_version;
    __DRIversion   drm_version;
    __DRIframebuffer  framebuffer;
    int   fd = -1;
    int   status;
    const char * err_msg;
    const char * err_extra = NULL;
    int api_ver = __glXGetInternalVersion();
    drm_magic_t magic;
    drmVersionPtr version;
    char *driverName;
    drm_handle_t  hFB;
    int        junk;
    __GLcontextModes * driver_modes;
    __GLXscreenInfo *pGlxScreen;

    /* DRI protocol version. */
    dri_version.major = XF86DRI_MAJOR_VERSION;
    dri_version.minor = XF86DRI_MINOR_VERSION;
    dri_version.patch = XF86DRI_PATCH_VERSION;

    framebuffer.base = MAP_FAILED;
    framebuffer.dev_priv = NULL;

    if (!DRIOpenConnection(pScreen, &hSAREA, &BusID)) {
	err_msg = "DRIOpenConnection";
	err_extra = NULL;
	goto handle_error;
    }

    fd = drmOpen(NULL, BusID);
    Xfree(BusID); /* No longer needed */

    if (fd < 0) {
	err_msg = "open DRM";
	err_extra = strerror( -fd );
	goto handle_error;
    }

    if (drmGetMagic(fd, &magic)) {
	err_msg = "drmGetMagic";
	err_extra = NULL;
	goto handle_error;
    }

    version = drmGetVersion(fd);
    if (version) {
	drm_version.major = version->version_major;
	drm_version.minor = version->version_minor;
	drm_version.patch = version->version_patchlevel;
	drmFreeVersion(version);
    }
    else {
	drm_version.major = -1;
	drm_version.minor = -1;
	drm_version.patch = -1;
    }

    if (!DRIAuthConnection(pScreen, magic)) {
	err_msg = "DRIAuthConnection";
	goto handle_error;
    }

    /* Get device name (like "tdfx") and the ddx version numbers.
     * We'll check the version in each DRI driver's "createNewScreen"
     * function. */
    if (!DRIGetClientDriverName(pScreen,
				&ddx_version.major,
				&ddx_version.minor,
				&ddx_version.patch,
				&driverName)) {
	err_msg = "DRIGetClientDriverName";
	goto handle_error;
    }

    /*
     * Get device-specific info.  pDevPriv will point to a struct
     * (such as DRIRADEONRec in xfree86/driver/ati/radeon_dri.h) that
     * has information about the screen size, depth, pitch, ancilliary
     * buffers, DRM mmap handles, etc.
     */
    if (!DRIGetDeviceInfo(pScreen, &hFB, &junk,
			  &framebuffer.size, &framebuffer.stride,
			  &framebuffer.dev_priv_size, &framebuffer.dev_priv)) {
	err_msg = "XF86DRIGetDeviceInfo";
	goto handle_error;
    }

    framebuffer.width = pScreen->width;
    framebuffer.height = pScreen->height;

    /* Map the framebuffer region. */
    status = drmMap(fd, hFB, framebuffer.size, 
		    (drmAddressPtr)&framebuffer.base);
    if (status != 0) {
	err_msg = "drmMap of framebuffer";
	err_extra = strerror( -status );
	goto handle_error;
    }

    /* Map the SAREA region.  Further mmap regions may be setup in
     * each DRI driver's "createNewScreen" function.
     */
    status = drmMap(fd, hSAREA, SAREA_MAX, &pSAREA);
    if (status != 0) {
	err_msg = "drmMap of sarea";
	err_extra = strerror( -status );
	goto handle_error;
    }
    
    driver_modes = NULL;

    pGlxScreen = &__glXActiveScreens[pScreen->myNum];

    __glXActiveScreens[pScreen->myNum].driScreen.private =
	(*createNewScreen)(NULL, pScreen->myNum,
			   &__glXActiveScreens[pScreen->myNum].driScreen,
			   pGlxScreen->modes,
			   &ddx_version,
			   &dri_version,
			   &drm_version,
			   &framebuffer,
			   pSAREA,
			   fd,
			   api_ver,
			   &interface_methods,
			   &driver_modes);

    if (__glXActiveScreens[pScreen->myNum].driScreen.private == NULL) {
	err_msg = "InitDriver";
	err_extra = NULL;
	goto handle_error;
    }

    filter_modes(&pGlxScreen->modes, driver_modes);
    _gl_context_modes_destroy(driver_modes);

    return __glXActiveScreens[pScreen->myNum].driScreen.private;

 handle_error:
    if (pSAREA != MAP_FAILED)
	drmUnmap(pSAREA, SAREA_MAX);

    if (framebuffer.base != MAP_FAILED)
	drmUnmap((drmAddress)framebuffer.base, framebuffer.size);

    if (framebuffer.dev_priv != NULL)
	Xfree(framebuffer.dev_priv);

    if (fd >= 0)
	drmClose(fd);

    DRICloseConnection(pScreen);

    if (err_extra != NULL)
	fprintf(stderr, "libGL error: %s failed (%s)\n", err_msg,
		err_extra);
    else
	fprintf(stderr, "libGL error: %s failed\n", err_msg );

    fprintf(stderr, "libGL error: reverting to (slow) indirect rendering\n");

    return NULL;
}

#if 0
/* This function isn't currently used.
 */
static void driDestroyDisplay(Display *dpy, void *private)
{
    __DRIdisplayPrivate *pdpyp = (__DRIdisplayPrivate *)private;

    if (pdpyp) {
        const int numScreens = ScreenCount(dpy);
        int i;
        for (i = 0; i < numScreens; i++) {
            if (pdpyp->libraryHandles[i])
                dlclose(pdpyp->libraryHandles[i]);
        }
        Xfree(pdpyp->libraryHandles);
	Xfree(pdpyp);
    }
}
#endif

void __glXLoaderInitScreen(int screen)
{
    ScreenPtr pScreen;
    __DRIdriver *driver;

    /* dynamically discover DRI drivers for all screens, saving each
     * driver's "__driCreateScreen" function pointer.  That's the bootstrap
     * entrypoint for all DRI drivers.
     */
    pScreen = screenInfo.screens[screen];
    driver = driGetDriver(pScreen);

    if (driver) {
	__glXActiveScreens[screen].driver = driver;

	CallCreateNewScreen(pScreen,
			    &__glXActiveScreens[screen].driScreen,
			    driver->createNewScreenFunc);
	__glXActiveScreens[screen].driScreen.screenConfigs = 
	    &__glXActiveScreens[screen];
    }
    else {
	/* FIXME: Fall back to loading sw dri driver here. */
    }
}
