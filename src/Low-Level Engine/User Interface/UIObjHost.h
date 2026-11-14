// The toolkit's whole objects (R6 ui round 10, rust/crates/ee_ui/src/obj.rs): the ops on UIControl, the base class
// every whole object's C++ shim keeps (UIControl.cpp). Requires rust/ee_ui.h and lle/UIControl.h.
#ifndef LLE_UIOBJHOST_H
#define LLE_UIOBJHOST_H
#include "rust/ee_ui.h"

struct UIObjHost {
    // UIControl's op (EeUiOp 1..19, 40, 90..94, 114, 115, 119, 140..146) on the control: true and *outResult set when
    // it was one
    static bool Base(UIControl* inControl, EeUiOp inOp, intptr_t a, float* f, intptr_t* outResult);
    // a bitmap's, text's or string's op (EeUiOp 24..26, 28, 76..84, 97, 101, 112, 113, 147, 148) on the object at a, for
    // the control
    static bool Media(UIControl* inControl, EeUiOp inOp, intptr_t a, intptr_t b, float* f, intptr_t* outResult);
    // a part's op (EeUiOp 43..53, 61, 62) on the control at its address
    static bool Part(UIControl* inPart, EeUiOp inOp, float* f, intptr_t* outResult);
    // the list box's ops that do not depend on T (UILBString.cpp; lle/UIListBox.h's UIListBoxHostOp)
    static intptr_t List(UIControl* inList, EeUiOp inOp, intptr_t a, intptr_t b, float* f);
    static GERectangle Rect(const float* f) { return GERectangle(f[1], f[3], f[2], f[0]); }
    static void PutRect(float* f, const GERectangle& r)
    {
        f[0] = r.GetBottom();
        f[1] = r.GetLeft();
        f[2] = r.GetRight();
        f[3] = r.GetTop();
    }
};

#endif
