// The WONAPI GUI's backend on the platform layer (every system), in place of the original's Win32 GDI backend
// (WONLobbyGUIMS.h). The lobby is not a set of native windows any more: its windows are rectangles of one lobby
// surface the game shows in its own window.
//
//  - PlatWindowManager (the WindowManager) owns the event loop while the lobby runs: GetNextEvent waits for a
//    PlatInputEvent (platform/PlatWindow.h), translates it into WONAPI window events for the window under the
//    pointer / with the focus / with the mouse capture, and runs ProcessEvents (at least every 50 ms, the original's
//    window timer); Signal (any thread) wakes it with PlatWindowWake. After the events it paints the windows whose
//    root is invalid into their offscreen images and composites the visible windows, in z order, into the frame.
//  - PlatGUIWindow (the Window): a window without a parent fills the whole surface (the lobby's main window); a
//    window with a parent is placed in the surface, with a classic frame and caption unless its style has 0x20
//    (WS_POPUP: popups and borderless dialogs). Mouse coordinates are client pixels of the game window scaled to the
//    surface (client size vs surface size).
//  - Drawing: Graphics, images and fonts draw into software surfaces (WONSurface.h, 32-bit 0x00RRGGBB, B G R x in
//    memory) with GDI's rules (end points, rectangle edges, text cell, mask blits); text through platform/PlatFont.h
//    (Windows-1252: other characters show as '?').
//  - Pixel scale: the surface is LOGICAL (the 1024x768 the lobby is laid out for: every window rectangle, event,
//    hit test and drawing coordinate); the frame and the windows' offscreen images hold k times as many pixels
//    (SetPixelScale; the game sets k so the frame maps 1:1 onto the window's pixels). DisplayContext::mPixelScale
//    is a surface's k: PlatGraphics scales coordinates, rectangles, the clip and line widths by it, PlatGUIFont
//    rasterises at the font size times k, and PlatNativeImage::Draw resamples between images of different scales
//    (bilinear, never across a transparency mask's edge or out of the source rectangle; the mask itself is applied
//    at its logical pixels, so transparent edges stay where they were). Images decoded from files are at scale 1,
//    or at their pack scale when Data/Textures-4x has a larger copy (PlatGUIPackImage).
//
// The game's side (EELobbyInterface.cpp, RTSGraphics::RenderLobby) uses only the "game interface" below.
// Threads: everything runs on the UI thread (the thread that created the manager, the window system's thread) except
// Signal, SetSurfaceSize and LockFrame/UnlockFrame, which any thread may call. Lock order: the game's EELobbyLock,
// then the frame lock.
#ifndef EE_WONAPI_WONLOBBYPLATFORM_H
#define EE_WONAPI_WONLOBBYPLATFORM_H

#include <stdint.h>
#include <math.h>
#include <string>
#include <vector>
#include "platform/PlatWindow.h"
#include "platform/PlatThread.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONSurface.h"

struct PlatFont;

namespace WONAPI {

class PlatGUIWindow;

class PlatWindowManager : public WindowManager {
public:
    // ---- game interface (stable)
    typedef void (*EventForwardFunc)(const PlatInputEvent& inEvent);
    // Creates the manager (it becomes the default window manager) on the UI thread; the standard cursors are set.
    PlatWindowManager();
    virtual ~PlatWindowManager();
    // The lobby surface's size in pixels (the frame LockFrame returns; the screen the windows live on). Windows
    // without a parent follow it. Any thread: applied at once on the UI thread, otherwise at the next GetNextEvent.
    void SetSurfaceSize(int inWidth, int inHeight);
    // The surface's pixels per logical pixel (>= 1; the frame is the surface's size times it, rounded:
    // PlatGUIPixels). Any thread, as SetSurfaceSize. Windows repaint at the new scale.
    void SetPixelScale(float inScale);
    float GetPixelScale() const { return this->mPixelScale; }
    // Where the game shows the frame in its window, as fractions of the window's client area (left, top, width,
    // height): the pointer is mapped back through it (ScaleToSurface). Before the first call: the surface scaled
    // uniformly to fit, centred. Any thread.
    void SetPresentRect(float inLeft, float inTop, float inWidth, float inHeight);
    // The last composited frame: 32-bit pixels 0xFFRRGGBB (B, G, R, 0xFF in memory), outWidth x outHeight pixels,
    // outPitch bytes per row, showing outLogicalWidth x outLogicalHeight of the surface, and a serial that changes
    // with every new frame -> false when nothing was composited yet (nothing to unlock then). Holds the frame lock
    // until UnlockFrame: copy the pixels out and unlock. Any thread.
    bool LockFrame(const uint32_t*& outPixels, int& outWidth, int& outHeight, int& outPitch, int& outLogicalWidth,
                   int& outLogicalHeight, uint32_t& outSerial);
    void UnlockFrame();
    // The platform events the game must still see while the lobby owns the event loop: QUIT, CLOSE, RESIZED,
    // MINIMIZED, RESTORED, FOCUS_GAINED, FOCUS_LOST, EXPOSED, and KEY_DOWN/KEY_UP when they go to a window that
    // forwards its keys (PlatGUIWindow::mForwardKeyMsgToParent; the original sent them to the game window). Called
    // on the UI thread, after the lobby handled the event. NULL: none.
    void SetEventForward(EventForwardFunc inFunc);

