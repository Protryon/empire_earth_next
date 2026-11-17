// The WONAPI GUI's window manager on the platform layer (WONLobbyPlatform.h), in place of the GDI backend's
// MSWindowManager.cpp and MSWindow's window procedure. R6 WON round 40: the manager is ee_won's Rust object
// (rust/crates/ee_won_gui/src/platwm.rs: the surface's size and scale, the frame and its lock, the windows' z order,
// focus, capture and drag, the event translation, the compositor); this class is its shell: the base class, the
// standard cursors, the platform event loop's wait and the host the Rust calls the windows and the platform through.
#include <windows.h>
#include <string>
#include <string.h>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPlatform.h"
#include "platform/PlatTime.h"
#include "rust/won_gui/platimg.h"
#include "rust/won_gui/platwin.h"
#include "rust/won_gui/platwm.h"

namespace WONAPI {

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
    case Op::SetRegistered: w->Rust(EeWonPlatWinCall::SetRegistered, b != 0); return 0;
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
    std::vector<uint32_t> theHand(32 * 32);
    ee_won_plat_hand_cursor(theHand.data());   // the WON hand (32x32, hot spot 12, 4; platimg.rs)
    Cursor::SetStandardCursor(StandardCursor_Hand, new PlatCursor(theHand.data(), 32, 32, 12, 4));
    Cursor::SetStandardCursor(StandardCursor_Arrow, new PlatCursor(PLAT_CURSOR_ARROW));
    Cursor::SetStandardCursor(StandardCursor_IBeam, new PlatCursor(PLAT_CURSOR_IBEAM));
    Cursor::SetStandardCursor(StandardCursor_EastWest, new PlatCursor(PLAT_CURSOR_SIZE_WE));
    Cursor::SetStandardCursor(StandardCursor_NorthSouth, new PlatCursor(PLAT_CURSOR_SIZE_NS));
    Cursor::SetStandardCursor(StandardCursor_NorthSouthEastWest, new PlatCursor(PLAT_CURSOR_SIZE_ALL));
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

// ---- game interface

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

// ---- WindowManager

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

} // namespace WONAPI
