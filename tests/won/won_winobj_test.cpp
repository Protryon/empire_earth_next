// Window as a whole object (R6 WON round 32): the original Window.cpp's statements (the input dispatch, the cursor,
// the resize, closing, centring, the child windows, the base PlatformClose and ComponentApiCompletion's
// CheckDoubleClick), copied from the C++ before the port with every call out (the root container's methods, the
// platform virtuals, GetRect / SetPosSize, the manager's screen rectangle and AddWindow, WONTickCount, the new
// offscreen image, the close event, the cursors, the other windows) replaced by the world below, against ee_won's
// Rust windows (winobj.rs) over the same world through the host ops.
//
// The world models the platform window as logged host ops: four windows (ids 1..4; a parent's id is always smaller,
// so the child sets have no cycle), each with a root (its bounds, current cursor, timer flags and CanClose answer), a
// platform rectangle (SetPosSize writes it, GetRect reads it; the client rectangle is inset), a visible flag
// (PlatformShow writes it) and an allow-close answer; the screen rectangle; a clock the driver advances before each
// call. The root's cursor changes on MouseEnter, MouseMove, MouseDrag and MouseExit from the arguments. Every op is
// traced with its arguments (rectangle pointers as 0) and its answer; a nested op (a child's CanClose, the parent's
// child set) runs on the side's own windows between its op line and its answer. After each call every window's
// fields are traced.
//
// Each of 20,000 sequences makes 40 random calls on the windows (every Window method, the accessors' setters, world
// changes). The traces must be the same. The first 30 sequences and every later one that makes an op or a call none
// before made are rust/crates/ee_won/src/golden_winobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <set>
#include "rust/ee_won.h"

typedef EeWonGuiOp Op;
typedef uint32_t DWORD;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

enum { kN = 4 };

// ---- the world
struct Rect { int x, y, w, h; };
struct WinWorld {
    int rootL, rootT, rootW, rootH;
    intptr_t rootCursor;
    int timerFlags;
    bool rootCanClose;
    bool visible;
    Rect rect;
    bool allowClose;
};
static WinWorld sW[kN + 1];
static Rect sScreen;
static uint32_t sClock;
static std::vector<std::string>* sTrace;
static std::set<std::string> sMet;

