#include "rust/ee_won.h"
// PlatWindowManager.cpp's statements after the port (R6 WON round 40: the shell and host over ee_won's platwm.rs),

// copied from the working tree by build/r40/gen_platwm.py (the constructor without its cursors), with the class and

// PlatGUIWindow::GetInfo and GetClientRect; won_platwm_test.cpp compiles them over its stand-in world.



class PlatWindowManager : public WindowManager {
public:
    // ---- game interface (stable)
    typedef void (*EventForwardFunc)(const PlatInputEvent& inEvent);
    // Creates the manager (it becomes the default window manager) on the UI thread; the standard cursors are set.
    PlatWindowManager();
    virtual ~PlatWindowManager();
    // The lobby surface's size in pixels (the frame LockFrame returns; the screen the windows live on). Windows
    // without a parent follow it. Any thread: applied at once on the UI thread, otherwise at the next GetNextEvent.
    void SetSurfaceSize(int inWidth, int inHeight);
    // The surface's pixels per logical pixel (>= 1; the frame is the surface's size times it, rounded:
    // PlatGUIPixels). Any thread, as SetSurfaceSize. Windows repaint at the new scale.
    void SetPixelScale(float inScale);
    float GetPixelScale() const;
    // Where the game shows the frame in its window, as fractions of the window's client area (left, top, width,
    // height): the pointer is mapped back through it (ScaleToSurface). Before the first call: the surface scaled
    // uniformly to fit, centred. Any thread.
    void SetPresentRect(float inLeft, float inTop, float inWidth, float inHeight);
    // The last composited frame: 32-bit pixels 0xFFRRGGBB (B, G, R, 0xFF in memory), outWidth x outHeight pixels,
    // outPitch bytes per row, showing outLogicalWidth x outLogicalHeight of the surface, and a serial that changes
    // with every new frame -> false when nothing was composited yet (nothing to unlock then). Holds the frame lock
    // until UnlockFrame: copy the pixels out and unlock. Any thread.
    bool LockFrame(const uint32_t*& outPixels, int& outWidth, int& outHeight, int& outPitch, int& outLogicalWidth,
                   int& outLogicalHeight, uint32_t& outSerial);
    void UnlockFrame();
    // The platform events the game must still see while the lobby owns the event loop: QUIT, CLOSE, RESIZED,
    // MINIMIZED, RESTORED, FOCUS_GAINED, FOCUS_LOST, EXPOSED, and KEY_DOWN/KEY_UP when they go to a window that
    // forwards its keys (PlatGUIWindow::mForwardKeyMsgToParent; the original sent them to the game window). Called
    // on the UI thread, after the lobby handled the event. NULL: none.
    void SetEventForward(EventForwardFunc inFunc);

    // ---- WindowManager
    virtual void GetNextEvent();
    virtual bool SetScreenResolutionHook(int inWidth, int inHeight);
    virtual void GetScreenResolutionHook(int& outWidth, int& outHeight);
    virtual void Signal();
    virtual void GetScreenRect(WONRectangle& outRect);
    virtual void GetSystemColorScheme();

    // ---- backend (PlatGUIWindow; the Rust manager's, platwm.rs)
    void AddPlatWindow(PlatGUIWindow* inWindow) { this->Call(EeWonPlatWmCall::Add, (intptr_t)inWindow); }
    void RemovePlatWindow(PlatGUIWindow* inWindow) { this->Call(EeWonPlatWmCall::Remove, (intptr_t)inWindow); }
    void BringToFront(PlatGUIWindow* inWindow) { this->Call(EeWonPlatWmCall::BringToFront, (intptr_t)inWindow); }
    void SetFocusWindow(PlatGUIWindow* inWindow) { this->Call(EeWonPlatWmCall::SetFocus, (intptr_t)inWindow); }
    void SetCaptureWindow(PlatGUIWindow* inWindow) { this->Call(EeWonPlatWmCall::SetCapture, (intptr_t)inWindow); }
    PlatGUIWindow* GetCaptureWindow() { return (PlatGUIWindow*)this->Call(EeWonPlatWmCall::Capture); }
    PlatGUIWindow* GetFocusWindow() { return (PlatGUIWindow*)this->Call(EeWonPlatWmCall::Focus); }
    // the topmost visible window whose window rectangle holds the surface point (WindowFromPoint)
    PlatGUIWindow* WindowFromPoint(int inX, int inY) { return (PlatGUIWindow*)this->Call(EeWonPlatWmCall::WindowFromPoint, inX, inY); }
    bool GetMousePosition(int& outX, int& outY) { return ee_won_plat_wm_mouse(this->mRust, &outX, &outY); }
    void SetDirty() { this->Call(EeWonPlatWmCall::SetDirty); }
    int GetSurfaceWidth() const { return (int)this->Call(EeWonPlatWmCall::SurfaceWidth); }
    int GetSurfaceHeight() const { return (int)this->Call(EeWonPlatWmCall::SurfaceHeight); }
    PlatWindow* GetPlatWindow() { return (PlatWindow*)this->Call(EeWonPlatWmCall::PlatWindow); }
    // paints every window whose root is invalid, then composites when anything changed
    void PaintAndComposite() { this->Call(EeWonPlatWmCall::PaintAndComposite); }

protected:
    void HandlePlatEvent(const PlatInputEvent& inEvent);
    intptr_t Call(EeWonPlatWmCall inCall, intptr_t a = 0, intptr_t b = 0) const;

