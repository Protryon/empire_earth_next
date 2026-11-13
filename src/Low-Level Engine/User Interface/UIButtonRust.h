// The Rust button subclasses' view of a UIButton (rust/crates/ee_ui/src/button_ffi.rs, docs/RUST_PLAN.md R6): the
// object, its whole Rust button (buttonobj.rs) and the subclass's own record. A friend of UIButton,
// UIRadioButton, UIScrollButton and UIDualButton; the subclass members are defined in the subclass's unit. Requires lle/UIButton.h, lle/UITextLabel.h.
#ifndef LLE_UIBUTTONRUST_H
#define LLE_UIBUTTONRUST_H

#include <stddef.h>
#include "rust/ee_ui.h"

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
            this->mRefs.button = theButton;
            this->mRefs.obj = theButton->mRust;
            this->mRefs.sub = inSub;
            this->mRefs.ctx = inContext;
        }
        operator const EeUiButtonRefs*() const { return &this->mRefs; }
    };
    static UIButton* Button(void* inButton) { return static_cast<UIButton*>(inButton); }
    // a whole subclass's method (its record is the Rust button's, buttonobj.rs Sub): one inward call
    static intptr_t Call(const UIButton* inButton, EeUiButtonCall inCall, intptr_t a = 0, intptr_t b = 0,
                         float f0 = 0.0f, float f1 = 0.0f, float f2 = 0.0f, float f3 = 0.0f)
    {
        float f[4] = {f0, f1, f2, f3};
        return ee_ui_button_call(inButton->mRust, inCall, a, b, f);
    }
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
    // the subclasses' records (their units)
    static void* RadioFields(UIRadioButton* inButton);
    static void* ScrollFields(UIScrollButton* inButton);
    static UITextLabel** RadioLabel(UIRadioButton* inButton);
    static void RadioSwap(UIRadioButton* inButton);
    static void RadioUnCheck(UIRadioButton* inButton);
    static TMPoll* ScrollDelay(void* inButton);
    static TMPoll* ScrollRepeat(void* inButton);
    static UISlider* ScrollSlider(void* inButton);
};

#endif
