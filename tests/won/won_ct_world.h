// The scripted world of the container goldens (won_ctobj_test, won_dialogobj_test): twelve components, the first
// four of them containers, with their bounds, flags, parents, timer flags, invalid states, focus ids and control
// ids, and the root, the window's focus, the mouse and the cursors; Component's own behaviour; the host every side
// runs over. A test defines, before including it: SideCall (a container's call on the side running), CtObjectOf
// (the object CtContainerOf answers), ExtraRead (its own pure reads), ExtraOp (its own traced ops) and ExtraLine
// (an op line it prints itself); and WON_CT_N / WON_CT_CT for a bigger world (won_tabobj_test).
#ifndef WON_CT_WORLD_H
#define WON_CT_WORLD_H
#include "rust/ee_won.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sDriverSeed;
static unsigned DR(unsigned n) { sDriverSeed = sDriverSeed * 69069u + 1u; return n ? (sDriverSeed >> 16) % n : 0; }
static unsigned sAnswerSeed;
static unsigned AR(unsigned n) { sAnswerSeed = sAnswerSeed * 1103515245u + 12345u; return n ? (sAnswerSeed >> 16) % n : 0; }

#ifdef WON_CT_N
enum { kN = WON_CT_N, kCt = WON_CT_CT };
#else
enum { kN = 12, kCt = 4 };
#endif
static const intptr_t kGraphics = 0x7000, kIds = 0x6000, kEvent = 0x6100;

struct World {
    int x, y, w, h;
    uint32_t bits;
    intptr_t parent;
    int timer, invalid, focusId, controlId;
    int refs;
};
static World sW[kN + 1];
static intptr_t sRoot, sDefCursor, sScheme;
static bool sRootFocused, sMouseIn;
static int sMouseX, sMouseY, sKeyMod;
static std::map<intptr_t, int> sCursorRefs;
static bool sRustSide;
static std::vector<std::string> sTrace;
static std::set<int> sOpsMet;

static void Line(const char* f, long long a = 0, long long b = 0, long long c = 0, long long d = 0, long long e = 0,
                 long long g = 0)
{
    char aLine[300];
    snprintf(aLine, sizeof(aLine), f, a, b, c, d, e, g);
    sTrace.push_back(aLine);
}

static void Set(int id, int field, long long v)
{
    World& w = sW[id];
    switch (field) {
    case 0: w.bits = (uint32_t)v; break;
    case 1: w.invalid = (int)v; break;
    case 2: w.timer = (int)v; break;
    case 3: w.x = (int)v; break;
    case 4: w.y = (int)v; break;
    case 5: w.w = (int)v; break;
    case 6: w.h = (int)v; break;
    case 7: w.controlId = (int)v; break;
    case 8: w.parent = (intptr_t)v; break;
    }
    Line("set %lld %lld %lld", id, field, v);
}

static void Env(int field, long long v)
{
    switch (field) {
    case 0: sRoot = (intptr_t)v; break;
    case 1: sRootFocused = v != 0; break;
    case 2: sMouseIn = v != 0; break;
    case 3: sMouseX = (int)v; break;
    case 4: sMouseY = (int)v; break;
    case 5: sKeyMod = (int)v; break;
    case 6: sDefCursor = (intptr_t)v; break;
    }
    Line("env %lld %lld", field, v);
}

// ---- the host: the world, Component's behaviour, the containers' calls on each side

static bool IsCt(intptr_t c) { return c >= 1 && c <= kCt; }


