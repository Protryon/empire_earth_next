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
// R6 WON rounds 33-34: the Rust manager's calls out (EeWonGuiOp's Wm* ops; rust/crates/ee_won/src/wmobj.rs,
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

// FUNCTION: WON 0x0072b660
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
// FUNCTION: WON 0x0072ba90
// Deleting destructor of the window manager (compiler-generated)
// (??_GWindowManager@WONAPI@@UAEPAXI@Z)

// FUNCTION: WON 0x0072bab0
// Destroys the named-font map tree, freeing every node
// (??1?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE@XZ)

// FUNCTION: WON 0x0072bb70
// Destroys the native image cache tree, freeing every node
// (??1?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE@XZ)

// FUNCTION: WON 0x0072bc30
// Destroys the raw image cache tree, freeing every node
// (??1?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE@XZ)

// FUNCTION: WON 0x0072bcf0
// Destroys the delay-load image cache tree, freeing every node
// (??1?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE@XZ)

// FUNCTION: WON 0x0072bdb0
// Destroys the window set tree, freeing every node
// (??1?$_Tree@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@std@@@std@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE@XZ)

// FUNCTION: WON 0x0072be70
// Destroys the timer event set tree, freeing every node
// (??1?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE@XZ)

// FUNCTION: WON 0x0072bf30
// Destroys the broadcast component set tree, freeing every node
// (??1?$_Tree@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE@XZ)

// FUNCTION: WON 0x0072bff0
// Destroys the window manager after closing every open window
WindowManager::~WindowManager()
{
	this->CloseAllWindows();
}

// FUNCTION: WON 0x0072c230
// Fills the colour scheme with the 17 standard Windows-style colours (face, shadow, highlight, text, links ...)
void WindowManager::InitColorScheme()
{
	this->mColorScheme->SetStandardColor(StandardColor_3DDarkShadow,0x000000);
	this->mColorScheme->SetStandardColor(StandardColor_3DFace,0xD0D0D0);
	this->mColorScheme->SetStandardColor(StandardColor_3DHilight,0xFFFFFF);
	this->mColorScheme->SetStandardColor(StandardColor_3DShadow,0x848484);
	this->mColorScheme->SetStandardColor(StandardColor_Scrollbar,0xDEDEDE);

	this->mColorScheme->SetStandardColor(StandardColor_ButtonText,0x000000);
	this->mColorScheme->SetStandardColor(StandardColor_GrayText,0x848484);
	this->mColorScheme->SetStandardColor(StandardColor_Hilight,0x08246B);
	this->mColorScheme->SetStandardColor(StandardColor_HilightText,0xFFFFFF);

	this->mColorScheme->SetStandardColor(StandardColor_ToolTipBack,0xFFFFDD);
	this->mColorScheme->SetStandardColor(StandardColor_ToolTipText,0x000000);

	this->mColorScheme->SetStandardColor(StandardColor_MenuBack,0xD0D0D0);
	this->mColorScheme->SetStandardColor(StandardColor_MenuText,0);

	this->mColorScheme->SetStandardColor(StandardColor_Back,0xFFFFFF);
	this->mColorScheme->SetStandardColor(StandardColor_Text,0x000000);

	this->mColorScheme->SetStandardColor(StandardColor_Link,0x0000FF);
	this->mColorScheme->SetStandardColor(StandardColor_LinkDown,0xFF0000);
}

// FUNCTION: WON 0x0072c2d0
// Records that no window has focus and when focus was lost (tick 0 is stored as 1)
void WindowManager::NotifyLostFocus()
{
	this->WmCall(EeWonWmCall::NotifyLostFocus);
}

// FUNCTION: WON 0x0072c300
// Records the window that gained focus and clears the lost-focus tick
void WindowManager::NotifyGotFocus(Window *theWindow)
{
	this->WmCall(EeWonWmCall::NotifyGotFocus, (intptr_t)theWindow);
}

// FUNCTION: WON 0x0072c320
// Switches the screen resolution and remembers the previous one; only when both width and height differ
// from the current mode
void WindowManager::SetScreenResolution(int theWidth, int theHeight)
{
	this->WmCall(EeWonWmCall::SetScreenResolution, theWidth, theHeight);
}

// FUNCTION: WON 0x0072c390
// Minimizes open windows that ask for it and switches back to the remembered screen resolution, if any
void WindowManager::RestoreScreenResolution()
{
	this->WmCall(EeWonWmCall::RestoreScreenResolution);
}