    // ---- WindowManager
    virtual void GetNextEvent();
    virtual bool SetScreenResolutionHook(int inWidth, int inHeight);
    virtual void GetScreenResolutionHook(int& outWidth, int& outHeight);
    virtual void Signal();
    virtual void GetScreenRect(WONRectangle& outRect);
    virtual void GetSystemColorScheme();

    // ---- backend (PlatGUIWindow)
    void AddPlatWindow(PlatGUIWindow* inWindow);
    void RemovePlatWindow(PlatGUIWindow* inWindow);
    void BringToFront(PlatGUIWindow* inWindow);
    void SetFocusWindow(PlatGUIWindow* inWindow);
    void SetCaptureWindow(PlatGUIWindow* inWindow) { this->mCaptureWindow = inWindow; }
    PlatGUIWindow* GetCaptureWindow() { return this->mCaptureWindow; }
    PlatGUIWindow* GetFocusWindow() { return this->mPlatFocusWindow; }
    // the topmost visible window whose window rectangle holds the surface point (WindowFromPoint)
    PlatGUIWindow* WindowFromPoint(int inX, int inY);
    bool GetMousePosition(int& outX, int& outY) { outX = this->mMouseX; outY = this->mMouseY; return this->mHaveMouse; }
    void SetDirty() { this->mDirty = true; }
    int GetSurfaceWidth() const { return this->mSurfaceWidth; }
    int GetSurfaceHeight() const { return this->mSurfaceHeight; }
    PlatWindow* GetPlatWindow() { return this->mPlatWindow; }
    const PlatFont* GetCaptionFont();
    // paints every window whose root is invalid, then composites when anything changed
    void PaintAndComposite();

protected:
    void ApplySurfaceSize(int inWidth, int inHeight);
    void ApplyPixelScale(float inScale);
    void HandlePlatEvent(const PlatInputEvent& inEvent);
    void HandleMouse(const PlatInputEvent& inEvent);
    void HandleKey(const PlatInputEvent& inEvent);
    void Composite();
    void ScaleToSurface(const PlatInputEvent& inEvent, int& ioX, int& ioY);

    PlatMutex* mFrameMutex;                     // the frame, its size and serial, the pending surface size
    std::vector<uint32_t> mFrame;
    int mFrameWidth, mFrameHeight;              // pixels
    int mFrameLogicalWidth, mFrameLogicalHeight;
    uint32_t mFrameSerial;
    int mPendingWidth, mPendingHeight;          // a SetSurfaceSize from another thread (0: none)
    float mPendingScale;                        // a SetPixelScale from another thread (0: none)
    float mPixelScale;                          // the frame's and the windows' images' pixels per surface pixel
    float mPresentRect[4];                      // SetPresentRect's (under the frame lock; width 0: none)
    int mSurfaceWidth, mSurfaceHeight;
    volatile int32_t mWakePending;             // Signals since the loop last ran (PlatAtomicIncrement)
    EventForwardFunc mEventForward;
    std::vector<PlatGUIWindow*> mZOrder;        // bottom to top
    PlatGUIWindow* mPlatFocusWindow;            // the window with the keyboard focus (WM_SETFOCUS's)
    PlatGUIWindow* mCaptureWindow;
    PlatGUIWindow* mDragWindow;                 // a framed window dragged by its caption
    int mDragOffsetX, mDragOffsetY;
    bool mAppActive;                            // the game window has the system's focus
    bool mDirty;                                // composite at the next opportunity
    int mMouseX, mMouseY;                       // the pointer in surface pixels
    bool mHaveMouse;
    PlatWindow* mPlatWindow;                    // the game window (from its events)
    uint32_t mLastProcessTick;
    PlatFont* mCaptionFont;
};

// A WONAPI window on the lobby surface (MSWindow's replacement; CreateNativeWindow makes one).
class PlatGUIWindow : public Window {
public:
    PlatGUIWindow();
    virtual ~PlatGUIWindow();
    virtual SmartPtr<Font> PlatformGetFont(const FontDescriptor& inDescriptor);
    virtual bool IdleUpdate();
    virtual void TimerEvent(DWORD inElapsed);
    virtual void PlatformSetTitle();
    virtual void PlatformSetIcon();
    virtual void PlatformSetCursor(Cursor* inCursor);
    virtual void PlatformShowNativeCursor(bool inShow);
    virtual void PlatformShow(bool inShow);
    virtual void PlatformMinimize(bool inMinimize);
    virtual void PlatformSetTopMost(bool inTopMost);
    virtual void PlatformClose();
    virtual void Create(const WONRectangle& inRect);
    virtual void SetPosSize(int inX, int inY, int inWidth, int inHeight);
    virtual void GetRect(WONRectangle& outRect, bool inClientArea);
    virtual void PlatformSetFocus();
    virtual void PlatformSetMouseCapture(bool inOn);
    virtual bool PlatformIsVisible();

