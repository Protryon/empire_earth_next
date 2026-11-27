// WONGUI's draw plans (ee_won's guidraw module, rust/crates/ee_won_gui/src/guidraw.rs): the C++ gathers a paint's
// inputs, Rust returns the ordered draw operations, and gfxrun.rs runs them on the Graphics (R6 WON GUI round 38;
// the image, font and wrapped-text calls are Graphics.cpp's host).
#ifndef EE_WONAPI_WONGUIDRAW_H
#define EE_WONAPI_WONGUIDRAW_H
#include "rust/won_gui/records.h"
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/mspaint.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include <vector>

namespace WONAPI {

// what a plan's slots name: images, the font, the string and the wrapped text its operations draw
struct WONDrawSlots {
    Image* mImages[4];
    Font* mFont;
    const GUIString* mString;
    WrappedText* mText;
    WONDrawSlots() : mFont(NULL), mString(NULL), mText(NULL) { mImages[0] = mImages[1] = mImages[2] = mImages[3] = NULL; }
    WONDrawSlots(Font* f, const GUIString* s, Image* i0 = NULL, WrappedText* w = NULL) : mFont(f), mString(s), mText(w) { mImages[0] = i0; mImages[1] = mImages[2] = mImages[3] = NULL; }
};

// a plan's operations: a buffer for the usual few, asked again at the plan's length when it did not fit (the
// plans are pure)
class WONDrawOps {
public:
    enum { kInline = 48 };
    template <class Plan> explicit WONDrawOps(Plan plan)
    {
        this->mCount = plan(this->mInline, (size_t)kInline);
        this->mOps = this->mInline;
        if (this->mCount > (size_t)kInline) {
            this->mMore.resize(this->mCount);
            this->mCount = plan(&this->mMore[0], this->mMore.size());
            this->mOps = &this->mMore[0];
        }
    }
    const EeWonDrawOp* begin() const { return this->mOps; }
    const EeWonDrawOp* end() const { return this->mOps + this->mCount; }
private:
    EeWonDrawOp mInline[kInline];
    std::vector<EeWonDrawOp> mMore;
    const EeWonDrawOp* mOps;
    size_t mCount;
};

// executes one operation (gfxrun.rs); EE_WON_DRAW_CALL is the caller's (none asks for it here)
struct WONDrawNoCall {
    void operator()(const EeWonDrawOp&) {}
};
template <class Call> inline void WONDrawOne(Graphics& g, const EeWonDrawOp& o, const WONDrawSlots& s, Call&&)
{
    ee_won_graphics_draw(g.Rec(), &o, 1, (const EeWonDrawSlots*)&s);
}
inline void WONDrawRun(Graphics& g, const WONDrawOps& ops, const WONDrawSlots& s)
{
    ee_won_graphics_draw(g.Rec(), ops.begin(), (size_t)(ops.end() - ops.begin()), (const EeWonDrawSlots*)&s);
}

// the Windows skin's drawing (mspaint.rs): one paint's inputs, its plan run on the Graphics (gfxrun.rs)
inline EeWonMsPaint WONMsPaintOf(EeWonMsPaintKind k, int w, int h, uint32_t flags = 0, int a = 0, int b = 0,
                                 int c = 0, int d = 0, int e = 0)
{
    EeWonMsPaint p;
    p.kind = k; p.flags = flags; p.width = w; p.height = h;
    p.a = a; p.b = b; p.c = c; p.d = d; p.e = e;
    return p;
}
inline void WONMsPaint(Graphics& g, const EeWonMsPaint& p) { ee_won_graphics_paint(g.Rec(), &p); }

// (the font measures a wrapped text's plan asks for are wrapobj.rs's, over platfont.rs: R6 WON GUI round 36)

inline EeWonSelColor WONDrawSelOf(const SelectionColor& s) { return s; } // (the Rust record itself)

}
#endif
