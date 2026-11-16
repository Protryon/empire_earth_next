// The stand-in world of won_platwm_test.cpp, included once in each of its two namespaces (Orig: PlatWindowManager.cpp's
// statements as they were, won_platwm_orig.h; Port: the shell and host over ee_won's platwm.rs, won_platwm_port.h):
// the WONAPI window, event, colour scheme and image classes with the members the manager reads, windows that behave
// as PlatGUIWindow's methods do (a window taking the focus raises itself and moves the focus, a closing one leaves
// the manager, a moved one queues its size events), and the platform's window system (its events, client size,
// modifiers, wake, minimize, the UI thread). Every call out is appended to the namespace's trace.
using WONAPI::RefCount;
using WONAPI::SmartPtr;
using WONAPI::SurfaceBase;
static std::string sTrace;
static void tr(const std::string& s) { sTrace += s; sTrace += '\n'; }
static std::string I(long long v) { return std::to_string(v); }
struct WONRectangle {
    int x, y, width, height;
    WONRectangle() : x(0), y(0), width(0), height(0) {}
    WONRectangle(int a, int b, int c, int d) : x(a), y(b), width(c), height(d) {}
};
enum { InputEvent_GotFocus = 101, InputEvent_LostFocus, InputEvent_MouseExit, InputEvent_KeyDown, InputEvent_KeyUp, InputEvent_KeyChar,
       InputEvent_MouseMove, InputEvent_MouseDown, InputEvent_MouseDoubleClick, InputEvent_MouseUp, InputEvent_MouseWheel, InputEvent_Size };
enum StandardColor { StandardColor_3DDarkShadow = 0, StandardColor_3DFace, StandardColor_3DHilight, StandardColor_3DShadow,
    StandardColor_Scrollbar, StandardColor_ButtonText, StandardColor_GrayText, StandardColor_Hilight, StandardColor_HilightText,
    StandardColor_ToolTipBack, StandardColor_ToolTipText, StandardColor_MenuBack, StandardColor_MenuText, StandardColor_Back,
    StandardColor_Text };
