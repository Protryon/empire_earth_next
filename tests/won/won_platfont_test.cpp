// PlatGUIFont as a Rust object (R6 WON round 40): PlatGUIFont.cpp's statements copied verbatim before the port
// (won_platfont_orig.h) against the port's shell (won_platfont_port.h, copied from the working tree: the forwarders
// to ee_won's platfont.rs), both on the platform fonts (ee_plat_ui) with the test face of ee_plat_ui's goldens
// registered as "Synth" (and "Arial" an alias of it, an unknown face falling back to it).
//
// 6,000 fonts (face, size 0..30 points, every style), each with 12 random calls: the metrics, character widths
// (ASCII, Latin-1, others, surrogates), string widths and draws of part of a random string (ASCII, accented, CJK,
// a surrogate pair, an unpaired surrogate) at a random place onto a surface of a pixel scale (1, 1.25, 1.5, 2, 3) with a
// clip, or none, with a Graphics of a colour or none. The answers, the surfaces' pixel bytes and the underlines the
// Graphics was asked to draw must be the same. `--print` writes the first 400 fonts:
// rust/crates/ee_won_net/src/golden_platfont.txt, which the Rust tests replay. Exit code: the number of mismatches.
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
#include <fstream>
#include <iterator>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONSurface.h"
#include "platform/PlatFont.h"
#include "platform/PlatString.h"
#include "rust/ee_won.h"

static unsigned long long sSeed = 0xD1B54A32D192ED03ull;
static unsigned Rand(unsigned n) { sSeed = sSeed * 6364136223846793005ull + 1442695040888963407ull; return n ? (unsigned)((sSeed >> 33) % n) : 0; }
static int Range(int lo, int hi) { return lo + (int)Rand((unsigned)(hi - lo + 1)); }
static uint32_t Next(uint32_t& s) { s = s * 1103515245u + 12345u; return s ^ (s >> 15); }
static uint32_t Fnv(const void* d, size_t n)
{
    uint32_t h = 2166136261u;
    const unsigned char* p = (const unsigned char*)d;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}
static const float kScales[] = {1.0f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f};
static const char* kFaces[] = {"Synth", "Arial", "synth", "Unknown Face", ""};

struct Call { int kind; int a, b, c, d; float scale; int sw, sh; uint32_t seed; int clip[4]; int graphics; uint32_t color; std::vector<wchar_t> text; };

#define EE_PLATFONT_WORLD \
using WONAPI::SurfaceBase; using WONAPI::SmartPtr; using WONAPI::SurfaceFormat_32; \
static std::string sTrace; \
struct GUIString { struct Data { wchar_t* mText; }; Data* mData; }; \
struct Graphics { DWORD mColor; virtual ~Graphics() {} \
    virtual void DrawLineHook(int a, int b, int c, int d) { char t[64]; snprintf(t, sizeof t, "line %d %d %d %d;", a, b, c, d); sTrace += t; } }; \
struct DisplayContext { SurfaceBase* mSurface; Graphics* mGraphics; float mPixelScale; }; \
struct FontStyleNames { enum { FontStyle_Plain = 0, FontStyle_Bold = 1, FontStyle_Italic = 2, FontStyle_Underline = 4 }; }; \
enum { FontStyle_Plain = 0, FontStyle_Bold = 1, FontStyle_Italic = 2, FontStyle_Underline = 4 }; \
struct FontDescriptor { std::string mFace; int mStyle; int mSize; }; \
struct Font { Font(DisplayContext*, const FontDescriptor& d) : mDescriptor(d) {} virtual ~Font() {} FontDescriptor mDescriptor; }; \
inline int PlatGUIPixels(int inLogical, float inScale) { return inScale == 1.0f ? inLogical : (int)floorf((float)inLogical * inScale + 0.5f); } \
static std::string PlatGUIToUtf8(const wchar_t* inText, unsigned short inLength) \
{ \
    std::string theText; \
    if (inText == NULL || inLength == 0) return theText; \
    uint32_t theCount = PlatWideToUtf8(inText, inLength, NULL, 0); \
    theText.resize(theCount); \
    if (theCount != 0) PlatWideToUtf8(inText, inLength, &theText[0], theCount); \
    return theText; \
}

