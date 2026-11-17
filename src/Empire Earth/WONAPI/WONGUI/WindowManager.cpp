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
#include "ee/wonapi/WONLobbyDelayLoadImage.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/ee_won.h"
using namespace WONAPI;
// The popup stack, the modal dialog, the message loop, focus and the caches are ee_won's Rust manager (wmobj.rs,
// wmcache.rs; R6 WON round 33); the window set, the event list, the timers, the broadcasts, the event loop and the
// screen resolution are its wmloop.rs (round 34). This unit keeps the colour scheme, the decoders, the font
// helpers and the forwarders.
static EeWonRect4 WinRect4(const WONRectangle& inRect)
{
    EeWonRect4 aRect = { inRect.x, inRect.y, inRect.width, inRect.height };
    return aRect;
}
// R6 WON rounds 33-34: the Rust manager's calls out (EeWonGuiOp's Wm* ops; rust/crates/ee_won_gui/src/wmobj.rs,
// wmloop.rs)
typedef EeWonGuiOp Op;

static intptr_t WmCompCall(Component* t, intptr_t call, intptr_t c, intptr_t d)
{
    switch ((EeWonWmComp)call) {
    case EeWonWmComp::SetFlags: t->SetComponentFlags((int)c, d != 0); return 0;
    case EeWonWmComp::Parent: return (intptr_t)(Component*)t->mParent;
    case EeWonWmComp::EnterModal: static_cast<Container*>(t)->EnterModal(); return 0;
    case EeWonWmComp::LeaveModal: static_cast<Container*>(t)->LeaveModal(); return 0;
    case EeWonWmComp::RequestFocus: t->RequestFocus(); return 0;
    case EeWonWmComp::SetPos: t->SetPos((int)c, (int)d); return 0;
    case EeWonWmComp::Left: return t->Left();
    case EeWonWmComp::Top: return t->Top();
    case EeWonWmComp::Width: return t->Width();
    case EeWonWmComp::Height: return t->Height();
    case EeWonWmComp::Root: return (intptr_t)t->GetRoot();
    case EeWonWmComp::OwnScheme: return (intptr_t)t->GetOwnColorScheme();
    case EeWonWmComp::Scheme: return (intptr_t)t->GetColorScheme();
    case EeWonWmComp::SetScheme: t->SetColorScheme((ColorScheme*)c); return 0;
    case EeWonWmComp::FireEndPopup: t->Fn2F5380(ComponentEvent_EndPopup, 0); return 0;
    case EeWonWmComp::Bits: return (intptr_t)(uint32_t)t->mComponentBits;
    case EeWonWmComp::RootContainer: return (intptr_t)(Component*)t->GetRootContainer();
    case EeWonWmComp::ClickChildAt: return (intptr_t)static_cast<Container*>(t)->FindClickChildAt((int)c, (int)d);
    case EeWonWmComp::AsContainer: return (intptr_t)(Component*)dynamic_cast<Container*>(t);
    case EeWonWmComp::Hold: t->AddRef(); return 0;
    case EeWonWmComp::Release: t->Release(); return 0;
    case EeWonWmComp::MouseWheel: t->MouseWheel((int)c); return 0;
    case EeWonWmComp::HandleComponentEvent: t->HandleComponentEvent((ComponentEvent*)c); return 0;
    }
    return 0;
}