struct InputEvent {
    int mType; std::string mArgs;
    explicit InputEvent(int t) : mType(t) {}
    virtual ~InputEvent() {}
};
struct KeyEvent : InputEvent { KeyEvent(int t, int k, int m) : InputEvent(t) { mArgs = I(k) + "," + I(m); } };
struct MouseEvent : InputEvent { MouseEvent(int t, int x, int y, int m, int b) : InputEvent(t) { mArgs = I(x) + "," + I(y) + "," + I(m) + "," + I(b); } };
struct SizeEvent : InputEvent { SizeEvent(int x, int y, int w, int h) : InputEvent(InputEvent_Size) { mArgs = I(x) + "," + I(y) + "," + I(w) + "," + I(h); } };
struct ColorScheme : RefCount {
    void SetStandardColor(StandardColor c, DWORD v) { tr("scheme " + I(c) + "=" + I(v)); }
};
struct NativeImage : RefCount { virtual ~NativeImage() {} };
struct PlatNativeImage : NativeImage {
    SmartPtr<SurfaceBase> mSurface;
    int GetPixelWidth() { return this->mSurface->GetWidth(); }
    int GetPixelHeight() { return this->mSurface->GetHeight(); }
    uint32_t* GetPixels() { return (uint32_t*)this->mSurface->GetPixelsBase(); }
    int GetPitchPixels() { return this->mSurface->GetPixelsPerRow(); }
};
struct Window : RefCount {
    enum { WindowState_Open = 1, WindowState_Closed = 2 };
    int mId; Window* mParent; int mStyle; bool mPopup; int mState; bool mNotModal; bool mInvalid;
    std::wstring mTitle;
    SmartPtr<NativeImage> mOffscreenImage;
    Window() : mId(0), mParent(NULL), mStyle(0), mPopup(false), mState(WindowState_Open), mNotModal(false), mInvalid(false) {}
    virtual ~Window() {}
    Window* GetParent() const { return this->mParent; }
    bool IsPopup() const { return this->mPopup; }
    int GetStyle() const { return this->mStyle; }
    int GetWindowState() const { return this->mState; }
    void QueueEvent(InputEvent* e) { tr("w" + I(this->mId) + " event " + I(e->mType) + " " + e->mArgs); delete e; }
    void ClosePopup(Window*) { tr("w" + I(this->mId) + " ClosePopup"); }
    virtual void Close() = 0;
};
class PlatWindowManager;
struct PlatGUIWindow : Window {
    int mX, mY, mWidth, mHeight;
    bool mVisible, mTopMost, mRegistered, mMinimizeOnAltTab, mForwardKeyMsgToParent;
    PlatGUIWindow() : mX(0), mY(0), mWidth(0), mHeight(0), mVisible(false), mTopMost(false), mRegistered(false),
                      mMinimizeOnAltTab(false), mForwardKeyMsgToParent(false) {}
    bool FillsSurface() const { return this->GetParent() == NULL; }
    bool HasFrame() const { return this->GetParent() != NULL && !(this->GetStyle() & 0x20); }
    void GetClientRect(WONRectangle& outRect);
    void GetInfo(EeWonPlatWinInfo& outInfo);
    PlatWindowManager* GetPlatManager();
    void FollowSurface();
    void FollowPixelScale();
    void OnPaint();
    void QueueMouse(int inType, int inSurfaceX, int inSurfaceY, int inMod, int inButtonOrAmount);
    void SetPosSize(int x, int y, int width, int height);
    void PlatformSetFocus();
    virtual void Close();
};
struct WindowManager : RefCount {
    SmartPtr<ColorScheme> mColorScheme;
    std::vector<PlatGUIWindow*> mAll;   // the world's windows (CloseAllWindows)
    WindowManager() { this->mColorScheme = new ColorScheme; }
    virtual ~WindowManager() {}
    static DWORD& ThreadId() { static DWORD t = 1; return t; }
    static DWORD GetThreadId() { return ThreadId(); }
    bool NotModalWindow(Window* w) { tr("NotModalWindow w" + I(w->mId)); return w->mNotModal; }
    void ProcessEvents() { tr("ProcessEvents"); }
    void CloseAllWindows()
    {
        tr("CloseAllWindows");
        for (size_t i = 0; i < this->mAll.size(); i++)
            if (this->mAll[i]->mState == Window::WindowState_Open) this->mAll[i]->Close();
    }
    virtual void GetNextEvent() {}
    virtual bool SetScreenResolutionHook(int, int) { return false; }
    virtual void GetScreenResolutionHook(int&, int&) {}
    virtual void Signal() {}
    virtual void GetScreenRect(WONRectangle&) {}
    virtual void GetSystemColorScheme() {}
};
// the platform's window system
static DWORD sCurrentThread = 1;
static int32_t sClientW, sClientH;
static uint32_t sModifiers;
static uint64_t sClock = 1000;
static std::vector<PlatInputEvent> sEvents;
static DWORD PlatCurrentThreadId() { return sCurrentThread; }
static void PlatWindowWake() { tr("PlatWindowWake"); }
static void PlatWindowGetClientSize(PlatWindow* w, int32_t* x, int32_t* y) { tr("ClientSize " + I((long long)(intptr_t)w)); *x = sClientW; *y = sClientH; }
static void PlatWindowMinimize(PlatWindow* w) { tr("Minimize " + I((long long)(intptr_t)w)); }
static uint32_t PlatModifiers() { tr("PlatModifiers"); return sModifiers; }
static uint64_t PlatTimeMilliseconds64() { return sClock += 7; }
static void PlatSleep(uint32_t) {}   // a timing artifact (the timeout is the clock's): not traced
static int PlatWindowWaitEvent(PlatInputEvent* e, uint32_t)
{
    tr("WaitEvent");
    if (sEvents.empty()) return 0;
    *e = sEvents.back();
    sEvents.pop_back();
    return 1;
}
