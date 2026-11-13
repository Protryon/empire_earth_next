// Scrollbar as a whole object (R6 WON round 25): the original Scrollbar.cpp's methods, the statements copied from
// the C++ before the port with every call on a C++ object (the base Container, the arrows, the track, the thumb,
// the events) replaced by the host op that makes it (GuiHost.cpp runs the same calls in the same order), against
// ee_won's Rust Scrollbar (barobj.rs) over one scripted host. The host keeps the parts (positions, sizes,
// visibility, enabled state, button flags, parent, references), the bar's size, disabled state, mouse child and
// mouse point, and the events.
//
// What listeners receive: the scroll event is traced as the WindowEventType the C++ fires. The original's side
// fires ComponentEvent_Scroll from the original enum (ee/wonapi/WONEventTypes.h, the header WONLobbyGUI.h takes it
// from); the Rust side's FireEvent op is turned into a type by ee_won_gui_event_type, the very call GuiHost.cpp
// makes. (Round 23's golden traced the op only and missed that GuiHost delivered every list event as
// InputTextChanged.)
//
// The host's pure reads (sizes, positions, the parent test, the disabled state, the mouse child, the event's type
// and component, the mouse point) are not traced: their order is the compiler's (argument evaluation), not the
// program's. Each of 20,000 sequences creates the bar, gives it its parts, adds it to a parent and makes 40 random
// inward calls (sizes, mouse presses and drags on the thumb and elsewhere, button events from the arrows, the
// track and others, enabling, copied attributes, ranges, steps, positions, thumb layouts, new parts, the
// orientation and thumb options, the getters). The traces must be the same, and every reference the bar takes
// must be given back when it goes. The first 40 sequences and every later one that makes an op or a call none
// before made are rust/crates/ee_won/src/golden_barobj.txt, which the Rust tests replay; `--print` prints them.
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

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

struct HostComp {
    int x, y, w, h;
    bool visible, enabled, mine;
    int flags, held;
};
struct HostEvent {
    int type;
    intptr_t comp;
};
static std::map<intptr_t, HostComp> sComps;
static std::map<intptr_t, HostEvent> sEvents;
static intptr_t sNextId;
static int sWidth, sHeight;
static bool sDisabled;
static intptr_t sMouseChild;
static int sMouseX, sMouseY;
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
    c.x = (int)R(30); c.y = (int)R(30); c.w = w; c.h = h;
    c.visible = R(5) != 0; c.enabled = true; c.mine = false; c.flags = 0; c.held = 0;
    intptr_t id = 0x2000 + (++sNextId);
    sComps[id] = c;
    Line("comp %llx %lld %lld %lld %lld", (long long)id, c.x, c.y, c.w, c.h);
    sTrace.back() += c.visible ? " 1" : " 0";
    return id;
}

