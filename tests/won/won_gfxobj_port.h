// won_gfxobj_test's port side: WONLobbyGUI.h's Graphics and Font and Graphics.cpp's bodies as they were at 71c097464
// (R6 WONGUI B round 1's shell over gfxobj.rs: each hook a virtual the driver overrides to trace it). R6 WON GUI
// round 35 folded PlatGraphics into the game's Graphics and PlatGUIFont into its Font (no virtuals; gfxplat.rs
// answers the drawing hooks), so the driver keeps this copy to test gfxobj.rs through. Generated once by
// build/w35/mk_gfxport.py from the two files; included by won_gfxobj_test.cpp after WONLobbyGUI.h with the game's
// two classes renamed (RealGraphics, RealFont).
namespace WONAPI {
class Font : public RefCount {
public:
    Font(DisplayContext*, const FontDescriptor&); //3268a0
    const FontDescriptor& GetDescriptor() { return this->mDescriptor; }
    int GetSize() { return this->mDescriptor.mSize; }
    int GetStyle() { return this->mDescriptor.mStyle; }
    virtual ~Font();                        // 0x326a60, out of line
    virtual void PrepareContext(DisplayContext*);
    virtual void DrawString(DisplayContext*, const GUIString&, unsigned short, unsigned short, int, int);
    int GetStringWidth(const GUIString& theStr) { return this->GetStringWidth(theStr,0,(unsigned short)theStr.length()); }
    virtual int GetStringWidth(const GUIString&, unsigned short, unsigned short);
    virtual int GetCharacterWidth(int inChar);
    virtual int GetHeight();
    virtual int GetAscent();
    virtual int GetDescent();
    FontDescriptor mDescriptor;             // +08..1f
};
class Graphics {
public:
    virtual void ClearClipRectHook() {}
    virtual void SetClipRectHook(int, int, int, int) {}
    virtual DWORD GetPixelHook(int, int) { return 0; }
    virtual void SetPixelHook(int, int, DWORD) {}
    virtual void SetColorHook() {}
    virtual void DrawLineHook(int, int, int, int) {}
    virtual void DrawRectHook(int, int, int, int);
    virtual void DrawDottedRectHook(int, int, int, int);
    virtual void FillRectHook(int, int, int, int);
    virtual void DrawPolygonHook(POINT*, int) {}
    virtual void FillPolygonHook(POINT*, int) {}
    virtual void DrawEllipseHook(int, int, int, int) {}
    virtual void FillEllipseHook(int, int, int, int) {}
    virtual void DrawArcHook(int, int, int, int, int, int) {}
    virtual void FillRectBlendHook(int, int, int, int, int) {}
    Graphics(); //326ad0
    virtual ~Graphics(); //326b90
    virtual void Attach(DisplayContext*); //326c70
    virtual void Detach();
    virtual bool BlendSupported() { return false; }
    void SetColor(DWORD); //326ce0
    DWORD GetStandardColor(int);
    void SetSysColor(int);
    void SetFont(Font*);
    void Translate(int, int);
    void DoTranslate(int&, int&);
    void SetPixel(int, int, DWORD);
    void DrawImage(Image*, int, int);
    void DrawImage(Image*, int, int, int, int, int, int);
    void DrawLine(int, int, int, int);
    void DrawRect(int, int, int, int);
    void FillRect(int, int, int, int);
    void FillRectBlend(int, int, int, int, int);
    void DrawDottedRect(int, int, int, int);
    void DrawEllipse(int, int, int, int);
    void FillEllipse(int, int, int, int);
    void DrawArc(int, int, int, int, int, int);
    void DrawString(const GUIString&, int, int);
    void DrawString(const GUIString&, unsigned short, unsigned short, int, int);
    void PerformClip();
    void PushClipRect(int, int, int, int);
    void PopClipRect();
    SmartPtr<DisplayContext> mDisplayContext; //04
    // R6 WONGUI B round 1: the methods are ee_won's gfxobj.rs. The translation, colour and clip rectangle are the block
    // of members it edits in place (EeWonGfxState: the original's +08, +0c, +14, +24; checked in Graphics.cpp), the
    // clip stack (+18) its object
    int mTransX, mTransY;
    DWORD mColor;
    WONRectangle mClipRect;
    SmartPtr<Font> mFont; //10
    ColorScheme* mColorScheme; //34, borrowed
    EeWonGfx* mGfx; // the clip stack, owned
private:
    DWORD GCall(EeWonGfxCall c, int a = 0, int b = 0, int cc = 0, int d = 0, int e = 0, int f = 0, int g = 0, const void* p = NULL);
};
}
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
// TEST ONLY: won_gfxobj_test's log of the image draws in place of the images' own (NULL in the game; the images are
// imgobj.rs objects since R6 WONLobby round 24, their draws no longer the driver's to define)
namespace WONAPI { void (*gWONGfxImageLog)(Image* inImage, bool inPart, const int32_t* inArgs) = NULL; }

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
    case H::ImageEntire:
        if (gWONGfxImageLog != NULL) gWONGfxImageLog((Image*)p, false, a);
        else ((Image*)p)->DrawEntire(g->mDisplayContext.get(), a[0], a[1]);
        return 0;
    case H::ImagePart:
        if (gWONGfxImageLog != NULL) gWONGfxImageLog((Image*)p, true, a);
        else ((Image*)p)->Draw(g->mDisplayContext.get(), a[0], a[1], a[2], a[3], a[4], a[5]);
        return 0;
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
    this->mDisplayContext->mGraphics = (RealGraphics*)(void*)this; // (never read by the driver)
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
