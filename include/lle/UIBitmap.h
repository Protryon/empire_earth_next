// C:\EEX\Low-Level Engine\User Interface\UIBitmap.h (RECONSTRUCTION). A control that draws one GEBitmap at its
// top left corner. Vtable 0x10098bfc. The original's sizeof was 0xb0 (??_E stride, 0x1005d46c).
//
// Whole in Rust (rust/crates/ee_ui/src/mediaobj.rs, R6 ui round 21): the members are EeUiBitmapFields, the class's
// private base; every method forwards there (inline: UIBitmap.cpp is gone), and the Rust record calls out through
// Op (UIControl's and the media's ops, lle/UIObjHost.h). Requires lle/UIControl.h.
#ifndef LLE_UIBITMAP_H
#define LLE_UIBITMAP_H

#include "lle/UIControl.h"
#include "rust/ui/records.h"
#include "rust/ui/lle_hosts.h"
#include "rust/ui/mediaobj.h"

class LLE_API UIBitmap : public UIControl, public EeUiBitmapFields {
public:
    UIBitmap(unsigned long inId, GEBitmap* inBitmap, const GERectangle& inRectangle, UIForm* inForm,
             unsigned long inIndex)
        : UIControl(kControlTypeBitmap, inId, inRectangle, inForm, inIndex, 0)
    {
        this->Call(EeUiBitmapCall::Init, (intptr_t)inBitmap);
    }
    virtual ~UIBitmap() { this->Call(EeUiBitmapCall::Release); }
    virtual long CopyData() { return (long)this->Call(EeUiBitmapCall::CopyData); }
    virtual long DrawPolygons() { return (long)this->Call(EeUiBitmapCall::DrawPolygons); }
    virtual long DrawTextA() { return (long)this->Call(EeUiBitmapCall::DrawText); }
    GEBitmap* GetBitmap() const { return static_cast<GEBitmap*>(this->mBitmap); }
    void SetBitmap(GEBitmap* inBitmap) { this->Call(EeUiBitmapCall::SetBitmap, (intptr_t)inBitmap); }
    virtual long UpdateKeyboard() { return kErrorOK; }
    virtual long MouseLButtonDown() { return kErrorOK; }
    virtual long MouseLButtonUp() { return kErrorOK; }
    virtual long MouseMove(const GE2DPoint& inLocation) { return kErrorOK; }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedTo() {}
    virtual void TabbedFrom() {}
    // the Rust record's calls out: UIControl's and the media's ops on this control (ee_ui lle_hosts.rs)
    static intptr_t Op(void* inObj, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)
    { return ee_ui_bitmap_host(static_cast<UIControl*>(static_cast<UIBitmap*>(static_cast<EeUiBitmapFields*>(inObj))), inOp, a, b, c, d, f); }
    intptr_t Call(EeUiBitmapCall inCall, intptr_t a = 0) { return ee_ui_bitmap_call(this, UIBitmap::Op, inCall, a); }
};

#endif
