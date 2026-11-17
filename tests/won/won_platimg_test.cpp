// PlatNativeImage's pixel work as Rust (R6 WON round 40): PlatNativeImage.cpp's statements copied verbatim before the
// port (won_platimg_orig.h: the images' constructor, Draw with its scaled draw and taps, GetRaw, the texture pack's
// copies, the raw images' native images; PlatWindowManager's hand cursor) against the port's statements
// (won_platimg_port.h, copied from the working tree: the forwarders to ee_won's platimg.rs), both over the same
// stand-in world (won_platimg_world.h: the classes with the game's members over the real surface, the pack's files,
// the decoder, the setting).
//
//  - 60,000 draws: a source image of 0..20 x 0..20 logical pixels at a pixel scale (1, 1.25, 1.5, 2, 3, 4), its
//    pixels from a seed, now and then a mask (bits from a seed, its size near the image's), onto a destination
//    surface of 0..64 x 0..64 pixels at a scale, with a clip, its pixels from a seed; the source rectangle and place
//    at random (inside, across and outside). The destinations' pixel bytes must be the same.
//  - 20,000 raw copies (GetRaw) of such images: the pixels and the transparency flag.
//  - 30,000 raw images' native images (32-bit, 8-bit with palettes of 0..300 colours and a transparent index, 1-bit):
//    the new image's size, scale and pixel bytes.
//  - 20,000 texture pack copies: an image of 1..12 x 1..12 logical pixels with or without a mask, a path (inside
//    WONLobby Resources or not, either separator, any case, with or without an extension), the setting's shift
//    (0..3), the pack's files (either candidate name present or not, an integer multiple 1..5 of the size or not,
//    a decoder that throws), a window manager or none. The answer (the image itself or a new one: its size, scale,
//    pixel bytes and mask) and the files opened must be the same.
//  - the hand cursor's pixels.
// `--print` writes the first 300 draws, 200 raw copies, 300 native images, 300 pack copies (inputs by seed) and the
// hand cursor's hash: rust/crates/ee_won_gui/src/golden_platimg.txt, which the Rust tests replay. Exit code: mismatches.
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
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONSurface.h"
#include "rust/ee_won.h"

static unsigned long long sSeed = 0x9E3779B97F4A7C15ull;
static unsigned Rand(unsigned n) { sSeed = sSeed * 6364136223846793005ull + 1442695040888963407ull; return n ? (unsigned)((sSeed >> 33) % n) : 0; }
static int Range(int lo, int hi) { return lo + (int)Rand((unsigned)(hi - lo + 1)); }
// buffers from a seed (the Rust tests make the same)
static uint32_t Next(uint32_t& s) { s = s * 1103515245u + 12345u; return s ^ (s >> 15); }
static void FillWords(uint32_t* p, size_t n, uint32_t seed) { for (size_t i = 0; i < n; i++) p[i] = Next(seed); }
static void FillBytes(unsigned char* p, size_t n, uint32_t seed) { for (size_t i = 0; i < n; i++) p[i] = (unsigned char)(Next(seed) >> 8); }
static uint32_t Fnv(const void* d, size_t n, uint32_t h = 2166136261u)
{
    const unsigned char* p = (const unsigned char*)d;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}
static const float kScales[] = {1.0f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f, 4.0f};

struct DrawCase {
    int sw, sh; float ss; uint32_t sseed;
    int mask; int mw, mh; uint32_t mseed;
    int dw, dh; float ds; uint32_t dseed; int clip[4];
    int a[6];
};
struct RawCase { int kind, w, h; uint32_t seed; int colors, transparent; uint32_t pseed; };
struct PackCase {
    int w, h, mask; uint32_t seed, mseed; long shift; int manager; std::string path;
    struct File { std::string name; int throws, kind, w, h; uint32_t seed; };
    std::vector<File> files;
};