#define EE_PLATFONT_RUN \
static std::string Run(const FontDescriptor& fd, const std::vector<Call>& calls) \
{ \
    PlatGUIFont* f = new PlatGUIFont(NULL, fd); \
    std::string out; \
    char b[160]; \
    for (size_t i = 0; i < calls.size(); i++) { \
        const Call& c = calls[i]; \
        std::vector<wchar_t> text = c.text; text.push_back(0); \
        GUIString::Data data = {text.data()}; GUIString s = {&data}; \
        sTrace.clear(); \
        if (c.kind == 0) snprintf(b, sizeof b, "m %d %d %d", f->GetHeight(), f->GetAscent(), f->GetDescent()); \
        else if (c.kind == 1) snprintf(b, sizeof b, "c %d", f->GetCharacterWidth(c.a)); \
        else if (c.kind == 2) snprintf(b, sizeof b, "w %d", f->GetStringWidth(s, (unsigned short)c.a, (unsigned short)c.b)); \
        else { \
            SmartPtr<SurfaceBase> sf; \
            if (c.sw >= 0) { \
                sf = SurfaceBase::GetNewSurface(SurfaceFormat_32); sf->Create(c.sw, c.sh); \
                uint32_t seed = c.seed; uint32_t* p = (uint32_t*)sf->GetPixelsBase(); \
                for (int k = 0; k < sf->GetPixelsPerRow() * c.sh; k++) p[k] = Next(seed); \
                sf->SetClipRect(c.clip[0], c.clip[1], c.clip[2], c.clip[3]); \
            } \
            Graphics g; g.mColor = c.color; \
            DisplayContext ctx = {sf.get(), c.graphics ? &g : NULL, c.scale}; \
            f->DrawString(c.kind == 4 ? NULL : &ctx, s, (unsigned short)c.a, (unsigned short)c.b, c.c, c.d); \
            snprintf(b, sizeof b, "d %08x", sf.get() ? Fnv(sf->GetPixelsBase(), (size_t)sf->GetBytesPerRow() * c.sh) : 0u); \
        } \
        out += b; out += sTrace; out += "\n"; \
    } \
    delete f; \
    return out; \
}

namespace Orig {
EE_PLATFONT_WORLD
class PlatGUIFont : public Font {
public:
    PlatGUIFont(DisplayContext* inContext, const FontDescriptor& inDescriptor);
    virtual ~PlatGUIFont();
    void DrawString(DisplayContext* inContext, const GUIString& inText, unsigned short inOffset, unsigned short inLength, int x, int y);
    int GetStringWidth(const GUIString& inText, unsigned short inOffset, unsigned short inLength);
    int GetCharacterWidth(int inChar);
    int GetHeight();
    int GetAscent();
    int GetDescent();
    const PlatFont* FontForScale(float inScale);
    PlatFont* mFont;
    PlatFont* mScaledFont;
    float mScaledFontScale;
    int mHeight, mAscent, mDescent;
};
#include "won_platfont_orig.h"
EE_PLATFONT_RUN
}
namespace Port {
EE_PLATFONT_WORLD
class PlatGUIFont : public Font {
public:
    PlatGUIFont(DisplayContext* inContext, const FontDescriptor& inDescriptor);
    virtual ~PlatGUIFont();
    void PrepareContext(DisplayContext*);
    void DrawString(DisplayContext* inContext, const GUIString& inText, unsigned short inOffset, unsigned short inLength, int x, int y);
    int GetStringWidth(const GUIString& inText, unsigned short inOffset, unsigned short inLength);
    int GetCharacterWidth(int inChar);
    int GetHeight();
    int GetAscent();
    int GetDescent();
    EeWonPlatFont* mRust;
};
#include "won_platfont_port.h"
EE_PLATFONT_RUN
}

