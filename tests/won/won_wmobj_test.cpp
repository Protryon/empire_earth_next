// WindowManager's popup stack, modal dialog, message loop and focus as a whole object (R6 WON round 33): the
// original WindowManager.cpp's statements (NotifyLostFocus, NotifyGotFocus, GetPopupWindow, the static SetModal,
// CalcModality, NotModalWindow, DoDialog, EndDialog, ExitMessageLoop, MessageLoop, AdjustPopupPosition, DoPopup,
// EndPopup, CheckEndPopup, FindMouseWindow, CheckPopupClick, CheckModalValid), copied from the C++ before the port
// with every call out (the components' and windows' methods, the screen rectangle, CreateNativeWindow, the
// platform's next event, the manager's reference, the window set) replaced by the world below, against ee_won's
// Rust manager (wmobj.rs) over the same world through the host ops.
//
// The world models the platform as logged host ops: eight windows (1..4 open at the start, 5..8 made by DoPopup's
// CreateNativeWindow), each with a root container (component 100 + its id), a platform rectangle, a popup flag, a
// pressed left button, a mouse place, a parent, a state, a visible flag, a modal flag and a reference count; twelve
// components (1..12) under the roots or each other or detached, each with its flags, bounds, colour schemes,
// container flag, modal level and references. A component's window is its top ancestor's (GetRoot); the click
// child is the first child under the point; AddComponent and the roots' RemoveChild / RemoveAllChildren move
// components. Every op is traced with its arguments (rectangle pointers as 0, a rectangle passed in as its numbers)
// and its answer. The platform's next event and a popup's end event run nested calls on the side's own manager
// (traced as "call" and "ret" lines between the op and its answer), so the message loop ends and a popup's
// listener can close popups; after each top-level call the manager's fields and popup list are traced.
//
// The caches (the named fonts, the default font, the native, raw and delay-load images and the caching count) use
// the same world: fonts and images are ids with reference counts; the decoders answer a new image, or NULL for some
// names; the default window's font is a new font. A cache is traced as one line after each call; the maps' clears
// give their images back in name order on both sides (the original's tree went in its own order).
//
// Round 34 adds the rest of the manager (wmloop.rs): the window set, the event list and PostEvent, the timer
// events, the broadcast set, ProcessEvents, DeliverEvent, CloseAllWindows, Add/RemoveWindow and the screen
// resolution, the original statements copied from WindowManager.cpp before the move (the event's fields, which the
// C++ read directly, are WmEvent ops on both sides; reads with no call between them are made once). The world adds
// events (ids from 0x6001: a type, a window, a mouse place and button, a key, a size, references; posted events
// start unreferenced, a window's close event is the world's), the windows' client-area, minimize and close-event
// state, the screen mode and the loop's wake-ups. A window's Close posts its close event (a nested PostEvent); an
// event's Deliver, the windows' handlers, a broadcast and a window's timer tick may make nested calls (never one
// that erases the entry a set walk is on: the original's iterator would be undefined there); the platform's next
// event often posts an event and runs ProcessEvents. Each sequence starts with AddWindow of windows 1..4; DoPopup's
// new window adds itself (a nested AddWindow). After each top-level call the loop's fields and sets are traced too;
// at the end every event's references must be back at what the world holds.
//
// Each of 20,000 sequences makes 40 random calls (every method, the setters, world changes). The traces must be
// the same. The first 30 sequences and every later one that makes an op or a call none before made are
// rust/crates/ee_won_gui/src/golden_wmobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <ctype.h>
#include "rust/ee_won.h"

typedef EeWonGuiOp Op;
typedef EeWonWmComp CC;
typedef EeWonWmWin WW;
typedef uint32_t DWORD;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

enum { kWins = 8, kComps = 12, kRoot = 100 };

// ---- the world
struct Rect { int x, y, w, h; };
struct WinW {
    bool exists, isPopup, mouseDown, visible, modal, inClient, minOnRestore, minimized;
    int state, mouseX, mouseY, parent, refs;
    intptr_t closeEvent;
    Rect rect;
};
// an event (round 34): ids kEv0 + 1 .. kEv0 + kEvMax
struct EvW {
    int type, x, y, button, key, refs, base;
    intptr_t window;
    Rect size;
};
enum { kEv0 = 0x6000, kEvMax = 3000 };
static EvW sEv[kEvMax + 1];
static int sEvCount, sSignals, sResW, sResH;
struct CompW {
    int parent, refs, modalLevel;
    unsigned bits;
    Rect b;
    bool container;
    intptr_t ownScheme;
};
static WinW sWin[kWins + 1];
static CompW sComp[kRoot + kWins + 1];
static Rect sScreen;
static uint32_t sClock;
static int sSelfRefs, sNextNew, sEvents, sNest;
static std::vector<std::string>* sTrace;
static std::set<std::string> sMet;

struct Rng {
    uint32_t s;
    uint32_t Next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    int Int(int n) { return (int)(Next() % (uint32_t)n); }
    bool Bool() { return (Next() & 1) != 0; }
};
static Rng* sRng;

static void Line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void Line(const char* fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    sTrace->push_back(buf);
}

static bool IsComp(intptr_t c) { return (c >= 1 && c <= kComps) || (c > kRoot && c <= kRoot + kWins && sWin[c - kRoot].exists); }
static int CompWindow(intptr_t c)
{
    for (int i = 0; i < 30 && IsComp(c); i++) {
        if (c > kRoot) return (int)c - kRoot;
        c = sComp[c].parent;
    }
    return 0;
}
static intptr_t CompTop(intptr_t c)
{
    for (int i = 0; i < 30 && IsComp(c); i++) {
        if (c > kRoot) return c;
        c = sComp[c].parent;
    }
    return 0;
}
static intptr_t SchemeOf(intptr_t c)
{
    for (int i = 0; i < 30 && IsComp(c); i++) {
        if (sComp[c].ownScheme) return sComp[c].ownScheme;
        c = sComp[c].parent;
    }
    return 0x9000;
}
static bool WinOk(intptr_t w) { return w >= 1 && w <= kWins && sWin[w].exists; }

// ---- the caches' objects: fonts and images by id, with their references (fonts 0x3001..0x3004 exist at the start)
static std::map<intptr_t, int> sObj;
static int sNextObj;
static intptr_t NewObj() { intptr_t id = 0x4000 + (++sNextObj); sObj[id] = 1; return id; }
static unsigned NameHash(const char* s, size_t n)
{
    unsigned h = 7;
    for (size_t i = 0; i < n; i++) h = h * 31 + (unsigned)tolower((unsigned char)s[i]);
    return h;
}
static const char* const kNames[] = {"a.png", "A.PNG", "b.png", "Lobby.jpg", "LOBBY.jpg", "c.png", "Title", "title", "Body"};
enum { kNameCount = 9 };

// the side's nested calls
enum { kArgs = 12 };
static intptr_t SideCall(bool rust, int call, const int* v);
static void TraceCall(bool rust, int call, const int* v, bool dump);
static void Nested(bool rust, bool fromEvent);
static void NestedFrom(bool rust, int kind);
enum { kExitLoop = 7, kEndDialog = 6, kPostEventCall = 36, kSubscribeCall = 40, kProcessCall = 44, kAddWindowCall = 34 };
enum { kFromDeliver, kFromHandler };
static bool EvOk(intptr_t e) { return e > kEv0 && e <= kEv0 + sEvCount; }
// a new event: the type, a window 1..8 (mostly an open one), the fields at random
static intptr_t NewEvent(Rng& g, int type)
{
    if (sEvCount >= kEvMax) return 0;
    EvW& e = sEv[++sEvCount];
    e = EvW();
    e.type = type;
    e.window = g.Int(5) == 0 ? 5 + g.Int(4) : 1 + g.Int(4);
    e.x = g.Int(500) - 50; e.y = g.Int(500) - 50;
    e.button = g.Int(4) == 0 ? g.Int(5) - 1 : 0;
    static const int keys[] = {27, 13, 65, 9, 40};
    e.key = keys[g.Int(5)];
    e.size.x = g.Int(3) == 0 ? -1 : g.Int(300); e.size.y = g.Int(3) == 0 ? -1 : g.Int(300);
    e.size.w = g.Int(3) == 0 ? -1 : 50 + g.Int(300); e.size.h = g.Int(3) == 0 ? -1 : 50 + g.Int(300);
    return kEv0 + sEvCount;
}
static int RandomType(Rng& g)
{
    int t = g.Int(18);
    return t < 15 ? t : t == 15 ? 2 : t == 16 ? 3 : 20; // 0 and 15..: no input event (Deliver); moves and presses often
}