static intptr_t ScriptedHost(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)inComp; (void)d;
    sOpsMet.insert((int)inOp);
    intptr_t r = 0;
    int n = (int)inOp;
    switch (inOp) {
    // pure reads: not traced
    case Op::Get: return (Fd)a == Fd::Width ? sWidth : (Fd)a == Fd::Height ? sHeight : 0;
    case Op::ChildLeft: return sComps[a].x;
    case Op::ChildTop: return sComps[a].y;
    case Op::ChildWidth: return sComps[a].w;
    case Op::ChildHeight: return sComps[a].h;
    case Op::ChildIsMine: return sComps[a].mine ? 1 : 0;
    case Op::IsDisabled: return sDisabled ? 1 : 0;
    case Op::MouseChild: return sMouseChild;
    case Op::EventType: return sEvents[a].type;
    case Op::EventComponent: return sEvents[a].comp;
    case Op::GetMousePos: *(int*)a = sMouseX; *(int*)b = sMouseY; return 0;
    // what listeners receive: the type GuiHost.cpp fires
    case Op::FireEvent: Line("fire %lld", ee_won_gui_event_type((EeWonGuiEvent)a)); return 0;
    case Op::ChildSetPosSize: {
        const EeWonRect4* q = (const EeWonRect4*)b;
        HostComp& k = sComps[a];
        k.x = q->x; k.y = q->y; k.w = q->w; k.h = q->h;
        Line("%lld pos-size %llx %lld %lld %lld", n, (long long)a, q->x, q->y, q->w);
        sTrace.back() += " " + std::to_string(q->h);
        return 0;
    }
    case Op::ChildSetPos: sComps[a].x = (int)b; sComps[a].y = (int)c; break;
    case Op::ChildSetSize: sComps[a].w = (int)b; sComps[a].h = (int)c; break;
    case Op::ChildSetVisible: sComps[a].visible = b != 0; break;
    case Op::ChildSetEnabled: sComps[a].enabled = b != 0; break;
    case Op::ChildHold: sComps[a].held++; break;
    case Op::ChildRelease: sComps[a].held--; break;
    case Op::AddChild: sComps[a].mine = true; break;
    case Op::NewButton: {
        HostComp k;
        k.x = k.y = k.w = k.h = 0; k.visible = true; k.enabled = true; k.mine = false; k.flags = 0; k.held = 0;
        r = 0x2000 + (++sNextId);
        sComps[r] = k;
        break;
    }
    case Op::ButtonSetFlags: if (c) sComps[a].flags |= (int)b; else sComps[a].flags &= ~(int)b; break;
    case Op::BaseSetEnabled: sDisabled = a == 0; break;
    default: break;
    }
    Line("%lld %llx %llx %llx -> %llx", n, (long long)a, (long long)b, (long long)c, (long long)r);
    return r;
}