static intptr_t WmWinCall(Window* t, intptr_t call, intptr_t c, intptr_t d)
{
    switch ((EeWonWmWin)call) {
    case EeWonWmWin::SetFocus: t->PlatformSetFocus(); return 0;
    case EeWonWmWin::SetCapture: t->PlatformSetMouseCapture(c != 0); return 0;
    case EeWonWmWin::GetRect: t->GetRect(*(WONRectangle*)c, d != 0); return 0;
    case EeWonWmWin::IsPopup: return t->IsPopup() ? 1 : 0;
    case EeWonWmWin::SetIsPopup: t->SetIsPopup(c != 0); return 0;
    case EeWonWmWin::Show: t->Show(c != 0); return 0;
    case EeWonWmWin::Close: t->Close(); return 0;
    case EeWonWmWin::SetPosSize: {
        const WONRectangle& r = *(const WONRectangle*)c;
        t->SetPosSize(r.x, r.y, r.width, r.height);
        return 0;
    }
    case EeWonWmWin::Parent: return (intptr_t)t->GetParent();
    case EeWonWmWin::AddComponent: t->AddComponent((Component*)c); return 0;
    case EeWonWmWin::SetModal: t->SetModal(c != 0); return 0;
    case EeWonWmWin::IsMouseDown: return t->IsMouseDown((MouseButton)c) ? 1 : 0;
    case EeWonWmWin::MouseX: return t->GetMouseX();
    case EeWonWmWin::MouseY: return t->GetMouseY();
    case EeWonWmWin::Root: return (intptr_t)(Component*)t->mRoot.get();
    case EeWonWmWin::RootLostFocus: t->mRoot->LostFocus(); return 0;
    case EeWonWmWin::RootGotFocus: t->mRoot->GotFocus(); return 0;
    case EeWonWmWin::RootRemoveAll: t->mRoot->RemoveAllChildren(); return 0;
    case EeWonWmWin::RootRemoveChild: t->mRoot->RemoveChild((Component*)c); return 0;
    case EeWonWmWin::State: return t->GetWindowState();
    case EeWonWmWin::IsVisible: return t->PlatformIsVisible() ? 1 : 0;
    case EeWonWmWin::Hold: t->AddRef(); return 0;
    case EeWonWmWin::Release: t->Release(); return 0;
    case EeWonWmWin::MinimizeOnRestore: return t->GetMinimizeOnResolutionRestore() ? 1 : 0;
    case EeWonWmWin::Minimize: t->Minimize(c != 0); return 0;
    case EeWonWmWin::InClient: return t->IsMouseInClientArea() ? 1 : 0;
    case EeWonWmWin::HandleMouse: t->HandleMouseEvent((MouseEvent*)c); return 0;
    case EeWonWmWin::HandleKey: t->HandleKeyEvent((KeyEvent*)c); return 0;
    case EeWonWmWin::HandleInput: t->HandleInputEvent((InputEvent*)c); return 0;
    case EeWonWmWin::PlatformClose: t->PlatformClose(); return 0;
    case EeWonWmWin::CloseEvent: return (intptr_t)t->mCloseEvent.get();
    case EeWonWmWin::TimerEvent: t->TimerEvent((DWORD)c); return 0;
    case EeWonWmWin::IdleUpdate: t->IdleUpdate(); return 0;
    }
    return 0;
}

// R6 WON round 34: a window event's fields and methods (EeWonGuiOp::WmEvent)
static intptr_t WmEventCall(WindowEventBase* t, intptr_t call, intptr_t c)
{
    switch ((EeWonWmEvent)call) {
    case EeWonWmEvent::Type: return (intptr_t)t->mEventType;
    case EeWonWmEvent::Window: return (intptr_t)t->mWindow;
    case EeWonWmEvent::SetWindow: t->mWindow = (Window*)c; return 0;
    case EeWonWmEvent::Mouse: {
        MouseEvent* m = static_cast<MouseEvent*>(t);
        EeWonWmMouse* o = (EeWonWmMouse*)c;
        o->window = (intptr_t)m->mWindow; o->x = m->mx; o->y = m->my; o->button = m->mButtonOrAmount;
        return 0;
    }
    case EeWonWmEvent::SetMouse: {
        MouseEvent* m = static_cast<MouseEvent*>(t);
        const EeWonWmMouse* o = (const EeWonWmMouse*)c;
        m->mWindow = (Window*)o->window; m->mx = o->x; m->my = o->y;
        return 0;
    }
    case EeWonWmEvent::Button: return static_cast<MouseEvent*>(t)->mButtonOrAmount;
    case EeWonWmEvent::Key: return static_cast<KeyEvent*>(t)->mKey;
    case EeWonWmEvent::Size: {
        SizeEvent* z = static_cast<SizeEvent*>(t);
        EeWonRect4 r = { z->mx, z->my, z->mWidth, z->mHeight };
        *(EeWonRect4*)c = r;
        return 0;
    }
    case EeWonWmEvent::SetSize: {
        SizeEvent* z = static_cast<SizeEvent*>(t);
        const EeWonRect4& r = *(const EeWonRect4*)c;
        z->mx = r.x; z->my = r.y; z->mWidth = r.w; z->mHeight = r.h;
        return 0;
    }
    case EeWonWmEvent::Deliver: t->Deliver(); return 0;
    case EeWonWmEvent::Hold: t->AddRef(); return 0;
    case EeWonWmEvent::Release: t->Release(); return 0;
    }
    return 0;
}

