// ScrollArea as a whole object (R6 WON round 25): the original ScrollArea.cpp's methods (and SetScroller from
// ScrollAreaViewport.cpp), the statements copied from the C++ before the port with every call on a C++ object
// (the base Container, the viewport, the background, the scroller, the Graphics, the area's own virtuals)
// replaced by the host op that makes it (GuiHost.cpp runs the same calls in the same order), against ee_won's
// Rust ScrollArea (paneobj.rs) over one scripted host. The host keeps the area's size, the viewport's place, the
// scrollers (which of them are ScrollbarScrollers), the wheel's answer and the Graphics' operations; as the C++
// does, the area's AdjustPosition and Virtual08 virtuals run the area's own (a plain ScrollArea), and a scroller's
// notification lays the area out again when the driver says so (MSScrollContainer's Virtual08 sizing the area
// from inside NotifyScrollChange). Pure reads (the size, the viewport, the casts) are not traced.
//
// Each of 20,000 sequences makes an area and 40 random inward calls (sizes, paddings, contents, offsets, the
// wheel, adjustments with and without the scroller's notification, scrollers and bars set, translations, the
// desired and fitted sizes, visible rectangles, paints, every field read and written). The traces must be the
// same. The first 40 sequences and every later one that makes an op or a call none before made are
// rust/crates/ee_won/src/golden_paneobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "rust/ee_won.h"

typedef EeWonGuiOp Op;
typedef EeWonGuiField Fd;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sDriverSeed;
static unsigned DR(unsigned n) { sDriverSeed = sDriverSeed * 69069u + 1u; return n ? (sDriverSeed >> 16) % n : 0; }

static const intptr_t kViewport = 0x3001;
static int sWidth, sHeight;
static std::map<intptr_t, bool> sBarsScroller; // the scrollers that are ScrollbarScrollers
static intptr_t sNextScroller;
static int sWheelBase;
static bool sRelayout, sInRelayout;
static bool sRustSide;
static std::vector<std::string> sTrace;
static std::set<int> sOpsMet;

static void Line(const char* f, long long a, long long b = 0, long long c = 0, long long d = 0, long long e = 0)
{
    char aLine[300];
    snprintf(aLine, sizeof(aLine), f, a, b, c, d, e);
    sTrace.push_back(aLine);
}

static void SideAdjust(bool inScrollbar);
static void SideSizeChanged();

static intptr_t ScriptedHost(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)inComp; (void)d;
    sOpsMet.insert((int)inOp);
    intptr_t r = 0;
    int n = (int)inOp;
    switch (inOp) {
    // pure reads: not traced
    case Op::Get: return (Fd)a == Fd::Width ? sWidth : (Fd)a == Fd::Height ? sHeight : 0;
    case Op::GetViewport: return kViewport;
    case Op::ScrollerAsBars: return a != 0 && sBarsScroller[a] ? a : 0;
    case Op::ChildSetPosSize: {
        const EeWonRect4* q = (const EeWonRect4*)b;
        Line("%lld pos-size %llx %lld %lld %lld", n, (long long)a, q->x, q->y, q->w);
        sTrace.back() += " " + std::to_string(q->h);
        return 0;
    }
    case Op::Draw: {
        const EeWonDrawOp* o = (const EeWonDrawOp*)b;
        Line("draw %llx %lld %lld %lld %lld", (long long)a, o->kind, o->a, o->b, o->c);
        sTrace.back() += " " + std::to_string(o->d);
        return 0;
    }
    // the area's own virtuals (a plain ScrollArea's) and the scroller's notification: traced first, then run
    case Op::VirtAdjustPosition:
        Line("%lld %llx", n, (long long)a);
        SideAdjust(a != 0);
        return 0;
    case Op::VirtSizeChanged:
        Line("%lld", n);
        SideSizeChanged();
        return 0;
    case Op::ScrollerNotify:
        Line("%lld %llx %llx", n, (long long)a, (long long)b);
        if (sRelayout && !sInRelayout) {
            sInRelayout = true;
            SideSizeChanged();
            sInRelayout = false;
        }
        return 0;
    case Op::BaseMouseWheel: r = sWheelBase; break;
    case Op::NewScrollbarScroller: r = 0x5000 + (++sNextScroller); sBarsScroller[r] = true; break;
    case Op::SetSize: sWidth = (int)a; sHeight = (int)b; break;
    default: break;
    }
    Line("%lld %llx %llx %llx -> %llx", n, (long long)a, (long long)b, (long long)c, (long long)r);
    return r;
}

