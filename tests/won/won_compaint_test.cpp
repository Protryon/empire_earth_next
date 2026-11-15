// The image and rectangle components' paints as Rust plans (R6 WON round 37): the original ImageComponent.cpp's
// ImageComponent::Paint (after Component::Paint), RectangleComponent::Paint (after Component::Paint) and
// RectangleComponent3D::Paint, copied from the C++ before the port over a logged Graphics and image, against
// ee_won's plans (guidraw.rs image_component_paint, rectangle_paint, rectangle_3d_paint) executed as WONDrawOne
// executes them on the same logged Graphics.
//
// Each of 300,000 paints draws a random component size (-3..70, now and then 0 or large), an image or none (its size
// 0..40, 1, or 0 with the component empty in that direction: the original never left the tiling of a 0-pixel image
// over a component with room), stretch and tile flags, colours (-1, 0xffffffff, random), a thickness -2..6 and up
// or down; the two traces (every Graphics and image call) are compared. The first 400 paints are
// rust/crates/ee_won/src/golden_compaint.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include "rust/ee_won.h"

typedef uint32_t DWORD;
enum { COLOR_INVALID = 0xffffffff };
static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }
static int RI(int lo, int hi) { return lo + (int)R((unsigned)(hi - lo + 1)); }

static std::string sTrace;
static void T(const char* f, ...)
{
    char aLine[160];
    va_list v;
    va_start(v, f);
    vsnprintf(aLine, sizeof(aLine), f, v);
    va_end(v);
    if (!sTrace.empty()) sTrace += "; ";
    sTrace += aLine;
}

struct Image {
    unsigned short mWidth, mHeight;
    void SetSize(int w, int h) { T("setsize %d %d", w, h); mWidth = (unsigned short)w; mHeight = (unsigned short)h; }
};
struct Graphics {
    void SetColor(DWORD c) { T("color %x", (unsigned)c); }
    void FillRect(int x, int y, int w, int h) { T("fill %d %d %d %d", x, y, w, h); }
    void DrawRect(int x, int y, int w, int h) { T("rect %d %d %d %d", x, y, w, h); }
    void DrawLine(int x1, int y1, int x2, int y2) { T("line %d %d %d %d", x1, y1, x2, y2); }
    void DrawImage(Image* i, int x, int y) { T("image %d %d %d %d", x, y, i->mWidth, i->mHeight); }
    void PushClipRect(int x, int y, int w, int h) { T("push %d %d %d %d", x, y, w, h); }
    void PopClipRect() { T("pop"); }
};
template <class X> struct P {
    X* p;
    X* get() const { return p; }
    X* operator->() const { return p; }
};
struct Comp {
    int w, h;
    int Width() { return w; }
    int Height() { return h; }
};

// ---- the original statements (`this` a stand-in with the members the paints read)
namespace orig {
struct ImageComponent : Comp {
    P<Image> mImage;
    bool mTile, mStretch;
    void Paint(Graphics& g)
    {
        if(this->mImage.get()==NULL)
            return;
        if(this->mStretch)
        {
            if(this->mImage->mWidth!=this->Width() || this->mImage->mHeight!=this->Height())
                this->mImage->SetSize(this->Width(),this->Height());
            g.DrawImage(this->mImage.get(),0,0);
        }
        else if(!this->mTile)
            g.DrawImage(this->mImage.get(),0,0);
        else
        {
            g.PushClipRect(0,0,this->Width(),this->Height());
            for(int x=0;x<this->Width();x+=this->mImage->mWidth)
                for(int y=0;y<this->Height();y+=this->mImage->mHeight)
                    g.DrawImage(this->mImage.get(),x,y);
            g.PopClipRect();
        }
    }
};
struct RectangleComponent : Comp {
    int mSolidColor, mOutlineColor;
    void Paint(Graphics &g)
    {
        if(this->mSolidColor!=-1)
        {
            g.SetColor(this->mSolidColor);
            g.FillRect(0,0,this->Width(),this->Height());
        }

        if(this->mOutlineColor!=-1)
        {
            g.SetColor(this->mOutlineColor);
            g.DrawRect(0,0,this->Width(),this->Height());
        }
    }
};
struct RectangleComponent3D : Comp {
    bool mUp;
    int mThickness;
    int mLightColor, mDarkColor, mSolidColor;
    void Paint(Graphics &g)
    {
        int width = this->Width()-1;
        int height = this->Height()-1;
        bool up = this->mUp;
        if(this->mSolidColor!=COLOR_INVALID)
        {
            g.SetColor(this->mSolidColor);
            g.FillRect(0,0,this->Width(),this->Height());
        }

        for(int i=0; i<this->mThickness; i++)
        {
            if(up)
                g.SetColor(this->mLightColor);
            else
                g.SetColor(this->mDarkColor);

            g.DrawLine(i,i,width-i,i);
            g.DrawLine(i,i,i,height-i);
            if(up)
                g.SetColor(this->mDarkColor);
            else
                g.SetColor(this->mLightColor);

            g.DrawLine(width-i,i,width-i,height);
            g.DrawLine(i,height-i,width,height-i);
        }
    }
};
}