static intptr_t World(bool rust, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonWmEvent EE;
    bool rectA = op == Op::WmScreenRect;
    bool evPtr = op == Op::WmEvent && (a == (intptr_t)EE::Mouse || a == (intptr_t)EE::SetMouse ||
                                       a == (intptr_t)EE::Size || a == (intptr_t)EE::SetSize);
    bool resGet = op == Op::WmResolution && a == 0;
    bool rectC = (op == Op::WmWin && (a == (intptr_t)WW::GetRect || a == (intptr_t)WW::SetPosSize)) || evPtr || resGet;
    bool rectB = op == Op::WmNewPopupWindow || resGet;
    bool strB = op == Op::WmDecode, strA = op == Op::WmDecodeRaw || op == Op::WmDelayImage;
    char extra[64] = "";
    if (strA || strB) {
        std::string s(strB ? (const char*)b : (const char*)a, (size_t)(strB ? c : b));
        snprintf(extra, sizeof extra, " s %s", s.c_str());
    }
    if ((op == Op::WmWin && a == (intptr_t)WW::SetPosSize) || (op == Op::WmEvent && a == (intptr_t)EE::SetSize)) {
        const Rect& r = *(const Rect*)c;
        snprintf(extra, sizeof extra, " r %d %d %d %d", r.x, r.y, r.w, r.h);
    } else if (op == Op::WmEvent && a == (intptr_t)EE::SetMouse) {
        const EeWonWmMouse& m = *(const EeWonWmMouse*)c;
        snprintf(extra, sizeof extra, " m %ld %d %d", (long)m.window, m.x, m.y);
    } else if (op == Op::WmNewPopupWindow) {
        const Rect& r = *(const Rect*)b;
        snprintf(extra, sizeof extra, " r %d %d %d %d", r.x, r.y, r.w, r.h);
    }
    Line("op %d %ld %ld %ld %ld%s", (int)op, (rectA || strA) ? 0L : (long)a, (rectB || strB) ? 0L : (long)b,
         rectC ? 0L : (long)c, (long)d, extra);
    char key[32];
    snprintf(key, sizeof key, "op %d %ld", (int)op, (op == Op::WmComp || op == Op::WmWin || op == Op::WmEvent) ? (long)a : 0L);
    sMet.insert(key);
    intptr_t r = 0;
    Rect out = {0, 0, 0, 0};
    bool hasOut = false;
    int outKind = 0; // 1 a mouse (window x y button), 2 the screen mode (w h)
    EeWonWmMouse outMouse = {0, 0, 0, 0};
    switch (op) {
    case Op::WmTick: r = (intptr_t)sClock; break;
    case Op::WmComp: {
        CompW* x = IsComp(b) ? &sComp[b] : NULL;
        if (x == NULL) break;
        switch ((CC)a) {
        case CC::SetFlags: if (d) x->bits |= (unsigned)c; else x->bits &= ~(unsigned)c; break;
        case CC::Parent: r = x->parent; break;
        case CC::EnterModal: x->modalLevel++; break;
        case CC::LeaveModal: x->modalLevel--; break;
        case CC::RequestFocus: break;
        case CC::SetPos: x->b.x = (int)c; x->b.y = (int)d; break;
        case CC::Left: r = x->b.x; break;
        case CC::Top: r = x->b.y; break;
        case CC::Width: r = x->b.w; break;
        case CC::Height: r = x->b.h; break;
        case CC::Root: r = CompWindow(b); break;
        case CC::OwnScheme: r = x->ownScheme; break;
        case CC::Scheme: r = SchemeOf(b); break;
        case CC::SetScheme: x->ownScheme = c; break;
        case CC::FireEndPopup:
            // the popup's listener: sometimes it closes popups or asks about them
            if (sNest < 3 && sRng->Int(4) == 0) Nested(rust, false);
            break;
        case CC::Bits: r = (intptr_t)x->bits; break;
        case CC::RootContainer: r = CompTop(b); break;
        case CC::ClickChildAt:
            for (int k = 1; k <= kComps; k++) {
                const CompW& y = sComp[k];
                if (y.parent == b && c >= y.b.x && c < y.b.x + y.b.w && d >= y.b.y && d < y.b.y + y.b.h) { r = k; break; }
            }
            break;
        case CC::AsContainer: r = (x->container || b > kRoot) ? b : 0; break;
        case CC::Hold: x->refs++; break;
        case CC::Release: x->refs--; break;
        case CC::MouseWheel: break;
        case CC::HandleComponentEvent:
            // a listener (un)subscribes another component (never itself: the original's walk would break)
            if (sNest < 3 && sRng->Int(4) == 0) {
                int v[kArgs] = {0};
                v[0] = 1 + sRng->Int(kComps);
                v[1] = sRng->Bool();
                if (v[0] != b) { sNest++; TraceCall(rust, kSubscribeCall, v, false); sNest--; }
            }
            break;
        }
        break;
    }
    case Op::WmWin: {
        WinW* w = WinOk(b) ? &sWin[b] : NULL;
        if (w == NULL) {
            if ((WW)a == WW::GetRect) { *(Rect*)c = out; hasOut = true; }
            break;
        }
        switch ((WW)a) {
        case WW::SetFocus: case WW::SetCapture: case WW::RootLostFocus: case WW::RootGotFocus: case WW::IdleUpdate: break;
        case WW::GetRect:
            out = w->rect;
            if (d) { out.x += 4; out.y += 20; out.w -= 8; out.h -= 24; }
            *(Rect*)c = out;
            hasOut = true;
            break;
        case WW::IsPopup: r = w->isPopup; break;
        case WW::SetIsPopup: w->isPopup = c != 0; break;
        case WW::Show: w->visible = c != 0; break;
        case WW::Close:
            // Window::Close: an open window queues its close event (QueueEvent, a nested PostEvent)
            if (w->state == 0) {
                w->state = 1;
                int v[kArgs] = {0};
                v[0] = (int)NewEvent(*sRng, 14);
                if (v[0]) sEv[v[0] - kEv0].window = b;
                v[1] = 1;
                if (v[0]) { sNest++; TraceCall(rust, kPostEventCall, v, false); sNest--; }
            }
            break;
        case WW::SetPosSize: w->rect = *(const Rect*)c; break;
        case WW::Parent: r = w->parent; break;
        case WW::AddComponent: if (IsComp(c) && c <= kComps) sComp[c].parent = kRoot + (int)b; break;
        case WW::SetModal: w->modal = c != 0; break;
        case WW::IsMouseDown: r = c == 0 && w->mouseDown; break;
        case WW::MouseX: r = w->mouseX; break;
        case WW::MouseY: r = w->mouseY; break;
        case WW::Root: r = kRoot + b; break;
        case WW::RootRemoveAll:
            for (int k = 1; k <= kComps; k++) if (sComp[k].parent == kRoot + b) sComp[k].parent = 0;
            break;
        case WW::RootRemoveChild: if (IsComp(c) && c <= kComps && sComp[c].parent == kRoot + b) sComp[c].parent = 0; break;
        case WW::State: r = w->state; break;
        case WW::IsVisible: r = w->visible; break;
        case WW::Hold: w->refs++; break;
        case WW::Release: w->refs--; break;
        case WW::MinimizeOnRestore: r = w->minOnRestore; break;
        case WW::Minimize: w->minimized = c != 0; break;
        case WW::InClient: r = w->inClient; break;
        case WW::HandleMouse: case WW::HandleKey: case WW::HandleInput:
            if (sNest < 3 && sRng->Int(5) == 0) NestedFrom(rust, kFromHandler);
            break;
        case WW::PlatformClose: w->state = 2; break;
        case WW::CloseEvent: r = w->closeEvent; break;
        case WW::TimerEvent:
            // a window's tick sometimes queues an event
            if (sNest < 3 && sRng->Int(8) == 0) {
                int v[kArgs] = {0};
                v[0] = (int)NewEvent(*sRng, RandomType(*sRng));
                if (v[0]) { sNest++; TraceCall(rust, kPostEventCall, v, false); sNest--; }
            }
            break;
        }
        break;
    }
    case Op::WmEvent: {
        EvW* e = EvOk(b) ? &sEv[b - kEv0] : NULL;
        if (e == NULL) {
            if (a == (intptr_t)EE::Mouse) { *(EeWonWmMouse*)c = outMouse; outKind = 1; }
            if (a == (intptr_t)EE::Size) { *(Rect*)c = out; hasOut = true; }
            break;
        }
        switch ((EE)a) {
        case EE::Type: r = e->type; break;
        case EE::Window: r = e->window; break;
        case EE::SetWindow: e->window = c; break;
        case EE::Mouse:
            outMouse.window = e->window; outMouse.x = e->x; outMouse.y = e->y; outMouse.button = e->button;
            *(EeWonWmMouse*)c = outMouse;
            outKind = 1;
            break;
        case EE::SetMouse: { const EeWonWmMouse& m = *(const EeWonWmMouse*)c; e->window = m.window; e->x = m.x; e->y = m.y; break; }
        case EE::Button: r = e->button; break;
        case EE::Key: r = e->key; break;
        case EE::Size: out = e->size; *(Rect*)c = out; hasOut = true; break;
        case EE::SetSize: e->size = *(const Rect*)c; break;
        case EE::Deliver:
            if (sNest < 3 && sRng->Int(4) == 0) NestedFrom(rust, kFromDeliver);
            break;
        case EE::Hold: e->refs++; break;
        case EE::Release: e->refs--; break;
        }
        break;
    }
    case Op::WmResolution:
        if (a == 0) { *(int*)b = sResW; *(int*)c = sResH; outKind = 2; }
        else { sResW = (int)b; sResH = (int)c; r = 1; }
        break;
    case Op::WmSignal: sSignals++; break;
    case Op::WmScreenRect: out = sScreen; *(Rect*)a = out; hasOut = true; break;
    case Op::WmNewPopupWindow: {
        int id = 5 + sNextNew;
        sNextNew = (sNextNew + 1) % 4;
        WinW& w = sWin[id];
        w = WinW();
        w.exists = true; w.isPopup = true; w.mouseDown = false; w.visible = false; w.modal = false;
        w.inClient = true;
        w.state = 0; w.mouseX = w.mouseY = 0; w.parent = (int)a; w.refs = 1; w.rect = *(const Rect*)b;
        sComp[kRoot + id] = CompW();
        sComp[kRoot + id].container = true;
        sComp[kRoot + id].b.w = w.rect.w; sComp[kRoot + id].b.h = w.rect.h;
        for (int k = 1; k <= kComps; k++) if (sComp[k].parent == kRoot + id) sComp[k].parent = 0;
        // Window::Create adds the window to its manager
        { int v[kArgs] = {0}; v[0] = id; sNest++; TraceCall(rust, kAddWindowCall, v, false); sNest--; }
        r = id;
        break;
    }
    case Op::WmNextEvent:
        if (++sEvents > 60 || sRng->Int(3) == 0) {
            // the loop ends: ExitMessageLoop, or EndDialog naming no window and no dialog
            int v[kArgs] = {0};
            bool exit = sRng->Bool();
            if (!exit) v[0] = sRng->Int(5) - 1;
            TraceCall(rust, exit ? kExitLoop : kEndDialog, v, false);
        } else if (sNest < 2) {
            // the platform's events: often one queued and the loop run (PlatWindowManager::GetNextEvent)
            if (sRng->Int(2) == 0) {
                int v[kArgs] = {0};
                v[0] = (int)NewEvent(*sRng, RandomType(*sRng));
                sNest++;
                if (v[0]) TraceCall(rust, kPostEventCall, v, false);
                int none[kArgs] = {0};
                TraceCall(rust, kProcessCall, none, false);
                sNest--;
            } else
                Nested(rust, true);
        }
        break;
    case Op::WmRef: sObj[b] += c ? 1 : -1; break;
    case Op::WmDecode: r = NameHash((const char*)b, (size_t)c) % 4 == 0 ? 0 : NewObj(); break;
    case Op::WmDecodeRaw: r = NameHash((const char*)a, (size_t)b) % 5 == 0 ? 0 : NewObj(); break;
    case Op::WmDelayImage: r = NameHash((const char*)a, (size_t)b) % 3 == 0 ? 0 : NewObj(); break;
    case Op::WmDefaultWindowFont: r = NewObj(); break;
    case Op::WmSelfHold: sSelfRefs++; break;
    case Op::WmSelfRelease: sSelfRefs--; break;
    default: break;
    }
    if (hasOut) Line("= %ld %d %d %d %d", (long)r, out.x, out.y, out.w, out.h);
    else if (outKind == 1) Line("= %ld %ld %d %d %d", (long)r, (long)outMouse.window, outMouse.x, outMouse.y, outMouse.button);
    else if (outKind == 2) Line("= %ld %d %d", (long)r, sResW, sResH);
    else Line("= %ld", (long)r);
    return r;
}

static intptr_t Wo(Op op, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0) { return World(false, op, a, b, c, d); }
static intptr_t C(CC call, intptr_t comp, intptr_t c = 0, intptr_t d = 0) { return Wo(Op::WmComp, (intptr_t)call, comp, c, d); }
static intptr_t Wn(WW call, intptr_t win, intptr_t c = 0, intptr_t d = 0) { return Wo(Op::WmWin, (intptr_t)call, win, c, d); }
static intptr_t Ev(EeWonWmEvent call, intptr_t ev, intptr_t c = 0, intptr_t d = 0) { return Wo(Op::WmEvent, (intptr_t)call, ev, c, d); }

// the original's SmartPtr over the world's references
template <bool kWindow> struct Ptr {
    intptr_t p;
    Ptr() : p(0) {}
    Ptr(intptr_t q) : p(q) { if (p) Hold(p); }
    Ptr(const Ptr& o) : p(o.p) { if (p) Hold(p); }
    ~Ptr() { if (p) Rel(p); }
    static void Hold(intptr_t q) { if (kWindow) Wn(WW::Hold, q); else C(CC::Hold, q); }
    static void Rel(intptr_t q) { if (kWindow) Wn(WW::Release, q); else C(CC::Release, q); }
    Ptr& operator=(intptr_t q) { if (p != q) { if (p) Rel(p); p = q; if (p) Hold(p); } return *this; }
    Ptr& operator=(const Ptr& o) { intptr_t q = o.p; return *this = q; }
    void Adopt(intptr_t q) { if (p) Rel(p); p = q; } // CreateNativeWindow's window comes held once
    intptr_t get() const { return p; }
};
typedef Ptr<true> WinPtr;
typedef Ptr<false> CompPtr;
template <EeWonWmRefKind kKind> struct ObjPtr {
    intptr_t p;
    ObjPtr() : p(0) {}
    ObjPtr(const ObjPtr& o) : p(o.p) { if (p) Hold(p); }
    ~ObjPtr() { if (p) Rel(p); }
    static void Hold(intptr_t q) { Wo(Op::WmRef, (intptr_t)kKind, q, 1); }
    static void Rel(intptr_t q) { Wo(Op::WmRef, (intptr_t)kKind, q, 0); }
    ObjPtr& operator=(intptr_t q) { if (p != q) { if (p) Rel(p); p = q; if (p) Hold(p); } return *this; }
    ObjPtr& operator=(const ObjPtr& o) { intptr_t q = o.p; return *this = q; }
    void Adopt(intptr_t q) { p = q; }
    intptr_t get() const { return p; }
};
struct StringLessNoCase {
    bool operator()(const std::string& a, const std::string& b) const
    { return ee_won_compare_no_case((const uint8_t*)a.data(), a.size(), (const uint8_t*)b.data(), b.size()) < 0; }
};
typedef ObjPtr<EeWonWmRefKind::Font> FontPtr;
typedef ObjPtr<EeWonWmRefKind::NativeImage> NativePtr;
typedef ObjPtr<EeWonWmRefKind::RawImage> RawPtr;
typedef ObjPtr<EeWonWmRefKind::Image> ImagePtr;

enum { PopupFlag_WantInput = 0x80, PopupFlag_AdjustPosition = 0x200, PopupFlag_EatMouseDownOnEnd = 0x800,
       PopupFlag_FireEventOnEnd = 0x2000, PopupFlag_DontWantMouse = 0x4000 };
enum { ComponentFlag_GrabBG = 0x100, ComponentFlag_NoInput = 0x800, ComponentFlag_AlwaysOnTop = 0x1000 };

struct PopupParams { intptr_t mParent, mPopup, mInputComponent, mOwnerComponent; int mFlags, mx, my; Rect mAvoidRect; };
struct MouseEv { intptr_t mWindow; int mx, my, mButtonOrAmount; };
typedef EeWonWmEvent EE;
static void EvHold(intptr_t e) { if (e) Ev(EE::Hold, e); }
static void EvRel(intptr_t e) { if (e) Ev(EE::Release, e); }
// the round-33 forwarders' copy of a mouse event's fields in and out
static MouseEv EvMouse(intptr_t e)
{
    EeWonWmMouse m = {0, 0, 0, 0};
    Ev(EE::Mouse, e, (intptr_t)&m);
    MouseEv r = { m.window, m.x, m.y, m.button };
    return r;
}
static void EvSetMouse(intptr_t e, const MouseEv& m)
{
    EeWonWmMouse o = { m.mWindow, m.mx, m.my, m.mButtonOrAmount };
    Ev(EE::SetMouse, e, (intptr_t)&o);
}

