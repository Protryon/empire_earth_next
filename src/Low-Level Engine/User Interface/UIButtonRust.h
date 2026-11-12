// The Rust button logic's view of a UIButton and its subclasses (rust/crates/ee_ui/src/button.rs, docs/RUST_PLAN.md
// R6): the object, its members from mTextRectangle on (EeUiButtonFields), the UIControl members the logic uses and the
// subclass's own record. A friend of UIButton, UICheckBox, UIRadioButton, UIScrollButton and UIDualButton; the
// subclass members are defined in the subclass's unit. Requires lle/UIButton.h, lle/UITextLabel.h.
#ifndef LLE_UIBUTTONRUST_H
#define LLE_UIBUTTONRUST_H

#include <stddef.h>
#include "rust/ee_ui.h"

class UICheckBox;
class UIRadioButton;
class UIScrollButton;
class UISlider;
class TMPoll;

struct UIButtonRust {
    // A SetText's or SetLocation's label: where the subclass keeps it, the text and the font
    struct Label {
        UITextLabel** mLabel;
        const UWideString* mText;
        GEFont* mFont;
    };
    struct Refs {
        EeUiButtonRefs mRefs;
        Refs(const UIButton* inButton, void* inSub = NULL, void* inContext = NULL)
        {
            UIButton* theButton = const_cast<UIButton*>(inButton);
            UIButtonRust::CheckLayout();
            this->mRefs.button = theButton;
            this->mRefs.fields = reinterpret_cast<EeUiButtonFields*>(&theButton->mTextRectangle);
            this->mRefs.border_type = &theButton->mBorderType;
            this->mRefs.rect = reinterpret_cast<const EeUiRect*>(&theButton->mRectangle);
            this->mRefs.border = &theButton->mBorderThickness;
            this->mRefs.index = &theButton->mIndex;
            this->mRefs.sub = inSub;
            this->mRefs.ctx = inContext;
        }
        operator const EeUiButtonRefs*() const { return &this->mRefs; }
    };
    static UIButton* Button(void* inButton) { return static_cast<UIButton*>(inButton); }
    static EeUiPoint Point(const GE2DPoint& inPoint) { return EeUiPoint{inPoint.GetX(), inPoint.GetY()}; }
    static GE2DPoint Point(const EeUiPoint& inPoint) { return GE2DPoint(inPoint.x, inPoint.y); }
    static EeUiRect Rect(const GERectangle& inRectangle)
    {
        return EeUiRect{inRectangle.mBottom, inRectangle.mLeft, inRectangle.mRight, inRectangle.mTop};
    }
    static GERectangle Rect(const EeUiRect& inRectangle)
    {
        return GERectangle(inRectangle.left, inRectangle.top, inRectangle.right, inRectangle.bottom);
    }
    // the check box's and radio button's bitmap offset in their constructors (float_border: the check box's sum)
    static void BitmapOffset(UIButton* inButton, float inHeight, unsigned long inBorder, bool inScaleBitmap,
                             bool inFloatBorder)
    {
        EeUiPoint theOffset;
        if (ee_ui_button_label_bitmap_offset(inHeight, inBorder, inScaleBitmap, inFloatBorder, &theOffset))
            inButton->SetBitmapDrawOffset(UIButtonRust::Point(theOffset));
    }
    static void CheckLayout()
    {
        // the record's offsets from mTextRectangle (ee_ui/src/button.rs ButtonFields)
#define EE_UI_BUTTON_FIELD(member, field)                                                                             \
    static_assert(offsetof(UIButton, member) - offsetof(UIButton, mTextRectangle) == offsetof(EeUiButtonFields, field), \
                  "ee_ui/src/button.rs")
        EE_UI_BUTTON_FIELD(mTextRectangle, text_rect);
        EE_UI_BUTTON_FIELD(mBitmapLocation, bitmap_location);
        EE_UI_BUTTON_FIELD(mPressedBitmap, pressed_bitmap);
        EE_UI_BUTTON_FIELD(mNormalBitmap, normal_bitmap);
        EE_UI_BUTTON_FIELD(mFocusedBitmap, focused_bitmap);
        EE_UI_BUTTON_FIELD(mCopiedBitmap, copied_bitmap);
        EE_UI_BUTTON_FIELD(mState, state);
        EE_UI_BUTTON_FIELD(mCopiedTextRectangle, copied_text_rect);
        EE_UI_BUTTON_FIELD(mBitmapDrawOffset, bitmap_draw_offset);
        EE_UI_BUTTON_FIELD(mFocusedTextColor, focused_text_color);
        EE_UI_BUTTON_FIELD(mButtonDownSound, button_down_sound);
        EE_UI_BUTTON_FIELD(mButtonUpSound, button_up_sound);
        EE_UI_BUTTON_FIELD(mText, text);
        EE_UI_BUTTON_FIELD(mCopiedText, copied_text);
        EE_UI_BUTTON_FIELD(mKeyDown, key_down);
        EE_UI_BUTTON_FIELD(mMouseDown, mouse_down);
        EE_UI_BUTTON_FIELD(mHasCopiedText, has_copied_text);
#undef EE_UI_BUTTON_FIELD
        static_assert(sizeof(UIButton) - offsetof(UIButton, mTextRectangle) >= sizeof(EeUiButtonFields),
                      "ee_ui/src/button.rs");
        static_assert(sizeof(GERectangle) == sizeof(EeUiRect), "ee_ui/src/rect.rs");
        static_assert(sizeof(GE2DPoint) == sizeof(EeUiPoint), "ee_ui/src/rect.rs");
        static_assert(sizeof(GEColor) == sizeof(EeUiColor), "ee_ui/src/button.rs");
        static_assert(sizeof(((UIButton*)0)->mBorderType) == sizeof(unsigned long), "ee_ui/src/button_ffi.rs");
        static_assert(sizeof(((UIButton*)0)->mIndex) == sizeof(unsigned long), "ee_ui/src/button_ffi.rs");
    }
    // the subclasses' records (their units)
    static void* CheckFields(UICheckBox* inBox);
    static void* RadioFields(UIRadioButton* inButton);
    static void* ScrollFields(UIScrollButton* inButton);
    static UITextLabel** CheckLabel(UICheckBox* inBox);
    static UITextLabel** RadioLabel(UIRadioButton* inButton);
    static void RadioSwap(UIRadioButton* inButton);
    static void RadioUnCheck(UIRadioButton* inButton);
    static TMPoll* ScrollDelay(void* inButton);
    static TMPoll* ScrollRepeat(void* inButton);
    static UISlider* ScrollSlider(void* inButton);
    // the protected UIControl members the host calls use (UIButton.cpp)
    static void CaptureMouse(void* inButton) { Button(inButton)->CaptureMouse(); }
    static void CaptureKeyboard(void* inButton) { Button(inButton)->CaptureKeyboard(); }
    static const GE2DPoint& MouseLocation(void* inButton) { return Button(inButton)->GetMouseLocation(); }
    static bool Key(int inKey)
    {
        switch (inKey) {
        case 0:
            return UIButton::sSpaceKey->JustPressed();
        case 1:
            return UIButton::sEnterKey->JustPressed();
        case 2:
            return UIButton::sSpaceKey->JustUnpressed();
        default:
            return UIButton::sEnterKey->JustUnpressed();
        }
    }
    static void ButtonDown(void* inButton) { Button(inButton)->ButtonDown(); }
    static void ButtonUp(void* inButton) { Button(inButton)->ButtonUp(); }
    static void MouseOver(void* inButton) { Button(inButton)->MouseOver(); }
    static void MouseMove(void* inButton, float inX, float inY) { Button(inButton)->MouseMove(GE2DPoint(inX, inY)); }
    static void SetCopiedBitmap(void* inButton, GEBitmap* inBitmap)
    {
        UIButton* theButton = Button(inButton);
        if (theButton->mCopiedBitmap)
            theButton->mCopiedBitmap->Dereference();
        theButton->mCopiedBitmap = inBitmap;
        if (theButton->mCopiedBitmap)
            theButton->mCopiedBitmap->Reference();
    }
    static GEText* Text(void* inButton) { return Button(inButton)->mText; }
    static void CopyText(void* inButton, bool inFocused);   // UIButton.cpp
};

#endif
