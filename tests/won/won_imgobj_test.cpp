// The image classes whole in Rust (R6 WONLobby round 24): the original statements (won_imgobj_orig.h: Image.cpp,
// Palette.cpp, DelayLoadImage.cpp, MSSkinImage.cpp, ImageFilters.cpp, GuiHost.cpp's stretch images,
// PlatNativeImage.cpp and ImageDecoder.cpp as they were) against ee_won_gui's imgobj.rs through its C ABI, both over
// one world: a logged Graphics (every call it gets, an image it draws drawn at its place), display contexts on the
// real surfaces (WONSurface.h), the window manager's decode and pixel scale, the texture pack's setting.
//
//  - 20,000 random sequences of 40 steps over a pool of images: new 32- and 8-bit raw images (random pixels with and
//    without alpha, palettes of 1..8 colours), MSImages of every type and flag, stretch images over raw images,
//    stretch-centre images, native images at pixel scales 0.75 (made 1) .. 3, delay-load images; then duplicates, native images
//    (on no context or the screen's), raw copies, sizes, draws onto a seeded screen at random places and source
//    rectangles, the crop, scale, tint and transparent filters, the stretch images' setters and copies, releases.
//    Each side's trace (the Graphics calls, the decodes) and, at the sequence's end, every image's description (its
//    kind, sizes, flags, pixels, palette, a raw copy and a draw onto a fresh surface) and the screen's pixels must be
//    the same, and every image freed (no leak on either side).
//  - the decoders over PNG files the driver writes (well formed, truncated, not a PNG, upper-case extensions, missing,
//    unregistered extensions) and the texture pack's copies (Data/Textures-4x/WONLobby Resources/ at 2x, 3x and 4x,
//    or another size) at the setting's shifts 0..2, with and without a window manager: the attributes, the raw
//    decode, the decode (or the FileReader status thrown) and every image's description.
// Exit code: the number of mismatches.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <filesystem>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONSurface.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONStringCompare.h"
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/imgdec.h"
#include "rust/won_gui/imgfilter.h"
#include "rust/won_gui/imgobj.h"
#include "rust/won_gui/mspaint.h"
#include "rust/won_gui/platimg.h"
#include "rust/won_gui/stretchobj.h"
using WONAPI::FileReader;
using WONAPI::FileReaderException;
using WONAPI::RefCount;
using WONAPI::SmartPtr;
using WONAPI::SurfaceBase;
using WONAPI::SurfaceFormat_32;
using WONAPI::StringCompareNoCase;

// ---- the world

static std::string* sT; // the side's trace
static void T(const char* f, long long a = 0, long long b = 0, long long c = 0, long long d = 0, long long e = 0, long long g = 0)
{
    char s[256];
    snprintf(s, sizeof s, "%s %lld %lld %lld %lld %lld %lld\n", f, a, b, c, d, e, g);
    *sT += s;
}
static float sWmScale = 1.0f; // PlatWindowManager::GetPixelScale
static long sShift;           // the texture pack's logical shift
static bool sHaveWm;          // a default window manager

struct DisplayContext;
// the logged Graphics: its translation and colour kept, an image drawn at its place (the real Graphics::DrawImage
// draws it entire at the translated place)
struct Graphics {
    typedef void (*DrawFn)(void*, DisplayContext*, int, int);
    explicit Graphics(DrawFn d) : mDraw(d), mDisplayContext(NULL), mTransX(0), mTransY(0), mColor(0) {}
    virtual ~Graphics() {}
    DrawFn mDraw;
    DisplayContext* mDisplayContext;
    int mTransX, mTransY;
    DWORD mColor;
    void Attach(DisplayContext* c);
    void Translate(int x, int y) { mTransX += x; mTransY += y; T("Translate", x, y); }
    void PushClipRect(int a, int b, int c, int d) { T("PushClip", a, b, c, d); }
    void PopClipRect() { T("PopClip"); }
    void SetColor(DWORD c) { mColor = c; T("SetColor", c); }
    void SetSysColor(int c) { mColor = 0x80000000u + c; T("SetSysColor", c); }
    DWORD GetStandardColor(int c) { return 0x01010101u * (DWORD)c; }
    void FillRect(int a, int b, int c, int d) { T("FillRect", a, b, c, d); }
    void DrawRect(int a, int b, int c, int d) { T("DrawRect", a, b, c, d); }
    void DrawDottedRect(int a, int b, int c, int d) { T("DottedRect", a, b, c, d); }
    void DrawLine(int a, int b, int c, int d) { T("Line", a, b, c, d); }
    void SetPixel(int a, int b, DWORD c) { T("Pixel", a, b, c); }
    void FillEllipse(int a, int b, int c, int d) { T("FillEllipse", a, b, c, d); }
    void DrawArc(int a, int b, int c, int d, int e, int f) { T("Arc", a, b, c, d, e, f); }
    void DrawImage(void* i, int x, int y)
    {
        T("DrawImage", x, y, i != NULL);
        if (i != NULL) mDraw(i, mDisplayContext, x + mTransX, y + mTransY);
    }
};
struct DisplayContext : RefCount {
    DisplayContext(SurfaceBase* s = NULL) : mSurface(s), mGraphics(NULL), mPixelScale(1.0f) {}
    SurfaceBase* mSurface;
    Graphics* mGraphics;
    float mPixelScale;
};
void Graphics::Attach(DisplayContext* c)
{
    mDisplayContext = c;
    if (c != NULL) c->mGraphics = this;
    T("Attach");
}
inline int PlatGUIPixels(int inLogical, float inScale)
{
    return inScale == 1.0f ? inLogical : (int)floorf((float)inLogical * inScale + 0.5f);
}
// a plan's operations on the Graphics (WONDrawOne's), its image slots drawn and sized by the side
typedef void (*SizeFn)(void*, int, int);
static void RunOps(Graphics& g, const EeWonDrawOp* ops, size_t n, void* const* slots, SizeFn size)
{
    for (size_t k = 0; k < n; k++) {
        const EeWonDrawOp& o = ops[k];
        switch (o.kind) {
            case EE_WON_DRAW_SET_COLOR: g.SetColor((DWORD)(unsigned int)o.a); break;
            case EE_WON_DRAW_SET_SYS_COLOR: g.SetSysColor(o.a); break;
            case EE_WON_DRAW_FILL_RECT: g.FillRect(o.a, o.b, o.c, o.d); break;
            case EE_WON_DRAW_RECT: g.DrawRect(o.a, o.b, o.c, o.d); break;
            case EE_WON_DRAW_DOTTED_RECT: g.DrawDottedRect(o.a, o.b, o.c, o.d); break;
            case EE_WON_DRAW_LINE: g.DrawLine(o.a, o.b, o.c, o.d); break;
            case EE_WON_DRAW_IMAGE: g.DrawImage(slots[o.a], o.b, o.c); break;
            case EE_WON_DRAW_IMAGE_SIZE: T("ImageSize", o.a, o.b, o.c); if (slots[o.a]) size(slots[o.a], o.b, o.c); break;
            case EE_WON_DRAW_PUSH_CLIP: g.PushClipRect(o.a, o.b, o.c, o.d); break;
            case EE_WON_DRAW_POP_CLIP: g.PopClipRect(); break;
            case EE_WON_DRAW_TRANSLATE: g.Translate(o.a, o.b); break;
            case EE_WON_DRAW_PIXEL: g.SetPixel(o.a, o.b, (DWORD)(unsigned int)o.c); break;
            case EE_WON_DRAW_SYS_PIXEL: g.SetPixel(o.a, o.b, g.GetStandardColor(o.c)); break;
            case EE_WON_DRAW_FILL_ELLIPSE: g.FillEllipse(o.a, o.b, o.c, o.d); break;
            case EE_WON_DRAW_ARC: g.DrawArc(o.a, o.b, o.c, o.d, o.e, o.f); break;
            default: T("Op", o.kind, o.a, o.b, o.c, o.d); break;
        }
    }
}

