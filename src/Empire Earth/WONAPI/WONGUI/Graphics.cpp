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
    this->mPlat = ee_won_gfx_plat_of(theContext->mSurface != NULL ? theContext->mSurface->GetRust() : NULL, theContext->mPixelScale);
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
void Graphics::DoTranslate(int &x, int &y) { x += this->mTransX; y += this->mTransY; }

