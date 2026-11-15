// C:\EEX\Low-Level Engine\Graphics Engine\GEWindow.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10038020..0x10038527: GEDeviceContext's static init chains (sHDC 0x38020, sLock 0x38071), GEDeviceContext and
// GEWindow members, then the map<HWND__*, GEDCCriticalSection> COMDATs its dtor pulls in (~_Tree, erase(F, L),
// erase(P), _Erase). Every other GEWindow member is header-inline (GEBitmap.cpp emits them).
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/GE2DPoint.h"
#include "lle/TSScheduler.h"
#include "GEWindow.h"
#include "platform/PlatWindow.h"

// Destroys the window object, destroying the native window only in full-screen mode
GEWindow::~GEWindow()
{
    if (this->mIsFullScreen)
        PlatWindowDestroy(this->mWindow);
}

// Returns the screen position of the window's client area
const GE2DPoint& GEWindow::GetClientOrigin()
{
    int32_t x = 0, y = 0;
    PlatWindowGetPosition(this->mWindow, &x, &y);
    this->mClientOrigin.SetX((float)x);
    this->mClientOrigin.SetY((float)y);
    return this->mClientOrigin;
}

// Resizes the window's client area (the original added the frame for an overlapped window, depth 1: the platform
// window sizes the client area either way)
void GEWindow::Resize(unsigned long inWidth, unsigned long inHeight)
{
    PlatWindowSetSize(this->mWindow, (int32_t)inWidth, (int32_t)inHeight);
    this->mWidth = inWidth;
    this->mHeight = inHeight;
}
