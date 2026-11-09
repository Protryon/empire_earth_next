// C:\EEX\Low-Level Engine\User Interface\UIButton.h (RECONSTRUCTION). A three-state bitmap button with an optional
// text label. UIButton.cpp (0x1005e47c..0x1005ef4b) holds the ctor and the out-of-line members; the inline members
// are emitted first by UIBlinkingButton.cpp (0x1005db0f..0x1005df23). Vtable 0x10098cac (37 slots: UIControl's
// 34, then ButtonDown, ButtonUp, MouseOver). sizeof 0x104 (padded from the last bool at +0x102).
// Requires lle/UIControl.h, lle/GEBitmap.h.
#ifndef LLE_UIBUTTON_H
#define LLE_UIBUTTON_H

#include "lle/UIControl.h"
#include "lle/UIButtonState.h"

class GEBitmap;
class GEText;

class LLE_API UIButton : public UIControl {
    friend struct UIButtonRust;             // the Rust button logic's host calls (User Interface/UIButtonRust.h)
public:
    UIButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap, GEBitmap* inPressedBitmap,
             const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~UIButton();
    virtual void ChangeFont(GEFont* inFont);
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    unsigned long GetButtonDownSound() const { return this->mButtonDownSound; }
    unsigned long GetButtonUpSound() const { return this->mButtonUpSound; }
    GEBitmap* GetCurrentBitmap() const
    {
        GEBitmap* theBitmap = NULL;
        switch (this->mState) {
        case kButtonStateNormal:
            theBitmap = this->mNormalBitmap;
            break;
        case kButtonStateFocused:
            theBitmap = this->mFocusedBitmap;
            break;
        case kButtonStatePressed:
            theBitmap = this->mPressedBitmap;
            break;
        }
        return theBitmap;
    }
    GEBitmap* GetFocusedBitmap() const { return this->mFocusedBitmap; }
    GEText* GetGEText() { return this->mText; }
    bool GetIsButtonDown() const { return this->mMouseDown || this->mKeyDown; }
    GEBitmap* GetNormalBitmap() const { return this->mNormalBitmap; }
    GEBitmap* GetPressedBitmap() const { return this->mPressedBitmap; }
    void SetBitmapDrawOffset(const GE2DPoint& inOffset) { this->mBitmapDrawOffset = inOffset; }
    void SetButtonDownSound(const UString& inName);
    void SetButtonUpSound(const UString& inName);
    void SetFocusedBitmap(GEBitmap* inBitmap)
    {
        if (this->mFocusedBitmap)
            this->mFocusedBitmap->Dereference();
        this->mFocusedBitmap = inBitmap;
        if (this->mFocusedBitmap)
            this->mFocusedBitmap->Reference();
        this->SetDirty(true);
    }
    void SetFocusedTextColor(const GEColor& inColor) { this->mFocusedTextColor = inColor; }
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetNormalBitmap(GEBitmap* inBitmap)
    {
        if (this->mNormalBitmap)
            this->mNormalBitmap->Dereference();
        this->mNormalBitmap = inBitmap;
        if (this->mNormalBitmap)
            this->mNormalBitmap->Reference();
        this->SetDirty(true);
    }
    void SetPressedBitmap(GEBitmap* inBitmap)
    {
        if (this->mPressedBitmap)
            this->mPressedBitmap->Dereference();
        this->mPressedBitmap = inBitmap;
        if (this->mPressedBitmap)
            this->mPressedBitmap->Reference();
        this->SetDirty(true);
    }
    void SetText(const UWideString& inText, GEFont* inFont);
    virtual long UpdateKeyboard();
protected:
    virtual void ButtonDown();              // slot 0x88
    virtual void ButtonUp();                // slot 0x8c
    virtual long MouseLButtonDown();
    virtual long MouseLButtonUp();
    virtual long MouseMove(const GE2DPoint& inLocation);
    virtual void MouseOver();               // slot 0x90
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedTo();
    virtual void TabbedFrom();

    GERectangle mTextRectangle;             // +0xa0 mRectangle inset by the border thickness
    GE2DPoint mBitmapLocation;              // +0xb0
    GEBitmap* mPressedBitmap;               // +0xb8 (referenced)
    GEBitmap* mNormalBitmap;                // +0xbc
    GEBitmap* mFocusedBitmap;               // +0xc0
    GEBitmap* mCopiedBitmap;                // +0xc4 the current bitmap as of the last CopyData
    unsigned long mState;                   // +0xc8 0 normal, 1 focused, 2 pressed
    GERectangle mCopiedTextRectangle;       // +0xcc
    GE2DPoint mBitmapDrawOffset;            // +0xdc
    GEColor mFocusedTextColor;              // +0xe4
    unsigned long mButtonDownSound;         // +0xf0 sound manager sample id, -1 for none
    unsigned long mButtonUpSound;           // +0xf4
    GEText* mText;                          // +0xf8
    GEText* mCopiedText;                    // +0xfc
    bool mKeyDown;                          // +0x100
    bool mMouseDown;                        // +0x101
    bool mHasCopiedText;                    // +0x102
};

#endif
