#include <windows.h>

typedef struct
{
    HWND    hwnd;
    HRGN    hrgn;
    RECT    rect;
} winWindowInfoRec, *winWindowInfoPtr;

extern void winGetWindowInfo(WindowPtr pWin, winWindowInfoPtr pWinInfo);