// FUNCTION: WON 0x0072c440
// Returns the window of the first open popup, or NULL when no popup is up
Window* WindowManager::GetPopupWindow()
{
	return (Window*)this->WmCall(EeWonWmCall::PopupWindow);
}

// FUNCTION: WON 0x0072c470
// Stores a font under a name (case-insensitive), replacing any earlier one
void WindowManager::SetNamedFont(const std::string &theName, Font *theFont)
{
	this->WmCall(EeWonWmCall::SetNamedFont, (intptr_t)theName.data(), (intptr_t)theName.size(), (intptr_t)theFont);
}

// FUNCTION: WON 0x0072c4d0
// Returns the font stored under a name, or the default font when there is none
Font* WindowManager::GetNamedFont(const std::string &theName)
{
	return (Font*)this->WmCall(EeWonWmCall::GetNamedFont, (intptr_t)theName.data(), (intptr_t)theName.size());
}

// FUNCTION: WON 0x0072c540
// Returns the default font, creating Tahoma 8 on the default window the first time
Font* WindowManager::GetDefaultFont()
{
	return (Font*)this->WmCall(EeWonWmCall::GetDefaultFont);
}

// FUNCTION: WON 0x0072c700
// Returns the default font with another style and, when the size is positive, another size
Font* WindowManager::GetDefaultFontMod(int theStyle, int theSize)
{
	Font *aFont = GetDefaultFont();
	FontDescriptor aDesc = aFont->mDescriptor;
	aDesc.mStyle = theStyle;
	if(theSize>0)
		aDesc.mSize = theSize;

	return this->GetDefaultWindow()->GetFont(aDesc).get();
}

// FUNCTION: WON 0x0072c7f0
// Adds or drops one image-caching request; when the count reaches 0 it empties the native and raw image caches
void WindowManager::SetCacheImages(bool cache)
{
	this->WmCall(EeWonWmCall::SetCacheImages, cache ? 1 : 0);
}

// FUNCTION: WON 0x0072c950
// Decodes an image file for a display context, cached while caching is on; NULL without a context
SmartPtr<NativeImage> WindowManager::DecodeImage(DisplayContext *theContext, const char *theFilePath)
{
	// the Rust's answer carries the returned SmartPtr's reference
	SmartPtr<NativeImage> anImage;
	anImage.mPtr = (NativeImage*)this->WmCall(EeWonWmCall::DecodeImage, (intptr_t)theContext, (intptr_t)theFilePath,
	                                          (intptr_t)strlen(theFilePath));
	return anImage;
}

// FUNCTION: WON 0x0072cc20
// Destroys a native image cache entry (file name and image)
// (??1?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@QAE@XZ)

// FUNCTION: WON 0x0072ccb0
// Decodes an image file for the default window's display context; NULL when there is no window
SmartPtr<NativeImage> WindowManager::DecodeImage(const char *theFilePath)
{
	if(this->GetDefaultWindow()==NULL)
		return NULL;

	return this->DecodeImage(this->GetDefaultWindow()->mDisplayContext.get(),theFilePath);
}

// FUNCTION: WON 0x0072ccf0
// Returns an image that loads its file on first use, sized from the file's attributes; cached by path while
// caching is on; NULL when the attributes cannot be read
SmartPtr<Image> WindowManager::DecodeDelayImage(const char *theFilePath)
{
	SmartPtr<Image> anImage;
	anImage.mPtr = (Image*)this->WmCall(EeWonWmCall::DecodeDelayImage, (intptr_t)theFilePath, (intptr_t)strlen(theFilePath));
	return anImage;
}

// FUNCTION: WON 0x0072d0d0
// Destroys a delay-load image cache entry (file name and image)
// (??1?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@QAE@XZ)

// FUNCTION: WON 0x0072d160
// Decodes an image file into a raw image, through the raw image cache while caching is on
SmartPtr<RawImage> WindowManager::DecodeImageRaw(const char *theFilePath)
{
	SmartPtr<RawImage> anImage;
	anImage.mPtr = (RawImage*)this->WmCall(EeWonWmCall::DecodeImageRaw, (intptr_t)theFilePath, (intptr_t)strlen(theFilePath));
	return anImage;
}

// FUNCTION: WON 0x0072d400
// Destroys a raw image cache entry (file name and image)
// (??1?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@QAE@XZ)

