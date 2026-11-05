// C:\EEX\Low-Level Engine\Graphics Engine\GEWindow.h (RECONSTRUCTION; __FILE__ anchor in the binary). GEDeviceContext
// and GEWindow. GEWindow.cpp (0x10038020..0x10038527) defines GEDeviceContext's statics (their init chains come first)
// and holds the out-of-line members; the rest is inline (GEBitmap.cpp emits it first). The GEDCCriticalSection ctor's
// ESVERIFY is the original's line 48 (push 0x30 in sLock's initializer 0x3807b).
#ifndef LLE_GEWINDOW_H
#define LLE_GEWINDOW_H


struct PlatWindow;

// (GEDeviceContext, the windows' GDI device contexts with a lock per window, went with GDI text: PORT.md step 4.5)

// The game's window: a platform window (platform/PlatWindow.h; a Win32 HWND in the original)
class LLE_API GEWindow {                    // sizeof 0x1c (implicit copy 0xe2e0)
public:
    // 0xe2b0: the origin is default-constructed; the rest is assigned in the body (store order)
    GEWindow(PlatWindow* inWindow, unsigned long inWidth, unsigned long inHeight, unsigned long inDepth,
             bool inIsFullScreen)
    {
        this->mWindow = inWindow;
        this->mIsFullScreen = inIsFullScreen;
        this->mDepth = inDepth;
        this->mWidth = inWidth;
        this->mHeight = inHeight;
    }
    ~GEWindow();
    const GE2DPoint& GetClientOrigin();
    unsigned long GetHeight() const { return this->mHeight; }
    PlatWindow* GetOSRepresentation() const { return this->mWindow; }
    // the system's own handle of the window (Win32: its HWND, for DirectInput and the IME until PORT.md steps 4.4
    // and 4.6; the X11 Window, the wl_surface or the CAMetalLayer elsewhere)
    void* GetNativeHandle() const;
    unsigned long GetWidth() const { return this->mWidth; }
    void Resize(unsigned long inWidth, unsigned long inHeight);

    PlatWindow* mWindow;                    // +0x00 (an HWND in the original)
    GE2DPoint mClientOrigin;                // +0x04
    unsigned long mWidth;                   // +0x0c
    unsigned long mHeight;                  // +0x10
    unsigned long mDepth;                   // +0x14 PROVISIONAL name (Resize adjusts the rect only when it is 1)
    bool mIsFullScreen;                     // +0x18 PROVISIONAL name (the dtor destroys the window when set)
};

#endif