static EeWonRect4 R4(const Rect& r) { EeWonRect4 a = { r.x, r.y, r.w, r.h }; return a; }

// ---- the original (WindowManager.cpp before the port), over the world
struct OrigWm {
    bool mEndPopup, mInDoPopup, mInEndPopup, mPopupIsUp, mBA5;
    int mPopupFlags;
    bool mDoMouseCapturePopupSupport;
    int mCurMsgLoopDepth, mTargetMsgLoopDepth, mDialogResult;
    intptr_t mModalWindow, mModalComponent;
    WinPtr mPopupWindowSave;
    intptr_t mPopupOwnerWindow;
    CompPtr mPopupInputComponent, mPopupOwnerComponent;
    std::list<intptr_t> mPopupList;
    intptr_t mFocusWindow, mMouseDownWindow;
    DWORD mLastLoseFocusTick;
    FontPtr mDefaultFont;
    std::map<std::string, FontPtr, StringLessNoCase> mNamedFontMap;
    std::map<std::string, NativePtr, StringLessNoCase> mNativeImageMap;
    std::map<std::string, RawPtr, StringLessNoCase> mRawImageMap;
    std::map<std::string, ImagePtr, StringLessNoCase> mDelayImageMap;
    int mCacheImageCount;
    // round 34: the containers hold their entries (SmartPtrs: Hold on an insert, Release on an erase)
    std::set<intptr_t> mWindowSet;
    std::list<intptr_t> mWindowEventList;
    std::set<intptr_t> mTimerEvents, mRemoveTimerEvents;
    std::set<intptr_t> mBroadcastSet;
    intptr_t mDefaultWindow;
    DWORD mWindowCount, mLastTimerTick, mLastInputTick;
    bool mIsFullScreen, mCallingTimers;
    int mFullScreenWidth, mFullScreenHeight, mOriginalScreenWidth, mOriginalScreenHeight;

    // a map's clear (and its destruction): the entries given back in name order
    template <class M> static void Clear(M& m)
    {
        for (typename M::iterator i = m.begin(); i != m.end(); ++i) i->second = 0;
        m.clear();
    }

    void SetNamedFont(const std::string& theName, intptr_t theFont) { this->mNamedFontMap[theName] = theFont; }
    intptr_t GetNamedFont(const std::string& theName)
    {
        std::map<std::string, FontPtr, StringLessNoCase>::iterator anItr = this->mNamedFontMap.find(theName);
        if (anItr == this->mNamedFontMap.end())
            return GetDefaultFont();
        else
            return anItr->second.get();
    }
    intptr_t GetDefaultFont()
    {
        if (this->mDefaultFont.get() == 0)
        {
            FontPtr aTemp;
            aTemp.Adopt(Wo(Op::WmDefaultWindowFont));
            this->mDefaultFont = aTemp;
        }
        return this->mDefaultFont.get();
    }
    void SetDefaultFont(intptr_t theFont) { this->mDefaultFont = theFont; }
    void SetCacheImages(bool cache)
    {
        if (cache)
            this->mCacheImageCount++;
        else
        {
            this->mCacheImageCount--;
            if (this->mCacheImageCount <= 0)
            {
                this->mCacheImageCount = 0;
                Clear(this->mNativeImageMap);
                Clear(this->mRawImageMap);
            }
        }
    }
    // the answers carry the returned SmartPtr's reference (the caller gives it back)
    intptr_t DecodeImage(intptr_t theContext, const std::string& theFilePath)
    {
        if (theContext == 0)
            return 0;
        if (this->mCacheImageCount)
        {
            std::map<std::string, NativePtr, StringLessNoCase>::iterator anItr =
                this->mNativeImageMap.insert(std::make_pair(theFilePath, NativePtr())).first;
            if (anItr->second.get() == 0)
            {
                NativePtr aTemp;
                aTemp.Adopt(Wo(Op::WmDecode, theContext, (intptr_t)theFilePath.data(), (intptr_t)theFilePath.size()));
                anItr->second = aTemp;
            }
            if (anItr->second.get()) NativePtr::Hold(anItr->second.get());
            return anItr->second.get();
        }
        else
            return Wo(Op::WmDecode, theContext, (intptr_t)theFilePath.data(), (intptr_t)theFilePath.size());
    }
    intptr_t DecodeImageRaw(const std::string& theFilePath)
    {
        if (this->mCacheImageCount)
        {
            std::map<std::string, RawPtr, StringLessNoCase>::iterator anItr =
                this->mRawImageMap.insert(std::make_pair(theFilePath, RawPtr())).first;
            if (anItr->second.get() == 0)
            {
                RawPtr aTemp;
                aTemp.Adopt(Wo(Op::WmDecodeRaw, (intptr_t)theFilePath.data(), (intptr_t)theFilePath.size()));
                anItr->second = aTemp;
            }
            if (anItr->second.get()) RawPtr::Hold(anItr->second.get());
            return anItr->second.get();
        }
        else
            return Wo(Op::WmDecodeRaw, (intptr_t)theFilePath.data(), (intptr_t)theFilePath.size());
    }
    intptr_t DecodeDelayImage(const std::string& theFilePath)
    {
        if (this->mCacheImageCount)
        {
            std::pair<std::map<std::string, ImagePtr, StringLessNoCase>::iterator, bool> aRet =
                this->mDelayImageMap.insert(std::make_pair(theFilePath, ImagePtr()));
            if (aRet.second)
                aRet.first->second.Adopt(Wo(Op::WmDelayImage, (intptr_t)theFilePath.data(), (intptr_t)theFilePath.size()));
            if (aRet.first->second.get()) ImagePtr::Hold(aRet.first->second.get());
            return aRet.first->second.get();
        }
        else
            return Wo(Op::WmDelayImage, (intptr_t)theFilePath.data(), (intptr_t)theFilePath.size());
    }
    template <class M> static void DumpMap(std::string& l, const char* tag, M& m)
    {
        l += " ";
        l += tag;
        for (typename M::iterator i = m.begin(); i != m.end(); ++i) {
            char b[64];
            snprintf(b, sizeof b, " %s=%ld", i->first.c_str(), (long)i->second.get());
            l += b;
        }
    }
    void DumpCache()
    {
        char b[64];
        snprintf(b, sizeof b, "cache %d %ld", mCacheImageCount, (long)mDefaultFont.get());
        std::string l = b;
        DumpMap(l, "fonts", mNamedFontMap);
        DumpMap(l, "native", mNativeImageMap);
        DumpMap(l, "raw", mRawImageMap);
        DumpMap(l, "delay", mDelayImageMap);
        Line("%s", l.c_str());
    }

    void Construct()
    {
        mLastLoseFocusTick = 0;
        mEndPopup = false;
        mCurMsgLoopDepth = 0; mTargetMsgLoopDepth = 0; mDialogResult = 0;
        mModalWindow = 0; mModalComponent = 0; mPopupOwnerWindow = 0;
        mFocusWindow = 0; mMouseDownWindow = 0;
        mInDoPopup = false; mInEndPopup = false; mPopupFlags = 0;
        mPopupIsUp = false; mBA5 = true;
        mDoMouseCapturePopupSupport = false;
        mPopupList.clear();
        mPopupWindowSave.p = 0; mPopupInputComponent.p = 0; mPopupOwnerComponent.p = 0;
        mDefaultFont.p = 0;
        mNamedFontMap.clear(); mNativeImageMap.clear(); mRawImageMap.clear(); mDelayImageMap.clear();
        mCacheImageCount = 0; // the original left it unset
        mWindowSet.clear(); mWindowEventList.clear(); mTimerEvents.clear(); mRemoveTimerEvents.clear();
        mBroadcastSet.clear();
        mDefaultWindow = 0; mWindowCount = 0; mLastTimerTick = 0; mLastInputTick = 0;
        mIsFullScreen = false; mCallingTimers = false;
        mFullScreenWidth = mFullScreenHeight = 0; // the original left them unset
        mOriginalScreenWidth = mOriginalScreenHeight = 0;
    }
    template <bool kWindow> static void ReleaseSet(std::set<intptr_t>& theSet)
    {
        for (std::set<intptr_t>::iterator i = theSet.begin(); i != theSet.end(); ++i)
            if (kWindow) Wn(WW::Release, *i); else EvRel(*i);
        theSet.clear();
    }
    void Destruct()
    {
        // the members in reverse order: the broadcast set, the timer sets, the event list, the window set
        for (std::set<intptr_t>::iterator i = mBroadcastSet.begin(); i != mBroadcastSet.end(); ++i) C(CC::Release, *i);
        mBroadcastSet.clear();
        ReleaseSet<false>(mRemoveTimerEvents);
        ReleaseSet<false>(mTimerEvents);
        for (std::list<intptr_t>::iterator i = mWindowEventList.begin(); i != mWindowEventList.end(); ++i) EvRel(*i);
        mWindowEventList.clear();
        ReleaseSet<true>(mWindowSet);
        // then the round-33 members
        mPopupOwnerComponent = 0;
        mPopupInputComponent = 0;
        mPopupWindowSave = 0;
        Clear(mDelayImageMap);
        Clear(mRawImageMap);
        Clear(mNativeImageMap);
        Clear(mNamedFontMap);
        mDefaultFont = 0;
    }

