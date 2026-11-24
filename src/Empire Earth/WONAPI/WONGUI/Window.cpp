// Window (R6 WON round 32): the Rust Window's calls out (rust/crates/ee_won_gui/src/winobj.rs, EeWonGuiOp's Win* ops), its
// constructor and destructor, and the methods over the members that stay C++ (the fonts, the images). The original's
// Window.cpp (the input dispatch, the cursor, the resize, closing, centring, the child windows) is the Rust object.
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
#include "ee/wonapi/WONRouting.h" // WONTickCount
#include "rust/won_gui/fontmap.h"
#include "rust/won_gui/guihost.h"
#include "rust/won_gui/winobj.h"
#include "ee/wonapi/WONLobbyPlatform.h"
#include "rust/won_gui/platwin.h"
#include "rust/won_gui/platwm.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;

static intptr_t RootCall(RootContainer* r, intptr_t call, intptr_t b, intptr_t c, intptr_t d)
{
    switch ((EeWonWinRoot)call) {
    case EeWonWinRoot::SetSize: r->SetSize((int)b, (int)c); return 0;
    case EeWonWinRoot::Invalidate: r->Invalidate(); return 0;
    case EeWonWinRoot::MouseExit: r->MouseExit(); return 0;
    case EeWonWinRoot::KeyDown: r->KeyDown((int)b); return 0;
    case EeWonWinRoot::KeyUp: r->KeyUp((int)b); return 0;
    case EeWonWinRoot::KeyChar: r->KeyChar((int)b); return 0;
    case EeWonWinRoot::MouseDown: r->MouseDown((int)b, (int)c, (int)d); return 0;
    case EeWonWinRoot::MouseEnter: r->MouseEnter((int)b, (int)c); return 0;
    case EeWonWinRoot::MouseDrag: r->MouseDrag((int)b, (int)c); return 0;
    case EeWonWinRoot::MouseMove: r->MouseMove((int)b, (int)c); return 0;
    case EeWonWinRoot::MouseUp: r->MouseUp((int)b, (int)c, (int)d); return 0;
    case EeWonWinRoot::MouseWheel: r->MouseWheel((int)b); return 0;
    case EeWonWinRoot::GotFocus: r->GotFocus(); return 0;
    case EeWonWinRoot::LostFocus: r->LostFocus(); return 0;
    case EeWonWinRoot::MouseHover: r->MouseHover((DWORD)b); return 0;
    case EeWonWinRoot::TimerEvent: r->TimerEvent((DWORD)b); return 0;
    case EeWonWinRoot::CanClose: return r->CanClose() ? 1 : 0;
    case EeWonWinRoot::AddChild: r->AddChild((Component*)b); return 0;
    case EeWonWinRoot::RemoveChild: r->RemoveChild((Component*)b); return 0;
    case EeWonWinRoot::EnterModal: r->EnterModal(); return 0;
    case EeWonWinRoot::LeaveModal: r->LeaveModal(); return 0;
    case EeWonWinRoot::UnionBounds: r->UnionInvalidRect(r->GetBounds()); return 0;
    case EeWonWinRoot::CurrentCursor: return (intptr_t)r->GetCurrentCursor();
    case EeWonWinRoot::Left: return r->Left();
    case EeWonWinRoot::Top: return r->Top();
    case EeWonWinRoot::Width: return r->Width();
    case EeWonWinRoot::Height: return r->Height();
    case EeWonWinRoot::TimerFlags: return (intptr_t)r->mTimerFlags;
    }
    return 0;
}

// the font a run of ops shares (winobj.rs get_font: the original's local SmartPtr)
static SmartPtr<Font> sHostFont;

