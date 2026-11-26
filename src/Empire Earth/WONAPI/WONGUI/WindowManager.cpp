// Window manager: windows, focus, event queue and timers, modal dialogs, popups, fonts and image caches
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPlatform.h" // CreateNativeWindow
#include "ee/wonapi/WONRouting.h" // WONTickCount
#include "platform/PlatKeys.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/won_gui/guictl.h"
#include "rust/won_gui/guihost.h"
#include "rust/won_gui/guisplit.h"
#include "rust/won_gui/wmcache.h"
#include "rust/won_gui/wmobj.h"
#include "platform/PlatTime.h"
#include "rust/won_gui/platimg.h"
#include "rust/won_gui/platwin.h"
#include "rust/won_gui/platwm.h"
#include "rust/won_gui/platloop.h"
#include "rust/won_gui/gamewin.h"
#include <string.h>
#include <set>
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
#include "ee/wonapi/WONLobbyStretchImage.h"
#include "lle/GETextureManager.h"
#include "rust/won_gui/platcursor.h"
using namespace WONAPI;
// The popup stack, the modal dialog, the message loop, focus and the caches are ee_won's Rust manager (wmobj.rs,
// wmcache.rs; R6 WON round 33); the window set, the event list, the timers, the broadcasts, the event loop and the
// screen resolution are its wmloop.rs (round 34). This unit keeps the colour scheme, the decoders, the font
// helpers and the forwarders.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static_assert(sizeof(WONRectangle) == sizeof(EeWonRect4) && sizeof(PopupParams) == sizeof(EeWonWmPopup) &&
              offsetof(PopupParams, mOwnerComponent) == offsetof(EeWonWmPopup, owner) &&
              offsetof(PopupParams, mFlags) == offsetof(EeWonWmPopup, flags) && offsetof(PopupParams, my) == offsetof(EeWonWmPopup, y) &&
              offsetof(PopupParams, mAvoidRect) == offsetof(EeWonWmPopup, avoid) && offsetof(WindowEventBase, mRec) == EE_WON_WM_EVENT_REC_AT,
              "wmobj.rs reads a WONRectangle as EeWonRect4 and PopupParams as EeWonWmPopup (DoPopup, AdjustPopupPosition)");
#pragma clang diagnostic pop
// R6 WON rounds 33-34: the Rust manager's calls out (EeWonGuiOp's Wm* ops; rust/crates/ee_won_gui/src/wmobj.rs,
// wmloop.rs)
typedef EeWonGuiOp Op;

// the pictures and attributes a run of ops shares (guisplit.rs wm_op: the original's locals)
static SmartPtr<NativeImage> sHostImage;
static SmartPtr<RawImage> sHostRaw;
static ImageAttributes sHostAttr;
static SmartPtr<Font> sHostFont; // WmDefaultWindowFont's local

extern "C" intptr_t ee_won_guicpp_wm_call(void* inWm, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    WindowManager* t = (WindowManager*)inWm;
    switch (op) {
    case Op::WmTick: return (intptr_t)(uint32_t)WONTickCount();
    // (a component's calls, its two virtuals among them, are wmobj.rs game_comp's: R6 WON GUI rounds 37, 40)
    case Op::WmScreenRect: t->GetScreenRect(*(WONRectangle*)a); return 0;
    case Op::WmInputRec: return (intptr_t)&((WindowEventBase*)a)->mRec;
    case Op::WmDecodeKeep: sHostImage = t->mImageDecoder.Decode((DisplayContext*)a, std::string((const char*)b, (size_t)c).c_str()); return 0;
    case Op::WmHostImage: return (intptr_t)sHostImage.get();
    case Op::WmHostImageClear: sHostImage = SmartPtr<NativeImage>(); return 0;
    case Op::WmDecodeRawKeep: sHostRaw = t->mImageDecoder.DecodeRaw(std::string((const char*)a, (size_t)b).c_str()); return 0;
    case Op::WmHostRaw: return (intptr_t)sHostRaw.get();
    case Op::WmHostRawClear: sHostRaw = SmartPtr<RawImage>(); return 0;
    case Op::WmImageAttributes: return t->mImageDecoder.GetImageAttributes(std::string((const char*)a, (size_t)b).c_str(), sHostAttr) ? 1 : 0;
    case Op::WmNewDelayImage: return (intptr_t)new Image(ee_won_image_new_delay(WONImageHost, (const uint8_t*)a, (size_t)b, sHostAttr.mWidth, sHostAttr.mHeight));
    case Op::WmNextEvent: t->GetNextEvent(); return 0;
    // (an event's type, window and input record and an input event's delivery are guisplit.rs's over its record,
    // the window's calls gamewin.rs's: R6 WON GUI round 38)
    case Op::WmEvent: if ((EeWonWmEvent)a == EeWonWmEvent::Deliver) ((WindowEventBase*)b)->Deliver(); return 0;
    case Op::WmSignal: t->Signal(); return 0;
    // the statements of the new popup window, the default window font, the resolution and the size events
    // (guisplit.rs composes them: R6 WON round 33)
    case Op::WmNativeWindowNew: return (intptr_t)CreateNativeWindow();
    case Op::WinSetOneComponent: ((Window*)a)->SetOneComponentWindow(true); return 0;
    case Op::WinSetStyle: ((Window*)a)->SetStyle((int)b); return 0;
    case Op::WinSetParent: ((Window*)a)->SetParent((Window*)b); return 0;
    case Op::WinCreate: ((Window*)a)->Create(*(const WONRectangle*)b); return 0;
    case Op::WmFontKeep: sHostFont = t->GetDefaultWindow()->GetFont(FontDescriptor("Tahoma", 0, 8)); return 0;
    case Op::WmHostFont: return (intptr_t)sHostFont.get();
    case Op::WmHostFontClear: sHostFont = SmartPtr<Font>(); return 0;
    case Op::WmScreenResGet: t->GetScreenResolutionHook(*(int*)a, *(int*)b); return 0;
    case Op::WmScreenResSet: return t->SetScreenResolutionHook((int)a, (int)b) ? 1 : 0;
    default: return 0;
    }
}