// the window manager's decode: a native image of the path's size and pixels (none for one path in four)
static unsigned Hash(const std::string& s) { unsigned h = 2166136261u; for (char c : s) { h ^= (unsigned char)c; h *= 16777619u; } return h; }
static void Fill(uint32_t* px, int pitch, int pw, int ph, unsigned seed)
{
    for (int y = 0; y < ph; y++)
        for (int x = 0; x < pw; x++) { seed = seed * 1103515245u + 12345u; px[y * pitch + x] = seed; }
}
// ---- the original classes

namespace Orig {
struct NativeImage;
static SmartPtr<NativeImage> WmDecodeImage(const char* path);
#include "won_imgobj_orig.h"
SmartPtr<NativeImage> WmDecodeImage(const char* path)
{
    T("decode", (long long)Hash(path));
    unsigned h = Hash(path);
    if (h % 4 == 0) return NULL;
    PlatNativeImage* i = new PlatNativeImage(NULL, 1 + h % 6, 1 + (h >> 4) % 6);
    Fill((uint32_t*)i->mSurface->GetPixelsBase(), i->mSurface->GetPixelsPerRow(), i->mSurface->GetWidth(), i->mSurface->GetHeight(), h);
    return i;
}
}

// ---- the port: imgobj.rs through its C ABI, the host over the same world

static int sPortLive;
struct PImg : RefCount {
    EeWonImage* r;
    explicit PImg(EeWonImage* x) : r(x) { ee_won_image_bind(r, (intptr_t)this); sPortLive++; }
    ~PImg() { ee_won_image_free(r); sPortLive--; }
    intptr_t Get(EeWonImageField f) const { return ee_won_image_get(r, f); }
    intptr_t Call(EeWonImageCall c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0, intptr_t d = 0) { return ee_won_image_call(r, c, a, b, cc, d); }
    int W() const { return (int)Get(EeWonImageField::Width); }
    int H() const { return (int)Get(EeWonImageField::Height); }
};
static SmartPtr<PImg> Adopt(intptr_t p) { SmartPtr<PImg> s; s.mPtr = (PImg*)p; return s; }
static void PortDraw(void* i, DisplayContext* c, int x, int y) { PImg* p = (PImg*)i; ee_won_image_draw(p->r, (intptr_t)c, x, y, 0, 0, p->W(), p->H()); }
static void PortSize(void* i, int w, int h) { ((PImg*)i)->Call(EeWonImageCall::SetSize, w, h); }

