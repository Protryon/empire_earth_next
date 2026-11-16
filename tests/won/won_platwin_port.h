#include "rust/ee_won.h"
// PlatGUIWindow.cpp's shell over ee_won's platwin.rs (R6 WON round 41, copied from the working tree; the font, the
// factory and the pixel scale's new image left out), for tests/won/won_platwin_test.cpp



// The Rust window's calls on its shell (EeWonPlatWinOp), each the original's statement
static intptr_t PlatWinHost(void* inMe, EeWonPlatWinOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    PlatGUIWindow* me = (PlatGUIWindow*)inMe;
    PlatWindowManager* m = me->GetPlatManager();
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
        me->Window::Create(WONRectangle((int)a, (int)b, (int)c, (int)d));
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
        PlatGUIWindow* theParent = dynamic_cast<PlatGUIWindow*>(me->GetParent());
        return theParent != NULL && theParent->Rust(EeWonPlatWinCall::IsVisible) ? (intptr_t)theParent : 0;
    }
    case Op::WindowClose: me->Window::PlatformClose(); return 0;
    case Op::WindowTimer: me->Window::TimerEvent((DWORD)a); return 0;
    case Op::ApplyCursor:
        if (PlatCursor* theCursor = dynamic_cast<PlatCursor*>((Cursor*)a)) {
            PlatCursorShow(1);
            theCursor->Apply();
        }
        return 0;
    case Op::PopupHit: {
        Window* thePopup = m->GetPopupWindow();
        return thePopup != NULL && (m->GetPopupFlags() & PopupFlag_DontWantMouse) && thePopup == (PlatGUIWindow*)a;
    }
    case Op::CursorShow: PlatCursorShow((int)a); return 0;
    case Op::PlatMinimize: PlatWindowMinimize((PlatWindow*)a); return 0;
    }
    return 0;
}

PlatGUIWindow::PlatGUIWindow() : mRust(ee_won_plat_win_new(this, PlatWinHost)) {}

PlatGUIWindow::~PlatGUIWindow()
{
    this->Rust(EeWonPlatWinCall::Destroy);
    ee_won_plat_win_free(this->mRust);
}

intptr_t PlatGUIWindow::Rust(EeWonPlatWinCall inCall, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    return ee_won_plat_win(this->mRust, inCall, a, b, c, d);
}

PlatWindowManager* PlatGUIWindow::GetPlatManager() { return dynamic_cast<PlatWindowManager*>(this->mManager); }


void PlatGUIWindow::GetInfo(EeWonPlatWinInfo& outInfo) { this->Rust(EeWonPlatWinCall::Info, (intptr_t)&outInfo); }

void PlatGUIWindow::GetClientRect(WONRectangle& outRect) { this->GetRect(outRect, true); }

void PlatGUIWindow::Create(const WONRectangle& inRect)
{
    int r[4] = {inRect.x, inRect.y, inRect.width, inRect.height};
    this->Rust(EeWonPlatWinCall::Create, (intptr_t)r);
}

void PlatGUIWindow::FollowSurface() { this->Rust(EeWonPlatWinCall::FollowSurface); }


void PlatGUIWindow::SetPosSize(int x, int y, int width, int height) { this->Rust(EeWonPlatWinCall::SetPosSize, x, y, width, height); }

void PlatGUIWindow::GetRect(WONRectangle& outRect, bool inClientArea)
{
    int r[4];
    this->Rust(EeWonPlatWinCall::GetRect, (intptr_t)r, inClientArea);
    outRect = WONRectangle(r[0], r[1], r[2], r[3]);
}

void PlatGUIWindow::PlatformSetTitle() { this->Rust(EeWonPlatWinCall::SetTitle); }
// Window icons have no place on the lobby surface
void PlatGUIWindow::PlatformSetIcon() {}
void PlatGUIWindow::PlatformSetCursor(Cursor* inCursor) { this->Rust(EeWonPlatWinCall::SetCursor, (intptr_t)inCursor); }
void PlatGUIWindow::PlatformShowNativeCursor(bool inShow) { this->Rust(EeWonPlatWinCall::ShowNativeCursor, inShow); }
void PlatGUIWindow::PlatformShow(bool inShow) { this->Rust(EeWonPlatWinCall::Show, inShow); }
void PlatGUIWindow::PlatformMinimize(bool inMinimize) { this->Rust(EeWonPlatWinCall::Minimize, inMinimize); }
void PlatGUIWindow::PlatformSetTopMost(bool inTopMost) { this->Rust(EeWonPlatWinCall::SetTopMost, inTopMost); }
void PlatGUIWindow::PlatformClose() { this->Rust(EeWonPlatWinCall::Close); }
void PlatGUIWindow::PlatformSetFocus() { this->Rust(EeWonPlatWinCall::SetFocus); }
void PlatGUIWindow::PlatformSetMouseCapture(bool inOn) { this->Rust(EeWonPlatWinCall::SetCapture, inOn); }
bool PlatGUIWindow::PlatformIsVisible() { return this->Rust(EeWonPlatWinCall::IsVisible) != 0; }
bool PlatGUIWindow::IdleUpdate() { return Window::IdleUpdate(); }
void PlatGUIWindow::TimerEvent(DWORD inElapsed) { this->Rust(EeWonPlatWinCall::Timer, inElapsed); }
void PlatGUIWindow::OnPaint() { this->Rust(EeWonPlatWinCall::Paint); }

void PlatGUIWindow::QueueMouse(int inType, int inSurfaceX, int inSurfaceY, int inMod, int inButtonOrAmount)
{
    int p[5] = {inType, inSurfaceX, inSurfaceY, inMod, inButtonOrAmount};
    this->Rust(EeWonPlatWinCall::QueueMouse, (intptr_t)p);
}

