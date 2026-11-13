// The Rust button subclasses' view of a UIButton (rust/crates/ee_ui/src/button_ffi.rs, docs/RUST_PLAN.md R6): the
// object, its whole Rust button (buttonobj.rs) and the subclass's own record. A friend of UIButton,
// UIScrollButton and UIDualButton; the subclass members are defined in the subclass's unit. Requires lle/UIButton.h, lle/UITextLabel.h.
#ifndef LLE_UIBUTTONRUST_H
#define LLE_UIBUTTONRUST_H

#include <stddef.h>
#include "rust/ee_ui.h"

class UIScrollButton;
class UISlider;
class TMPoll;

struct UIButtonRust {
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
    // the subclasses' records (their units)
    static void* ScrollFields(UIScrollButton* inButton);
    static TMPoll* ScrollDelay(void* inButton);
    static TMPoll* ScrollRepeat(void* inButton);
    static UISlider* ScrollSlider(void* inButton);
};

#endif
