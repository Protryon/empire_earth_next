// The toolkit's whole objects (R6 ui round 10, rust/crates/ee_ui/src/obj.rs): the float buffer's rectangles. The
// ops every whole object's host shares (UIControl's, the media's, a part's) are Rust tables over UIControl.cpp's
// one-statement shims (ee_ui lle_hosts.rs, UI lane round 14). Requires nothing.
#ifndef LLE_UIOBJHOST_H
#define LLE_UIOBJHOST_H
#include <string.h>
#include "rust/ui/obj.h"
#include "lle/GE2DPoint.h"
#include "lle/GEColor.h"
#include "lle/GERectangle.h"

// Four floats for a Rust call (ee_ui obj.rs's buffer: a point's x and y, a colour's red, green and blue, a
// rectangle's bottom, left, right and top), alive for the call's full expression
struct UIF4 {
    UIF4(float a = 0.0f, float b = 0.0f, float c = 0.0f, float d = 0.0f) : f{a, b, c, d} {}
    UIF4(const GE2DPoint& p) : f{p.GetX(), p.GetY(), 0.0f, 0.0f} {}
    UIF4(const GEColor& c) : f{c.GetRed(), c.GetGreen(), c.GetBlue(), 0.0f} {}
    UIF4(const GERectangle& r) : f{r.mBottom, r.mLeft, r.mRight, r.mTop} {}
    operator float*() { return this->f; }
    GE2DPoint Point() const { return GE2DPoint(this->f[0], this->f[1]); }
    GEColor Color() const { return GEColor(this->f[0], this->f[1], this->f[2]); }
    // a number's bits as a float (a buffer slot that carries an integer)
    static float Bits(int32_t inValue)
    {
        float theFloat;
        memcpy(&theFloat, &inValue, 4);
        return theFloat;
    }
    float f[4];
};
// the floats a Rust call writes (inCall(args..., f) on a fresh buffer)
template <class F, class... A> UIF4 UIOut(F inCall, A... inArgs)
{
    UIF4 theOut;
    inCall(inArgs..., theOut.f);
    return theOut;
}

struct UIObjHost {
    // the buffer's rectangle (bottom, left, right, top: GERectangle's own order) and the buffer set from one
    static GERectangle Rect(const float* f) { return GERectangle(f[1], f[3], f[2], f[0]); }
    static void PutRect(float* f, const GERectangle& r) { *reinterpret_cast<GERectangle*>(f) = r; }
};

#endif