// ---- the plans, executed as WONDrawOne executes them (slot 0 the image)
static void Run(Graphics& g, Image* image, const std::vector<EeWonDrawOp>& ops)
{
    for (size_t i = 0; i < ops.size(); i++) {
        const EeWonDrawOp& o = ops[i];
        switch (o.kind) {
        case EE_WON_DRAW_SET_COLOR: g.SetColor((DWORD)(unsigned int)o.a); break;
        case EE_WON_DRAW_FILL_RECT: g.FillRect(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_RECT: g.DrawRect(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_LINE: g.DrawLine(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_IMAGE: g.DrawImage(o.a == 0 ? image : NULL, o.b, o.c); break;
        case EE_WON_DRAW_IMAGE_SIZE: (o.a == 0 ? image : NULL)->SetSize(o.b, o.c); break;
        case EE_WON_DRAW_PUSH_CLIP: g.PushClipRect(o.a, o.b, o.c, o.d); break;
        case EE_WON_DRAW_POP_CLIP: g.PopClipRect(); break;
        default: T("bad kind %d", o.kind); break;
        }
    }
}
template <class Plan> static std::vector<EeWonDrawOp> Ops(Plan plan)
{
    std::vector<EeWonDrawOp> v(48);
    size_t n = plan(&v[0], v.size());
    if (n > v.size()) { v.resize(n); n = plan(&v[0], v.size()); }
    v.resize(n);
    return v;
}

static int Size()
{
    switch (R(16)) {
    case 0: return 0;
    case 1: return RI(-3, -1);
    case 2: return RI(200, 400);
    default: return RI(1, 70);
    }
}
static int Color()
{
    switch (R(5)) {
    case 0: return -1;
    case 1: return (int)COLOR_INVALID;
    default: return (int)(R(0x8000) | (R(0x8000) << 15) | (R(4) << 30));
    }
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    int aFailures = 0;
    const int kPaints = 300000;
    for (int n = 0; n < kPaints; n++) {
        sSeed = 2749u * (unsigned)n + 11u;
        int kind = (int)R(3), w = Size(), h = Size();
        Graphics g;
        std::string ta, tb, in;
        char aLine[200];
        if (kind == 0) {
            bool has = R(8) != 0, stretch = R(3) == 0, tile = R(2) != 0;
            int iw = R(6) == 0 ? 0 : R(6) == 0 ? 1 : (int)R(41), ih = R(6) == 0 ? 0 : R(6) == 0 ? 1 : (int)R(41);
            if (R(3) == 0) { iw = w; ih = h; }
            if (tile && (iw < 4 || ih < 4)) { if (w > 70) w = 70; if (h > 70) h = 70; }   // a few thousand tiles at most
            // the original's tiling never ended over a component with room in a direction the image has none of
            if (has && !stretch && tile && ((iw == 0 && w > 0) || (ih == 0 && h > 0 && w > 0))) tile = false;
            Image a = {(unsigned short)iw, (unsigned short)ih}, b = a;
            orig::ImageComponent c;
            c.w = w; c.h = h; c.mImage.p = has ? &a : NULL; c.mTile = tile; c.mStretch = stretch;
            c.Paint(g); ta = sTrace; sTrace.clear();
            Run(g, &b, Ops([&](EeWonDrawOp* o, size_t cap) {
                return ee_won_gui_draw_image_component(has, stretch, tile, b.mWidth, b.mHeight, w, h, o, cap); }));
            tb = sTrace; sTrace.clear();
            snprintf(aLine, sizeof(aLine), "image %d %d %d %d %d %d %d", has ? 1 : 0, stretch ? 1 : 0, tile ? 1 : 0, iw, ih, w, h);
        } else if (kind == 1) {
            orig::RectangleComponent c;
            c.w = w; c.h = h; c.mSolidColor = Color(); c.mOutlineColor = Color();
            c.Paint(g); ta = sTrace; sTrace.clear();
            Run(g, NULL, Ops([&](EeWonDrawOp* o, size_t cap) {
                return ee_won_gui_draw_rectangle(c.mSolidColor, c.mOutlineColor, w, h, o, cap); }));
            tb = sTrace; sTrace.clear();
            snprintf(aLine, sizeof(aLine), "rect %d %d %d %d", c.mSolidColor, c.mOutlineColor, w, h);
        } else {
            orig::RectangleComponent3D c;
            c.w = w; c.h = h; c.mUp = R(2) != 0; c.mThickness = RI(-2, 6);
            c.mLightColor = Color(); c.mDarkColor = Color(); c.mSolidColor = Color();
            c.Paint(g); ta = sTrace; sTrace.clear();
            Run(g, NULL, Ops([&](EeWonDrawOp* o, size_t cap) {
                return ee_won_gui_draw_rectangle_3d(c.mUp, c.mThickness, c.mSolidColor, c.mLightColor, c.mDarkColor, w, h, o, cap); }));
            tb = sTrace; sTrace.clear();
            snprintf(aLine, sizeof(aLine), "rect3d %d %d %d %d %d %d %d", c.mUp ? 1 : 0, c.mThickness, c.mSolidColor,
                     c.mLightColor, c.mDarkColor, w, h);
        }
        if (ta != tb && ++aFailures < 20) printf("FAIL %d %s\n orig %s\n rust %s\n", n, aLine, ta.c_str(), tb.c_str());
        if (aPrint && n < 400) printf("%s | %s\n", aLine, ta.c_str());
    }
    if (aPrint) return 0;
    printf("won_compaint_test: %d paints, %d failures\n", kPaints, aFailures);
    return aFailures ? 1 : 0;
}
