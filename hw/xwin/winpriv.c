#include "win.h"
#include "winpriv.h"
#include "winwindow.h"


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
        winScreenInfoPtr pScreenInfo = NULL;

        ErrorF("winGetWindowInfo: returning a window\n");
        
        pWinInfo->hwnd = NULL;
        pWinInfo->hrgn = NULL;
        pWinInfo->rect = rect;
    
        if (pWinScreen == NULL) 
            return;

        pScreenInfo = pWinScreen->pScreenInfo;
        pWinInfo->hwnd = pWinScreen->hwndScreen;

#ifdef XWIN_MULTIWINDOW
        if (pWinScreen->pScreenInfo->fMultiWindow)
        {
            winWindowPriv(pWin);

            ErrorF("winGetWindowInfo: multiwindow\n");

            if (pWinPriv == NULL)
            {
                ErrorF("winGetWindowInfo: window has no privates\n");
                return;
            }

            RECT rect = {
                0,
                0,
                pWin->drawable.width,
                pWin->drawable.height
            };

            if (pWinPriv->hWnd != NULL)
                pWinInfo->hwnd = pWinPriv->hWnd;
            pWinInfo->hrgn = NULL;
            pWinInfo->rect = rect;
            
            return;
        }
#endif
#ifdef XWIN_MULTIWINDOWEXTWM
        if (pWinScreen->pScreenInfo->fMWExtWM)
        {
            win32RootlessWindowPtr pRLWinPriv
                = (win32RootlessWindowPtr) RootlessFrameForWindow (pWin, FALSE);

            ErrorF("winGetWindowInfo: multiwindow extwm\n");
            
            if (pRLWinPriv == NULL) {
                ErrorF("winGetWindowInfo: window has no privates\n");
                return;
            }
            
            RECT rect = {
                0,
                0,
                pWin->drawable.width,
                pWin->drawable.height
            };

            if (pRLWinPriv->hWnd != NULL)
                pWinInfo->hwnd = pRLWinPriv->hWnd;
            pWinInfo->hrgn = NULL;
            pWinInfo->rect = rect;
            
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
