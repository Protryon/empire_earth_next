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
#include "rust/won_gui/fontmap.h"
#include "rust/won_gui/winobj.h"
#include "ee/wonapi/WONLobbyPlatform.h"
#include <stdlib.h>
#include "rust/won_gui/clipobj.h"
#include "platform/PlatWindow.h"
#include "platform/PlatString.h"
#include <stddef.h>
#include "ee/wonapi/WONGUIDraw.h"
#include "rust/won_gui/grabobj.h"
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

// ---- Window.cpp until R6 WON GUI round 41
// Window (R6 WON round 32): the Rust Window's calls out (rust/crates/ee_won_gui/src/winobj.rs, EeWonGuiOp's Win* ops), its
// constructor and destructor, and the methods over the members that stay C++ (the fonts, the images). The original's
// Window.cpp (the input dispatch, the cursor, the resize, closing, centring, the child windows) is the Rust object.

typedef EeWonGuiOp Op;

// the font a run of ops shares (winobj.rs get_font: the original's local SmartPtr)
static SmartPtr<Font> sWinHostFont;

extern "C" intptr_t ee_won_guicpp_win_call(void* inWin, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    Window* t = (Window*)inWin;
    switch (op) {
    case Op::WinTick: return (intptr_t)(uint32_t)WONTickCount();
    // (the offscreen image is gamewin.rs's: R6 WON GUI round 40)
    case Op::WinAllowClose: return 1; // (no window ever had an allow-close function)
    case Op::WinStdArrow: return (intptr_t)Cursor::GetStandardCursor(StandardCursor_Arrow);
    case Op::WinCursorNative: return (intptr_t)((Cursor*)a)->GetNative();
    case Op::WinPlatformFont: sWinHostFont = t->PlatformGetFont(*(const FontDescriptor*)a); return 0;
    case Op::WinHostFont: return (intptr_t)sWinHostFont.get();
    case Op::WinHostFontClear: sWinHostFont = SmartPtr<Font>(); return 0;
    default: return 0;
    }
}

// R6 WON GUI round 35: the platform window (the port's PlatGUIWindow.cpp, folded into Window)
namespace WONAPI {
// The Rust window's calls on its shell (EeWonPlatWinOp), each the original's statement
static intptr_t PlatWinHost(void* inMe, EeWonPlatWinOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    Window* me = (Window*)inMe;
    typedef EeWonPlatWinOp Op;
    switch (inOp) {
    case Op::WindowCreate: ee_won_win_create(me->mWinObj.p, (int)c, (int)d); return 0; // the default icon, the parent, the root's size, the manager
    case Op::NewContext: me->mDisplayContext = new DisplayContext(NULL); return 0;
    // (the offscreen image and its paints are gamewin.rs's: R6 WON GUI round 40)
    case Op::ApplyCursor: ee_won_cursor_apply(a != 0 ? ((Cursor*)a)->mRust : NULL, (void (*)(int32_t))PlatCursorShow); return 0;
    case Op::CursorShow: PlatCursorShow((int)a); return 0;
    case Op::PlatMinimize: PlatWindowMinimize((PlatWindow*)a); return 0;
    default: return 0;
    }
}
}

// Creates a closed window on the default window manager with a new root container and the title "Sierra App" (the
// Rust window: closed, hidden, no parent, the mouse outside, sizes 0..64000)
Window::Window()
{
	this->mWinObj.p = ee_won_win_new(this, ee_won_guicpp_win_call);
	this->mManager = WONAPI::WindowManager::GetDefaultWindowManager();
	this->mRoot = new RootContainer(this);
	this->mRust = ee_won_plat_win_new(this, PlatWinHost);
	// its Rust parts call each other directly (gamewin.rs; R6 WON GUI round 37)
	ee_won_win_bind(this, this->mWinObj.p, this->mRust, static_cast<Component*>(this->mRoot.get()), this->mRoot->mRootObj);
}

// Destroys the window: the platform window, the members, then the Rust window (its child set; mWinObj is the first
// member)
Window::~Window()
{
	this->Rust(EeWonPlatWinCall::Destroy);
	ee_won_plat_win_free(this->mRust);
	this->mRust = NULL;
	ee_won_win_bind(this, this->mWinObj.p, NULL, static_cast<Component*>(this->mRoot.get()), this->mRoot->mRootObj);
}

