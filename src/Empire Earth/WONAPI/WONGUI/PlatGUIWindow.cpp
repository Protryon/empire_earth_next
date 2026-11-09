// The WONAPI GUI's window on the lobby surface (WONLobbyPlatform.h), in place of the GDI backend's MSWindow.cpp:
// a rectangle of the surface the manager composites. A window without a parent fills the surface; another is
// placed in it, framed with a caption unless its style has 0x20 (WS_POPUP). The style bits keep MSWindow::Create's
// meaning: 0x01 centre, 0x02 start hidden, 0x20 no frame, 0x40 the rectangle is the client area, 0x80 default
// position. The offscreen image, the paint on an invalid root and the mouse-exit check are MSWindow's.
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPlatform.h"

namespace WONAPI {

// the classic dialog frame: a 3-pixel border and an 18-pixel caption
static const int kFrameBorder = 3;
static const int kFrameCaption = 18;

Window* CreateNativeWindow() { return new PlatGUIWindow; }

PlatGUIWindow::PlatGUIWindow()
    : mX(0), mY(0), mWidth(0), mHeight(0), mVisible(false), mTopMost(false), mRegistered(false),
      mMinimizeOnAltTab(false), mForwardKeyMsgToParent(false) {}

PlatGUIWindow::~PlatGUIWindow()
{
    if (this->mRegistered) {
        if (PlatWindowManager* theManager = this->GetPlatManager())
            theManager->RemovePlatWindow(this);
    }
}

PlatWindowManager* PlatGUIWindow::GetPlatManager() { return dynamic_cast<PlatWindowManager*>(this->mManager); }

SmartPtr<Font> PlatGUIWindow::PlatformGetFont(const FontDescriptor& inDescriptor)
{
    return new PlatGUIFont(this->mDisplayContext.get(), inDescriptor);
}

void PlatGUIWindow::GetClientRect(WONRectangle& outRect)
{
    if (this->HasFrame()) {
        outRect.x = this->mX + kFrameBorder;
        outRect.y = this->mY + kFrameBorder + kFrameCaption;
        outRect.width = this->mWidth - 2 * kFrameBorder;
        outRect.height = this->mHeight - 2 * kFrameBorder - kFrameCaption;
        if (outRect.width < 0)
            outRect.width = 0;
        if (outRect.height < 0)
            outRect.height = 0;
    } else {
        outRect.x = this->mX;
        outRect.y = this->mY;
        outRect.width = this->mWidth;
        outRect.height = this->mHeight;
    }
}

// Opens the window: its rectangle in the surface, the root sized to its client area, a display context, an
// offscreen image of the root's size (OnCreate), then centred and shown as the style asks
void PlatGUIWindow::Create(const WONRectangle& inRect)
{
    PlatWindowManager* theManager = this->GetPlatManager();
    if (this->FillsSurface() && theManager != NULL) {
        this->mX = this->mY = 0;
        this->mWidth = theManager->GetSurfaceWidth();
        this->mHeight = theManager->GetSurfaceHeight();
    } else {
        this->mX = inRect.x;
        this->mY = inRect.y;
        this->mWidth = inRect.width;
        this->mHeight = inRect.height;
        if (this->HasFrame() && (this->mStyle & 0x40)) {
            this->mWidth += 2 * kFrameBorder;
            this->mHeight += 2 * kFrameBorder + kFrameCaption;
        }
    }
    WONRectangle theClient;
    this->GetClientRect(theClient);
    Window::Create(WONRectangle(theClient.x, theClient.y, theClient.width, theClient.height));
    this->mDisplayContext = new DisplayContext(NULL);
    this->mOffscreenImage = this->CreateImage(this->mRoot->Width(), this->mRoot->Height());
    if (theManager != NULL)
        theManager->AddPlatWindow(this);

    if (!this->FillsSurface()) {
        if (this->mStyle & 0x01)
            this->Center(true);
        else if (this->mStyle & 0x80)
            this->Center(false);
    }
    if (!(this->mStyle & 0x02)) {
        this->mIsShowing = true;
        this->PlatformShow(true);
    } else
        this->mIsShowing = false;
}

// A window that fills the surface takes the surface's new size (WM_SIZE: a size event for the root)
void PlatGUIWindow::FollowSurface()
{
    PlatWindowManager* theManager = this->GetPlatManager();
    if (!this->FillsSurface() || theManager == NULL || this->mWindowState != WindowState_Open)
        return;
    int theWidth = theManager->GetSurfaceWidth(), theHeight = theManager->GetSurfaceHeight();
    if (theWidth == this->mWidth && theHeight == this->mHeight)
        return;
    this->mX = this->mY = 0;
    this->mWidth = theWidth;
    this->mHeight = theHeight;
    this->QueueEvent(new SizeEvent(-1, -1, theWidth, theHeight));
    theManager->SetDirty();
}

// The manager's pixel scale changed: a new offscreen image at it (the old one drawn in until the repaint, as
// Window::CheckResize does) and the whole root repainted
void PlatGUIWindow::FollowPixelScale()
{
    PlatWindowManager* theManager = this->GetPlatManager();
    PlatNativeImage* theOld = dynamic_cast<PlatNativeImage*>(this->mOffscreenImage.get());
    if (theManager == NULL || theOld == NULL || this->mWindowState != WindowState_Open ||
        theOld->GetScale() == theManager->GetPixelScale())
        return;
    SmartPtr<NativeImage> theImage = this->CreateImage(theOld->GetWidth(), theOld->GetHeight());
    theImage->GetGraphics().DrawImage(theOld, 0, 0);
    this->mOffscreenImage = theImage;
    this->mRoot->Invalidate();
    theManager->SetDirty();
}

// Moves and resizes the window (-1: keep the width or height); one that fills the surface stays as it is. A size
// change queues a size event with the client size (WM_SIZE), a move one with the client position (WM_MOVE).
void PlatGUIWindow::SetPosSize(int x, int y, int width, int height)
{
    if (this->mWindowState != WindowState_Open || this->FillsSurface())
        return;
    if (width == -1)
        width = this->mWidth;
    if (height == -1)
        height = this->mHeight;
    WONRectangle theOld;
    this->GetClientRect(theOld);
    this->mX = x;
    this->mY = y;
    this->mWidth = width;
    this->mHeight = height;
    WONRectangle theNew;
    this->GetClientRect(theNew);
    if (theNew.width != theOld.width || theNew.height != theOld.height)
        this->QueueEvent(new SizeEvent(-1, -1, theNew.width, theNew.height));
    if (theNew.x != theOld.x || theNew.y != theOld.y)
        this->QueueEvent(new SizeEvent(theNew.x, theNew.y, -1, -1));
    if (PlatWindowManager* theManager = this->GetPlatManager())
        theManager->SetDirty();
}

// The window's rectangle, or its client area, in surface pixels
void PlatGUIWindow::GetRect(WONRectangle& outRect, bool inClientArea)
{
    if (inClientArea)
        this->GetClientRect(outRect);
    else {
        outRect.x = this->mX;
        outRect.y = this->mY;
        outRect.width = this->mWidth;
        outRect.height = this->mHeight;
    }
}

void PlatGUIWindow::PlatformSetTitle()
{
    if (PlatWindowManager* theManager = this->GetPlatManager())
        theManager->SetDirty();
}

// Window icons have no place on the lobby surface
void PlatGUIWindow::PlatformSetIcon() {}

// Makes the cursor the pointer's shape while the pointer is in this window (or it holds the capture)
void PlatGUIWindow::PlatformSetCursor(Cursor* inCursor)
{
    PlatWindowManager* theManager = this->GetPlatManager();
    if (!this->mMouseIsInClientArea && (theManager == NULL || theManager->GetCaptureWindow() != this))
        return;
    if (PlatCursor* theCursor = dynamic_cast<PlatCursor*>(inCursor)) {
        PlatCursorShow(1);
        theCursor->Apply();
    }
}

// Hides the system pointer for a cursor the GUI draws itself (MSWindow: SetCursor(NULL)); showing it shows it
void PlatGUIWindow::PlatformShowNativeCursor(bool inShow)
{
    if (inShow)
        PlatCursorShow(1);
    else if (this->mMouseIsInClientArea)
        PlatCursorShow(0);
}

// Shows or hides the window; showing one that is not a popup activates it (SW_SHOW / SW_SHOWNA)
void PlatGUIWindow::PlatformShow(bool inShow)
{
    PlatWindowManager* theManager = this->GetPlatManager();
    if (this->mVisible == inShow)
        return;
    this->mVisible = inShow;
    if (theManager == NULL)
        return;
    theManager->SetDirty();
    if (inShow) {
        this->InvalidateRoot();
        if (!this->mIsPopup) {
            theManager->BringToFront(this);
            theManager->SetFocusWindow(this);
        }
    } else if (theManager->GetFocusWindow() == this) {
        PlatGUIWindow* theNext = dynamic_cast<PlatGUIWindow*>(this->mParent);
        if (theNext != NULL && !theNext->mVisible)
            theNext = NULL;
        theManager->SetFocusWindow(theNext);
    }
}

// Minimizing the window that fills the surface minimizes the game window
void PlatGUIWindow::PlatformMinimize(bool inMinimize)
{
    PlatWindowManager* theManager = this->GetPlatManager();
    if (inMinimize && this->FillsSurface() && theManager != NULL && theManager->GetPlatWindow() != NULL)
        PlatWindowMinimize(theManager->GetPlatWindow());
}

void PlatGUIWindow::PlatformSetTopMost(bool inTopMost)
{
    this->mTopMost = inTopMost;
    if (PlatWindowManager* theManager = this->GetPlatManager())
        theManager->SetDirty();
}

// Closes the window and takes it off the surface (DestroyWindow)
void PlatGUIWindow::PlatformClose()
{
    Window::PlatformClose();
    this->mVisible = false;
    if (this->mRegistered) {
        if (PlatWindowManager* theManager = this->GetPlatManager())
            theManager->RemovePlatWindow(this);
    }
}

void PlatGUIWindow::PlatformSetFocus()
{
    PlatWindowManager* theManager = this->GetPlatManager();
    if (theManager == NULL || this->mWindowState != WindowState_Open)
        return;
    if (!this->mIsPopup)
        theManager->BringToFront(this);
    theManager->SetFocusWindow(this);
}

void PlatGUIWindow::PlatformSetMouseCapture(bool inOn)
{
    if (PlatWindowManager* theManager = this->GetPlatManager())
        theManager->SetCaptureWindow(inOn ? this : NULL);
}

bool PlatGUIWindow::PlatformIsVisible() { return this->mVisible; }

bool PlatGUIWindow::IdleUpdate()
{
    if (!Window::IdleUpdate())
        return false;
    return true;
}

// Runs the timer update and queues a mouse-exit event once the pointer has left the window (being over the
// manager's popup window while the popup does not want the mouse does not count)
void PlatGUIWindow::TimerEvent(DWORD inElapsed)
{
    Window::TimerEvent(inElapsed);
    if (!this->mMouseIsInClientArea)
        return;
    PlatWindowManager* theManager = this->GetPlatManager();
    if (theManager == NULL)
        return;
    int theX = -100000, theY = -100000;
    PlatGUIWindow* theHit = NULL;
    if (theManager->GetMousePosition(theX, theY))
        theHit = theManager->WindowFromPoint(theX, theY);
    WONRectangle theClient;
    this->GetClientRect(theClient);
    if (theHit == this && this->mRoot->Contains(theX - theClient.x, theY - theClient.y))
        return;
    if (theHit != this) {
        Window* thePopup = theManager->GetPopupWindow();
        if (thePopup != NULL && (theManager->mPopupFlags & PopupFlag_DontWantMouse) && thePopup == theHit)
            return;
    }
    this->QueueEvent(new InputEvent(InputEvent_MouseExit));
}

// Repaints an invalid root into the offscreen image (MSWindow::OnPaint without the blit: the manager composites)
void PlatGUIWindow::OnPaint()
{
    if (this->mRoot->mInvalidState == 0 || this->mOffscreenImage.get() == NULL)
        return;
    this->mRoot->PrePaint(this->mOffscreenImage->GetGraphics());
    this->mRoot->Paint(this->mOffscreenImage->GetGraphics());
    if (PlatWindowManager* theManager = this->GetPlatManager())
        theManager->SetDirty();
}

// Queues a mouse event at a surface point, in client coordinates
void PlatGUIWindow::QueueMouse(int inType, int inSurfaceX, int inSurfaceY, int inMod, int inButtonOrAmount)
{
    WONRectangle theClient;
    this->GetClientRect(theClient);
    this->QueueEvent(new MouseEvent(inType, inSurfaceX - theClient.x, inSurfaceY - theClient.y, inMod, inButtonOrAmount));
}

} // namespace WONAPI
