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

class LLE_API UITriStateCheckBox : public UIButton {
public:
    UITriStateCheckBox(unsigned long inId, const UWideString& inText, GEFont* inFont, const GERectangle& inRectangle,
                       UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness);
    // the dtor (0x100805c4) deleted the label: the Rust button's release does
    void ChangeWidth(unsigned long inWidth);
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    unsigned long GetCheckState() const;    // ICF-folded (0x100660be)
    UITextLabel* GetTextLabel() const;      // ICF-folded (0x100633b1)
    virtual unsigned long GetType() const { return kControlTypeTriStateCheckBox; }  // 0x100805e6
    void SetCheckState(unsigned long inState);
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetText(const UWideString& inText, GEFont* inFont);
protected:
    virtual void ButtonUp();                // 0x100805ea: the cycle, then UIButton's
    void CycleCheck();
    virtual void TabbedTo() { UIButton::TabbedTo(); }     // ICF-folded (0x1005fcac)
    virtual void TabbedFrom() { UIButton::TabbedFrom(); } // ICF-folded (0x1005fcb1)
    // the original's members (+0x104 mTextLabel, +0x108 mCheckState 0 off, 1 on, 2 "not", +0x10c mHasText; sizeof
    // 0x110) are the Rust button's record (checkobj.rs)
};

// UITriStateCheckBox::SetCheckState: the UIManager check bitmaps each state shows
enum UICheckState {
    kCheckStateOff = 0,  // SetCheckState case -> UIManager::GetCheckOff/GetCheckDownOff
    kCheckStateOn = 1,  // SetCheckState case -> UIManager::GetCheckOn/GetCheckDownOn
    kCheckStateNot = 2,  // SetCheckState case -> UIManager::GetCheckNot/GetCheckDownNot
};

#endif
