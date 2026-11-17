// PlatGraphics' hooks as Rust (R6 WON round 40): the hooks' statements copied verbatim before the port
// (won_platgfx_orig.h, PlatGraphics renamed OrigGfx) against the port (the shell's forwarders to ee_won's platgfx.rs),
// each on its own copy of the same real surface (WONSurface.h over surface.rs).
//
// 30,000 sequences: a pixel scale (1, 1.25, 1.5, 2, 2.5, 3, 4), a 32-bit surface of 0..48 x 0..48 pixels (now and
// then none, or without pixels), its pixels from a seed, then 40 random hooks (pixels read and set, lines, framed
// and filled rectangles, blends, polygons and filled polygons of 0..7 points, ellipses, filled ellipses, arcs, clip
// rectangles set and cleared) with coordinates -12..60, negative sizes, any colour. After each hook the two
// surfaces' pixel bytes, clip rectangles and colours, and GetPixel's answer, must be the same. `--print` writes the
// first 300 sequences (each hook with the FNV-1a hash of the pixels after it): rust/crates/ee_won/src//
// golden_platgfx.txt, which the Rust tests replay. Exit code: the number of mismatches.
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

namespace WONAPI {
// WONLobbyPlatform.h's
inline int PlatGUIPixels(int inLogical, float inScale)
{
    return inScale == 1.0f ? inLogical : (int)floorf((float)inLogical * inScale + 0.5f);
}

struct OrigGfx {
    SurfaceBase* mSurface;
    float mScale;
    DWORD mColor;
    int PX(int v) const;
    void FillLogicalRect(SurfaceBase* inSurface, int x, int y, int width, int height);
    void ScaledLine(SurfaceBase* inSurface, int x1, int y1, int x2, int y2);
    SurfaceBase* PrepareSurface();
    DWORD GetPixelHook(int x, int y);
    void SetPixelHook(int x, int y, DWORD inColor);
    void DrawLineHook(int x1, int y1, int x2, int y2);
    void DrawRectHook(int x, int y, int width, int height);
    void FillRectHook(int x, int y, int width, int height);
    void FillRectBlendHook(int x, int y, int width, int height, int inBlendAmount);
    void DrawPolygonHook(POINT* inPoints, int inCount);
    void FillPolygonHook(POINT* inPoints, int inCount);
    void DrawEllipseHook(int x, int y, int width, int height);
    void FillEllipseHook(int x, int y, int width, int height);
    void DrawArcHook(int x, int y, int width, int height, int inStartAngle, int inArcAngle);
    void ClearClipRectHook();
    void SetClipRectHook(int x, int y, int width, int height);
};
#include "won_platgfx_orig.h"

// the port: PlatGraphics.cpp's forwarders
struct PortGfx {
    SurfaceBase* mSurface;
    float mScale;
    DWORD mColor;
    DWORD Hook(int inOp, int a = 0, int b = 0, int c = 0, int d = 0, int e = 0, int f = 0)
    {
        return ee_won_plat_gfx(this->mSurface != NULL ? this->mSurface->GetRust() : NULL, this->mScale, (uint32_t)this->mColor,
                               (EeWonPlatGfx)inOp, a, b, c, d, e, f);
    }
    void Polygon(POINT* inPoints, int inCount, bool inFill)
    {
        std::vector<int32_t> theXY;
        for (int i = 0; i < inCount; i++) {
            theXY.push_back((int32_t)inPoints[i].x);
            theXY.push_back((int32_t)inPoints[i].y);
        }
        ee_won_plat_gfx_polygon(this->mSurface != NULL ? this->mSurface->GetRust() : NULL, this->mScale, (uint32_t)this->mColor,
                                inFill, theXY.data(), inCount);
    }
};
}
using namespace WONAPI;

static unsigned long long sSeed = 0x2545F4914F6CDD1Dull;
static unsigned Rand(unsigned n) { sSeed = sSeed * 6364136223846793005ull + 1442695040888963407ull; return n ? (unsigned)((sSeed >> 33) % n) : 0; }
static int Range(int lo, int hi) { return lo + (int)Rand((unsigned)(hi - lo + 1)); }

// the pixels of a seed (the Rust tests make the same)
static void Fill(SurfaceBase* s, uint32_t seed)
{
    uint32_t* p = (uint32_t*)s->GetPixelsBase();
    if (p == NULL) return;
    size_t n = (size_t)s->GetPixelsPerRow() * s->GetHeight();
    for (size_t i = 0; i < n; i++) { seed = seed * 1103515245u + 12345u; p[i] = seed ^ (seed >> 15); }
}
static uint32_t Hash(SurfaceBase* s)
{
    uint32_t h = 2166136261u;
    const unsigned char* p = (const unsigned char*)(s ? s->GetPixelsBase() : NULL);
    if (p == NULL) return 0;
    size_t n = (size_t)s->GetBytesPerRow() * s->GetHeight();
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}
static std::string State(SurfaceBase* s)
{
    if (s == NULL) return "none";
    char b[128];
    snprintf(b, sizeof b, "%08x clip %d %d %d %d color %08x", Hash(s), s->GetClipX1(), s->GetClipY1(), s->GetClipX2(), s->GetClipY2(),
             (unsigned)s->GetColor());
    return b;
}

