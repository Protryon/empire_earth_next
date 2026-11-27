// C:\EEX\Empire Earth\User Interface Thread\EEUICPing.h (RECONSTRUCTION; file path inferred). The lobby's ping meter
// control: five 10x10 bars 12 pixels apart, the first mNumBars + 1 drawn (RTTI EEUICPing > UIControl, vtable 0x449f1c;
// EEUICPing.cpp 0x258464..0x25861c: ctor, ??_G, dtor, DrawPolygons, SetPing; the unit is gone). Whole in Rust (ee_screens ping.rs,
// round 29): mNumBars is the record EeScreensPingFields; the bars are Rust's (hud_draw.rs, HUD 2D round 1). Requires
// lle/UIControl.h.
#ifndef EE_EEUICPING_H
#define EE_EEUICPING_H

#include "ee/EEUIRustControl.h"
#include "rust/game/records.h"

class EEUICPing : public EEUIRustControl, protected EeScreensPingFields {
public:
    // the lobby's ping meter (60x10; its bars made, placed and drawn by ee_screens ping.rs and hud_draw.rs, which set
    // the lit count: DrawPolygons and SetPing forward)
    EEUICPing(unsigned long inId, const GE2DPoint& inLocation, UIForm* inForm, unsigned long inIndex)
        : EEUIRustControl(inId, GERectangle(inLocation, 60.0f, 10.0f), inForm, inIndex, 0), EeScreensPingFields()
    {
        ee_screens_ping_init(this, this->Host(), inLocation.GetX(), inLocation.GetY());
    }
    virtual long DrawPolygons() { return ee_screens_ping_draw(this, this->Host()); }
    virtual bool GetTabable() const { return false; }
    void SetPing(float inPing) { ee_screens_ping_set(this, this->Host(), inPing); }
};

#endif
