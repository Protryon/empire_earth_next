// RootContainer as a whole object (R6 WON round 29): the original RootContainer.cpp, the statements copied from the C++
// before the port with every call on a C++ object (the window, the cursors, the grabbers, the images, the Graphics, the
// components) replaced by the host op that makes it (RootContainer.cpp's root host runs the same calls in the same
// order) and every Container::X call made on the root's real Rust container (each side has its own), against ee_won's
// Rust RootContainer (rootobj.rs).
//
// The world is the container goldens' (won_ct_world.h): component 1 is the root, 2..4 plain Rust containers, the rest
// leaves. A call the world makes on the root runs its overrides on the side running. The cursors 0x5002..0x5004 draw
// themselves with a component (11 or 12) and a hot spot; the grabbers are 0x9000 + a component's id (its GetGrabBG),
// each with a drawing state the driver changes ("grab" lines) and the root reads (traced with its answer, "grab-read",
// so the replay needs no model); the images 0xC001..0xC006 have sizes from their id. The window's one-component flag,
// its mouse and its place are the driver's (env 14..18). Component's SetPos and SetVisible run in the world as the tab
// golden's do. The images' references are counted and must all be given back.
//
// The original's keyboard listener is left out: nothing in the game set it (its three calls never ran).
//
// Each of 20,000 sequences builds a tree under the root and makes 40 random calls on the root (and a few on the
// containers), with random changes of the world between them, then frees the root and the containers. The traces must
// be the same, every reference given back. The first 40 sequences and every later one that makes an op or a call none
// before made are rust/crates/ee_won/src/golden_rootobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include "rust/ee_won.h"

typedef EeWonGuiOp Op;
typedef EeWonCtVirt V;
typedef EeWonCtCall C;
typedef EeWonRootCall R;