    void NotifyLostFocus()
    {
        this->mFocusWindow = 0;
        this->mLastLoseFocusTick = (DWORD)Wo(Op::WmTick);
        if (this->mLastLoseFocusTick == 0)
            this->mLastLoseFocusTick = 1;
    }
    void NotifyGotFocus(intptr_t theWindow)
    {
        this->mFocusWindow = theWindow;
        this->mLastLoseFocusTick = 0;
    }
    intptr_t GetPopupWindow()
    {
        if (this->mPopupIsUp)
            return this->mPopupList.empty() ? 0 : C(CC::Root, this->mPopupList.front()); // empty: undefined in the original
        else
            return 0;
    }
    static void SetModal(intptr_t component, bool modal)
    {
        C(CC::SetFlags, component, 0x8000, modal);
        intptr_t parent = C(CC::Parent, component);
        while (parent != 0) {
            C(CC::SetFlags, parent, 0x8000, modal);
            if (modal) C(CC::EnterModal, parent); else C(CC::LeaveModal, parent);
            parent = C(CC::Parent, parent);
        }
    }
    void CalcModality()
    {
        if (this->mPopupInputComponent.get() != 0)
            SetModal(this->mPopupInputComponent.get(), false);
        if (this->mModalComponent != 0)
            SetModal(this->mModalComponent, false);

        this->mPopupIsUp = false;
        if (!this->mPopupList.empty())
        {
            this->mPopupIsUp = true;
            std::list<intptr_t>::iterator anItr = this->mPopupList.begin();
            while (anItr != this->mPopupList.end())
            {
                intptr_t aPopup = *anItr;
                C(CC::SetFlags, aPopup, 0x8000 | 0x10000, true);
                ++anItr;
            }
        }

        uint32_t aModality = ee_won_gui_modality(this->mPopupIsUp, this->mPopupFlags, this->mModalComponent != 0);
        bool modal = (aModality & 1) != 0;
        if (aModality & 2)
            SetModal(this->mModalComponent, true);

        if (!this->mPopupIsUp && this->mPopupOwnerComponent.get() != 0)
            this->mPopupOwnerComponent = 0;

        if (this->mPopupInputComponent.get() != 0)
        {
            if (this->mPopupIsUp)
                SetModal(this->mPopupInputComponent.get(), true);
            else
                this->mPopupInputComponent = 0;
        }

        for (std::set<intptr_t>::iterator anItr = this->mWindowSet.begin(); anItr != this->mWindowSet.end(); ++anItr)
        {
            intptr_t aWindow = *anItr;
            Wn(WW::SetModal, aWindow, ee_won_gui_window_modal(aWindow == this->mModalWindow, this->mPopupIsUp,
                                                               this->mModalComponent == Wn(WW::Root, aWindow), modal));
        }
    }
    bool NotModalWindow(intptr_t theWindow)
    {
        if (this->mModalWindow != 0 && theWindow != this->mModalWindow)
            return true;
        else
            return false;
    }
    int DoDialog(intptr_t theWindow, intptr_t theComponent)
    {
        CompPtr anOldDialog = this->mModalComponent;
        WinPtr anOldModalWindow = this->mModalWindow;
        this->mDialogResult = 0;

        if (anOldDialog.get() != 0)
            C(CC::SetFlags, anOldDialog.get(), 0x8000, false);

        if (theComponent == 0)
            theComponent = Wn(WW::Root, theWindow);

        if (this->mModalComponent != 0)
            SetModal(this->mModalComponent, false);

        this->mModalWindow = theWindow;
        this->mModalComponent = theComponent;
        this->CalcModality();
        Wn(WW::SetFocus, this->mModalWindow);
        C(CC::RequestFocus, theComponent);

        this->MessageLoop();

        SetModal(this->mModalComponent, false);
        this->mModalWindow = anOldModalWindow.get();
        this->mModalComponent = anOldDialog.get();
        this->CalcModality();

        return this->mDialogResult;
    }
    void EndDialog(int theResult, intptr_t theWindow, intptr_t theDialog)
    {
        if (!ee_won_gui_ends_dialog(theWindow != 0, theWindow == this->mModalWindow, theDialog != 0,
                                    theDialog == this->mModalComponent))
            return;
        this->mDialogResult = theResult;
        this->mTargetMsgLoopDepth--;
    }
    void ExitMessageLoop() { this->mTargetMsgLoopDepth--; }
    int MessageLoop()
    {
        if (this->mWindowSet.empty())
            return -1;
        Wo(Op::WmSelfHold);
        this->mTargetMsgLoopDepth = ++this->mCurMsgLoopDepth;
        while (this->mCurMsgLoopDepth <= this->mTargetMsgLoopDepth)
            Wo(Op::WmNextEvent);
        this->mCurMsgLoopDepth--;
        int aDialogResult = this->mDialogResult;
        Wo(Op::WmSelfRelease);
        return aDialogResult;
    }
    void AdjustPopupPosition(intptr_t theParent, intptr_t theComponent, bool inOwnWindow, const Rect& theAvoidRect)
    {
        Rect aBoundingRect;
        Rect anAvoidRect = theAvoidRect;
        if (inOwnWindow)
        {
            Wo(Op::WmScreenRect, (intptr_t)&aBoundingRect);
            Rect aWindowRect;
            Wn(WW::GetRect, theParent, (intptr_t)&aWindowRect, true);
            aBoundingRect.x -= aWindowRect.x;
            aBoundingRect.y -= aWindowRect.y;
        }
        else
        {
            intptr_t aRoot = Wn(WW::Root, theParent);
            aBoundingRect.x = (int)C(CC::Left, aRoot);
            aBoundingRect.y = (int)C(CC::Top, aRoot);
            aBoundingRect.w = (int)C(CC::Width, aRoot);
            aBoundingRect.h = (int)C(CC::Height, aRoot);
        }
        int l = (int)C(CC::Left, theComponent), t = (int)C(CC::Top, theComponent);
        int w = (int)C(CC::Width, theComponent), h = (int)C(CC::Height, theComponent);
        EeWonRect4 aPopupRect = { l, t, w, h };
        int aMoves[8];
        size_t aCount = ee_won_gui_popup_adjust(aPopupRect, R4(aBoundingRect), R4(anAvoidRect), aMoves);
        for (size_t i = 0; i < aCount; i++)
            C(CC::SetPos, theComponent, aMoves[2 * i], aMoves[2 * i + 1]);
    }
    void DoPopup(const PopupParams& theParams)
    {
        WinPtr aParent = theParams.mParent;
        CompPtr aPopup = theParams.mPopup;

        if (this->mInDoPopup || this->mInEndPopup)
            return;

        bool additional = ee_won_gui_popup_additional(theParams.mFlags, !this->mPopupList.empty());
        intptr_t aFocusCheckWindow = aParent.get();
        if (additional)
            aFocusCheckWindow = this->mPopupOwnerWindow;

        if (this->mFocusWindow != aFocusCheckWindow)
            return;

        this->mInDoPopup = true;
        if (this->mPopupIsUp && !additional)
            this->EndPopup(0);

        int y;
        int mx = (int)Wn(WW::MouseX, aParent.get());
        int my = (int)Wn(WW::MouseY, aParent.get());
        int x = ee_won_gui_popup_place(theParams.mx, theParams.my, mx, my, &y);

        C(CC::SetPos, aPopup.get(), x, y);

        this->mEndPopup = false;

        if (!additional)
        {
            this->mPopupFlags = theParams.mFlags;
            this->mPopupOwnerWindow = aParent.get();
            this->mPopupInputComponent = theParams.mInputComponent;
        }

        this->mPopupList.push_back(aPopup.get());
        bool inOwnWindow = ee_won_gui_popup_own_window(this->mPopupFlags, this->mBA5);

        if (((this->mPopupFlags & (PopupFlag_AdjustPosition)) ? true : false))
            this->AdjustPopupPosition(aParent.get(), aPopup.get(), inOwnWindow, theParams.mAvoidRect);

        WinPtr aPopupWindow;
        if (inOwnWindow)
        {
            Rect aRect;
            Wn(WW::GetRect, aParent.get(), (intptr_t)&aRect, true);
            aRect.x += (int)C(CC::Left, aPopup.get());
            aRect.y += (int)C(CC::Top, aPopup.get());
            aRect.w = (int)C(CC::Width, aPopup.get());
            aRect.h = (int)C(CC::Height, aPopup.get());

            if (this->mPopupWindowSave.get() != 0)
            {
                if (Wn(WW::Parent, this->mPopupWindowSave.get()) != aParent.get())
                    Wn(WW::Close, this->mPopupWindowSave.get());
                else
                {
                    Wn(WW::SetPosSize, this->mPopupWindowSave.get(), (intptr_t)&aRect);
                    Wn(WW::SetIsPopup, this->mPopupWindowSave.get(), true);
                    Wn(WW::Show, this->mPopupWindowSave.get(), true);
                    aPopupWindow = this->mPopupWindowSave;
                }
                this->mPopupWindowSave = 0;
            }

            if (aPopupWindow.get() == 0)
                aPopupWindow.Adopt(Wo(Op::WmNewPopupWindow, aParent.get(), (intptr_t)&aRect));
            Wn(WW::SetIsPopup, aPopupWindow.get(), true);

            if (((this->mPopupFlags & (PopupFlag_WantInput)) ? true : false))
            {
                Wn(WW::SetFocus, this->mPopupOwnerWindow);
                Wn(WW::RootLostFocus, this->mPopupOwnerWindow);
            }

            C(CC::SetPos, aPopup.get(), 0, 0);
        }
        else
        {
            aPopupWindow = aParent;
            C(CC::SetFlags, aPopup.get(), ComponentFlag_GrabBG, true);
            if (((this->mPopupFlags & (PopupFlag_DontWantMouse)) ? true : false))
                C(CC::SetFlags, aPopup.get(), ComponentFlag_NoInput, true);
        }

        this->mPopupOwnerComponent = theParams.mOwnerComponent;
        if (theParams.mOwnerComponent != 0 && C(CC::OwnScheme, aPopup.get()) == 0)
            C(CC::SetScheme, aPopup.get(), C(CC::Scheme, theParams.mOwnerComponent));

        C(CC::SetFlags, aPopup.get(), ComponentFlag_AlwaysOnTop, true);
        Wn(WW::AddComponent, aPopupWindow.get(), aPopup.get());
        if (((this->mPopupFlags & (PopupFlag_WantInput)) ? true : false))
            C(CC::RequestFocus, aPopup.get());

        if (this->mDoMouseCapturePopupSupport)
            Wn(WW::SetCapture, this->mPopupOwnerWindow, true);

        this->mInDoPopup = false;
        this->CalcModality();
    }
    void EndPopup(intptr_t thePopup)
    {
        if (this->mPopupList.empty())
            return;

        if (thePopup != 0 && !(C(CC::Bits, thePopup) & 0x10000))
            return;

        bool inEndPopup = this->mInEndPopup;
        this->mInEndPopup = true;
        while (!this->mPopupList.empty() && this->mPopupList.back() != thePopup)
            this->EndPopup(this->mPopupList.back());

        if (this->mPopupList.empty())
        {
            this->mInEndPopup = inEndPopup;
            return;
        }

        this->mEndPopup = false;

        if (this->mPopupIsUp)
        {
            CompPtr aPopupComponent = this->mPopupList.back();
            WinPtr aPopupWindow = C(CC::Root, aPopupComponent.get());
            WinPtr aPopupOwnerWindow = this->mPopupOwnerWindow;
            this->mPopupList.pop_back();

            if (this->mPopupFlags & PopupFlag_FireEventOnEnd)
                C(CC::FireEndPopup, aPopupComponent.get());

            if (this->mPopupList.empty())
            {
                if (this->mDoMouseCapturePopupSupport)
                    Wn(WW::SetCapture, this->mPopupOwnerWindow, false);

                this->mPopupOwnerWindow = 0;
                this->mMouseDownWindow = 0;
            }

            if (aPopupWindow.get() != aPopupOwnerWindow.get())
            {
                Wn(WW::SetIsPopup, aPopupWindow.get(), false);
                if (this->mPopupList.empty())
                    this->mPopupWindowSave = aPopupWindow;

                Wn(WW::RootRemoveAll, aPopupWindow.get());
                if (this->mPopupWindowSave.get() != 0)
                {
                    Wn(WW::RootRemoveAll, aPopupWindow.get());
                    if (!this->mInDoPopup)
                        Wn(WW::Show, aPopupWindow.get(), false);
                }
                else
                    Wn(WW::Close, aPopupWindow.get());

                if (this->mPopupList.empty() && ((this->mPopupFlags & (PopupFlag_WantInput)) ? true : false))
                {
                    if (this->mFocusWindow == aPopupOwnerWindow.get())
                        Wn(WW::RootGotFocus, aPopupOwnerWindow.get());
                }
            }
            else
            {
                C(CC::SetFlags, aPopupComponent.get(), 0x8000 | 0x10000, false);
                Wn(WW::RootRemoveChild, aPopupWindow.get(), aPopupComponent.get());
            }

            this->CalcModality();
        }

        this->mInEndPopup = inEndPopup;
    }
    void CheckEndPopup(intptr_t theAncestor)
    {
        if (!this->mPopupIsUp || this->mPopupOwnerComponent.get() == 0)
            return;

        intptr_t aComponent = this->mPopupOwnerComponent.get();
        while (aComponent != 0)
        {
            if (aComponent == theAncestor)
            {
                this->EndPopup(0);
                return;
            }
            aComponent = C(CC::Parent, aComponent);
        }
    }
    void FindMouseWindow(MouseEv* theEvent)
    {
        if (!this->mPopupIsUp)
            return;

        if (((this->mPopupFlags & (PopupFlag_DontWantMouse)) ? true : false) && Wn(WW::IsPopup, theEvent->mWindow))
        {
            Rect aPopupRect, anOwnerRect;
            Wn(WW::GetRect, theEvent->mWindow, (intptr_t)&aPopupRect, true);
            Wn(WW::GetRect, this->mPopupOwnerWindow, (intptr_t)&anOwnerRect, true);
            anOwnerRect.x -= aPopupRect.x;
            anOwnerRect.y -= aPopupRect.y;
            if (ee_won_gui_rect_contains(R4(anOwnerRect), theEvent->mx, theEvent->my))
            {
                theEvent->mWindow = this->mPopupOwnerWindow;
                theEvent->mx -= anOwnerRect.x;
                theEvent->my -= anOwnerRect.y;
                return;
            }
        }

        if (!this->mDoMouseCapturePopupSupport)
            return;

        Rect anOwnerRect;
        Wn(WW::GetRect, theEvent->mWindow, (intptr_t)&anOwnerRect, true);
        if (this->mMouseDownWindow != 0 && Wn(WW::IsMouseDown, this->mMouseDownWindow, 0))
        {
            Rect aRect;
            Wn(WW::GetRect, this->mMouseDownWindow, (intptr_t)&aRect, true);
            aRect.x -= anOwnerRect.x;
            aRect.y -= anOwnerRect.y;

            theEvent->mWindow = this->mMouseDownWindow;
            theEvent->mx -= aRect.x;
            theEvent->my -= aRect.y;
            return;
        }

        std::vector<intptr_t> aWindows;
        std::vector<EeWonRect4> aRects;
        for (std::list<intptr_t>::iterator anItr = this->mPopupList.begin(); anItr != this->mPopupList.end(); ++anItr)
        {
            Rect aRect;
            intptr_t aWindow = C(CC::Root, *anItr);
            Wn(WW::GetRect, aWindow, (intptr_t)&aRect, true);
            aRect.x -= anOwnerRect.x;
            aRect.y -= anOwnerRect.y;
            aWindows.push_back(aWindow);
            aRects.push_back(R4(aRect));
        }
        int64_t aHit = ee_won_gui_popup_window_at(aRects.empty() ? NULL : &aRects[0], aRects.size(), theEvent->mx, theEvent->my);
        if (aHit >= 0)
        {
            theEvent->mWindow = aWindows[(size_t)aHit];
            theEvent->mx -= aRects[(size_t)aHit].x;
            theEvent->my -= aRects[(size_t)aHit].y;
        }
    }
    bool CheckPopupClick(MouseEv* theEvent)
    {
        this->FindMouseWindow(theEvent);
        if (theEvent->mButtonOrAmount == 0)
            this->mMouseDownWindow = theEvent->mWindow;

        if (ee_won_gui_popup_click_ends(this->mPopupFlags, theEvent->mButtonOrAmount))
        {
            int x = theEvent->mx, y = theEvent->my;
            intptr_t aContainer = Wn(WW::Root, theEvent->mWindow);
            bool endPopup = true;
            while (true)
            {
                x -= (int)C(CC::Left, aContainer);
                y -= (int)C(CC::Top, aContainer);

                intptr_t aClickChild = C(CC::ClickChildAt, aContainer, x, y);
                intptr_t aChildContainer = aClickChild != 0 ? C(CC::AsContainer, aClickChild) : 0;
                uint32_t aBits = aClickChild != 0 ? (uint32_t)C(CC::Bits, aClickChild) : 0;
                uint8_t aStep = ee_won_gui_wm_popup_click_step(aClickChild != 0, aClickChild == this->mPopupInputComponent.get(),
                                                               aBits, this->mPopupInputComponent.get() != 0, aChildContainer != 0);
                if (aStep == 1)
                {
                    endPopup = false;
                    break;
                }
                else if (aStep == 0)
                    break;
                aContainer = aChildContainer;
            }

            if (endPopup)
            {
                bool eat = ((this->mPopupFlags & (PopupFlag_EatMouseDownOnEnd)) ? true : false);
                this->EndPopup(0);
                if (eat)
                    return true;
            }
        }

        if (Wn(WW::IsPopup, theEvent->mWindow))
            Wn(WW::SetFocus, this->mPopupOwnerWindow);

        return false;
    }
    bool CheckModalValid()
    {
        if (this->mModalWindow == 0)
            return true;
        if (Wn(WW::State, this->mModalWindow) != 0)
            return false;
        if (!Wn(WW::IsVisible, this->mModalWindow))
            return false;
        if (this->mModalComponent != 0)
        {
            if (C(CC::Bits, this->mModalComponent) & 1)
                return false;
            if (C(CC::RootContainer, this->mModalComponent) != Wn(WW::Root, this->mModalWindow))
                return false;
        }
        return true;
    }