static intptr_t Held(RefCount* o) { if (o) o->AddRef(); return (intptr_t)o; }
static intptr_t Host(EeWonImageHost op, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
static intptr_t PortDecode(const char* path)
{
    T("decode", (long long)Hash(path));
    unsigned h = Hash(path);
    if (h % 4 == 0) return 0;
    PImg* i = new PImg(ee_won_image_new_native(Host, 1 + h % 6, 1 + (h >> 4) % 6, 1.0f));
    Fill((uint32_t*)i->Get(EeWonImageField::Pixels), (int)i->Get(EeWonImageField::PitchPixels), (int)i->Get(EeWonImageField::PixelWidth),
         (int)i->Get(EeWonImageField::PixelHeight), h);
    return Held(i);
}
static intptr_t Host(EeWonImageHost op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonImageHost H;
    DisplayContext* ctx = (DisplayContext*)a;
    Graphics* g = (Graphics*)a;
    switch (op) {
        case H::Ref: ((RefCount*)a)->AddRef(); return 0;
        case H::Unref: ((RefCount*)a)->Release(); return 0;
        case H::Wrap: return (intptr_t)new PImg((EeWonImage*)a);
        case H::RustOf: return (intptr_t)((PImg*)a)->r;
        case H::CtxGraphics: return ctx != NULL ? (intptr_t)ctx->mGraphics : 0;
        case H::CtxSurface: return ctx != NULL && ctx->mSurface != NULL ? (intptr_t)ctx->mSurface->GetRust() : 0;
        case H::CtxScale: { uint32_t s; memcpy(&s, &ctx->mPixelScale, 4); return (intptr_t)s; }
        case H::GfxTrans: ((int*)b)[0] = g->mTransX; ((int*)b)[1] = g->mTransY; return 0;
        case H::GfxContext: return (intptr_t)g->mDisplayContext;
        case H::GfxRun: RunOps(*g, (const EeWonDrawOp*)b, (size_t)c, (void* const*)d, PortSize); return 0;
        case H::GfxDrawImage: g->DrawImage((void*)b, ((int*)c)[0], ((int*)c)[1]); return 0;
        case H::NewSurface: {
            SmartPtr<SurfaceBase> s = SurfaceBase::GetNewSurface(SurfaceFormat_32);
            int w = a < 0 ? 0 : (int)a, h = b < 0 ? 0 : (int)b;
            s->Create(w, h);
            if (s->GetPixelsBase() != NULL) memset(s->GetPixelsBase(), 0, (size_t)s->GetBytesPerRow() * h);
            return Held(s.get());
        }
        case H::SurfaceRust: return (intptr_t)((SurfaceBase*)a)->GetRust();
        case H::NewContext: {
            DisplayContext* n = new DisplayContext((SurfaceBase*)a);
            uint32_t s = (uint32_t)b;
            memcpy(&n->mPixelScale, &s, 4);
            return Held(n);
        }
        case H::NewGraphics: { Graphics* n = new Graphics(PortDraw); n->Attach(ctx); return (intptr_t)n; }
        case H::FreeGraphics: delete g; return 0;
        case H::CreateImage: return Held(new PImg(ee_won_image_new_native(Host, (int)b, (int)c, sWmScale)));
        case H::Decode: return PortDecode((const char*)a);
        case H::PackShift: return sHaveWm ? sShift : 0;
    }
    return 0;
}

// ---- descriptions (the same text from both sides)

static unsigned long long Fnv(const void* p, size_t n, unsigned long long h = 1469598103934665603ull)
{
    for (size_t i = 0; i < n; i++) { h ^= ((const unsigned char*)p)[i]; h *= 1099511628211ull; }
    return h;
}
static std::string Hex(unsigned long long v) { char s[32]; snprintf(s, sizeof s, "%016llx", v); return s; }
static unsigned long long SurfaceHash(SurfaceBase* s)
{
    if (s == NULL || s->GetPixelsBase() == NULL) return 0;
    return Fnv(s->GetPixelsBase(), (size_t)s->GetBytesPerRow() * s->GetHeight());
}
// a fresh 20 x 20 surface (seeded) at scale 1 to draw on, its hash after the draw
struct Probe {
    SmartPtr<SurfaceBase> s;
    SmartPtr<DisplayContext> c;
    Probe(unsigned seed)
    {
        s = SurfaceBase::GetNewSurface(SurfaceFormat_32);
        s->Create(20, 20);
        Fill((uint32_t*)s->GetPixelsBase(), s->GetPixelsPerRow(), 20, 20, seed);
        c = new DisplayContext(s.get());
    }
};

static std::string DescOrig(Orig::Image* i)
{
    using namespace Orig;
    if (i == NULL) return "null\n";
    char b[512];
    std::string o;
    snprintf(b, sizeof b, "size %d %d", i->mWidth, i->mHeight);
    o += b;
    if (RawImage* r = dynamic_cast<RawImage*>(i)) {
        snprintf(b, sizeof b, " raw %d trans %d", r->mType, r->HasTransparency() ? 1 : 0);
        o += b;
        if (r->mType == 2) {
            RawImage32* q = static_cast<RawImage32*>(r);
            o += " px " + Hex(Fnv(q->mImageData, (size_t)q->mWidth * q->mHeight * 4));
        } else if (r->mType == 1) {
            RawImage8* q = static_cast<RawImage8*>(r);
            snprintf(b, sizeof b, " bpr %d ti %d pal %d", q->mBytesPerRow, q->mTransparentColor, q->mPalette.get() ? q->mPalette->mNumColors : -1);
            o += b;
            o += " px " + Hex(Fnv(q->mImageData, (size_t)q->mBytesPerRow * q->mHeight));
            if (q->mPalette.get()) o += " colors " + Hex(Fnv(q->mPalette->mColors, (size_t)q->mPalette->mNumColors * 4));
        }
    } else if (PlatNativeImage* n = dynamic_cast<PlatNativeImage*>(i)) {
        snprintf(b, sizeof b, " native %d %d %g mask %d", n->mSurface->GetWidth(), n->mSurface->GetHeight(), n->mScale,
                 n->mTransparentMask.get() != NULL);
        o += b;
        o += " px " + Hex(SurfaceHash(n->mSurface.get()));
        SmartPtr<RawImage32> raw = n->GetRaw();
        o += " raw " + Hex(Fnv(raw->mImageData, (size_t)raw->mWidth * raw->mHeight * 4)) + (raw->mDoTransparency ? " t" : " o");
        Probe p(n->mWidth * 31 + n->mHeight);
        n->Draw(p.c.get(), 2, 3, 0, 0, n->mWidth, n->mHeight);
        o += " drawn " + Hex(SurfaceHash(p.s.get()));
    } else if (StretchImage* s = dynamic_cast<StretchImage*>(i)) {
        snprintf(b, sizeof b, " stretch %d %d %d %d %d %d %d %d", s->GetLeft(), s->GetTop(), s->GetRight(), s->GetBottom(),
                 s->Get(EeWonStretchField::TileHorz), s->Get(EeWonStretchField::TileVert), s->Get(EeWonStretchField::SkipCenter),
                 s->Get(EeWonStretchField::NeedRecalc));
        o += b;
    }
    return o + "\n";
}
static std::string DescPort(PImg* i)
{
    typedef EeWonImageField F;
    if (i == NULL) return "null\n";
    char b[512];
    std::string o;
    snprintf(b, sizeof b, "size %d %d", i->W(), i->H());
    o += b;
    EeWonImageKind k = ee_won_image_kind(i->r);
    if (k == EeWonImageKind::Raw32 || k == EeWonImageKind::Raw8 || k == EeWonImageKind::Raw1) {
        snprintf(b, sizeof b, " raw %d trans %d", (int)i->Get(F::RawType), (int)i->Get(F::HasTransparency));
        o += b;
        if (k == EeWonImageKind::Raw32) {
            o += " px " + Hex(Fnv((void*)i->Get(F::Pixels), (size_t)i->W() * i->H() * 4));
        } else if (k == EeWonImageKind::Raw8) {
            int n = (int)i->Get(F::PaletteCount);
            snprintf(b, sizeof b, " bpr %d ti %d pal %d", (int)i->Get(F::BytesPerRow), (int)i->Get(F::TransparentIndex), n);
            o += b;
            o += " px " + Hex(Fnv((void*)i->Get(F::Pixels), (size_t)i->Get(F::BytesPerRow) * i->H()));
            if (n >= 0) {
                std::vector<uint32_t> c(n + 1);
                ee_won_image_palette(i->r, c.data(), (size_t)n);
                o += " colors " + Hex(Fnv(c.data(), (size_t)n * 4));
            }
        }
    } else if (k == EeWonImageKind::Native) {
        float sc;
        uint32_t bits = (uint32_t)i->Get(F::Scale);
        memcpy(&sc, &bits, 4);
        SmartPtr<PImg> raw = Adopt(i->Call(EeWonImageCall::Raw));
        // a mask makes the raw copy transparent
        snprintf(b, sizeof b, " native %d %d %g mask %d", (int)i->Get(F::PixelWidth), (int)i->Get(F::PixelHeight), sc,
                 (int)raw->Get(F::HasTransparency));
        o += b;
        o += " px " + Hex(SurfaceHash((SurfaceBase*)i->Get(F::Surface)));
        o += " raw " + Hex(Fnv((void*)raw->Get(F::Pixels), (size_t)raw->W() * raw->H() * 4)) + (raw->Get(F::HasTransparency) ? " t" : " o");
        Probe p(i->W() * 31 + i->H());
        ee_won_image_draw(i->r, (intptr_t)p.c.get(), 2, 3, 0, 0, i->W(), i->H());
        o += " drawn " + Hex(SurfaceHash(p.s.get()));
    } else if (k == EeWonImageKind::Stretch) {
        snprintf(b, sizeof b, " stretch %d %d %d %d %d %d %d %d", (int)i->Get(F::Left), (int)i->Get(F::Top), (int)i->Get(F::Right),
                 (int)i->Get(F::Bottom), (int)i->Get(F::TileHorz), (int)i->Get(F::TileVert), (int)i->Get(F::SkipCenter), (int)i->Get(F::NeedRecalc));
        o += b;
    }
    return o + "\n";
}

// ---- the sequences

static unsigned sRng;
static unsigned R(unsigned n) { sRng = sRng * 1664525u + 1013904223u; return n ? (sRng >> 8) % n : 0; }
static int sMismatches;
static void Compare(const std::string& what, const std::string& a, const std::string& b)
{
    if (a == b) return;
    sMismatches++;
    if (sMismatches > 6) return;
    size_t i = 0;
    while (i < a.size() && i < b.size() && a[i] == b[i]) i++;
    size_t from = a.rfind('\n', i == 0 ? 0 : i - 1);
    from = from == std::string::npos ? 0 : from + 1;
    printf("MISMATCH %s at %zu\n  orig: %s\n  port: %s\n", what.c_str(), i, a.substr(from, 400).c_str(), b.substr(from, 400).c_str());
}

// what one step makes: the same random draws on both sides (the generator runs once per side from one seed)
struct Side {
    virtual ~Side() {}
    virtual int Count() = 0;
    virtual int Kind(int i) = 0; // 0 raw32, 1 raw8, 2 ms, 3 stretch, 4 stretch-centre, 5 native, 6 delay, 7 raw1
    virtual void Raw32(int w, int h, bool alpha, bool trans) = 0;
    virtual void Raw8(int w, int h, int colors) = 0;
    virtual void Ms(int t, int f, int w, int h) = 0;
    virtual void Stretch(int raw, int l, int t, int r, int b) = 0;
    virtual void StretchCenter(int s, int c) = 0;
    virtual void Native(int w, int h, float scale) = 0;
    virtual void Delay(const std::string& path, int w, int h) = 0;
    virtual void Duplicate(int i) = 0;
    virtual void GetNative(int i, bool screen) = 0;
    virtual void GetRaw(int i) = 0;
    virtual void SetSize(int i, int w, int h) = 0;
    virtual void Draw(int i, int x, int y, int l, int t, int w, int h) = 0;
    virtual void Filter(int i, int which, int a, int b, int c, int d) = 0;
    virtual void StretchSet(int i, int which, int a, int b, int c, int d) = 0;
    virtual void Release(int i) = 0;
    virtual std::string Describe() = 0;
    virtual int Width(int i) = 0;
    virtual int Height(int i) = 0;
    virtual unsigned Pixel(int i, int x, int y) = 0; // a raw image's pixel (for the transparent colour)
    virtual bool HasPalette(int i) = 0;              // a raw 8-bit image's palette (the tint and transparent filters need one)
};

static int KindOrig(Orig::Image* i)
{
    using namespace Orig;
    if (dynamic_cast<RawImage32*>(i)) return 0;
    if (dynamic_cast<RawImage8*>(i)) return 1;
    if (dynamic_cast<RawImage1*>(i)) return 7;
    if (dynamic_cast<MSImage*>(i)) return 2;
    if (dynamic_cast<StretchImage*>(i)) return 3;
    if (dynamic_cast<StretchCenterImage*>(i)) return 4;
    if (dynamic_cast<PlatNativeImage*>(i)) return 5;
    return 6;
}
static int KindPort(PImg* i)
{
    switch (ee_won_image_kind(i->r)) {
        case EeWonImageKind::Raw32: return 0;
        case EeWonImageKind::Raw8: return 1;
        case EeWonImageKind::Raw1: return 7;
        case EeWonImageKind::Ms: return 2;
        case EeWonImageKind::Stretch: return 3;
        case EeWonImageKind::StretchCenter: return 4;
        case EeWonImageKind::Native: return 5;
        default: return 6;
    }
}

// the screen both sides draw on: a 40 x 40 surface (seeded) at a scale with a logged Graphics
struct Screen {
    SmartPtr<SurfaceBase> s;
    SmartPtr<DisplayContext> c;
    Graphics* g;
    Screen(Graphics::DrawFn d, float scale, unsigned seed)
    {
        s = SurfaceBase::GetNewSurface(SurfaceFormat_32);
        s->Create(40, 40);
        Fill((uint32_t*)s->GetPixelsBase(), s->GetPixelsPerRow(), 40, 40, seed);
        c = new DisplayContext(s.get());
        c->mPixelScale = scale;
        g = new Graphics(d);
        g->Attach(c.get());
    }
    ~Screen() { delete g; }
};

struct OrigSide : Side {
    std::vector<SmartPtr<Orig::Image> > v;
    Screen* scr;
    Orig::Image* At(int i) { return v[i].get(); }
    void Push(Orig::Image* i) { if (i != NULL) v.push_back(i); else T("none"); }
    int Count() { return (int)v.size(); }
    int Kind(int i) { return KindOrig(At(i)); }
    int Width(int i) { return At(i)->mWidth; }
    int Height(int i) { return At(i)->mHeight; }
    bool HasPalette(int i) { Orig::RawImage8* r = dynamic_cast<Orig::RawImage8*>(At(i)); return r == NULL || r->mPalette.get() != NULL; }
    unsigned Pixel(int i, int x, int y)
    {
        if (Orig::RawImage32* r = dynamic_cast<Orig::RawImage32*>(At(i))) return r->GetPixel(x, y);
        if (Orig::RawImage8* r = dynamic_cast<Orig::RawImage8*>(At(i))) return r->mPalette.get() ? r->mPalette->mColors[r->GetPixel(x, y) % r->mPalette->mNumColors] : 0;
        return 0;
    }
    void Raw32(int w, int h, bool alpha, bool trans)
    {
        Orig::RawImage32* r = new Orig::RawImage32(w, h);
        for (int k = 0; k < w * h; k++) { unsigned p = R(0x7fffffff) * 2u + R(2); r->mImageData[k] = alpha ? p : (p | 0xff000000u); }
        r->mDoTransparency = trans;
        Push(r);
    }
    void Raw8(int w, int h, int colors)
    {
        Orig::Palette* p = colors > 0 ? new Orig::Palette(colors) : NULL;
        for (int k = 0; k < colors; k++) p->mColors[k] = R(0x7fffffff) * 2u + R(2);
        Orig::RawImage8* r = new Orig::RawImage8(w, h, p);
        for (int k = 0; k < r->mBytesPerRow * h; k++) r->mImageData[k] = (unsigned char)R(colors + 2);
        Push(r);
    }
    void Ms(int t, int f, int w, int h) { Push(new Orig::MSImage((unsigned char)t, (unsigned char)f, w, h)); }
    void Stretch(int raw, int l, int t, int r, int b) { Push(new Orig::StretchImage(static_cast<Orig::RawImage*>(At(raw)), l, t, r, b)); }
    void StretchCenter(int s, int c) { Push(new Orig::StretchCenterImage(At(s), At(c))); }
    void Native(int w, int h, float scale)
    {
        Orig::PlatNativeImage* n = new Orig::PlatNativeImage(NULL, w, h, scale);
        Fill((uint32_t*)n->mSurface->GetPixelsBase(), n->mSurface->GetPixelsPerRow(), n->mSurface->GetWidth(), n->mSurface->GetHeight(), R(1u << 30));
        Push(n);
    }
    void Delay(const std::string& path, int w, int h) { Push(new Orig::DelayLoadImage(path, w, h)); }
    void Duplicate(int i) { Push(At(i)->Duplicate().get()); }
    void GetNative(int i, bool screen) { Push(At(i)->GetNative(screen ? scr->c.get() : NULL).get()); }
    void GetRaw(int i) { Push(static_cast<Orig::NativeImage*>(At(i))->GetRaw().get()); }
    void SetSize(int i, int w, int h) { At(i)->SetSize(w, h); }
    void Draw(int i, int x, int y, int l, int t, int w, int h) { At(i)->Draw(scr->c.get(), x, y, l, t, w, h); }
    void Filter(int i, int which, int a, int b, int c, int d)
    {
        using namespace Orig;
        RawImage* r = static_cast<RawImage*>(At(i));
        SmartPtr<ImageFilter> f;
        switch (which) {
            case 0: f = new CropImageFilter(a, b, c, d); break;
            case 1: { ScaleImageFilter* s = new ScaleImageFilter(0); s->SetPercentSize(a, b); f = s; break; }
            case 2: f = new ScaleImageFilter(a, b); break;
            case 3: f = new TintImageFilter(a, b, c); break;
            default: f = new TransparentImageFilter((DWORD)(unsigned)a); break;
        }
        Push(f->Filter(r).get());
    }
    void StretchSet(int i, int which, int a, int b, int c, int d)
    {
        Orig::StretchImage* s = static_cast<Orig::StretchImage*>(At(i));
        switch (which) {
            case 0: s->SetBorders(a, b, c, d); break;
            case 1: s->SetTile(a != 0, b != 0); break;
            case 2: s->SetSkipCenter(a != 0); break;
            case 3: s->SetImage(static_cast<Orig::RawImage*>(At(a))); break;
            default: s->CopyAttributes(static_cast<Orig::StretchImage*>(At(a))); break;
        }
    }
    void Release(int i) { v.erase(v.begin() + i); }
    std::string Describe()
    {
        std::string o;
        for (size_t i = 0; i < v.size(); i++) o += DescOrig(v[i].get());
        return o + "screen " + Hex(SurfaceHash(scr->s.get())) + "\n";
    }
};

struct PortSide : Side {
    std::vector<SmartPtr<PImg> > v;
    Screen* scr;
    PImg* At(int i) { return v[i].get(); }
    void Push(intptr_t p) { SmartPtr<PImg> s = Adopt(p); if (p != 0) v.push_back(s); else T("none"); }
    void PushNew(EeWonImage* r) { v.push_back(new PImg(r)); }
    int Count() { return (int)v.size(); }
    int Kind(int i) { return KindPort(At(i)); }
    int Width(int i) { return At(i)->W(); }
    int Height(int i) { return At(i)->H(); }
    bool HasPalette(int i) { return KindPort(At(i)) != 1 || At(i)->Get(EeWonImageField::PaletteCount) >= 0; }
    unsigned Pixel(int i, int x, int y)
    {
        PImg* p = At(i);
        if (KindPort(p) == 0) return ((uint32_t*)p->Get(EeWonImageField::Pixels))[y * p->W() + x];
        if (KindPort(p) == 1) {
            int n = (int)p->Get(EeWonImageField::PaletteCount);
            if (n <= 0) return 0;
            std::vector<uint32_t> c(n);
            ee_won_image_palette(p->r, c.data(), (size_t)n);
            return c[((uint8_t*)p->Get(EeWonImageField::Pixels))[y * p->Get(EeWonImageField::BytesPerRow) + x] % n];
        }
        return 0;
    }
    void Raw32(int w, int h, bool alpha, bool trans)
    {
        PImg* r = new PImg(ee_won_image_new_raw32(Host, w, h));
        uint32_t* px = (uint32_t*)r->Get(EeWonImageField::Pixels);
        for (int k = 0; k < w * h; k++) { unsigned p = R(0x7fffffff) * 2u + R(2); px[k] = alpha ? p : (p | 0xff000000u); }
        r->Call(EeWonImageCall::SetTransparency, trans);
        v.push_back(r);
    }
    void Raw8(int w, int h, int colors)
    {
        PImg* r = new PImg(ee_won_image_new_raw8(Host, w, h));
        std::vector<uint32_t> c(colors + 1);
        for (int k = 0; k < colors; k++) c[k] = R(0x7fffffff) * 2u + R(2);
        if (colors > 0) ee_won_image_set_palette(r->r, c.data(), (size_t)colors);
        uint8_t* px = (uint8_t*)r->Get(EeWonImageField::Pixels);
        for (int k = 0; k < r->Get(EeWonImageField::BytesPerRow) * h; k++) px[k] = (unsigned char)R(colors + 2);
        v.push_back(r);
    }
    void Ms(int t, int f, int w, int h) { PushNew(ee_won_image_new_ms(Host, (uint8_t)t, (uint8_t)f, w, h)); }
    void Stretch(int raw, int l, int t, int r, int b) { PushNew(ee_won_image_new_stretch(Host, (intptr_t)At(raw), l, t, r, b)); }
    void StretchCenter(int s, int c) { PushNew(ee_won_image_new_stretch_center(Host, (intptr_t)At(s), (intptr_t)At(c))); }
    void Native(int w, int h, float scale)
    {
        PImg* n = new PImg(ee_won_image_new_native(Host, w, h, scale));
        Fill((uint32_t*)n->Get(EeWonImageField::Pixels), (int)n->Get(EeWonImageField::PitchPixels), (int)n->Get(EeWonImageField::PixelWidth),
             (int)n->Get(EeWonImageField::PixelHeight), R(1u << 30));
        v.push_back(n);
    }
    void Delay(const std::string& path, int w, int h) { PushNew(ee_won_image_new_delay(Host, (const uint8_t*)path.data(), path.size(), w, h)); }
    void Duplicate(int i) { Push(At(i)->Call(EeWonImageCall::Duplicate)); }
    void GetNative(int i, bool screen) { Push(At(i)->Call(EeWonImageCall::Native, screen ? (intptr_t)scr->c.get() : 0)); }
    void GetRaw(int i) { Push(At(i)->Call(EeWonImageCall::Raw)); }
    void SetSize(int i, int w, int h) { At(i)->Call(EeWonImageCall::SetSize, w, h); }
    void Draw(int i, int x, int y, int l, int t, int w, int h) { ee_won_image_draw(At(i)->r, (intptr_t)scr->c.get(), x, y, l, t, w, h); }
    void Filter(int i, int which, int a, int b, int c, int d)
    {
        static const EeWonImageCall kCall[5] = {EeWonImageCall::Crop, EeWonImageCall::ScalePercent, EeWonImageCall::ScaleTo,
                                                EeWonImageCall::Tint, EeWonImageCall::Transparent};
        Push(At(i)->Call(kCall[which], which == 4 ? (intptr_t)(unsigned)a : a, b, c, d));
    }
    void StretchSet(int i, int which, int a, int b, int c, int d)
    {
        static const EeWonImageCall kCall[5] = {EeWonImageCall::StretchSetBorders, EeWonImageCall::StretchSetTile,
                                                EeWonImageCall::StretchSetSkip, EeWonImageCall::StretchSetImage, EeWonImageCall::StretchCopy};
        if (which >= 3) At(i)->Call(kCall[which], (intptr_t)At(a));
        else At(i)->Call(kCall[which], a, b, c, d);
    }
    void Release(int i) { v.erase(v.begin() + i); }
    std::string Describe()
    {
        std::string o;
        for (size_t i = 0; i < v.size(); i++) o += DescPort(v[i].get());
        return o + "screen " + Hex(SurfaceHash(scr->s.get())) + "\n";
    }
};

// one random step (the side answers the generator's questions the same way when it is right)
static void Step(Side& s)
{
    static const float kScales[6] = {1.0f, 1.25f, 1.5f, 2.0f, 3.0f, 0.75f}; // (under 1: made 1)
    static const char* kPaths[5] = {"a.png", "b.png", "Pic\\c.jpg", "d", "WONLobby Resources/e.png"};
    int n = s.Count();
    unsigned what = n == 0 ? R(3) : R(26);
    T("step", what, n);
    if (what < 2) s.Raw32(1 + R(9), 1 + R(9), R(2) != 0, R(2) != 0);
    else if (what < 3) s.Raw8(1 + R(9), 1 + R(9), R(5) == 0 ? 0 : 1 + R(8));
    else if (what < 4) s.Ms(R(19), R(16), R(31), R(31));
    else if (what < 5) s.Native(R(9), R(9), kScales[R(6)]);
    else if (what < 6) s.Delay(kPaths[R(5)], R(9), R(9));
    else {
        int i = R(n), k = s.Kind(i);
        int w = s.Width(i), h = s.Height(i);
        T("on", i, k, w, h);
        switch (what) {
            case 6:
                // (borders inside the source: the stretch filter's plan reads out of a smaller one)
                if ((k == 0 || k == 1) && w >= 3 && h >= 3) s.Stretch(i, R(2), R(2), R(2), R(2));
                break;
            case 7: {
                int c = R(n);
                if (k != 6 && k != 7 && s.Kind(c) != 6 && s.Kind(c) != 7 && k != 4) s.StretchCenter(i, c);
                break;
            }
            case 8: case 9: s.Duplicate(i); break;
            case 10: case 11: s.GetNative(i, R(3) != 0); break;
            case 12: if (k == 5) s.GetRaw(i); break;
            case 13: s.SetSize(i, R(30), R(30)); break;
            case 14: case 15: case 16:
                if (k != 7) s.Draw(i, (int)R(40) - 8, (int)R(40) - 8, R(4) == 0 ? (int)R(w + 1) : 0, R(4) == 0 ? (int)R(h + 1) : 0,
                                   R(4) == 0 ? (int)R(w + 2) : w, R(4) == 0 ? (int)R(h + 2) : h);
                break;
            case 17: case 18: case 19: case 20:
                if ((k == 0 || k == 1) && w > 0 && h > 0) {
                    unsigned f = R(5);
                    if (f == 0) { int x = R(w), y = R(h); s.Filter(i, 0, x, y, 1 + R(w - x), 1 + R(h - y)); }
                    else if (f == 1) s.Filter(i, 1, 1 + R(300), 1 + R(300), 0, 0);
                    else if (f == 2) s.Filter(i, 2, 1 + R(20), 1 + R(20), 0, 0);
                    else if (!s.HasPalette(i)) break;
                    else if (f == 3) s.Filter(i, 3, R(200), R(200), R(200), 0);
                    else { int x = R(w), y = R(h); s.Filter(i, 4, (int)(R(2) ? s.Pixel(i, x, y) : R(0x7fffffff) * 2u), 0, 0, 0); }
                }
                break;
            case 21: case 22:
                if (k == 3) {
                    unsigned f = R(5);
                    if (f == 0) s.StretchSet(i, 0, R(2), R(2), R(2), R(2));
                    else if (f == 1) s.StretchSet(i, 1, R(2), R(2), 0, 0);
                    else if (f == 2) s.StretchSet(i, 2, R(2), 0, 0, 0);
                    else { int j = R(n); int kj = s.Kind(j); if ((f == 3 && (kj == 0 || kj == 1) && s.Width(j) >= 3 && s.Height(j) >= 3) || (f == 4 && kj == 3)) s.StretchSet(i, f, j, 0, 0, 0); }
                }
                break;
            default: s.Release(i); break;
        }
    }
}

static void Sequences(int count)
{
    for (int seq = 0; seq < count; seq++) {
        std::string ta, tb;
        float scale = R(2) ? 1.0f : 2.0f;
        unsigned seed = sRng;
        sWmScale = (seq % 3) == 0 ? 2.0f : 1.0f;
        {
            sT = &ta;
            OrigSide o;
            Screen scr(Orig::DrawOf, scale, seq);
            o.scr = &scr;
            sRng = seed;
            for (int k = 0; k < 40; k++) Step(o);
            ta += o.Describe();
        }
        unsigned after = sRng;
        {
            sT = &tb;
            PortSide p;
            Screen scr(PortDraw, scale, seq);
            p.scr = &scr;
            sRng = seed;
            for (int k = 0; k < 40; k++) Step(p);
            tb += p.Describe();
        }
        if (sRng != after) { printf("MISMATCH seq %d: the generator's draws differ\n", seq); sMismatches++; }
        char what[64];
        snprintf(what, sizeof what, "sequence %d", seq);
        if (getenv("EE_IMGOBJ_DUMP") != NULL && atoi(getenv("EE_IMGOBJ_DUMP")) == seq) {
            FILE* f = fopen("orig.txt", "wb"); fwrite(ta.data(), 1, ta.size(), f); fclose(f);
            f = fopen("port.txt", "wb"); fwrite(tb.data(), 1, tb.size(), f); fclose(f);
        }
        Compare(what, ta, tb);
        if (Orig::sLive != 0 || sPortLive != 0) {
            printf("MISMATCH sequence %d: images alive orig %d port %d\n", seq, Orig::sLive, sPortLive);
            sMismatches++;
            Orig::sLive = sPortLive = 0;
        }
    }
}

// ---- the decoders

static unsigned Crc32(const unsigned char* p, size_t n)
{
    unsigned c = 0xffffffffu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
    }
    return ~c;
}
static void Be32(std::vector<unsigned char>& o, unsigned v) { o.push_back(v >> 24); o.push_back(v >> 16); o.push_back(v >> 8); o.push_back(v); }
static void Chunk(std::vector<unsigned char>& o, const char* kind, const std::vector<unsigned char>& d)
{
    Be32(o, (unsigned)d.size());
    std::vector<unsigned char> c(kind, kind + 4);
    c.insert(c.end(), d.begin(), d.end());
    o.insert(o.end(), c.begin(), c.end());
    Be32(o, Crc32(c.data(), c.size()));
}
// a w x h RGBA (or RGB) PNG in one stored zlib block, its pixels from a seed
static std::vector<unsigned char> Png(int w, int h, bool alpha, unsigned seed)
{
    std::vector<unsigned char> png = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a}, ihdr, raw;
    Be32(ihdr, w); Be32(ihdr, h);
    ihdr.push_back(8); ihdr.push_back(alpha ? 6 : 2); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    Chunk(png, "IHDR", ihdr);
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        for (int x = 0; x < w * (alpha ? 4 : 3); x++) { seed = seed * 1103515245u + 12345u; raw.push_back((unsigned char)(seed >> 16)); }
    }
    std::vector<unsigned char> z = {0x78, 0x01, 1, (unsigned char)(raw.size() & 0xff), (unsigned char)(raw.size() >> 8),
                                    (unsigned char)(~raw.size() & 0xff), (unsigned char)((~raw.size() >> 8) & 0xff)};
    z.insert(z.end(), raw.begin(), raw.end());
    unsigned a = 1, b = 0;
    for (unsigned char v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    Be32(z, (b << 16) | a);
    Chunk(png, "IDAT", z);
    Chunk(png, "IEND", {});
    return png;
}
static void Write(const std::string& path, const std::vector<unsigned char>& b)
{
    std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    FILE* f = fopen(path.c_str(), "wb");
    fwrite(b.data(), 1, b.size(), f);
    fclose(f);
}

