// C:\EEX\Low-Level Engine\User Interface\UIRadioButton.h (RECONSTRUCTION). A UIButton checked exclusively
// within its entity group (control type 6). UIRadioButton.cpp (0x1007bbff..) held the ctor and the out-of-line
// members; the inline members are emitted first by UICheckBox.cpp (0x1005fc41..). Vtable 0x100990c8 (UIButton's 37
// slots, then Check, UnCheck). Owner: the utils worker; started by ui-core with what UIForm.cpp needs (GetIsChecked).
// Requires lle/UIButton.h (ui-core added the inline members UICheckBox.cpp emits, the copy-ctor-proven bools and the
// exported member declarations).
//
// The class is whole in Rust (rust/crates/ee_ui/src/checkobj.rs, R6 ui round 12): its record (the other check
// state's bitmaps, the label, the checked, has-text, push-button and scaled-bitmap flags) is the Rust button's, and
// every method forwards to it. Check and UnCheck stay virtual: the scenario editor's tabs and tool buttons override
// them, and the Rust button calls them through UIButton.cpp's host (the radio walk unchecks a sibling by its index).
#ifndef LLE_UIRADIOBUTTON_H
#define LLE_UIRADIOBUTTON_H

#include "lle/UIButton.h"
#include "lle/UIButtonRust.h"
#include "rust/ui/checkobj.h"
#include "lle/UITextLabel.h"   // the inline dtor deletes the label

class LLE_API UIRadioButton : public UIButton {
public:
    UIRadioButton(unsigned long inId, const UWideString& inText, GEFont* inFont, const GERectangle& inRectangle,
                  UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness, bool inIsPushButton,
                  bool inIsChecked, bool inScaleBitmap)
        : UIButton(inId, inForm->GetManager()->GetRadioOff(), NULL, inForm->GetManager()->GetRadioDownOff(),
               UIButtonRust::Rect(ee_ui_button_box_rect(UIButtonRust::Rect(inRectangle), inScaleBitmap)),
               inForm, inIndex, inBorderThickness)
{
    this->SubCall(EeUiButtonCall::RadioInit, (intptr_t)&inText, (intptr_t)inFont, inRectangle.GetHeight(), inIsPushButton ? 1.0f : 0.0f,
                inIsChecked ? 1.0f : 0.0f, inScaleBitmap ? 1.0f : 0.0f);
}
    // the dtor (0x1005fc41) deleted the label and released the other state's bitmaps: the Rust button's release does
    // Checks the button: unchecks its checked neighbour, swaps to the checked bitmaps and fires the checked trigger
    virtual void Check() { this->SubCall(EeUiButtonCall::RadioCheck); } // slot 0x94
    // Snapshots the button and its label (deleted once its text is cleared); a checked push button loses its border
    virtual long CopyData() { return (long)this->SubCall(EeUiButtonCall::RadioCopyData); }
    // Draws the button (its bitmap scaled to the control height when scaled), then its label
    virtual long DrawPolygons() { return (long)this->SubCall(EeUiButtonCall::RadioDrawPolygons); }
    // Draws the button's text and its label
    virtual long DrawTextA() { return (long)this->SubCall(EeUiButtonCall::RadioDrawText); }
    bool GetIsChecked() const { return this->SubCall(EeUiButtonCall::RadioIsChecked) != 0; }
    UITextLabel* GetTextLabel() const { return (UITextLabel*)this->SubCall(EeUiButtonCall::TextLabel); }
    virtual bool GetTabable() const { return this->SubCall(EeUiButtonCall::RadioTabable) != 0; } // 0x1005fc88: not a push button
    virtual unsigned long GetType() const { return kControlTypeRadioButton; }
    // Sets the pressed bitmap shown while checked
    void SetDownCheckedBitmap(GEBitmap* inBitmap) { this->SubCall(EeUiButtonCall::RadioSetDownChecked, (intptr_t)inBitmap); }
    // Sets the pressed bitmap shown while unchecked
    void SetDownUncheckedBitmap(GEBitmap* inBitmap) { this->SubCall(EeUiButtonCall::RadioSetDownUnchecked, (intptr_t)inBitmap); }
    // Moves the button, keeping its label to the right of the bitmap
    virtual void SetLocation(const GE2DPoint& inLocation) { this->SubCall(EeUiButtonCall::RadioSetLocation, 0, 0, inLocation.GetX(), inLocation.GetY()); }
    // Sets the label text, creating the label right of the bitmap on first use; empty text hides the label
    void SetText(const UWideString& inText, GEFont* inFont) { this->SubCall(EeUiButtonCall::RadioSetText, (intptr_t)&inText, (intptr_t)inFont); }
    // Sets the normal bitmap shown while checked
    void SetUpCheckedBitmap(GEBitmap* inBitmap) { this->SubCall(EeUiButtonCall::RadioSetUpChecked, (intptr_t)inBitmap); }
    // Sets the normal bitmap shown while unchecked
    void SetUpUncheckedBitmap(GEBitmap* inBitmap) { this->SubCall(EeUiButtonCall::RadioSetUpUnchecked, (intptr_t)inBitmap); }
    // Checks the button (the virtual Check), then releases it
    virtual void ButtonUp() { this->SubCall(EeUiButtonCall::RadioButtonUp); } // 0x1005fc98: the virtual Check, then UIButton's
    // Presses the button, except a checked push button
    virtual long MouseLButtonDown() { return (long)this->SubCall(EeUiButtonCall::RadioMouseLButtonDown); }
    // Releases the button (plain button behaviour)
    virtual long MouseLButtonUp() { return UIButton::MouseLButtonUp(); }
    virtual void TabbedTo() { UIButton::TabbedTo(); }
    virtual void TabbedFrom() { UIButton::TabbedFrom(); }
    // Unchecks the button: swaps to the unchecked bitmaps, restores its border and fires the unchecked trigger
    virtual void UnCheck() { this->SubCall(EeUiButtonCall::RadioUnCheck); } // slot 0x98
    // the original's members (+0x104 mOtherNormalBitmap, +0x108 mOtherPressedBitmap, +0x10c mTextLabel, +0x110
    // mIsChecked, +0x111 mHasText, +0x112 mIsPushButton, +0x113 mScaleBitmap; sizeof 0x114) and the private
    // RemoveOtherChecks are the Rust button's (checkobj.rs)
};

#endif
