// C:\EEX\Low-Level Engine\User Interface\UIRadioButton.h (RECONSTRUCTION). A UIButton checked exclusively
// within its entity group (control type 6). UIRadioButton.cpp (0x1007bbff..) holds the ctor and the out-of-line
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
#include "lle/UITextLabel.h"   // the inline dtor deletes the label

class LLE_API UIRadioButton : public UIButton {
    friend struct UIButtonHost;             // UIButton.cpp: the Rust button's calls of Check and UnCheck
public:
    UIRadioButton(unsigned long inId, const UWideString& inText, GEFont* inFont, const GERectangle& inRectangle,
                  UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness, bool inIsPushButton,
                  bool inIsChecked, bool inScaleBitmap);
    // the dtor (0x1005fc41) deleted the label and released the other state's bitmaps: the Rust button's release does
    virtual void Check();                   // slot 0x94
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    bool GetIsChecked() const;
    UITextLabel* GetTextLabel() const;
    virtual bool GetTabable() const;        // 0x1005fc88: not a push button
    virtual unsigned long GetType() const { return kControlTypeRadioButton; }
    void SetDownCheckedBitmap(GEBitmap* inBitmap);
    void SetDownUncheckedBitmap(GEBitmap* inBitmap);
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetText(const UWideString& inText, GEFont* inFont);
    void SetUpCheckedBitmap(GEBitmap* inBitmap);
    void SetUpUncheckedBitmap(GEBitmap* inBitmap);
protected:
    virtual void ButtonUp();                // 0x1005fc98: the virtual Check, then UIButton's
    virtual long MouseLButtonDown();
    virtual long MouseLButtonUp();
    virtual void TabbedTo() { UIButton::TabbedTo(); }
    virtual void TabbedFrom() { UIButton::TabbedFrom(); }
    virtual void UnCheck();                 // slot 0x98
    // the original's members (+0x104 mOtherNormalBitmap, +0x108 mOtherPressedBitmap, +0x10c mTextLabel, +0x110
    // mIsChecked, +0x111 mHasText, +0x112 mIsPushButton, +0x113 mScaleBitmap; sizeof 0x114) and the private
    // RemoveOtherChecks are the Rust button's (checkobj.rs)
};

#endif
