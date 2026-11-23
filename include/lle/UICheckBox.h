// C:\EEX\Low-Level Engine\User Interface\UICheckBox.h (RECONSTRUCTION). A UIButton with a checked state and an
// optional text label (control type 2). UICheckBox.cpp (0x1005ef4b..0x100602e4) emits the inline members of UISlider,
// UIScrollButton, UIScrollBar, UITextLabel, UICheckBox and (after the U2D key chains) UIRadioButton first, then its
// own code (0x1005fdef..). ChangeWidth and DrawTextA (0x10080916, 0x10080a05) sit in a later unit. Vtable
// 0x10099030. Layout proven by the implicit copy ctor 0x1005fa18 (sizeof 0x10c). Requires lle/UIButton.h and
// lle/UITextLabel.h.
//
// The class is whole in Rust (rust/crates/ee_ui/src/checkobj.rs, R6 ui round 12): its record (the label, the
// checked, has-text, scaled-bitmap and disabled flags) is the Rust button's, and every method forwards to it.
#ifndef LLE_UICHECKBOX_H
#define LLE_UICHECKBOX_H

#include "lle/UIButton.h"
#include "lle/UITextLabel.h"   // the inline dtor deletes the label

class UITextLabel;

class LLE_API UICheckBox : public UIButton {
public:
    UICheckBox(unsigned long inId, const UWideString& inText, GEFont* inFont, const GERectangle& inRectangle,
               UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness, bool inScaleBitmap);
    // the dtor (0x1005f9da) deleted the label: the Rust button's release does
    // Resizes the check box and its label to a new width. No marker: ICF-folded into UITriStateCheckBox::ChangeWidth
    // (0x10080916), the same body over the same offsets.
    void ChangeWidth(unsigned long inWidth) { this->SubCall(EeUiButtonCall::CheckChangeWidth, (intptr_t)inWidth); }
    // Copies the button and its label for the draw pass, deleting the label once the text was cleared
    virtual long CopyData() { return (long)this->SubCall(EeUiButtonCall::CheckCopyData); }
    // Draws the box bitmap (scaled to the control height when scaling) and the label
    virtual long DrawPolygons() { return (long)this->SubCall(EeUiButtonCall::CheckDrawPolygons); }
    // Draws the control's text and the label. No marker: ICF-folded into UITriStateCheckBox::DrawTextA (0x10080a05).
    virtual long DrawTextA() { return (long)this->SubCall(EeUiButtonCall::CheckDrawText); }
    bool GetIsChecked() const { return this->SubCall(EeUiButtonCall::CheckIsChecked) != 0; }
    UITextLabel* GetTextLabel() const { return (UITextLabel*)this->SubCall(EeUiButtonCall::TextLabel); }
    virtual unsigned long GetType() const { return kControlTypeCheckBox; }
    // Checks or unchecks the box (unless disabled), swapping its bitmaps and firing the checked or unchecked trigger
    void SetChecked(bool inChecked) { this->SubCall(EeUiButtonCall::CheckSetChecked, inChecked); }
    // Enables or disables the box; a disabled box shows pressed, an enabled one normal
    // Fixed (kept): enabling shows the box normal, not pressed
    void SetDisabled(bool inDisabled) { this->SubCall(EeUiButtonCall::CheckSetDisabled, inDisabled); }
    // Moves the check box and its label
    virtual void SetLocation(const GE2DPoint& inLocation) { this->SubCall(EeUiButtonCall::CheckSetLocation, 0, 0, inLocation.GetX(), inLocation.GetY()); }
    // Sets the label text, creating the label on first use; empty text hides the label
    void SetText(const UWideString& inText, GEFont* inFont) { this->SubCall(EeUiButtonCall::CheckSetText, (intptr_t)&inText, (intptr_t)inFont); }
protected:
    // Toggles the check, then releases the button
    virtual void ButtonUp() { this->SubCall(EeUiButtonCall::CheckButtonUp); } // 0x1005fa07: the toggle, then UIButton's
    // Presses the box unless it is disabled
    virtual void ButtonDown() { this->SubCall(EeUiButtonCall::CheckButtonDown); }
    // Handles a left-button press unless the box is disabled
    virtual long MouseLButtonDown() { return (long)this->SubCall(EeUiButtonCall::CheckMouseLButtonDown); }
    // Handles a left-button release unless the box is disabled
    virtual long MouseLButtonUp() { return (long)this->SubCall(EeUiButtonCall::CheckMouseLButtonUp); }
    // Handles mouse movement unless the box is disabled
    virtual long MouseMove(const GE2DPoint& inLocation) { return (long)this->SubCall(EeUiButtonCall::CheckMouseMove, 0, 0, inLocation.GetX(), inLocation.GetY()); }
    // Shows the box pressed
    virtual void MouseOver() { UIButton::MouseOver(); }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedTo() { UIButton::TabbedTo(); }
    virtual void TabbedFrom() { UIButton::TabbedFrom(); }
    // the original's members (+0x104 mTextLabel, +0x108 mIsChecked, +0x109 mHasText, +0x10a mScaleBitmap,
    // +0x10b mIsDisabled) are the Rust button's record (checkobj.rs)
};

#endif