// R6 WON GUI round 35: the platform manager (the port's PlatWindowManager.cpp, folded into WindowManager)
namespace WONAPI {
static_assert(sizeof(PlatInputEvent) == sizeof(EeWonPlatEvent), "platwm.rs reads PlatInputEvent as EeWonPlatEvent");


// The Rust manager's calls on the windows and the platform (EeWonPlatWmOp), each the original's statement
static intptr_t PlatWmHost(void* inMe, EeWonPlatWmOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    WindowManager* me = (WindowManager*)inMe;
    Window* w = (Window*)a;
    typedef EeWonPlatWmOp Op;
    switch (inOp) {
    case Op::IsUiThread: return PlatCurrentThreadId() == WindowManager::GetThreadId();
    case Op::ClosePopup: w->ClosePopup(NULL); return 0;
    case Op::Modifiers: return (intptr_t)PlatModifiers();
    case Op::ClientSize: PlatWindowGetClientSize((PlatWindow*)a, (int32_t*)b, (int32_t*)b + 1); return 0;
    case Op::Minimize: PlatWindowMinimize((PlatWindow*)a); return 0;
    case Op::Wake: PlatWindowWake(); return 0;
    default: return 0;
    }
}
}

// the platform's cursor calls (cursorobj.rs's platform cursors)
static const EeWonPlatCursorFns kCursorFns = {(void (*)(uint32_t))PlatCursorSetSystem, PlatCursorCreate, PlatCursorSet,
                                              PlatCursorDestroy};

// Creates the window manager: becomes the default manager, sets standard colours. The original registered BMP and
// WIM decoders here; the port reads PNG and JPEG pictures only (D9; the lobby's game code registers both), so they
// went (round 18)
WindowManager::WindowManager() {
    this->mWmObj.p = ee_won_wm_new(this, ee_won_gui_wm_game);
    WindowManager::mThreadId=PlatCurrentThreadId();
    WindowManager::mDefaultWindowManager=this;
    this->WmSet(EeWonWmField::LastTimerTick, (intptr_t)(uint32_t)WONTickCount());
    this->mColorScheme=new ColorScheme;
    ee_won_wm_standard_colors((uint32_t*)this->mColorScheme->mColors); // InitColorScheme (0x32c230): wmobj.rs's table
    // the platform manager and the standard cursors: the WON hand from its bits, the others the system's
    this->mRust = ee_won_plat_wm_new(this, PlatWmHost);
    ee_won_wm_bind(this, this->mWmObj.p, this->mRust, WONImageHost); // the windows' Rust parts call it directly (gamewin.rs)
    this->mEventForward = NULL;
    ee_won_cursor_install_standard(WONCursorHost, &kCursorFns);
    // The window set, the events, the timers, the broadcasts and the screen state start empty in the Rust manager
    // (wmloop.rs); the original left the image cache count and the full-screen size unset
}

// Destroys the window manager after closing every open window
WindowManager::~WindowManager()
{
    this->CloseAllWindows();
    this->Call(EeWonPlatWmCall::UnregisterAll);
    ee_won_plat_wm_free(this->mRust);
    this->mRust = NULL;
    ee_won_wm_bind(this, this->mWmObj.p, NULL, WONImageHost);
}

// The default font with another style and, when the size is positive, another size, from the default window
Font* WindowManager::GetDefaultFontMod(int inStyle, int inSize)
{
    return this->GetDefaultWindow()->GetFont(FontDescriptor(this->GetDefaultFont()->mDescriptor.mFace, inStyle, inSize > 0 ? inSize : this->GetDefaultFont()->mDescriptor.mSize)).get();
}

// Decodes an image file for the default window's display context; NULL when there is no window
SmartPtr<NativeImage> WindowManager::DecodeImage(const char *theFilePath)
{
    Window* aWindow = this->GetDefaultWindow();
    return aWindow == NULL ? SmartPtr<NativeImage>() : this->DecodeImage(aWindow->mDisplayContext.get(), theFilePath);
}