    EeWonPlatWm* mRust;                         // the manager (platwm.rs)
    EventForwardFunc mEventForward;
};

static_assert(sizeof(PlatInputEvent) == sizeof(EeWonPlatEvent), "platwm.rs reads PlatInputEvent as EeWonPlatEvent");

static uint32_t TickNow() { return (uint32_t)PlatTimeMilliseconds64(); }

static const int kInputTypes[] = {InputEvent_GotFocus, InputEvent_LostFocus, InputEvent_MouseExit};
static const int kKeyTypes[] = {InputEvent_KeyDown, InputEvent_KeyUp, InputEvent_KeyChar};
static const int kMouseTypes[] = {InputEvent_MouseMove, InputEvent_MouseDown, InputEvent_MouseDoubleClick,
                                  InputEvent_MouseUp, InputEvent_MouseWheel};

// The Rust manager's calls on the windows and the platform (EeWonPlatWmOp), each the original's statement
static intptr_t PlatWmHost(void* inMe, EeWonPlatWmOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    PlatWindowManager* me = (PlatWindowManager*)inMe;
    PlatGUIWindow* w = (PlatGUIWindow*)a;
    typedef EeWonPlatWmOp Op;
    switch (inOp) {
    case Op::IsUiThread: return PlatCurrentThreadId() == WindowManager::GetThreadId();
    case Op::Info: w->GetInfo(*(EeWonPlatWinInfo*)b); return 0;
    case Op::SetRegistered: w->mRegistered = b != 0; return 0;
    case Op::FollowSurface: w->FollowSurface(); return 0;
    case Op::FollowPixelScale: w->FollowPixelScale(); return 0;
    case Op::QueueInput: w->QueueEvent(new InputEvent(kInputTypes[b])); return 0;
    case Op::QueueKey: w->QueueEvent(new KeyEvent(kKeyTypes[b], (int)c, (int)d)); return 0;
    case Op::QueueMouse: {
        const int32_t* p = (const int32_t*)c;
        w->QueueMouse(kMouseTypes[b], p[0], p[1], p[2], p[3]);
        return 0;
    }
    case Op::Parent: return (intptr_t)dynamic_cast<PlatGUIWindow*>(w->GetParent());
    case Op::Hold: w->AddRef(); return 0;
    case Op::Release: w->Release(); return 0;
    case Op::Close: w->Close(); return 0;
    case Op::ClosePopup: w->ClosePopup(NULL); return 0;
    case Op::NotModal: return me->NotModalWindow(w);
    case Op::SetFocus: w->PlatformSetFocus(); return 0;
    case Op::SetPosSize: w->SetPosSize((int)b, (int)c, -1, -1); return 0;
    case Op::Paint: w->OnPaint(); return 0;
    case Op::Title: {
        static std::wstring theTitle;
        theTitle = w->mTitle;
        *(int32_t*)b = (int32_t)theTitle.size();
        return (intptr_t)theTitle.c_str();
    }
    case Op::Offscreen: {
        PlatNativeImage* theImage = dynamic_cast<PlatNativeImage*>(w->mOffscreenImage.get());
        return theImage != NULL ? (intptr_t)theImage->mSurface->GetRust() : 0;
    }
    case Op::Modifiers: return (intptr_t)PlatModifiers();
    case Op::ClientSize: PlatWindowGetClientSize((PlatWindow*)a, (int32_t*)b, (int32_t*)b + 1); return 0;
    case Op::Minimize: PlatWindowMinimize((PlatWindow*)a); return 0;
    case Op::Wake: PlatWindowWake(); return 0;
    }
    return 0;
}


// Creates the manager and installs the standard cursors: the WON hand from its bits, the others the system's
PlatWindowManager::PlatWindowManager()
    : mRust(ee_won_plat_wm_new(this, PlatWmHost)), mEventForward(NULL)
{
}

PlatWindowManager::~PlatWindowManager()
{
    // the base destructor closes the windows too, but by then this part is gone
    this->CloseAllWindows();
    this->Call(EeWonPlatWmCall::UnregisterAll);
    ee_won_plat_wm_free(this->mRust);
}

intptr_t PlatWindowManager::Call(EeWonPlatWmCall inCall, intptr_t a, intptr_t b) const
{
    return ee_won_plat_wm(this->mRust, inCall, a, b);
}

void PlatWindowManager::SetSurfaceSize(int inWidth, int inHeight) { this->Call(EeWonPlatWmCall::SetSurfaceSize, inWidth, inHeight); }

