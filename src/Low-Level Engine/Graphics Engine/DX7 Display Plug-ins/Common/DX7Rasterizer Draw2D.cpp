// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Rasterizer Draw2D.cpp (file name
// inferred: the second of the four DX7Rasterizer units in link order; it holds the 2D draws)
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x10002b04..0x1000383a: the U2D key init chains, DX7Rasterizer::Draw for bitmaps, lines, points, rhombi and text
// (export order), the vector<GESurface*> iterator operator+ COMDAT, then the template-static init chain head and
// its atexit registrars.
#include <windows.h>
#include "gx/GX.h"
#include "platform/PlatFont.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "dx7/DX7Rasterizer.h"
#include "dx7/DX7PolygonArray.h"
#include "dx7/DX7StandardVertexArray.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Screen.h"
#include "lle/GEBitmap.h"
#include "lle/GEFont.h"
#include "lle/GEFontBitmap.h"
#include "lle/GELine.h"
#include "lle/GEPoint.h"
#include "lle/GERhombus.h"
#include "lle/GEText.h"
#include "dx7/DX7DrawSeam.h"

// The U2D key init chains:
// SYNTHETIC: DX7HR 0x10002b04
// SYNTHETIC: DX7HR 0x10002b0e
// SYNTHETIC: DX7HR 0x10002b1f
// SYNTHETIC: DX7HR 0x10002b2b
// SYNTHETIC: DX7HR 0x10002b36
// SYNTHETIC: DX7HR 0x10002b40
// SYNTHETIC: DX7HR 0x10002b51
// SYNTHETIC: DX7HR 0x10002b5d
// SYNTHETIC: DX7HR 0x10002b68
// SYNTHETIC: DX7HR 0x10002b72
// SYNTHETIC: DX7HR 0x10002b83
// SYNTHETIC: DX7HR 0x10002b8f
// SYNTHETIC: DX7HR 0x10002b9a
// SYNTHETIC: DX7HR 0x10002ba4
// SYNTHETIC: DX7HR 0x10002bb5
// SYNTHETIC: DX7HR 0x10002bc1
// SYNTHETIC: DX7HR 0x10002bcc
// SYNTHETIC: DX7HR 0x10002bd6
// SYNTHETIC: DX7HR 0x10002be7
// SYNTHETIC: DX7HR 0x10002bf3
// SYNTHETIC: DX7HR 0x10002bfe
// SYNTHETIC: DX7HR 0x10002c08
// SYNTHETIC: DX7HR 0x10002c19
// SYNTHETIC: DX7HR 0x10002c25
// SYNTHETIC: DX7HR 0x10002c30
// SYNTHETIC: DX7HR 0x10002c3a
// SYNTHETIC: DX7HR 0x10002c4b
// SYNTHETIC: DX7HR 0x10002c57

// FUNCTION: DX7HR 0x10002c62
// The draws moved to Rust (rust/crates/ee_dx7/src/draw2d.rs, docs/RUST_PLAN.md R4.2). The C++ here forwards; the
// ee_dx7cpp_* below read the GE objects for it.
long DX7Rasterizer::Draw(GEBitmap* inBitmap, unsigned long inScreen)
{
    return ee_dx7r_draw_bitmap(this, inScreen, DX7DrawState(this, inScreen), inBitmap);
}

// FUNCTION: DX7HR 0x10002e88
long DX7Rasterizer::Draw(GELine* inLine, unsigned long inCount, unsigned long inScreen)
{
    return ee_dx7r_draw_lines(this, inScreen, DX7DrawState(this, inScreen), inLine, inCount);
}

// FUNCTION: DX7HR 0x10003042
long DX7Rasterizer::Draw(GEPoint* inPoint, unsigned long inCount, unsigned long inScreen)
{
    return ee_dx7r_draw_points(this, inScreen, DX7DrawState(this, inScreen), inPoint, inCount);
}

// FUNCTION: DX7HR 0x100031b6
long DX7Rasterizer::Draw(GERhombus* inRhombus, unsigned long inScreen)
{
    return ee_dx7r_draw_rhombus(this, inScreen, DX7DrawState(this, inScreen), inRhombus);
}

static Dx7StdColorVertex* DX7StdColorVerticesOf(GEVertexArray* inArray)
{
    return reinterpret_cast<Dx7StdColorVertex*>(static_cast<DX7StdColorVertexArray*>(inArray)->mVertices);
}

