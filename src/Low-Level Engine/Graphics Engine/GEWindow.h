// C:\EEX\Low-Level Engine\Graphics Engine\GEWindow.h (RECONSTRUCTION; __FILE__ anchor in the binary). GEDeviceContext
// and GEWindow. GEWindow.cpp (0x10038020..0x10038527) defines GEDeviceContext's statics (their init chains come first)
// and holds the out-of-line members; the rest is inline (GEBitmap.cpp emits it first). The GEDCCriticalSection ctor's
// ESVERIFY is the original's line 48 (push 0x30 in sLock's initializer 0x3807b).
#ifndef LLE_GEWINDOW_H
#define LLE_GEWINDOW_H

#ifdef LOWLEVELENGINE_EXPORTS
#include <map>
#endif

struct HDC__;
struct PlatWindow;

// A window's GDI device context (Win32 only: text through GDI until PORT.md step 4.5; NULL elsewhere). A NULL window
// is the screen's.
class LLE_API GEDeviceContext {
public:
    static HDC__* GetDeviceContext(PlatWindow* inWindow);
    static long ReleaseDeviceContext(PlatWindow* inWindow, HDC__* inContext);
    // implicit operator= exported (ICF-folded 0x37fb)

private:
    // A critical section per window device context (sizeof 4)
    struct GEDCCriticalSection {
        GEDCCriticalSection();
        ~GEDCCriticalSection();

        TSCriticalSection* mLock;
    };

    // PROVISIONAL GEWindow.cpp-only visibility: a static data member consumes a VC6 $E number in every unit that sees
    // the class, and ~15 engine units include this header (their $E binds were recorded without these).
#ifdef LOWLEVELENGINE_EXPORTS
    static std::map<PlatWindow*, GEDCCriticalSection> sHDC;     // 0x1012d8c0
    static GEDCCriticalSection sLock;                       // 0x1012d8b4
#endif
};

// ----------------------------------------------------------------------------------------------------------------
// GEDeviceContext::GEDCCriticalSection inline members (used only by GEWindow.cpp's static initializers)
// ----------------------------------------------------------------------------------------------------------------
//
//
//
//

inline GEDeviceContext::GEDCCriticalSection::GEDCCriticalSection()
{
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mLock) == keseOK, 48);
}

inline GEDeviceContext::GEDCCriticalSection::~GEDCCriticalSection()
{
    TSScheduler::DestroyCriticalSection(this->mLock);
}

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