void PlatWindowManager::SetPixelScale(float inScale) { ee_won_plat_wm_set_scale(this->mRust, inScale); }

float PlatWindowManager::GetPixelScale() const { return ee_won_plat_wm_scale(this->mRust); }

void PlatWindowManager::SetPresentRect(float inLeft, float inTop, float inWidth, float inHeight)
{ ee_won_plat_wm_present(this->mRust, inLeft, inTop, inWidth, inHeight); }

bool PlatWindowManager::LockFrame(const uint32_t*& outPixels, int& outWidth, int& outHeight, int& outPitch,
                                  int& outLogicalWidth, int& outLogicalHeight, uint32_t& outSerial)
{
    uint32_t v[6];
    if (!ee_won_plat_wm_lock_frame(this->mRust, &outPixels, v))
        return false;
    outWidth = (int)v[0];
    outHeight = (int)v[1];
    outPitch = (int)v[2];
    outLogicalWidth = (int)v[3];
    outLogicalHeight = (int)v[4];
    outSerial = v[5];
    return true;
}

void PlatWindowManager::UnlockFrame() { this->Call(EeWonPlatWmCall::UnlockFrame); }

void PlatWindowManager::SetEventForward(EventForwardFunc inFunc) { this->mEventForward = inFunc; }

// Waits for one platform event (GetMessage; at most until 50 ms after the last pass, the original's window timer),
// handles it, runs ProcessEvents (the window procedure did after each message), then paints and composites
void PlatWindowManager::GetNextEvent()
{
    uint32_t theTimeout = (uint32_t)this->Call(EeWonPlatWmCall::BeginPass);
    uint32_t theStart = TickNow();
    PlatInputEvent theEvent;
    if (PlatWindowWaitEvent(&theEvent, theTimeout))
        this->HandlePlatEvent(theEvent);
    else {
        // no window system (or a spurious return): never spin
        uint32_t theWaited = TickNow() - theStart;
        if (theWaited < theTimeout)
            PlatSleep(theTimeout - theWaited);
    }
    this->Call(EeWonPlatWmCall::EndWait);
    this->ProcessEvents();
    this->PaintAndComposite();
}

// The display mode is the game's: nothing to change
bool PlatWindowManager::SetScreenResolutionHook(int, int) { return true; }

void PlatWindowManager::GetScreenResolutionHook(int& outWidth, int& outHeight)
{
    outWidth = this->GetSurfaceWidth();
    outHeight = this->GetSurfaceHeight();
}

void PlatWindowManager::Signal() { this->Call(EeWonPlatWmCall::Signal); }

// The lobby surface is the screen
void PlatWindowManager::GetScreenRect(WONRectangle& outRect)
{
    outRect.x = outRect.y = 0;
    outRect.width = this->GetSurfaceWidth();
    outRect.height = this->GetSurfaceHeight();
}

// The colour scheme's standard colours from the classic Windows scheme (platwm.rs CLASSIC)
void PlatWindowManager::GetSystemColorScheme()
{
    for (int i = 0; this->Call(EeWonPlatWmCall::Classic, i) >= 0; i++)
        this->mColorScheme->SetStandardColor((StandardColor)i, (DWORD)this->Call(EeWonPlatWmCall::Classic, i));
}

// The platform event translated by the Rust; the events the game must still see go on to it
void PlatWindowManager::HandlePlatEvent(const PlatInputEvent& inEvent)
{
    if (ee_won_plat_wm_event(this->mRust, (const EeWonPlatEvent*)&inEvent) && this->mEventForward != NULL)
        this->mEventForward(inEvent);
}

// The fields the Rust manager reads (platwm.rs EeWonPlatWinInfo)
void PlatGUIWindow::GetInfo(EeWonPlatWinInfo& outInfo)
{
    outInfo.x = this->mX;
    outInfo.y = this->mY;
    outInfo.width = this->mWidth;
    outInfo.height = this->mHeight;
    outInfo.visible = this->mVisible;
    outInfo.top_most = this->mTopMost;
    outInfo.registered = this->mRegistered;
    outInfo.popup = this->IsPopup();
    outInfo.fills = this->FillsSurface();
    outInfo.frame = this->HasFrame();
    outInfo.open = this->GetWindowState() == WindowState_Open;
    outInfo.minimize_on_alt_tab = this->mMinimizeOnAltTab;
    outInfo.forward_keys = this->mForwardKeyMsgToParent;
}

// Inside the classic frame and caption of a framed window, else the window rectangle (platwm.rs client_rect)
void PlatGUIWindow::GetClientRect(WONRectangle& outRect)
{
    EeWonPlatWinInfo theInfo;
    this->GetInfo(theInfo);
    int r[4];
    ee_won_plat_client_rect(&theInfo, r);
    outRect.x = r[0];
    outRect.y = r[1];
    outRect.width = r[2];
    outRect.height = r[3];
}