// FUNCTION: WON 0x0072d490
// Sets the decoder used for sounds
void WindowManager::SetSoundDecoder(SoundDecoder *theDecoder)
{
	this->mSoundDecoder = theDecoder;
}

// FUNCTION: WON 0x0072d4f0
// Decodes a sound with the sound decoder, or returns a plain sound for the descriptor when there is none
SmartPtr<Sound> WindowManager::DecodeSound(const SoundDescriptor &theDesc)
{
	if(this->mSoundDecoder.get()==NULL)
		return new Sound(theDesc);
	else
		return this->mSoundDecoder->DecodeDescriptor(theDesc);
}

// FUNCTION: WON 0x0072d5b0
// Closes every window, processes the resulting events and restores the screen resolution when full screen
void WindowManager::CloseAllWindows()
{
	this->WmCall(EeWonWmCall::CloseAllWindows);
}

// FUNCTION: WON 0x0072d6a0
// Adds a window to the managed set (the first one becomes the default window); always returns true
bool WindowManager::AddWindow(Window *theWindow)
{
	return this->WmCall(EeWonWmCall::AddWindow, (intptr_t)theWindow) != 0;
}

// FUNCTION: WON 0x0072d840
// Removes a window: drops its queued events, picks a new default window, ends its modal dialog and forgets
// it as the saved popup window; false when it was not managed
bool WindowManager::RemoveWindow(Window *theWindow)
{
	return this->WmCall(EeWonWmCall::RemoveWindow, (intptr_t)theWindow) != 0;
}

// FUNCTION: WON 0x0072ddf0
// Makes the innermost message loop return
void WindowManager::ExitMessageLoop()
{
	this->WmCall(EeWonWmCall::ExitMessageLoop);
}

// FUNCTION: WON 0x0072e7d0
// Runs a nested message loop until it is exited; returns the dialog result, or -1 when there are no windows
int WindowManager::MessageLoop()
{
	return (int)this->WmCall(EeWonWmCall::MessageLoop);
}

// FUNCTION: WON 0x0072e850
// Queues a window event and signals the event loop when asked or when no message loop is running
void WindowManager::PostEvent(WindowEventBase *theEvent, bool needSignal)
{
	this->WmCall(EeWonWmCall::PostEvent, (intptr_t)theEvent, needSignal ? 1 : 0);
}

// FUNCTION: WON 0x0072e960
// Adds an event to be delivered on every timer tick
void WindowManager::AddTimerEvent(WindowEventBase *theEvent)
{
	this->WmCall(EeWonWmCall::AddTimerEvent, (intptr_t)theEvent);
}

// FUNCTION: WON 0x0072ea70
// Removes a timer event, deferring the removal while timers are being delivered
void WindowManager::RemoveTimerEvent(WindowEventBase *theEvent)
{
	this->WmCall(EeWonWmCall::RemoveTimerEvent, (intptr_t)theEvent);
}

// FUNCTION: WON 0x0072f3f0
// Adds a component to or removes it from the set that receives broadcast events
void WindowManager::SubscribeToBroadcast(Component *theComponent, bool subscribe)
{
	this->WmCall(EeWonWmCall::SubscribeToBroadcast, (intptr_t)theComponent, subscribe ? 1 : 0);
}

// FUNCTION: WON 0x0072f5d0
// Removes every component from the broadcast set
void WindowManager::ClearSubscriptions()
{
	this->WmCall(EeWonWmCall::ClearSubscriptions);
}

// FUNCTION: WON 0x0072f680
// Sends a component event to every subscribed component
void WindowManager::BroadcastEvent(ComponentEvent *theEvent)
{
	this->WmCall(EeWonWmCall::BroadcastEvent, (intptr_t)theEvent);
}

// FUNCTION: WON 0x0072f700
// Returns the next queued window event without removing it, or NULL
WindowEventBase* WindowManager::PeekNextEvent()
{
	return (WindowEventBase*)this->WmCall(EeWonWmCall::PeekNextEvent);
}

// FUNCTION: WON 0x0072f760
// Every 50 ms ends or keeps popups by focus, restores the resolution after focus loss and ticks windows and
// timer events; then delivers queued events and idles every window (deferred timer removals are never cleared)
void WindowManager::ProcessEvents()
{
	this->WmCall(EeWonWmCall::ProcessEvents);
}