// Class statics: the default window manager and the GUI thread id (constant-initialised)
WindowManager* WindowManager::mDefaultWindowManager = NULL;
DWORD WindowManager::mThreadId = 0;

// the host behind the game's (guisplit.rs ee_won_gui_wm_game)
[[maybe_unused]] static const bool sSplitWmHost = (ee_won_gui_split_register(EeWonGuiSplitHost::Wm, ee_won_guicpp_wm_call), true);

// the platform's calls of an event pass (platloop.rs ee_won_plat_wm_next_event)
const EeWonPlatLoopFns WindowManager::kLoopFns = {(int32_t (*)(void*, uint32_t))PlatWindowWaitEvent, PlatTimeMilliseconds64,
                                                 PlatSleep};

// R6 WON GUI round 35: the cursors are cursorobj.rs's (the kinds, the standard cursors' table); this is what they ask
// of the C++: the references a cursor keeps (a CursorRef's cursor, a ComponentCursor's component), another cursor's
// answers, and the handles
intptr_t WONAPI::WONCursorHost(EeWonCursorOp op, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonCursorOp Op;
    switch (op) {
    case Op::Native: return (intptr_t)((Cursor*)a)->GetNative();
    case Op::Component: return (intptr_t)((Cursor*)a)->GetComponent(*(int*)b, *(int*)c);
    case Op::Wrap: return (intptr_t)new Cursor((EeWonCursor*)a);
    case Op::RustOf: return (intptr_t)((Cursor*)a)->mRust;
    }
    return 0;
}

// ---- PlatNativeImage.cpp until R6 WON GUI round 39
// The WONAPI GUI's images on the platform layer (WONLobbyPlatform.h): the images' host, icons, cursors and
// DisplayContext::CreateImage. Replaces the GDI backend's MSNativeImage.cpp, MSRawImage.cpp and DisplayContext.cpp.
// R6 WONLobby round 24: the images themselves (the native images in software surfaces, their masks, the raw images'
// conversions, the texture pack's copies) are ee_won_gui's imgobj.rs; this is what they ask of the C++
// (WONImageHost: references, the handles, the surfaces, display contexts and Graphics, the draw plans' run).

namespace WONAPI {

void WONImageThrow(int inStatus) { FileReaderCheck(inStatus); }

// A blank (black) image of the given size at the lobby surface's pixel scale (the windows' offscreen images and
// what the GUI composes for them)
static NativeImage* NewOffscreen(int inWidth, int inHeight)
{
    WindowManager* theManager = WindowManager::GetDefaultWindowManager();
    return new Image(ee_won_image_new_native(WONImageHost, inWidth, inHeight, theManager != NULL && theManager->mRust != NULL ? theManager->GetPixelScale() : 1.0f));
}

// imgobj.rs reads a handle's mRust and a context's members in place (R6 WON GUI round 38)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static_assert(offsetof(Image, mRust) == EE_WON_IMAGE_RUST_AT && offsetof(DisplayContext, mSurface) == EE_WON_CONTEXT_AT &&
              offsetof(DisplayContext, mGraphics) == EE_WON_CONTEXT_AT + sizeof(void*) &&
              offsetof(DisplayContext, mPixelScale) == EE_WON_CONTEXT_AT + 2 * sizeof(void*),
              "imgobj.rs's EeWonDisplayContextRec");
#pragma clang diagnostic pop

static intptr_t Held(RefCount* inObject)
{
    if (inObject != NULL)
        inObject->AddRef();
    return (intptr_t)inObject;
}

intptr_t WONImageHost(EeWonImageHost op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonImageHost H;
    switch (op) {
        case H::Wrap: {
            EeWonImage* theRust = (EeWonImage*)a;
            Image* theImage;
            switch (ee_won_image_kind(theRust)) {
                case EeWonImageKind::Stretch: theImage = new StretchImage(theRust); break;
                case EeWonImageKind::StretchCenter: theImage = new StretchCenterImage(theRust); break;
                default: theImage = new Image(theRust); break;
            }
            return (intptr_t)theImage;
        }
        // (a Graphics' translation, context, plan and image draw, its making and freeing are gfxrun.rs's: R6 WON GUI
        // round 39)
        // (the surface is the Rust image's own: no SurfaceBase, R6 WON GUI round 36)
        case H::NewContext: return Held(new DisplayContext((EeWonSurface*)a, *(const float*)b));
        case H::CreateImage: return Held(NewOffscreen((int)b, (int)c));
        case H::Decode: {
            SmartPtr<NativeImage> theImage = WindowManager::GetDefaultWindowManager()->DecodeImage((const char*)a);
            return Held(theImage.get());
        }
        case H::PackShift:
            return WindowManager::GetDefaultWindowManager() != NULL ? (intptr_t)GETextureManager::GetTexturePackLogicalShift() : 0;
        default: break;
    }
    return 0;
}

} // namespace WONAPI