    // ---- round 34: WindowManager.cpp's event loop, window set, timers, broadcasts and screen resolution
    void SetScreenResolution(int theWidth, int theHeight)
    {
        int aWidth = 0, aHeight = 0;
        Wo(Op::WmResolution, 0, (intptr_t)&aWidth, (intptr_t)&aHeight);
        if (aWidth == theWidth || aHeight == theHeight)
            return;
        Wo(Op::WmResolution, 1, theWidth, theHeight);
        this->mOriginalScreenWidth = aWidth;
        this->mOriginalScreenHeight = aHeight;
    }
    void RestoreScreenResolution()
    {
        std::set<intptr_t>::iterator anItr = this->mWindowSet.begin();
        while (anItr != this->mWindowSet.end())
        {
            intptr_t aWindow = *anItr;
            if (Wn(WW::State, aWindow) == 0 && Wn(WW::MinimizeOnRestore, aWindow))
                Wn(WW::Minimize, aWindow, true);
            ++anItr;
        }
        if (this->mOriginalScreenWidth == 0 || this->mOriginalScreenHeight == 0)
            return;
        this->SetScreenResolution(this->mOriginalScreenWidth, this->mOriginalScreenHeight);
        this->mOriginalScreenWidth = 0;
        this->mOriginalScreenHeight = 0;
    }
    void CloseAllWindows()
    {
        bool hadWindows = !this->mWindowSet.empty();
        std::set<intptr_t>::iterator anItr = this->mWindowSet.begin();
        while (anItr != this->mWindowSet.end())
        {
            Wn(WW::Close, *anItr);
            ++anItr;
        }
        if (hadWindows)
            this->ProcessEvents();
        if (this->mIsFullScreen)
            this->RestoreScreenResolution();
    }
    bool AddWindow(intptr_t theWindow)
    {
        if (this->mWindowSet.empty())
            this->mDefaultWindow = theWindow;
        if (this->mWindowSet.insert(theWindow).second)
        {
            Wn(WW::Hold, theWindow);
            this->mWindowCount++;
        }
        return true;
    }
    bool RemoveWindow(intptr_t theWindow)
    {
        if (this->mWindowSet.erase(theWindow) > 0)
        {
            Wn(WW::Release, theWindow);
            this->mWindowCount--;
            if (this->mWindowCount <= 0)
                this->mTargetMsgLoopDepth = 0;
            if (this->mWindowSet.empty())
                this->mDefaultWindow = 0;
            else
                this->mDefaultWindow = *this->mWindowSet.begin();
            std::list<intptr_t>::iterator anItr = this->mWindowEventList.begin();
            while (anItr != this->mWindowEventList.end())
            {
                intptr_t anEvent = *anItr;
                if (Ev(EE::Window, anEvent) == theWindow)
                {
                    this->mWindowEventList.erase(anItr++);
                    EvRel(anEvent);
                }
                else
                    ++anItr;
            }
            if (theWindow == this->mModalWindow)
                this->EndDialog(-1, 0, 0);
            if (theWindow == this->mPopupWindowSave.get())
                this->mPopupWindowSave = 0;
            return true;
        }
        return false;
    }
    void PostEvent(intptr_t theEvent, bool needSignal)
    {
        EvHold(theEvent);
        this->mWindowEventList.push_back(theEvent);
        if (this->mCurMsgLoopDepth <= 0)
            needSignal = true;
        if (needSignal)
            Wo(Op::WmSignal);
    }
    void AddTimerEvent(intptr_t theEvent)
    {
        if (this->mTimerEvents.insert(theEvent).second) EvHold(theEvent);
    }
    void RemoveTimerEvent(intptr_t theEvent)
    {
        if (this->mCallingTimers)
        {
            if (this->mRemoveTimerEvents.insert(theEvent).second) EvHold(theEvent);
        }
        else if (this->mTimerEvents.erase(theEvent))
            EvRel(theEvent);
    }
    void RemoveAllTimerEvents()
    {
        if (this->mCallingTimers)
        {
            // mRemoveTimerEvents = mTimerEvents: the old entries given back, the copies held
            ReleaseSet<false>(this->mRemoveTimerEvents);
            for (std::set<intptr_t>::iterator i = this->mTimerEvents.begin(); i != this->mTimerEvents.end(); ++i)
                EvHold(*i);
            this->mRemoveTimerEvents = this->mTimerEvents;
        }
        else
            ReleaseSet<false>(this->mTimerEvents);
    }
    void SubscribeToBroadcast(intptr_t theComponent, bool subscribe)
    {
        if (subscribe)
        {
            if (this->mBroadcastSet.insert(theComponent).second) C(CC::Hold, theComponent);
        }
        else if (this->mBroadcastSet.erase(theComponent))
            C(CC::Release, theComponent);
    }
    void ClearSubscriptions()
    {
        for (std::set<intptr_t>::iterator i = mBroadcastSet.begin(); i != mBroadcastSet.end(); ++i) C(CC::Release, *i);
        this->mBroadcastSet.clear();
    }
    void BroadcastEvent(intptr_t theEvent)
    {
        std::set<intptr_t>::iterator anItr = this->mBroadcastSet.begin();
        while (anItr != this->mBroadcastSet.end())
        {
            C(CC::HandleComponentEvent, *anItr, theEvent);
            ++anItr;
        }
    }
    intptr_t PeekNextEvent()
    {
        if (this->mWindowEventList.empty())
            return 0;
        else
            return this->mWindowEventList.front();
    }
    void ProcessEvents()
    {
        DWORD aTick = (DWORD)Wo(Op::WmTick);
        uint32_t anElapsed;
        if (ee_won_gui_wm_timer_step(aTick, this->mLastTimerTick, &anElapsed))
        {
            this->mLastTimerTick = aTick;
            bool aFocusIsPopup = this->mFocusWindow != 0 && Wn(WW::IsPopup, this->mFocusWindow) != 0;
            switch (ee_won_gui_wm_popup_focus_step(this->mPopupIsUp, this->mFocusWindow == this->mPopupOwnerWindow,
                                                   this->mPopupFlags, this->mFocusWindow != 0, aFocusIsPopup,
                                                   this->mEndPopup))
            {
                case 1:
                    this->mEndPopup = false;
                    Wn(WW::SetFocus, this->mPopupOwnerWindow);
                    break;
                case 2:
                    this->EndPopup(0);
                    break;
                case 3:
                    this->mEndPopup = true;
                    break;
                default:
                    this->mEndPopup = false;
                    break;
            }
            if (ee_won_gui_wm_restore_due(this->mLastLoseFocusTick, this->mFocusWindow != 0, aTick))
            {
                this->mLastLoseFocusTick = 0;
                if (this->mIsFullScreen)
                    this->RestoreScreenResolution();
            }
            std::set<intptr_t>::iterator anItr = this->mWindowSet.begin();
            while (anItr != this->mWindowSet.end())
            {
                Wn(WW::TimerEvent, *anItr, anElapsed);
                ++anItr;
            }
            if (!this->mTimerEvents.empty())
            {
                this->mCallingTimers = true;
                for (std::set<intptr_t>::iterator aTimerItr = this->mTimerEvents.begin(); aTimerItr != this->mTimerEvents.end(); ++aTimerItr)
                    Ev(EE::Deliver, *aTimerItr);
                this->mCallingTimers = false;
            }
            if (!this->mRemoveTimerEvents.empty())
            {
                for (std::set<intptr_t>::iterator aTimerItr = this->mRemoveTimerEvents.begin(); aTimerItr != this->mRemoveTimerEvents.end(); ++aTimerItr)
                    if (this->mTimerEvents.erase(*aTimerItr)) EvRel(*aTimerItr);
            }
        }
        while (!this->mWindowEventList.empty() && this->mCurMsgLoopDepth <= this->mTargetMsgLoopDepth)
        {
            // the copy and pop_front: the list's reference moves to the local
            intptr_t anEvent = this->mWindowEventList.front();
            this->mWindowEventList.pop_front();
            this->DeliverEvent(anEvent);
            EvRel(anEvent);
        }
        std::set<intptr_t>::iterator anItr = this->mWindowSet.begin();
        while (anItr != this->mWindowSet.end())
        {
            Wn(WW::IdleUpdate, *anItr);
            ++anItr;
        }
    }
    // the round-33 C++ forwarder: the event's fields in, the window and place back
    void FindMouseOn(intptr_t theEvent)
    {
        MouseEv m = EvMouse(theEvent);
        this->FindMouseWindow(&m);
        EvSetMouse(theEvent, m);
    }
    void DeliverEvent(intptr_t theEvent)
    {
        long aType = (long)Ev(EE::Type, theEvent);
        switch (aType)
        {
            case 4: // InputEvent_MouseDoubleClick
            case 5: // InputEvent_MouseDownCheckDoubleClick
            case 3: // InputEvent_MouseDown
            {
                if (this->mPopupIsUp)
                {
                    MouseEv m = EvMouse(theEvent);
                    bool eat = this->CheckPopupClick(&m);
                    EvSetMouse(theEvent, m);
                    if (eat)
                        break;
                }
                intptr_t aWindow = Ev(EE::Window, theEvent);
                bool aPopup = Wn(WW::IsPopup, aWindow) != 0;
                if (ee_won_gui_wm_press_focuses(this->mFocusWindow == aWindow, aPopup,
                                                this->mFocusWindow != aWindow && !aPopup && this->NotModalWindow(aWindow)))
                    Wn(WW::SetFocus, aWindow);
                int aButton = (int)Ev(EE::Button, theEvent);
                if (ee_won_gui_wm_capture_changes(aButton, this->mPopupIsUp, this->mDoMouseCapturePopupSupport))
                    Wn(WW::SetCapture, Ev(EE::Window, theEvent), true);
            }
            {
                intptr_t aWindow = Ev(EE::Window, theEvent);
                if (Wn(WW::InClient, aWindow))
                    Wn(WW::HandleMouse, aWindow, theEvent);
            }
            break;

            case 2: // InputEvent_MouseMove
            {
                intptr_t nextEvent = this->PeekNextEvent();
                if (nextEvent != 0 && Ev(EE::Type, nextEvent) == 2 && Ev(EE::Window, nextEvent) == Ev(EE::Window, theEvent))
                    return;
                this->FindMouseOn(theEvent);
                if (ee_won_gui_wm_move_ends_popup(this->mPopupIsUp, this->mPopupFlags))
                    this->EndPopup(0);
                Wn(WW::HandleMouse, Ev(EE::Window, theEvent), theEvent);
                break;
            }

            case 6: // InputEvent_MouseUp
            {
                this->FindMouseOn(theEvent);
                int aButton = (int)Ev(EE::Button, theEvent);
                if (aButton == 0)
                    this->mMouseDownWindow = 0;
                if (ee_won_gui_wm_capture_changes(aButton, this->mPopupIsUp, this->mDoMouseCapturePopupSupport))
                    Wn(WW::SetCapture, Ev(EE::Window, theEvent), false);
                Wn(WW::HandleMouse, Ev(EE::Window, theEvent), theEvent);
                break;
            }

            case 7: // InputEvent_MouseWheel
            {
                this->FindMouseOn(theEvent);
                if (ee_won_gui_wm_wheel_to_popup(this->mPopupIsUp, this->mPopupFlags))
                {
                    // the back of an empty list (popups flagged up mid-EndPopup): undefined in the original
                    intptr_t aPopupComponent = this->mPopupList.empty() ? 0 : this->mPopupList.back();
                    intptr_t anAmount = Ev(EE::Button, theEvent);
                    if (aPopupComponent != 0)
                        C(CC::MouseWheel, aPopupComponent, anAmount);
                    return;
                }
                Wn(WW::HandleMouse, Ev(EE::Window, theEvent), theEvent);
                break;
            }

            case 8: // InputEvent_KeyDown
            case 9: // InputEvent_KeyUp
            case 10: // InputEvent_KeyChar
            {
                int aKey = (int)Ev(EE::Key, theEvent);
                uint8_t aKeyStep = ee_won_gui_wm_popup_key(this->mPopupIsUp, (int)aType, this->mPopupFlags, aKey);
                if (aKeyStep == 1)
                {
                    this->EndPopup(0);
                    break;
                }
                if (aKeyStep == 2)
                {
                    intptr_t aPopupWindow = C(CC::Root, this->mPopupList.empty() ? 0 : this->mPopupList.back());
                    if (aPopupWindow != Ev(EE::Window, theEvent))
                        Ev(EE::SetWindow, theEvent, aPopupWindow);
                }
            }
            Wn(WW::HandleKey, Ev(EE::Window, theEvent), theEvent);
            break;

            case 11: // InputEvent_GotFocus
            {
                if (this->mIsFullScreen)
                    this->SetScreenResolution(this->mFullScreenWidth, this->mFullScreenHeight);
                intptr_t aWindow = Ev(EE::Window, theEvent);
                this->NotifyGotFocus(aWindow);
                Wn(WW::HandleInput, aWindow, theEvent);
                if (this->NotModalWindow(Ev(EE::Window, theEvent)))
                    Wn(WW::SetFocus, this->mModalWindow);
                return;
            }

            case 12: // InputEvent_LostFocus
                this->NotifyLostFocus();
                Wn(WW::HandleInput, Ev(EE::Window, theEvent), theEvent);
                return;

            case 1: // InputEvent_SizeChange
            {
                intptr_t aWindow = Ev(EE::Window, theEvent);
                if (aWindow == this->mPopupOwnerWindow)
                    this->EndPopup(0);
                else
                {
                    intptr_t nextEvent = this->PeekNextEvent();
                    if (nextEvent != 0 && Ev(EE::Type, nextEvent) == 1 && Ev(EE::Window, nextEvent) == aWindow)
                    {
                        Rect s1, s2;
                        Ev(EE::Size, theEvent, (intptr_t)&s1);
                        Ev(EE::Size, nextEvent, (intptr_t)&s2);
                        int32_t a1[4] = { s1.x, s1.y, s1.w, s1.h };
                        int32_t a2[4] = { s2.x, s2.y, s2.w, s2.h };
                        ee_won_gui_wm_size_merge(a1, a2);
                        Rect m = { a2[0], a2[1], a2[2], a2[3] };
                        Ev(EE::SetSize, nextEvent, (intptr_t)&m);
                        return;
                    }
                }
                Wn(WW::HandleInput, Ev(EE::Window, theEvent), theEvent);
                return;
            }

            case 13: // InputEvent_MouseExit
                Wn(WW::HandleInput, Ev(EE::Window, theEvent), theEvent);
                return;

            case 14: // InputEvent_WindowClose
            {
                intptr_t aWindow = Ev(EE::Window, theEvent);
                bool aPopup = Wn(WW::IsPopup, aWindow) != 0;
                if (ee_won_gui_wm_close_ends_popup(this->mPopupIsUp, aWindow == this->mPopupOwnerWindow, aPopup))
                    this->EndPopup(0);
                aWindow = Ev(EE::Window, theEvent);
                if (this->mFocusWindow == aWindow)
                    this->NotifyLostFocus();
                if (this->mMouseDownWindow == aWindow)
                    this->mMouseDownWindow = 0;
                Wn(WW::PlatformClose, aWindow);
                aWindow = Ev(EE::Window, theEvent);
                intptr_t aCloseEvent = Wn(WW::CloseEvent, aWindow); // SmartPtr<WindowEventBase> aCloseEvent = ...
                EvHold(aCloseEvent);
                if (aCloseEvent != 0)
                {
                    Ev(EE::SetWindow, aCloseEvent, aWindow);
                    Ev(EE::Deliver, aCloseEvent);
                }
                this->RemoveWindow(Ev(EE::Window, theEvent));
                EvRel(aCloseEvent);
            }
            return;

            default:
                Ev(EE::Deliver, theEvent);
                return;
        }
        this->mLastInputTick = (DWORD)Wo(Op::WmTick);
    }
    void DumpLoop()
    {
        Line("loop %ld %u %d %d %d %d %d %u %u %d", (long)mDefaultWindow, (unsigned)mWindowCount, mIsFullScreen,
             mFullScreenWidth, mFullScreenHeight, mOriginalScreenWidth, mOriginalScreenHeight, (unsigned)mLastTimerTick,
             (unsigned)mLastInputTick, mCallingTimers);
        std::string l = "sets";
        const std::set<intptr_t>* sets[] = { &mWindowSet, NULL, &mTimerEvents, &mRemoveTimerEvents, &mBroadcastSet };
        for (int k = 0; k < 5; k++) {
            l += " |";
            if (k == 1) {
                for (std::list<intptr_t>::iterator i = mWindowEventList.begin(); i != mWindowEventList.end(); ++i) {
                    char b[16]; snprintf(b, sizeof b, " %ld", (long)*i); l += b;
                }
                continue;
            }
            for (std::set<intptr_t>::const_iterator i = sets[k]->begin(); i != sets[k]->end(); ++i) {
                char b[16]; snprintf(b, sizeof b, " %ld", (long)*i); l += b;
            }
        }
        Line("%s", l.c_str());
    }
    void Dump()
    {
        Line("state %d %d %d %d %d %d %d %d %d %d %ld %ld %ld %ld %ld %ld %ld %ld %u %u %ld %ld", mEndPopup, mInDoPopup,
             mInEndPopup, mPopupIsUp, mBA5, mPopupFlags, mDoMouseCapturePopupSupport, mCurMsgLoopDepth,
             mTargetMsgLoopDepth, mDialogResult, (long)mModalWindow, (long)mModalComponent, (long)mPopupWindowSave.get(),
             (long)mPopupOwnerWindow, (long)mPopupInputComponent.get(), (long)mPopupOwnerComponent.get(),
             (long)mFocusWindow, (long)mMouseDownWindow, (unsigned)mLastLoseFocusTick, (unsigned)mPopupList.size(),
             mPopupList.empty() ? 0L : (long)mPopupList.back(), mPopupList.empty() ? 0L : (long)mPopupList.front());
        std::string l = "list";
        for (std::list<intptr_t>::iterator i = mPopupList.begin(); i != mPopupList.end(); ++i) {
            char b[16];
            snprintf(b, sizeof b, " %ld", (long)*i);
            l += b;
        }
        Line("%s", l.c_str());
        DumpCache();
        DumpLoop();
    }
};

