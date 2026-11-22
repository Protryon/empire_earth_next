// C:\EEX\Low-Level Engine\User Interface\UISlider.h (RECONSTRUCTION). A thumb (the UI drag-bar bitmap) moving along
// a horizontal (style 0) or vertical (style 1) track; the scroll bar's middle part. Vtable 0x10098ddc (UIControl's
// 34 slots, then ButtonDown, ButtonUp, ChangeRange). The original's sizeof was 0xf8 (the implicit copy ctor
// 0x1005efdc; the object is never copied).
//
// The class is whole in Rust (rust/crates/ee_ui/src/sliderobj.rs, R6 ui round 10): its state and every method are
// the Rust object mRust; UISlider.cpp holds the forwarders, the ctor and dtor and the host (the op switch). The
// virtuals stay virtual: EEUIFSEESElevation overrides ChangeRange. Requires lle/UIControl.h.
#ifndef LLE_UISLIDER_H
#define LLE_UISLIDER_H

#include "lle/GETextureCoordinate.h"
#include "lle/GEBitmap.h"      // the units including this header rely on it
#include "rust/ui/sliderobj.h"

struct EeUiSlider;

class LLE_API UISlider : public UIControl {
public:
    UISlider(unsigned long inId, const GERectangle& inRectangle, unsigned long inSliderStyle, UIForm* inForm,
             unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~UISlider();
    virtual long CopyData() { return (long)this->Call(EeUiSliderCall::CopyData); }
    virtual long DrawPolygons() { return (long)this->Call(EeUiSliderCall::DrawPolygons); }
    long GetCurrentRangeValue() const { return (long)this->Call(EeUiSliderCall::CurrentRangeValue); }
    long GetMaxRange() const { return (long)this->Call(EeUiSliderCall::MaxRange); }
    unsigned long GetSliderStyle() const { return (unsigned long)this->Call(EeUiSliderCall::Style); }
    virtual bool GetTabable() const { return false; }
    void SetCurrentRangeValue(long inValue) { this->Call(EeUiSliderCall::SetCurrentRangeValue, inValue); }
    virtual void SetLocation(const GE2DPoint& inLocation) { this->CallAt(EeUiSliderCall::SetLocation, inLocation); }
    void SetPageSize(unsigned long inPageSize) { this->Call(EeUiSliderCall::SetPageSize, inPageSize); }
    void SetRange(unsigned long inMin, unsigned long inMax) { this->Call(EeUiSliderCall::SetRange, inMin, inMax); }
    virtual long UpdateKeyboard() { return kErrorOK; }
    virtual void ButtonDown() {}            // slot 0x88
    virtual void ButtonUp() { this->Call(EeUiSliderCall::ButtonUp); } // slot 0x8c
    virtual void ChangeRange(unsigned long inValue) { this->Call(EeUiSliderCall::ChangeRange, inValue); } // slot 0x90
    void DecrementByPageSize() { this->Call(EeUiSliderCall::DecrementByPageSize); }
    void IncrementByPageSize() { this->Call(EeUiSliderCall::IncrementByPageSize); }
    virtual long MouseLButtonDown() { return (long)this->Call(EeUiSliderCall::MouseLButtonDown); }
    virtual long MouseLButtonUp() { return (long)this->Call(EeUiSliderCall::MouseLButtonUp); }
    virtual long MouseMove(const GE2DPoint& inLocation) { return (long)this->CallAt(EeUiSliderCall::MouseMove, inLocation); }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedFrom() {}
    virtual void TabbedTo() {}

    intptr_t Call(EeUiSliderCall c, intptr_t a = 0, intptr_t b = 0, float* f = NULL) const { return ee_ui_slider_call(this->mRust, c, a, b, f); }
    intptr_t CallAt(EeUiSliderCall c, const GE2DPoint& p) const { float f[4] = {p.GetX(), p.GetY(), 0.0f, 0.0f}; return ee_ui_slider_call(this->mRust, c, 0, 0, f); }
    EeUiSlider* mRust;                      // the state (sliderobj.rs)
};

#endif
