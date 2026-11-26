// Graphics (R6 WONGUI B round 1): translated drawing, colours and the clip stack are ee_won's gfxobj.rs, its members
// gfxrun.rs's record and the draw plans run there (round 38). This unit keeps the destructor, the display context's
// attachment and the font (C++ objects), and the host: an image's draw and size, the font's draw, a wrapped text's.
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
#include "rust/won_gui/guilayout.h"
#include "ee/wonapi/WONGUIDraw.h"
using namespace WONAPI;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static_assert(offsetof(Graphics, mTransX) == offsetof(EeWonGraphics, st) &&
              offsetof(Graphics, mTransY) == offsetof(EeWonGraphics, st) + offsetof(EeWonGfxState, trans_y) &&
              offsetof(Graphics, mColor) == offsetof(EeWonGraphics, st) + offsetof(EeWonGfxState, color) &&
              offsetof(Graphics, mClipRect) == offsetof(EeWonGraphics, st) + offsetof(EeWonGfxState, clip) &&
              offsetof(Graphics, mGfx) == offsetof(EeWonGraphics, gfx) &&
              offsetof(Graphics, mColorScheme) == offsetof(EeWonGraphics, scheme) &&
              offsetof(Graphics, mPlat) == offsetof(EeWonGraphics, plat) &&
              sizeof(WONRectangle) == sizeof(EeWonRect) && sizeof(WONDrawSlots) == sizeof(EeWonDrawSlots) &&
              offsetof(WONDrawSlots, mText) == offsetof(EeWonDrawSlots, text), "gfxrun.rs EeWonGraphics, EeWonDrawSlots");
#pragma clang diagnostic pop

typedef EeWonGfxHook H;
// the Rust Graphics' calls out that are not the surface's (gfxplat.rs, gfxrun.rs): an image's draw and size (a[]
// its ints, p the image), the font's draw and the font (p the text, the font) and a wrapped text's paint
static uint32_t GfxHost(void* inGraphics, EeWonGfxHook inHook, const int32_t* a, const void* p)
{
    Graphics* g = (Graphics*)inGraphics;
    switch (inHook) {
    case H::ImageEntire: ((Image*)p)->DrawEntire(g->mDisplayContext.get(), a[0], a[1]); return 0;
    case H::ImagePart: ((Image*)p)->Draw(g->mDisplayContext.get(), a[0], a[1], a[2], a[3], a[4], a[5]); return 0;
    case H::String: g->mFont->DrawString(g->mDisplayContext.get(), *(const GUIString*)p, (unsigned short)a[0], (unsigned short)a[1], a[2], a[3]); return 0;
    case H::SetFont: g->SetFont((Font*)p); return 0;
    case H::Text: ((WrappedText*)p)->Paint(*g, a[0], a[1]); return 0;
    case H::ImageSize: ((Image*)p)->SetSize(a[0], a[1]); return 0;
    default: return 0;
    }
}
[[maybe_unused]] static const bool sGfxHost = (ee_won_graphics_register(GfxHost, offsetof(ColorScheme, mColors)), true);

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
// Draws a two-pixel 3D frame, raised or sunken (down), at the given position
void LobbyDraw3DFrame(Graphics &g, int x, int y, int width, int height, bool down) { WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Frame3D,width,height,down?EE_WON_MSPAINT_PRESSED:0,x,y)); }
// Draws a raised button frame, or a flat shadow-coloured rectangle when down
void LobbyDrawFrame(Graphics &g, int x, int y, int width, int height, bool down) { WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Frame,width,height,down?EE_WON_MSPAINT_PRESSED:0,x,y)); }
}
