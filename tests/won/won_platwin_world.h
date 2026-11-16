#include "rust/ee_won.h"
// The stand-in world of won_platwin_test.cpp, included once in each of its two namespaces (Orig: PlatGUIWindow.cpp's
// statements as they were, won_platwin_orig.h; Port: the shell over ee_won's platwin.rs, won_platwin_port.h): the
// WONAPI window base with the members the window reads and calls, its root and events, the platform manager with the
// calls the window makes, the cursors and the platform's pointer and minimize. Every call out is appended to the
// namespace's trace; every answer comes from the world's state (never from the order of the questions).
static std::string sTrace;
static void tr(const std::string& s) { sTrace += s; sTrace += '\n'; }
static std::string I(long long v) { return std::to_string(v); }
typedef unsigned long DWORD;
struct WONRectangle {
    int x, y, width, height;
    WONRectangle() : x(0), y(0), width(0), height(0) {}
    WONRectangle(int a, int b, int c, int d) : x(a), y(b), width(c), height(d) {}
};
enum { InputEvent_MouseExit = 3, InputEvent_Size = 50 };
enum { PopupFlag_DontWantMouse = 0x4000 };
struct InputEvent {
    int mType; std::string mArgs;
    explicit InputEvent(int t) : mType(t) {}
    virtual ~InputEvent() {}
};
struct MouseEvent : InputEvent { MouseEvent(int t, int x, int y, int m, int b) : InputEvent(t) { mArgs = I(x) + "," + I(y) + "," + I(m) + "," + I(b); } };
struct SizeEvent : InputEvent { SizeEvent(int x, int y, int w, int h) : InputEvent(InputEvent_Size) { mArgs = I(x) + "," + I(y) + "," + I(w) + "," + I(h); } };
struct Cursor { int mId; explicit Cursor(int i) : mId(i) {} virtual ~Cursor() {} };
struct PlatCursor : Cursor { explicit PlatCursor(int i) : Cursor(i) {} void Apply() { tr("cursor " + I(mId) + " Apply"); } };
struct PlatWindow;
static void PlatCursorShow(int s) { tr("PlatCursorShow " + I(s)); }
static void PlatWindowMinimize(PlatWindow* p) { tr("PlatWindowMinimize " + I((long long)(intptr_t)p)); }
struct Graphics { int mOwner; };
struct NativeImage { Graphics mG; Graphics& GetGraphics() { return mG; } };
struct DisplayContext { explicit DisplayContext(void*) {} };
struct Root {
    int mId; int mInvalidState; int mW, mH;
    Root() : mId(0), mInvalidState(0), mW(0), mH(0) {}
    int Width() { return mW; }
    int Height() { return mH; }
    bool Contains(int x, int y) { return x >= 0 && y >= 0 && x < mW && y < mH; }
    void PrePaint(Graphics&) { tr("w" + I(mId) + " root PrePaint"); }
    void Paint(Graphics&) { tr("w" + I(mId) + " root Paint"); }
};
struct WindowManager { virtual ~WindowManager() {} };
struct Window {
    enum WindowState { WindowState_Open, WindowState_Closing, WindowState_Closed };
    int mId; WindowManager* mManager; Window* mParent; int mStyle; bool mPopup; int mState; bool mMouseIn;
    DisplayContext* mDisplayContext; NativeImage* mOffscreenImageP; Root* mRoot;
    struct ImageSlot {
        NativeImage* p;
        ImageSlot& operator=(NativeImage* n) { p = n; return *this; }
        NativeImage* get() const { return p; }
        NativeImage* operator->() const { return p; }
    } mOffscreenImage;
    Window() : mId(0), mManager(NULL), mParent(NULL), mStyle(0), mPopup(false), mState(WindowState_Open), mMouseIn(false),
               mDisplayContext(NULL), mOffscreenImageP(NULL), mRoot(new Root) { mOffscreenImage.p = NULL; }
    virtual ~Window() {}
    Window* GetParent() const { return mParent; }
    int GetStyle() const { return mStyle; }
    int GetWindowState() const { return mState; }
    bool IsPopup() const { return mPopup; }
    void Create(const WONRectangle& r)
    {
        tr("w" + I(mId) + " Window::Create " + I(r.x) + "," + I(r.y) + "," + I(r.width) + "," + I(r.height));
        mRoot->mW = r.width; mRoot->mH = r.height; mRoot->mId = mId;
    }
    NativeImage* CreateImage(int w, int h) { tr("w" + I(mId) + " CreateImage " + I(w) + "," + I(h)); return new NativeImage; }
    void Center(bool b) { tr("w" + I(mId) + " Center " + I(b)); }
    void SetIsShowing(bool b) { tr("w" + I(mId) + " SetIsShowing " + I(b)); }
    void QueueEvent(InputEvent* e) { tr("w" + I(mId) + " event " + I(e->mType) + " " + e->mArgs); delete e; }
    void InvalidateRoot() { tr("w" + I(mId) + " InvalidateRoot"); }
    bool IsMouseInClientArea() { return mMouseIn; }
    void PlatformClose() { tr("w" + I(mId) + " Window::PlatformClose"); }
    void TimerEvent(DWORD e) { tr("w" + I(mId) + " Window::TimerEvent " + I((long long)e)); }
    bool IdleUpdate() { return true; }
    virtual void PlatformShow(bool) = 0;
};
struct PlatGUIWindow;
static std::string Id(PlatGUIWindow* w);
struct PlatWindowManager : WindowManager {
    int mSurfW = 800, mSurfH = 600;
    PlatGUIWindow* mCapture = NULL; PlatGUIWindow* mFocus = NULL; PlatGUIWindow* mHit = NULL;
    bool mMouseOk = false; int mMx = 0, mMy = 0;
    PlatWindow* mPlat = NULL; Window* mPopup = NULL; int mPopupFlags = 0;
    void AddPlatWindow(PlatGUIWindow* w) { tr("wm Add " + Id(w)); }
    void RemovePlatWindow(PlatGUIWindow* w) { tr("wm Remove " + Id(w)); }
    void BringToFront(PlatGUIWindow* w) { tr("wm BringToFront " + Id(w)); }
    void SetFocusWindow(PlatGUIWindow* w) { tr("wm SetFocus " + Id(w)); mFocus = w; }
    void SetCaptureWindow(PlatGUIWindow* w) { tr("wm SetCapture " + Id(w)); mCapture = w; }
    PlatGUIWindow* GetCaptureWindow() { return mCapture; }
    PlatGUIWindow* GetFocusWindow() { return mFocus; }
    PlatGUIWindow* WindowFromPoint(int x, int y) { tr("wm WindowFromPoint " + I(x) + "," + I(y)); return mHit; }
    bool GetMousePosition(int& x, int& y) { if (mMouseOk) { x = mMx; y = mMy; } return mMouseOk; }
    void SetDirty() { tr("wm SetDirty"); }
    int GetSurfaceWidth() const { return mSurfW; }
    int GetSurfaceHeight() const { return mSurfH; }
    PlatWindow* GetPlatWindow() { return mPlat; }
    Window* GetPopupWindow() { return mPopup; }
    int GetPopupFlags() { return mPopupFlags; }
    // the shell's Call, as PlatWindowManager's (the calls the window makes)
    intptr_t Call(EeWonPlatWmCall c, intptr_t a = 0, intptr_t b = 0)
    {
        typedef EeWonPlatWmCall C;
        switch (c) {
        case C::Add: AddPlatWindow((PlatGUIWindow*)a); return 0;
        case C::Remove: RemovePlatWindow((PlatGUIWindow*)a); return 0;
        case C::BringToFront: BringToFront((PlatGUIWindow*)a); return 0;
        case C::SetFocus: SetFocusWindow((PlatGUIWindow*)a); return 0;
        case C::SetCapture: SetCaptureWindow((PlatGUIWindow*)a); return 0;
        case C::Capture: return (intptr_t)GetCaptureWindow();
        case C::Focus: return (intptr_t)GetFocusWindow();
        case C::WindowFromPoint: return (intptr_t)WindowFromPoint((int)a, (int)b);
        case C::SetDirty: SetDirty(); return 0;
        case C::SurfaceWidth: return GetSurfaceWidth();
        case C::SurfaceHeight: return GetSurfaceHeight();
        case C::PlatWindow: return (intptr_t)GetPlatWindow();
        default: tr("wm unexpected call " + I((int)c)); return 0;
        }
    }
};