extern "C" intptr_t ee_won_guicpp_wm_call(void* inWm, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    WindowManager* t = (WindowManager*)inWm;
    switch (op) {
    case Op::WmTick: return (intptr_t)(uint32_t)WONTickCount();
    case Op::WmComp: return WmCompCall((Component*)b, a, c, d);
    case Op::WmWin: return WmWinCall((Window*)b, a, c, d);
    case Op::WmScreenRect: t->GetScreenRect(*(WONRectangle*)a); return 0;
    case Op::WmNewPopupWindow: {
        // DoPopup's `aPopupWindow = CreateNativeWindow();` and its set-up; the Rust gives the reference back
        Window* aPopupWindow = CreateNativeWindow();
        aPopupWindow->AddRef();
        aPopupWindow->SetOneComponentWindow(true);
        aPopupWindow->SetStyle(0x20);
        aPopupWindow->SetParent((Window*)a);
        aPopupWindow->SetIsPopup(true);
        aPopupWindow->Create(*(const WONRectangle*)b);
        return (intptr_t)aPopupWindow;
    }
    case Op::WmRef:
        switch ((EeWonWmRefKind)a) {
        case EeWonWmRefKind::Font: if (c) ((Font*)b)->AddRef(); else ((Font*)b)->Release(); return 0;
        case EeWonWmRefKind::NativeImage: if (c) ((NativeImage*)b)->AddRef(); else ((NativeImage*)b)->Release(); return 0;
        case EeWonWmRefKind::RawImage: if (c) ((RawImage*)b)->AddRef(); else ((RawImage*)b)->Release(); return 0;
        case EeWonWmRefKind::Image: if (c) ((Image*)b)->AddRef(); else ((Image*)b)->Release(); return 0;
        }
        return 0;
    case Op::WmDecode: {
        // the decoder's SmartPtr: its object answered with one reference
        std::string aPath((const char*)b, (size_t)c);
        SmartPtr<NativeImage> anImage = t->mImageDecoder.Decode((DisplayContext*)a, aPath.c_str());
        if (anImage.get() != NULL) anImage->AddRef();
        return (intptr_t)anImage.get();
    }
    case Op::WmDecodeRaw: {
        std::string aPath((const char*)a, (size_t)b);
        SmartPtr<RawImage> anImage = t->mImageDecoder.DecodeRaw(aPath.c_str());
        if (anImage.get() != NULL) anImage->AddRef();
        return (intptr_t)anImage.get();
    }
    case Op::WmDelayImage: {
        std::string aPath((const char*)a, (size_t)b);
        ImageAttributes anAttr;
        if (!t->mImageDecoder.GetImageAttributes(aPath.c_str(), anAttr))
            return 0;
        Image* anImage = new DelayLoadImage(aPath.c_str(), anAttr.mWidth, anAttr.mHeight);
        anImage->AddRef();
        return (intptr_t)anImage;
    }
    case Op::WmDefaultWindowFont: {
        SmartPtr<Font> aFont = t->GetDefaultWindow()->GetFont(FontDescriptor("Tahoma", 0, 8));
        if (aFont.get() != NULL) aFont->AddRef();
        return (intptr_t)aFont.get();
    }
    case Op::WmNextEvent: t->GetNextEvent(); return 0;
    case Op::WmSelfHold: t->AddRef(); return 0;
    case Op::WmSelfRelease: t->Release(); return 0;
    case Op::WmEvent: return WmEventCall((WindowEventBase*)b, a, c);
    case Op::WmResolution:
        if (a == 0) { t->GetScreenResolutionHook(*(int*)b, *(int*)c); return 0; }
        return t->SetScreenResolutionHook((int)b, (int)c) ? 1 : 0;
    case Op::WmSignal: t->Signal(); return 0;
    default: return 0;
    }
}

// Creates the window manager: becomes the default manager, sets standard colours. The original registered BMP and
// WIM decoders here; the port reads PNG and JPEG pictures only (D9; the lobby's game code registers both), so they
// went (round 18)
WindowManager::WindowManager() {
    this->mWmObj.p = ee_won_wm_new(this, ee_won_guicpp_wm_call);
    WindowManager::mThreadId=PlatCurrentThreadId();
    WindowManager::mDefaultWindowManager=this;
    this->WmSet(EeWonWmField::LastTimerTick, (intptr_t)(uint32_t)WONTickCount());
    this->mColorScheme=new ColorScheme;
    this->InitColorScheme();
    // The window set, the events, the timers, the broadcasts and the screen state start empty in the Rust manager
    // (wmloop.rs); the original left the image cache count and the full-screen size unset
}

