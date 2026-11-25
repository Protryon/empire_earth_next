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
//  - Drawing: Graphics, images and fonts draw into software surfaces (surface.rs, 32-bit 0x00RRGGBB, B G R x in
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
#include "rust/won_gui/surface.h"
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
// (so are the platform graphics and font: Graphics and Font)

Window* CreateNativeWindow();

// ---- drawing (backend internals)

// Graphics on a DisplayContext's surface (MSGraphics' replacement): GDI's results per hook
inline int PlatGUIPixels(int inLogical, float inScale)
{
    return inScale == 1.0f ? inLogical : (int)floorf((float)inLogical * inScale + 0.5f);
}

// MSNativeImage's replacement: a 32-bit image in a software surface (WONLobbyGUI.h NativeImage, imgobj.rs; the
// texture pack's copies of the lobby's images are its decoder's)
typedef NativeImage PlatNativeImage;

// a pointer shape: one of the system's, or pixels (0xAARRGGBB) made into a platform cursor on first use (ee_won's
// platcursor.rs, R6 WON round 41)
// (PlatCursor is Cursor's platform kind: cursorobj.rs over platcursor.rs; R6 WON GUI round 35)

// a font on platform/PlatFont.h (MSFont's replacement)
} // namespace WONAPI

#endif
