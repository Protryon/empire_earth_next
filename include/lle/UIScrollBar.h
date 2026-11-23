// C:\EEX\Low-Level Engine\User Interface\UIScrollBar.h (RECONSTRUCTION). A UISlider between two UIScrollButtons
// (decrement at the top/left, increment at the bottom/right). Vtable 0x10098f0c (UIControl's 34 slots). The original's
// sizeof was 0xb0 (the implicit copy ctor 0x1005f417; the object is never copied).
//
// The class is whole in Rust (rust/crates/ee_ui/src/scrollbarobj.rs, R6 ui round 10): its state (the parts and
// which was pressed) and every method are the Rust object mRust; UIScrollBar.cpp holds the forwarders, the ctor and
// dtor and the host (the op switch, the parts' new and delete). Requires lle/UIControl.h (includes lle/UISlider.h).
#ifndef LLE_UISCROLLBAR_H
#define LLE_UISCROLLBAR_H

#include "lle/UISlider.h"
#include "rust/ui/scrollbarobj.h"


// The width of a scroll bar (0x1009916c, one .rdata constant: the edit and list boxes, the scroll bar and the check
// box load it). Inferred home.
#define kScrollBarWidth 20.0f   // a #define: every user reads the one pooled literal (lle 0x1009916c, exe 0x43bff4 __real 20.0f; exe UIListBox::SetLocation 0x5ad1f2)

class LLE_API UIScrollBar : public UIControl {
public:
    UIScrollBar(unsigned long inId, const GE2DPoint& inLocation, float inLength, unsigned long inStyle, UIForm* inForm,
                unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~UIScrollBar();
    virtual long CopyData() { return (long)this->Call(EeUiScrollBarCall::CopyData); }
    virtual long DrawPolygons() { return (long)this->Call(EeUiScrollBarCall::DrawPolygons); }
    virtual long DrawTextA() { return (long)this->Call(EeUiScrollBarCall::DrawText); }
    const UISlider* GetSlider() const { return reinterpret_cast<const UISlider*>(this->Call(EeUiScrollBarCall::Slider)); }
    virtual bool GetTabable() const { return false; }
    void IncrementBy(long inAmount) { this->Call(EeUiScrollBarCall::IncrementBy, inAmount); }
    virtual long MouseLButtonDown() { return (long)this->Call(EeUiScrollBarCall::MouseLButtonDown); }
    virtual long MouseLButtonUp() { return (long)this->Call(EeUiScrollBarCall::MouseLButtonUp); }
    virtual long MouseMove(const GE2DPoint& inLocation) { return (long)this->CallF(EeUiScrollBarCall::MouseMove, inLocation.GetX(), inLocation.GetY()); }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    void SetCurrentRangeValue(unsigned long inValue) { this->Call(EeUiScrollBarCall::SetCurrentRangeValue, inValue); }
    virtual void SetBgColor(const GEColor& inColor) { this->CallF(EeUiScrollBarCall::SetBgColor, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue()); }
    virtual void SetBgOpacity(float inOpacity) { this->CallF(EeUiScrollBarCall::SetBgOpacity, inOpacity); }
    virtual void SetLocation(const GE2DPoint& inLocation) { this->CallF(EeUiScrollBarCall::SetLocation, inLocation.GetX(), inLocation.GetY()); }
    void SetPageSize(unsigned long inPageSize) { this->Call(EeUiScrollBarCall::SetPageSize, inPageSize); }
    void SetRange(unsigned long inMin, unsigned long inMax) { this->Call(EeUiScrollBarCall::SetRange, inMin, inMax); }
    virtual void TabbedFrom() {}
    virtual void TabbedTo() {}
    virtual long UpdateKeyboard() { return kErrorOK; }
    intptr_t Call(EeUiScrollBarCall c, intptr_t a = 0, intptr_t b = 0, float* f = NULL) const { return ee_ui_scroll_bar_call(this->mRust, c, a, b, f); }
    intptr_t CallF(EeUiScrollBarCall c, float f0, float f1 = 0.0f, float f2 = 0.0f) const { return ee_ui_scroll_bar_call(this->mRust, c, 0, 0, UIF4(f0, f1, f2)); }
    EeUiScrollBar* mRust;                   // the state (scrollbarobj.rs)
};

#endif
