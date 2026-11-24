// The WONAPI GUI's backend on the platform layer (every system), in place of the original's Win32 GDI backend
// (deleted in round 18). The lobby is not a set of native windows any more: its windows are rectangles of one lobby
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
#include "rust/won_gui/platcursor.h"
#include "rust/won_gui/platfont.h"
#include "rust/won_gui/platimg.h"
#include "rust/won_gui/platwin.h"
#include "rust/won_gui/platwm.h"

namespace WONAPI {

// R6 WON GUI round 35: the platform manager and window are WindowManager and Window themselves (WONLobbyGUI.h;
// the classes folded: nothing else derived from either); the names stay for the game's glue
typedef WindowManager PlatWindowManager;
typedef Window PlatGUIWindow;

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
    SurfaceBase* mSurface;                  // the context's, borrowed
    float mScale;                           // the context's pixel scale (1: GDI's results, pixel for pixel)
protected:
    // the hook inOp (EeWonPlatGfx) of ee_won's platgfx.rs on the surface, the scale and the colour
    DWORD Hook(int inOp, int a = 0, int b = 0, int c = 0, int d = 0, int e = 0, int f = 0);
    void Polygon(POINT* inPoints, int inCount, bool inFill);
};

// a logical coordinate or size at a pixel scale (the one rounding every part of the backend and the game uses)
inline int PlatGUIPixels(int inLogical, float inScale)
{
    return inScale == 1.0f ? inLogical : (int)floorf((float)inLogical * inScale + 0.5f);
}

// MSNativeImage's replacement: a 32-bit image in a software surface (WONLobbyGUI.h NativeImage, imgobj.rs; the
// texture pack's copies of the lobby's images are its decoder's)
typedef NativeImage PlatNativeImage;

class PlatNativeIcon : public NativeIcon {
public:
    PlatNativeIcon(DisplayContext* inContext, NativeImage* inImage) : NativeIcon(inContext), mImage(inImage) {}
    virtual ~PlatNativeIcon();
    SmartPtr<NativeImage> mImage;
};

// a pointer shape: one of the system's, or pixels (0xAARRGGBB) made into a platform cursor on first use (ee_won's
// platcursor.rs, R6 WON round 41)
class PlatCursor : public Cursor {
protected:
    virtual ~PlatCursor() { ee_won_plat_cursor_free(this->mRust); }
public:
    explicit PlatCursor(PlatSystemCursor inSystem) : mRust(ee_won_plat_cursor_system(&kFns, (uint32_t)inSystem)) {}
    PlatCursor(const uint32_t* inPixels, int inWidth, int inHeight, int inHotX, int inHotY)
        : mRust(ee_won_plat_cursor_pixels(&kFns, inPixels, inWidth, inHeight, inHotX, inHotY)) {}
    static const EeWonPlatCursorFns kFns;   // the platform's cursor calls (PlatNativeImage.cpp)
    virtual Cursor* GetNative() { return this; }
    void Apply() { ee_won_plat_cursor_apply(this->mRust); }   // makes it the pointer's shape (UI thread)
    EeWonPlatCursor* mRust;
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
    EeWonPlatFont* mRust;                   // the font (platfont.rs)
};

// wide text (GUIString) -> UTF-8 bytes (decision D6)
std::string PlatGUIToUtf8(const wchar_t* inText, unsigned short inLength);

} // namespace WONAPI

#endif