static wchar_t RandomChar()
{
    static const wchar_t kOthers[] = {0x20AC, 0x4E2D, 0x0416, 0xFFFD, 0x2603, 0x00A0};
    switch (Rand(10)) {
    case 0: return (wchar_t)Range(0xA0, 0xFF);
    case 1: return kOthers[Rand(6)];
    case 2: return (wchar_t)Range(0xD800, 0xDFFF);
    case 3: return (wchar_t)Range(1, 0x1F);
    default: return (wchar_t)Range(0x20, 0x7E);
    }
}

int main(int argc, char** argv)
{
    bool print = argc > 1 && !strcmp(argv[1], "--print");
    std::string ttf = std::string(__FILE__);
    ttf = ttf.substr(0, ttf.find_last_of("/\\")) + "/../../rust/crates/ee_base/src/plat_ui/golden_font.ttf";
    std::ifstream in(ttf.c_str(), std::ios::binary);
    static std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.empty() || !PlatFontRegister("Synth", 0, 0, data.data(), (uint32_t)data.size())) { printf("no test font at %s\n", ttf.c_str()); return 1; }
    PlatFontAlias("Arial", "Synth");
    int fonts = print ? 400 : 6000, failures = 0;
    long checks = 0;
    for (int n = 0; n < fonts; n++) {
        Orig::FontDescriptor od; Port::FontDescriptor pd;
        od.mFace = pd.mFace = kFaces[Rand(5)];
        od.mSize = pd.mSize = Rand(10) ? Range(4, 30) : Range(0, 3);
        od.mStyle = pd.mStyle = (int)Rand(8);
        std::vector<Call> calls;
        for (int i = 0; i < 12; i++) {
            Call c;
            c.kind = Rand(12) == 0 ? 4 : (int)Rand(4);
            int len = (int)Rand(12);
            for (int k = 0; k < len; k++) c.text.push_back(Rand(4) == 0 ? RandomChar() : (wchar_t)Range('a', 'z'));
            if (Rand(20) == 0 && len >= 2) { c.text[0] = 0xD83D; c.text[1] = 0xDE00; }
            c.a = c.kind == 1 ? (int)RandomChar() : (int)Rand((unsigned)len + 1);
            c.b = (int)Rand((unsigned)(len - (c.kind == 1 ? 0 : c.a) + 1));
            if (c.kind == 1) c.b = 0;
            c.c = Range(-10, 40); c.d = Range(-10, 40);
            c.scale = kScales[Rand(6)];
            c.sw = Rand(10) ? Range(0, 90) : -1; c.sh = Range(0, 60); c.seed = Rand(0x7fffffff);
            if (Rand(3)) { c.clip[0] = Range(-5, 30); c.clip[1] = Range(-5, 30); c.clip[2] = Range(-5, 90); c.clip[3] = Range(-5, 60); }
            else { c.clip[0] = c.clip[1] = 0; c.clip[2] = c.clip[3] = 1000; }
            c.graphics = Rand(5) != 0; c.color = Rand(0xffffffffu);
            calls.push_back(c);
        }
        std::string a = Orig::Run(od, calls), b = Port::Run(pd, calls);
        checks += 12;
        if (a != b && ++failures < 8)
            printf("MISMATCH font %d '%s' %d %d\n--- original\n%s--- port\n%s\n", n, od.mFace.c_str(), od.mSize, od.mStyle, a.c_str(), b.c_str());
        if (print) {
            printf("font [%s] %d %d\n", od.mFace.c_str(), od.mSize, od.mStyle);
            size_t at = 0;
            for (size_t i = 0; i < calls.size(); i++) {
                const Call& c = calls[i];
                size_t e = b.find('\n', at);
                printf("call %d %d %d %d %d %.2f %d %d %u %d %d %d %d %d %u %d", c.kind, c.a, c.b, c.c, c.d, c.scale, c.sw, c.sh, c.seed,
                       c.clip[0], c.clip[1], c.clip[2], c.clip[3], c.graphics, c.color, (int)c.text.size());
                for (size_t k = 0; k < c.text.size(); k++) printf(" %d", (int)c.text[k]);
                printf(" = %s\n", b.substr(at, e - at).c_str());
                at = e + 1;
            }
        }
    }
    if (!print) printf("%d fonts, %ld calls, %d mismatching fonts\n", fonts, checks, failures);
    return failures;
}