#define EE_PLATIMG_CASES \
static SmartPtr<PlatNativeImage> MakeImage(DisplayContext* c, int w, int h, float s, uint32_t seed, int mask, int mw, int mh, uint32_t mseed) \
{ \
    SmartPtr<PlatNativeImage> theImage = new PlatNativeImage(c, w, h, s); \
    if (theImage->GetPixels() != NULL) \
        FillWords(theImage->GetPixels(), (size_t)theImage->GetPitchPixels() * theImage->GetPixelHeight(), seed); \
    if (mask) { \
        int bpr = ((mw + 7) / 8 + 3) / 4 * 4; \
        std::vector<unsigned char> bits((size_t)bpr * (mh > 0 ? mh : 0) + 1); \
        FillBytes(bits.data(), bits.size(), mseed); \
        theImage->mTransparentMask = new PlatNativeMask(c, mw, mh, bpr, bits.data()); \
    } \
    return theImage; \
} \
static std::string RunDraw(const DrawCase& k) \
{ \
    SmartPtr<DisplayContext> theSource = new DisplayContext(NULL); \
    SmartPtr<PlatNativeImage> theImage = MakeImage(theSource, k.sw, k.sh, k.ss, k.sseed, k.mask, k.mw, k.mh, k.mseed); \
    SmartPtr<SurfaceBase> theDest = SurfaceBase::GetNewSurface(SurfaceFormat_32); \
    if (k.dw >= 0) { theDest->Create(k.dw, k.dh); if (theDest->GetPixelsBase()) FillWords((uint32_t*)theDest->GetPixelsBase(), (size_t)theDest->GetPixelsPerRow() * k.dh, k.dseed); } \
    theDest->SetClipRect(k.clip[0], k.clip[1], k.clip[2], k.clip[3]); \
    DisplayContext theContext(theDest.get()); \
    theContext.mPixelScale = k.ds; \
    theImage->Draw(&theContext, k.a[0], k.a[1], k.a[2], k.a[3], k.a[4], k.a[5]); \
    char b[32]; snprintf(b, sizeof b, "%08x", theDest->GetPixelsBase() ? Fnv(theDest->GetPixelsBase(), (size_t)theDest->GetBytesPerRow() * theDest->GetHeight()) : 0u); \
    return b; \
} \
static std::string RunRaw(const DrawCase& k) \
{ \
    SmartPtr<DisplayContext> theSource = new DisplayContext(NULL); \
    SmartPtr<PlatNativeImage> theImage = MakeImage(theSource, k.sw, k.sh, k.ss, k.sseed, k.mask, k.mw, k.mh, k.mseed); \
    SmartPtr<RawImage32> theRaw = theImage->GetRaw(); \
    char b[64]; snprintf(b, sizeof b, "%dx%d %d %08x", theRaw->mWidth, theRaw->mHeight, (int)theRaw->mDoTransparency, Fnv(theRaw->mImageData, (size_t)theRaw->mWidth * theRaw->mHeight * 4)); \
    return b; \
} \
static std::string Describe(NativeImage* inImage) \
{ \
    PlatNativeImage* p = dynamic_cast<PlatNativeImage*>(inImage); \
    if (p == NULL) return "null"; \
    char b[128]; \
    PlatNativeMask* m = static_cast<PlatNativeMask*>(p->mTransparentMask.get()); \
    snprintf(b, sizeof b, "%dx%d @%.2f %dx%d %08x mask %s", p->mWidth, p->mHeight, p->mScale, p->GetPixelWidth(), p->GetPixelHeight(), \
             p->GetPixels() ? Fnv(p->GetPixels(), (size_t)p->mSurface->GetBytesPerRow() * p->GetPixelHeight()) : 0u, \
             m ? (std::to_string(m->mWidth) + "x" + std::to_string(m->mHeight) + ":" + std::to_string(Fnv(m->mBits.data(), m->mBits.size()))).c_str() : "-"); \
    return b; \
} \
static SmartPtr<RawImage> MakeRaw(int kind, int w, int h, uint32_t seed, int colors, int transparent, uint32_t pseed) \
{ \
    if (kind == 2) { RawImage32* r = new RawImage32(w, h); FillWords((uint32_t*)r->mImageData, (size_t)w * h, seed); return r; } \
    if (kind == 1) { \
        Palette* p = colors >= 0 ? new Palette(colors) : NULL; \
        if (p) FillWords((uint32_t*)p->mColors, (size_t)colors, pseed); \
        RawImage8* r = new RawImage8(w, h, p); r->mTransparentColor = transparent; \
        FillBytes(r->mImageData, (size_t)r->mBytesPerRow * h, seed); return r; \
    } \
    RawImage1* r = new RawImage1(w, h); FillBytes(r->mImageData, (size_t)r->mBytesPerRow * h, seed); return r; \
} \
static std::string RunNative(const RawCase& k) \
{ \
    SmartPtr<DisplayContext> theContext = new DisplayContext(NULL); \
    SmartPtr<RawImage> theRaw = MakeRaw(k.kind, k.w, k.h, k.seed, k.colors, k.transparent, k.pseed); \
    SmartPtr<NativeImage> theNative; \
    if (k.kind == 2) theNative = static_cast<RawImage32*>(theRaw.get())->CreateNativeImage(theContext); \
    else if (k.kind == 1) theNative = static_cast<RawImage8*>(theRaw.get())->CreateNativeImage(theContext); \
    else theNative = static_cast<RawImage1*>(theRaw.get())->GetNative(theContext); \
    return Describe(theNative); \
} \
static std::string RunPack(const PackCase& k) \
{ \
    sTrace.clear(); sFiles.clear(); sShift = k.shift; \
    static WindowManager theManager; WindowManager::Default() = k.manager ? &theManager : NULL; \
    for (size_t i = 0; i < k.files.size(); i++) { \
        PackFile f; f.mThrows = k.files[i].throws; f.mImage = MakeRaw(k.files[i].kind, k.files[i].w, k.files[i].h, k.files[i].seed, 16, -1, 7); \
        sFiles[k.files[i].name] = f; \
    } \
    SmartPtr<DisplayContext> theContext = new DisplayContext(NULL); \
    SmartPtr<PlatNativeImage> theImage = MakeImage(theContext, k.w, k.h, 1.0f, k.seed, k.mask, k.w, k.h, k.mseed); \
    SmartPtr<NativeImage> theResult = PlatGUIPackImage(theContext, theImage, k.path.empty() ? NULL : k.path.c_str()); \
    std::string s = theResult.get() == theImage.get() ? "same" : Describe(theResult); \
    if (theResult.get() != theImage.get() && theResult.get() != NULL && theResult->mTransparentMask.get() != theImage->mTransparentMask.get()) s += " other mask"; \
    return s + "\n" + sTrace; \
} \
static std::string RunHand() \
{ \
    std::vector<uint32_t> theHand; HandCursor(theHand); \
    char b[32]; snprintf(b, sizeof b, "%08x", Fnv(theHand.data(), theHand.size() * 4)); return b; \
}

