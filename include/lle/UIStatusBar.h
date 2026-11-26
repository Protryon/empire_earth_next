// C:\EEX\Low-Level Engine\User Interface\UIStatusBar.h (RECONSTRUCTION). A progress bar: a coloured rhombus whose
// width follows a value in [min, max], with an optional centred text (control type 9). Vtable 0x100998d4 (UIControl's
// 34 slots, no new virtuals). The original's sizeof was 0x114 (the copy ctor; the object is never copied).
//
// The class is whole in Rust (rust/crates/ee_ui/src/statusobj.rs, R6 ui round 10): its state (the text areas and
// colour, the texts, the value and range) and every method are the Rust object mRust; the two rhombi stay members
// (GERhombus objects the rasterizer draws). UIStatusBar.cpp holds the forwarders, the ctor and dtor and the host.
// Requires lle/UIControl.h, lle/GERhombus.h, lle/GEText.h.
#ifndef LLE_UISTATUSBAR_H
#define LLE_UISTATUSBAR_H

#include "lle/UIControl.h"
#include "lle/GEText.h"
#include "rust/ui/statusobj.h"
#include "rust/ui/lle_hosts.h"


class LLE_API UIStatusBar : public UIControl {
public:
    UIStatusBar(unsigned long inId, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness) : UIControl(kControlTypeStatusBar, inId, inRectangle, inForm, inIndex, inBorderThickness) { this->mRust = ee_ui_status_bar_new(this, ee_ui_status_bar_host, inBorderThickness); }
    virtual ~UIStatusBar() { ee_ui_status_bar_free(this->mRust); }
    virtual void ChangeFont(GEFont* inFont) { this->Call(EeUiStatusBarCall::ChangeFont, (intptr_t)inFont); }
    virtual long CopyData() { return (long)this->Call(EeUiStatusBarCall::CopyData); }
    virtual long DrawPolygons() { return (long)this->Call(EeUiStatusBarCall::DrawPolygons); }
    virtual long DrawTextA() { return (long)this->Call(EeUiStatusBarCall::DrawText); }
    GEText* GetGEText() { return reinterpret_cast<GEText*>(this->Call(EeUiStatusBarCall::Text)); }
    virtual bool GetTabable() const { return false; }
    void SetCurrentRangeValue(long inValue) { this->Call(EeUiStatusBarCall::SetCurrentRangeValue, inValue); }
    void SetFocusedTextColor(const GEColor& inColor) { this->CallF(EeUiStatusBarCall::SetFocusedTextColor, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue()); }
    virtual void SetLocation(const GE2DPoint& inLocation) { this->CallF(EeUiStatusBarCall::SetLocation, inLocation.GetX(), inLocation.GetY()); }
    void SetRange(unsigned long inMin, unsigned long inMax) { this->Call(EeUiStatusBarCall::SetRange, inMin, inMax); }
    void SetStatusBarOpacity(float inOpacity) { this->CallF(EeUiStatusBarCall::SetStatusBarOpacity, inOpacity); }
    void SetStatusBarColor(const GEColor& inColor) { this->CallF(EeUiStatusBarCall::SetStatusBarColor, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue()); }
    void SetText(const UWideString& inText, GEFont* inFont) { this->Call(EeUiStatusBarCall::SetText, (intptr_t)&inText, (intptr_t)inFont); }
    virtual long UpdateKeyboard() { return kErrorOK; }
    virtual long MouseLButtonDown() { return kErrorOK; }
    virtual long MouseLButtonUp() { return kErrorOK; }
    virtual long MouseMove(const GE2DPoint& inLocation) { return kErrorOK; }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedTo() {}
    virtual void TabbedFrom() {}

    GERhombus mStatusRhombus;               // the bar
    GERhombus mCopiedStatusRhombus;         // the bar as of the last CopyData (inset by the border)
    intptr_t Call(EeUiStatusBarCall c, intptr_t a = 0, intptr_t b = 0, float* f = NULL) const { return ee_ui_status_bar_call(this->mRust, c, a, b, f); }
    intptr_t CallF(EeUiStatusBarCall c, float f0, float f1 = 0.0f, float f2 = 0.0f) const { return ee_ui_status_bar_call(this->mRust, c, 0, 0, UIF4(f0, f1, f2)); }
    EeUiStatusBar* mRust;                   // the state (statusobj.rs)
};

#endif
