// New (port): the base of the game's own controls whose logic is Rust (R6 game screens round 29; ee_screens
// hud_ctl.rs): EEUICPing, EEUICChatLine, EEUICSelectUnits, EEUICCommandUnits. Each keeps its own members as a Rust
// record (its protected base, EeScreens*Fields: the members under their C++ names) and the C++ objects it owns
// as members; its methods forward to ee_screens. The one host, ee_screenscpp_hud_ctl, answers any control's ops
// (HostOp: the toolkit's virtuals and base members, the mouse, the keyboard, the game view) and passes the others to
// the class's RustOp. The inert mouse, tab and keyboard slots of these controls are here. Requires lle/UIControl.h.
#ifndef EE_EEUIRUSTCONTROL_H
#define EE_EEUIRUSTCONTROL_H

#include "rust/game/screens.h"

class EEUIRustControl : public UIControl {
public:
    EEUIRustControl(unsigned long inId, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                    unsigned long inBorderThickness)
        : UIControl(kControlTypeCustom, inId, inRectangle, inForm, inIndex, inBorderThickness) {}
    virtual ~EEUIRustControl() { ee_screens_hud_draw_free(this); }   // its draw objects (ee_screens hud_draw.rs)
    // the host (ee_screenscpp_hud_ctl): EE_SCREENS_HC_* here, the class's own ops in RustOp
    static int64_t HostOp(EEUIRustControl* inControl, uint32_t inOp, int64_t inA, int64_t inB, float* ioF);
protected:
    void* Host() { return this; }   // the host's object (what ee_screenscpp_hud_ctl casts back)
    virtual int64_t RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF) { return 0; }
    virtual long MouseLButtonDown() { return 0; }
    virtual long MouseLButtonUp() { return 0; }
    virtual long MouseMove(const GE2DPoint& inLocation) { return 0; }
    virtual long MouseRButtonDown() { return 0; }
    virtual long MouseRButtonUp() { return 0; }
    virtual void TabbedTo() {}
    virtual void TabbedFrom() {}
    virtual long UpdateKeyboard() { return 0; }
};

#endif