extern "C" intptr_t ee_won_guicpp_win_call(void* inWin, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    Window* t = (Window*)inWin;
    switch (op) {
    case Op::WinTick: return (intptr_t)(uint32_t)WONTickCount();
    case Op::WinRoot: return RootCall(t->mRoot.get(), a, b, c, d);
    case Op::WinPlat:
        switch ((EeWonWinPlat)a) {
        case EeWonWinPlat::SetTitle: t->PlatformSetTitle(); return 0;
        case EeWonWinPlat::SetIcon: t->PlatformSetIcon(); return 0;
        case EeWonWinPlat::SetCursor: t->PlatformSetCursor((Cursor*)b); return 0;
        case EeWonWinPlat::ShowNativeCursor: t->PlatformShowNativeCursor(b != 0); return 0;
        case EeWonWinPlat::Show: t->PlatformShow(b != 0); return 0;
        case EeWonWinPlat::Minimize: t->PlatformMinimize(b != 0); return 0;
        case EeWonWinPlat::SetFocus: t->PlatformSetFocus(); return 0;
        }
        return 0;
    case Op::WinSetPosSize: t->SetPosSize((int)a, (int)b, (int)c, (int)d); return 0;
    case Op::WinGetRect: ((Window*)a)->GetRect(*(WONRectangle*)b, c != 0); return 0;
    case Op::WinIsVisible: return ((Window*)a)->PlatformIsVisible() ? 1 : 0;
    case Op::WinScreenRect: t->mManager->GetScreenRect(*(WONRectangle*)a); return 0;
    case Op::WinAddToManager: t->mManager->AddWindow(t); return 0;
    case Op::WinNewOffscreen: {
        SmartPtr<NativeImage> anImage = t->CreateImage((int)a, (int)b);
        Graphics &g = anImage->GetGraphics();
        g.DrawImage(t->mOffscreenImage.get(), 0, 0);
        t->mOffscreenImage = anImage;
        return 0;
    }
    case Op::WinQueueClose: t->QueueEvent(new InputEvent(InputEvent_WindowClose)); return 0;
    case Op::WinCanClose: return ((Window*)a)->CanClose() ? 1 : 0;
    case Op::WinAllowClose: return (t->mAllowCloseFunc != NULL && !t->mAllowCloseFunc(t)) ? 0 : 1;
    case Op::WinChildSet: ((Window*)a)->WCall(EeWonWinCall::ChildSet, (intptr_t)t, b); return 0;
    case Op::WinStdArrow: return (intptr_t)Cursor::GetStandardCursor(StandardCursor_Arrow);
    case Op::WinCursorNative: return (intptr_t)((Cursor*)a)->GetNative();
    case Op::WinEvType: return ((InputEvent*)a)->mEventType;
    case Op::WinKeyEvKeyMod: return static_cast<KeyEvent*>((InputEvent*)a)->mKeyMod;
    case Op::WinKeyEvKey: return static_cast<KeyEvent*>((InputEvent*)a)->mKey;
    case Op::WinMouseEvX: return static_cast<MouseEvent*>((InputEvent*)a)->mx;
    case Op::WinMouseEvY: return static_cast<MouseEvent*>((InputEvent*)a)->my;
    case Op::WinMouseEvKeyMod: return static_cast<MouseEvent*>((InputEvent*)a)->mKeyMod;
    case Op::WinMouseEvButton: return static_cast<MouseEvent*>((InputEvent*)a)->mButtonOrAmount;
    case Op::WinSizeEvWidth: return static_cast<SizeEvent*>((InputEvent*)a)->mWidth;
    case Op::WinSizeEvHeight: return static_cast<SizeEvent*>((InputEvent*)a)->mHeight;
    case Op::WinPlatformFont: sHostFont = t->PlatformGetFont(*(const FontDescriptor*)a); return 0;
    case Op::WinHostFont: return (intptr_t)sHostFont.get();
    case Op::WinFontHold: ((Font*)a)->AddRef(); return 0;
    case Op::WinHostFontClear: sHostFont = SmartPtr<Font>(); return 0;
    // (no window has an icon: the manager's default was never set either; R6 WON GUI round 35)
    case Op::WinHasIcon: return 0;
    case Op::WinIconDefault: return 0;
    default: return 0;
    }
}