static intptr_t H(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
{
    return ScriptedHost((void*)0x54, inOp, a, b, c, 0);
}

// ---- the original (Scrollbar.cpp; the base Container, the parts and the events through the host)

namespace orig {
// the original's SmartPtr: the old object released, then the new one referenced
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
struct Comp {
    intptr_t id;
    int Left() { return (int)H(Op::ChildLeft, id); }
    int Top() { return (int)H(Op::ChildTop, id); }
    int Width() { return (int)H(Op::ChildWidth, id); }
    int Height() { return (int)H(Op::ChildHeight, id); }
};
static Comp C(intptr_t id) { Comp c; c.id = id; return c; }
struct Scrollbar {
    Ptr mUpArrow, mDownArrow, mScrollRect;
    Ptr mThumb;
    bool mResizeThumb, mVertical;
    unsigned char mMinThumbSize;
    int mPosition, mPageSize, mTotalSize;
    int mLineUpScrollSize, mLineDownScrollSize, mPageScrollSize;
    int mDragX, mDragY;
    Scrollbar(bool vertical)
    {
        this->mPosition = 0;
        this->mPageSize = 0;
        this->mTotalSize = 0;
        this->mLineUpScrollSize = this->mLineDownScrollSize = 10;
        this->mPageScrollSize = 50;
        this->mResizeThumb = true;
        this->mVertical = vertical;
        this->mMinThumbSize = 10;
        this->mDragX = this->mDragY = 0;
    }
    int Width() { return (int)H(Op::Get, (intptr_t)Fd::Width); }
    int Height() { return (int)H(Op::Get, (intptr_t)Fd::Height); }
    bool IsDisabled() { return H(Op::IsDisabled) != 0; }
    int S_Left(intptr_t c) { return this->mVertical?C(c).Left():C(c).Top(); }
    int S_Top(intptr_t c) { return this->mVertical?C(c).Top():C(c).Left(); }
    int S_Bottom(intptr_t c) { return this->mVertical?C(c).Top()+C(c).Height():C(c).Left()+C(c).Width(); }
    int S_Height(intptr_t c) { return this->mVertical?C(c).Height():C(c).Width(); }
    int S_Width(intptr_t c) { return this->mVertical?C(c).Width():C(c).Height(); }
    int S_Height(Scrollbar*) { return this->mVertical?this->Height():this->Width(); }
    int S_Width(Scrollbar*) { return this->mVertical?this->Width():this->Height(); }
    void S_SetPos(intptr_t c, int x, int y)
    {
        if (this->mVertical) H(Op::ChildSetPos, c, x, y);
        else H(Op::ChildSetPos, c, y, x);
    }
    void S_SetSize(intptr_t c, int width, int height)
    {
        if (this->mVertical) H(Op::ChildSetSize, c, width, height);
        else H(Op::ChildSetSize, c, height, width);
    }
    void S_SetPosSize(intptr_t c, int x, int y, int width, int height)
    {
        EeWonRect4 r;
        if (this->mVertical) { r.x = x; r.y = y; r.w = width; r.h = height; }
        else { r.x = y; r.y = x; r.w = height; r.h = width; }
        H(Op::ChildSetPosSize, c, (intptr_t)&r);
    }
    void CopyAttributes(Scrollbar* theCopyFrom)
    {
        this->mLineUpScrollSize = theCopyFrom->mLineUpScrollSize;
        this->mLineDownScrollSize = theCopyFrom->mLineDownScrollSize;
        this->mResizeThumb = theCopyFrom->mResizeThumb;
        this->mMinThumbSize = theCopyFrom->mMinThumbSize;
    }
    void AddedToParent()
    {
        if (H(Op::ChildIsMine, this->mUpArrow.get()) == 0)
            this->AddControls();
        H(Op::BaseAddedToParent);
    }
    void AddControls()
    {
        if (this->mScrollRect.get() == 0)
            this->mScrollRect = H(Op::NewButton);
        H(Op::ButtonSetFlags, this->mUpArrow.get(), 2, 1);
        H(Op::ButtonSetFlags, this->mDownArrow.get(), 2, 1);
        H(Op::ButtonSetFlags, this->mScrollRect.get(), 2, 1);
        H(Op::AddChild, this->mScrollRect.get());
        H(Op::AddChild, this->mThumb.get());
        H(Op::AddChild, this->mUpArrow.get());
        H(Op::AddChild, this->mDownArrow.get());
    }
    void LayoutThumb(bool changePos)
    {
        EeWonThumbIn anIn;
        anIn.page = this->mPageSize;
        anIn.total = this->mTotalSize;
        anIn.position = this->mPosition;
        anIn.track_top = this->S_Top(this->mScrollRect.get());
        anIn.track_len = this->S_Height(this->mScrollRect.get());
        anIn.thumb_top = this->S_Top(this->mThumb.get());
        anIn.thumb_len = this->S_Height(this->mThumb.get());
        anIn.resize_thumb = this->mResizeThumb;
        anIn.min_thumb = this->mMinThumbSize;
        anIn.change_pos = changePos;
        EeWonThumbOut anOut = ee_won_gui_thumb_layout(anIn);
        if (anOut.hidden) {
            H(Op::ChildSetVisible, this->mThumb.get(), 0);
            return;
        }
        H(Op::ChildSetVisible, this->mThumb.get(), !this->IsDisabled());
        int aThumbWidth = this->S_Width(this->mThumb.get());
        if (aThumbWidth == 0)
            aThumbWidth = this->S_Width(this);
        if (anOut.set_pos)
            this->S_SetPosSize(this->mThumb.get(), this->S_Left(this->mThumb.get()), anOut.pos, aThumbWidth, anOut.len);
        else
            this->S_SetSize(this->mThumb.get(), aThumbWidth, anOut.len);
    }
    void Virtual08()
    {
        H(Op::BaseSizeChanged);
        if (this->S_Height(this->mUpArrow.get()) == 0)
            this->S_SetSize(this->mUpArrow.get(), this->S_Width(this), this->S_Width(this));
        if (this->S_Height(this->mDownArrow.get()) == 0)
            this->S_SetSize(this->mDownArrow.get(), this->S_Width(this), this->S_Width(this));
        this->S_SetPos(this->mDownArrow.get(), 0, this->S_Height(this) - this->S_Height(this->mDownArrow.get()));
        this->S_SetPosSize(this->mScrollRect.get(), 0, this->S_Bottom(this->mUpArrow.get()), this->S_Width(this),
                           this->S_Height(this) - this->S_Height(this->mUpArrow.get()) - this->S_Height(this->mDownArrow.get()));
        this->LayoutThumb(false);
    }
    void MouseDown(int x, int y, int theButton)
    {
        H(Op::BaseMouseDown, x, y, theButton);
        if (theButton == 0 && H(Op::MouseChild) == this->mThumb.get()) {
            this->mDragX = x - C(this->mThumb.get()).Left();
            this->mDragY = y - C(this->mThumb.get()).Top();
        }
    }
    void MouseDrag(int x, int y)
    {
        H(Op::BaseMouseDrag, x, y);
        if (H(Op::MouseChild) != this->mThumb.get())
            return;
        int dx = (x - C(this->mThumb.get()).Left()) - this->mDragX;
        int dy = (y - C(this->mThumb.get()).Top()) - this->mDragY;
        int aNewY = this->mVertical ? (C(this->mThumb.get()).Top() + dy) : (C(this->mThumb.get()).Left() + dx);
        int oldPosition = this->mPosition;
        int aTop = 0;
        this->mPosition = ee_won_gui_thumb_drag(aNewY, this->S_Top(this->mScrollRect.get()),
            this->S_Height(this->mScrollRect.get()), this->S_Height(this->mThumb.get()), this->mTotalSize, this->mPageSize, &aTop);
        aNewY = aTop;
        if (aNewY != this->S_Top(this->mThumb.get())) {
            this->S_SetPos(this->mThumb.get(), this->S_Left(this->mThumb.get()), aNewY);
            H(Op::ChildInvalidate, this->mScrollRect.get());
        }
        if (this->mPosition != oldPosition)
            Line("fire %lld", WONAPI::ComponentEvent_Scroll); // Fn2F5380(ComponentEvent_Scroll, 0)
    }
    void HandleComponentEvent(intptr_t theEvent)
    {
        if (H(Op::EventType, theEvent) != WONAPI::ComponentEvent_ButtonPressed) {
            H(Op::BaseHandleEvent, theEvent);
            return;
        }
        int anOldPosition = this->mPosition;
        EeWonScrollPress aPress;
        int aMouseY = 0;
        intptr_t aComponent = H(Op::EventComponent, theEvent);
        if (aComponent == this->mUpArrow.get())
            aPress = EeWonScrollPress::LineUp;
        else if (aComponent == this->mDownArrow.get())
            aPress = EeWonScrollPress::LineDown;
        else if (aComponent == this->mScrollRect.get()) {
            int aMouseX;
            H(Op::GetMousePos, (intptr_t)&aMouseX, (intptr_t)&aMouseY);
            if (!this->mVertical)
                aMouseY = aMouseX;
            aPress = EeWonScrollPress::Track;
        } else {
            H(Op::BaseHandleEvent, theEvent);
            return;
        }
        this->mPosition = ee_won_gui_scroll_step(aPress, aMouseY, this->S_Top(this->mThumb.get()),
            this->S_Height(this->mThumb.get()), this->mPosition, this->mLineUpScrollSize, this->mLineDownScrollSize,
            this->mPageScrollSize, this->mTotalSize, this->mPageSize);
        if (this->mPosition == anOldPosition)
            return;
        this->LayoutThumb(true);
        H(Op::ChildInvalidate, this->mScrollRect.get());
        Line("fire %lld", WONAPI::ComponentEvent_Scroll);
    }
    void SetEnabled(bool isEnabled)
    {
        if (isEnabled == !this->IsDisabled())
            return;
        H(Op::BaseSetEnabled, isEnabled);
        H(Op::ChildSetEnabled, this->mUpArrow.get(), isEnabled);
        H(Op::ChildSetEnabled, this->mDownArrow.get(), isEnabled);
        if (this->mTotalSize > this->mPageSize)
            H(Op::ChildSetVisible, this->mThumb.get(), isEnabled);
    }
    Ptr& Part(int i) { return i == 0 ? mUpArrow : i == 1 ? mDownArrow : i == 2 ? mScrollRect : mThumb; }
};
}

// ---- the driver

enum { kSize, kDown, kDrag, kEvent, kEnable, kCopy, kRange, kLines, kPosition, kLayout, kPart, kVertical, kResize,
       kMin, kAdded, kAddControls, kGetPart, kIsVertical, kGetPosition, kCalls };
static const char* kCallNames[kCalls] = {"size", "down", "drag", "event", "enable", "copy", "range", "lines",
                                         "position", "layout", "part", "vertical", "resize", "min", "added",
                                         "add-controls", "get-part", "is-vertical", "get-position"};
static unsigned sDriverSeed;
static unsigned DR(unsigned n) { sDriverSeed = sDriverSeed * 69069u + 1u; return n ? (sDriverSeed >> 16) % n : 0; }
static orig::Scrollbar* sOrig;
static EeWonScrollbar* sRust;
static intptr_t sParts[4]; // the driver's view, for mouse children and events

static void Call(bool inRust, int k, long long a, long long b, long long c)
{
    char aLine[120];
    snprintf(aLine, sizeof(aLine), "call %s %llx %llx %llx", kCallNames[k], a, b, c);
    sTrace.push_back(aLine);
    size_t aCallLine = sTrace.size() - 1;
    intptr_t r = 0;
    if (inRust) {
        typedef EeWonBarCall B;
        const EeWonScrollbar* t = sRust;
        switch (k) {
        case kSize: ee_won_bar_call(t, B::SizeChanged, 0, 0, 0); break;
        case kDown: ee_won_bar_call(t, B::MouseDown, a, b, c); break;
        case kDrag: ee_won_bar_call(t, B::MouseDrag, a, b, 0); break;
        case kEvent: ee_won_bar_call(t, B::HandleEvent, a, 0, 0); break;
        case kEnable: ee_won_bar_call(t, B::SetEnabled, a, 0, 0); break;
        case kCopy: {
            EeWonScrollbar* s = ee_won_bar_new((void*)0x55, ScriptedHost, true);
            ee_won_bar_call(s, B::SetLineScrollSizes, a, b, 0);
            ee_won_bar_call(s, B::SetResizeThumb, c & 1, 0, 0);
            ee_won_bar_call(s, B::SetMinThumbSize, c >> 1, 0, 0);
            ee_won_bar_call(t, B::CopyAttributes, (intptr_t)s, 0, 0);
            ee_won_bar_free(s);
            break;
        }
        case kRange: ee_won_bar_call(t, B::SetRange, a, b, 0); break;
        case kLines: ee_won_bar_call(t, B::SetLineScrollSizes, a, b, 0); break;
        case kPosition: ee_won_bar_call(t, B::SetPosition, a, 0, 0); break;
        case kLayout: ee_won_bar_call(t, B::LayoutThumb, a, 0, 0); break;
        case kPart: ee_won_bar_call(t, B::SetPart, a, b, 0); break;
        case kVertical: ee_won_bar_call(t, B::SetVertical, a, 0, 0); break;
        case kResize: ee_won_bar_call(t, B::SetResizeThumb, a, 0, 0); break;
        case kMin: ee_won_bar_call(t, B::SetMinThumbSize, a, 0, 0); break;
        case kAdded: ee_won_bar_call(t, B::AddedToParent, 0, 0, 0); break;
        case kAddControls: ee_won_bar_call(t, B::AddControls, 0, 0, 0); break;
        case kGetPart: r = ee_won_bar_call(t, B::GetPart, a, 0, 0); break;
        case kIsVertical: r = ee_won_bar_call(t, B::IsVertical, 0, 0, 0); break;
        case kGetPosition: r = ee_won_bar_call(t, B::GetPosition, 0, 0, 0); break;
        }
    } else {
        orig::Scrollbar* t = sOrig;
        switch (k) {
        case kSize: t->Virtual08(); break;
        case kDown: t->MouseDown((int)a, (int)b, (int)c); break;
        case kDrag: t->MouseDrag((int)a, (int)b); break;
        case kEvent: t->HandleComponentEvent((intptr_t)a); break;
        case kEnable: t->SetEnabled(a != 0); break;
        case kCopy: {
            orig::Scrollbar s(true);
            s.mLineUpScrollSize = (int)a; s.mLineDownScrollSize = (int)b;
            s.mResizeThumb = (c & 1) != 0; s.mMinThumbSize = (unsigned char)(c >> 1);
            t->CopyAttributes(&s);
            break;
        }
        case kRange: t->mTotalSize = (int)a; t->mPageSize = (int)b; t->mPageScrollSize = (int)b; break;
        case kLines: t->mLineUpScrollSize = (int)a; t->mLineDownScrollSize = (int)b; break;
        case kPosition: t->mPosition = (int)a; break;
        case kLayout: t->LayoutThumb(a != 0); break;
        case kPart: t->Part((int)a) = (intptr_t)b; break;
        case kVertical: t->mVertical = a != 0; break;
        case kResize: t->mResizeThumb = a != 0; break;
        case kMin: t->mMinThumbSize = (unsigned char)a; break;
        case kAdded: t->AddedToParent(); break;
        case kAddControls: t->AddControls(); break;
        case kGetPart: r = t->Part((int)a).get(); break;
        case kIsVertical: r = t->mVertical; break;
        case kGetPosition: r = t->mPosition; break;
        }
    }
    sTrace[aCallLine] += " -> " + std::to_string((long long)r);
    if (k == kPart) sParts[a] = (intptr_t)b;
    if (k == kAdded || k == kAddControls) sParts[2] = inRust ? ee_won_bar_call(sRust, EeWonBarCall::GetPart, 2, 0, 0) : sOrig->mScrollRect.get();
}

static intptr_t SomeComp()
{
    unsigned w = DR(6);
    if (w < 4 && sParts[w] != 0) return sParts[w];
    if (w == 4 || sComps.empty()) return 0;
    return 0x2001 + (intptr_t)DR((unsigned)sComps.size());
}

static void Run(bool inRust)
{
    static const int kWeights[kCalls] = {3, 3, 5, 6, 2, 1, 3, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int aTotal = 0;
    for (int i = 0; i < kCalls; i++) aTotal += kWeights[i];
    int r0 = (int)DR(aTotal), k = 0;
    while (r0 >= kWeights[k]) r0 -= kWeights[k++];
    long long a = 0, b = 0, c = 0;
    switch (k) {
    case kSize:
        sWidth = 5 + (int)DR(30); sHeight = 30 + (int)DR(300);
        if (DR(4) == 0) { int t = sWidth; sWidth = sHeight; sHeight = t; }
        a = sWidth; b = sHeight;
        Line("size %lld %lld", sWidth, sHeight);
        break;
    case kDown: case kDrag:
        a = (int)DR(sWidth + 20) - 10; b = (int)DR(sHeight + 20) - 10; c = k == kDown ? DR(3) : 0;
        sMouseChild = DR(3) != 0 ? sParts[3] : SomeComp();
        Line("mouse-child %llx", (long long)sMouseChild);
        break;
    case kEvent: {
        HostEvent e;
        e.type = DR(6) != 0 ? 15 : (int)DR(30);
        e.comp = SomeComp();
        a = 0x9000 + (long long)sEvents.size();
        sEvents[(intptr_t)a] = e;
        sMouseX = (int)DR(sWidth + 20) - 10; sMouseY = (int)DR(sHeight + 20) - 10;
        Line("event %llx %lld %llx", a, e.type, (long long)e.comp);
        Line("mouse-pos %lld %lld", sMouseX, sMouseY);
        break;
    }
    case kEnable: a = DR(2); break;
    case kCopy: a = DR(60); b = DR(60); c = DR(600); break;
    case kRange: b = DR(5) == 0 ? 0 : DR(200); a = DR(4) == 0 ? (long long)DR((unsigned)b + 1) : b + 1 + DR(400); break;
    case kLines: a = 1 + DR(60); b = 1 + DR(60); break;
    case kPosition: a = (int)DR(500) - 50; break;
    case kLayout: a = DR(2); break;
    case kPart: {
        a = DR(4);
        if (DR(5) == 0 && sParts[a] != 0) b = sParts[a];
        else if (a == 3) b = NewComp(DR(4) == 0 ? 0 : 3 + (int)DR(20), 3 + (int)DR(40));
        else b = NewComp(DR(4) == 0 ? 0 : 8 + (int)DR(12), DR(4) == 0 ? 0 : 8 + (int)DR(12));
        break;
    }
    case kVertical: a = DR(2); break;
    case kResize: a = DR(2); break;
    case kMin: a = DR(400); break;
    case kGetPart: a = DR(4); break;
    default: break;
    }
    Call(inRust, k, a, b, c);
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sComps.clear(); sEvents.clear();
    sNextId = 0;
    sSeed = inSeq * 2654435761u + 31u;
    sDriverSeed = inSeq * 40503u + 13u;
    bool aVertical = DR(4) != 0;
    sWidth = 8 + (int)DR(20);
    sHeight = 40 + (int)DR(300);
    if (!aVertical) { int t = sWidth; sWidth = sHeight; sHeight = t; }
    sDisabled = false;
    sMouseChild = 0; sMouseX = sMouseY = 0;
    for (int i = 0; i < 4; i++) sParts[i] = 0;
    char aLine[100];
    snprintf(aLine, sizeof(aLine), "start %d %d %d", sWidth, sHeight, aVertical ? 1 : 0);
    sTrace.push_back(aLine);
    if (inRust) sRust = ee_won_bar_new((void*)0x54, ScriptedHost, aVertical);
    else sOrig = new orig::Scrollbar(aVertical);
    // the parts (a skin's or MSScrollbar's constructor), then the bar is added to its parent
    for (int i = 0; i < 4; i++) {
        if (i == 2 && DR(4) == 0) continue;
        intptr_t p = i == 3 ? NewComp(DR(4) == 0 ? 0 : 3 + (int)DR(20), 3 + (int)DR(40))
                            : NewComp(DR(3) == 0 ? 0 : 8 + (int)DR(12), DR(3) == 0 ? 0 : 8 + (int)DR(12));
        Call(inRust, kPart, i, p, 0);
    }
    Call(inRust, kAdded, 0, 0, 0);
    for (int i = 0; i < inCalls; i++) Run(inRust);
    sTrace.push_back("free");
    if (inRust) { ee_won_bar_free(sRust); sRust = NULL; }
    else { delete sOrig; sOrig = NULL; }
    bool aBack = true;
    for (std::map<intptr_t, HostComp>::const_iterator i = sComps.begin(); i != sComps.end(); ++i)
        if (i->second.held != 0) aBack = false;
    sTrace.push_back(aBack ? "refs back" : "refs LEAKED");
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    // the events a component object fires are the original enum's types
    CHECK(ee_won_gui_event_type(EeWonGuiEvent::Scroll) == WONAPI::ComponentEvent_Scroll);
    CHECK(ee_won_gui_event_type(EeWonGuiEvent::InputReturn) == WONAPI::ComponentEvent_InputReturn);
    CHECK(ee_won_gui_event_type(EeWonGuiEvent::InputTextChanged) == WONAPI::ComponentEvent_InputTextChanged);
    CHECK(ee_won_gui_event_type(EeWonGuiEvent::ListSelChanged) == WONAPI::ComponentEvent_ListSelChanged);
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
        printf("won_barobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
