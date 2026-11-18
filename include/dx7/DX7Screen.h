// DX7Screen (RECONSTRUCTION): a GEScreen over one DirectDraw device (one per monitor), whole in Rust
// (rust/crates/ee_render/src/dx7/scrobj.rs, R4.3 part 14: the modes, the desktop rectangle, the name, the device, the flags and
// the screen list). The class keeps its vtable, CreateAWindow (the platform window calls, DX7Trampolines.cpp) and one
// member, the object. The original's layout: sizeof 0x44 (ctor 0x1000d271).
#ifndef DX7_DX7SCREEN_H
#define DX7_DX7SCREEN_H

#include "dx7/DX7Common.h"
#include "lle/GERectangle.h"
#include "lle/GEScreen.h"
#include "rust/render/dx7.h"

class DX7_API DX7Screen : public GEScreen {
public:
    DX7Screen(void* inMonitor, GXDirectDraw* inDirectDraw, const UString& inName, bool inAttachedToDesktop,
              bool inSharesPrimary);
    virtual ~DX7Screen() { ee_dx7sc_destroy(this->mDX7); }
    operator GXDirectDraw*() { return static_cast<GXDirectDraw*>(ee_dx7sc_direct_draw(this->mDX7)); }

    virtual long Capture(const GEWindow* inWindow, bool inToClipboard)
        { return ee_dx7sc_capture(this->mDX7, const_cast<GEWindow*>(inWindow), inToClipboard); }
    virtual long CreateSurface(GESurfaceDescription& inDescription, GESurface** outSurface)
        { return ee_dx7sc_create_surface(this->mDX7, reinterpret_cast<EeDx7SurfaceDescription*>(&inDescription), reinterpret_cast<void**>(outSurface)); }
    virtual long CreateAWindow(const GE2DPoint& inOrigin, unsigned long inWidth, unsigned long inHeight,
                               UString& inClassName, UString& inTitle, GEWindow*& outWindow, unsigned long inStyle,
                               GEWindow* inParent);
    virtual bool Get32BitDeviceSupport() { return ee_dx7sc_flag(this->mDX7, 5); }
    virtual bool Get32BitTextureSupport() { return ee_dx7sc_flag(this->mDX7, 6); }
    virtual const GEVideoMode& GetCurrentMode() const { return *reinterpret_cast<const GEVideoMode*>(ee_dx7sc_current_mode(this->mDX7)); }
    virtual unsigned long GetCurrentModeIndex() const { return ee_dx7sc_current_mode_index(this->mDX7); }
    virtual const GERectangle& GetDesktopRectangle() { return *reinterpret_cast<const GERectangle*>(ee_dx7sc_desktop_rectangle(this->mDX7)); }
    virtual bool GetIsFullScreen() const { return ee_dx7sc_flag(this->mDX7, 1); }
    virtual const GEVideoMode& GetMode(unsigned long inIndex) const { return *reinterpret_cast<const GEVideoMode*>(ee_dx7sc_mode(this->mDX7, inIndex)); }
    virtual unsigned long GetModeCount() const { return ee_dx7sc_mode_count(this->mDX7); }
    virtual const UString& GetName() const { return *static_cast<const UString*>(ee_dx7sc_name(this->mDX7)); }
    static DX7Screen* GetScreen(unsigned long inIndex) { return static_cast<DX7Screen*>(ee_dx7sc_screen(inIndex)); }
    static unsigned long GetScreenCount() { return ee_dx7sc_screen_count(); }
    virtual long Release(const GEWindow* inWindow) { return ee_dx7sc_release(this->mDX7, const_cast<GEWindow*>(inWindow)); }
    virtual long RestoreAllSurfaces() { return ee_dx7sc_restore_all(this->mDX7); }
    virtual long SetMode(unsigned long inIndex) { return ee_dx7sc_set_mode(this->mDX7, inIndex); }

    EeDx7Screen* mDX7;
};

#endif
