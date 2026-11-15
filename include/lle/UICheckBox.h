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
    void ChangeWidth(unsigned long inWidth);
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    bool GetIsChecked() const;
    UITextLabel* GetTextLabel() const;
    virtual unsigned long GetType() const { return kControlTypeCheckBox; }
    void SetChecked(bool inChecked);
    void SetDisabled(bool inDisabled);
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetText(const UWideString& inText, GEFont* inFont);
protected:
    virtual void ButtonUp();                // 0x1005fa07: the toggle, then UIButton's
    virtual void ButtonDown();
    virtual long MouseLButtonDown();
    virtual long MouseLButtonUp();
    virtual long MouseMove(const GE2DPoint& inLocation);
    virtual void MouseOver();
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedTo() { UIButton::TabbedTo(); }
    virtual void TabbedFrom() { UIButton::TabbedFrom(); }
    // the original's members (+0x104 mTextLabel, +0x108 mIsChecked, +0x109 mHasText, +0x10a mScaleBitmap,
    // +0x10b mIsDisabled) are the Rust button's record (checkobj.rs)
};

#endif