extern "C" {
void* ee_dx7cpp_bitmap_texture(void* inBitmap, unsigned long inScreen) { return static_cast<GEBitmap*>(inBitmap)->GetTexture(inScreen); }
Dx7StdColorVertex* ee_dx7cpp_bitmap_vertices(void* inBitmap) { return DX7StdColorVerticesOf(static_cast<GEBitmap*>(inBitmap)->GetVertexArray()); }
void* ee_dx7cpp_line_texture(void* inLines, unsigned long inScreen) { return static_cast<GELine*>(inLines)->GetTexture(inScreen); }
Dx7StdColorVertex* ee_dx7cpp_line_vertices(void* inLines, unsigned long inIndex) { return DX7StdColorVerticesOf(static_cast<GELine*>(inLines)[inIndex].GetVertexArray()); }
void* ee_dx7cpp_point_texture(void* inPoints, unsigned long inScreen) { return static_cast<GEPoint*>(inPoints)->GetTexture(inScreen); }
Dx7StdColorVertex* ee_dx7cpp_point_vertices(void* inPoints, unsigned long inIndex) { return DX7StdColorVerticesOf(static_cast<GEPoint*>(inPoints)[inIndex].GetVertexArray()); }
Dx7StdColorVertex* ee_dx7cpp_rhombus_vertices(void* inRhombus) { return DX7StdColorVerticesOf(static_cast<GERhombus*>(inRhombus)->GetVertexArray()); }
void ee_dx7cpp_texture_frame(void* inTexture, float* outSize, float* outCorner)
{
    GETexture* theTexture = static_cast<GETexture*>(inTexture);
    *outSize = theTexture->GetFrameUVSize();
    outCorner[0] = theTexture->GetFrameTopLeftCorner().GetU();
    outCorner[1] = theTexture->GetFrameTopLeftCorner().GetV();
}
int32_t ee_dx7cpp_text_measure(const void* inFont, const char* inText, uint32_t inLength) { return PlatFontMeasure(static_cast<const PlatFont*>(inFont), inText, inLength); }
const void* ee_dx7cpp_text_scaled(const void* inFont, float inScale) { return PlatFontScaled(static_cast<const PlatFont*>(inFont), inScale); }
int32_t ee_dx7cpp_font_height(const void* inFont)
{
    PlatFontMetrics theMetrics;
    PlatFontGetMetrics(static_cast<const PlatFont*>(inFont), &theMetrics);
    return theMetrics.mHeight;
}
void ee_dx7cpp_text_draw(const void* inFont, const char* inText, uint32_t inLength, int32_t inX, int32_t inY, uint32_t inColor, int inOpaque,
                         uint32_t inBackground, const int32_t* inClip, const void* inTarget)
{
    PlatFontRect theClip = {inClip[0], inClip[1], inClip[2], inClip[3]};
    PlatFontDraw(static_cast<const PlatFont*>(inFont), inText, inLength, inX, inY, inColor, inOpaque, inBackground, &theClip,
                 static_cast<const PlatFontTarget*>(inTarget));
}
}

// One text draw's plain inputs (Rust: draw2d.rs TextInfo). The layout and the draws are in Rust.
static EeDx7TextInfo DX7TextInfoOf(GEText* inText)
{
    GEFont* theFont = inText->GetFont();
    EeDx7TextInfo theInfo;
    theInfo.font = theFont->GetOSFont();
    theInfo.text = inText->GetText();
    // the text's bytes, as PlatFontMeasure/PlatFontDraw take it (GetLength counts characters: under UTF-8, decision
    // D6, a multi-byte character made it short, and the text lost its last characters)
    theInfo.length = inText->GetText().GetTCHARLength();
    theInfo.x = inText->GetLocation().GetX();
    theInfo.y = inText->GetLocation().GetY();
    theInfo.alignment = inText->GetAlignment();
    theInfo.transparent = inText->GetIsTransparent();
    theInfo.shadow_font = (theFont->GetFontStyle() & kFontStyleShadow) != 0;
    theInfo.foreground[0] = inText->GetFgColor().GetRed();
    theInfo.foreground[1] = inText->GetFgColor().GetGreen();
    theInfo.foreground[2] = inText->GetFgColor().GetBlue();
    theInfo.background[0] = inText->GetBgColor().GetRed();
    theInfo.background[1] = inText->GetBgColor().GetGreen();
    theInfo.background[2] = inText->GetBgColor().GetBlue();
    return theInfo;
}

static_assert(sizeof(EeDx7TextRect) == sizeof(RECT), "ee_dx7/src/draw2d.rs TextRect");

// One text draw (GDI in the original): a GXSurface::DrawText callback; the target covers the surface from
// (inOriginX, inOriginY). The font is referenced around the draw, as the original did.
struct DX7TextJob {
    GEText* mText;
    const RECT* mClip;
    long mResult;
};

static bool DX7DrawTextPlat(const PlatFontTarget& inTarget, long inOriginX, long inOriginY, void* inJob)
{
    DX7TextJob* theJob = static_cast<DX7TextJob*>(inJob);
    GEFont* theFont = theJob->mText->GetFont();
    theFont->Reference();
    EeDx7TextInfo theInfo = DX7TextInfoOf(theJob->mText);
    if (!ee_dx7r_text_draw(&theInfo, reinterpret_cast<const EeDx7TextRect*>(theJob->mClip), inOriginX, inOriginY,
                           inTarget.mWidth, inTarget.mHeight, inTarget.mScale, &inTarget))
        theJob->mResult = kErrorNativeError;
    theFont->Dereference();
    return true;
}

