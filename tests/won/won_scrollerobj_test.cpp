// Scroller and ScrollbarScroller as one whole object (R6 WON round 25): the original Scroller.cpp's and
// ScrollbarScroller.cpp's methods, the statements copied from the C++ before the port with every call on a C++
// object (the base Container, the scroll area, the bars' C++ virtuals, the events, the scroller's own virtuals)
// replaced by the host op that makes it (GuiHost.cpp runs the same calls in the same order), against ee_won's
// Rust Scroller (scrollerobj.rs) over one scripted host. Both sides drive real Rust scroll bars (barobj.rs, its
// own golden is won_barobj_test): the original's statements through the bars' C ABI as the C++ Scrollbar's
// forwarders did, the Rust object through the bars' Rust objects; the bars' ops are in the traces.
//
// The host keeps the components (places, sizes, visibility, enabled and disabled state, parents, references), the
// scroll areas' numbers, the focus child, the wheel's answers and the events; as the C++ does, a bar's SetEnabled
// and AddedToParent run the bar's own methods, the area's AdjustPosition calls the scroller's NotifyScrollChange
// back once the area has the scroller, and the scroller's NotifyScrollChange virtual is its own. The bars' scroll
// events are traced as the WindowEventType the C++ fires (ee_won_gui_event_type, GuiHost.cpp's call). Pure reads
// are not traced (their order is the compiler's).
//
// Each of 20,000 sequences makes a scroller, an area, three bars with their parts, and 40 random inward calls
// (the area and the bars set, adding to a parent, conditions, the bars' conditions one by one, notifications,
// scroll events from the bars and other events, offsets, mouse presses with and without a focus child, the
// wheel, the getters, the area's numbers and the bars' visibility changed between calls). The traces must be the
// same, and every reference given back when the scroller and the bars go. The first 40 sequences and every later
// one that makes an op or a call none before made are rust/crates/ee_won/src/golden_scrollerobj.txt, which the
// Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "rust/ee_won.h"
#include "ee/wonapi/WONEventTypes.h"

typedef EeWonGuiOp Op;
typedef EeWonGuiField Fd;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sDriverSeed;
static unsigned DR(unsigned n) { sDriverSeed = sDriverSeed * 69069u + 1u; return n ? (sDriverSeed >> 16) % n : 0; }

struct HostComp {
    int x, y, w, h;
    bool visible, enabled, disabled;
    intptr_t parent;
    int held;
};
static const intptr_t kScroller = 0x54;
static std::map<intptr_t, HostComp> sComps;
static std::map<intptr_t, std::map<int, int> > sAreas; // the scroll areas' fields by EeWonGuiField
static std::map<intptr_t, intptr_t> sAreaScroller;
static std::map<intptr_t, EeWonScrollbar*> sBars;
static std::map<intptr_t, std::pair<int, intptr_t> > sEvents;
static intptr_t sNextId;
static bool sFocusChild;
static int sWheelBase, sWheelArea;
static bool sRustSide;
static std::vector<std::string> sTrace;
static std::set<int> sOpsMet;

static void Line(const char* f, long long a, long long b = 0, long long c = 0, long long d = 0, long long e = 0)
{
    char aLine[300];
    snprintf(aLine, sizeof(aLine), f, a, b, c, d, e);
    sTrace.push_back(aLine);
}

static intptr_t NewComp(int w, int h)
{
    HostComp c;
    c.x = (int)DR(30); c.y = (int)DR(30); c.w = w; c.h = h;
    c.visible = DR(5) != 0; c.enabled = true; c.disabled = false; c.parent = 0; c.held = 0;
    intptr_t id = 0x2000 + (++sNextId);
    sComps[id] = c;
    Line("comp %llx %lld %lld %lld %lld", (long long)id, c.x, c.y, c.w, c.h);
    sTrace.back() += c.visible ? " 1" : " 0";
    return id;
}

static void SideNotify(bool inBecause);