// Returns the cached font for a descriptor, creating it through the platform on first use (a NULL platform font
// is asked again at the next call; the map holds one reference to each font)
SmartPtr<Font> Window::GetFont(const FontDescriptor &theDescriptor)
{
	return (Font*)ee_won_win_get_font(this->mWinObj.p, this->mFontMap.p, (intptr_t)&theDescriptor, (const uint8_t*)theDescriptor.mFace.data(), theDescriptor.mFace.size(), theDescriptor.mStyle, theDescriptor.mSize);
}
// the map's reference to a font given back
static void ReleaseFont(intptr_t inFont) { ((Font*)inFont)->Release(); }
// Gives each cached font's reference back and frees the map (fontmap.rs)
Window::FontMap::~FontMap() { ee_won_fontmap_free_releasing(this->p, ReleaseFont); }

// Sets the window title, updating the platform window when open
void Window::SetTitle(const GUIString &theTitle)
{
	ee_won_plat_win_set_title(this->mRust, theTitle.mData->mText, theTitle.length());
	this->WCall(EeWonWinCall::TitleChanged);
}

// Returns the default window manager's default window
Window* Window::GetDefaultWindow()
{
	return WONAPI::WindowManager::GetDefaultWindowManager()->GetDefaultWindow();
}

// Creates an image of the given size for the window's display (one reference)
SmartPtr<NativeImage> Window::CreateImage(int theWidth, int theHeight)
{
	return WONImageAdopt<NativeImage>(WONImageHost(EeWonImageHost::CreateImage, 0, theWidth, theHeight, 0));
}

static bool gWindowSkipDecodeImages = false;

// Turns image decoding off or on for every window (a blank placeholder is used)
void Window::SetSkipDecodeImages(bool skip) {
    gWindowSkipDecodeImages = skip;
}

// Decodes an image file for the window's display; while decoding is off returns one shared blank 10x10 image
SmartPtr<NativeImage> Window::DecodeImage(const char* path) {
    if(gWindowSkipDecodeImages) {
        static SmartPtr<NativeImage> image;
        if(image.get()==NULL) {
            image=this->CreateImage(10,10);
            image->GetGraphics().SetColor(0);
            image->GetGraphics().FillRect(0,0,20,20);
        }
        return image;
    }
    return this->mManager->DecodeImage(this->mDisplayContext.get(),path);
}

// Decodes a raw image file; while decoding is off returns a blank 10x10 image
SmartPtr<RawImage> Window::DecodeImageRaw(const char* path) {
    if(gWindowSkipDecodeImages)
        return new RawImage(ee_won_image_new_raw8(WONImageHost,10,10));
    return this->mManager->DecodeImageRaw(path);
}

// Tags the event with this window and posts it to the window manager's queue (the base QueueEvent)
void Window::QueueEvent(WindowEventBase* event) { event->mRec.window=(intptr_t)this; this->mManager->PostEvent(event); }

namespace WONAPI {
Window* CreateNativeWindow() { return new Window; }

SmartPtr<Font> Window::PlatformGetFont(const FontDescriptor& inDescriptor)
{
    return new Font(this->mDisplayContext.get(), inDescriptor);
}

}

// The GUI's clipboard on the platform layer (PlatClipboard.cpp folded in, R6 WON GUI round 37): UTF-8 text
// through PlatClipboardGetText/SetText (decision D6)
namespace WONAPI {

// R6 WON GUI round 35: both methods are clipobj.rs's (the conversions between the platform's UTF-8 text and the
// GUI's wide text); these are the platform's calls it makes
static const EeWonClipFns kClipFns = {PlatClipboardGetText, PlatClipboardSetText, free};

// Returns the clipboard's text; empty when it holds none
GUIString Clipboard::Get() { return GUIString(std::wstring((const wchar_t*)ee_won_clip_get(&kClipFns))); }

// Replaces the clipboard's contents with the string
void Clipboard::Set(const GUIString& theStr) { ee_won_clip_set(&kClipFns, theStr.mData->mText, theStr.length()); }

} // namespace WONAPI

// ---- Graphics.cpp until R6 WON GUI round 39
// Graphics (R6 WONGUI B round 1): translated drawing, colours and the clip stack are ee_won's gfxobj.rs, the draw
// plans run in gfxrun.rs (round 38), and the Graphics is gfxrun.rs's record, made, attached and freed there (round
// 39). This unit keeps the host (the font's draw, a wrapped text's) and the Windows-style frames.
typedef EeWonGfxHook H;
// the Rust Graphics' calls out that are not the surface's nor Rust's (gfxplat.rs, gfxrun.rs): the font's draw (a[]
// its ints, p the text) and a wrapped text's paint
static uint32_t GfxHost(void* inGraphics, EeWonGfxHook inHook, const int32_t* a, const void* p)
{
    Graphics* g = (Graphics*)inGraphics;
    switch (inHook) {
    case H::String: g->GetFont()->DrawString(g->GetContext(), *(const GUIString*)p, (unsigned short)a[0], (unsigned short)a[1], a[2], a[3]); return 0;
    // (a wrapped text's paint is wrapobj.rs game_paint's: R6 WON GUI round 41)
    default: return 0;
    }
}
[[maybe_unused]] static const bool sGfxHost = (ee_won_graphics_register(GfxHost, offsetof(ColorScheme, mColors)), true);

