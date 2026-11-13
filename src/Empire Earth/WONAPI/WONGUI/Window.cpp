// Window (R6 WON round 32): the Rust Window's calls out (rust/crates/ee_won/src/winobj.rs, EeWonGuiOp's Win* ops), its
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
#include "rust/ee_won.h"
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
    default: return 0;
    }
}

// FUNCTION: WON 0x00729c30
// Creates a closed window on the default window manager with a new root container and the title "Sierra App" (the
// Rust window: closed, hidden, no parent, the mouse outside, sizes 0..64000)
Window::Window()
{
	this->mWinObj.p = ee_won_win_new(this, ee_won_guicpp_win_call);
	this->mManager = WONAPI::WindowManager::GetDefaultWindowManager();
	this->mRoot = new RootContainer(this);
	this->mAllowCloseFunc = NULL;
	this->mTitle = "Sierra App";
}

// FUNCTION: WON 0x0072a020
// Destroys the window: the members, then the Rust window (its child set; mWinObj is the first member)
Window::~Window()
{
}

// FUNCTION: WON 0x0072a230
// Returns the cached font for a descriptor, creating it through the platform on first use
SmartPtr<Font> Window::GetFont(const FontDescriptor &theDescriptor)
{
	std::map<FontDescriptor,SmartPtr<Font> >::iterator anItr = this->mFontMap.insert(std::map<FontDescriptor,SmartPtr<Font> >::value_type(theDescriptor, NULL)).first;
	SmartPtr<Font> aFont = anItr->second;
	if(aFont.get()==NULL)
	{
		aFont = this->PlatformGetFont(theDescriptor);
		anItr->second = aFont;
	}

	return aFont;
}

// FUNCTION: WON 0x0072adc0
// Sets the window title, updating the platform window when open
void Window::SetTitle(const GUIString &theTitle)
{
	this->mTitle = theTitle;
	this->WCall(EeWonWinCall::TitleChanged);
}

// FUNCTION: WON 0x0072ae20
// Sets the window icon, updating the platform window when open
void Window::SetIcon(NativeIcon *theIcon)
{
	this->mIcon = theIcon;
	this->WCall(EeWonWinCall::IconChanged);
}

// FUNCTION: WON 0x0072a900
// Opens the window: default icon, registration with the parent, root sized to the rectangle, added to the manager
void Window::Create(const WONRectangle& theWindowRectR)
{
	if(this->mIcon.get()==NULL)
		this->mIcon = this->mManager->mDefaultWindowIcon.get();

	this->WCall(EeWonWinCall::Create, theWindowRectR.Width(), theWindowRectR.Height());
}

// FUNCTION: WON 0x0072a220
// Returns the default window manager's default window
Window* Window::GetDefaultWindow()
{
	return WONAPI::WindowManager::GetDefaultWindowManager()->mDefaultWindow;
}

// FUNCTION: WON 0x0072a8c0
// Creates an image of the given size for the window's display
SmartPtr<NativeImage> Window::CreateImage(int theWidth, int theHeight)
{
	return this->mDisplayContext->CreateImage(theWidth,theHeight);
}

static bool gWindowSkipDecodeImages = false;

// FUNCTION: WON 0x0072a640
// Turns image decoding off or on for every window (a blank placeholder is used)
void Window::SetSkipDecodeImages(bool skip) {
    gWindowSkipDecodeImages = skip;
}

// FUNCTION: WON 0x0072a650
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

// FUNCTION: WON 0x0072a800
// Decodes a raw image file; while decoding is off returns a blank 10x10 image
SmartPtr<RawImage> Window::DecodeImageRaw(const char* path) {
    if(gWindowSkipDecodeImages)
        return new RawImage8(10,10,NULL);
    return this->mManager->DecodeImageRaw(path);
}

// The two functions after this one are not identified in the original exe
// FUNCTION: WON 0x0072a8f0
// Returns the window's offscreen image
NativeImage* Window::GetOffscreenImage()
{
    return this->mOffscreenImage.get();
}

Graphics& Window::GetGraphics()
{
    return this->mOffscreenImage->GetGraphics();
}