static void Decoders()
{
    const char* kFiles[] = {"img/a.png", "img/B.PNG", "img/c.jpg", "img/d.gif", "img/missing.png", "img/trunc.png",
                            "img/notpng.png", "WONLobby Resources/p.png", "WONLobby Resources/q.jpe", "WONLobby Resources/r.png",
                            "WONLobby Resources/s.png", "wonlobby resources\\T.png"};
    for (int round = 0; round < 60; round++) {
        sRng = 7000 + round;
        std::filesystem::remove_all("img");
        std::filesystem::remove_all("WONLobby Resources");
        std::filesystem::remove_all("Data");
        Write("img/a.png", Png(1 + R(7), 1 + R(7), R(2) != 0, R(1000)));
        Write("img/B.PNG", Png(1 + R(7), 1 + R(7), R(2) != 0, R(1000)));
        Write("img/c.jpg", Png(2, 2, false, 1)); // a PNG named .jpg: the JPEG decoder fails on it
        Write("img/d.gif", Png(2, 2, true, 2));
        std::vector<unsigned char> t = Png(4, 4, true, 3);
        t.resize(t.size() - 20 - R(20));
        Write("img/trunc.png", t);
        Write("img/notpng.png", std::vector<unsigned char>(5 + R(30), 'x'));
        // the pack: a 2..4x copy, or another size, or none
        int pw = 1 + R(5), ph = 1 + R(5);
        Write("WONLobby Resources/p.png", Png(pw, ph, R(2) != 0, R(1000)));
        int f = 2 + R(3);
        if (R(4) != 0) Write("Data/Textures-4x/WONLobby Resources/p.png", Png(pw * f + (R(5) == 0 ? 1 : 0), ph * f, R(2) != 0, R(1000)));
        Write("WONLobby Resources/q.jpe", Png(3, 3, false, 5));
        if (R(2)) Write("Data/Textures-4x/WONLobby Resources/q.png", Png(6, 6, true, 6));
        Write("WONLobby Resources/r.png", Png(2, 3, true, 7));
        if (R(2)) Write("Data/Textures-4x/WONLobby Resources/r.png", std::vector<unsigned char>(20, 'y'));
        Write("WONLobby Resources/s.png", Png(3, 2, R(2) != 0, 8));
        Write("Data/Textures-4x/WONLobby Resources/s.png", Png(12, 8, R(2) != 0, 9));
        sShift = R(3);
        sHaveWm = R(4) != 0;
        bool jpg = R(3) != 0, png = R(5) != 0;
        std::string ta, tb;
        {
            sT = &ta;
            Orig::MultiImageDecoder d;
            if (jpg) Orig::JPGDecoder::Add(&d);
            if (png) Orig::PNGDecoder::Add(&d);
            Orig::sWmDecoder = sHaveWm ? &d : NULL;
            for (const char* p : kFiles) {
                Orig::ImageAttributes a;
                bool ok = d.GetImageAttributes(p, a);
                char b[256];
                snprintf(b, sizeof b, "%s attr %d %d %d\n", p, ok, ok ? a.mWidth : 0, ok ? a.mHeight : 0);
                ta += b;
                try { ta += "raw " + DescOrig(d.DecodeRaw(p).get()); } catch (FileReaderException& e) { ta += std::string("raw threw ") + e.what() + "\n"; }
                try { ta += "decode " + DescOrig(d.Decode(NULL, p).get()); } catch (FileReaderException& e) { ta += std::string("decode threw ") + e.what() + "\n"; }
            }
            Orig::sWmDecoder = NULL;
        }
        {
            sT = &tb;
            EeWonImgDecoder* d = ee_won_img_decoder_new(Host);
            if (jpg) { const char* e[4] = {"jpg", "jpe", "jpeg", "pjpeg"}; for (const char* x : e) ee_won_img_decoder_add(d, EeWonImgKind::Jpg, x); }
            if (png) ee_won_img_decoder_add(d, EeWonImgKind::Png, "png");
            for (const char* p : kFiles) {
                int wh[2] = {0, 0};
                int32_t thrown = 0;
                bool ok = ee_won_img_decoder_run(d, EeWonImgDecoderRun::Attributes, 0, p, wh, &thrown) != 0;
                char b[256];
                snprintf(b, sizeof b, "%s attr %d %d %d\n", p, ok, ok ? wh[0] : 0, ok ? wh[1] : 0);
                tb += b;
                SmartPtr<PImg> r = Adopt(ee_won_img_decoder_run(d, EeWonImgDecoderRun::DecodeRaw, 0, p, wh, &thrown));
                if (thrown) tb += std::string("raw threw ") + ee_won_file_status_text(thrown) + "\n";
                else tb += "raw " + DescPort(r.get());
                SmartPtr<PImg> n = Adopt(ee_won_img_decoder_run(d, EeWonImgDecoderRun::Decode, 0, p, wh, &thrown));
                if (thrown) tb += std::string("decode threw ") + ee_won_file_status_text(thrown) + "\n";
                else tb += "decode " + DescPort(n.get());
            }
            ee_won_img_decoder_free(d);
        }
        char what[64];
        snprintf(what, sizeof what, "decoders %d", round);
        if (getenv("EE_IMGOBJ_DECODERS") != NULL) printf("%s", ta.c_str());
        Compare(what, ta, tb);
        if (Orig::sLive != 0 || sPortLive != 0) {
            printf("MISMATCH decoders %d: images alive orig %d port %d\n", round, Orig::sLive, sPortLive);
            sMismatches++;
            Orig::sLive = sPortLive = 0;
        }
    }
    std::filesystem::remove_all("img");
    std::filesystem::remove_all("WONLobby Resources");
    std::filesystem::remove_all("Data");
}

int main(int argc, char** argv)
{
    int count = argc > 1 ? atoi(argv[1]) : 20000;
    sRng = 12345;
    Sequences(count);
    Decoders();
    printf("won_imgobj_test: %d sequences, 60 decoder rounds, %d mismatches\n", count, sMismatches);
    return sMismatches;
}