// Windows-style frames (MSGraphicsUtil.cpp folded in, R6 WON GUI round 37): mspaint.rs's plans run on the Graphics
namespace WONAPI {
// the standard colours and directions mspaint.rs names
static_assert(StandardColor_3DDarkShadow==0 && StandardColor_3DFace==1 && StandardColor_3DHilight==2 &&
              StandardColor_3DShadow==3 && StandardColor_Scrollbar==4 && StandardColor_ButtonText==5 &&
              StandardColor_GrayText==6 && StandardColor_MenuBack==11 && StandardColor_Back==13 &&
              StandardColor_Text==14,"mspaint.rs SYS_*");
static_assert(Direction_Left==0 && Direction_Right==1 && Direction_Up==2 && Direction_Down==3,"mspaint.rs DIR_*");
// Draws a two-pixel 3D frame, raised or sunken (down), at the given position
void LobbyDraw3DFrame(Graphics &g, int x, int y, int width, int height, bool down) { WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Frame3D,width,height,down?EE_WON_MSPAINT_PRESSED:0,x,y)); }
// Draws a raised button frame, or a flat shadow-coloured rectangle when down
void LobbyDrawFrame(Graphics &g, int x, int y, int width, int height, bool down) { WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Frame,width,height,down?EE_WON_MSPAINT_PRESSED:0,x,y)); }
}

// ---- BackgroundGrabber.cpp until R6 WON GUI round 39
// BackgroundGrabber (R6 WON round 37): the Rust grabber (rust/crates/ee_won_gui/src/grabobj.rs) holds the captured image's
// place, its clip, the flags, the translucent colour and level and the capture's order and runs every method (the
// capture, the debug window, the tint, putting the background back); this is its host (the component, the Graphics,
// the background image, the debug window) and the constructor and destructor.

typedef EeWonGrab Gb;
static_assert(offsetof(BackgroundGrabber, mBGImage) == 0 && sizeof(SmartPtr<NativeImage>) == sizeof(void*) &&
              offsetof(BackgroundGrabber, mRust) == 2 * sizeof(void*), "grabobj.rs reads the image and its grabber");
static_assert(ComponentFlag_DebugBG == EE_WON_GRAB_FLAG_DEBUG_BG && ComponentFlag_Translucent == EE_WON_GRAB_FLAG_TRANSLUCENT,
              "grabobj.rs flags");

// the grabber's calls out, each one of the original's statements (R6 WON round 34: their runs are grabobj.rs's; the
// component's and the background image's are grabobj.rs's own in the game, R6 WON GUI round 39)
extern "C" intptr_t ee_won_guicpp_grab_call(void* inMe, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    BackgroundGrabber* t = (BackgroundGrabber*)inMe;
    Component* comp = (Component*)b;
    if (op != EeWonGuiOp::Grab)
        return 0;
    switch ((Gb)a) {
    case Gb::DebugExists: return t->mDebugWindow.get() != NULL ? 1 : 0;
    case Gb::DebugNew: t->mDebugWindow = CreateNativeWindow(); return 0;
    case Gb::DebugParent: t->mDebugWindow->SetParent(comp->GetRoot()); return 0;
    case Gb::DebugStyle: t->mDebugWindow->SetStyle(0x40); return 0;
    case Gb::DebugCreateAt: t->mDebugWindow->Create(WONRectangle(0, 0, (int)c, (int)d)); return 0;
    case Gb::DebugRemoveAll: t->mDebugWindow->mRoot.get()->RemoveAllChildren(); return 0;
    case Gb::DebugAddImage: t->mDebugWindow->AddComponent(new ImageComponent(t->mBGImage.get())); return 0;
    case Gb::ScreenRect: comp->GetRoot()->GetRect(*(WONRectangle*)c, false); return 0;
    case Gb::DebugPos: t->mDebugWindow->SetPosSize((int)c, (int)d, -1, -1); return 0;
    case Gb::DebugInvalidate: t->mDebugWindow->mRoot.get()->Invalidate(); return 0;
    default: return 0;
    }
}

// Creates a grabber with no captured background and a half-strength translucent tint (the Rust grabber's state)
BackgroundGrabber::BackgroundGrabber()
{
    this->mRust = ee_won_grab_new(this, ee_won_guicpp_grab_call);
}

// Destroys the grabber: the Rust grabber goes, then the image and the debug window
BackgroundGrabber::~BackgroundGrabber()
{
    ee_won_grab_free(this->mRust);
}