static intptr_t ScriptedHost(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)d;
    intptr_t self = (intptr_t)inComp;
    sOpsMet.insert((int)inOp);
    intptr_t r = 0;
    int n = (int)inOp;
    switch (inOp) {
    // pure reads: not traced
    case Op::Get: return (Fd)a == Fd::Width ? sComps[self].w : (Fd)a == Fd::Height ? sComps[self].h : 0;
    case Op::ChildLeft: return sComps[a].x;
    case Op::ChildTop: return sComps[a].y;
    case Op::ChildWidth: return sComps[a].w;
    case Op::ChildHeight: return sComps[a].h;
    case Op::ChildIsVisible: return sComps[a].visible ? 1 : 0;
    case Op::ChildIsMine: return sComps[a].parent == self ? 1 : 0;
    case Op::IsDisabled: return sComps[self].disabled ? 1 : 0;
    case Op::MouseChild: return 0;
    case Op::EventType: return sEvents[a].first;
    case Op::EventComponent: return sEvents[a].second;
    case Op::GetMousePos: *(int*)a = 0; *(int*)b = 0; return 0;
    case Op::BarOf: return (intptr_t)sBars[a];
    case Op::AreaGet: return sAreas[a][(int)b];
    case Op::HasFocusChild: return sFocusChild ? 1 : 0;
    // what listeners receive: the type GuiHost.cpp fires
    case Op::FireEvent: Line("fire %lld from %llx", ee_won_gui_event_type((EeWonGuiEvent)a), (long long)self); return 0;
    case Op::ChildSetPosSize: {
        const EeWonRect4* q = (const EeWonRect4*)b;
        HostComp& k = sComps[a];
        k.x = q->x; k.y = q->y; k.w = q->w; k.h = q->h;
        Line("%lld pos-size %llx %lld %lld %lld", n, (long long)a, q->x, q->y, q->w);
        sTrace.back() += " " + std::to_string(q->h);
        return 0;
    }
    // the C++ virtuals the ops run: traced first, then what they do
    case Op::ChildSetEnabled:
        Line("%lld %llx %llx %llx", n, (long long)a, (long long)b, (long long)c);
        if (sBars.count(a)) ee_won_bar_call(sBars[a], EeWonBarCall::SetEnabled, b, 0, 0);
        else sComps[a].enabled = b != 0;
        return 0;
    case Op::AddChild:
        Line("%lld %llx %llx %llx", n, (long long)a, (long long)b, (long long)c);
        sComps[a].parent = self;
        if (sBars.count(a)) ee_won_bar_call(sBars[a], EeWonBarCall::AddedToParent, 0, 0, 0);
        return 0;
    case Op::AreaAdjustPosition:
        Line("%lld %llx %llx %llx", n, (long long)a, (long long)b, (long long)c);
        if (sAreaScroller[a] == kScroller) SideNotify(true);
        return 0;
    case Op::VirtNotifyScrollChange:
        Line("%lld %llx %llx %llx", n, (long long)a, (long long)b, (long long)c);
        SideNotify(a != 0);
        return 0;
    case Op::ChildSetPos: sComps[a].x = (int)b; sComps[a].y = (int)c; break;
    case Op::ChildSetSize: sComps[a].w = (int)b; sComps[a].h = (int)c; break;
    case Op::ChildSetVisible: sComps[a].visible = b != 0; break;
    case Op::ChildHold: sComps[a].held++; break;
    case Op::ChildRelease: sComps[a].held--; break;
    case Op::NewButton: {
        HostComp k;
        k.x = k.y = k.w = k.h = 0; k.visible = true; k.enabled = true; k.disabled = false; k.parent = 0; k.held = 0;
        r = 0x2000 + (++sNextId);
        sComps[r] = k;
        break;
    }
    case Op::BaseSetEnabled: sComps[self].disabled = a == 0; break;
    case Op::AreaSet: sAreas[a][(int)b] = (int)c; break;
    case Op::AreaSetScroller: sAreaScroller[a] = self; break;
    case Op::BaseMouseWheel: r = sWheelBase; break;
    case Op::AreaMouseWheel: r = sWheelArea; break;
    default: break;
    }
    Line("%lld %llx %llx %llx -> %llx", n, (long long)a, (long long)b, (long long)c, (long long)r);
    return r;
}