// FUNCTION: WON 0x0072da30
// Recomputes modal state from the open popups and the modal dialog: flags popups, the popup input component
// and the dialog, and makes every other window modal
void WindowManager::CalcModality()
{
	this->WmCall(EeWonWmCall::CalcModality);
}

// FUNCTION: WON 0x0072dc30
// Returns true when a modal window exists and the given window is not it
bool WindowManager::NotModalWindow(Window *theWindow)
{
	return this->WmCall(EeWonWmCall::NotModalWindow, (intptr_t)theWindow) != 0;
}

// FUNCTION: WON 0x0072dc50
// Shows a component (default: the window's root) as a modal dialog until it ends; returns its result
int WindowManager::DoDialog(Window *theWindow, Component *theComponent)
{
	return (int)this->WmCall(EeWonWmCall::DoDialog, (intptr_t)theWindow, (intptr_t)theComponent);
}

// FUNCTION: WON 0x0072ddb0
// Ends the current modal dialog with a result, unless another window or dialog is named
void WindowManager::EndDialog(int theResult, Window *theWindow, Component *theDialog)
{
	this->WmCall(EeWonWmCall::EndDialog, theResult, (intptr_t)theWindow, (intptr_t)theDialog);
}

// FUNCTION: WON 0x0072de00
// Moves a popup back inside the screen or the parent's root, flipping it to the other side of the avoid rectangle
void WindowManager::AdjustPopupPosition(Window *theParent, Component *theComponent, bool inOwnWindow, const WONRectangle &theAvoidRect)
{
	EeWonRect4 anAvoid = WinRect4(theAvoidRect);
	this->WmCall(EeWonWmCall::AdjustPopupPosition, (intptr_t)theParent, (intptr_t)theComponent, inOwnWindow ? 1 : 0,
	             (intptr_t)&anAvoid);
}

// FUNCTION: WON 0x0072e040
// Opens a popup at the given or mouse position, in its own popup window or inside the parent window, per
// the popup flags; ignored while another popup opens or closes, or when the parent lacks focus
void WindowManager::DoPopup(const PopupParams &theParams)
{
	EeWonWmPopup aParams = { (intptr_t)theParams.mParent, (intptr_t)theParams.mPopup, (intptr_t)theParams.mInputComponent,
	                         (intptr_t)theParams.mOwnerComponent, theParams.mFlags, theParams.mx, theParams.my,
	                         WinRect4(theParams.mAvoidRect) };
	this->WmCall(EeWonWmCall::DoPopup, (intptr_t)&aParams);
}

// FUNCTION: WON 0x0072e4d0
// Closes the given popup and every popup opened after it (NULL: all), firing the end-popup event and
// keeping the last popup window for reuse
void WindowManager::EndPopup(Component *thePopup)
{
	this->WmCall(EeWonWmCall::EndPopup, (intptr_t)thePopup);
}

// FUNCTION: WON 0x0072e790
// Closes the popups when the popup's owner component lies inside the given component
void WindowManager::CheckEndPopup(Component *theAncestor)
{
	this->WmCall(EeWonWmCall::CheckEndPopup, (intptr_t)theAncestor);
}

// FUNCTION: WON 0x0072ef20
// Dispatches one window event: mouse, key, focus, resize, close and other events, applying popup, modal
// and full-screen rules; merges repeated mouse moves and size changes
void WindowManager::DeliverEvent(WindowEventBase *theEvent)
{
	this->WmCall(EeWonWmCall::DeliverEvent, (intptr_t)theEvent);
}

// Class statics: the default window manager and the GUI thread id (constant-initialised)
WindowManager* WindowManager::mDefaultWindowManager = NULL;
DWORD WindowManager::mThreadId = 0;

// Not identified in the original exe: GetNamedFontMod (a named font with another style/size), GetDefaultFont
// for a descriptor, GetDefaultDisplayContext, CheckModalValid (the modal window is open and visible and the
// dialog is shown in it) and RemoveAllTimerEvents
Font* WindowManager::GetNamedFontMod(const std::string& theName, int theStyle, int theSize)
{
    Font* aFont = this->GetNamedFont(theName);
    FontDescriptor aDesc = aFont->GetDescriptor();
    aDesc.mStyle = theStyle;
    if(theSize > 0)
        aDesc.mSize = theSize;

    return this->GetDefaultWindow()->GetFont(aDesc).get();
}

