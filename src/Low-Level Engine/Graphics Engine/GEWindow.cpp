// C:\EEX\Low-Level Engine\Graphics Engine\GEWindow.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10038020..0x10038527: GEDeviceContext's static init chains (sHDC 0x38020, sLock 0x38071), GEDeviceContext and
// GEWindow members, then the map<HWND__*, GEDCCriticalSection> COMDATs its dtor pulls in (~_Tree, erase(F, L),
// erase(P), _Erase). Every other GEWindow member is header-inline (GEBitmap.cpp emits them).
//
// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x10038020
// SYNTHETIC: LLE 0x1003802a
// SYNTHETIC: LLE 0x1003804a
// SYNTHETIC: LLE 0x10038056
// SYNTHETIC: LLE 0x10038071
// SYNTHETIC: LLE 0x1003807b
// SYNTHETIC: LLE 0x1003809f
// SYNTHETIC: LLE 0x100380ab
// TEMPLATE: LLE 0x10038199
// TEMPLATE: LLE 0x100381c4
// TEMPLATE: LLE 0x10038235
// TEMPLATE: LLE 0x100384ec
#include <windows.h>

#define LLE_GEWINDOW_CPP    // GEDeviceContext's statics are visible here only (see GEWindow.h) (provisional name)
#include "lle/LowLevelEngineCore.h"
#include "lle/GE2DPoint.h"
#include "lle/TSScheduler.h"
#include "GEWindow.h"
#include "platform/PlatWindow.h"

std::map<PlatWindow*, GEDeviceContext::GEDCCriticalSection> GEDeviceContext::sHDC;
GEDeviceContext::GEDCCriticalSection GEDeviceContext::sLock;

// FUNCTION: LLE 0x100380c8
// Returns the window's device context (GetDC; a NULL window: the screen's). NULL off Windows (no GDI).
HDC__* GEDeviceContext::GetDeviceContext(PlatWindow* inWindow)
{
#ifdef _WIN32
    PlatNativeWindow theNative = {};
    if (inWindow != NULL)
        PlatWindowGetNative(inWindow, &theNative);
    return GetDC((HWND)theNative.mWindow);
#else
    return NULL;
#endif
}

// FUNCTION: LLE 0x100380d3
// Releases a window's device context; always succeeds
long GEDeviceContext::ReleaseDeviceContext(PlatWindow* inWindow, HDC__* inContext)
{
#ifdef _WIN32
    PlatNativeWindow theNative = {};
    if (inWindow != NULL)
        PlatWindowGetNative(inWindow, &theNative);
    ReleaseDC((HWND)theNative.mWindow, inContext);
#endif
    return kErrorOK;
}

// FUNCTION: LLE 0x100380e4
// Destroys the window object, destroying the native window only in full-screen mode
GEWindow::~GEWindow()
{
    if (this->mIsFullScreen)
        PlatWindowDestroy(this->mWindow);
}

void* GEWindow::GetNativeHandle() const
{
    PlatNativeWindow theNative = {};
    if (this->mWindow == NULL || !PlatWindowGetNative(this->mWindow, &theNative))
        return NULL;
    return theNative.mWindow;
}

// FUNCTION: LLE 0x100380f3
// Returns the screen position of the window's client area
const GE2DPoint& GEWindow::GetClientOrigin()
{
    int32_t x = 0, y = 0;
    PlatWindowGetPosition(this->mWindow, &x, &y);
    this->mClientOrigin.SetX((float)x);
    this->mClientOrigin.SetY((float)y);
    return this->mClientOrigin;
}

// FUNCTION: LLE 0x10038120
// Resizes the window's client area (the original added the frame for an overlapped window, depth 1: the platform
// window sizes the client area either way)
void GEWindow::Resize(unsigned long inWidth, unsigned long inHeight)
{
    PlatWindowSetSize(this->mWindow, (int32_t)inWidth, (int32_t)inHeight);
    this->mWidth = inWidth;
    this->mHeight = inHeight;
}