namespace Orig {
#include "won_platimg_world.h"
#include "won_platimg_orig.h"
EE_PLATIMG_CASES
}
namespace Port {
#include "won_platimg_world.h"
#include "won_platimg_port.h"
EE_PLATIMG_CASES
}

static int sFailures;
static long sChecks;
static void Check(const char* what, int n, const std::string& a, const std::string& b)
{
    sChecks++;
    if (a != b && ++sFailures < 10) printf("MISMATCH %s %d\n--- original\n%s\n--- port\n%s\n", what, n, a.c_str(), b.c_str());
}

static DrawCase RandomDraw()
{
    DrawCase k;
    k.sw = Rand(8) ? Range(0, 20) : Range(0, 2); k.sh = Rand(8) ? Range(0, 20) : Range(0, 2);
    k.ss = kScales[Rand(7)]; k.sseed = Rand(0x7fffffff);
    k.mask = Rand(3) == 0; k.mw = (std::max)(0, k.sw + Range(-2, 2)); k.mh = (std::max)(0, k.sh + Range(-2, 2)); k.mseed = Rand(0x7fffffff);
    k.dw = Rand(20) ? Range(0, 64) : -1; k.dh = Range(0, 64); k.ds = kScales[Rand(7)]; k.dseed = Rand(0x7fffffff);
    if (Rand(3)) { k.clip[0] = Range(-5, 40); k.clip[1] = Range(-5, 40); k.clip[2] = Range(-5, 70); k.clip[3] = Range(-5, 70); }
    else { k.clip[0] = 0; k.clip[1] = 0; k.clip[2] = 1000; k.clip[3] = 1000; }
    for (int i = 0; i < 2; i++) k.a[i] = Range(-12, 30);
    for (int i = 2; i < 4; i++) k.a[i] = Range(-8, 22);
    for (int i = 4; i < 6; i++) k.a[i] = Range(-4, 26);
    return k;
}
static RawCase RandomRaw()
{
    RawCase k;
    k.kind = (int)Rand(3); k.w = Range(0, 24); k.h = Range(0, 24); k.seed = Rand(0x7fffffff);
    k.colors = Rand(10) ? Range(0, 300) : -1; k.transparent = Rand(3) ? -1 : Range(0, 260); k.pseed = Rand(0x7fffffff);
    return k;
}
static PackCase RandomPack()
{
    static const char* kPaths[] = {"Data/WONLobby Resources/Images/button.bmp", "C:\\Game\\WONLOBBY RESOURCES\\ok.png",
        "wonlobby resources/a/b.c/d", "Data/MyWONLobby Resources/x.bmp", "Data/WONLobby Resources/", "Data/WONLobby Resources",
        "WONLobby Resources\\deep\\dir\\pic.jpg", "Data/Other/x.bmp", "", "Data/WONLobby ResourcesX/y.bmp",
        "x/wonlobby resources/noext", "Data/WONLobby Resources/two.dots.gif"};
    PackCase k;
    k.w = Range(1, 12); k.h = Rand(4) ? Range(1, 12) : k.w; k.mask = Rand(2); k.seed = Rand(0x7fffffff); k.mseed = Rand(0x7fffffff);
    k.shift = Range(0, 3); k.manager = Rand(10) != 0;
    k.path = kPaths[Rand(12)];
    // the pack's candidate names (whatever the path; the world finds only exact names)
    std::string rest = k.path;
    size_t at = 0;
    for (size_t i = 0; i < rest.size(); i++) if (rest[i] == '\\') rest[i] = '/';
    std::string low = rest;
    for (size_t i = 0; i < low.size(); i++) low[i] = (char)tolower((unsigned char)low[i]);
    at = low.find("wonlobby resources/");
    std::string name = "Data/Textures-4x/WONLobby Resources/" + (at == std::string::npos ? rest : rest.substr(at + 19));
    std::string png = name;
    size_t dot = png.find_last_of('.');
    if (dot != std::string::npos && png.find('/', dot) == std::string::npos) png = png.substr(0, dot) + ".png";
    const std::string names[2] = {name, png};
    for (int i = 0; i < 2; i++) {
        if (Rand(3) == 0) continue;
        PackCase::File f;
        f.name = names[i]; f.throws = Rand(8) == 0; f.kind = Rand(5) ? 2 : 1;
        int m = Range(1, 5);
        f.w = k.w * m + (Rand(6) == 0 ? 1 : 0); f.h = k.h * m + (Rand(6) == 0 ? -1 : 0);
        f.seed = Rand(0x7fffffff);
        k.files.push_back(f);
    }
    return k;
}