SmartPtr<Font> WindowManager::GetDefaultFont(const FontDescriptor& theDescriptor)
{
    Window* aWindow = Window::GetDefaultWindow();
    if(aWindow == NULL)
        return NULL;
    else
        return aWindow->GetFont(theDescriptor);
}

DisplayContext* WindowManager::GetDefaultDisplayContext()
{
    Window* aWindow = Window::GetDefaultWindow();
    if(aWindow == NULL)
        return NULL;
    else
        return aWindow->GetDisplayContext();
}

bool WindowManager::CheckModalValid()
{
    return this->WmCall(EeWonWmCall::CheckModalValid) != 0;
}

void WindowManager::RemoveAllTimerEvents()
{
    this->WmCall(EeWonWmCall::RemoveAllTimerEvents);
}

// Runs one pass of the event loop (not identified in the original exe)
void WindowManager::PumpOnce()
{
	this->GetNextEvent();
}

// FUNCTION: WON 0x0072fc40
// Destroys a named-font map entry (name and font)
// (??1?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@QAE@XZ)

// FUNCTION: WON 0x0072fcd0
// Links a new node into the native image cache tree and rebalances it
// (?_Insert@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAE?AViterator@12@_NPAU_Node@12@ABU?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@2@@Z)

// FUNCTION: WON 0x0072ffd0
// Links a new node into the raw image cache tree and rebalances it
// (?_Insert@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAE?AViterator@12@_NPAU_Node@12@ABU?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@2@@Z)

// FUNCTION: WON 0x007302d0
// Links a new node into the delay-load image cache tree and rebalances it
// (?_Insert@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAE?AViterator@12@_NPAU_Node@12@ABU?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@2@@Z)

// FUNCTION: WON 0x007305d0
// Erases a range of entries from the window set (the whole tree at once when the range is everything)
// (?erase@?$_Tree@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@std@@@std@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@V312@0@Z)

// FUNCTION: WON 0x00730680
// Returns the range of timer events equal to the given event
// (?equal_range@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AU?$pair@Viterator@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@V123@@2@ABV?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@Z)

// FUNCTION: WON 0x007306e0
// Links a new node into the window set tree and rebalances it
// (?_Insert@?$_Tree@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@std@@@std@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@5@$0A@@std@@IAE?AViterator@12@_NPAU_Node@12@ABV?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@Z)

// FUNCTION: WON 0x007309e0
// Advances a broadcast component set iterator to the next entry
// (?_Inc@const_iterator@?$_Tree@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@$0A@@std@@QAEXXZ)

// FUNCTION: WON 0x00730a30
// Erases a range of entries from the timer event set (the whole tree at once when the range is everything)
// (?erase@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@V312@0@Z)

// FUNCTION: WON 0x00730ae0
// Links a new node into the timer event set tree and rebalances it
// (?_Insert@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@IAE?AViterator@12@_NPAU_Node@12@ABV?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@Z)

// FUNCTION: WON 0x00730de0
// Erases a range of entries from the broadcast component set (the whole tree at once when the range is everything)
// (?erase@?$_Tree@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@V312@0@Z)

// FUNCTION: WON 0x00730e90
// Links a new node into the broadcast component set tree and rebalances it
// (?_Insert@?$_Tree@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@$0A@@std@@IAE?AViterator@12@_NPAU_Node@12@ABV?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@Z)

// FUNCTION: WON 0x007318f0
// Builds the (position, inserted) result of a native image cache insert
// (??0?$pair@Viterator@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$_Mapk@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@_N@std@@QAE@ABViterator@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$_Mapk@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@1@AB_N@Z)

// FUNCTION: WON 0x00731a60
// Erases one entry from the named-font map and rebalances the tree; returns the next entry
// (?erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE?AViterator@12@V312@@Z)

// FUNCTION: WON 0x00731f80
// Frees a subtree of the named-font map recursively, without rebalancing
// (?_Erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAEXPAU_Node@12@@Z)

// FUNCTION: WON 0x00731fc0
// Links a new node into the named-font map tree and rebalances it
// (?_Insert@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAE?AViterator@12@_NPAU_Node@12@ABU?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@2@@Z)

// FUNCTION: WON 0x007322e0
// Erases one entry from the native image cache and rebalances the tree; returns the next entry
// (?erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE?AViterator@12@V312@@Z)

// FUNCTION: WON 0x00732800
// Frees a subtree of the native image cache recursively, without rebalancing
// (?_Erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAEXPAU_Node@12@@Z)