static void Line(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void Line(const char* fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    sTrace->push_back(buf);
}

static intptr_t CursorFrom(intptr_t b, intptr_t c)
{
    int v = (int)((b * 7 + c * 3) % 9);
    if (v < 0) v = -v;
    return v == 0 ? 0 : 0x5000 + v;
}
static intptr_t Native(intptr_t c) { return (c & 3) == 0 ? 0 : c + 0x100; }

// the side's nested calls
static intptr_t SideCanClose(bool rust, int w);
static void SideChildSet(bool rust, int parent, int child, bool insert);

static intptr_t World(bool rust, int win, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    bool ptrA = op == Op::WinScreenRect, ptrB = op == Op::WinGetRect;
    Line("op %d %d %ld %ld %ld %ld", win, (int)op, ptrA ? 0L : (long)a, ptrB ? 0L : (long)b, (long)c, (long)d);
    char key[32];
    snprintf(key, sizeof key, "op %d %ld", (int)op, (op == Op::WinRoot || op == Op::WinPlat) ? (long)a : 0L);
    sMet.insert(key);
    WinWorld& w = sW[win];
    intptr_t r = 0;
    Rect out = {0, 0, 0, 0};
    bool hasOut = false;
    switch (op) {
    case Op::WinTick: r = (intptr_t)sClock; break;
    case Op::WinRoot:
        switch ((EeWonWinRoot)a) {
        case EeWonWinRoot::SetSize: w.rootW = (int)b; w.rootH = (int)c; break;
        case EeWonWinRoot::MouseEnter: case EeWonWinRoot::MouseMove: case EeWonWinRoot::MouseDrag:
            w.rootCursor = CursorFrom(b, c); break;
        case EeWonWinRoot::MouseExit: w.rootCursor = 0x5003; break;
        case EeWonWinRoot::CanClose: r = w.rootCanClose ? 1 : 0; break;
        case EeWonWinRoot::CurrentCursor: r = w.rootCursor; break;
        case EeWonWinRoot::Left: r = w.rootL; break;
        case EeWonWinRoot::Top: r = w.rootT; break;
        case EeWonWinRoot::Width: r = w.rootW; break;
        case EeWonWinRoot::Height: r = w.rootH; break;
        case EeWonWinRoot::TimerFlags: r = w.timerFlags; break;
        default: break;
        }
        break;
    case Op::WinPlat:
        if ((EeWonWinPlat)a == EeWonWinPlat::Show) w.visible = b != 0;
        break;
    case Op::WinSetPosSize: w.rect.x = (int)a; w.rect.y = (int)b; w.rect.w = (int)c; w.rect.h = (int)d; break;
    case Op::WinGetRect: {
        const Rect& s = sW[a].rect;
        out = s;
        if (c) { out.x += 4; out.y += 20; out.w -= 8; out.h -= 24; }
        *(Rect*)b = out;
        hasOut = true;
        break;
    }
    case Op::WinIsVisible: r = sW[a].visible ? 1 : 0; break;
    case Op::WinScreenRect: out = sScreen; *(Rect*)a = out; hasOut = true; break;
    case Op::WinCanClose: r = SideCanClose(rust, (int)a); break;
    case Op::WinAllowClose: r = w.allowClose ? 1 : 0; break;
    case Op::WinChildSet: SideChildSet(rust, (int)a, win, b != 0); break;
    case Op::WinStdArrow: r = 0x5001; break;
    case Op::WinCursorNative: r = Native(a); break;
    default: break;
    }
    if (hasOut) Line("= %ld %d %d %d %d", (long)r, out.x, out.y, out.w, out.h);
    else Line("= %ld", (long)r);
    return r;
}

// ---- the original (Window.cpp before the port), over the world
enum { WindowState_Open, WindowState_Closing, WindowState_Closed };
enum { InputEvent_SizeChange = 1, InputEvent_MouseMove, InputEvent_MouseDown, InputEvent_MouseDoubleClick,
       InputEvent_MouseDownCheckDoubleClick, InputEvent_MouseUp, InputEvent_MouseWheel, InputEvent_KeyDown,
       InputEvent_KeyUp, InputEvent_KeyChar, InputEvent_GotFocus, InputEvent_LostFocus, InputEvent_MouseExit };

struct OrigWindow {
    int id;
    int mWindowState;
    int mMouseX, mMouseY, mKeyMod;
    bool mMouseIsDown[3];
    bool mMouseIsInClientArea;
    intptr_t mCurrentCursor;
    bool mNativeCursorShowing;
    DWORD mButtonDownTick[3];
    bool mButtonDoubleClick[3];
    bool mDetectDoubleClick;
    int mDoubleClickX, mDoubleClickY;
    DWORD mLastMoveTick;
    int mMinWidth, mMinHeight, mMaxWidth, mMaxHeight;
    bool mNeedResize;
    int mNewWidth, mNewHeight;
    DWORD mResizeDelay, mLastResizeTick, mLastCheckCursorTick;
    int mSuspendRootResize;
    int mStyle;
    bool mIsPopup, mIsOneComponentWindow, mMinimizeOnResolutionRestore;
    OrigWindow* mParent;
    std::set<OrigWindow*> mChildWindowSet;
    bool mIsShowing;

    intptr_t W(Op op, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0) { return World(false, id, op, a, b, c, d); }
    intptr_t R(EeWonWinRoot call, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0) { return W(Op::WinRoot, (intptr_t)call, b, c, d); }
    void P(EeWonWinPlat call, intptr_t b = 0) { W(Op::WinPlat, (intptr_t)call, b); }
    DWORD Tick() { return (DWORD)W(Op::WinTick); }
    Rect GetRect(OrigWindow* win, bool client) { Rect r; W(Op::WinGetRect, win->id, (intptr_t)&r, client ? 1 : 0); return r; }

    // Window::Window, the state (the original left the ticks and double-click state unset: 0 here and in Rust)
    void Construct(int theId)
    {
        this->id = theId;
        this->mParent = NULL;
        this->mChildWindowSet.clear();
        this->mCurrentCursor = 0;
        this->mMouseIsDown[0] = this->mMouseIsDown[1] = this->mMouseIsDown[2] = false;
        this->mMouseIsInClientArea = false;
        this->mNativeCursorShowing = true;
        this->mDetectDoubleClick = false;
        for (int i = 0; i < 3; i++) { this->mButtonDownTick[i] = 0; this->mButtonDoubleClick[i] = false; }
        this->mDoubleClickX = this->mDoubleClickY = 0;
        this->mMouseX = this->mMouseY = this->mKeyMod = 0;
        this->mLastMoveTick = 0;
        this->mLastCheckCursorTick = 0;
        this->mMinWidth = 0;
        this->mMinHeight = 0;
        this->mMaxWidth = 64000;
        this->mMaxHeight = 64000;
        this->mNeedResize = false;
        this->mNewWidth = this->mNewHeight = 0;
        this->mResizeDelay = 0;
        this->mLastResizeTick = 0;
        this->mStyle = 0;
        this->mWindowState = WindowState_Closed;
        this->mIsShowing = false;
        this->mIsPopup = false;
        this->mIsOneComponentWindow = false;
        this->mSuspendRootResize = 0;
        this->mMinimizeOnResolutionRestore = false;
    }

    bool CanClose()
    {
        bool childrenCanClose = true;
        for (std::set<OrigWindow*>::iterator anItr = this->mChildWindowSet.begin(); anItr != this->mChildWindowSet.end(); ++anItr)
        {
            OrigWindow* aChild = *anItr;
            if (!W(Op::WinCanClose, aChild->id))
                childrenCanClose = false;
        }
        if (!childrenCanClose)
            return false;
        if (!R(EeWonWinRoot::CanClose))
            return false;
        if (!W(Op::WinAllowClose))
            return false;
        return true;
    }

    bool Close()
    {
        if (ee_won_gui_win_close_step(this->mWindowState) == 0)
            return true;
        if (!this->CanClose())
            return false;
        this->mWindowState = WindowState_Closing;
        W(Op::WinQueueClose);
        return true;
    }

    void CheckResize(DWORD theTick)
    {
        if (!ee_won_gui_win_resize_due(this->mNeedResize, theTick, this->mLastResizeTick, this->mResizeDelay,
                                      this->mSuspendRootResize))
            return;
        W(Op::WinNewOffscreen, this->mNewWidth, this->mNewHeight);
        R(EeWonWinRoot::SetSize, this->mNewWidth, this->mNewHeight);
        R(EeWonWinRoot::Invalidate);
        this->mNeedResize = false;
        this->mLastResizeTick = Tick();
    }

    void SetTitle() { if (this->mWindowState == WindowState_Open) P(EeWonWinPlat::SetTitle); }
    void SetIcon() { if (this->mWindowState == WindowState_Open) P(EeWonWinPlat::SetIcon); }

    void Center(bool inParent)
    {
        Rect aScreenRect, aWindowRect;
        if (!ee_won_gui_win_center_on_parent(inParent, this->mParent != NULL,
                                            inParent && this->mParent != NULL && W(Op::WinIsVisible, this->mParent->id)))
            W(Op::WinScreenRect, (intptr_t)&aScreenRect);
        else
            aScreenRect = GetRect(this->mParent, false);
        aWindowRect = GetRect(this, false);
        int top;
        int left = ee_won_gui_win_center(aScreenRect.x, aScreenRect.y, aScreenRect.w, aScreenRect.h,
                                         aWindowRect.w, aWindowRect.h, &top);
        W(Op::WinSetPosSize, left, top, aWindowRect.w, aWindowRect.h);
    }

    void SetClientSize(int width, int height)
    {
        Rect aRect = GetRect(this, false);
        W(Op::WinSetPosSize, aRect.x, aRect.y, width, height);
    }

    void Show(bool show)
    {
        this->mIsShowing = show;
        if (!show)
        {
            if (this->mMouseIsInClientArea || this->mMouseIsDown[0])
                this->HandleMouseExit();
        }
        P(EeWonWinPlat::Show, show ? 1 : 0);
    }

    void HandleMouseExit()
    {
        this->ShowNativeCursor(true);
        this->mMouseIsInClientArea = false;
        int l = (int)R(EeWonWinRoot::Left), t = (int)R(EeWonWinRoot::Top);
        int w = (int)R(EeWonWinRoot::Width), h = (int)R(EeWonWinRoot::Height);
        this->mMouseX = ee_won_gui_win_exit_point(l, t, w, h, &this->mMouseY);
        this->mMouseIsDown[0] = this->mMouseIsDown[1] = this->mMouseIsDown[2] = false;
        R(EeWonWinRoot::MouseExit);
        this->mCurrentCursor = 0;
    }

    void HandleKeyEvent(const EeWonWinEvent* theEvent)
    {
        this->mKeyMod = theEvent->key_mod;
        switch (theEvent->kind)
        {
        case InputEvent_KeyDown: R(EeWonWinRoot::KeyDown, theEvent->key); break;
        case InputEvent_KeyUp: R(EeWonWinRoot::KeyUp, theEvent->key); break;
        case InputEvent_KeyChar: R(EeWonWinRoot::KeyChar, theEvent->key); break;
        }
    }

    void HandleMouseDown(const EeWonWinEvent* theEvent)
    {
        int aButton = theEvent->button;
        this->mMouseIsDown[aButton] = true;
        DWORD aTick = theEvent->kind == InputEvent_MouseDownCheckDoubleClick ? Tick() : 0;
        bool aStart, aSet;
        uint32_t aNewTick;
        bool isDoubleClick = ee_won_gui_win_double_click(theEvent->kind, this->mDetectDoubleClick,
                                                         this->mButtonDownTick[aButton], aTick, &aStart, &aSet, &aNewTick);
        if (aStart)
        {
            for (int i = 0; i < 3; i++)
                this->mButtonDownTick[i] = 0;
            this->mDetectDoubleClick = true;
            this->mDoubleClickX = theEvent->x;
            this->mDoubleClickY = theEvent->y;
        }
        if (aSet)
            this->mButtonDownTick[aButton] = aNewTick;
        this->mButtonDoubleClick[aButton] = isDoubleClick;
        R(EeWonWinRoot::MouseDown, this->mMouseX, this->mMouseY, aButton);
    }

    void HandleMouseEvent(const EeWonWinEvent* theEvent)
    {
        this->mMouseX = theEvent->x;
        this->mMouseY = theEvent->y;
        this->mKeyMod = theEvent->key_mod;
        if (ee_won_gui_win_mouse_enters(this->mMouseIsInClientArea, this->mMouseIsDown[0]))
        {
            this->mMouseIsInClientArea = true;
            R(EeWonWinRoot::MouseEnter, this->mMouseX, this->mMouseY);
            this->CheckSetCursor(true);
        }
        switch (theEvent->kind)
        {
        case InputEvent_MouseMove:
            this->mLastMoveTick = Tick();
            if (ee_won_gui_win_move_ends_double(this->mDetectDoubleClick, this->mMouseX, this->mMouseY,
                                               this->mDoubleClickX, this->mDoubleClickY))
                this->mDetectDoubleClick = false;
            if (this->mMouseIsDown[0])
                R(EeWonWinRoot::MouseDrag, this->mMouseX, this->mMouseY);
            else
                R(EeWonWinRoot::MouseMove, this->mMouseX, this->mMouseY);
            break;
        case InputEvent_MouseDown:
        case InputEvent_MouseDoubleClick:
        case InputEvent_MouseDownCheckDoubleClick:
            this->HandleMouseDown(theEvent);
            break;
        case InputEvent_MouseUp:
        {
            this->mLastMoveTick = 0;
            int aButton = theEvent->button;
            this->mMouseIsDown[aButton] = false;
            if (ee_won_gui_win_release_exits(aButton, this->mMouseIsInClientArea))
            {
                this->HandleMouseExit();
                break;
            }
            R(EeWonWinRoot::MouseUp, this->mMouseX, this->mMouseY, aButton);
        }
        break;
        case InputEvent_MouseWheel:
            R(EeWonWinRoot::MouseWheel, theEvent->button);
            break;
        }
    }

    void HandleInputEvent(const EeWonWinEvent* theEvent)
    {
        switch (theEvent->kind)
        {
        case InputEvent_SizeChange:
        {
            int rw = (int)R(EeWonWinRoot::Width), rh = (int)R(EeWonWinRoot::Height);
            if (ee_won_gui_win_size_resizes(theEvent->width, theEvent->height, rw, rh))
            {
                this->mNeedResize = true;
                this->mNewWidth = theEvent->width;
                this->mNewHeight = theEvent->height;
                this->CheckResize(Tick());
            }
        }
        break;
        case InputEvent_GotFocus: R(EeWonWinRoot::GotFocus); break;
        case InputEvent_LostFocus: R(EeWonWinRoot::LostFocus); break;
        case InputEvent_MouseExit:
            this->mMouseIsInClientArea = false;
            if (!this->mMouseIsDown[0])
                this->HandleMouseExit();
            break;
        }
    }

    bool IdleUpdate()
    {
        if (this->mWindowState == WindowState_Open)
        {
            bool force = false;
            if (this->mMouseIsInClientArea)
            {
                DWORD aTick = Tick();
                if (ee_won_gui_win_cursor_forced(true, aTick, this->mLastCheckCursorTick))
                {
                    force = true;
                    this->mLastCheckCursorTick = aTick;
                }
            }
            this->CheckSetCursor(force);
            return true;
        }
        else
            return false;
    }

    void TimerEvent(DWORD theElapsed)
    {
        if (this->mMouseIsInClientArea && this->mLastMoveTick != 0 && !this->mMouseIsDown[0] && !this->mMouseIsDown[1] &&
            !this->mMouseIsDown[2])
        {
            uint32_t anElapsed;
            if (ee_won_gui_win_hover(true, this->mLastMoveTick, false, false, false, Tick(), &anElapsed))
                R(EeWonWinRoot::MouseHover, anElapsed);
        }
        this->CheckResize(Tick());
        if (R(EeWonWinRoot::TimerFlags) != 0)
            R(EeWonWinRoot::TimerEvent, theElapsed);
    }

    void SetParent(OrigWindow* theParent) { this->mParent = theParent; }

    void Create(int width, int height)
    {
        this->mWindowState = WindowState_Open;
        if (this->mParent != NULL)
            W(Op::WinChildSet, this->mParent->id, 1);
        R(EeWonWinRoot::SetSize, width, height);
        W(Op::WinAddToManager);
    }

    void PlatformClose()
    {
        this->ShowNativeCursor(true);
        this->mWindowState = WindowState_Closed;
        this->mIsShowing = false;
        if (this->mParent != NULL)
            W(Op::WinChildSet, this->mParent->id, 0);
    }

    void ShowNativeCursor(bool show)
    {
        this->mNativeCursorShowing = show;
        P(EeWonWinPlat::ShowNativeCursor, show ? 1 : 0);
    }

    void AddComponent(intptr_t c) { R(EeWonWinRoot::AddChild, c); }
    void SetModal(bool modal) { if (modal) R(EeWonWinRoot::EnterModal); else R(EeWonWinRoot::LeaveModal); }

    void CheckSetCursor(bool force)
    {
        if (!ee_won_gui_win_cursor_refresh(force, R(EeWonWinRoot::CurrentCursor) == this->mCurrentCursor))
            return;
        this->mCurrentCursor = R(EeWonWinRoot::CurrentCursor);
        intptr_t cursor = R(EeWonWinRoot::CurrentCursor);
        if (cursor == 0)
            cursor = W(Op::WinStdArrow);
        cursor = W(Op::WinCursorNative, cursor);
        if (cursor != 0) {
            this->ShowNativeCursor(true);
            P(EeWonWinPlat::SetCursor, cursor);
        } else {
            this->ShowNativeCursor(false);
        }
    }

    void InvalidateRoot() { R(EeWonWinRoot::Invalidate); R(EeWonWinRoot::UnionBounds); }
    void Minimize(bool minimize) { P(EeWonWinPlat::Minimize, minimize ? 1 : 0); }

    void SuspendRootResize(bool suspend)
    {
        if (suspend)
            this->mSuspendRootResize++;
        else
        {
            this->mSuspendRootResize--;
            if (this->mSuspendRootResize <= 0)
            {
                this->mSuspendRootResize = 0;
                this->CheckResize(Tick());
            }
        }
    }

    void RemoveComponent(intptr_t c) { R(EeWonWinRoot::RemoveChild, c); }
    bool IsMouseDown(int b) { return this->mMouseIsDown[b]; }
    bool CheckDoubleClick(int b) { return this->mButtonDoubleClick[b]; }

    void Dump()
    {
        Line("state %d %d %d %d %d %d %d %d %d %ld %d %u %u %u %d %d %d %d %d %d %u %d %d %d %d %d %d %d %u %u %u %d %d "
             "%d %d %d %d %d %u",
             id, mWindowState, mMouseX, mMouseY, mKeyMod, mMouseIsDown[0], mMouseIsDown[1], mMouseIsDown[2],
             mMouseIsInClientArea, (long)mCurrentCursor, mNativeCursorShowing, mButtonDownTick[0], mButtonDownTick[1],
             mButtonDownTick[2], mButtonDoubleClick[0], mButtonDoubleClick[1], mButtonDoubleClick[2],
             mDetectDoubleClick, mDoubleClickX, mDoubleClickY, mLastMoveTick, mMinWidth, mMinHeight, mMaxWidth,
             mMaxHeight, mNeedResize, mNewWidth, mNewHeight, mResizeDelay, mLastResizeTick, mLastCheckCursorTick,
             mSuspendRootResize, mStyle, mIsPopup, mIsOneComponentWindow, mMinimizeOnResolutionRestore,
             mParent ? mParent->id : 0, mIsShowing, (unsigned)mChildWindowSet.size());
    }
};

static OrigWindow sOrig[kN + 1]; // in an array: their addresses are in id order, as the Rust child sets' keys are
static EeWonWindow* sRust[kN + 1];

static intptr_t RustHost(void* inWin, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    return World(true, (int)(intptr_t)inWin, op, a, b, c, d);
}

static intptr_t SideCanClose(bool rust, int w)
{
    if (rust) return ee_won_win_call(sRust[w], EeWonWinCall::CanClose, 0, 0);
    return sOrig[w].CanClose() ? 1 : 0;
}

static void SideChildSet(bool rust, int parent, int child, bool insert)
{
    if (rust) { ee_won_win_call(sRust[parent], EeWonWinCall::ChildSet, child, insert ? 1 : 0); return; }
    if (insert) sOrig[parent].mChildWindowSet.insert(&sOrig[child]);
    else sOrig[parent].mChildWindowSet.erase(&sOrig[child]);
}

static void DumpRust(int w)
{
    intptr_t f[EE_WON_WIN_FIELD_COUNT + 1];
    for (int i = 1; i <= EE_WON_WIN_FIELD_COUNT; i++) f[i] = ee_won_win_call(sRust[w], EeWonWinCall::GetField, i, 0);
    Line("state %d %d %d %d %d %d %d %d %d %ld %d %u %u %u %d %d %d %d %d %d %u %d %d %d %d %d %d %d %u %u %u %d %d "
         "%d %d %d %d %d %u",
         w, (int)f[1], (int)f[2], (int)f[3], (int)f[4], (int)f[5], (int)f[6], (int)f[7], (int)f[8], (long)f[9], (int)f[10],
         (unsigned)f[11], (unsigned)f[12], (unsigned)f[13], (int)f[14], (int)f[15], (int)f[16], (int)f[17], (int)f[18],
         (int)f[19], (unsigned)f[20], (int)f[21], (int)f[22], (int)f[23], (int)f[24], (int)f[25], (int)f[26], (int)f[27],
         (unsigned)f[28], (unsigned)f[29], (unsigned)f[30], (int)f[31], (int)f[32], (int)f[33], (int)f[34], (int)f[35],
         (int)f[36], (int)f[37], (unsigned)f[38]);
}

// ---- the driver
struct Rng {
    uint32_t s;
    uint32_t Next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    int Int(int n) { return (int)(Next() % (uint32_t)n); }
    bool Bool() { return (Next() & 1) != 0; }
};

enum Call {
    kCanClose, kClose, kCheckResize, kSetTitle, kSetIcon, kCenter, kSetClientSize, kShow, kMouseExit, kKey,
    kMouseDown, kMouse, kInput, kIdle, kTimer, kCreate, kPlatformClose, kShowNative, kCheckSetCursor,
    kInvalidateRoot, kMinimize, kSuspend, kRemoveComponent, kIsMouseDown, kAddComponent, kSetModal, kCheckDouble,
    kSetParent, kSetShowing, kSetPopup, kSetOne, kSetStyle, kWorld, kCallCount
};

static EeWonWinEvent RandomEvent(Rng& g, int call, int win)
{
    EeWonWinEvent e = {};
    switch (call) {
    case kKey: e.kind = g.Int(4) == 0 ? 1 + g.Int(13) : 8 + g.Int(3); e.key = g.Int(300); e.key_mod = g.Int(8); break;
    case kMouseDown: e.kind = 3 + g.Int(3); e.button = g.Int(3); break;
    case kMouse: e.kind = g.Int(5) == 0 ? 1 + g.Int(13) : 2 + g.Int(6); e.key_mod = g.Int(8);
        e.button = (e.kind == 7) ? g.Int(9) - 4 : g.Int(3); break;
    default: {
        static const int kinds[] = {1, 1, 1, 11, 12, 13, 13, 2};
        e.kind = kinds[g.Int(8)];
        int pick = g.Int(4);
        e.width = pick == 0 ? 0 : pick == 1 ? sW[win].rootW : 100 + g.Int(50);
        e.height = pick == 0 ? g.Int(2) * 120 : pick == 1 ? sW[win].rootH : 100 + g.Int(50);
        break;
    }
    }
    // the place: near the last double-click place often, so the move's test runs both ways
    e.x = g.Int(3) == 0 ? 10 : g.Int(200) - 20;
    e.y = g.Int(3) == 0 ? 12 : g.Int(200) - 20;
    return e;
}

static intptr_t DoCall(bool rust, int w, int call, intptr_t a, intptr_t b, const EeWonWinEvent* e)
{
    OrigWindow& o = sOrig[w];
    EeWonWindow* r = sRust[w];
    typedef EeWonWinCall C;
#define RC(c, x, y) ee_won_win_call(r, c, (intptr_t)(x), (intptr_t)(y))
    switch (call) {
    case kCanClose: return rust ? RC(C::CanClose, 0, 0) : o.CanClose();
    case kClose: return rust ? RC(C::Close, 0, 0) : o.Close();
    case kCheckResize: if (rust) RC(C::CheckResize, (uint32_t)a, 0); else o.CheckResize((DWORD)a); return 0;
    case kSetTitle: if (rust) RC(C::TitleChanged, 0, 0); else o.SetTitle(); return 0;
    case kSetIcon: if (rust) RC(C::IconChanged, 0, 0); else o.SetIcon(); return 0;
    case kCenter: if (rust) RC(C::Center, a, 0); else o.Center(a != 0); return 0;
    case kSetClientSize: if (rust) RC(C::SetClientSize, a, b); else o.SetClientSize((int)a, (int)b); return 0;
    case kShow: if (rust) RC(C::Show, a, 0); else o.Show(a != 0); return 0;
    case kMouseExit: if (rust) RC(C::HandleMouseExit, 0, 0); else o.HandleMouseExit(); return 0;
    case kKey: if (rust) RC(C::HandleKeyEvent, e, 0); else o.HandleKeyEvent(e); return 0;
    case kMouseDown: if (rust) RC(C::HandleMouseDown, e, 0); else o.HandleMouseDown(e); return 0;
    case kMouse: if (rust) RC(C::HandleMouseEvent, e, 0); else o.HandleMouseEvent(e); return 0;
    case kInput: if (rust) RC(C::HandleInputEvent, e, 0); else o.HandleInputEvent(e); return 0;
    case kIdle: return rust ? RC(C::IdleUpdate, 0, 0) : o.IdleUpdate();
    case kTimer: if (rust) RC(C::TimerEvent, (uint32_t)a, 0); else o.TimerEvent((DWORD)a); return 0;
    case kCreate: if (rust) RC(C::Create, a, b); else o.Create((int)a, (int)b); return 0;
    case kPlatformClose: if (rust) RC(C::PlatformClose, 0, 0); else o.PlatformClose(); return 0;
    case kShowNative: if (rust) RC(C::ShowNativeCursor, a, 0); else o.ShowNativeCursor(a != 0); return 0;
    case kCheckSetCursor: if (rust) RC(C::CheckSetCursor, a, 0); else o.CheckSetCursor(a != 0); return 0;
    case kInvalidateRoot: if (rust) RC(C::InvalidateRoot, 0, 0); else o.InvalidateRoot(); return 0;
    case kMinimize: if (rust) RC(C::Minimize, a, 0); else o.Minimize(a != 0); return 0;
    case kSuspend: if (rust) RC(C::SuspendRootResize, a, 0); else o.SuspendRootResize(a != 0); return 0;
    case kRemoveComponent: if (rust) RC(C::RemoveComponent, a, 0); else o.RemoveComponent(a); return 0;
    case kIsMouseDown: return rust ? RC(C::IsMouseDown, a, 0) : o.IsMouseDown((int)a);
    case kAddComponent: if (rust) RC(C::AddComponent, a, 0); else o.AddComponent(a); return 0;
    case kSetModal: if (rust) RC(C::SetModal, a, 0); else o.SetModal(a != 0); return 0;
    case kCheckDouble: return rust ? RC(C::CheckDoubleClick, a, 0) : o.CheckDoubleClick((int)a);
    case kSetParent: if (rust) RC(C::SetParent, a, 0); else o.SetParent(a ? &sOrig[a] : NULL); return 0;
    case kSetShowing: if (rust) RC(C::SetField, EeWonWinField::IsShowing, a); else o.mIsShowing = a != 0; return 0;
    case kSetPopup: if (rust) RC(C::SetField, EeWonWinField::IsPopup, a); else o.mIsPopup = a != 0; return 0;
    case kSetOne: if (rust) RC(C::SetField, EeWonWinField::OneComponent, a); else o.mIsOneComponentWindow = a != 0; return 0;
    case kSetStyle: if (rust) RC(C::SetField, EeWonWinField::Style, a); else o.mStyle = (int)a; return 0;
    }
#undef RC
    return 0;
}

static std::vector<std::string> RunSide(bool rust, unsigned seed, int calls)
{
    std::vector<std::string> trace;
    sTrace = &trace;
    Rng g = {seed * 2654435761u + 12345u};
    if (g.s == 0) g.s = 1;
    sClock = g.Int(4) == 0 ? 0xFFFFFF00u : 1000;
    sScreen.x = g.Int(20); sScreen.y = g.Int(20); sScreen.w = 640 + g.Int(400); sScreen.h = 480 + g.Int(300);
    for (int w = 1; w <= kN; w++) {
        WinWorld& x = sW[w];
        x.rootL = g.Int(5); x.rootT = g.Int(5); x.rootW = 100 + g.Int(50); x.rootH = 100 + g.Int(50);
        x.rootCursor = g.Int(3) == 0 ? 0 : 0x5000 + g.Int(9); x.timerFlags = g.Int(2); x.rootCanClose = g.Int(4) != 0;
        x.visible = g.Bool(); x.rect.x = g.Int(300); x.rect.y = g.Int(300); x.rect.w = 50 + g.Int(400); x.rect.h = 50 + g.Int(400);
        x.allowClose = g.Int(4) != 0;
        if (rust) sRust[w] = ee_won_win_new((void*)(intptr_t)w, RustHost);
        else sOrig[w].Construct(w);
        // a few windows start open, under a smaller id, as Create left them
    }
    for (int i = 0; i < calls; i++) {
        sClock += g.Int(5) == 0 ? 0 : (uint32_t)g.Int(1500);
        int w = 1 + g.Int(kN);
        int call = g.Int(kCallCount);
        intptr_t a = 0, b = 0;
        EeWonWinEvent e = {};
        switch (call) {
        case kCheckResize: a = (intptr_t)(sClock - (uint32_t)g.Int(100)); break;
        case kCenter: case kShow: case kShowNative: case kCheckSetCursor: case kMinimize: case kSetModal:
        case kSetShowing: case kSetPopup: case kSetOne: a = g.Int(2); break;
        case kSuspend: a = g.Int(3) != 0; break;
        case kSetClientSize: case kCreate: a = 50 + g.Int(300); b = 50 + g.Int(300); break;
        case kKey: case kMouseDown: case kMouse: case kInput: e = RandomEvent(g, call, w); break;
        case kTimer: a = g.Int(1000); break;
        case kRemoveComponent: case kAddComponent: a = 0x7000 + g.Int(10); break;
        case kIsMouseDown: case kCheckDouble: a = g.Int(3); break;
        case kSetParent: a = w > 1 && g.Int(4) != 0 ? 1 + g.Int(w - 1) : 0; break;
        case kSetStyle: a = g.Int(256); break;
        case kWorld: {
            WinWorld& x = sW[w];
            int which = g.Int(6);
            if (which == 0) x.rootCursor = g.Int(3) == 0 ? 0 : 0x5000 + g.Int(9);
            else if (which == 1) x.rootCanClose = !x.rootCanClose;
            else if (which == 2) x.allowClose = !x.allowClose;
            else if (which == 3) x.visible = !x.visible;
            else if (which == 4) x.timerFlags = g.Int(2);
            else { x.rootW = 100 + g.Int(50); x.rootH = 100 + g.Int(50); }
            Line("set %d %d", w, which);
            continue;
        }
        }
        Line("call %d %d %ld %ld %d %d %d %d %d %d %d %d", w, call, (long)a, (long)b, e.kind, e.x, e.y, e.key_mod,
             e.button, e.key, e.width, e.height);
        char key[32];
        snprintf(key, sizeof key, "call %d %d", call, e.kind);
        sMet.insert(key);
        intptr_t r = DoCall(rust, w, call, a, b, &e);
        Line("ret %ld", (long)r);
        for (int k = 1; k <= kN; k++) {
            if (rust) DumpRust(k);
            else sOrig[k].Dump();
        }
    }
    if (rust)
        for (int w = 1; w <= kN; w++) { ee_won_win_free(sRust[w]); sRust[w] = NULL; }
    return trace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kSequences = 20000;
    const int kCalls = 40;
    size_t aLines = 0;
    std::set<std::string> aSeen;
    int aDiffering = 0;
    for (int seq = 0; seq < kSequences; seq++) {
        sMet.clear();
        std::vector<std::string> a = RunSide(false, (unsigned)seq, kCalls);
        std::vector<std::string> b = RunSide(true, (unsigned)seq, kCalls);
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
        bool aNew = seq < 30;
        for (std::set<std::string>::const_iterator i = sMet.begin(); i != sMet.end(); ++i)
            if (aSeen.insert(*i).second) aNew = true;
        if (aPrint && aNew) {
            printf("seq %d\n", seq);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (!aPrint)
        printf("won_winobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