int main(int argc, char** argv)
{
    bool print = argc > 1 && !strcmp(argv[1], "--print");
    int nDraw = print ? 300 : 60000, nRaw = print ? 200 : 20000, nNative = print ? 300 : 30000, nPack = print ? 300 : 20000;
    for (int n = 0; n < nDraw; n++) {
        DrawCase k = RandomDraw();
        std::string a = Orig::RunDraw(k), b = Port::RunDraw(k);
        Check("draw", n, a, b);
        if (print)
            printf("draw %d %d %.2f %u %d %d %d %u %d %d %.2f %u %d %d %d %d %d %d %d %d %d %d = %s\n", k.sw, k.sh, k.ss, k.sseed, k.mask, k.mw, k.mh,
                   k.mseed, k.dw, k.dh, k.ds, k.dseed, k.clip[0], k.clip[1], k.clip[2], k.clip[3], k.a[0], k.a[1], k.a[2], k.a[3], k.a[4], k.a[5],
                   b.c_str());
    }
    for (int n = 0; n < nRaw; n++) {
        DrawCase k = RandomDraw();
        std::string a = Orig::RunRaw(k), b = Port::RunRaw(k);
        Check("raw", n, a, b);
        if (print) printf("raw %d %d %.2f %u %d %d %d %u = %s\n", k.sw, k.sh, k.ss, k.sseed, k.mask, k.mw, k.mh, k.mseed, b.c_str());
    }
    for (int n = 0; n < nNative; n++) {
        RawCase k = RandomRaw();
        std::string a = Orig::RunNative(k), b = Port::RunNative(k);
        Check("native", n, a, b);
        if (print) printf("native %d %d %d %u %d %d %u = %s\n", k.kind, k.w, k.h, k.seed, k.colors, k.transparent, k.pseed, b.c_str());
    }
    long packed = 0;
    for (int n = 0; n < nPack; n++) {
        PackCase k = RandomPack();
        std::string a = Orig::RunPack(k), b = Port::RunPack(k);
        Check("pack", n, a, b);
        if (b.compare(0, 4, "same") != 0) packed++;
        if (print) {
            printf("pack %d %d %d %u %u %ld %d %d [%s]", k.w, k.h, k.mask, k.seed, k.mseed, k.shift, k.manager, (int)k.files.size(), k.path.c_str());
            for (size_t i = 0; i < k.files.size(); i++)
                printf(" {%s} %d %d %d %d %u", k.files[i].name.c_str(), k.files[i].throws, k.files[i].kind, k.files[i].w, k.files[i].h, k.files[i].seed);
            std::string first = b.substr(0, b.find('\n'));
            printf(" = %s\n", first.c_str());
        }
    }
    std::string ha = Orig::RunHand(), hb = Port::RunHand();
    Check("hand", 0, ha, hb);
    if (print) { printf("hand = %s\n", hb.c_str()); return 0; }
    printf("%ld checks (%d draws, %d raw copies, %d native images, %d pack copies of which %ld made a new image, the hand cursor), %d mismatches\n",
           sChecks, nDraw, nRaw, nNative, nPack, packed, sFailures);
    return sFailures;
}
