#include <GL/gl.h>
#include <GL/glext.h>

#include <glxserver.h>
#include <glxext.h>

#include <mipointrst.h>
#include <miscstruct.h>
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
#include <stdlib.h>

#define WINDOWS_LEAN_AND_CLEAN
#include <windows.h>


typedef struct {
    unsigned enableDebug : 1;
    unsigned dumpPFD : 1;
    unsigned dumpHWND : 1;
    unsigned dumpDC : 1;
} glWinDebugSettingsRec, *glWinDebugSettingsPtr;
extern glWinDebugSettingsRec glWinDebugSettings;

typedef struct {
    int num_vis;
    __GLXvisualConfig *glx_vis;
    void **priv;

    /* wrapped screen functions */
    RealizeWindowProcPtr RealizeWindow;
    UnrealizeWindowProcPtr UnrealizeWindow;
    CopyWindowProcPtr CopyWindow;
} glWinScreenRec;

extern glWinScreenRec glWinScreens[MAXSCREENS];

#define glWinGetScreenPriv(pScreen)  &glWinScreens[pScreen->myNum]
#define glWinScreenPriv(pScreen) glWinScreenRec *pScreenPriv = glWinGetScreenPriv(pScreen);

#if 1
#define GLWIN_DEBUG_MSG if (glWinDebugSettings.enableDebug) ErrorF
#else
#define GLWIN_DEBUG_MSG(a, ...)
#endif