static OrigWm sOrig;
static EeWonWm* sRust;

static intptr_t RustHost(void* inWm, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)inWm;
    return World(true, op, a, b, c, d);
}

static void DumpRust()
{
    intptr_t f[EE_WON_WM_FIELD_COUNT + 1];
    for (int i = 1; i <= EE_WON_WM_FIELD_COUNT; i++) f[i] = ee_won_wm_call(sRust, EeWonWmCall::GetField, i, 0, 0, 0);
    Line("state %d %d %d %d %d %d %d %d %d %d %ld %ld %ld %ld %ld %ld %ld %ld %u %u %ld %ld", (int)f[1], (int)f[2],
         (int)f[3], (int)f[4], (int)f[5], (int)f[6], (int)f[7], (int)f[8], (int)f[9], (int)f[10], (long)f[11],
         (long)f[12], (long)f[13], (long)f[14], (long)f[15], (long)f[16], (long)f[17], (long)f[18], (unsigned)f[19],
         (unsigned)f[20], (long)f[21], (long)f[22]);
    std::string l = "list";
    int n = (int)f[20];
    for (int i = 0; i < n; i++) {
        char b[16];
        snprintf(b, sizeof b, " %ld", (long)ee_won_wm_popup_at(sRust, (size_t)i));
        l += b;
    }
    Line("%s", l.c_str());
    char aCache[4096];
    size_t n2 = ee_won_wm_cache_line(sRust, (uint8_t*)aCache, sizeof aCache - 1);
    aCache[n2] = 0;
    Line("%s", aCache);
    Line("loop %ld %u %d %d %d %d %d %u %u %d", (long)f[23], (unsigned)f[24], (int)f[25], (int)f[26], (int)f[27],
         (int)f[28], (int)f[29], (unsigned)f[30], (unsigned)f[31], (int)f[32]);
    std::string s = "sets";
    for (int k = 0; k < 5; k++) {
        s += " |";
        int n = (int)f[33 + k];
        for (int i = 0; i < n; i++) {
            char b[16];
            snprintf(b, sizeof b, " %ld", (long)ee_won_wm_set_at(sRust, (uint32_t)k, (size_t)i));
            s += b;
        }
    }
    Line("%s", s.c_str());
}

// ---- the calls (the "call" lines' numbers; the Rust replay maps the same numbers)
enum Call {
    kNotifyLost, kNotifyGot, kPopupWindow, kCalcModality, kNotModal, kDoDialog, kEndDialogCall, kExitLoopCall,
    kMessageLoop, kAdjust, kDoPopup, kEndPopup, kCheckEndPopup, kFindMouse, kCheckClick, kCheckModalValid,
    kSetOwnWindow, kSetCapture, kSetEndPopup, kSetMouseDown, kSetTarget, kSetLastLose, kSetWindowSave,
    kSetNamedFont, kGetNamedFont, kGetDefaultFont, kSetDefaultFont, kSetCacheImages, kDecodeImage, kDecodeRaw,
    kDecodeDelay,
    // round 34
    kSetScreenRes, kRestoreScreenRes, kCloseAll, kAddWindow, kRemoveWindow, kPostEvent, kAddTimer, kRemoveTimer,
    kRemoveAllTimers, kSubscribe, kClearSubs, kBroadcast, kPeek, kProcess, kDeliver, kSetFullScreen, kSetLastTimer,
    kWorld, kCallCount
};
static_assert(kPostEvent == kPostEventCall && kSubscribe == kSubscribeCall && kProcess == kProcessCall &&
              kAddWindow == kAddWindowCall, "the world's nested call numbers");

