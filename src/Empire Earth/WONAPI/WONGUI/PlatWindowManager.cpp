// The WONAPI GUI's window manager on the platform layer (WONLobbyPlatform.h), in place of the GDI backend's
// MSWindowManager.cpp and MSWindow's window procedure: the event loop on PlatInputEvent, the event translation
// (mouse, keys, text, focus), the standard cursors, the classic colour scheme and the compositor of the lobby
// surface the game shows (at the pixel scale the game sets: WONLobbyPlatform.h "Pixel scale").
#include <windows.h>
#include <string>
#include <string.h>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <math.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPlatform.h"
#include "platform/PlatFont.h"
#include "platform/PlatKeys.h"
#include "platform/PlatString.h"
#include "platform/PlatTime.h"

namespace WONAPI {

// The WON hand cursor (32x32, hot spot 12, 4): the XOR plane
static const unsigned char gHandCursorData[] =
{
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x0c,
	0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x0d,0x80,0x00,0x00,
	0x0d,0xb0,0x00,0x00,0x0d,0xb4,0x00,0x00,0x0d,0xb6,0x00,0x00,0xcf,0xf6,0x00,
	0x00,0xef,0xfe,0x00,0x00,0x6f,0xfe,0x00,0x00,0x2f,0xfe,0x00,0x00,0x3f,0xfe,
	0x00,0x00,0x1f,0xfe,0x00,0x00,0x1f,0xfc,0x00,0x00,0x0f,0xfc,0x00,0x00,0x0f,
	0xfc,0x00,0x00,0x07,0xf8,0x00,0x00,0x07,0xf8,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
// ... and the AND plane
static const unsigned char gHandCursorMaskData[] =
{
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xf3,0xff,0xff,0xff,0xe1,0xff,0xff,0xff,0xe1,0xff,0xff,0xff,0xe1,
	0xff,0xff,0xff,0xe1,0xff,0xff,0xff,0xe0,0x7f,0xff,0xff,0xe0,0x0f,0xff,0xff,
	0xe0,0x03,0xff,0xff,0xe0,0x01,0xff,0xfe,0x20,0x00,0xff,0xfe,0x00,0x00,0xff,
	0xfe,0x00,0x00,0xff,0xff,0x00,0x00,0xff,0xff,0x80,0x00,0xff,0xff,0x80,0x00,
	0xff,0xff,0xc0,0x00,0xff,0xff,0xc0,0x01,0xff,0xff,0xe0,0x01,0xff,0xff,0xe0,
	0x01,0xff,0xff,0xf0,0x03,0xff,0xff,0xf0,0x03,0xff,0xff,0xf0,0x03,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
};

// The classic Windows colours (the "Windows Classic" scheme of the time) the colour scheme takes in place of
// GetSysColor's, as 0xRRGGBB
static const DWORD kClassic3DDarkShadow = 0x404040, kClassic3DFace = 0xD4D0C8, kClassic3DHilight = 0xFFFFFF,
                   kClassic3DShadow = 0x808080, kClassicScrollbar = 0xD4D0C8, kClassicButtonText = 0x000000,
                   kClassicGrayText = 0x808080, kClassicHilight = 0x0A246A, kClassicHilightText = 0xFFFFFF,
                   kClassicInfoBack = 0xFFFFE1, kClassicInfoText = 0x000000, kClassicMenu = 0xD4D0C8,
                   kClassicMenuText = 0x000000, kClassicWindow = 0xFFFFFF, kClassicWindowFrame = 0x000000;
static const DWORD kClassicCaptionActive = 0x0A246A, kClassicCaptionInactive = 0x808080;
static const int kFrameBorder = 3, kFrameCaption = 18;
// the surface before SetSurfaceSize
static const int kDefaultSurfaceWidth = 800, kDefaultSurfaceHeight = 600;

static int KeyModFromPlat(uint32_t inModifiers)
{
    return ((inModifiers & PLAT_MOD_SHIFT) ? 1 : 0) | ((inModifiers & PLAT_MOD_ALT) ? 2 : 0) |
           ((inModifiers & PLAT_MOD_CONTROL) ? 4 : 0);
}

static uint32_t TickNow() { return (uint32_t)PlatTimeMilliseconds64(); }

// Creates the manager and installs the standard cursors: the WON hand from its bits, the others the system's
PlatWindowManager::PlatWindowManager()
    : mFrameMutex(PlatMutexCreate()), mFrameWidth(0), mFrameHeight(0), mFrameLogicalWidth(0), mFrameLogicalHeight(0),
      mFrameSerial(0), mPendingWidth(0), mPendingHeight(0), mPendingScale(0.0f), mPixelScale(1.0f), mSurfaceWidth(kDefaultSurfaceWidth), mSurfaceHeight(kDefaultSurfaceHeight),
      mWakePending(0), mEventForward(NULL), mPlatFocusWindow(NULL), mCaptureWindow(NULL), mDragWindow(NULL),
      mDragOffsetX(0), mDragOffsetY(0), mAppActive(true), mDirty(true), mMouseX(0), mMouseY(0), mHaveMouse(false),
      mPlatWindow(NULL), mLastProcessTick(TickNow()), mCaptionFont(NULL)
{
    this->mPresentRect[0] = this->mPresentRect[1] = this->mPresentRect[2] = this->mPresentRect[3] = 0.0f;
    std::vector<uint32_t> theHand(32 * 32);
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
            int theByte = y * 4 + x / 8, theBit = 0x80 >> (x & 7);
            bool theAnd = (gHandCursorMaskData[theByte] & theBit) != 0;
            bool theXor = (gHandCursorData[theByte] & theBit) != 0;
            theHand[y * 32 + x] = theAnd ? (theXor ? 0xFF000000u : 0u) : (theXor ? 0xFFFFFFFFu : 0xFF000000u);
        }
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
    for (size_t i = 0; i < this->mZOrder.size(); i++)
        this->mZOrder[i]->mRegistered = false;
    this->mZOrder.clear();
    if (this->mCaptionFont != NULL)
        PlatFontDestroy(this->mCaptionFont);
    PlatMutexDestroy(this->mFrameMutex);
}

// ---- game interface

void PlatWindowManager::SetSurfaceSize(int inWidth, int inHeight)
{
    if (PlatCurrentThreadId() == WindowManager::GetThreadId()) {
        this->ApplySurfaceSize(inWidth, inHeight);
        return;
    }
    PlatMutexLock(this->mFrameMutex);
    this->mPendingWidth = inWidth;
    this->mPendingHeight = inHeight;
    PlatMutexUnlock(this->mFrameMutex);
    this->Signal();
}

void PlatWindowManager::ApplySurfaceSize(int inWidth, int inHeight)
{
    if (inWidth <= 0 || inHeight <= 0 || (inWidth == this->mSurfaceWidth && inHeight == this->mSurfaceHeight))
        return;
    this->mSurfaceWidth = inWidth;
    this->mSurfaceHeight = inHeight;
    for (size_t i = 0; i < this->mZOrder.size(); i++)
        this->mZOrder[i]->FollowSurface();
    this->mDirty = true;
}

// The pixel scale, at least 1 (a smaller lobby is shown scaled down) and at most 4
void PlatWindowManager::SetPixelScale(float inScale)
{
    if (!(inScale >= 1.0f))
        inScale = 1.0f;
    if (inScale > 4.0f)
        inScale = 4.0f;
    if (PlatCurrentThreadId() == WindowManager::GetThreadId()) {
        this->ApplyPixelScale(inScale);
        return;
    }
    // (a scale the manager has or is about to take wakes nothing: the game gives it with every frame)
    PlatMutexLock(this->mFrameMutex);
    bool theSame = this->mPendingScale > 0.0f ? this->mPendingScale == inScale : this->mPixelScale == inScale;
    if (!theSame)
        this->mPendingScale = inScale;
    PlatMutexUnlock(this->mFrameMutex);
    if (!theSame)
        this->Signal();
}

void PlatWindowManager::SetPresentRect(float inLeft, float inTop, float inWidth, float inHeight)
{
    PlatMutexLock(this->mFrameMutex);
    this->mPresentRect[0] = inLeft;
    this->mPresentRect[1] = inTop;
    this->mPresentRect[2] = inWidth;
    this->mPresentRect[3] = inHeight;
    PlatMutexUnlock(this->mFrameMutex);
}

// A new scale: every window repaints into a new offscreen image of that scale, then the frame is composited anew
void PlatWindowManager::ApplyPixelScale(float inScale)
{
    if (inScale == this->mPixelScale)
        return;
    PlatMutexLock(this->mFrameMutex);   // SetPixelScale reads it on other threads
    this->mPixelScale = inScale;
    PlatMutexUnlock(this->mFrameMutex);
    for (size_t i = 0; i < this->mZOrder.size(); i++)
        this->mZOrder[i]->FollowPixelScale();
    this->mDirty = true;
}

bool PlatWindowManager::LockFrame(const uint32_t*& outPixels, int& outWidth, int& outHeight, int& outPitch,
                                  int& outLogicalWidth, int& outLogicalHeight, uint32_t& outSerial)
{
    PlatMutexLock(this->mFrameMutex);
    if (this->mFrameSerial == 0 || this->mFrame.empty()) {
        PlatMutexUnlock(this->mFrameMutex);
        return false;
    }
    outPixels = this->mFrame.data();
    outWidth = this->mFrameWidth;
    outHeight = this->mFrameHeight;
    outPitch = this->mFrameWidth * 4;
    outLogicalWidth = this->mFrameLogicalWidth;
    outLogicalHeight = this->mFrameLogicalHeight;
    outSerial = this->mFrameSerial;
    return true;
}

void PlatWindowManager::UnlockFrame() { PlatMutexUnlock(this->mFrameMutex); }

void PlatWindowManager::SetEventForward(EventForwardFunc inFunc) { this->mEventForward = inFunc; }

// ---- WindowManager

// Waits for one platform event (GetMessage; at most until 50 ms after the last pass, the original's window timer),
// handles it, runs ProcessEvents (the window procedure did after each message), then paints and composites
void PlatWindowManager::GetNextEvent()
{
    this->mWakePending = 0;
    int thePendingWidth = 0, thePendingHeight = 0;
    float thePendingScale = 0.0f;
    PlatMutexLock(this->mFrameMutex);
    thePendingWidth = this->mPendingWidth;
    thePendingHeight = this->mPendingHeight;
    thePendingScale = this->mPendingScale;
    this->mPendingWidth = this->mPendingHeight = 0;
    this->mPendingScale = 0.0f;
    PlatMutexUnlock(this->mFrameMutex);
    if (thePendingWidth > 0)
        this->ApplySurfaceSize(thePendingWidth, thePendingHeight);
    if (thePendingScale > 0.0f)
        this->ApplyPixelScale(thePendingScale);

    uint32_t theStart = TickNow();
    uint32_t theElapsed = theStart - this->mLastProcessTick;
    uint32_t theTimeout = theElapsed >= 50 ? 0 : 50 - theElapsed;
    PlatInputEvent theEvent;
    if (PlatWindowWaitEvent(&theEvent, theTimeout))
        this->HandlePlatEvent(theEvent);
    else {
        // no window system (or a spurious return): never spin
        uint32_t theWaited = TickNow() - theStart;
        if (theWaited < theTimeout)
            PlatSleep(theTimeout - theWaited);
    }
    this->mLastProcessTick = TickNow();
    this->ProcessEvents();
    this->PaintAndComposite();
}

// The display mode is the game's: nothing to change
bool PlatWindowManager::SetScreenResolutionHook(int, int) { return true; }

void PlatWindowManager::GetScreenResolutionHook(int& outWidth, int& outHeight)
{
    outWidth = this->mSurfaceWidth;
    outHeight = this->mSurfaceHeight;
}

// Wakes the event loop (any thread): one platform wake until the loop runs again
void PlatWindowManager::Signal()
{
    if (PlatAtomicIncrement(&this->mWakePending) == 1)
        PlatWindowWake();
}

// The lobby surface is the screen
void PlatWindowManager::GetScreenRect(WONRectangle& outRect)
{
    outRect.x = outRect.y = 0;
    outRect.width = this->mSurfaceWidth;
    outRect.height = this->mSurfaceHeight;
}

// The colour scheme's standard colours from the classic Windows scheme (text takes the window frame's, as
// MSWindowManager did)
void PlatWindowManager::GetSystemColorScheme()
{
    ColorScheme* theScheme = this->mColorScheme.get();
    theScheme->SetStandardColor(StandardColor_3DDarkShadow, kClassic3DDarkShadow);
    theScheme->SetStandardColor(StandardColor_3DFace, kClassic3DFace);
    theScheme->SetStandardColor(StandardColor_3DHilight, kClassic3DHilight);
    theScheme->SetStandardColor(StandardColor_3DShadow, kClassic3DShadow);
    theScheme->SetStandardColor(StandardColor_Scrollbar, kClassicScrollbar);
    theScheme->SetStandardColor(StandardColor_ButtonText, kClassicButtonText);
    theScheme->SetStandardColor(StandardColor_GrayText, kClassicGrayText);
    theScheme->SetStandardColor(StandardColor_Hilight, kClassicHilight);
    theScheme->SetStandardColor(StandardColor_HilightText, kClassicHilightText);
    theScheme->SetStandardColor(StandardColor_ToolTipBack, kClassicInfoBack);
    theScheme->SetStandardColor(StandardColor_ToolTipText, kClassicInfoText);
    theScheme->SetStandardColor(StandardColor_MenuBack, kClassicMenu);
    theScheme->SetStandardColor(StandardColor_MenuText, kClassicMenuText);
    theScheme->SetStandardColor(StandardColor_Back, kClassicWindow);
    theScheme->SetStandardColor(StandardColor_Text, kClassicWindowFrame);
}

// ---- windows

void PlatWindowManager::AddPlatWindow(PlatGUIWindow* inWindow)
{
    if (std::find(this->mZOrder.begin(), this->mZOrder.end(), inWindow) == this->mZOrder.end())
        this->mZOrder.push_back(inWindow);
    inWindow->mRegistered = true;
    this->mDirty = true;
}

// Takes a closed window off the surface; the focus goes to its parent or the topmost window left (Windows
// activates another window when the active one is destroyed)
void PlatWindowManager::RemovePlatWindow(PlatGUIWindow* inWindow)
{
    std::vector<PlatGUIWindow*>::iterator theItr = std::find(this->mZOrder.begin(), this->mZOrder.end(), inWindow);
    if (theItr != this->mZOrder.end())
        this->mZOrder.erase(theItr);
    inWindow->mRegistered = false;
    if (this->mCaptureWindow == inWindow)
        this->mCaptureWindow = NULL;
    if (this->mDragWindow == inWindow)
        this->mDragWindow = NULL;
    if (this->mPlatFocusWindow == inWindow) {
        this->mPlatFocusWindow = NULL;
        PlatGUIWindow* theNext = dynamic_cast<PlatGUIWindow*>(inWindow->mParent);
        if (theNext == NULL || !theNext->mRegistered || !theNext->mVisible) {
            theNext = NULL;
            for (size_t i = this->mZOrder.size(); i-- > 0;)
                if (this->mZOrder[i]->mVisible && !this->mZOrder[i]->mIsPopup) {
                    theNext = this->mZOrder[i];
                    break;
                }
        }
        this->SetFocusWindow(theNext);
    }
    this->mDirty = true;
}

// Raises a window above the other windows of its kind (the windows that fill the surface stay below the others)
void PlatWindowManager::BringToFront(PlatGUIWindow* inWindow)
{
    std::vector<PlatGUIWindow*>::iterator theItr = std::find(this->mZOrder.begin(), this->mZOrder.end(), inWindow);
    if (theItr == this->mZOrder.end() || theItr + 1 == this->mZOrder.end())
        return;
    this->mZOrder.erase(theItr);
    this->mZOrder.push_back(inWindow);
    this->mDirty = true;
}

// Moves the keyboard focus (WM_KILLFOCUS to the old window, WM_SETFOCUS to the new one)
void PlatWindowManager::SetFocusWindow(PlatGUIWindow* inWindow)
{
    if (inWindow == this->mPlatFocusWindow)
        return;
    PlatGUIWindow* theOld = this->mPlatFocusWindow;
    this->mPlatFocusWindow = inWindow;
    if (theOld != NULL && theOld->mWindowState == Window::WindowState_Open && this->mAppActive)
        theOld->QueueEvent(new InputEvent(InputEvent_LostFocus));
    if (inWindow != NULL && this->mAppActive)
        inWindow->QueueEvent(new InputEvent(InputEvent_GotFocus));
    this->mDirty = true;
}

// The windows in drawing order, bottom to top: those that fill the surface, the others, the topmost ones, popups
static void DrawingOrder(const std::vector<PlatGUIWindow*>& inWindows, std::vector<PlatGUIWindow*>& outOrder)
{
    outOrder.clear();
    for (int thePass = 0; thePass < 4; thePass++)
        for (size_t i = 0; i < inWindows.size(); i++) {
            PlatGUIWindow* w = inWindows[i];
            int theLayer = w->mIsPopup ? 3 : w->mTopMost ? 2 : w->FillsSurface() ? 0 : 1;
            if (theLayer == thePass)
                outOrder.push_back(w);
        }
}

PlatGUIWindow* PlatWindowManager::WindowFromPoint(int inX, int inY)
{
    std::vector<PlatGUIWindow*> theOrder;
    DrawingOrder(this->mZOrder, theOrder);
    for (size_t i = theOrder.size(); i-- > 0;) {
        PlatGUIWindow* w = theOrder[i];
        if (w->mVisible && inX >= w->mX && inY >= w->mY && inX < w->mX + w->mWidth && inY < w->mY + w->mHeight)
            return w;
    }
    return NULL;
}

// ---- events

void PlatWindowManager::ScaleToSurface(const PlatInputEvent& inEvent, int& ioX, int& ioY)
{
    PlatWindow* theWindow = inEvent.mWindow != NULL ? inEvent.mWindow : this->mPlatWindow;
    int32_t theWidth = 0, theHeight = 0;
    if (theWindow != NULL)
        PlatWindowGetClientSize(theWindow, &theWidth, &theHeight);
    if (theWidth <= 0 || theHeight <= 0 || this->mSurfaceWidth <= 0 || this->mSurfaceHeight <= 0)
        return;
    // the surface is shown letterboxed (RTSGraphics::RenderLobby): in the rectangle the game gave, else scaled
    // uniformly to fit the window, centred
    float theRect[4];
    PlatMutexLock(this->mFrameMutex);
    memcpy(theRect, this->mPresentRect, sizeof(theRect));
    PlatMutexUnlock(this->mFrameMutex);
    if (theRect[2] > 0.0f && theRect[3] > 0.0f) {
        ioX = (int)floor((ioX - theRect[0] * theWidth) / (theRect[2] * theWidth) * this->mSurfaceWidth);
        ioY = (int)floor((ioY - theRect[1] * theHeight) / (theRect[3] * theHeight) * this->mSurfaceHeight);
        return;
    }
    double theScale = (std::min)((double)theWidth / this->mSurfaceWidth, (double)theHeight / this->mSurfaceHeight);
    double theLeft = (theWidth - this->mSurfaceWidth * theScale) / 2.0;
    double theTop = (theHeight - this->mSurfaceHeight * theScale) / 2.0;
    ioX = (int)floor((ioX - theLeft) / theScale);
    ioY = (int)floor((ioY - theTop) / theScale);
}

void PlatWindowManager::HandlePlatEvent(const PlatInputEvent& inEvent)
{
    if (inEvent.mWindow != NULL)
        this->mPlatWindow = inEvent.mWindow;
    bool theForward = false;
    switch (inEvent.mType) {
    case PLAT_EVENT_MOUSE_MOVE:
    case PLAT_EVENT_MOUSE_DOWN:
    case PLAT_EVENT_MOUSE_UP:
    case PLAT_EVENT_MOUSE_WHEEL:
        this->HandleMouse(inEvent);
        break;
    case PLAT_EVENT_KEY_DOWN:
    case PLAT_EVENT_KEY_UP:
        this->HandleKey(inEvent);
        break;
    case PLAT_EVENT_TEXT: {
        // WM_CHAR: one character event per byte of the committed text
        PlatGUIWindow* theTarget = this->mPlatFocusWindow;
        if (theTarget == NULL)
            break;
        int theMod = KeyModFromPlat(PlatModifiers());
        for (const char* p = inEvent.mText.mText; *p != 0; p++) {
            wchar_t theChar = 0;
            if (PlatCp1252ToWide(p, 1, &theChar, 1) == 1)
                theTarget->QueueEvent(new KeyEvent(InputEvent_KeyChar, (int)theChar, theMod));
        }
        break;
    }
    case PLAT_EVENT_FOCUS_GAINED:
        this->mAppActive = true;
        if (this->mPlatFocusWindow != NULL)
            this->mPlatFocusWindow->QueueEvent(new InputEvent(InputEvent_GotFocus));
        else {
            for (size_t i = this->mZOrder.size(); i-- > 0;)
                if (this->mZOrder[i]->mVisible && !this->mZOrder[i]->mIsPopup) {
                    this->SetFocusWindow(this->mZOrder[i]);
                    break;
                }
        }
        this->mDirty = true;
        theForward = true;
        break;
    case PLAT_EVENT_FOCUS_LOST:
        if (this->mPlatFocusWindow != NULL)
            this->mPlatFocusWindow->QueueEvent(new InputEvent(InputEvent_LostFocus));
        this->mAppActive = false;
        this->mDirty = true;
        theForward = true;
        break;
    case PLAT_EVENT_QUIT:
    case PLAT_EVENT_CLOSE: {
        // WM_CLOSE to the windows that fill the surface (the lobby's main window)
        std::vector<SmartPtr<Window> > theWindows;
        for (size_t i = 0; i < this->mZOrder.size(); i++)
            if (this->mZOrder[i]->FillsSurface())
                theWindows.push_back(this->mZOrder[i]);
        for (size_t i = 0; i < theWindows.size(); i++)
            theWindows[i]->Close();
        theForward = true;
        break;
    }
    case PLAT_EVENT_EXPOSED:
    case PLAT_EVENT_RESIZED:
    case PLAT_EVENT_MINIMIZED:
    case PLAT_EVENT_RESTORED:
        this->mDirty = true;
        theForward = true;
        break;
    default:
        break;
    }
    if (theForward && this->mEventForward != NULL)
        this->mEventForward(inEvent);
}

// Mouse input: to the capture window, else to the topmost window under the pointer; a press on a frame closes the
// popups (WM_NCLBUTTONDOWN) and, on a caption, drags the window unless another window is modal
void PlatWindowManager::HandleMouse(const PlatInputEvent& inEvent)
{
    int x = inEvent.mMouse.mX, y = inEvent.mMouse.mY;
    this->ScaleToSurface(inEvent, x, y);
    this->mMouseX = x;
    this->mMouseY = y;
    this->mHaveMouse = true;
    int theMod = KeyModFromPlat(inEvent.mMouse.mModifiers);
    int theButton = -1;
    if (inEvent.mType == PLAT_EVENT_MOUSE_DOWN || inEvent.mType == PLAT_EVENT_MOUSE_UP) {
        if (inEvent.mMouse.mButton >= 1 && inEvent.mMouse.mButton <= 3)
            theButton = (int)inEvent.mMouse.mButton - 1;
        else
            return;
    }

    if (this->mDragWindow != NULL) {
        if (inEvent.mType == PLAT_EVENT_MOUSE_MOVE)
            this->mDragWindow->SetPosSize(x - this->mDragOffsetX, y - this->mDragOffsetY, -1, -1);
        else if (inEvent.mType == PLAT_EVENT_MOUSE_UP && theButton == 0)
            this->mDragWindow = NULL;
        return;
    }

    PlatGUIWindow* theTarget = this->mCaptureWindow != NULL ? this->mCaptureWindow : this->WindowFromPoint(x, y);
    if (theTarget == NULL)
        return;
    if (this->mCaptureWindow == NULL) {
        WONRectangle theClient;
        theTarget->GetClientRect(theClient);
        if (!(x >= theClient.x && y >= theClient.y && x < theClient.x + theClient.width &&
              y < theClient.y + theClient.height)) {
            if (inEvent.mType == PLAT_EVENT_MOUSE_DOWN) {
                SmartPtr<Window> theHold = theTarget;
                theTarget->ClosePopup(NULL);
                if (!this->NotModalWindow(theTarget) && theTarget->mWindowState == Window::WindowState_Open) {
                    theTarget->PlatformSetFocus();
                    if (theButton == 0 && y < theClient.y && y >= theTarget->mY + kFrameBorder) {
                        this->mDragWindow = theTarget;
                        this->mDragOffsetX = x - theTarget->mX;
                        this->mDragOffsetY = y - theTarget->mY;
                    }
                }
            }
            return;
        }
    }

    switch (inEvent.mType) {
    case PLAT_EVENT_MOUSE_MOVE:
        theTarget->QueueMouse(InputEvent_MouseMove, x, y, theMod, 0);
        break;
    case PLAT_EVENT_MOUSE_DOWN: {
        // CS_DBLCLKS: the second press of a double click is a double-click event
        bool theDouble = inEvent.mMouse.mClicks >= 2 && (inEvent.mMouse.mClicks % 2) == 0;
        theTarget->QueueMouse(theDouble ? InputEvent_MouseDoubleClick : InputEvent_MouseDown, x, y, theMod, theButton);
        break;
    }
    case PLAT_EVENT_MOUSE_UP:
        theTarget->QueueMouse(InputEvent_MouseUp, x, y, theMod, theButton);
        break;
    case PLAT_EVENT_MOUSE_WHEEL: {
        int theAmount = (int)lround(inEvent.mMouse.mWheel * 120.0f);
        if (theAmount != 0)
            theTarget->QueueMouse(InputEvent_MouseWheel, x, y, theMod, (short)theAmount);
        break;
    }
    default:
        break;
    }
}

// Keys to the focus window (WM_KEYDOWN / WM_KEYUP with the virtual key), with the control characters
// TranslateMessage made of them (Backspace, Tab, Enter, Escape, Ctrl+letter); with Alt held they were system keys
// the window procedure left to DefWindowProc. A window that forwards its keys passes them on to the game.
void PlatWindowManager::HandleKey(const PlatInputEvent& inEvent)
{
    PlatGUIWindow* theTarget = this->mPlatFocusWindow;
    uint32_t theKey = inEvent.mKey.mVirtualKey;
    uint32_t theModifiers = inEvent.mKey.mModifiers;
    bool theDown = inEvent.mType == PLAT_EVENT_KEY_DOWN;
    bool theSystem = (theModifiers & PLAT_MOD_ALT) != 0 || theKey == VK_MENU || theKey == VK_F10;
    if (theTarget != NULL && theKey != 0) {
        int theMod = KeyModFromPlat(theModifiers);
        if (theSystem) {
            if (theDown && theKey == VK_TAB && theTarget->mMinimizeOnAltTab && this->mPlatWindow != NULL)
                PlatWindowMinimize(this->mPlatWindow);
        } else {
            theTarget->QueueEvent(new KeyEvent(theDown ? InputEvent_KeyDown : InputEvent_KeyUp, (int)theKey, theMod));
            int theChar = 0;
            if (theDown) {
                if (theKey == VK_BACK)
                    theChar = 8;
                else if (theKey == VK_TAB)
                    theChar = 9;
                else if (theKey == VK_RETURN)
                    theChar = 13;
                else if (theKey == VK_ESCAPE)
                    theChar = 27;
                else if ((theModifiers & PLAT_MOD_CONTROL) && theKey >= 'A' && theKey <= 'Z')
                    theChar = (int)(theKey - 'A' + 1);
            }
            if (theChar != 0)
                theTarget->QueueEvent(new KeyEvent(InputEvent_KeyChar, theChar, theMod));
        }
    }
    if (theTarget != NULL && theTarget->mForwardKeyMsgToParent && this->mEventForward != NULL)
        this->mEventForward(inEvent);
}

// ---- painting and compositing

const PlatFont* PlatWindowManager::GetCaptionFont()
{
    if (this->mCaptionFont == NULL)
        this->mCaptionFont = PlatFontCreate("Tahoma", -11, 1, 0, 0, 0);
    return this->mCaptionFont;
}

void PlatWindowManager::PaintAndComposite()
{
    std::vector<SmartPtr<Window> > theWindows;
    for (size_t i = 0; i < this->mZOrder.size(); i++)
        theWindows.push_back(this->mZOrder[i]);
    for (size_t i = 0; i < theWindows.size(); i++) {
        PlatGUIWindow* w = static_cast<PlatGUIWindow*>(theWindows[i].get());
        if (w->mRegistered && w->mVisible && w->mWindowState == Window::WindowState_Open)
            w->OnPaint();
    }
    if (this->mDirty)
        this->Composite();
}

// Fills the frame pixels of a logical rectangle
static void FrameFill(std::vector<uint32_t>& ioFrame, int inFrameWidth, int inFrameHeight, float inScale, int x, int y,
                      int width, int height, uint32_t inColor)
{
    if (width <= 0 || height <= 0)
        return;
    int theLeft = PlatGUIPixels(x, inScale), theTop = PlatGUIPixels(y, inScale);
    width = PlatGUIPixels(x + width, inScale) - theLeft;
    height = PlatGUIPixels(y + height, inScale) - theTop;
    x = theLeft;
    y = theTop;
    int theX1 = (std::max)(x, 0), theY1 = (std::max)(y, 0);
    int theX2 = (std::min)(x + width, inFrameWidth), theY2 = (std::min)(y + height, inFrameHeight);
    for (int j = theY1; j < theY2; j++)
        for (int i = theX1; i < theX2; i++)
            ioFrame[(size_t)j * inFrameWidth + i] = 0xFF000000u | inColor;
}

// Composites the visible windows into the frame (under the frame lock) and bumps its serial
void PlatWindowManager::Composite()
{
    std::vector<PlatGUIWindow*> theOrder;
    DrawingOrder(this->mZOrder, theOrder);
    PlatMutexLock(this->mFrameMutex);
    float k = this->mPixelScale;
    int W = PlatGUIPixels(this->mSurfaceWidth, k), H = PlatGUIPixels(this->mSurfaceHeight, k);
    this->mFrameWidth = W;
    this->mFrameHeight = H;
    this->mFrameLogicalWidth = this->mSurfaceWidth;
    this->mFrameLogicalHeight = this->mSurfaceHeight;
    this->mFrame.assign((size_t)W * H, 0xFF000000u);
    for (size_t n = 0; n < theOrder.size(); n++) {
        PlatGUIWindow* w = theOrder[n];
        if (!w->mVisible)
            continue;
        if (w->HasFrame()) {
            int x = w->mX, y = w->mY, ww = w->mWidth, hh = w->mHeight;
            FrameFill(this->mFrame, W, H, k, x, y, ww, hh, kClassic3DFace);
            FrameFill(this->mFrame, W, H, k, x, y, ww, 1, kClassic3DFace);
            FrameFill(this->mFrame, W, H, k, x, y, 1, hh, kClassic3DFace);
            FrameFill(this->mFrame, W, H, k, x, y + hh - 1, ww, 1, kClassic3DDarkShadow);
            FrameFill(this->mFrame, W, H, k, x + ww - 1, y, 1, hh, kClassic3DDarkShadow);
            FrameFill(this->mFrame, W, H, k, x + 1, y + 1, ww - 2, 1, kClassic3DHilight);
            FrameFill(this->mFrame, W, H, k, x + 1, y + 1, 1, hh - 2, kClassic3DHilight);
            FrameFill(this->mFrame, W, H, k, x + 1, y + hh - 2, ww - 2, 1, kClassic3DShadow);
            FrameFill(this->mFrame, W, H, k, x + ww - 2, y + 1, 1, hh - 2, kClassic3DShadow);
            bool theActive = w == this->mPlatFocusWindow && this->mAppActive;
            int cx = x + kFrameBorder, cy = y + kFrameBorder, cw = ww - 2 * kFrameBorder;
            FrameFill(this->mFrame, W, H, k, cx, cy, cw, kFrameCaption,
                      theActive ? kClassicCaptionActive : kClassicCaptionInactive);
            const PlatFont* theFont = PlatFontScaled(this->GetCaptionFont(), k);
            if (theFont != NULL && cw > 4) {
                std::wstring theWide = w->mTitle;
                std::string theTitle = PlatGUIToCp1252(theWide.c_str(), (unsigned short)theWide.size());
                PlatFontMetrics theMetrics;
                PlatFontGetMetrics(theFont, &theMetrics);
                PlatFontTarget theTarget;
                theTarget.mPixels = this->mFrame.data();
                theTarget.mPitch = W * 4;
                theTarget.mWidth = W;
                theTarget.mHeight = H;
                theTarget.mFlags = 0;
                theTarget.mScale = 0.0f;
                int theCaptionTop = PlatGUIPixels(cy, k), theCaptionBottom = PlatGUIPixels(cy + kFrameCaption, k);
                PlatFontRect theClip = {(std::max)(PlatGUIPixels(cx, k), 0), (std::max)(theCaptionTop, 0),
                                        (std::min)(PlatGUIPixels(cx + cw - 2, k), W), (std::min)(theCaptionBottom, H)};
                if (theClip.mRight > theClip.mLeft && theClip.mBottom > theClip.mTop)
                    PlatFontDraw(theFont, theTitle.data(), (uint32_t)theTitle.size(), PlatGUIPixels(cx + 3, k),
                                 theCaptionTop + (theCaptionBottom - theCaptionTop - theMetrics.mHeight) / 2, 0xFFFFFF,
                                 0, 0, &theClip, &theTarget);
            }
        }
        WONRectangle theClient;
        w->GetClientRect(theClient);
        PlatNativeImage* theImage = dynamic_cast<PlatNativeImage*>(w->mOffscreenImage.get());
        if (theImage == NULL)
            continue;
        // in frame pixels (an image still at another scale, until it repaints, shows with its pixels as they are)
        int theClientX = PlatGUIPixels(theClient.x, k), theClientY = PlatGUIPixels(theClient.y, k);
        int theWidth = (std::min)(theImage->GetPixelWidth(), PlatGUIPixels(theClient.x + theClient.width, k) - theClientX);
        int theHeight =
            (std::min)(theImage->GetPixelHeight(), PlatGUIPixels(theClient.y + theClient.height, k) - theClientY);
        const uint32_t* thePixels = theImage->GetPixels();
        int thePitch = theImage->GetPitchPixels();
        int theX1 = (std::max)(theClientX, 0), theY1 = (std::max)(theClientY, 0);
        int theX2 = (std::min)(theClientX + theWidth, W), theY2 = (std::min)(theClientY + theHeight, H);
        for (int j = theY1; j < theY2; j++) {
            const uint32_t* theFrom = thePixels + (size_t)(j - theClientY) * thePitch + (theX1 - theClientX);
            uint32_t* theTo = this->mFrame.data() + (size_t)j * W + theX1;
            for (int i = theX1; i < theX2; i++)
                *theTo++ = *theFrom++ | 0xFF000000u;
        }
    }
    if (++this->mFrameSerial == 0)
        this->mFrameSerial = 1;
    PlatMutexUnlock(this->mFrameMutex);
    this->mDirty = false;
}

} // namespace WONAPI
