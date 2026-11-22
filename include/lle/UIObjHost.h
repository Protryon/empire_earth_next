// The toolkit's whole objects (R6 ui round 10, rust/crates/ee_ui/src/obj.rs): the float buffer's rectangles. The
// ops every whole object's host shares (UIControl's, the media's, a part's) are Rust tables over UIControl.cpp's
// one-statement shims (ee_ui lle_hosts.rs, UI lane round 14). Requires lle/UIControl.h.
#ifndef LLE_UIOBJHOST_H
#define LLE_UIOBJHOST_H
#include "rust/ui/obj.h"

struct UIObjHost {
    // the buffer's rectangle (bottom, left, right, top: GERectangle's own order) and the buffer set from one
    static GERectangle Rect(const float* f) { return GERectangle(f[1], f[3], f[2], f[0]); }
    static void PutRect(float* f, const GERectangle& r) { *reinterpret_cast<GERectangle*>(f) = r; }
};

#endif