static intptr_t H(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
{
    return ScriptedHost((void*)0x54, inOp, a, b, c, 0);
}

// ---- the original (ScrollArea.cpp; the base Container, the viewport, the background, the scroller and the
// Graphics through the host)

namespace orig {
struct WONRectangle {
    int x, y, width, height;
    bool Intersects(const WONRectangle& o) const
    {
        EeWonRect a = {x, y, width, height}, b = {o.x, o.y, o.width, o.height};
        return ee_won_gui_rect_intersects(a, b);
    }
};
static void Draw(intptr_t g, int kind, int a = 0, int b = 0, int c = 0, int d = 0)
{
    EeWonDrawOp o;
    memset(&o, 0, sizeof(o));
    o.kind = kind; o.a = a; o.b = b; o.c = c; o.d = d;
    H(Op::Draw, g, (intptr_t)&o);
}
struct ScrollArea {
    int mVertOffset, mHorzOffset, mTotalHeight, mTotalWidth;
    int mAVertOffset, mAHorzOffset, mATotalHeight, mATotalWidth;
    int mOldScrollAreaWidth, mOldScrollAreaHeight;
    int mLeftPad, mRightPad, mTopPad, mBottomPad;
    bool mUserHasScrolled;
    intptr_t mScroller;
    WONRectangle mScrollArea;
    int mExtraHeight;
    int mUpScrollAmount, mDownScrollAmount, mLeftScrollAmount, mRightScrollAmount;
    ScrollArea()
    {
        this->mUpScrollAmount = this->mDownScrollAmount = 10;
        this->mLeftScrollAmount = this->mRightScrollAmount = 10;
        this->mVertOffset = this->mHorzOffset = this->mTotalHeight = this->mTotalWidth = 0;
        this->mAVertOffset = this->mAHorzOffset = this->mATotalHeight = this->mATotalWidth = 0;
        this->mExtraHeight = 0;
        this->mOldScrollAreaWidth = this->mOldScrollAreaHeight = 0;
        this->mLeftPad = this->mRightPad = this->mTopPad = this->mBottomPad = 0;
        this->mScroller = 0;
        this->mUserHasScrolled = false;
        this->mScrollArea.x = this->mScrollArea.y = this->mScrollArea.width = this->mScrollArea.height = 0;
    }
    int Width() { return (int)H(Op::Get, (intptr_t)Fd::Width); }
    int Height() { return (int)H(Op::Get, (intptr_t)Fd::Height); }
    EeWonScrollArea AreaOf()
    {
        EeWonScrollArea r = { mVertOffset, mHorzOffset, mTotalWidth, mTotalHeight, mExtraHeight,
                              mScrollArea.x, mScrollArea.y, mScrollArea.width, mScrollArea.height };
        return r;
    }
    void SetDelegatePos()
    {
        EeWonRect4 r = {this->mScrollArea.x - this->mHorzOffset, this->mScrollArea.y - this->mVertOffset,
                        this->mTotalWidth, this->mTotalHeight};
        H(Op::ChildSetPosSize, H(Op::GetViewport), (intptr_t)&r);
    }
    void AdjustPosition(bool scrollbar)
    {
        if (scrollbar) this->mUserHasScrolled = true;
        EeWonScrollArea a = this->AreaOf();
        ee_won_gui_scroll_clamp(&a);
        this->mVertOffset = a.vert_offset;
        this->mHorzOffset = a.horz_offset;
        if (this->mScroller != 0) H(Op::ScrollerNotify, this->mScroller, scrollbar);
        this->mAVertOffset = this->mVertOffset;
        this->mAHorzOffset = this->mHorzOffset;
        this->mATotalHeight = this->mTotalHeight;
        this->mATotalWidth = this->mTotalWidth;
        this->mOldScrollAreaWidth = this->mScrollArea.width;
        this->mOldScrollAreaHeight = this->mScrollArea.height;
        this->SetDelegatePos();
        H(Op::Invalidate);
    }
    void SetVertOffset(int offset) { this->mVertOffset = offset; H(Op::VirtAdjustPosition, 0); }
    void SetOffsets(int horizontal, int vertical)
    {
        this->mHorzOffset = horizontal;
        this->mVertOffset = vertical;
        H(Op::VirtAdjustPosition, 0);
    }
    bool OnBottom() { return ee_won_gui_scroll_on_bottom(this->AreaOf()); }
    void Virtual08()
    {
        H(Op::BaseSizeChanged);
        EeWonScrollArea a = this->AreaOf();
        ee_won_gui_scroll_view(&a, this->Width(), this->Height(), this->mLeftPad, this->mTopPad, this->mRightPad,
                               this->mBottomPad);
        this->mScrollArea.x = a.view_x;
        this->mScrollArea.y = a.view_y;
        this->mScrollArea.width = a.view_width;
        this->mScrollArea.height = a.view_height;
        this->mVertOffset = a.vert_offset;
        H(Op::VirtScrollSizeChanged);
        H(Op::VirtAdjustPosition, 0);
    }
    void SetBorderPadding(int left, int top, int right, int bottom)
    {
        if (this->mLeftPad == left && this->mRightPad == right && this->mTopPad == top && this->mBottomPad == bottom) return;
        this->mLeftPad = left; this->mTopPad = top; this->mRightPad = right; this->mBottomPad = bottom;
        H(Op::VirtSizeChanged);
    }
    void ScrollTranslate(int& x, int& y)
    {
        x += this->mHorzOffset - this->mScrollArea.x;
        y += this->mVertOffset - this->mScrollArea.y;
    }
    void ScrollUntranslate(int& x, int& y)
    {
        x += this->mScrollArea.x - this->mHorzOffset;
        y += this->mScrollArea.y - this->mVertOffset;
    }
    bool MouseWheel(int amount)
    {
        if (H(Op::BaseMouseWheel, amount)) return true;
        this->mUserHasScrolled = true;
        this->mVertOffset -= amount;
        H(Op::VirtAdjustPosition, 0);
        return true;
    }
    void SetSizeEqualToTotal(int minWidth, int minHeight)
    {
        int width = 0, height = 0;
        ee_won_gui_scroll_size(1, this->AreaOf(), minWidth, minHeight, this->mLeftPad, this->mTopPad, this->mRightPad,
                               this->mBottomPad, &width, &height);
        H(Op::SetSize, width, height);
    }
    void GetDesiredSize(int& width, int& height)
    {
        ee_won_gui_scroll_size(0, this->AreaOf(), 0, 0, 0, 0, 0, 0, &width, &height);
    }
    bool IsRectVisible(const WONRectangle& r)
    {
        WONRectangle view = this->mScrollArea;
        view.x = this->mHorzOffset;
        view.y = this->mVertOffset;
        return r.Intersects(view);
    }
    void Paint(intptr_t g)
    {
        H(Op::PaneBackground, g, this->mHorzOffset, this->mVertOffset); // mBackground.Paint(g,0,0,Width(),Height(),h,v)
        Draw(g, EE_WON_DRAW_PUSH_CLIP, this->mScrollArea.x, this->mScrollArea.y, this->mScrollArea.width, this->mScrollArea.height);
        H(Op::ContainerPaint, g);
        int dx = 0, dy = 0;
        this->ScrollTranslate(dx, dy);
        Draw(g, EE_WON_DRAW_TRANSLATE, -dx, -dy);
        H(Op::VirtScrollPaint, g);
        Draw(g, EE_WON_DRAW_TRANSLATE, dx, dy);
        Draw(g, EE_WON_DRAW_POP_CLIP);
    }
    void SetScrollDimension(int theWidth, int theHeight)
    {
        this->mTotalWidth = theWidth;
        this->mTotalHeight = theHeight;
        H(Op::VirtAdjustPosition, 0);
    }
    void SetScroller(intptr_t scroller)
    {
        this->mScroller = scroller;
        if (scroller != 0) H(Op::ScrollerSetArea, scroller);
    }
    void SetScrollbar(intptr_t theScrollbar)
    {
        intptr_t aScroller = H(Op::ScrollerAsBars, this->mScroller); // dynamic_cast<ScrollbarScroller*>
        if (aScroller == 0) {
            aScroller = H(Op::NewScrollbarScroller);
            this->SetScroller(aScroller);
        }
        H(Op::ScrollerSetScrollbar, aScroller, theScrollbar);
    }
    int* FieldOf(int f)
    {
        switch ((Fd)f) {
        case Fd::ScrollWidth: return &mScrollArea.width;
        case Fd::ScrollHeight: return &mScrollArea.height;
        case Fd::ScrollX: return &mScrollArea.x;
        case Fd::ScrollY: return &mScrollArea.y;
        case Fd::VertOffset: return &mVertOffset;
        case Fd::HorzOffset: return &mHorzOffset;
        case Fd::TotalWidth: return &mTotalWidth;
        case Fd::TotalHeight: return &mTotalHeight;
        case Fd::ExtraHeight: return &mExtraHeight;
        case Fd::UpScrollAmount: return &mUpScrollAmount;
        case Fd::DownScrollAmount: return &mDownScrollAmount;
        case Fd::LeftScrollAmount: return &mLeftScrollAmount;
        case Fd::RightScrollAmount: return &mRightScrollAmount;
        case Fd::LeftPad: return &mLeftPad;
        case Fd::TopPad: return &mTopPad;
        case Fd::RightPad: return &mRightPad;
        case Fd::BottomPad: return &mBottomPad;
        default: return NULL;
        }
    }
    int GetField(int f) { if ((Fd)f == Fd::UserHasScrolled) return mUserHasScrolled; int* p = FieldOf(f); return p ? *p : 0; }
    void SetField(int f, int v) { if ((Fd)f == Fd::UserHasScrolled) mUserHasScrolled = v != 0; else if (FieldOf(f)) *FieldOf(f) = v; }
};
}

// ---- the driver

enum { kSize, kPadding, kDimension, kVertOffset, kOffsets, kWheel, kAdjust, kSetScroller, kSetScrollbar,
       kTranslate, kUntranslate, kFit, kDesired, kRectVisible, kPaint, kOnBottom, kGetField, kSetField, kDelegate,
       kCalls };
static const char* kCallNames[kCalls] = {"size", "padding", "dimension", "vert-offset", "offsets", "wheel", "adjust",
                                         "set-scroller", "set-scrollbar", "translate", "untranslate", "fit",
                                         "desired", "rect-visible", "paint", "on-bottom", "get-field", "set-field",
                                         "delegate"};
static orig::ScrollArea* sOrig;
static EeWonScrollPane* sRust;
static const int kFieldCount = 18;
static const Fd kFields[kFieldCount] = {Fd::ScrollWidth, Fd::ScrollHeight, Fd::ScrollX, Fd::ScrollY, Fd::VertOffset,
                                        Fd::HorzOffset, Fd::TotalWidth, Fd::TotalHeight, Fd::ExtraHeight,
                                        Fd::UserHasScrolled, Fd::UpScrollAmount, Fd::DownScrollAmount,
                                        Fd::LeftScrollAmount, Fd::RightScrollAmount, Fd::LeftPad, Fd::TopPad,
                                        Fd::RightPad, Fd::BottomPad};

static void SideAdjust(bool inScrollbar)
{
    if (sRustSide) ee_won_pane_call(sRust, EeWonPaneCall::AdjustPosition, inScrollbar, 0, 0);
    else sOrig->AdjustPosition(inScrollbar);
}
static void SideSizeChanged()
{
    if (sRustSide) ee_won_pane_call(sRust, EeWonPaneCall::SizeChanged, 0, 0, 0);
    else sOrig->Virtual08();
}

static void Call(bool inRust, int k, long long a, long long b, long long c, long long d)
{
    char aLine[160];
    snprintf(aLine, sizeof(aLine), "call %s %llx %llx %llx %llx", kCallNames[k], a, b, c, d);
    sTrace.push_back(aLine);
    size_t aCallLine = sTrace.size() - 1;
    long long r = 0, r2 = 0;
    if (inRust) {
        typedef EeWonPaneCall P;
        const EeWonScrollPane* t = sRust;
        switch (k) {
        case kSize: ee_won_pane_call(t, P::SizeChanged, 0, 0, 0); break;
        case kPadding: { int p[4] = {(int)a, (int)b, (int)c, (int)d}; ee_won_pane_call(t, P::SetBorderPadding, (intptr_t)p, 0, 0); break; }
        case kDimension: ee_won_pane_call(t, P::SetScrollDimension, a, b, 0); break;
        case kVertOffset: ee_won_pane_call(t, P::SetVertOffset, a, 0, 0); break;
        case kOffsets: ee_won_pane_call(t, P::SetOffsets, a, b, 0); break;
        case kWheel: r = ee_won_pane_call(t, P::MouseWheel, a, 0, 0); break;
        case kAdjust: ee_won_pane_call(t, P::AdjustPosition, a, 0, 0); break;
        case kSetScroller: ee_won_pane_call(t, P::SetScroller, a, 0, 0); break;
        case kSetScrollbar: ee_won_pane_call(t, P::SetScrollbar, a, 0, 0); break;
        case kTranslate: case kUntranslate: {
            int x = (int)a, y = (int)b;
            ee_won_pane_call(t, k == kTranslate ? P::ScrollTranslate : P::ScrollUntranslate, (intptr_t)&x, (intptr_t)&y, 0);
            r = x; r2 = y;
            break;
        }
        case kFit: ee_won_pane_call(t, P::SetSizeEqualToTotal, a, b, 0); break;
        case kDesired: { int w = -1, h = -1; ee_won_pane_call(t, P::DesiredSize, (intptr_t)&w, (intptr_t)&h, 0); r = w; r2 = h; break; }
        case kRectVisible: { int q[4] = {(int)a, (int)b, (int)c, (int)d}; r = ee_won_pane_call(t, P::IsRectVisible, (intptr_t)q, 0, 0); break; }
        case kPaint: ee_won_pane_call(t, P::Paint, a, 0, 0); break;
        case kOnBottom: r = ee_won_pane_call(t, P::OnBottom, 0, 0, 0); break;
        case kGetField: r = ee_won_pane_call(t, P::GetField, a, 0, 0); break;
        case kSetField: ee_won_pane_call(t, P::SetField, a, b, 0); break;
        case kDelegate: ee_won_pane_call(t, P::SetDelegatePos, 0, 0, 0); break;
        }
    } else {
        orig::ScrollArea* t = sOrig;
        switch (k) {
        case kSize: t->Virtual08(); break;
        case kPadding: t->SetBorderPadding((int)a, (int)b, (int)c, (int)d); break;
        case kDimension: t->SetScrollDimension((int)a, (int)b); break;
        case kVertOffset: t->SetVertOffset((int)a); break;
        case kOffsets: t->SetOffsets((int)a, (int)b); break;
        case kWheel: r = t->MouseWheel((int)a); break;
        case kAdjust: t->AdjustPosition(a != 0); break;
        case kSetScroller: t->SetScroller((intptr_t)a); break;
        case kSetScrollbar: t->SetScrollbar((intptr_t)a); break;
        case kTranslate: case kUntranslate: {
            int x = (int)a, y = (int)b;
            if (k == kTranslate) t->ScrollTranslate(x, y); else t->ScrollUntranslate(x, y);
            r = x; r2 = y;
            break;
        }
        case kFit: t->SetSizeEqualToTotal((int)a, (int)b); break;
        case kDesired: { int w = -1, h = -1; t->GetDesiredSize(w, h); r = w; r2 = h; break; }
        case kRectVisible: { orig::WONRectangle q = {(int)a, (int)b, (int)c, (int)d}; r = t->IsRectVisible(q); break; }
        case kPaint: t->Paint((intptr_t)a); break;
        case kOnBottom: r = t->OnBottom(); break;
        case kGetField: r = t->GetField((int)a); break;
        case kSetField: t->SetField((int)a, (int)b); break;
        case kDelegate: t->SetDelegatePos(); break;
        }
    }
    sTrace[aCallLine] += " -> " + std::to_string(r) + " " + std::to_string(r2);
}

static void Run(bool inRust)
{
    static const int kWeights[kCalls] = {4, 2, 4, 3, 2, 2, 3, 1, 1, 1, 1, 1, 1, 1, 2, 1, 3, 3, 1};
    int aTotal = 0;
    for (int i = 0; i < kCalls; i++) aTotal += kWeights[i];
    int r0 = (int)DR(aTotal), k = 0;
    while (r0 >= kWeights[k]) r0 -= kWeights[k++];
    long long a = 0, b = 0, c = 0, d = 0;
    sRelayout = DR(4) == 0;
    Line("relayout %lld", sRelayout ? 1 : 0);
    switch (k) {
    case kSize:
        sWidth = (int)DR(400); sHeight = (int)DR(400);
        Line("size %lld %lld", sWidth, sHeight);
        break;
    case kPadding: a = DR(8); b = DR(8); c = DR(8); d = DR(8); break;
    case kDimension: a = (int)DR(800) - 5; b = (int)DR(1500) - 5; break;
    case kVertOffset: a = (int)DR(1200) - 100; break;
    case kOffsets: a = (int)DR(900) - 100; b = (int)DR(1200) - 100; break;
    case kWheel:
        a = (int)DR(121) - 60;
        sWheelBase = DR(4) == 0;
        Line("wheel %lld", sWheelBase);
        break;
    case kAdjust: a = DR(2); break;
    case kSetScroller: {
        a = DR(4) == 0 ? 0 : 0x5000 + (long long)DR(4);
        if (a) { sBarsScroller[(intptr_t)a] = DR(2) != 0; Line("scroller %llx %lld", a, sBarsScroller[(intptr_t)a] ? 1 : 0); }
        break;
    }
    case kSetScrollbar: a = 0x6000 + DR(5); break;
    case kTranslate: case kUntranslate: a = (int)DR(400) - 50; b = (int)DR(400) - 50; break;
    case kFit: a = (int)DR(500) - 20; b = (int)DR(500) - 20; break;
    case kRectVisible: a = (int)DR(900) - 50; b = (int)DR(1500) - 50; c = (int)DR(200) - 5; d = (int)DR(200) - 5; break;
    case kPaint: a = 0x7000 + DR(3); break;
    case kGetField: a = (int)kFields[DR(kFieldCount)]; break;
    case kSetField: a = (int)kFields[DR(kFieldCount)]; b = (int)DR(600) - 50; break;
    default: break;
    }
    Call(inRust, k, a, b, c, d);
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sBarsScroller.clear();
    sNextScroller = 100;
    sDriverSeed = inSeq * 40503u + 13u;
    sWheelBase = 0;
    sRelayout = sInRelayout = false;
    sRustSide = inRust;
    sWidth = (int)DR(400); sHeight = (int)DR(400);
    Line("start %lld %lld", sWidth, sHeight);
    if (inRust) sRust = ee_won_pane_new((void*)0x54, ScriptedHost);
    else sOrig = new orig::ScrollArea;
    for (int i = 0; i < inCalls; i++) Run(inRust);
    sTrace.push_back("free");
    if (inRust) { ee_won_pane_free(sRust); sRust = NULL; }
    else { delete sOrig; sOrig = NULL; }
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kSequences = 20000;
    const int kCallsPer = aPrint ? 16 : 40;
    size_t aLines = 0;
    std::set<std::string> aSeen;
    int aDiffering = 0;
    for (int seq = 0; seq < kSequences; seq++) {
        sOpsMet.clear();
        std::vector<std::string> a = RunSide(false, (unsigned)seq, kCallsPer);
        std::vector<std::string> b = RunSide(true, (unsigned)seq, kCallsPer);
        aLines += a.size();
        bool same = a == b;
        CHECK(same);
        if (!same) {
            aDiffering++;
            if (aDiffering <= 3) {
                printf("sequence %d differs\n", seq);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    const char* x = i < a.size() ? a[i].c_str() : "<end>";
                    const char* y = i < b.size() ? b[i].c_str() : "<end>";
                    if (strcmp(x, y) != 0) { printf("  line %u\n  orig: %s\n  rust: %s\n", (unsigned)i, x, y); break; }
                }
            }
        }
        bool aNew = seq < 40;
        for (std::set<int>::const_iterator i = sOpsMet.begin(); i != sOpsMet.end(); ++i)
            if (aSeen.insert("op" + std::to_string(*i)).second) aNew = true;
        for (size_t i = 0; i < a.size(); i++)
            if (a[i].compare(0, 5, "call ") == 0)
                if (aSeen.insert(a[i].substr(0, a[i].find(' ', 5))).second) aNew = true;
        if (aPrint && aNew) {
            printf("seq %d\n", seq);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (!aPrint)
        printf("won_paneobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
