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
#include "rust/won_gui/gamewin.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;

// the font a run of ops shares (winobj.rs get_font: the original's local SmartPtr)
static SmartPtr<Font> sHostFont;

extern "C" intptr_t ee_won_guicpp_win_call(void* inWin, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    Window* t = (Window*)inWin;
    switch (op) {
    case Op::WinTick: return (intptr_t)(uint32_t)WONTickCount();
    case Op::WinNewOffscreen: {
        SmartPtr<NativeImage> anImage = t->CreateImage((int)a, (int)b);
        Graphics &g = anImage->GetGraphics();
        g.DrawImage(t->mOffscreenImage.get(), 0, 0);
        t->mOffscreenImage = anImage;
        return 0;
    }
    case Op::WinQueueClose: t->QueueEvent(new InputEvent(InputEvent_WindowClose)); return 0;
    case Op::WinAllowClose: return 1; // (no window ever had an allow-close function)
    case Op::WinStdArrow: return (intptr_t)Cursor::GetStandardCursor(StandardCursor_Arrow);
    case Op::WinCursorNative: return (intptr_t)((Cursor*)a)->GetNative();
    case Op::WinPlatformFont: sHostFont = t->PlatformGetFont(*(const FontDescriptor*)a); return 0;
    case Op::WinHostFont: return (intptr_t)sHostFont.get();
    case Op::WinFontHold: ((Font*)a)->AddRef(); return 0;
    case Op::WinHostFontClear: sHostFont = SmartPtr<Font>(); return 0;
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
    case Op::WindowCreate: me->BaseCreate((int)c, (int)d); return 0;
    case Op::NewContext: me->mDisplayContext = new DisplayContext(NULL); return 0;
    case Op::OffscreenOfRoot: me->mOffscreenImage = me->CreateImage(me->mRoot->Width(), me->mRoot->Height()); return 0;
    case Op::Offscreen: return me->mOffscreenImage.get() != NULL ? (intptr_t)me->mOffscreenImage->mRust : 0;
    case Op::CopyOffscreen: return ee_won_guicpp_win_call(me, EeWonGuiOp::WinNewOffscreen, a, b, 0, 0);
    case Op::QueueSize: me->QueueEvent(new SizeEvent((int)a, (int)b, (int)c, (int)d)); return 0;
    case Op::QueueExit: me->QueueEvent(new InputEvent(InputEvent_MouseExit)); return 0;
    case Op::QueueMouse: {
        const int32_t* p = (const int32_t*)a;
        me->QueueEvent(new MouseEvent(p[0], p[1], p[2], p[3], p[4]));
        return 0;
    }
    case Op::Paint: me->mRoot->PrePaint(me->mOffscreenImage->GetGraphics()); return 0;
    case Op::RootPaint: me->mRoot->Paint(me->mOffscreenImage->GetGraphics()); return 0;
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

namespace WONAPI {
Window* CreateNativeWindow() { return new Window; }

SmartPtr<Font> Window::PlatformGetFont(const FontDescriptor& inDescriptor)
{
    return new Font(this->mDisplayContext.get(), inDescriptor);
}

}
