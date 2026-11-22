// C:\EEX\Low-Level Engine\User Interface\UIButton.h (RECONSTRUCTION). A three-state bitmap button with an optional
// text label. UIButton.cpp (0x1005e47c..0x1005ef4b) holds the ctor and the out-of-line members; the inline members
// are emitted first by UIBlinkingButton.cpp (0x1005db0f..0x1005df23). Vtable 0x10098cac (37 slots: UIControl's
// 34, then ButtonDown, ButtonUp, MouseOver). sizeof 0x104 (padded from the last bool at +0x102).
//
// The class is whole in Rust (rust/crates/ee_ui/src/buttonobj.rs, R6 ui round 11): its state (the text rectangle,
// the bitmaps, the state, the texts, the sounds, the presses) and every method are the Rust object mRust; UIButton.cpp
// holds the forwarders, the ctor and dtor and the host (the op switch). The virtuals stay virtual: the toolkit's and
// the game's subclasses override them. Requires lle/UIControl.h, lle/GEBitmap.h.
#ifndef LLE_UIBUTTON_H
#define LLE_UIBUTTON_H

#include "lle/UIControl.h"
#include "lle/UIButtonState.h"
#include "rust/ui/buttonobj.h"

class GEBitmap;
class GEText;
struct EeUiButton;

class LLE_API UIButton : public UIControl {
public:
    UIButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap, GEBitmap* inPressedBitmap,
             const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~UIButton();
    virtual void ChangeFont(GEFont* inFont) { this->Call(EeUiButtonCall::ChangeFont, (intptr_t)inFont); }
    virtual long CopyData() { return (long)this->Call(EeUiButtonCall::CopyData); }
    virtual long DrawPolygons() { return (long)this->Call(EeUiButtonCall::DrawPolygons); }
    virtual long DrawTextA() { return (long)this->Call(EeUiButtonCall::DrawText); }
    GE2DPoint GetBitmapLocation() const;
    unsigned long GetState() const { return (unsigned long)this->Call(EeUiButtonCall::State); } // UIButtonState
    GEText* GetGEText() { return (GEText*)this->Call(EeUiButtonCall::Text); }
    GEBitmap* GetNormalBitmap() const { return (GEBitmap*)this->Call(EeUiButtonCall::NormalBitmap); }
    GEBitmap* GetPressedBitmap() const { return (GEBitmap*)this->Call(EeUiButtonCall::PressedBitmap); }
    void SetBitmapDrawOffset(const GE2DPoint& inOffset) { this->CallF(EeUiButtonCall::SetBitmapDrawOffset, inOffset.GetX(), inOffset.GetY()); }
    void SetButtonDownSound(const UString& inName) { this->Call(EeUiButtonCall::SetButtonDownSound, (intptr_t)&inName); }
    void SetButtonUpSound(const UString& inName) { this->Call(EeUiButtonCall::SetButtonUpSound, (intptr_t)&inName); }
    void SetFocusedBitmap(GEBitmap* inBitmap) { this->Call(EeUiButtonCall::SetFocusedBitmap, (intptr_t)inBitmap); }
    void SetFocusedTextColor(const GEColor& inColor) { this->CallF(EeUiButtonCall::SetFocusedTextColor, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue()); }
    virtual void SetLocation(const GE2DPoint& inLocation) { this->CallF(EeUiButtonCall::SetLocation, inLocation.GetX(), inLocation.GetY()); }
    void SetNormalBitmap(GEBitmap* inBitmap) { this->Call(EeUiButtonCall::SetNormalBitmap, (intptr_t)inBitmap); }
    void SetPressedBitmap(GEBitmap* inBitmap) { this->Call(EeUiButtonCall::SetPressedBitmap, (intptr_t)inBitmap); }
    void SetText(const UWideString& inText, GEFont* inFont) { this->Call(EeUiButtonCall::SetText, (intptr_t)&inText, (intptr_t)inFont); }
    void SetTextRectangle(const GERectangle& inRectangle)
    {
        this->CallF(EeUiButtonCall::SetTextRectangle, inRectangle.mBottom, inRectangle.mLeft, inRectangle.mRight,
                            inRectangle.mTop);
    }
    virtual long UpdateKeyboard() { return (long)this->Call(EeUiButtonCall::UpdateKeyboard); }   // Fixed (kept): a key press on it
    virtual void ButtonDown() { this->Call(EeUiButtonCall::ButtonDown); } // slot 0x88
    virtual void ButtonUp() { this->Call(EeUiButtonCall::ButtonUp); }   // Fixed (kept): plays the button-up sound                // slot 0x8c
    virtual long MouseLButtonDown() { return (long)this->Call(EeUiButtonCall::MouseLButtonDown); }
    virtual long MouseLButtonUp() { return (long)this->Call(EeUiButtonCall::MouseLButtonUp); }
    virtual long MouseMove(const GE2DPoint& inLocation) { return (long)this->CallF(EeUiButtonCall::MouseMove, inLocation.GetX(), inLocation.GetY()); }
    virtual void MouseOver() { this->Call(EeUiButtonCall::MouseOver); } // slot 0x90
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedTo() { this->Call(EeUiButtonCall::TabbedTo); }
    virtual void TabbedFrom() { this->Call(EeUiButtonCall::TabbedFrom); }

    intptr_t Call(EeUiButtonCall c, intptr_t a = 0, intptr_t b = 0, float* f = NULL) const { return ee_ui_button_call(this->mRust, c, a, b, f); }
    intptr_t CallF(EeUiButtonCall c, float f0, float f1, float f2 = 0.0f, float f3 = 0.0f) const { float f[4] = {f0, f1, f2, f3}; return ee_ui_button_call(this->mRust, c, 0, 0, f); }
    // the subclasses' calls (UIButtonRust::Call's)
    intptr_t SubCall(EeUiButtonCall c, intptr_t a = 0, intptr_t b = 0, float f0 = 0.0f, float f1 = 0.0f, float f2 = 0.0f, float f3 = 0.0f) const { float f[4] = {f0, f1, f2, f3}; return ee_ui_button_call(this->mRust, c, a, b, f); }
    EeUiButton* mRust;                      // the state (buttonobj.rs)
};

#endif
