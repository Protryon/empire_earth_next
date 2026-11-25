// Graphics (R6 WONGUI B round 1): translated drawing, colours and the clip stack are ee_won's gfxobj.rs. This unit
// keeps the constructor, the destructor, the display context's attachment and the font (C++ objects), one-line
// forwarders, and the host: each hook a virtual on this object, an image's draw or the font's.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stddef.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "rust/won_gui/gfxobj.h"
#include "rust/won_gui/guilayout.h"
#include "rust/won_gui/gfxplat.h"
#include "ee/wonapi/WONGUIDraw.h"
using namespace WONAPI;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static_assert(offsetof(Graphics, mTransY) == offsetof(Graphics, mTransX) + offsetof(EeWonGfxState, trans_y) &&
              offsetof(Graphics, mColor) == offsetof(Graphics, mTransX) + offsetof(EeWonGfxState, color) &&
              offsetof(Graphics, mClipRect) == offsetof(Graphics, mTransX) + offsetof(EeWonGfxState, clip) &&
              sizeof(WONRectangle) == sizeof(EeWonRect), "gfxobj.rs EeWonGfxState");
#pragma clang diagnostic pop

typedef EeWonGfxHook H;
// the Rust Graphics' calls out that are not the surface's (gfxplat.rs): an image's draw (a[] its ints, p the image)
// and the font's (p the text)
static uint32_t GfxHost(void* inGraphics, EeWonGfxHook inHook, const int32_t* a, const void* p)
{
    Graphics* g = (Graphics*)inGraphics;
    switch (inHook) {
    case H::ImageEntire: ((Image*)p)->DrawEntire(g->mDisplayContext.get(), a[0], a[1]); return 0;
    case H::ImagePart: ((Image*)p)->Draw(g->mDisplayContext.get(), a[0], a[1], a[2], a[3], a[4], a[5]); return 0;
    case H::String: g->mFont->DrawString(g->mDisplayContext.get(), *(const GUIString*)p, (unsigned short)a[0], (unsigned short)a[1], a[2], a[3]); return 0;
    default: return 0;
    }
}

DWORD Graphics::GCall(EeWonGfxCall c, int a, int b, int cc, int d, int e, int f, int g, const void* p)
{
    int32_t anArgs[7] = { a, b, cc, d, e, f, g };
    return ee_won_gfx_plat_call(this, GfxHost, this->mGfx, (EeWonGfxState*)&this->mTransX,
                                this->mColorScheme != NULL ? (const uint32_t*)this->mColorScheme->mColors : NULL,
                                this->mPlat.surface, this->mPlat.scale, c, anArgs, p);
}

// Creates a graphics context with no translation, no colour scheme, an empty clip stack and no surface
Graphics::Graphics()
{
    this->mTransX = this->mTransY = 0;
    this->mColorScheme = NULL;
    this->mGfx = ee_won_gfx_new();
    this->mPlat = ee_won_gfx_plat_none();
}
// Destroys the graphics context, detaching it first
Graphics::~Graphics()
{
    this->Detach();
    ee_won_gfx_free(this->mGfx);
}
// Leaves the surface and the display context (nothing when attached to no surface)
void Graphics::Detach()
{
    if (this->mPlat.surface == NULL)
        return;
    this->mPlat.surface = NULL;
    this->mDisplayContext = NULL;
}
// Attaches the graphics context to a display context and links the display context back to it; draws into the
// context's surface at its pixel scale, the colour black (MSGraphics' black pen and brush)
void Graphics::Attach(DisplayContext *theContext)
{
    this->Detach();
    this->mDisplayContext = theContext;
    this->mDisplayContext->mGraphics = this;
    this->mPlat = ee_won_gfx_plat_of(theContext->mSurface, theContext->mPixelScale);
    this->mColor = 0;
}
// Selects a font into the display context when it differs from the current one
void Graphics::SetFont(Font *theFont)
{
    if (theFont != this->mFont.get()) {
        this->mFont = theFont;
        this->mFont->PrepareContext(this->mDisplayContext.get());
    }
}

// Windows-style frames (MSGraphicsUtil.cpp folded in, R6 WON GUI round 37): mspaint.rs's plans run on the Graphics
namespace WONAPI {
// the standard colours and directions mspaint.rs names
static_assert(StandardColor_3DDarkShadow==0 && StandardColor_3DFace==1 && StandardColor_3DHilight==2 &&
              StandardColor_3DShadow==3 && StandardColor_Scrollbar==4 && StandardColor_ButtonText==5 &&
              StandardColor_GrayText==6 && StandardColor_MenuBack==11 && StandardColor_Back==13 &&
              StandardColor_Text==14,"mspaint.rs SYS_*");
static_assert(Direction_Left==0 && Direction_Right==1 && Direction_Up==2 && Direction_Down==3,"mspaint.rs DIR_*");

static void MsDraw(Graphics& g, EeWonMsPaintKind k, int a, int b, int w, int h, bool down, bool checked=false)
{
    WONMsPaint(g,WONMsPaintOf(k,w,h,(down?EE_WON_MSPAINT_PRESSED:0)|(checked?EE_WON_MSPAINT_CHECKED:0),a,b));
}

// Draws a two-pixel 3D frame, raised or sunken (down), at the given position
void LobbyDraw3DFrame(Graphics &g, int x, int y, int width, int height, bool down) { MsDraw(g,EeWonMsPaintKind::Frame3D,x,y,width,height,down); }
// Draws a raised button frame, or a flat shadow-coloured rectangle when down
void LobbyDrawFrame(Graphics &g, int x, int y, int width, int height, bool down) { MsDraw(g,EeWonMsPaintKind::Frame,x,y,width,height,down); }
}