static intptr_t SideCall(bool rust, int call, const int* v)
{
    typedef EeWonWmCall K;
#define RC(c, a, b, cc, d) ee_won_wm_call(sRust, c, (intptr_t)(a), (intptr_t)(b), (intptr_t)(cc), (intptr_t)(d))
    OrigWm& o = sOrig;
    switch (call) {
    case kNotifyLost: if (rust) RC(K::NotifyLostFocus, 0, 0, 0, 0); else o.NotifyLostFocus(); return 0;
    case kNotifyGot: if (rust) RC(K::NotifyGotFocus, v[0], 0, 0, 0); else o.NotifyGotFocus(v[0]); return 0;
    case kPopupWindow: return rust ? RC(K::PopupWindow, 0, 0, 0, 0) : o.GetPopupWindow();
    case kCalcModality: if (rust) RC(K::CalcModality, 0, 0, 0, 0); else o.CalcModality(); return 0;
    case kNotModal: return rust ? RC(K::NotModalWindow, v[0], 0, 0, 0) : o.NotModalWindow(v[0]);
    case kDoDialog: return rust ? RC(K::DoDialog, v[0], v[1], 0, 0) : o.DoDialog(v[0], v[1]);
    case kEndDialog: if (rust) RC(K::EndDialog, v[0], v[1], v[2], 0); else o.EndDialog(v[0], v[1], v[2]); return 0;
    case kExitLoop: if (rust) RC(K::ExitMessageLoop, 0, 0, 0, 0); else o.ExitMessageLoop(); return 0;
    case kMessageLoop: return rust ? RC(K::MessageLoop, 0, 0, 0, 0) : o.MessageLoop();
    case kAdjust: {
        Rect r = { v[3], v[4], v[5], v[6] };
        EeWonRect4 r4 = R4(r);
        if (rust) RC(K::AdjustPopupPosition, v[0], v[1], v[2], &r4); else o.AdjustPopupPosition(v[0], v[1], v[2] != 0, r);
        return 0;
    }
    case kDoPopup: {
        if (rust) {
            EeWonWmPopup p = {};
            p.parent = v[0]; p.popup = v[1]; p.input = v[2]; p.owner = v[3]; p.flags = v[4]; p.x = v[5]; p.y = v[6];
            p.avoid.x = v[7]; p.avoid.y = v[8]; p.avoid.w = v[9]; p.avoid.h = v[10];
            RC(K::DoPopup, &p, 0, 0, 0);
        } else {
            PopupParams p = { v[0], v[1], v[2], v[3], v[4], v[5], v[6], { v[7], v[8], v[9], v[10] } };
            o.DoPopup(p);
        }
        return 0;
    }
    case kEndPopup: if (rust) RC(K::EndPopup, v[0], 0, 0, 0); else o.EndPopup(v[0]); return 0;
    case kCheckEndPopup: if (rust) RC(K::CheckEndPopup, v[0], 0, 0, 0); else o.CheckEndPopup(v[0]); return 0;
    case kFindMouse: case kCheckClick: {
        intptr_t r;
        intptr_t w;
        int x, y;
        if (rust) {
            EeWonWmMouse m = { v[0], v[1], v[2], v[3] };
            r = RC(call == kFindMouse ? K::FindMouseWindow : K::CheckPopupClick, &m, 0, 0, 0);
            w = m.window; x = m.x; y = m.y;
        } else {
            MouseEv m = { v[0], v[1], v[2], v[3] };
            if (call == kFindMouse) { o.FindMouseWindow(&m); r = 0; } else r = o.CheckPopupClick(&m);
            w = m.mWindow; x = m.mx; y = m.my;
        }
        Line("event %ld %d %d", (long)w, x, y);
        return r;
    }
    case kCheckModalValid: return rust ? RC(K::CheckModalValid, 0, 0, 0, 0) : o.CheckModalValid();
    case kSetOwnWindow: if (rust) RC(K::SetField, EeWonWmField::OwnWindow, v[0], 0, 0); else o.mBA5 = v[0] != 0; return 0;
    case kSetCapture: if (rust) RC(K::SetField, EeWonWmField::CaptureSupport, v[0], 0, 0); else o.mDoMouseCapturePopupSupport = v[0] != 0; return 0;
    case kSetEndPopup: if (rust) RC(K::SetField, EeWonWmField::EndPopup, v[0], 0, 0); else o.mEndPopup = v[0] != 0; return 0;
    case kSetMouseDown: if (rust) RC(K::SetField, EeWonWmField::MouseDownWindow, v[0], 0, 0); else o.mMouseDownWindow = v[0]; return 0;
    case kSetTarget: if (rust) RC(K::SetField, EeWonWmField::TargetDepth, v[0], 0, 0); else o.mTargetMsgLoopDepth = v[0]; return 0;
    case kSetLastLose: if (rust) RC(K::SetField, EeWonWmField::LastLoseFocusTick, v[0], 0, 0); else o.mLastLoseFocusTick = (DWORD)v[0]; return 0;
    case kSetWindowSave: if (rust) RC(K::SetField, EeWonWmField::PopupWindowSave, v[0], 0, 0); else o.mPopupWindowSave = v[0]; return 0;
    case kSetNamedFont: case kGetNamedFont: case kDecodeImage: case kDecodeRaw: case kDecodeDelay: {
        std::string n = kNames[v[call == kDecodeImage ? 1 : 0]];
        intptr_t s = (intptr_t)n.data(), len = (intptr_t)n.size();
        intptr_t r = 0;
        EeWonWmRefKind kind = EeWonWmRefKind::NativeImage;
        switch (call) {
        case kSetNamedFont: if (rust) RC(K::SetNamedFont, s, len, v[1], 0); else o.SetNamedFont(n, v[1]); return 0;
        case kGetNamedFont: return rust ? RC(K::GetNamedFont, s, len, 0, 0) : o.GetNamedFont(n);
        case kDecodeImage: r = rust ? RC(K::DecodeImage, v[0], s, len, 0) : o.DecodeImage(v[0], n); break;
        case kDecodeRaw: r = rust ? RC(K::DecodeImageRaw, s, len, 0, 0) : o.DecodeImageRaw(n); kind = EeWonWmRefKind::RawImage; break;
        default: r = rust ? RC(K::DecodeDelayImage, s, len, 0, 0) : o.DecodeDelayImage(n); kind = EeWonWmRefKind::Image; break;
        }
        // the caller's SmartPtr given back
        if (r) World(rust, Op::WmRef, (intptr_t)kind, r, 0, 0);
        return r;
    }
    case kGetDefaultFont: return rust ? RC(K::GetDefaultFont, 0, 0, 0, 0) : o.GetDefaultFont();
    case kSetDefaultFont: if (rust) RC(K::SetDefaultFont, v[0], 0, 0, 0); else o.SetDefaultFont(v[0]); return 0;
    case kSetCacheImages: if (rust) RC(K::SetCacheImages, v[0], 0, 0, 0); else o.SetCacheImages(v[0] != 0); return 0;
    case kSetScreenRes: if (rust) RC(K::SetScreenResolution, v[0], v[1], 0, 0); else o.SetScreenResolution(v[0], v[1]); return 0;
    case kRestoreScreenRes: if (rust) RC(K::RestoreScreenResolution, 0, 0, 0, 0); else o.RestoreScreenResolution(); return 0;
    case kCloseAll: if (rust) RC(K::CloseAllWindows, 0, 0, 0, 0); else o.CloseAllWindows(); return 0;
    case kAddWindow: return rust ? RC(K::AddWindow, v[0], 0, 0, 0) : o.AddWindow(v[0]);
    case kRemoveWindow: return rust ? RC(K::RemoveWindow, v[0], 0, 0, 0) : o.RemoveWindow(v[0]);
    case kPostEvent: if (rust) RC(K::PostEvent, v[0], v[1], 0, 0); else o.PostEvent(v[0], v[1] != 0); return 0;
    case kAddTimer: if (rust) RC(K::AddTimerEvent, v[0], 0, 0, 0); else o.AddTimerEvent(v[0]); return 0;
    case kRemoveTimer: if (rust) RC(K::RemoveTimerEvent, v[0], 0, 0, 0); else o.RemoveTimerEvent(v[0]); return 0;
    case kRemoveAllTimers: if (rust) RC(K::RemoveAllTimerEvents, 0, 0, 0, 0); else o.RemoveAllTimerEvents(); return 0;
    case kSubscribe: if (rust) RC(K::SubscribeToBroadcast, v[0], v[1], 0, 0); else o.SubscribeToBroadcast(v[0], v[1] != 0); return 0;
    case kClearSubs: if (rust) RC(K::ClearSubscriptions, 0, 0, 0, 0); else o.ClearSubscriptions(); return 0;
    case kBroadcast: if (rust) RC(K::BroadcastEvent, v[0], 0, 0, 0); else o.BroadcastEvent(v[0]); return 0;
    case kPeek: return rust ? RC(K::PeekNextEvent, 0, 0, 0, 0) : o.PeekNextEvent();
    case kProcess: if (rust) RC(K::ProcessEvents, 0, 0, 0, 0); else o.ProcessEvents(); return 0;
    case kDeliver: if (rust) RC(K::DeliverEvent, v[0], 0, 0, 0); else o.DeliverEvent(v[0]); return 0;
    case kSetFullScreen:
        if (rust) {
            RC(K::SetField, EeWonWmField::FullScreen, v[0], 0, 0);
            RC(K::SetField, EeWonWmField::FullScreenWidth, v[1], 0, 0);
            RC(K::SetField, EeWonWmField::FullScreenHeight, v[2], 0, 0);
        } else { o.mIsFullScreen = v[0] != 0; o.mFullScreenWidth = v[1]; o.mFullScreenHeight = v[2]; }
        return 0;
    case kSetLastTimer: if (rust) RC(K::SetField, EeWonWmField::LastTimerTick, v[0], 0, 0); else o.mLastTimerTick = (DWORD)v[0]; return 0;
    }
#undef RC
    return 0;
}

// a free component (not a popup now: a popup listed twice would make the original's EndPopup loop forever)
static int FreeComp(Rng& g)
{
    for (int t = 0; t < 8; t++) {
        int c = 1 + g.Int(kComps);
        if (!(sComp[c].bits & 0x10000)) return c;
    }
    return 0;
}
static int AnyWin(Rng& g) { return g.Int(8) == 0 ? 0 : 1 + g.Int(kWins); }
static int AnyComp(Rng& g) { return g.Int(6) == 0 ? 0 : g.Int(8) == 0 ? kRoot + 1 + g.Int(4) : 1 + g.Int(kComps); }

static int RandomFlags(Rng& g)
{
    static const int bits[] = {0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x800, 0x1000, 0x2000,
                               0x4000, 0x10000};
    int f = 0;
    for (int i = 0; i < 15; i++) if (g.Int(3) == 0) f |= bits[i];
    return f;
}

// the arguments of a call (the same draws on both sides)
static bool Args(Rng& g, int call, int* v)
{
    memset(v, 0, sizeof(int) * kArgs);
    switch (call) {
    case kNotifyGot: v[0] = g.Int(5) == 0 ? 0 : 1 + g.Int(g.Int(3) == 0 ? kWins : 4); break;
    case kNotModal: v[0] = AnyWin(g); break;
    case kDoDialog: v[0] = 1 + g.Int(4); v[1] = g.Int(3) == 0 ? 0 : 1 + g.Int(kComps); break;
    case kEndDialog: v[0] = g.Int(9) - 2; v[1] = g.Int(3) == 0 ? AnyWin(g) : 0; v[2] = g.Int(3) == 0 ? AnyComp(g) : 0; break;
    case kAdjust: v[0] = 1 + g.Int(4); v[1] = 1 + g.Int(kComps); v[2] = g.Bool();
        v[3] = g.Int(300); v[4] = g.Int(300); v[5] = g.Int(80); v[6] = g.Int(80); break;
    case kDoPopup:
        v[0] = g.Int(5) == 0 ? 1 + g.Int(kWins) : 1 + g.Int(4);
        v[1] = FreeComp(g);
        if (v[1] == 0) return false;
        v[2] = g.Int(3) == 0 ? 1 + g.Int(kComps) : 0;
        v[3] = g.Int(2) == 0 ? 1 + g.Int(kComps) : 0;
        v[4] = RandomFlags(g);
        v[5] = g.Int(3) == 0 ? -1 : g.Int(400) - 50;
        v[6] = g.Int(3) == 0 ? -1 : g.Int(400) - 50;
        v[7] = g.Int(300); v[8] = g.Int(300); v[9] = g.Int(60); v[10] = g.Int(60);
        break;
    case kEndPopup: v[0] = g.Int(3) == 0 ? 0 : 1 + g.Int(kComps); break;
    case kCheckEndPopup: v[0] = AnyComp(g); break;
    case kFindMouse: case kCheckClick:
        v[0] = 1 + g.Int(kWins); v[1] = g.Int(500) - 50; v[2] = g.Int(500) - 50; v[3] = g.Int(4) == 0 ? g.Int(5) - 1 : 0;
        if (g.Bool()) {
            // often inside a popup window, from its parent's place (where popup windows overlap, the topmost wins)
            int w = 5 + g.Int(4);
            if (sWin[w].exists && WinOk(sWin[w].parent)) {
                v[0] = sWin[w].parent;
                v[1] = sWin[w].rect.x - sWin[sWin[w].parent].rect.x + g.Int(30);
                v[2] = sWin[w].rect.y - sWin[sWin[w].parent].rect.y + g.Int(30);
            }
        }
        break;
    case kSetOwnWindow: case kSetEndPopup: v[0] = g.Int(3) == 0; break;
    case kSetCapture: v[0] = g.Int(3) != 0; break;
    case kSetMouseDown: v[0] = AnyWin(g); break;
    case kSetTarget: v[0] = g.Int(5) - 1; break;
    case kSetLastLose: v[0] = g.Int(3) == 0 ? 0 : (int)(sClock - (uint32_t)g.Int(5000)); break;
    case kSetWindowSave: v[0] = g.Int(3) == 0 ? 0 : 5 + g.Int(4); if (v[0] && !sWin[v[0]].exists) v[0] = 0; break;
    case kSetNamedFont: v[0] = g.Int(kNameCount); v[1] = g.Int(5) == 0 ? 0 : 0x3001 + g.Int(4); break;
    case kGetNamedFont: case kDecodeRaw: case kDecodeDelay: v[0] = g.Int(kNameCount); break;
    case kSetDefaultFont: v[0] = g.Int(4) == 0 ? 0 : 0x3001 + g.Int(4); break;
    case kSetCacheImages: v[0] = g.Int(5) < 3; break;
    case kDecodeImage: v[0] = g.Int(6) == 0 ? 0 : 0x7777; v[1] = g.Int(kNameCount); break;
    case kSetScreenRes: case kSetFullScreen: {
        static const int ws[] = {640, 800, 1024}, hs[] = {480, 600, 768};
        int k = 0;
        if (call == kSetFullScreen) v[k++] = g.Int(3) == 0;
        v[k] = ws[g.Int(3)]; v[k + 1] = hs[g.Int(3)];
        break;
    }
    case kAddWindow: v[0] = 1 + g.Int(kWins); if (!sWin[v[0]].exists) v[0] = 1 + g.Int(4); break;
    case kRemoveWindow: v[0] = AnyWin(g); break;
    case kPostEvent: v[0] = (int)NewEvent(g, RandomType(g)); v[1] = g.Int(3) == 0; if (!v[0]) return false; break;
    case kAddTimer: v[0] = (int)NewEvent(g, 20); if (!v[0]) return false; break;
    case kDeliver: v[0] = (int)NewEvent(g, RandomType(g)); if (!v[0]) return false; break;
    case kRemoveTimer: v[0] = sEvCount == 0 || g.Int(6) == 0 ? 0 : kEv0 + 1 + g.Int(sEvCount); break;
    case kSubscribe: v[0] = g.Int(8) == 0 ? kRoot + 1 + g.Int(4) : 1 + g.Int(kComps); v[1] = g.Int(3) != 0; break;
    case kBroadcast: v[0] = 0x7000 + g.Int(16); break;
    case kSetLastTimer: v[0] = (int)(sClock - (uint32_t)g.Int(100)); break;
    }
    return true;
}