static intptr_t H(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
{
    return ScriptedHost((void*)kScroller, inOp, a, b, c, 0);
}

// ---- the original (Scroller.cpp, ScrollbarScroller.cpp; the base Container, the area, the bars and the events
// through the host, the bars' non-virtual methods through their C ABI as the C++ Scrollbar's forwarders)

namespace orig {
struct Ptr {
    intptr_t p;
    Ptr() : p(0) {}
    ~Ptr() { if (p != 0) H(Op::ChildRelease, p); }
    void operator=(intptr_t q)
    {
        if (p != q) {
            if (p != 0) H(Op::ChildRelease, p);
            p = q;
            if (q != 0) H(Op::ChildHold, q);
        }
    }
    intptr_t get() const { return p; }
};
static const EeWonScrollbar* Bar(intptr_t s) { return (const EeWonScrollbar*)H(Op::BarOf, s); }
static bool IsVertical(intptr_t s) { return ee_won_bar_call(Bar(s), EeWonBarCall::IsVertical, 0, 0, 0) != 0; }
static int AreaField(intptr_t area, Fd f) { return (int)H(Op::AreaGet, area, (intptr_t)f); }
enum { ScrollbarCondition_Enable = 1 };
struct ScrollbarScroller {
    Ptr mScrollArea; // Scroller's
    Ptr mHorzScrollbar, mVertScrollbar;
    int mVertCondition, mHorzCondition;
    bool mSettingScrollbarConditions;
    ScrollbarScroller()
    {
        this->mVertCondition = this->mHorzCondition = ScrollbarCondition_Enable;
        this->mSettingScrollbarConditions = false;
    }
    // Scroller.cpp
    void SetScrollArea(intptr_t area) { this->mScrollArea = area; }
    void SetOffsets(int horizontal, int vertical)
    {
        if (AreaField(this->mScrollArea.get(), Fd::HorzOffset) == horizontal && AreaField(this->mScrollArea.get(), Fd::VertOffset) == vertical) return;
        H(Op::AreaSet, this->mScrollArea.get(), (intptr_t)Fd::HorzOffset, horizontal);
        H(Op::AreaSet, this->mScrollArea.get(), (intptr_t)Fd::VertOffset, vertical);
        H(Op::AreaAdjustPosition, this->mScrollArea.get(), 1);
    }
    void SetVertOffset(int vertical) { this->SetOffsets(AreaField(this->mScrollArea.get(), Fd::HorzOffset), vertical); }
    void SetHorzOffset(int horizontal) { this->SetOffsets(horizontal, AreaField(this->mScrollArea.get(), Fd::VertOffset)); }
    void MouseDown(int x, int y, int button)
    {
        H(Op::BaseMouseDown, x, y, button);
        if (button == 0) {
            if (H(Op::HasFocusChild) == 0) H(Op::AreaRequestFocus, this->mScrollArea.get());
            else H(Op::RequestFocus);
        }
    }
    bool MouseWheel(int amount)
    {
        if (H(Op::BaseMouseWheel, amount)) return true;
        if (this->mScrollArea.get() != 0) return H(Op::AreaMouseWheel, this->mScrollArea.get(), amount) != 0;
        return false;
    }
    // ScrollbarScroller.cpp
    void SetScrollbar(intptr_t scrollbar)
    {
        if (IsVertical(scrollbar)) this->mVertScrollbar = scrollbar;
        else this->mHorzScrollbar = scrollbar;
        H(Op::ListenTo, scrollbar);
    }
    void AddedToParent()
    {
        if (H(Op::ChildIsMine, this->mVertScrollbar.get()) == 0) this->AddControls();
        H(Op::BaseAddedToParent);
    }
    void AddControls()
    {
        H(Op::AreaSetScroller, this->mScrollArea.get());
        H(Op::AddChild, this->mScrollArea.get());
        H(Op::AddChild, this->mVertScrollbar.get());
        H(Op::AddChild, this->mHorzScrollbar.get());
        if (AreaField(this->mScrollArea.get(), Fd::TotalWidth) > 0 || AreaField(this->mScrollArea.get(), Fd::TotalHeight) > 0)
            H(Op::VirtNotifyScrollChange, 0);
    }
    EeWonScrollArea Numbers()
    {
        intptr_t a = this->mScrollArea.get();
        EeWonScrollArea anArea = { AreaField(a, Fd::VertOffset), AreaField(a, Fd::HorzOffset), AreaField(a, Fd::TotalWidth),
                                   AreaField(a, Fd::TotalHeight), AreaField(a, Fd::ExtraHeight), AreaField(a, Fd::ScrollX),
                                   AreaField(a, Fd::ScrollY), AreaField(a, Fd::ScrollWidth), AreaField(a, Fd::ScrollHeight) };
        return anArea;
    }
    bool EnableScrollbar(intptr_t scrollbar, int condition)
    {
        if (scrollbar == 0 || this->mScrollArea.get() == 0) return false;
        bool visible = H(Op::ChildIsVisible, scrollbar) != 0;
        EeWonScrollArea anArea = this->Numbers();
        int k = ee_won_gui_scrollbar_condition(condition, this->mSettingScrollbarConditions, visible, IsVertical(scrollbar), anArea);
        if (k < 0) return false;
        if (k == 0 || k == 1) H(Op::ChildSetVisible, scrollbar, k == 1);
        return visible != (H(Op::ChildIsVisible, scrollbar) != 0);
    }
    void SetScrollbarConditions(int horizontal, int vertical)
    {
        this->mHorzCondition = horizontal;
        this->mVertCondition = vertical;
        bool changed = this->EnableScrollbar(this->mHorzScrollbar.get(), this->mHorzCondition) ? true : false;
        changed = this->EnableScrollbar(this->mVertScrollbar.get(), this->mVertCondition) || changed;
        this->mSettingScrollbarConditions = true;
        if (changed) {
            H(Op::Invalidate);
            H(Op::VirtSizeChanged);
        }
        this->mSettingScrollbarConditions = false;
    }
    void HandleComponentEvent(intptr_t event)
    {
        if (H(Op::EventType, event) == WONAPI::ComponentEvent_Scroll) {
            intptr_t scrollbar = H(Op::EventComponent, event);
            int position = (int)ee_won_bar_call(Bar(scrollbar), EeWonBarCall::GetPosition, 0, 0, 0);
            if (IsVertical(scrollbar)) {
                if (scrollbar == this->mVertScrollbar.get()) { this->SetVertOffset(position); return; }
            } else if (scrollbar == this->mHorzScrollbar.get()) { this->SetHorzOffset(position); return; }
        }
        H(Op::ComponentHandleEvent, event);
    }
    void NotifyScrollChange(bool becauseOfScroller)
    {
        EeWonScrollArea anArea = this->Numbers();
        int aTotal = 0, aPage = 0;
        if (this->mVertScrollbar.get() != 0) {
            const EeWonScrollbar* v = Bar(this->mVertScrollbar.get());
            bool anEnabled = ee_won_gui_scrollbar_sync(anArea, true, &aTotal, &aPage);
            ee_won_bar_call(v, EeWonBarCall::SetRange, aTotal, aPage, 0);
            int up = AreaField(this->mScrollArea.get(), Fd::UpScrollAmount);
            int down = AreaField(this->mScrollArea.get(), Fd::DownScrollAmount);
            ee_won_bar_call(v, EeWonBarCall::SetLineScrollSizes, up, down, 0);
            if (!becauseOfScroller) ee_won_bar_call(v, EeWonBarCall::SetPosition, AreaField(this->mScrollArea.get(), Fd::VertOffset), 0, 0);
            ee_won_bar_call(v, EeWonBarCall::LayoutThumb, !becauseOfScroller, 0, 0);
            H(Op::ChildSetEnabled, this->mVertScrollbar.get(), anEnabled);
            H(Op::ChildInvalidate, this->mVertScrollbar.get());
        }
        if (this->mHorzScrollbar.get() != 0) {
            const EeWonScrollbar* h = Bar(this->mHorzScrollbar.get());
            bool anEnabled = ee_won_gui_scrollbar_sync(anArea, false, &aTotal, &aPage);
            ee_won_bar_call(h, EeWonBarCall::SetRange, aTotal, aPage, 0);
            if (!becauseOfScroller) ee_won_bar_call(h, EeWonBarCall::SetPosition, AreaField(this->mScrollArea.get(), Fd::HorzOffset), 0, 0);
            ee_won_bar_call(h, EeWonBarCall::LayoutThumb, !becauseOfScroller, 0, 0);
            H(Op::ChildSetEnabled, this->mHorzScrollbar.get(), anEnabled);
            H(Op::ChildInvalidate, this->mHorzScrollbar.get());
        }
        this->SetScrollbarConditions(this->mHorzCondition, this->mVertCondition);
    }
};
}

// ---- the driver

enum { kSetArea, kSetScrollbar, kSetBar, kAdded, kAddControls, kConditions, kEnable, kNotify, kEvent, kOffsets,
       kVertOffset, kHorzOffset, kDown, kWheel, kGetArea, kGetBar, kAreaChange, kVisible, kCalls };
static const char* kCallNames[kCalls] = {"set-area", "set-scrollbar", "set-bar", "added", "add-controls",
                                         "conditions", "enable", "notify", "event", "offsets", "vert-offset",
                                         "horz-offset", "down", "wheel", "get-area", "get-bar", "area", "visible"};
static orig::ScrollbarScroller* sOrig;
static EeWonScroller* sRust;
static std::vector<intptr_t> sBarIds, sAreaIds;

static void SideNotify(bool inBecause)
{
    if (sRustSide) ee_won_scroller_call(sRust, EeWonScrollerCall::NotifyScrollChange, inBecause, 0, 0);
    else sOrig->NotifyScrollChange(inBecause);
}

static void NewArea()
{
    intptr_t id = NewComp(20 + (int)DR(300), 20 + (int)DR(300));
    std::map<int, int>& f = sAreas[id];
    f[(int)Fd::VertOffset] = (int)DR(200); f[(int)Fd::HorzOffset] = (int)DR(200);
    f[(int)Fd::TotalWidth] = DR(5) == 0 ? 0 : (int)DR(600); f[(int)Fd::TotalHeight] = DR(5) == 0 ? 0 : (int)DR(900);
    f[(int)Fd::ExtraHeight] = DR(3) == 0 ? (int)DR(40) : 0;
    f[(int)Fd::ScrollX] = (int)DR(6); f[(int)Fd::ScrollY] = (int)DR(6);
    f[(int)Fd::ScrollWidth] = 10 + (int)DR(300); f[(int)Fd::ScrollHeight] = 10 + (int)DR(400);
    f[(int)Fd::UpScrollAmount] = 1 + (int)DR(30); f[(int)Fd::DownScrollAmount] = 1 + (int)DR(30);
    char aLine[200];
    snprintf(aLine, sizeof(aLine), "area %llx %d %d %d %d %d %d %d %d %d %d %d", (long long)id,
             f[(int)Fd::VertOffset], f[(int)Fd::HorzOffset], f[(int)Fd::TotalWidth], f[(int)Fd::TotalHeight],
             f[(int)Fd::ExtraHeight], f[(int)Fd::ScrollX], f[(int)Fd::ScrollY], f[(int)Fd::ScrollWidth],
             f[(int)Fd::ScrollHeight], f[(int)Fd::UpScrollAmount], f[(int)Fd::DownScrollAmount]);
    sTrace.push_back(aLine);
    sAreaIds.push_back(id);
}

static void NewBar(bool inVertical)
{
    int w = 10 + (int)DR(10), h = 40 + (int)DR(300);
    intptr_t id = inVertical ? NewComp(w, h) : NewComp(h, w);
    sBars[id] = ee_won_bar_new((void*)id, ScriptedHost, inVertical);
    Line("bar %llx %lld", (long long)id, inVertical ? 1 : 0);
    sBarIds.push_back(id);
    for (int i = 0; i < 4; i++) {
        intptr_t p = i == 3 ? NewComp(3 + (int)DR(15), 3 + (int)DR(40)) : NewComp(DR(3) == 0 ? 0 : 8 + (int)DR(10), DR(3) == 0 ? 0 : 8 + (int)DR(10));
        Line("bar-part %llx %lld %llx", (long long)id, i, (long long)p);
        ee_won_bar_call(sBars[id], EeWonBarCall::SetPart, i, p, 0);
    }
}

static void Call(bool inRust, int k, long long a, long long b, long long c)
{
    char aLine[120];
    snprintf(aLine, sizeof(aLine), "call %s %llx %llx %llx", kCallNames[k], a, b, c);
    sTrace.push_back(aLine);
    size_t aCallLine = sTrace.size() - 1;
    intptr_t r = 0;
    if (inRust) {
        typedef EeWonScrollerCall S;
        const EeWonScroller* t = sRust;
        switch (k) {
        case kSetArea: ee_won_scroller_call(t, S::SetScrollArea, a, 0, 0); break;
        case kSetScrollbar: ee_won_scroller_call(t, S::SetScrollbar, a, 0, 0); break;
        case kSetBar: ee_won_scroller_call(t, S::SetBar, a, b, 0); break;
        case kAdded: ee_won_scroller_call(t, S::AddedToParent, 0, 0, 0); break;
        case kAddControls: ee_won_scroller_call(t, S::AddControls, 0, 0, 0); break;
        case kConditions: ee_won_scroller_call(t, S::SetScrollbarConditions, a, b, 0); break;
        case kEnable: r = ee_won_scroller_call(t, S::EnableScrollbar, a, b, 0); break;
        case kNotify: ee_won_scroller_call(t, S::NotifyScrollChange, a, 0, 0); break;
        case kEvent: ee_won_scroller_call(t, S::HandleEvent, a, 0, 0); break;
        case kOffsets: ee_won_scroller_call(t, S::SetOffsets, a, b, 0); break;
        case kVertOffset: ee_won_scroller_call(t, S::SetVertOffset, a, 0, 0); break;
        case kHorzOffset: ee_won_scroller_call(t, S::SetHorzOffset, a, 0, 0); break;
        case kDown: ee_won_scroller_call(t, S::MouseDown, a, b, c); break;
        case kWheel: r = ee_won_scroller_call(t, S::MouseWheel, a, 0, 0); break;
        case kGetArea: r = ee_won_scroller_call(t, S::GetScrollArea, 0, 0, 0); break;
        case kGetBar: r = ee_won_scroller_call(t, S::GetScrollbar, a, 0, 0); break;
        default: break;
        }
    } else {
        orig::ScrollbarScroller* t = sOrig;
        switch (k) {
        case kSetArea: t->SetScrollArea((intptr_t)a); break;
        case kSetScrollbar: t->SetScrollbar((intptr_t)a); break;
        case kSetBar: if (a) t->mVertScrollbar = (intptr_t)b; else t->mHorzScrollbar = (intptr_t)b; break;
        case kAdded: t->AddedToParent(); break;
        case kAddControls: t->AddControls(); break;
        case kConditions: t->SetScrollbarConditions((int)a, (int)b); break;
        case kEnable: r = t->EnableScrollbar((intptr_t)a, (int)b); break;
        case kNotify: t->NotifyScrollChange(a != 0); break;
        case kEvent: t->HandleComponentEvent((intptr_t)a); break;
        case kOffsets: t->SetOffsets((int)a, (int)b); break;
        case kVertOffset: t->SetVertOffset((int)a); break;
        case kHorzOffset: t->SetHorzOffset((int)a); break;
        case kDown: t->MouseDown((int)a, (int)b, (int)c); break;
        case kWheel: r = t->MouseWheel((int)a); break;
        case kGetArea: r = t->mScrollArea.get(); break;
        case kGetBar: r = a ? t->mVertScrollbar.get() : t->mHorzScrollbar.get(); break;
        default: break;
        }
    }
    sTrace[aCallLine] += " -> " + std::to_string((long long)r);
}

static intptr_t SomeBar() { return sBarIds[DR((unsigned)sBarIds.size())]; }

static void Run(bool inRust)
{
    static const int kWeights[kCalls] = {1, 2, 1, 2, 1, 4, 3, 4, 6, 3, 2, 2, 2, 2, 1, 1, 3, 3};
    int aTotal = 0;
    for (int i = 0; i < kCalls; i++) aTotal += kWeights[i];
    int r0 = (int)DR(aTotal), k = 0;
    while (r0 >= kWeights[k]) r0 -= kWeights[k++];
    long long a = 0, b = 0, c = 0;
    switch (k) {
    case kSetArea:
        if (DR(3) == 0) NewArea();
        a = sAreaIds[DR((unsigned)sAreaIds.size())];
        break;
    case kSetScrollbar: a = SomeBar(); break;
    case kSetBar: a = DR(2); b = SomeBar(); break;
    case kConditions: a = DR(3); b = DR(3); break;
    case kEnable: a = DR(6) == 0 ? 0 : SomeBar(); b = DR(3); break;
    case kNotify: a = DR(2); break;
    case kEvent: {
        int type = DR(5) != 0 ? 16 : (int)DR(30);
        intptr_t comp = SomeBar();
        int pos = (int)DR(400) - 20;
        a = 0x9000 + (long long)sEvents.size();
        sEvents[(intptr_t)a] = std::make_pair(type, comp);
        ee_won_bar_call(sBars[comp], EeWonBarCall::SetPosition, pos, 0, 0);
        Line("event %llx %lld %llx", a, type, (long long)comp);
        Line("bar-pos %llx %lld", (long long)comp, pos);
        break;
    }
    case kOffsets: a = (int)DR(300) - 20; b = (int)DR(300) - 20; break;
    case kVertOffset: case kHorzOffset: a = (int)DR(300) - 20; break;
    case kDown:
        a = (int)DR(300); b = (int)DR(300); c = DR(3);
        sFocusChild = DR(2) != 0;
        Line("focus %lld", sFocusChild ? 1 : 0);
        break;
    case kWheel:
        a = (int)DR(7) - 3;
        sWheelBase = DR(3) == 0; sWheelArea = DR(2);
        Line("wheel %lld %lld", sWheelBase, sWheelArea);
        break;
    case kGetBar: a = DR(2); break;
    case kAreaChange: {
        // the area's content or view changes between calls
        intptr_t id = sAreaIds[DR((unsigned)sAreaIds.size())];
        int f = (int)DR(11);
        static const Fd kFields[11] = {Fd::VertOffset, Fd::HorzOffset, Fd::TotalWidth, Fd::TotalHeight, Fd::ExtraHeight,
                                      Fd::ScrollX, Fd::ScrollY, Fd::ScrollWidth, Fd::ScrollHeight, Fd::UpScrollAmount,
                                      Fd::DownScrollAmount};
        int v = (int)DR(700);
        sAreas[id][(int)kFields[f]] = v;
        Line("area-set %llx %lld %lld", (long long)id, (int)kFields[f], v);
        return;
    }
    case kVisible: {
        intptr_t id = SomeBar();
        bool v = DR(2) != 0;
        sComps[id].visible = v;
        Line("visible %llx %lld", (long long)id, v ? 1 : 0);
        return;
    }
    default: break;
    }
    Call(inRust, k, a, b, c);
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sComps.clear(); sAreas.clear(); sAreaScroller.clear(); sBars.clear(); sEvents.clear();
    sBarIds.clear(); sAreaIds.clear();
    sNextId = 0;
    sDriverSeed = inSeq * 40503u + 13u;
    sFocusChild = false; sWheelBase = sWheelArea = 0;
    sRustSide = inRust;
    HostComp self;
    self.x = self.y = 0; self.w = 50 + (int)DR(300); self.h = 50 + (int)DR(300);
    self.visible = true; self.enabled = true; self.disabled = false; self.parent = 0; self.held = 0;
    sComps[kScroller] = self;
    Line("start %lld %lld", self.w, self.h);
    if (inRust) sRust = ee_won_scroller_new((void*)kScroller, ScriptedHost);
    else sOrig = new orig::ScrollbarScroller;
    NewArea();
    NewBar(true);
    NewBar(false);
    NewBar(DR(2) != 0);
    // a skin's or MSScrollContainer's set-up: the area, the bars (with their listener or the field only), the
    // conditions
    Call(inRust, kSetArea, sAreaIds[0], 0, 0);
    if (DR(3) == 0) { Call(inRust, kSetBar, 1, sBarIds[0], 0); Call(inRust, kSetBar, 0, sBarIds[1], 0); }
    else { Call(inRust, kSetScrollbar, sBarIds[0], 0, 0); Call(inRust, kSetScrollbar, sBarIds[1], 0, 0); }
    if (DR(2)) Call(inRust, kConditions, DR(3), DR(3), 0);
    for (int i = 0; i < inCalls; i++) Run(inRust);
    sTrace.push_back("free");
    if (inRust) { ee_won_scroller_free(sRust); sRust = NULL; }
    else { delete sOrig; sOrig = NULL; }
    for (size_t i = 0; i < sBarIds.size(); i++) {
        Line("bar-free %llx", (long long)sBarIds[i]);
        ee_won_bar_free(sBars[sBarIds[i]]);
    }
    bool aBack = true;
    for (std::map<intptr_t, HostComp>::const_iterator i = sComps.begin(); i != sComps.end(); ++i)
        if (i->second.held != 0) aBack = false;
    sTrace.push_back(aBack ? "refs back" : "refs LEAKED");
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
        CHECK(a.back() == "refs back");
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
        printf("won_scrollerobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