    void SetMinimizeOnAltTab(bool inMinimize) { this->mMinimizeOnAltTab = inMinimize; }
    void SetForwardKeyMsgToParent(bool inForward) { this->mForwardKeyMsgToParent = inForward; }
    // the window fills the surface (no parent): no frame, the surface's size, never moved
    bool FillsSurface() const { return this->mParent == NULL; }
    bool HasFrame() const { return this->mParent != NULL && !(this->mStyle & 0x20); }
    void GetClientRect(WONRectangle& outRect);
    void FollowSurface();                       // a window that fills the surface takes its new size
    void FollowPixelScale();                    // a new offscreen image at the manager's pixel scale, repainted
    void OnPaint();                             // repaints an invalid root into the offscreen image
    void QueueMouse(int inType, int inSurfaceX, int inSurfaceY, int inMod, int inButtonOrAmount);
    PlatWindowManager* GetPlatManager();

    int mX, mY, mWidth, mHeight;                // the window rectangle in surface pixels (frame included)
    bool mVisible;
    bool mTopMost;
    bool mRegistered;
    bool mMinimizeOnAltTab;
    bool mForwardKeyMsgToParent;
};

// The factory the WONAPI code uses where the original said `new MSWindow`.
Window* CreateNativeWindow();

// ---- drawing (backend internals)

// Graphics on a DisplayContext's surface (MSGraphics' replacement): GDI's results per hook
class PlatGraphics : public Graphics {
public:
    PlatGraphics();
    virtual ~PlatGraphics();
    virtual void Attach(DisplayContext* inContext);
    virtual void Detach();
    virtual void SetColorHook();
    virtual DWORD GetPixelHook(int x, int y);
    virtual void SetPixelHook(int x, int y, DWORD inColor);
    virtual void DrawLineHook(int x1, int y1, int x2, int y2);
    virtual void DrawRectHook(int x, int y, int width, int height);
    virtual void DrawDottedRectHook(int x, int y, int width, int height);
    virtual void FillRectHook(int x, int y, int width, int height);
    virtual bool BlendSupported();
    virtual void FillRectBlendHook(int x, int y, int width, int height, int inBlendAmount);
    virtual void DrawPolygonHook(POINT* inPoints, int inCount);
    virtual void FillPolygonHook(POINT* inPoints, int inCount);
    virtual void DrawEllipseHook(int x, int y, int width, int height);
    virtual void FillEllipseHook(int x, int y, int width, int height);
    virtual void DrawArcHook(int x, int y, int width, int height, int inStartAngle, int inArcAngle);
    virtual void ClearClipRectHook();
    virtual void SetClipRectHook(int x, int y, int width, int height);
    SurfaceBase* PrepareSurface();          // the surface with the current colour, NULL when there is none
    SurfaceBase* mSurface;                  // the context's, borrowed
    float mScale;                           // the context's pixel scale (1: GDI's results, pixel for pixel)
protected:
    int PX(int v) const;                    // a logical coordinate -> the surface's pixels
    void FillLogicalRect(SurfaceBase* inSurface, int x, int y, int width, int height);   // pixels covering it
    void ScaledLine(SurfaceBase* inSurface, int x1, int y1, int x2, int y2);   // a line one logical pixel wide
};

// a logical coordinate or size at a pixel scale (the one rounding every part of the backend and the game uses)
inline int PlatGUIPixels(int inLogical, float inScale)
{
    return inScale == 1.0f ? inLogical : (int)floorf((float)inLogical * inScale + 0.5f);
}

// a 1-bit mask (RawImage1's rows: MSB first, rows padded to 4 bytes); a set bit is a transparent pixel
class PlatNativeMask : public NativeMask {
public:
    PlatNativeMask(DisplayContext* inContext, int inWidth, int inHeight, int inBytesPerRow, const unsigned char* inBits);
    virtual ~PlatNativeMask();
    bool IsTransparent(int x, int y) const
    { return (this->mBits[(size_t)y * this->mBytesPerRow + (x >> 3)] & (0x80 >> (x & 7))) != 0; }
    int mWidth, mHeight, mBytesPerRow;
    std::vector<unsigned char> mBits;
};

// a 32-bit image in a software surface (MSNativeImage's replacement: a device-dependent bitmap)
class PlatNativeImage : public NativeImage {
public:
    // cleared to black; inWidth x inHeight logical pixels, inScale times as many pixels (DisplayContext::mPixelScale)
    PlatNativeImage(DisplayContext* inContext, int inWidth, int inHeight, float inScale = 1.0f);
    virtual ~PlatNativeImage();
    virtual Graphics* PlatformGetGraphics();
    virtual void Draw(DisplayContext* inContext, int x, int y, int left, int top, int width, int height);
    virtual SmartPtr<RawImage32> GetRaw();
    virtual SmartPtr<Image> Duplicate();
    virtual SmartPtr<NativeIcon> GetIcon();
    SmartPtr<Cursor> GetCursor(int inHotX, int inHotY);
    uint32_t* GetPixels();
    int GetPitchPixels();
    int GetPixelWidth() { return this->mSurface->GetWidth(); }
    int GetPixelHeight() { return this->mSurface->GetHeight(); }
    float GetScale() const { return this->mScale; }
    // Draw onto a surface of another pixel scale (or both scaled): logical rectangle, resampled
    void DrawScaled(SurfaceBase* inDest, float inDestScale, int x, int y, int left, int top, int width, int height);
    SmartPtr<SurfaceBase> mSurface;
    float mScale;
};

// An image decoded from inPath with the texture pack's copy when there is one: with the pack on (the Texture
// Resolution setting, GETextureManager::GetTexturePackLogicalShift), Data/Textures-4x/WONLobby Resources/<the
// file's path under WONLobby Resources> (the same name, or .png) at an integer multiple of inImage's size becomes
// an image of inImage's logical size and transparency mask at that pixel scale (halved at the 2x setting). Else
// inImage itself.
SmartPtr<NativeImage> PlatGUIPackImage(DisplayContext* inContext, NativeImage* inImage, const char* inPath);

class PlatNativeIcon : public NativeIcon {
public:
    PlatNativeIcon(DisplayContext* inContext, NativeImage* inImage) : NativeIcon(inContext), mImage(inImage) {}
    virtual ~PlatNativeIcon();
    SmartPtr<NativeImage> mImage;
};

// a pointer shape: one of the system's, or pixels (0xAARRGGBB) made into a platform cursor on first use
class PlatCursor : public Cursor {
protected:
    virtual ~PlatCursor();
public:
    explicit PlatCursor(PlatSystemCursor inSystem);
    PlatCursor(const uint32_t* inPixels, int inWidth, int inHeight, int inHotX, int inHotY);
    virtual Cursor* GetNative() { return this; }
    void Apply();                           // makes it the pointer's shape (UI thread)
    bool mIsSystem;
    PlatSystemCursor mSystem;
    std::vector<uint32_t> mPixels;
    int mWidth, mHeight, mHotX, mHotY;
    void* mHandle;                          // PlatCursorCreate's
    bool mFailed;
};

// a font on platform/PlatFont.h (MSFont's replacement)
class PlatGUIFont : public Font {
public:
    PlatGUIFont(DisplayContext* inContext, const FontDescriptor& inDescriptor);
    virtual ~PlatGUIFont();
    virtual void PrepareContext(DisplayContext* inContext);
    virtual void DrawString(DisplayContext* inContext, const GUIString& inText, unsigned short inOffset,
                            unsigned short inLength, int x, int y);
    virtual int GetStringWidth(const GUIString& inText, unsigned short inOffset, unsigned short inLength);
    virtual int GetCharacterWidth(int inChar);
    virtual int GetHeight();
    virtual int GetAscent();
    virtual int GetDescent();
    const PlatFont* FontForScale(float inScale);   // mFont rasterised at its size times inScale
    PlatFont* mFont;
    PlatFont* mScaledFont;                  // the last FontForScale's (owned)
    float mScaledFontScale;
    int mHeight, mAscent, mDescent;
};

// wide text (GUIString) -> UTF-8 bytes (decision D6)
std::string PlatGUIToUtf8(const wchar_t* inText, unsigned short inLength);

} // namespace WONAPI

#endif
