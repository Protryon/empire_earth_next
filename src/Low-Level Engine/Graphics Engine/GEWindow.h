// C:\EEX\Low-Level Engine\Graphics Engine\GEWindow.h (RECONSTRUCTION; __FILE__ anchor in the binary). The game's
// window: a platform window (platform/PlatWindow.h; a Win32 HWND in the original), its size and client origin.
// port (R4.3, sixteenth part): the window is a whole Rust object (ee_ge smallobj.rs; GEWindow.cpp deleted). The header
// is included by most of the game, so it declares the five calls its inline members make instead of including
// rust/render/ge.h; the constructor and destructor (they name the host) are in GEBitmap.cpp.
#ifndef LLE_GEWINDOW_H
#define LLE_GEWINDOW_H

#include <stdbool.h>

struct PlatWindow;
#include "lle/LLEForward.h"
#include "rust/render/ge.h"   // the window (ee_ge_win_*)

class LLE_API GEWindow {
public:
    GEWindow(PlatWindow* inWindow, unsigned long inWidth, unsigned long inHeight, unsigned long inDepth,
             bool inIsFullScreen);
    ~GEWindow() { ee_ge_win_destroy(this->mRust); }   // destroys the platform window only in full-screen mode
    // the screen position of the client area
    const GE2DPoint& GetClientOrigin() { return *reinterpret_cast<const GE2DPoint*>(ee_ge_win_origin(this->mRust)); }
    unsigned long GetHeight() const { return ee_ge_win_size(this->mRust, true); }
    PlatWindow* GetOSRepresentation() const { return static_cast<PlatWindow*>(ee_ge_win_os(this->mRust)); }
    unsigned long GetWidth() const { return ee_ge_win_size(this->mRust, false); }
    // the client area (the platform sizes it)
    void Resize(unsigned long inWidth, unsigned long inHeight) { ee_ge_win_resize(this->mRust, inWidth, inHeight); }

    URust<EeGeWin> mRust;                         // the window (ee_ge smallobj.rs)
};

#endif
