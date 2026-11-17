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
using namespace WONAPI;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static_assert(offsetof(Graphics, mTransY) == offsetof(Graphics, mTransX) + offsetof(EeWonGfxState, trans_y) &&
              offsetof(Graphics, mColor) == offsetof(Graphics, mTransX) + offsetof(EeWonGfxState, color) &&
              offsetof(Graphics, mClipRect) == offsetof(Graphics, mTransX) + offsetof(EeWonGfxState, clip) &&
              sizeof(WONRectangle) == sizeof(EeWonRect), "gfxobj.rs EeWonGfxState");
#pragma clang diagnostic pop

typedef EeWonGfxHook H;
// the Rust Graphics' calls out: the hooks (a[] their ints, p the image or text)
static uint32_t GfxHost(void* inGraphics, EeWonGfxHook inHook, const int32_t* a, const void* p)
{
    Graphics* g = (Graphics*)inGraphics;
    switch (inHook) {
    case H::ClearClipRect: g->ClearClipRectHook(); return 0;
    case H::SetClipRect: g->SetClipRectHook(a[0], a[1], a[2], a[3]); return 0;
    case H::SetPixel: g->SetPixelHook(a[0], a[1], (DWORD)a[2]); return 0;
    case H::SetColor: g->SetColorHook(); return 0;
    case H::DrawLine: g->DrawLineHook(a[0], a[1], a[2], a[3]); return 0;
    case H::DrawRect: g->DrawRectHook(a[0], a[1], a[2], a[3]); return 0;
    case H::FillRect: g->FillRectHook(a[0], a[1], a[2], a[3]); return 0;
    case H::DrawDottedRect: g->DrawDottedRectHook(a[0], a[1], a[2], a[3]); return 0;
    case H::DrawEllipse: g->DrawEllipseHook(a[0], a[1], a[2], a[3]); return 0;
    case H::FillEllipse: g->FillEllipseHook(a[0], a[1], a[2], a[3]); return 0;
    case H::DrawArc: g->DrawArcHook(a[0], a[1], a[2], a[3], a[4], a[5]); return 0;
    case H::FillRectBlend: g->FillRectBlendHook(a[0], a[1], a[2], a[3], a[4]); return 0;
    case H::ImageEntire: ((Image*)p)->DrawEntire(g->mDisplayContext.get(), a[0], a[1]); return 0;
    case H::ImagePart: ((Image*)p)->Draw(g->mDisplayContext.get(), a[0], a[1], a[2], a[3], a[4], a[5]); return 0;
    case H::String:
        g->mFont->DrawString(g->mDisplayContext.get(), *(const GUIString*)p, (unsigned short)a[0], (unsigned short)a[1], a[2], a[3]);
        return 0;
    }
    return 0;
}

DWORD Graphics::GCall(EeWonGfxCall c, int a, int b, int cc, int d, int e, int f, int g, const void* p)
{
    int32_t anArgs[7] = { a, b, cc, d, e, f, g };
    return ee_won_gfx_call(this, GfxHost, this->mGfx, (EeWonGfxState*)&this->mTransX,
                           this->mColorScheme != NULL ? (const uint32_t*)this->mColorScheme->mColors : NULL, c, anArgs, p);
}

// Creates a graphics context with no translation, no colour scheme and an empty clip stack
Graphics::Graphics()
{
    this->mTransX = this->mTransY = 0;
    this->mColorScheme = NULL;
    this->mGfx = ee_won_gfx_new();
}
// Destroys the graphics context, detaching it first
Graphics::~Graphics()
{
    this->Detach();
    ee_won_gfx_free(this->mGfx);
}
void Graphics::Detach() {}
// Attaches the graphics context to a display context and links the display context back to it
void Graphics::Attach(DisplayContext *theContext)
{
    this->Detach();
    this->mDisplayContext = theContext;
    this->mDisplayContext->mGraphics = this;
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

// the forwarders (gfxobj.rs)
typedef EeWonGfxCall C;
void Graphics::SetColor(DWORD c) { this->GCall(C::SetColor, (int)c); }
DWORD Graphics::GetStandardColor(int i) { return this->GCall(C::StandardColor, i); }
void Graphics::SetSysColor(int i) { this->GCall(C::SetSysColor, i); }
void Graphics::Translate(int dx, int dy) { this->GCall(C::Translate, dx, dy); }
void Graphics::SetPixel(int x, int y, DWORD c) { this->GCall(C::SetPixel, x, y, (int)c); }
void Graphics::DrawImage(Image *i, int x, int y) { this->GCall(C::DrawImage, x, y, 0, 0, 0, 0, 0, i); }
void Graphics::DrawImage(Image *i, int x, int y, int l, int t, int w, int h) { this->GCall(C::DrawImagePart, x, y, l, t, w, h, 0, i); }
void Graphics::DrawLine(int x1, int y1, int x2, int y2) { this->GCall(C::DrawLine, x1, y1, x2, y2); }
void Graphics::DrawRect(int x, int y, int w, int h) { this->GCall(C::DrawRect, x, y, w, h); }
void Graphics::FillRect(int x, int y, int w, int h) { this->GCall(C::FillRect, x, y, w, h); }
void Graphics::FillRectBlend(int x, int y, int w, int h, int n) { this->GCall(C::FillRectBlend, x, y, w, h, n); }
void Graphics::DrawDottedRect(int x, int y, int w, int h) { this->GCall(C::DrawDottedRect, x, y, w, h); }
void Graphics::DrawEllipse(int x, int y, int w, int h) { this->GCall(C::DrawEllipse, x, y, w, h); }
void Graphics::FillEllipse(int x, int y, int w, int h) { this->GCall(C::FillEllipse, x, y, w, h); }
void Graphics::DrawArc(int x, int y, int w, int h, int s, int a) { this->GCall(C::DrawArc, x, y, w, h, s, a); }
void Graphics::DrawString(const GUIString &s, int x, int y) { this->GCall(C::DrawString, x, y, s.length(), 0, 0, 0, 0, &s); }
void Graphics::DrawString(const GUIString &s, unsigned short o, unsigned short n, int x, int y)
{ this->GCall(C::DrawStringPart, o, n, x, y, 0, 0, 0, &s); }
void Graphics::PerformClip() { this->GCall(C::PerformClip); }
void Graphics::PushClipRect(int x, int y, int w, int h) { this->GCall(C::PushClip, x, y, w, h); }
void Graphics::PopClipRect() { this->GCall(C::PopClip); }
void Graphics::DrawRectHook(int x, int y, int w, int h) { this->GCall(C::BaseRect, x, y, w, h); }
void Graphics::DrawDottedRectHook(int x, int y, int w, int h) { this->GCall(C::BaseDottedRect, x, y, w, h); }
void Graphics::FillRectHook(int x, int y, int w, int h) { this->GCall(C::BaseFillRect, x, y, w, h); }
