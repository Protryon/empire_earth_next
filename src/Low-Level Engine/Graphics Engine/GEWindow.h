// C:\EEX\Low-Level Engine\Graphics Engine\GEWindow.h (RECONSTRUCTION; __FILE__ anchor in the binary). The game's
// window: a platform window (platform/PlatWindow.h; a Win32 HWND in the original), its size and client origin.
// port (R4.3, sixteenth part): the window is a whole Rust object (ee_ge smallobj.rs; GEWindow.cpp deleted). The header
// is included by most of the game, so it names the object without rust/ee_ge.h; the forwarders are in GEBitmap.cpp.
#ifndef LLE_GEWINDOW_H
#define LLE_GEWINDOW_H

struct PlatWindow;
struct EeGeWin;
class GE2DPoint;

class LLE_API GEWindow {
public:
    GEWindow(PlatWindow* inWindow, unsigned long inWidth, unsigned long inHeight, unsigned long inDepth,
             bool inIsFullScreen);
    ~GEWindow();   // destroys the platform window only in full-screen mode
    GEWindow(const GEWindow&) = delete;
    GEWindow& operator=(const GEWindow&) = delete;
    const GE2DPoint& GetClientOrigin();   // the screen position of the client area
    unsigned long GetHeight() const;
    PlatWindow* GetOSRepresentation() const;
    unsigned long GetWidth() const;
    void Resize(unsigned long inWidth, unsigned long inHeight);   // the client area (the platform sizes it)

private:
    EeGeWin* mRust;                         // the window (ee_ge smallobj.rs)
};

#endif