// The forwarders (the header's one-line methods are the rest)
bool Window::CanClose() { return this->WCall(EeWonWinCall::CanClose) != 0; }                       // 0x0072ab80
bool Window::Close() { return this->WCall(EeWonWinCall::Close) != 0; }                             // 0x0072ac30
void Window::CheckResize(DWORD theTick) { this->WCall(EeWonWinCall::CheckResize, (intptr_t)(uint32_t)theTick); } // 0x0072ac90
void Window::Center(bool inParent) { this->WCall(EeWonWinCall::Center, inParent ? 1 : 0); }        // 0x0072ae80
void Window::SetClientSize(int w, int h) { this->WCall(EeWonWinCall::SetClientSize, w, h); }       // 0x0072af30
void Window::Show(bool show) { this->WCall(EeWonWinCall::Show, show ? 1 : 0); }                    // 0x0072af70
void Window::HandleMouseExit() { this->WCall(EeWonWinCall::HandleMouseExit); }                     // 0x0072b120
static EeWonWinEvent WinEvent(InputEvent* e)
{
    EeWonWinEvent r = {};
    r.kind = (int32_t)e->mEventType;
    return r;
}
void Window::HandleKeyEvent(KeyEvent *e)                                                           // 0x0072b170
{
    EeWonWinEvent r = WinEvent(e);
    r.key_mod = e->mKeyMod;
    r.key = e->mKey;
    this->WCall(EeWonWinCall::HandleKeyEvent, (intptr_t)&r);
}
static EeWonWinEvent MouseWinEvent(MouseEvent* e)
{
    EeWonWinEvent r = WinEvent(e);
    r.x = e->mx;
    r.y = e->my;
    r.key_mod = e->mKeyMod;
    r.button = e->mButtonOrAmount;
    return r;
}
void Window::HandleMouseDown(MouseEvent *e)                                                        // 0x0072b1c0
{
    EeWonWinEvent r = MouseWinEvent(e);
    this->WCall(EeWonWinCall::HandleMouseDown, (intptr_t)&r);
}
void Window::HandleMouseEvent(MouseEvent *e)                                                       // 0x0072b260
{
    EeWonWinEvent r = MouseWinEvent(e);
    this->WCall(EeWonWinCall::HandleMouseEvent, (intptr_t)&r);
}
void Window::HandleInputEvent(InputEvent *e)                                                       // 0x0072b390
{
    EeWonWinEvent r = WinEvent(e);
    if (e->mEventType == InputEvent_SizeChange) {
        r.width = static_cast<SizeEvent*>(e)->mWidth;
        r.height = static_cast<SizeEvent*>(e)->mHeight;
    }
    this->WCall(EeWonWinCall::HandleInputEvent, (intptr_t)&r);
}
bool Window::IdleUpdate() { return this->WCall(EeWonWinCall::IdleUpdate) != 0; }                   // 0x0072b460
void Window::TimerEvent(DWORD e) { this->WCall(EeWonWinCall::TimerEvent, (intptr_t)(uint32_t)e); } // 0x0072b4b0
void Window::SetParent(Window *p) { this->WCall(EeWonWinCall::SetParent, (intptr_t)p); }           // 0x0072a210
void Window::PlatformClose() { this->WCall(EeWonWinCall::PlatformClose); }                         // 0x0072aa60
void Window::ShowNativeCursor(bool show) { this->WCall(EeWonWinCall::ShowNativeCursor, show ? 1 : 0); } // 0x0072afc0
void Window::AddComponent(Component* c) { this->WCall(EeWonWinCall::AddComponent, (intptr_t)c); }  // 0x0072b040
void Window::SetModal(bool modal) { this->WCall(EeWonWinCall::SetModal, modal ? 1 : 0); }          // 0x0072b060
void Window::CheckSetCursor(bool force) { this->WCall(EeWonWinCall::CheckSetCursor, force ? 1 : 0); } // 0x0072afd0
namespace WONAPI {
void Window::InvalidateRoot() { this->WCall(EeWonWinCall::InvalidateRoot); }                       // 0x0072a1e0
void Window::Minimize(bool minimize) { this->WCall(EeWonWinCall::Minimize, minimize ? 1 : 0); }    // 0x0072afb0
}
void Window::SuspendRootResize(bool suspend) { this->WCall(EeWonWinCall::SuspendRootResize, suspend ? 1 : 0); }
void Window::RemoveComponent(Component* c) { this->WCall(EeWonWinCall::RemoveComponent, (intptr_t)c); }
bool Window::IsMouseDown(MouseButton b) { return this->WCall(EeWonWinCall::IsMouseDown, (intptr_t)b) != 0; }