// R6 WON GUI round 35: the platform window (the port's PlatGUIWindow.cpp, folded into Window)
namespace WONAPI {
// The Rust window's calls on its shell (EeWonPlatWinOp), each the original's statement
static intptr_t PlatWinHost(void* inMe, EeWonPlatWinOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    Window* me = (Window*)inMe;
    WindowManager* m = me->GetPlatManager();
    typedef EeWonPlatWinOp Op;
    switch (inOp) {
    case Op::HasWm: return m != NULL;
    case Op::Wm: return m->Call((EeWonPlatWmCall)a, b, c);
    case Op::Mouse: return m->GetMousePosition(((int*)a)[0], ((int*)a)[1]);
    case Op::Fills: return me->FillsSurface();
    case Op::Frame: return me->HasFrame();
    case Op::Popup: return me->IsPopup();
    case Op::Open: return me->GetWindowState() == Window::WindowState_Open;
    case Op::Style: return me->GetStyle();
    case Op::WindowCreate:
        me->BaseCreate((int)c, (int)d);
        me->mDisplayContext = new DisplayContext(NULL);
        me->mOffscreenImage = me->CreateImage(me->mRoot->Width(), me->mRoot->Height());
        return 0;
    case Op::Center: me->Center(a != 0); return 0;
    case Op::Showing: me->SetIsShowing(a != 0); return 0;
    case Op::Show: me->PlatformShow(a != 0); return 0;
    case Op::QueueSize: me->QueueEvent(new SizeEvent((int)a, (int)b, (int)c, (int)d)); return 0;
    case Op::QueueExit: me->QueueEvent(new InputEvent(InputEvent_MouseExit)); return 0;
    case Op::QueueMouse: {
        const int32_t* p = (const int32_t*)a;
        me->QueueEvent(new MouseEvent(p[0], p[1], p[2], p[3], p[4]));
        return 0;
    }
    case Op::InvalidateRoot: me->InvalidateRoot(); return 0;
    case Op::MouseIn: return me->IsMouseInClientArea();
    case Op::RootContains: return me->mRoot->Contains((int)a, (int)b);
    case Op::Paint:
        if (me->mRoot->mInvalidState == 0 || me->mOffscreenImage.get() == NULL)
            return 0;
        me->mRoot->PrePaint(me->mOffscreenImage->GetGraphics());
        me->mRoot->Paint(me->mOffscreenImage->GetGraphics());
        return 1;
    case Op::ParentVisible: {
        Window* theParent = me->GetParent();
        return theParent != NULL && theParent->Rust(EeWonPlatWinCall::IsVisible) ? (intptr_t)theParent : 0;
    }
    case Op::WindowClose: me->BasePlatformClose(); return 0;
    case Op::WindowTimer: me->BaseTimerEvent((DWORD)a); return 0;
    case Op::ApplyCursor: ee_won_cursor_apply(a != 0 ? ((Cursor*)a)->mRust : NULL, (void (*)(int32_t))PlatCursorShow); return 0;
    case Op::PopupHit: {
        Window* thePopup = m->GetPopupWindow();
        return thePopup != NULL && (m->GetPopupFlags() & PopupFlag_DontWantMouse) && thePopup == (Window*)a;
    }
    case Op::CursorShow: PlatCursorShow((int)a); return 0;
    case Op::PlatMinimize: PlatWindowMinimize((PlatWindow*)a); return 0;
    }
    return 0;
}
}

// Creates a closed window on the default window manager with a new root container and the title "Sierra App" (the
// Rust window: closed, hidden, no parent, the mouse outside, sizes 0..64000)
Window::Window()
{
	this->mWinObj.p = ee_won_win_new(this, ee_won_guicpp_win_call);
	this->mManager = WONAPI::WindowManager::GetDefaultWindowManager();
	this->mRoot = new RootContainer(this);
	this->mAllowCloseFunc = NULL;
	this->mTitle = "Sierra App";
	this->mRust = ee_won_plat_win_new(this, PlatWinHost);
}

