// C:\EEX\Low-Level Engine\User Interface\UICheckBox.h (RECONSTRUCTION). A UIButton with a checked state and an
// optional text label (control type 2). UICheckBox.cpp (0x1005ef4b..0x100602e4) emits the inline members of UISlider,
// UIScrollButton, UIScrollBar, UITextLabel, UICheckBox and (after the U2D key chains) UIRadioButton first, then its
// own code (0x1005fdef..). ChangeWidth and DrawTextA (0x10080916, 0x10080a05) sit in a later unit. Vtable
// 0x10099030. Layout proven by the implicit copy ctor 0x1005fa18 (sizeof 0x10c). Requires lle/UIButton.h and
// lle/UITextLabel.h.
#ifndef LLE_UICHECKBOX_H
#define LLE_UICHECKBOX_H

#include "lle/UIButton.h"
#include "lle/UITextLabel.h"   // the inline dtor deletes the label

class UITextLabel;

class LLE_API UICheckBox : public UIButton {
    friend struct UIButtonRust;             // the Rust button logic's host calls (User Interface/UIButtonRust.h)
public:
    UICheckBox(unsigned long inId, const UWideString& inText, GEFont* inFont, const GERectangle& inRectangle,
               UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness, bool inScaleBitmap);
    virtual ~UICheckBox() { delete this->mTextLabel; }  // 0x1005f9da
    void ChangeWidth(unsigned long inWidth);
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    bool GetIsChecked() const { return this->mIsChecked; }  // 0x1005f9fc
    UITextLabel* GetTextLabel() const { return this->mTextLabel; }  // 0x100633b1 (ICF)
    virtual unsigned long GetType() const { return kControlTypeCheckBox; }  // 0x1005fa03
    void SetChecked(bool inChecked);
    void SetDisabled(bool inDisabled);
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetText(const UWideString& inText, GEFont* inFont);
protected:
    // 0x1005fa07
    virtual void ButtonUp()
    {
        this->ToggleCheck();
        UIButton::ButtonUp();
    }
    void ToggleCheck();
    virtual void ButtonDown();
    virtual long MouseLButtonDown();
    virtual long MouseLButtonUp();
    virtual long MouseMove(const GE2DPoint& inLocation);
    virtual void MouseOver();
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedTo() { UIButton::TabbedTo(); }     // 0x1005fcac (ICF)
    virtual void TabbedFrom() { UIButton::TabbedFrom(); } // 0x1005fcb1 (ICF)

    UITextLabel* mTextLabel;                // +0x104 (deleted by the dtor)
    bool mIsChecked;                        // +0x108
    bool mHasText;                          // +0x109 SetText: a non-empty text
    bool mScaleBitmap;                      // +0x10a the check bitmap scales with the box height
    bool mIsDisabled;                       // +0x10b
};

#endif