int main(int argc, char** argv)
{
    bool print = argc > 1 && !strcmp(argv[1], "--print");
    static const float kScales[] = {1.0f, 1.0f, 1.25f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f};
    int sequences = print ? 300 : 30000, failures = 0;
    long checks = 0;
    for (int q = 0; q < sequences; q++) {
        float k = kScales[Rand(8)];
        int kind = (int)Rand(20); // 0: no surface, 1: no pixels, else pixels
        int w = Rand(6) == 0 ? (int)Rand(3) : Range(1, 48), h = Rand(6) == 0 ? (int)Rand(3) : Range(1, 48);
        uint32_t seed = (uint32_t)Rand(0x7fffffff);
        SurfaceBasePtr a, b;
        if (kind != 0) {
            a = SurfaceBase::GetNewSurface(SurfaceFormat_32);
            b = SurfaceBase::GetNewSurface(SurfaceFormat_32);
            if (kind != 1) { a->Create(w, h); b->Create(w, h); Fill(a.get(), seed); Fill(b.get(), seed); }
        }
        OrigGfx o = {a.get(), k, 0};
        PortGfx p = {b.get(), k, 0};
        if (print) printf("seq %d %d %d %d %.2f %u\n", q, kind, w, h, k, seed);
        for (int n = 0; n < 40; n++) {
            int op = Range(1, 13);
            int v[6];
            for (int i = 0; i < 6; i++) v[i] = Range(-12, 60);
            if (op == 6) v[4] = Range(-20, 300);
            if (op == 9) { v[4] = Range(-400, 800); v[5] = Range(-50, 500); }
            DWORD color = (DWORD)Rand(0xffffffffu) ^ (Rand(2) ? 0xff000000u : 0);
            o.mColor = p.mColor = (Rand(3) ? color : o.mColor);
            DWORD ra = 0, rb = 0;
            std::vector<POINT> pts;
            int count = (int)Rand(8);
            for (int i = 0; i < count; i++) { POINT pt; pt.x = Range(-12, 60); pt.y = Range(-12, 60); pts.push_back(pt); }
            std::vector<POINT> pts2 = pts;
            if (op == 13) op = 12;
            if (op == 7) op = Rand(2) ? 14 : 15;
            int A[6] = {v[0], v[1], v[2], v[3], v[4], v[5]};
            if (op == 4 || op == 5 || op == 8 || op == 9 || op == 10) { A[2] -= 12; A[3] -= 12; }
            if (op == 6 || op == 12) { A[2] -= 6; A[3] -= 6; }
            switch (op) {
            case 1: ra = o.GetPixelHook(A[0], A[1]); rb = p.Hook((int)EeWonPlatGfx::GetPixel, A[0], A[1]); break;
            case 2: o.SetPixelHook(A[0], A[1], color); p.Hook((int)EeWonPlatGfx::SetPixel, A[0], A[1], (int)color); break;
            case 3: o.DrawLineHook(A[0], A[1], A[2], A[3]); p.Hook((int)EeWonPlatGfx::Line, A[0], A[1], A[2], A[3]); break;
            case 4: o.DrawRectHook(A[0], A[1], A[2], A[3]); p.Hook((int)EeWonPlatGfx::Rect, A[0], A[1], A[2], A[3]); break;
            case 5: o.FillRectHook(A[0], A[1], A[2], A[3]); p.Hook((int)EeWonPlatGfx::FillRect, A[0], A[1], A[2], A[3]); break;
            case 6: o.FillRectBlendHook(A[0], A[1], A[2], A[3], A[4]); p.Hook((int)EeWonPlatGfx::FillRectBlend, A[0], A[1], A[2], A[3], A[4]); break;
            case 8: o.DrawEllipseHook(A[0], A[1], A[2], A[3]); p.Hook((int)EeWonPlatGfx::Ellipse, A[0], A[1], A[2], A[3]); break;
            case 9: o.DrawArcHook(A[0], A[1], A[2], A[3], A[4], A[5]); p.Hook((int)EeWonPlatGfx::Arc, A[0], A[1], A[2], A[3], A[4], A[5]); break;
            case 10: o.FillEllipseHook(A[0], A[1], A[2], A[3]); p.Hook((int)EeWonPlatGfx::FillEllipse, A[0], A[1], A[2], A[3]); break;
            case 11: o.ClearClipRectHook(); p.Hook((int)EeWonPlatGfx::ClearClip); break;
            case 12: o.SetClipRectHook(A[0], A[1], A[2], A[3]); p.Hook((int)EeWonPlatGfx::SetClip, A[0], A[1], A[2], A[3]); break;
            case 14: o.DrawPolygonHook(pts.data(), count); p.Polygon(pts2.data(), count, false); break;
            case 15: o.FillPolygonHook(pts.data(), count); p.Polygon(pts2.data(), count, true); break;
            }
            checks++;
            std::string sa = State(a.get()), sb = State(b.get());
            if (sa != sb || ra != rb) {
                if (++failures < 8)
                    printf("MISMATCH seq %d op %d (%d %d %d %d %d %d, colour %08x, %d points) scale %.2f: original %s -> %08x, port %s -> %08x\n",
                           q, op, v[0], v[1], v[2], v[3], v[4], v[5], (unsigned)color, count, k, sa.c_str(), (unsigned)ra, sb.c_str(), (unsigned)rb);
            }
            if (print) {
                printf("op %d %d %d %d %d %d %d %u %u %d", op, A[0], A[1], A[2], A[3], A[4], A[5], (unsigned)color, (unsigned)p.mColor, count);
                for (int i = 0; i < count; i++) printf(" %d %d", (int)pts[i].x, (int)pts[i].y);
                printf(" = %u %s\n", (unsigned)rb, sb.c_str());
            }
        }
    }
    if (!print) printf("%d sequences, %ld checks, %d mismatches\n", sequences, checks, failures);
    return failures;
}