// Destroys the window: the platform window, the members, then the Rust window (its child set; mWinObj is the first
// member)
Window::~Window()
{
	this->Rust(EeWonPlatWinCall::Destroy);
	ee_won_plat_win_free(this->mRust);
	this->mRust = NULL;
}

// Returns the cached font for a descriptor, creating it through the platform on first use (a NULL platform font
// is asked again at the next call; the map holds one reference to each font)
SmartPtr<Font> Window::GetFont(const FontDescriptor &theDescriptor)
{
	return (Font*)ee_won_win_get_font(this->mWinObj.p, this->mFontMap.p, (intptr_t)&theDescriptor, (const uint8_t*)theDescriptor.mFace.data(), theDescriptor.mFace.size(), theDescriptor.mStyle, theDescriptor.mSize);
}
Window::FontMap::~FontMap()
{
	for(intptr_t aFont; (aFont = ee_won_fontmap_pop(this->p)) >= 0; )
		if(aFont!=0) ((Font*)aFont)->Release();
	ee_won_fontmap_free(this->p);
}

// Sets the window title, updating the platform window when open
void Window::SetTitle(const GUIString &theTitle)
{
	this->mTitle = theTitle;
	this->WCall(EeWonWinCall::TitleChanged);
}

// Returns the default window manager's default window
Window* Window::GetDefaultWindow()
{
	return WONAPI::WindowManager::GetDefaultWindowManager()->GetDefaultWindow();
}

// Creates an image of the given size for the window's display
SmartPtr<NativeImage> Window::CreateImage(int theWidth, int theHeight)
{
	return this->mDisplayContext->CreateImage(theWidth,theHeight);
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
void Window::QueueEvent(WindowEventBase* event) { event->mWindow=this; this->mManager->PostEvent(event); }

// Not identified in the original exe
Graphics& Window::GetGraphics()
{
    return this->mOffscreenImage->GetGraphics();
}

// The handlers over the events' fields, read in Rust (winobj.rs ee_won_win_event; the others are inline)
void Window::HandleKeyEvent(KeyEvent *e)
{
    ee_won_win_event(this->mWinObj.p, EeWonWinCall::HandleKeyEvent, EeWonWinEventShape::Key, (intptr_t)static_cast<InputEvent*>(e));
}
void Window::HandleMouseDown(MouseEvent *e)
{
    ee_won_win_event(this->mWinObj.p, EeWonWinCall::HandleMouseDown, EeWonWinEventShape::Mouse, (intptr_t)static_cast<InputEvent*>(e));
}
void Window::HandleMouseEvent(MouseEvent *e)
{
    ee_won_win_event(this->mWinObj.p, EeWonWinCall::HandleMouseEvent, EeWonWinEventShape::Mouse, (intptr_t)static_cast<InputEvent*>(e));
}
void Window::HandleInputEvent(InputEvent *e)
{
    ee_won_win_event(this->mWinObj.p, EeWonWinCall::HandleInputEvent, EeWonWinEventShape::Input, (intptr_t)e);
}

namespace WONAPI {
Window* CreateNativeWindow() { return new Window; }

SmartPtr<Font> Window::PlatformGetFont(const FontDescriptor& inDescriptor)
{
    return new Font(this->mDisplayContext.get(), inDescriptor);
}

// The manager's pixel scale changed: a new offscreen image at it (the old one drawn in until the repaint, as
// Window::CheckResize does) and the whole root repainted
void Window::FollowPixelScale()
{
    WindowManager* theManager = this->GetPlatManager();
    NativeImage* theOld = this->mOffscreenImage.get();
    if (theManager == NULL || theOld == NULL || this->GetWindowState() != WindowState_Open ||
        theOld->GetScale() == theManager->GetPixelScale())
        return;
    SmartPtr<NativeImage> theImage = this->CreateImage(theOld->GetWidth(), theOld->GetHeight());
    theImage->GetGraphics().DrawImage(theOld, 0, 0);
    this->mOffscreenImage = theImage;
    this->mRoot->Invalidate();
    theManager->SetDirty();
}
}
