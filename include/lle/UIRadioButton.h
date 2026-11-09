// C:\EEX\Low-Level Engine\User Interface\UIRadioButton.h (RECONSTRUCTION). A UIButton checked exclusively
// within its entity group (control type 6). UIRadioButton.cpp (0x1007bbff..) holds the ctor and the out-of-line
// members; the inline members are emitted first by UICheckBox.cpp (0x1005fc41..). Vtable 0x100990c8 (UIButton's 37
// slots, then Check, UnCheck). Owner: the utils worker; started by ui-core with what UIForm.cpp needs (GetIsChecked).
// Requires lle/UIButton.h (ui-core added the inline members UICheckBox.cpp emits, the copy-ctor-proven bools and the
// exported member declarations).
#ifndef LLE_UIRADIOBUTTON_H
#define LLE_UIRADIOBUTTON_H

#include "lle/UIButton.h"
#include "lle/UITextLabel.h"   // the inline dtor deletes the label

class LLE_API UIRadioButton : public UIButton {
    friend struct UIButtonRust;             // the Rust button logic's host calls (User Interface/UIButtonRust.h)
public:
    UIRadioButton(unsigned long inId, const UWideString& inText, GEFont* inFont, const GERectangle& inRectangle,
                  UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness, bool inIsPushButton,
                  bool inIsChecked, bool inScaleBitmap);   // 0x1007bbff
    // 0x1005fc41 (ui-core: the inline members UICheckBox.cpp emits)
    virtual ~UIRadioButton()
    {
        delete this->mTextLabel;
        if (this->mOtherNormalBitmap)
            this->mOtherNormalBitmap->Dereference();
        if (this->mOtherPressedBitmap)
            this->mOtherPressedBitmap->Dereference();
    }
    virtual void Check();                   // slot 0x94
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    bool GetIsChecked() const { return this->mIsChecked; }  // 0x1005fc81
    UITextLabel* GetTextLabel() const { return this->mTextLabel; }  // 0x100633db (ICF)
    virtual bool GetTabable() const { return !this->mIsPushButton; }  // 0x1005fc88
    virtual unsigned long GetType() const { return kControlTypeRadioButton; }  // 0x1005fc94
    void SetDownCheckedBitmap(GEBitmap* inBitmap);
    void SetDownUncheckedBitmap(GEBitmap* inBitmap);
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetText(const UWideString& inText, GEFont* inFont);
    void SetUpCheckedBitmap(GEBitmap* inBitmap);
    void SetUpUncheckedBitmap(GEBitmap* inBitmap);
protected:
    // 0x1005fc98
    virtual void ButtonUp()
    {
        this->Check();
        UIButton::ButtonUp();
    }
    virtual long MouseLButtonDown();
    virtual long MouseLButtonUp();
    virtual void TabbedTo() { UIButton::TabbedTo(); }     // 0x1005fcac
    virtual void TabbedFrom() { UIButton::TabbedFrom(); } // 0x1005fcb1
    virtual void UnCheck();                 // slot 0x98
private:
    void RemoveOtherChecks();

    GEBitmap* mOtherNormalBitmap;           // +0x104 the normal bitmap of the other check state (swapped by Check/UnCheck)
    GEBitmap* mOtherPressedBitmap;          // +0x108 the pressed bitmap of the other check state
    UITextLabel* mTextLabel;                // +0x10c (deleted by the dtor)
    bool mIsChecked;                        // +0x110
    bool mHasText;                          // +0x111 SetText: a non-empty text
    bool mIsPushButton;                     // +0x112 provisional name: drawn pressed while checked, not tabable
    bool mScaleBitmap;                      // +0x113 the radio bitmap scales with the box height (sizeof 0x114:
                                            //        implicit copy ctor 0x1005fcb6)
};

#endif