// a container's call made by the host (traced as "do")
static intptr_t Do(intptr_t ct, C call, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
{
    Line("do %lld %lld %lld %lld %lld", ct, (long long)call, a, b, c);
    return SideCall((int)ct, call, a, b, c);
}

static bool Modal(intptr_t ct) { return IsCt(ct) && SideCall((int)ct, C::GetField, (intptr_t)EeWonCtField::ModalCount, 0, 0) > 0; }
static bool WantFocus(intptr_t c)
{
    World& w = sW[c];
    return ee_won_gui_want_focus(w.bits, !(w.bits & 1), (w.bits & 0x80) != 0, w.parent != 0 && Modal(w.parent));
}

static intptr_t VCall(intptr_t c, V v, intptr_t a = 0, intptr_t b = 0);

// Component's own behaviour (a leaf's virtuals, the base calls)
static intptr_t Component_(intptr_t c, V v, intptr_t a, intptr_t b, bool leaf)
{
    World& w = sW[c];
    switch (v) {
    case V::GotFocus: Set((int)c, 0, w.bits | 0x10); return 0;
    case V::LostFocus: Set((int)c, 0, w.bits & ~0x10u); return 0;
    case V::MarkInvalid: Set((int)c, 1, 2); return 0;
    case V::Paint: Set((int)c, 1, 0); return 0;
    case V::NotifyRootVisibility: Set((int)c, 0, a ? (w.bits | 0x40000) : (w.bits & ~0x40000u)); return 0;
    case V::HitTest: return !(w.bits & 1) && !(w.bits & 0x800);
    case V::NeedsPaint: return w.invalid != 0 && !(w.bits & 1);
    case V::Invalidate:
        if (w.invalid == 2) return 0;
        if (w.parent != 0) Do(w.parent, C::ChildInvalidated, c);
        VCall(c, V::MarkInvalid);
        return 0;
    case V::RequestFocus:
        if ((w.bits & 0x10) || !WantFocus(c)) return 0;
        if (w.parent == 0) VCall(c, V::GotFocus);
        else Do(w.parent, C::SetFocusChild, c);
        return 0;
    case V::RequestTimer: {
        bool old = w.timer != 0;
        Set((int)c, 2, a ? (w.timer | 2) : (w.timer & ~2));
        if ((w.timer != 0) != old && w.parent != 0) Do(w.parent, C::AddTimerChild, c);
        return 0;
    }
    case V::AddedToParent:
        if (w.parent == 0) return 0; // (the original read the NULL parent: not driven)
        if (w.bits & 0x10) {
            if (!WantFocus(c)) VCall(c, V::LostFocus);
            else Do(w.parent, C::SetFocusChild, c);
        }
        if (w.timer != 0) Do(w.parent, C::AddTimerChild, c);
        if ((sW[w.parent].bits & 0x40000) && !(w.bits & 1)) VCall(c, V::NotifyRootVisibility, 1);
        Set((int)c, 1, 0);
        VCall(c, V::Invalidate);
        return 0;
    case V::TabFocus:
        if (b && (w.bits & 0x4000) && WantFocus(c)) {
            VCall(c, V::RequestFocus);
            return 1;
        }
        return 0;
    case V::TimerEvent: return leaf ? AR(3) != 0 : 1;
    case V::KeyDown: case V::KeyChar: case V::KeyUp: case V::MouseWheel: return leaf ? AR(2) : 0;
    case V::CanClose: return leaf ? AR(5) != 0 : 1;
    default: return 0;
    }
}

static C OverrideOf(V v)
{
    switch (v) {
    case V::PrePaint: return C::PrePaint;
    case V::Paint: return C::Paint;
    case V::EnableControlsById: return C::EnableControlsById;
    case V::NotifyRootVisibility: return C::NotifyRootVisibility;
    case V::HitTest: return C::HitTest;
    case V::MouseEnter: return C::MouseEnter;
    case V::MouseExit: return C::MouseExit;
    case V::MouseMove: return C::MouseMove;
    case V::MouseDrag: return C::MouseDrag;
    case V::MouseWheel: return C::MouseWheel;
    case V::MouseHover: return C::MouseHover;
    case V::HandleBroadcastEvent: return C::HandleBroadcastEvent;
    case V::AddedToParent: return C::AddedToParent;
    case V::MarkInvalid: return C::MarkInvalid;
    case V::ClearGrabBG: return C::ClearGrabBG;
    case V::TabFocus: return C::TabFocus;
    case V::KeyDown: return C::KeyDown;
    case V::KeyChar: return C::KeyChar;
    case V::KeyUp: return C::KeyUp;
    case V::CanClose: return C::CanClose;
    case V::GotFocus: return C::GotFocus;
    case V::LostFocus: return C::LostFocus;
    case V::TimerEvent: return C::TimerEvent;
    case V::SetCursor: return C::SetCursor;
    case V::SizeChanged: return C::SizeChanged;
    case V::FindChildAt: return C::FindChildAt;
    case V::RemoveChild: return C::RemoveChild;
    case V::SetFocusChild: return C::SetFocusChild;
    case V::AddTimerChild: return C::AddTimerChild;
    case V::ChildInvalidated: return C::ChildInvalidated;
    default: return (C)0;
    }
}

// a virtual call on a component: a container's override on its side, else Component's behaviour
static intptr_t VCall(intptr_t c, V v, intptr_t a, intptr_t b)
{
    C o = OverrideOf(v);
    if (IsCt(c) && (int)o != 0) return Do(c, o, a, b);
    return Component_(c, v, a, b, !IsCt(c));
}

static intptr_t Host(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    intptr_t self = (intptr_t)inComp;
    sOpsMet.insert((int)inOp);
    World& w = sW[self];
    switch (inOp) {
    // the pure reads
    case Op::CtBits: return sW[a].bits;
    case Op::CtParentOf: return sW[a].parent;
    case Op::CtTimerFlags: return sW[a].timer;
    case Op::CtInvalidState: return sW[a].invalid;
    case Op::CtFocusId: return sW[a].focusId;
    case Op::ChildLeft: return sW[a].x;
    case Op::ChildTop: return sW[a].y;
    case Op::ChildWidth: return sW[a].w;
    case Op::ChildHeight: return sW[a].h;
    case Op::ChildControlId: return sW[a].controlId;
    case Op::CtContains: {
        World& q = sW[a];
        return b >= q.x && b < q.x + q.w && c >= q.y && c < q.y + q.h;
    }
    case Op::CtIntersects: {
        EeWonRect r1 = {sW[a].x, sW[a].y, sW[a].w, sW[a].h}, r2 = {sW[b].x, sW[b].y, sW[b].w, sW[b].h};
        return ee_won_gui_rect_intersects(r1, r2);
    }
    case Op::CtGrabBG: return (sW[a].bits & 0x100) ? 0x9000 + a : 0;
    case Op::CtOwnScheme: return a % 3 == 0 ? 0xA000 + a : 0;
    case Op::CtScheme: return sScheme;
    case Op::CtRoot: return sRoot;
    case Op::CtRootFocused: return sRootFocused;
    case Op::CtMouseInClient: return sMouseIn;
    case Op::CtDefaultCursor: return sDefCursor;
    case Op::CtWantFocus: return WantFocus(self);
    case Op::CtNextSibling: {
        intptr_t p = sW[a].parent;
        return p == 0 ? 0 : SideCall((int)p, C::NextChild, a, b, c);
    }
    case Op::CtContainerOf: return IsCt(a) ? CtObjectOf(a) : 0;
    case Op::VpAreaRectVisible: {
        // a scroll area's view: its own size from its top left
        if (a == 0) return 0;
        const int* q = (const int*)b;
        EeWonRect r1 = {q[0], q[1], q[2], q[3]}, r2 = {0, 0, sW[a].w, sW[a].h};
        return ee_won_gui_rect_intersects(r1, r2);
    }
    case Op::GetMousePos: *(int*)a = sMouseX; *(int*)b = sMouseY; return 0;
    case Op::KeyMod: return sKeyMod;
    default: {
        intptr_t r = 0;
        if (ExtraRead(self, inOp, a, b, c, d, &r)) return r;
        break;
    }
    }
    if (inOp == Op::Draw) {
        const EeWonDrawOp* o = (const EeWonDrawOp*)b;
        Line("op %lld %lld %lld %lld %lld %lld", (long long)inOp, a, o->kind, o->a, o->b, o->c);
        sTrace.back() += " " + std::to_string(o->d) + " @" + std::to_string((long long)self);
        Line("= 0");
        return 0;
    }
    if (!ExtraLine(self, inOp, a, b, c, d))
        Line("op %lld %lld %lld %lld %lld @%lld", (long long)inOp, a, b, c, d, self);
    intptr_t r = 0;
    switch (inOp) {
    case Op::CtBase: r = Component_(self, (V)a, b, c, false); break;
    case Op::CtChild: r = VCall(a, (V)b, c, d); break;
    case Op::CtChildMouseDown: if (IsCt(a)) Do(a, C::MouseDown, b, c, d); break;
    case Op::CtChildMouseUp: if (IsCt(a)) Do(a, C::MouseUp, b, c, d); break;
    case Op::CtParentCall:
        if (w.parent == 0) break; // (the original's NULL parent: not driven)
        if ((V)a == V::SetCursor) r = Do(w.parent, C::SetCursor, b);
        else r = Do(w.parent, OverrideOf((V)a), self);
        break;
    case Op::CtVirt: r = Do(self, OverrideOf((V)a), b, c); break;
    case Op::CtDetach: Do(sW[a].parent, C::RemoveChild, a); break;
    case Op::CtSetParent: sW[a].parent = b ? self : 0; break;
    case Op::CtSetFocusId: sW[a].focusId = (int)b; break;
    case Op::CtSetInvalidState: sW[a].invalid = (int)b; break;
    case Op::CtSetTimerFlags: w.timer = (int)a; break;
    case Op::ChildSetFlags: sW[a].bits = c ? (sW[a].bits | (uint32_t)b) : (sW[a].bits & ~(uint32_t)b); break;
    case Op::CtSetScheme: sScheme = b; break;
    case Op::ChildInvalidate: VCall(a, V::Invalidate); break;
    case Op::Invalidate: VCall(self, V::Invalidate); break;
    case Op::RequestFocus: VCall(self, V::RequestFocus); break;
    case Op::ChildHold: sW[a].refs++; break;
    case Op::ChildRelease: CHECK(sW[a].refs > 0); sW[a].refs--; break;
    case Op::CtCursorHold: sCursorRefs[a]++; break;
    case Op::CtCursorRelease: CHECK(sCursorRefs[a] > 0); sCursorRefs[a]--; break;
    default: ExtraOp(self, inOp, a, b, c, d, &r); break;
    }
    Line("= %lld", r);
    return r;
}

static void TopCall(int ct, C call, intptr_t a, intptr_t b, intptr_t c)
{
    Line("call %lld %lld %lld %lld %lld", ct, (long long)call, a, b, c);
    long long r = 0, r2 = 0;
    if (call == C::DesiredSize) {
        int w = -1, h = -1;
        SideCall(ct, call, (intptr_t)&w, (intptr_t)&h, 0);
        r = w; r2 = h;
    } else r = SideCall(ct, call, a, b, c);
    Line("-> %lld %lld", r, r2);
}

static intptr_t AnyComp() { return 1 + DR(kN); }

static void RandomChange()
{
    switch (DR(10)) {
    case 0: { int id = (int)AnyComp(); Set(id, 0, sW[id].bits ^ (1u << DR(16))); break; }
    case 1: { int id = (int)AnyComp(); Set(id, 2, DR(4)); break; }
    case 2: { int id = (int)AnyComp(); Set(id, 3, (int)DR(200) - 20); Set(id, 4, (int)DR(200) - 20); break; }
    case 3: { int id = (int)AnyComp(); Set(id, 1, DR(3)); break; }
    case 4: Env(0, DR(4) ? 1 : 0); break;
    case 5: Env(1, DR(2)); break;
    case 6: Env(2, DR(4) != 0); break;
    case 7: Env(3, (int)DR(260) - 30); Env(4, (int)DR(260) - 30); break;
    case 8: Env(5, DR(2)); break;
    default: Env(6, DR(3) ? 0x5000 + DR(3) : 0); break;
    }
}

// the world of a sequence: the components (no parents yet) and the environment
static void InitWorld()
{
    sScheme = 0xB000;
    for (int id = 1; id <= kN; id++) {
        World& q = sW[id];
        q.x = (int)DR(200) - 20; q.y = (int)DR(200) - 20; q.w = 10 + (int)DR(120); q.h = 10 + (int)DR(120);
        q.bits = 0;
        if (DR(8) == 0) q.bits |= 0x1;
        if (DR(4) != 0) q.bits |= 0x8;
        if (DR(8) == 0) q.bits |= 0x40;
        if (DR(8) == 0) q.bits |= 0x80;
        if (DR(8) == 0) q.bits |= 0x100;
        if (DR(10) == 0) q.bits |= 0x800;
        if (DR(6) == 0) q.bits |= 0x1000;
        if (DR(4) == 0) q.bits |= 0x2000;
        if (DR(2) == 0) q.bits |= 0x4000;
        if (DR(4) == 0) q.bits |= 0x8000;
        if (DR(3) == 0) q.bits |= 0x40000;
        q.parent = 0; q.timer = (int)DR(3); q.invalid = (int)DR(3); q.focusId = 0; q.controlId = (int)DR(8); q.refs = 0;
        Line("comp %lld %lld %lld %lld %lld %lld", id, q.x, q.y, q.w, q.h, q.bits);
        sTrace.back() += " " + std::to_string(q.timer) + " " + std::to_string(q.invalid) + " " + std::to_string(q.controlId);
    }
    Env(0, 1); Env(1, DR(2)); Env(2, DR(4) != 0); Env(3, (int)DR(260) - 30); Env(4, (int)DR(260) - 30); Env(5, DR(2));
    Env(6, 0x5000 + DR(3));
}

#endif