static void Dump(bool rust) { if (rust) DumpRust(); else sOrig.Dump(); }

static void TraceCall(bool rust, int call, const int* v, bool dump)
{
    Line("call %d %d %d %d %d %d %d %d %d %d %d %d %d", call, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9],
         v[10], v[11]);
    char key[32];
    snprintf(key, sizeof key, "call %d", call);
    sMet.insert(key);
    intptr_t r = SideCall(rust, call, v);
    Line("ret %ld", (long)r);
    if (dump) Dump(rust);
}

// a nested call from the platform's next event or a popup's end event
static void Nested(bool rust, bool fromEvent)
{
    static const int fromEvents[] = {kDoPopup, kDoPopup, kEndPopup, kEndPopup, kCheckEndPopup, kEndDialog, kCheckClick,
                                     kFindMouse, kNotifyGot, kNotifyLost, kCalcModality, kPopupWindow, kDoDialog,
                                     kCheckModalValid};
    static const int fromEnd[] = {kEndPopup, kCheckEndPopup, kDoPopup, kPopupWindow, kCalcModality};
    Rng& g = *sRng;
    int call = fromEvent ? fromEvents[g.Int(14)] : fromEnd[g.Int(5)];
    if (fromEvent && g.Int(3) == 0) {
        static const int more[] = {kPostEvent, kProcess, kSubscribe, kAddTimer, kRemoveTimer, kBroadcast};
        call = more[g.Int(6)];
    }
    if (call == kDoDialog && sNest > 0) call = kExitLoop;
    int v[kArgs];
    if (!Args(g, call, v)) return;
    sNest++;
    TraceCall(rust, call, v, false);
    sNest--;
}

// a nested call from an event's Deliver or a window's handler (never one that erases the entry a set walk is on:
// the windows' and the broadcasts' walks call no handler that removes a window or the walked component)
static void NestedFrom(bool rust, int kind)
{
    static const int fromDeliver[] = {kPostEvent, kPostEvent, kRemoveTimer, kAddTimer, kRemoveAllTimers, kSubscribe,
                                      kRemoveWindow, kEndPopup, kDoPopup, kNotifyGot, kBroadcast, kDoDialog, kProcess,
                                      kPeek};
    static const int fromHandler[] = {kDoPopup, kEndPopup, kPostEvent, kNotifyGot, kNotifyLost, kSubscribe,
                                      kRemoveWindow, kDoDialog, kCalcModality, kAddTimer, kCheckEndPopup};
    Rng& g = *sRng;
    int call = kind == kFromDeliver ? fromDeliver[g.Int(14)] : fromHandler[g.Int(11)];
    if (call == kDoDialog && sNest > 0) call = kExitLoop;
    int v[kArgs];
    if (!Args(g, call, v)) return;
    sNest++;
    TraceCall(rust, call, v, false);
    sNest--;
}

static std::vector<std::string> RunSide(bool rust, unsigned seed, int calls)
{
    std::vector<std::string> trace;
    sTrace = &trace;
    Rng g = {seed * 2654435761u + 12345u};
    if (g.s == 0) g.s = 1;
    sRng = &g;
    sClock = g.Int(4) == 0 ? 0xFFFFFF00u : 1000;
    sScreen.x = g.Int(20); sScreen.y = g.Int(20); sScreen.w = 640 + g.Int(400); sScreen.h = 480 + g.Int(300);
    sSelfRefs = 1; sNextNew = 0; sEvents = 0; sNest = 0;
    sEvCount = 0; sSignals = 0; sResW = 800; sResH = 600;
    sObj.clear(); sNextObj = 0;
    for (intptr_t f = 0x3001; f <= 0x3004; f++) sObj[f] = 1;
    for (int w = 1; w <= kWins; w++) {
        WinW& x = sWin[w];
        x = WinW();
        x.exists = w <= 4;
        x.inClient = g.Int(5) != 0; x.minOnRestore = g.Int(3) == 0; x.minimized = false;
        x.closeEvent = 0;
        if (w <= 4 && g.Bool()) {
            // the window's close event: the world's (one reference)
            x.closeEvent = NewEvent(g, 20);
            if (x.closeEvent) { sEv[x.closeEvent - kEv0].window = 0; sEv[x.closeEvent - kEv0].refs = sEv[x.closeEvent - kEv0].base = 1; }
        }
        x.isPopup = false; x.mouseDown = g.Int(4) == 0; x.visible = g.Int(5) != 0; x.modal = false;
        x.state = g.Int(6) == 0 ? 1 + g.Int(2) : 0; x.mouseX = g.Int(300); x.mouseY = g.Int(300);
        x.parent = w > 1 && g.Int(3) == 0 ? 1 + g.Int(w - 1) : 0; x.refs = 1;
        x.rect.x = g.Int(300); x.rect.y = g.Int(300); x.rect.w = 50 + g.Int(400); x.rect.h = 50 + g.Int(400);
        CompW& r = sComp[kRoot + w];
        r = CompW();
        r.container = true; r.b.x = g.Int(5); r.b.y = g.Int(5); r.b.w = x.rect.w - 8; r.b.h = x.rect.h - 24;
    }
    for (int c = 1; c <= kComps; c++) {
        CompW& x = sComp[c];
        x = CompW();
        int where = g.Int(4);
        x.parent = where == 0 ? 0 : where == 1 && c > 1 ? 1 + g.Int(c - 1) : kRoot + 1 + g.Int(4);
        x.container = g.Int(2) == 0;
        if (x.parent >= 1 && x.parent <= kComps && !sComp[x.parent].container) x.parent = kRoot + 1 + g.Int(4);
        x.bits = g.Int(6) == 0 ? 1 : 0;
        x.b.x = g.Int(200); x.b.y = g.Int(200); x.b.w = 10 + g.Int(150); x.b.h = 10 + g.Int(150);
        x.ownScheme = g.Int(3) == 0 ? 0x9100 + c : 0;
        x.refs = 1;
    }
    if (rust) sRust = ee_won_wm_new((void*)(intptr_t)1, RustHost);
    else sOrig.Construct();
    {
        // the constructor's tick, then the four windows open (Window::Create adds each)
        int v[kArgs] = {0};
        v[0] = (int)sClock;
        TraceCall(rust, kSetLastTimer, v, true);
        for (int w = 1; w <= 4; w++) { v[0] = w; TraceCall(rust, kAddWindow, v, true); }
    }
    for (int i = 0; i < calls; i++) {
        sClock += g.Int(5) == 0 ? 0 : (uint32_t)g.Int(1500);
        int call = g.Int(kCallCount + 10);
        // the focus first, popups, posted events and the loop often
        if (call >= kCallCount) {
            int k = call - kCallCount;
            call = k < 3 ? kNotifyGot : k < 6 ? kDoPopup : k < 8 ? kPostEvent : kProcess;
        }
        if (call == kWorld) {
            int which = g.Int(9);
            if (which == 0) { int w = 1 + g.Int(kWins); sWin[w].mouseDown = !sWin[w].mouseDown; }
            else if (which == 1) { int w = 1 + g.Int(kWins); sWin[w].visible = !sWin[w].visible; }
            else if (which == 2) { int w = 1 + g.Int(kWins); sWin[w].state = g.Int(3); }
            else if (which == 3) { int c = 1 + g.Int(kComps); sComp[c].bits ^= 1; }
            else if (which == 4) { int c = 1 + g.Int(kComps); sComp[c].container = !sComp[c].container; }
            else if (which == 5) { int w = 1 + g.Int(kWins); sWin[w].inClient = !sWin[w].inClient; }
            else if (which == 6) { int w = 1 + g.Int(kWins); sWin[w].minOnRestore = !sWin[w].minOnRestore; }
            else if (which == 7) { static const int ws[] = {640, 800, 1024}, hs[] = {480, 600, 768}; sResW = ws[g.Int(3)]; sResH = hs[g.Int(3)]; }
            else { int w = 1 + g.Int(kWins); sWin[w].mouseX = g.Int(300); sWin[w].mouseY = g.Int(300); }
            Line("set %d", which);
            continue;
        }
        int v[kArgs];
        if (!Args(g, call, v)) continue;
        TraceCall(rust, call, v, true);
    }
    // the destructor's releases, then every reference must be back
    if (rust) { ee_won_wm_free(sRust); sRust = NULL; } else sOrig.Destruct();
    std::string refs = "refs";
    // the four first windows' and the components' (the windows DoPopup made are the manager's and the world's)
    for (int w = 1; w <= 4; w++) { char b[16]; snprintf(b, sizeof b, " %d", sWin[w].refs); refs += b; }
    for (int c = 1; c <= kComps; c++) { char b[16]; snprintf(b, sizeof b, " %d", sComp[c].refs); refs += b; }
    Line("%s %d", refs.c_str(), sSelfRefs);
    // the fonts back at 1, every font or image the world made back at 0
    int aBad = 0;
    for (std::map<intptr_t, int>::iterator i = sObj.begin(); i != sObj.end(); ++i)
        if (i->second != (i->first < 0x4000 ? 1 : 0)) aBad++;
    // every event's references back at the world's (0, or 1 for a window's close event)
    for (int e = 1; e <= sEvCount; e++)
        if (sEv[e].refs != sEv[e].base) aBad++;
    Line("objs %u %d", (unsigned)(sObj.size() + (size_t)sEvCount), aBad);
    return trace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kSequences = 20000;
    const int kCalls = 40;
    size_t aLines = 0;
    std::set<std::string> aSeen;
    int aDiffering = 0, aRefsOff = 0;
    for (int seq = 0; seq < kSequences; seq++) {
        sMet.clear();
        std::vector<std::string> a = RunSide(false, (unsigned)seq, kCalls);
        std::vector<std::string> b = RunSide(true, (unsigned)seq, kCalls);
        aLines += a.size();
        bool same = a == b;
        CHECK(same);
        // every reference given back (the windows' and components' counts back at 1, the manager's at 1)
        const std::string& r = a[a.size() - 2];
        if (a.back().substr(a.back().rfind(' ')) != " 0") aRefsOff++;
        bool back = true;
        for (size_t i = 5; i < r.size(); i++) if (r[i] != ' ' && r[i] != '1') back = false;
        if (!back) aRefsOff++;
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
        bool aNew = seq < 30;
        for (std::set<std::string>::const_iterator i = sMet.begin(); i != sMet.end(); ++i)
            if (aSeen.insert(*i).second) aNew = true;
        if (aPrint && aNew) {
            printf("seq %d\n", seq);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (!aPrint)
        printf("won_wmobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d with references "
               "not back, %d checks, %d failures\n",
               kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, aRefsOff, sChecks, sFailures);
    return sFailures == 0 ? 0 : 1;
}
