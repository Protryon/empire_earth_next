// C:\EEX\Low-Level Engine\Graphics Engine\GEScreen.h (RECONSTRUCTION). Abstract in the original (DX7HRDisplay
// DX7Screen 0x10470 implemented it); the game links one renderer, so it is DX7Screen itself.
#ifndef LLE_GESCREEN_H
#define LLE_GESCREEN_H
#include "rust/render/dx7.h"

#include "lle/LLEForward.h"
// A display mode (DX7HRDisplay: DX7Screen::EnumVideoModeCallback 0x1000dc67 fills it from a DDSURFACEDESC2, SetMode
// 0x1000de5d and the ctor read it back; vector element stride 0x14). Names are provisional.
typedef EeDx7VideoMode GEVideoMode;          // the record is Rust's (ee_render dx7 screen.rs)
struct HINSTANCE__;

class GXDirectDraw;

// The game's one screen (LLE round 13: DX7Screen folded in, its virtuals gone), a GEScreen over one DirectDraw
// device (one per monitor), whole in Rust (ee_render dx7 scrobj.rs: the modes, the desktop rectangle, the name, the
// device, the flags and the screen list). The model store gate (tools/eemodel/storegate) links its own as the
// ee_dx7sc_* functions.
class LLE_API GEScreen {                    // sizeof 0xc (copy ctor 0xe331)
public:
    // Lists the device's display modes, finds the current one, probes 32-bit support and registers the screen
    // (DX7Screen's private ctor 0x1000d271; DX7Rasterizer.cpp: the host is there)
    GEScreen(void* inMonitor, GXDirectDraw* inDirectDraw, const UString& inName, bool inAttachedToDesktop,
             bool inSharesPrimary);
    ~GEScreen() { ee_dx7sc_destroy(this->mDX7); }
    operator GXDirectDraw*() { return static_cast<GXDirectDraw*>(ee_dx7sc_direct_draw(this->mDX7)); }

    long Capture(const GEWindow* inWindow, bool inToClipboard) { return ee_dx7sc_capture(this->mDX7, const_cast<GEWindow*>(inWindow), inToClipboard); }
    long CreateSurface(GESurfaceDescription& inDescription, GESurface** outSurface)
        { return ee_dx7sc_create_surface(this->mDX7, reinterpret_cast<EeDx7SurfaceDescription*>(&inDescription), reinterpret_cast<void**>(outSurface)); }
    // Creates a window (a platform window: style 1 a framed, resizable window with an inWidth x inHeight client area,
    // else a topmost borderless one at inOrigin; DX7Rasterizer.cpp). VC6 bound temporaries to non-const references:
    // the strings are const here.
    long CreateAWindow(const GE2DPoint& inOrigin, unsigned long inWidth, unsigned long inHeight,
                       const UString& inClassName, const UString& inTitle, GEWindow*& outWindow, unsigned long inStyle,
                       GEWindow* inParent);
    bool Get32BitDeviceSupport() { return ee_dx7sc_flag(this->mDX7, 5); }
    bool Get32BitTextureSupport() { return ee_dx7sc_flag(this->mDX7, 6); }
    const GEVideoMode& GetCurrentMode() const { return *reinterpret_cast<const GEVideoMode*>(ee_dx7sc_current_mode(this->mDX7)); }
    unsigned long GetCurrentModeIndex() const { return ee_dx7sc_current_mode_index(this->mDX7); }
    const GERectangle& GetDesktopRectangle() { return *reinterpret_cast<const GERectangle*>(ee_dx7sc_desktop_rectangle(this->mDX7)); }
    bool GetIsFullScreen() const { return ee_dx7sc_flag(this->mDX7, 1); }
    const GEVideoMode& GetMode(unsigned long inIndex) const { return *reinterpret_cast<const GEVideoMode*>(ee_dx7sc_mode(this->mDX7, inIndex)); }
    unsigned long GetModeCount() const { return ee_dx7sc_mode_count(this->mDX7); }
    const UString& GetName() const { return *static_cast<const UString*>(ee_dx7sc_name(this->mDX7)); }
    long Release(const GEWindow* inWindow) { return ee_dx7sc_release(this->mDX7, const_cast<GEWindow*>(inWindow)); }
    long RestoreAllSurfaces() { return ee_dx7sc_restore_all(this->mDX7); }
    long SetMode(unsigned long inIndex) { return ee_dx7sc_set_mode(this->mDX7, inIndex); }

    GETextureManager* GetTextureManager() const { return this->mTextureManager; }
    static HINSTANCE__* GetInstance() { return GEScreen::sInstance; }

    GETextureManager* mTextureManager = NULL;   // +0x04
    unsigned long mRasterizerIndex = 0;     // +0x08
    EeDx7Screen* mDX7;                      // the screen (ee_render dx7 scrobj.rs)
    static HINSTANCE__* sInstance;          // (private in the original) GEInitialize sets it (ee_render ge startup.rs)
};

#endif