// Destroys the window manager after closing every open window
WindowManager::~WindowManager() { this->CloseAllWindows(); }

// Fills the colour scheme with the 17 standard Windows-style colours (wmobj.rs's table)
void WindowManager::InitColorScheme()
{
    uint32_t aColors[StandardColor_Max];
    ee_won_wm_standard_colors(aColors);
    for (int i = 0; i < StandardColor_Max; i++)
        this->mColorScheme->SetStandardColor(i, aColors[i]);
}

// A font with another style and, when the size is positive, another size, from the default window (GetDefaultFontMod,
// GetNamedFontMod)
static Font* FontMod(WindowManager* inManager, Font* inFont, int inStyle, int inSize)
{
    FontDescriptor aDesc = inFont->mDescriptor;
    aDesc.mStyle = inStyle;
    if (inSize > 0)
        aDesc.mSize = inSize;
    return inManager->GetDefaultWindow()->GetFont(aDesc).get();
}
Font* WindowManager::GetDefaultFontMod(int inStyle, int inSize) { return FontMod(this, this->GetDefaultFont(), inStyle, inSize); }
Font* WindowManager::GetNamedFontMod(const std::string& inName, int inStyle, int inSize)
{ return FontMod(this, this->GetNamedFont(inName), inStyle, inSize); }

// Decodes an image file for the default window's display context; NULL when there is no window
SmartPtr<NativeImage> WindowManager::DecodeImage(const char *theFilePath)
{
    Window* aWindow = this->GetDefaultWindow();
    return aWindow == NULL ? SmartPtr<NativeImage>() : this->DecodeImage(aWindow->mDisplayContext.get(), theFilePath);
}
// Decodes a sound with the sound decoder, or returns a plain sound for the descriptor when there is none
SmartPtr<Sound> WindowManager::DecodeSound(const SoundDescriptor &theDesc)
{
    return this->mSoundDecoder.get() == NULL ? SmartPtr<Sound>(new Sound(theDesc)) : this->mSoundDecoder->DecodeDescriptor(theDesc);
}
// Moves a popup back inside the screen or the parent's root, flipping it to the other side of the avoid rectangle
void WindowManager::AdjustPopupPosition(Window *theParent, Component *theComponent, bool inOwnWindow, const WONRectangle &theAvoidRect)
{
    EeWonRect4 anAvoid = WinRect4(theAvoidRect);
    this->WmCall(EeWonWmCall::AdjustPopupPosition, (intptr_t)theParent, (intptr_t)theComponent, inOwnWindow ? 1 : 0,
                 (intptr_t)&anAvoid);
}
// Opens a popup at the given or mouse position, in its own popup window or inside the parent window, per
// the popup flags; ignored while another popup opens or closes, or when the parent lacks focus
void WindowManager::DoPopup(const PopupParams &theParams)
{
    EeWonWmPopup aParams = { (intptr_t)theParams.mParent, (intptr_t)theParams.mPopup, (intptr_t)theParams.mInputComponent,
                             (intptr_t)theParams.mOwnerComponent, theParams.mFlags, theParams.mx, theParams.my,
                             WinRect4(theParams.mAvoidRect) };
    this->WmCall(EeWonWmCall::DoPopup, (intptr_t)&aParams);
}
// The default window's font for a descriptor and its display context (NULL without a window; not identified in the
// original exe)
SmartPtr<Font> WindowManager::GetDefaultFont(const FontDescriptor& theDescriptor)
{
    Window* aWindow = Window::GetDefaultWindow();
    return aWindow == NULL ? SmartPtr<Font>() : aWindow->GetFont(theDescriptor);
}
DisplayContext* WindowManager::GetDefaultDisplayContext()
{
    Window* aWindow = Window::GetDefaultWindow();
    return aWindow == NULL ? NULL : aWindow->GetDisplayContext();
}

// Class statics: the default window manager and the GUI thread id (constant-initialised)
WindowManager* WindowManager::mDefaultWindowManager = NULL;
DWORD WindowManager::mThreadId = 0;