// Draws a text on the target: onto the GPU copy of a GPU-resident target (no read-back), else onto its CPU copy (a
// text-locked surface: the glyph atlases)
static long DX7DrawText(GEText* inText, const RECT* inClip, GESurface* inTarget)
{
    DX7TextJob theJob = {inText, inClip, kErrorOK};
    GXSurface* theSurface = *static_cast<DX7Surface*>(inTarget);
    EeDx7TextInfo theInfo = DX7TextInfoOf(inText);
    EeDx7TextRect theBounds;
    ee_dx7r_text_bounds(&theInfo, reinterpret_cast<const EeDx7TextRect*>(inClip), &theBounds);
    RECT theRect;
    memcpy(&theRect, &theBounds, sizeof(RECT));
    if (theSurface->DrawText(theRect, DX7DrawTextPlat, &theJob) != GX_OK && theJob.mResult == kErrorOK)
        theJob.mResult = kErrorNativeError;
    return theJob.mResult;
}

// FUNCTION: DX7HR 0x100032a9
// Draws a text with GDI on the target, aligned; shadow fonts get a black or white shadow 1 pixel down-right
long DX7Rasterizer::Draw(GEText* inText, GESurface* inTarget)
{
    return DX7DrawText(inText, NULL, inTarget);
}

// FUNCTION: DX7HR 0x100034e8
// Draws a text clipped to a rectangle: with the font's bitmap font when it has one, else with GDI (aligned, shadowed)
long DX7Rasterizer::Draw(GEText* inText, const GERectangle& inRectangle, GESurface* inTarget)
{
    if (inText->GetFont()->GetFontBitmap() != NULL)
        return inText->GetFont()->GetFontBitmap()->Draw(inText, inRectangle, this, 0);
    RECT theClip;
    theClip.top = UMFloor(inRectangle.GetTop());
    theClip.left = UMFloor(inRectangle.GetLeft());
    theClip.right = UMFloor(inRectangle.GetRight());
    theClip.bottom = UMFloor(inRectangle.GetBottom());
    return DX7DrawText(inText, &theClip, inTarget);
}

// COMDATs this unit is the first (link order) to instantiate, then the template-static init chain head and atexit
// registrars (GELOD x3, then U2DSparseArray):
// TEMPLATE: DX7HR 0x100037d0
// SYNTHETIC: DX7HR 0x100037e2
// SYNTHETIC: DX7HR 0x1000380a
// SYNTHETIC: DX7HR 0x10003816
// SYNTHETIC: DX7HR 0x10003822
// SYNTHETIC: DX7HR 0x1000382e

// DX7HRTnLDisplay.dll (the T&L twin builds this unit unchanged): the same functions at their twin addresses.
// SYNTHETIC: DX7HRTNL 0x10002b83
// SYNTHETIC: DX7HRTNL 0x10002b8d
// SYNTHETIC: DX7HRTNL 0x10002b9e
// SYNTHETIC: DX7HRTNL 0x10002baa
// SYNTHETIC: DX7HRTNL 0x10002bb5
// SYNTHETIC: DX7HRTNL 0x10002bbf
// SYNTHETIC: DX7HRTNL 0x10002bd0
// SYNTHETIC: DX7HRTNL 0x10002bdc
// SYNTHETIC: DX7HRTNL 0x10002be7
// SYNTHETIC: DX7HRTNL 0x10002bf1
// SYNTHETIC: DX7HRTNL 0x10002c02
// SYNTHETIC: DX7HRTNL 0x10002c0e
// SYNTHETIC: DX7HRTNL 0x10002c19
// SYNTHETIC: DX7HRTNL 0x10002c23
// SYNTHETIC: DX7HRTNL 0x10002c34
// SYNTHETIC: DX7HRTNL 0x10002c40
// SYNTHETIC: DX7HRTNL 0x10002c4b
// SYNTHETIC: DX7HRTNL 0x10002c55
// SYNTHETIC: DX7HRTNL 0x10002c66
// SYNTHETIC: DX7HRTNL 0x10002c72
// SYNTHETIC: DX7HRTNL 0x10002c7d
// SYNTHETIC: DX7HRTNL 0x10002c87
// SYNTHETIC: DX7HRTNL 0x10002c98
// SYNTHETIC: DX7HRTNL 0x10002ca4
// SYNTHETIC: DX7HRTNL 0x10002caf
// SYNTHETIC: DX7HRTNL 0x10002cb9
// SYNTHETIC: DX7HRTNL 0x10002cca
// SYNTHETIC: DX7HRTNL 0x10002cd6
// FUNCTION: DX7HRTNL 0x10002ce1
// FUNCTION: DX7HRTNL 0x10002f07
// FUNCTION: DX7HRTNL 0x100030c1
// FUNCTION: DX7HRTNL 0x10003235
// FUNCTION: DX7HRTNL 0x10003328
// FUNCTION: DX7HRTNL 0x10003567
// SYNTHETIC: DX7HRTNL 0x1000384f
// SYNTHETIC: DX7HRTNL 0x10003877
// SYNTHETIC: DX7HRTNL 0x10003883
// SYNTHETIC: DX7HRTNL 0x1000388f
// SYNTHETIC: DX7HRTNL 0x1000389b