// FUNCTION: WON 0x00732840
// Erases one entry from the raw image cache and rebalances the tree; returns the next entry
// (?erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE?AViterator@12@V312@@Z)

// FUNCTION: WON 0x00732d60
// Frees a subtree of the raw image cache recursively, without rebalancing
// (?_Erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAEXPAU_Node@12@@Z)

// FUNCTION: WON 0x00732da0
// Erases one entry from the delay-load image cache and rebalances the tree; returns the next entry
// (?erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE?AViterator@12@V312@@Z)

// FUNCTION: WON 0x007332c0
// Frees a subtree of the delay-load image cache recursively, without rebalancing
// (?_Erase@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAEXPAU_Node@12@@Z)

// FUNCTION: WON 0x00733300
// Erases one entry from the window set and rebalances the tree; returns the next entry
// (?erase@?$_Tree@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@std@@@std@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@V312@@Z)

// FUNCTION: WON 0x007337f0
// Frees a subtree of the window set recursively, without rebalancing
// (?_Erase@?$_Tree@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@std@@@std@@UComp@12@V?$allocator@V?$SmartPtr@VWindow@WONAPI@@@WONAPI@@@5@$0A@@std@@IAEXPAU_Node@12@@Z)

// FUNCTION: WON 0x00733830
// Grows the window event list's element count, throwing length_error when it would overflow
// (?_Incsize@?$list@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@@std@@IAEXI@Z)

// FUNCTION: WON 0x00733980
// Erases one entry from the timer event set and rebalances the tree; returns the next entry
// (?erase@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@V312@@Z)

// FUNCTION: WON 0x00733e70
// Frees a subtree of the timer event set recursively, without rebalancing
// (?_Erase@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@IAEXPAU_Node@12@@Z)

// FUNCTION: WON 0x00733eb0
// Erases one entry from the broadcast component set and rebalances the tree; returns the next entry
// (?erase@?$_Tree@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@V12@U_Kfn@?$set@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@V312@@Z)

// FUNCTION: WON 0x007343a0
// Returns the first timer event not ordered before the given one
// (?lower_bound@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@ABV?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@Z)

// FUNCTION: WON 0x007343e0
// Returns the first timer event ordered after the given one
// (?upper_bound@?$_Tree@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@5@$0A@@std@@QAE?AViterator@12@ABV?$SmartPtr@VWindowEventBase@WONAPI@@@WONAPI@@@Z)

// FUNCTION: WON 0x00734420
// Frees a subtree of the broadcast component set recursively, without rebalancing
// (?_Erase@?$_Tree@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@V12@U_Kfn@?$_Setk@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@std@@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@4@@std@@U?$less@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@V?$allocator@V?$SmartPtr@VComponent@WONAPI@@@WONAPI@@@5@$0A@@std@@IAEXPAU_Node@12@@Z)

// FUNCTION: WON 0x00734f70
// Advances a named-font map iterator to the next entry
// (?_Inc@const_iterator@?$_Tree@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@2@U_Kfn@?$map@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@UStringLessNoCase@4@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@@2@UStringLessNoCase@WONAPI@@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAEXXZ)

// FUNCTION: WON 0x007352a0
// Deleting destructor of a named-font map entry
// (??_G?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VFont@WONAPI@@@WONAPI@@@std@@QAEPAXI@Z)

// FUNCTION: WON 0x007352c0
// Deleting destructor of a native image cache entry
// (??_G?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VNativeImage@WONAPI@@@WONAPI@@@std@@QAEPAXI@Z)

// FUNCTION: WON 0x007352e0
// Deleting destructor of a raw image cache entry
// (??_G?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VRawImage@WONAPI@@@WONAPI@@@std@@QAEPAXI@Z)

// FUNCTION: WON 0x00735300
// Deleting destructor of a delay-load image cache entry
// (??_G?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$SmartPtr@VImage@WONAPI@@@WONAPI@@@std@@QAEPAXI@Z)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0072fab0
// (public: class WONAPI::SmartPtr<class WONAPI::Font> & __thiscall std::map<class std::basic_string<char,struct std::char_traits<char>,class std::allocat)
// TEMPLATE: WON 0x00735010
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WON)
// TEMPLATE: WON 0x00735090
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WON)
// TEMPLATE: WON 0x00735110
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WON)
// TEMPLATE: WON 0x00735220
// (void __cdecl std::_Construct(struct std::pair<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const ,class WON)
