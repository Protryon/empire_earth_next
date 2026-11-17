// The whole button subclasses' view of a UIButton (R6 ui round 12): one inward call on the Rust button
// (rust/crates/ee_ui/src/buttonobj.rs, checkobj.rs, variantobj.rs) and the GE conversions the constructors use. A
// friend of UIButton. Requires lle/UIButton.h.
#ifndef LLE_UIBUTTONRUST_H
#define LLE_UIBUTTONRUST_H

#include <stddef.h>
#include "rust/ui/buttonobj.h"
#include "rust/ui/rect.h"

struct UIButtonRust {
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
};

#endif