static intptr_t SideCall(int ct, C call, intptr_t a, intptr_t b, intptr_t c);
static intptr_t CtObjectOf(intptr_t ct);
static bool ExtraRead(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r);
static void ExtraOp(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r);
static bool ExtraLine(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
static intptr_t Host(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
#include "won_ct_world.h"

static const intptr_t kRoot = 1;
static EeWonContainer* sCt[kCt + 1];

// the window (env 14..18)
static intptr_t sOneComponent, sWinMouseX, sWinMouseY, sWinX, sWinY;
// the grabbers (0x9000 + id)
struct Grab { int need, order, x, y, cx, cy, cw, ch; intptr_t image; };
static Grab sGrab[kN + 1];
static int sOrderCounter;
static std::map<intptr_t, int> sImageRefs;

// a cursor's component and hot spot (Cursor::GetComponent)
static intptr_t CursorComponent(intptr_t cursor, int* hx, int* hy)
{
    if (cursor == 0) { *hx = *hy = 0; return 0; }
    int k = (int)(cursor & 0xF);
    *hx = k * 2 - 3;
    *hy = 5 - k;
    return cursor == 0x5002 || cursor == 0x5004 ? 11 : cursor == 0x5003 ? 12 : 0;
}
static void ImageSize(intptr_t image, int* w, int* h)
{
    *w = 8 + (int)(image & 0xF) * 11;
    *h = 6 + (int)(image & 0xF) * 7;
}

// ---- the original (RootContainer.cpp; Container::X on the root's Rust container)

namespace orig {
struct Rect { int x, y, width, height; };
static EeWonRect ToRust(const Rect& r) { EeWonRect e = {r.x, r.y, r.width, r.height}; return e; }
static Rect FromRust(const EeWonRect& e) { Rect r = {e.x, e.y, e.width, e.height}; return r; }
struct BGStruct {
    int mImageX, mImageY;
    Rect mClipRect;
    intptr_t mImage; // SmartPtr<NativeImage>
};
struct RootContainer {
    Rect mInvalidRect;
    intptr_t mCursorComponent;
    int mDragX, mDragY;
    bool mDragging, mAllowDrag;
    std::map<int, BGStruct> mBGStructMap;

    intptr_t H(Op op, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0) { return Host((void*)kRoot, op, a, b, c, d); }
    intptr_t Ct(C call, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return ee_won_ct_call(sCt[kRoot], call, a, b, c); }
    int Left() { return (int)H(Op::ChildLeft, kRoot); }
    int Top() { return (int)H(Op::ChildTop, kRoot); }
    void Hold(intptr_t i) { if (i != 0) H(Op::RootImageHold, i); }
    void Release(intptr_t i) { if (i != 0) H(Op::RootImageRelease, i); }

    RootContainer()
    {
        Rect z = {0, 0, 0, 0};
        this->mInvalidRect = z;
        this->mCursorComponent = 0;
        this->mAllowDrag = true;
        this->mDragX = this->mDragY = 0;
        this->mDragging = false;
    }
    // the implicit destructor: the background map's images released
    void Destroy()
    {
        for (std::map<int, BGStruct>::iterator i = this->mBGStructMap.begin(); i != this->mBGStructMap.end(); ++i)
            Release(i->second.mImage);
        this->mBGStructMap.clear();
    }
    void AddBG(intptr_t theGrabber)
    {
        if (theGrabber == 0) return;
        EeWonRootGrab q;
        H(Op::RootGrabberRead, theGrabber, (intptr_t)&q); // NeedDrawBG() and the fields
        if (!q.need) return;
        // mBGStructMap[order] = BGStruct(x, y, image, clip)
        Hold(q.image);                                     // the temporary's SmartPtr
        BGStruct& aStruct = this->mBGStructMap[q.order];   // (a new entry: no image)
        if (aStruct.mImage != q.image) {                   // SmartPtr::operator=
            Release(aStruct.mImage);
            aStruct.mImage = q.image;
            Hold(q.image);
        }
        aStruct.mImageX = q.x;
        aStruct.mImageY = q.y;
        Rect aClip = {q.clip.x, q.clip.y, q.clip.w, q.clip.h};
        aStruct.mClipRect = aClip;
        Release(q.image);                                  // the temporary goes
        H(Op::RootGrabberClear, theGrabber);
    }
    void Virtual08()
    {
        Ct(C::SizeChanged);
        Rect b = {Left(), Top(), (int)H(Op::ChildWidth, kRoot), (int)H(Op::ChildHeight, kRoot)};
        this->mInvalidRect = b;
    }
    void PrePaint(intptr_t g)
    {
        Ct(C::PrePaint, g);
        for (std::map<int, BGStruct>::reverse_iterator anItr = this->mBGStructMap.rbegin(); anItr != this->mBGStructMap.rend(); ++anItr) {
            BGStruct& aStruct = anItr->second;
            int w, h;
            H(Op::RootImageSize, aStruct.mImage, (intptr_t)&w, (intptr_t)&h);
            Rect anImageRect = {aStruct.mImageX, aStruct.mImageY, w, h};
            anImageRect = FromRust(ee_won_gui_rect_intersect(ToRust(anImageRect), ToRust(aStruct.mClipRect)));
            int q[6] = {anImageRect.x, anImageRect.y, anImageRect.x - aStruct.mImageX, anImageRect.y - aStruct.mImageY,
                        anImageRect.width, anImageRect.height};
            H(Op::RootDrawImage, g, aStruct.mImage, (intptr_t)q);
        }
        Destroy(); // mBGStructMap.clear()
    }
    void Paint(intptr_t g)
    {
        H(Op::CtSetScheme, g, H(Op::RootScheme)); // g.mColorScheme = GetColorScheme()
        EeWonDrawOp o = {};
        o.kind = EE_WON_DRAW_PUSH_CLIP;
        o.a = this->mInvalidRect.x; o.b = this->mInvalidRect.y; o.c = this->mInvalidRect.width; o.d = this->mInvalidRect.height;
        H(Op::Draw, g, (intptr_t)&o);
        Ct(C::Paint, g);
        EeWonDrawOp p = {};
        p.kind = EE_WON_DRAW_POP_CLIP;
        H(Op::Draw, g, (intptr_t)&p);
        this->mInvalidRect.x = this->mInvalidRect.y = this->mInvalidRect.width = this->mInvalidRect.height = 0;
    }
    void InvalidateRect(const Rect& r) { this->mInvalidRect = FromRust(ee_won_gui_rect_union(ToRust(this->mInvalidRect), ToRust(r))); }
    bool KeyDown(int k) { Ct(C::KeyDown, k); return true; }
    bool KeyUp(int k) { Ct(C::KeyUp, k); return true; }
    bool KeyChar(int k) { Ct(C::KeyChar, k); return true; }
    void MoveCursorComponent(int x, int y)
    {
        int mx, my;
        CursorComponentOf(Ct(C::GetField, (intptr_t)EeWonCtField::CurrentCursor), &mx, &my);
        x = ee_won_gui_root_cursor_place(x, y, mx, my, &y);
        H(Op::ChildInvalidate, this->mCursorComponent);
        H(Op::ChildSetPos, this->mCursorComponent, x, y);
        Rect aRect = {x, y, (int)H(Op::ChildWidth, this->mCursorComponent), (int)H(Op::ChildHeight, this->mCursorComponent)};
        this->InvalidateRect(aRect);
    }
    intptr_t CursorComponentOf(intptr_t cursor, int* mx, int* my)
    {
        return H(Op::RootCursorComponent, cursor, (intptr_t)mx, (intptr_t)my);
    }
    void MouseMove(int x, int y)
    {
        Ct(C::MouseMove, x, y);
        if (this->mCursorComponent == 0) return;
        MoveCursorComponent(x, y);
    }
    void MouseDrag(int x, int y)
    {
        if (!this->mDragging) {
            Ct(C::MouseDrag, x, y);
            if (this->mCursorComponent != 0) MoveCursorComponent(x, y);
        } else {
            int aNewX, aNewY;
            if (!ee_won_gui_root_drag_place(this->Left(), this->Top(), x, y, this->mDragX, this->mDragY, &aNewX, &aNewY))
                return;
            EeWonRect4 r;
            H(Op::RootWindowRect, (intptr_t)&r); // aWindow->GetRect(r, false)
            int aWindowY;
            int aWindowX = ee_won_gui_root_drag_window(r.x, r.y, aNewX, aNewY, &aWindowY);
            H(Op::RootWindowSetPos, aWindowX, aWindowY);
        }
    }
    void MouseDown(int x, int y, int theButton)
    {
        Ct(C::MouseDown, x, y, theButton);
        if (theButton != 0 || !this->mAllowDrag) return;
        intptr_t aMouseChild = Ct(C::GetField, (intptr_t)EeWonCtField::MouseChild); // GetMouseChild()
        if (H(Op::RootOneComponentWindow) && aMouseChild != 0)
            aMouseChild = H(Op::RootMouseChildOf, aMouseChild);
        if (aMouseChild == 0) return;
        if (!ee_won_gui_root_drag_starts(theButton, this->mAllowDrag, (H(Op::CtBits, aMouseChild) & 0x20) != 0))
            return;
        this->mDragging = true;
        this->mDragX = x;
        this->mDragY = y;
    }
    void MouseExit()
    {
        Ct(C::MouseExit);
        this->mDragging = false;
        if (this->mCursorComponent != 0) H(Op::CtVirt, (intptr_t)V::RemoveChild, this->mCursorComponent);
    }
    void MouseUp(int x, int y, int theButton)
    {
        Ct(C::MouseUp, x, y, theButton);
        if (ee_won_gui_root_drag_ends(theButton)) this->mDragging = false;
    }
    void RemoveChild(intptr_t theChild)
    {
        if (theChild == this->mCursorComponent) this->mCursorComponent = 0;
        Ct(C::RemoveChild, theChild);
    }
    void SetCursor(intptr_t theCursor)
    {
        Ct(C::SetCursor, theCursor);
        if (theCursor == 0) theCursor = H(Op::CtDefaultCursor);
        intptr_t aComponent = 0;
        int mx = 0, my = 0;
        if (theCursor != 0) aComponent = CursorComponentOf(theCursor, &mx, &my);
        if (this->mCursorComponent != aComponent) {
            if (this->mCursorComponent != 0) H(Op::CtVirt, (intptr_t)V::RemoveChild, this->mCursorComponent);
            this->mCursorComponent = aComponent;
            if (aComponent != 0) {
                if (H(Op::CtParentOf, aComponent) != 0) {
                    H(Op::CtDetach, aComponent); // aComponent->mParent->RemoveChild(aComponent)
                    intptr_t aBG = H(Op::CtGrabBG, aComponent);
                    if (aBG != 0) H(Op::RootGrabberDrop, aBG); // aBG->Clear(); aBG->mBGImage = NULL;
                }
                H(Op::ChildSetFlags, aComponent, 0x100 | 0x800 | 0x1000, 1);
                H(Op::ChildSetVisible, aComponent, 1);
                int wx, wy;
                H(Op::RootWindowMouse, (intptr_t)&wx, (intptr_t)&wy);
                int y;
                int x = ee_won_gui_root_cursor_place(wx, wy, mx, my, &y);
                H(Op::ChildSetPos, aComponent, x, y);
                Ct(C::AddChild, aComponent);
                intptr_t aTop = Ct(C::GetField, (intptr_t)EeWonCtField::BackChild);
                if (aTop != aComponent) H(Op::RootEnsureOnTop, aComponent, aTop);
            }
        }
    }
    intptr_t Field(int f)
    {
        switch ((EeWonRootField)f) {
        case EeWonRootField::InvalidX: return mInvalidRect.x;
        case EeWonRootField::InvalidY: return mInvalidRect.y;
        case EeWonRootField::InvalidWidth: return mInvalidRect.width;
        case EeWonRootField::InvalidHeight: return mInvalidRect.height;
        case EeWonRootField::CursorComponent: return mCursorComponent;
        case EeWonRootField::Dragging: return mDragging;
        case EeWonRootField::DragX: return mDragX;
        case EeWonRootField::DragY: return mDragY;
        case EeWonRootField::AllowDrag: return mAllowDrag;
        case EeWonRootField::BgCount: return (intptr_t)mBGStructMap.size();
        }
        return 0;
    }
    intptr_t Call(R call, intptr_t a, intptr_t b, intptr_t c)
    {
        switch (call) {
        case R::SizeChanged: Virtual08(); return 0;
        case R::PrePaint: PrePaint(a); return 0;
        case R::Paint: Paint(a); return 0;
        case R::MouseExit: MouseExit(); return 0;
        case R::MouseDown: MouseDown((int)a, (int)b, (int)c); return 0;
        case R::MouseUp: MouseUp((int)a, (int)b, (int)c); return 0;
        case R::MouseMove: MouseMove((int)a, (int)b); return 0;
        case R::MouseDrag: MouseDrag((int)a, (int)b); return 0;
        case R::KeyChar: return KeyChar((int)a);
        case R::KeyDown: return KeyDown((int)a);
        case R::KeyUp: return KeyUp((int)a);
        case R::InvalidateRect: InvalidateRect(*(const Rect*)a); return 0;
        case R::SetCursor: SetCursor(a); return 0;
        case R::RemoveChild: RemoveChild(a); return 0;
        case R::AddBG: AddBG(a); return 0;
        case R::GetField: return Field((int)a);
        }
        return 0;
    }
};
}

static orig::RootContainer* sOrig;
static EeWonRoot* sRust;

static intptr_t RootCall(R call, intptr_t a, intptr_t b, intptr_t c)
{
    if (sRustSide) return ee_won_root_call(sRust, call, a, b, c);
    return sOrig->Call(call, a, b, c);
}

// the root's overrides of the container's virtuals
static R RootOverride(C call)
{
    switch (call) {
    case C::SizeChanged: return R::SizeChanged;
    case C::PrePaint: return R::PrePaint;
    case C::Paint: return R::Paint;
    case C::MouseExit: return R::MouseExit;
    case C::MouseDown: return R::MouseDown;
    case C::MouseUp: return R::MouseUp;
    case C::MouseMove: return R::MouseMove;
    case C::MouseDrag: return R::MouseDrag;
    case C::KeyChar: return R::KeyChar;
    case C::KeyDown: return R::KeyDown;
    case C::KeyUp: return R::KeyUp;
    case C::SetCursor: return R::SetCursor;
    case C::RemoveChild: return R::RemoveChild;
    default: return (R)0;
    }
}

static intptr_t SideCall(int ct, C call, intptr_t a, intptr_t b, intptr_t c)
{
    if (ct == kRoot && (int)RootOverride(call) != 0) return RootCall(RootOverride(call), a, b, c);
    return ee_won_ct_call(sCt[ct], call, a, b, c);
}
static intptr_t CtObjectOf(intptr_t ct) { return (intptr_t)sCt[ct]; }

static bool ExtraRead(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r)
{
    (void)self; (void)d;
    switch (op) {
    case Op::RootCursorComponent: *r = CursorComponent(a, (int*)b, (int*)c); return true;
    case Op::RootOneComponentWindow: *r = sOneComponent; return true;
    case Op::RootWindowMouse: *(int*)a = (int)sWinMouseX; *(int*)b = (int)sWinMouseY; *r = 0; return true;
    case Op::RootWindowRect: {
        EeWonRect4* q = (EeWonRect4*)a;
        q->x = (int)sWinX; q->y = (int)sWinY; q->w = 640; q->h = 480;
        *r = 0;
        return true;
    }
    case Op::RootMouseChildOf: *r = IsCt(a) ? SideCall((int)a, C::GetField, (intptr_t)EeWonCtField::MouseChild, 0, 0) : 0; return true;
    case Op::RootScheme: *r = 0xA100; return true;
    case Op::RootImageSize: ImageSize(a, (int*)b, (int*)c); *r = 0; return true;
    case Op::RootGrabberRead: {
        // traced with its answer: the replay keeps no model of the grabbers
        const Grab& g = sGrab[a - 0x9000];
        EeWonRootGrab* q = (EeWonRootGrab*)b;
        q->need = g.need; q->order = g.order; q->x = g.x; q->y = g.y;
        q->clip.x = g.cx; q->clip.y = g.cy; q->clip.w = g.cw; q->clip.h = g.ch;
        q->image = g.image;
        Line("grab-read %lld %lld %lld %lld %lld %lld", a, g.need, g.order, g.x, g.y, g.image);
        sTrace.back() += " " + std::to_string(g.cx) + " " + std::to_string(g.cy) + " " + std::to_string(g.cw) + " " +
                         std::to_string(g.ch);
        *r = 0;
        return true;
    }
    default: return false;
    }
}

static bool ExtraLine(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)d;
    if (op != Op::RootDrawImage) return false;
    const int* q = (const int*)c;
    Line("op %lld draw %lld %lld %lld %lld %lld", (long long)op, a, b, q[0], q[1], q[2]);
    sTrace.back() += " " + std::to_string(q[3]) + " " + std::to_string(q[4]) + " " + std::to_string(q[5]) + " @" +
                     std::to_string((long long)self);
    return true;
}

static void ExtraOp(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r)
{
    (void)self; (void)d; (void)r;
    switch (op) {
    case Op::ChildSetPos: Set((int)a, 3, b); Set((int)a, 4, c); break;
    case Op::ChildSetVisible:
        if ((b != 0) == !(sW[a].bits & 1)) break;
        Set((int)a, 0, b ? (sW[a].bits & ~1u) : (sW[a].bits | 1));
        Set((int)a, 1, 0);
        if (sW[a].parent != 0) Do(sW[a].parent, C::ChildStateChanged, a);
        VCall(a, V::Invalidate);
        VCall(a, V::NotifyRootVisibility, b);
        break;
    case Op::RootWindowSetPos:
        sWinX = a; sWinY = b;
        Line("env 17 %lld %lld", sWinX, sWinY);
        break;
    case Op::RootGrabberClear: sGrab[a - 0x9000].need = 0; break;
    case Op::RootGrabberDrop: sGrab[a - 0x9000].need = 0; sGrab[a - 0x9000].image = 0; break;
    case Op::RootImageHold: sImageRefs[a]++; break;
    case Op::RootImageRelease: CHECK(sImageRefs[a] > 0); sImageRefs[a]--; break;
    case Op::RootEnsureOnTop:
        // Component::EnsureOnTopOf: the parent's EnsureOnTopOf(sibling, this)
        if (sW[a].parent != 0) Do(sW[a].parent, C::EnsureOnTopOf, b, a);
        break;
    default: break; // the draws: traced only
    }
}

// ---- the driver

static void RootTop(R call, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
{
    Line("root %lld %lld %lld %lld", (long long)call, a, b, c);
    long long r = RootCall(call, a, b, c);
    Line("-> %lld 0", r);
}

static void RootRect(int x, int y, int w, int h)
{
    Line("root-rect %lld %lld %lld %lld", x, y, w, h);
    orig::Rect q = {x, y, w, h};
    long long r = RootCall(R::InvalidateRect, (intptr_t)&q, 0, 0);
    Line("-> %lld 0", r);
}

// a grabber's background grabbed (or its state changed): the driver's, the same on both sides
static void GrabEvent(int id)
{
    Grab& g = sGrab[id];
    g.need = DR(5) != 0;
    g.order = (sOrderCounter > 0 && DR(4) == 0) ? (int)DR((unsigned)sOrderCounter) : sOrderCounter++;
    g.x = (int)DR(200) - 20; g.y = (int)DR(200) - 20;
    g.cx = (int)DR(200) - 20; g.cy = (int)DR(200) - 20; g.cw = (int)DR(150); g.ch = (int)DR(150);
    g.image = DR(8) ? 0xC001 + DR(6) : 0;
    Line("grab %lld %lld %lld %lld", id, g.need, g.order, g.image);
}

static intptr_t AnyCursor() { static const intptr_t k[6] = {0, 0x5000, 0x5001, 0x5002, 0x5003, 0x5004}; return k[DR(6)]; }

static void RandomRootCall()
{
    int x = (int)DR(260) - 30, y = (int)DR(260) - 30;
    switch (DR(24)) {
    case 0: RootTop(R::SizeChanged); break;
    case 1: RootTop(R::PrePaint, kGraphics); break;
    case 2: RootTop(R::Paint, kGraphics); break;
    case 3: RootTop(R::MouseExit); break;
    case 4: case 5: {
        // half of the presses on one of the root's children
        intptr_t n = SideCall(kRoot, C::GetField, (intptr_t)EeWonCtField::NumChildren, 0, 0);
        if (n > 0 && DR(2)) {
            int k = (int)DR((unsigned)n);
            intptr_t ch = SideCall(kRoot, C::GetField, (intptr_t)EeWonCtField::FrontChild, 0, 0);
            while (k-- > 0) ch = SideCall(kRoot, C::NextChild, ch, 1, 0);
            x = sW[ch].x + (int)DR((unsigned)sW[ch].w);
            y = sW[ch].y + (int)DR((unsigned)sW[ch].h);
        }
        RootTop(R::MouseDown, x, y, DR(4) ? 0 : 1);
        break;
    }
    case 6: RootTop(R::MouseUp, x, y, DR(3) ? 0 : 2); break;
    case 7: case 8: RootTop(R::MouseMove, x, y); break;
    case 9: case 10: RootTop(R::MouseDrag, x, y); break;
    case 11: { static const R kKeys[3] = {R::KeyChar, R::KeyDown, R::KeyUp}; RootTop(kKeys[DR(3)], 9 + DR(60)); break; }
    case 12: RootRect((int)DR(200) - 20, (int)DR(200) - 20, (int)DR(100), (int)DR(100)); break;
    case 13: case 14: RootTop(R::SetCursor, AnyCursor()); break;
    case 15: {
        intptr_t c = sW[11].parent == kRoot && DR(2) ? 11 : 2 + DR(kN - 1);
        if (sW[c].parent == kRoot) RootTop(R::RemoveChild, c);
        break;
    }
    case 16: case 17: {
        int id = 1 + (int)DR(kN);
        if (DR(2)) GrabEvent(id);
        RootTop(R::AddBG, DR(10) ? 0x9000 + id : 0);
        break;
    }
    case 18: RootTop(R::GetField, 1 + DR(10)); break;
    case 19: TopCall(kRoot, C::AddChild, 5 + DR(kN - 4), 0, 0); break;
    case 20: GrabEvent(1 + (int)DR(kN)); break;
    case 21: TopCall(2 + (int)DR(kCt - 1), C::MouseMove, x, y, 0); break;
    case 22: TopCall(kRoot, C::MouseEnter, x, y, 0); break;
    default:
        switch (DR(3)) {
        case 0: sOneComponent = DR(3) == 0; Line("env 14 %lld", sOneComponent); break;
        case 1: sWinMouseX = (int)DR(300) - 30; sWinMouseY = (int)DR(300) - 30; Line("env 15 %lld %lld", sWinMouseX, sWinMouseY); break;
        default: sWinX = (int)DR(400) - 50; sWinY = (int)DR(400) - 50; Line("env 17 %lld %lld", sWinX, sWinY); break;
        }
        break;
    }
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sCursorRefs.clear();
    sImageRefs.clear();
    sRustSide = inRust;
    sDriverSeed = inSeq * 40503u + 29u;
    sAnswerSeed = inSeq * 2654435761u + 11u;
    InitWorld();
    // drag handles: a third of the components
    for (int id = 2; id <= kN; id++)
        if (DR(3) == 0) Set(id, 0, sW[id].bits | 0x20);
    memset(sGrab, 0, sizeof(sGrab));
    sOrderCounter = 0;
    sOneComponent = DR(3) == 0; sWinMouseX = (int)DR(300) - 30; sWinMouseY = (int)DR(300) - 30;
    sWinX = (int)DR(400) - 50; sWinY = (int)DR(400) - 50;
    Line("env 14 %lld %lld %lld %lld %lld", sOneComponent, sWinMouseX, sWinMouseY, sWinX, sWinY);
    for (int ct = 1; ct <= kCt; ct++) {
        Line("new %lld", ct);
        sCt[ct] = ee_won_ct_new((void*)(intptr_t)ct, Host, false);
    }
    Line("root-new");
    if (inRust) sRust = ee_won_root_new((void*)kRoot, Host, sCt[kRoot]);
    else sOrig = new orig::RootContainer;
    // the containers under the root (or under 2), the leaves anywhere
    for (int id = 2; id <= kN; id++) {
        if (DR(5) == 0) continue;
        int p = id <= kCt ? (id == 2 || DR(2) ? 1 : 2) : 1 + (int)DR(kCt);
        TopCall(p, C::AddChild, id, 0, 0);
    }
    Env(0, kRoot);
    for (int i = 0; i < inCalls; i++) {
        if (DR(3) == 0) RandomChange();
        RandomRootCall();
    }
    Line("free-root");
    if (inRust) { ee_won_root_free(sRust); sRust = NULL; }
    else { sOrig->Destroy(); delete sOrig; sOrig = NULL; }
    for (int ct = kCt; ct >= 1; ct--) {
        Line("free %lld", ct);
        ee_won_ct_free(sCt[ct]);
        sCt[ct] = NULL;
    }
    for (int id = 1; id <= kN; id++) CHECK(sW[id].refs == 0);
    for (std::map<intptr_t, int>::iterator i = sCursorRefs.begin(); i != sCursorRefs.end(); ++i) CHECK(i->second == 0);
    for (std::map<intptr_t, int>::iterator i = sImageRefs.begin(); i != sImageRefs.end(); ++i) CHECK(i->second == 0);
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
                    if (strcmp(x, y) != 0) {
                        printf("  line %u\n", (unsigned)i);
                        for (size_t j = i > 6 ? i - 6 : 0; j < i; j++) printf("  both: %s\n", a[j].c_str());
                        printf("  orig: %s\n  rust: %s\n", x, y);
                        break;
                    }
                }
            }
        }
        bool aNew = seq < 40;
        for (std::set<int>::const_iterator i = sOpsMet.begin(); i != sOpsMet.end(); ++i)
            if (aSeen.insert("op" + std::to_string(*i)).second) aNew = true;
        for (size_t i = 0; i < a.size(); i++)
            if (a[i].compare(0, 5, "root ") == 0) {
                std::string key = a[i].substr(0, a[i].find(' ', 5));
                if (aSeen.insert(key).second) aNew = true;
            }
        if (aPrint && aNew) {
            printf("seq %d\n", seq);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (!aPrint)
        printf("won_rootobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
