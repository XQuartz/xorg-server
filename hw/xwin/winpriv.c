#include "win.h"
#include "winpriv.h"


extern void winGetWindowInfo(WindowPtr pWin, winWindowInfoPtr pWinInfo)
{
    if (pWinInfo == NULL)
        return;

    if (pWin != NULL) 
    {
        RECT rect = {
            pWin->drawable.x,
            pWin->drawable.y,
            pWin->drawable.x + pWin->drawable.width,
            pWin->drawable.y + pWin->drawable.height
        };
        ScreenPtr pScreen = pWin->drawable.pScreen;
        winPrivScreenPtr pWinScreen = winGetScreenPriv(pScreen);

        ErrorF("winGetWindowInfo: returning a window\n");
        
        pWinInfo->hwnd = NULL;
        pWinInfo->hrgn = NULL;
        pWinInfo->rect = rect;
    
        if (pWinScreen == NULL)
            return;

        pWinInfo->hwnd = pWinScreen->hwndScreen;

#ifdef XWIN_MULTIWINDOWEXTWM
        /* if (multiwindow) */
        {
            win32RootlessWindowPtr pRLWinPriv
                = (win32RootlessWindowPtr) RootlessFrameForWindow (pWin, FALSE);
            
            if (pRLWinPriv == NULL) {
                ErrorF("winGetWindowInfo: window is not rootless\n");
                return;
            }
            
            RECT rect = {
                0,
                0,
                pWin->drawable.width,
                pWin->drawable.height
            };

            pWinInfo->hwnd = pRLWinPriv->hWnd;
            pWinInfo->hrgn = NULL;
            pWinInfo->rect = rect;
            
            ErrorF("winGetWindowInfo: window is rootless\n");
            
            return;
        }
#endif
    } 
    else 
    {
        RECT rect = {0, 0, 0, 0};
        ScreenPtr pScreen = g_ScreenInfo[0].pScreen;
        winPrivScreenPtr pWinScreen = winGetScreenPriv(pScreen);

        pWinInfo->hwnd = NULL;
        pWinInfo->hrgn = NULL;
        pWinInfo->rect = rect;
        
        if (pWinScreen == NULL)
            return;

        ErrorF("winGetWindowInfo: returning root window\n");

        pWinInfo->hwnd = pWinScreen->hwndScreen;
    }
    return;
}
