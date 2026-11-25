// C:\EEX\Low-Level Engine\User Interface\UITriStateCheckBox.h (RECONSTRUCTION). A check box that cycles through
// off, on and "not" (control type 12). UITriStateCheckBox.cpp (0x100805c4..0x10080c07) is the first includer: it
// emits the inline members (dtor, GetType, ButtonUp), the implicit copy ctor/operator= and ??_E first, then the U2D
// key chains and its own code. Vtable 0x10099b5c (UIButton's 37 slots, no new virtuals). sizeof 0x110 (copy ctor).
// Requires lle/UIButton.h, lle/UITextLabel.h.
//
// The class is whole in Rust (rust/crates/ee_ui/src/checkobj.rs, R6 ui round 12): its record (the label, the check
// state, has-text) is the Rust button's, and every method forwards to it.
#ifndef LLE_UITRISTATECHECKBOX_H
#define LLE_UITRISTATECHECKBOX_H

#include "lle/UIButton.h"
#include "lle/UITextLabel.h"   // the inline dtor deletes the label
#include "lle/UIButtonRust.h"
#include "rust/ui/ids.h"
#include "rust/ui/checkobj.h"

class LLE_API UITriStateCheckBox : public UIButton {
public:
    UITriStateCheckBox(unsigned long inId, const UWideString& inText, GEFont* inFont, const GERectangle& inRectangle,
                       UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
        : UIButton(inId, UIButtonRust::CheckOff(inForm), NULL, UIButtonRust::CheckDownOff(inForm),
                   UIButtonRust::Rect(ee_ui_button_box_rect(UIButtonRust::Rect(inRectangle), false)), inForm, inIndex,
                   inBorderThickness)
    {
        Call(EeUiButtonCall::TriInit, (intptr_t)&inText, (intptr_t)inFont, inRectangle.GetHeight());
    }
    // the dtor (0x100805c4) deleted the label: the Rust button's release does
    void ChangeWidth(unsigned long inWidth) { Call(EeUiButtonCall::TriChangeWidth, (intptr_t)inWidth); }
    virtual long CopyData() { return (long)Call(EeUiButtonCall::TriCopyData); }
    virtual long DrawPolygons() { return (long)Call(EeUiButtonCall::TriDrawPolygons); }
    virtual long DrawTextA() { return (long)Call(EeUiButtonCall::TriDrawText); }
    // ICF-folded (0x100660be)
    unsigned long GetCheckState() const { return (unsigned long)Call(EeUiButtonCall::TriCheckState); }
    // ICF-folded (0x100633b1)
    UITextLabel* GetTextLabel() const { return (UITextLabel*)Call(EeUiButtonCall::TextLabel); }
    virtual unsigned long GetType() const { return kControlTypeTriStateCheckBox; }
    virtual void SetLocation(const GE2DPoint& inLocation)
    {
        Call(EeUiButtonCall::TriSetLocation, 0, 0, inLocation.GetX(), inLocation.GetY());
    }
    void SetText(const UWideString& inText, GEFont* inFont)
    {
        Call(EeUiButtonCall::TriSetText, (intptr_t)&inText, (intptr_t)inFont);
    }
    virtual void ButtonUp() { Call(EeUiButtonCall::TriButtonUp); }   // 0x100805ea: the cycle, then UIButton's
    virtual void TabbedTo() { UIButton::TabbedTo(); }     // ICF-folded (0x1005fcac)
    virtual void TabbedFrom() { UIButton::TabbedFrom(); } // ICF-folded (0x1005fcb1)
    // the original's members (+0x104 mTextLabel, +0x108 mCheckState 0 off, 1 on, 2 "not", +0x10c mHasText; sizeof
    // 0x110) are the Rust button's record (checkobj.rs)
    intptr_t Call(EeUiButtonCall inCall, intptr_t a = 0, intptr_t b = 0, float f0 = 0.0f, float f1 = 0.0f) const
    {
        return UIButtonRust::Call(this, inCall, a, b, f0, f1);
    }
};

// the enum (UI round 13): rust/ui/ids.h (ee_ui ids.rs)

#endif
