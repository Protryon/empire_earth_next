// C:\EEX\Empire Earth\User Interface Thread\EEUICPing.h (RECONSTRUCTION; file path inferred). The lobby's ping meter
// control: five 10x10 bars 12 pixels apart, the first mNumBars + 1 drawn (RTTI EEUICPing > UIControl, vtable 0x449f1c;
// EEUICPing.cpp 0x258464..0x25861c: ctor, ??_G, dtor, DrawPolygons, SetPing). Whole in Rust (ee_screens ping.rs,
// round 29): mNumBars is the record EeScreensPingFields, the bars stay C++. Requires lle/UIControl.h, lle/GERhombus.h.
#ifndef EE_EEUICPING_H
#define EE_EEUICPING_H

#include "ee/EEUIRustControl.h"

class EEUICPing : public EEUIRustControl, protected EeScreensPingFields {
public:
    EEUICPing(unsigned long inId, const GE2DPoint& inLocation, UIForm* inForm, unsigned long inIndex);   // 0x258464
    virtual long DrawPolygons() { return ee_screens_ping_draw(this, this->Host()); }   // 0x2585a6
    virtual bool GetTabable() const { return false; }
    void SetPing(float inPing) { ee_screens_ping_set(this, this->Host(), inPing); }   // 0x2585e7
protected:
    virtual int64_t RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF);

    GERhombus mBars[5];
};

#endif
