// WONGUI's draw plans (ee_won's guidraw module, rust/crates/ee_won_gui/src/guidraw.rs): the C++ gathers a paint's
// inputs, Rust returns the ordered draw operations, and WONDrawRun executes them on the Graphics (the Surface,
// PlatGraphics, image and font calls stay C++).
#ifndef EE_WONAPI_WONGUIDRAW_H
#define EE_WONAPI_WONGUIDRAW_H
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

// executes one operation; EE_WON_DRAW_CALL goes to the caller's handler
template <class Call> inline void WONDrawOne(Graphics& g, const EeWonDrawOp& o, const WONDrawSlots& s, Call&& call)
{
    switch (o.kind) {
        case EE_WON_DRAW_SET_COLOR: g.SetColor((DWORD)(unsigned int)o.a); break;
        case EE_WON_DRAW_SET_SYS_COLOR: g.SetSysColor(o.a); break;
        case EE_WON_DRAW_FILL_RECT: g.FillRect(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_RECT: g.DrawRect(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_DOTTED_RECT: g.DrawDottedRect(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_LINE: g.DrawLine(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_IMAGE: g.DrawImage(s.mImages[o.a], o.b, o.c); break;
        case EE_WON_DRAW_IMAGE_SIZE: s.mImages[o.a]->SetSize(o.b, o.c); break;
        case EE_WON_DRAW_PUSH_CLIP: g.PushClipRect(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_POP_CLIP: g.PopClipRect(); break;
        case EE_WON_DRAW_TRANSLATE: g.Translate(o.a, o.b); break;
        case EE_WON_DRAW_SET_FONT: g.SetFont(s.mFont); break;
        case EE_WON_DRAW_STRING: g.DrawString(*s.mString, (unsigned short)o.b, (unsigned short)o.c, o.d, o.e); break;
        case EE_WON_DRAW_STRING_ALL: g.DrawString(*s.mString, o.b, o.c); break;
        case EE_WON_DRAW_TEXT: s.mText->Paint(g, o.b, o.c); break;
        case EE_WON_DRAW_CALL: call(o); break;
        case EE_WON_DRAW_DOTTED_LINE: { // every other pixel in the current colour (TreeItem's dotted lines)
            DWORD aColor = g.mColor;
            int i = ((o.d ? o.b : o.a) & 1) ? 1 : 0;
            for (; i < o.c; i += 2) {
                if (o.d) g.SetPixel(o.a, o.b + i, aColor);
                else g.SetPixel(o.a + i, o.b, aColor);
            }
            break;
        }
        case EE_WON_DRAW_PIXEL: g.SetPixel(o.a, o.b, (DWORD)(unsigned int)o.c); break;
        case EE_WON_DRAW_SYS_PIXEL: g.SetPixel(o.a, o.b, g.GetStandardColor(o.c)); break;
        case EE_WON_DRAW_FILL_ELLIPSE: g.FillEllipse(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_ARC: g.DrawArc(o.a, o.b, o.c, o.d, o.e, o.f); break;
        default: break;
    }
}

struct WONDrawNoCall {
    void operator()(const EeWonDrawOp&) {}
};
// one operation with no slots and no call (the component hosts' Draw op)
inline void WONDrawPlain(Graphics& g, const EeWonDrawOp& o)
{
    WONDrawSlots aSlots;
    WONDrawNoCall aNoCall;
    WONDrawOne(g, o, aSlots, aNoCall);
}

template <class Call> inline void WONDrawRun(Graphics& g, const WONDrawOps& ops, const WONDrawSlots& s, Call call)
{
    for (const EeWonDrawOp* o = ops.begin(); o != ops.end(); ++o)
        WONDrawOne(g, *o, s, call);
}
inline void WONDrawRun(Graphics& g, const WONDrawOps& ops, const WONDrawSlots& s)
{
    WONDrawRun(g, ops, s, WONDrawNoCall());
}

// the Windows skin's drawing (mspaint.rs): one paint's inputs, its plan run on the Graphics; the plan's text call is
// the button's DrawButtonText
inline EeWonMsPaint WONMsPaintOf(EeWonMsPaintKind k, int w, int h, uint32_t flags = 0, int a = 0, int b = 0,
                                 int c = 0, int d = 0, int e = 0)
{
    EeWonMsPaint p;
    p.kind = k; p.flags = flags; p.width = w; p.height = h;
    p.a = a; p.b = b; p.c = c; p.d = d; p.e = e;
    return p;
}
inline void WONMsPaint(Graphics& g, const EeWonMsPaint& p, MSButtonBase* text = NULL)
{
    WONDrawRun(g, WONDrawOps([&](EeWonDrawOp* o, size_t n) { return ee_won_mspaint_plan(&p, o, n); }), WONDrawSlots(),
               [&](const EeWonDrawOp& o) { if (text != NULL && o.a == EE_WON_MSPAINT_CALL_TEXT) text->DrawButtonText(g, o.b, o.c); });
}

// (the font measures a wrapped text's plan asks for are wrapobj.rs's, over platfont.rs: R6 WON GUI round 36)

inline EeWonSelColor WONDrawSelOf(const SelectionColor& s) { return s; } // (the Rust record itself)

}
#endif
