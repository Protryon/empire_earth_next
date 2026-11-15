// C:\EEX\Low-Level Engine\Graphics Engine\GEBitmap.cpp (file name inferred: no __FILE__; alphabetically
// GE3DPoint < GEBitmap < GEBoundingBox, and GEBitmap's own code is the only non-inline code in the range)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1000dd9e..0x10010550. The unit opens with the header-inline members of every class it is the first to include,
// in include order (URandom, GEColor, GESurface, GETextureManager, GETexture, GEWindow, GEScreen, GERectangle,
// GEViewport, GEVertexArray, the U2DPoint/U2DRectangle key statics, GERasterizer, GE2DVertex), then GEBitmap's own
// code (0x1000f3a4) and the map<UString, GETexture*> / vector<GERScreenState*> COMDATs.
#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/URandom.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/TSScheduler.h"
#include "lle/UReferenceCounter.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GE2DVertex.h"
#include "lle/GEBitmap.h"
#include "lle/GELine.h"   // GEVertexHost
#include "GERustMath.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// header-inline members emitted by this unit: URandom(unsigned long) 0x1000dd9e, operator= 0x1000ddca (ICF pair
// with URandomIntegers'), Generate(float, float) 0x1000de1a, the default-ctor closure 0x1000de43
// FUNCTION: LLE 0x1000dd9e
// FUNCTION: LLE 0x1000ddca
// FUNCTION: LLE 0x1000de1a

// FUNCTION: LLE 0x1000dde9
// Advances the random generator one step and returns the new seed as a fraction in [0, 1)
void (*gURandomTraceHook)(URandom* inRandom) = NULL;   // port, TEST ONLY (lle/URandom.h)

float URandom::Generate()
{
    this->mSeed = (this->mMultiplier * this->mSeed + this->mIncrement) % this->mModulus;
    if (gURandomTraceHook != NULL)
        gURandomTraceHook(this);
    return (float)((double)this->mSeed / (double)this->mModulus);
}

// FUNCTION: LLE 0x1000de30
// Advances the random generator one step and returns the new seed
unsigned long URandom::NextInSeries()
{
    this->mSeed = (this->mMultiplier * this->mSeed + this->mIncrement) % this->mModulus;
    return this->mSeed;
}

// header-inline (lle/URandom.h, after the class), emitted by this unit
// FUNCTION: LLE 0x1000de5f
// FUNCTION: LLE 0x1000de95

// ---- GERectangle members this unit emits (0x1000e39e..0x1000e78f)

// SYNTHETIC: LLE 0x1000e39e
// GERectangle(): header-inline (lle/GERectangle.h), emitted here
//
//
//
//
//
//

// SYNTHETIC: LLE 0x1000e3b4
// GERectangle(const GE2DPoint&, const GE2DPoint&): header-inline (lle/GERectangle.h), emitted here

// FUNCTION: LLE 0x1000e3d6
// Creates a zero-size rectangle at the given point
GERectangle::GERectangle(const GE2DPoint& inPoint)
{
    this->mTop = inPoint.GetY();
    this->mLeft = inPoint.GetX();
    this->mBottom = inPoint.GetY();
    this->mRight = inPoint.GetX();
}

// FUNCTION: LLE 0x1000e3f4
// Creates a rectangle from a Windows RECT
GERectangle::GERectangle(const tagRECT& inRect)
{
    this->mTop = (float)inRect.top;
    this->mLeft = (float)inRect.left;
    this->mBottom = (float)inRect.bottom;
    this->mRight = (float)inRect.right;
}

// SYNTHETIC: LLE 0x1000e413
// GERectangle(long x4): header-inline (lle/GERectangle.h), emitted here

// SYNTHETIC: LLE 0x1000e433
// GERectangle(float, float, float, float): header-inline (lle/GERectangle.h), emitted here
//
//
//
//
//
//

// SYNTHETIC: LLE 0x1000e453
// GERectangle(const GE2DPoint&, float, float): header-inline (lle/GERectangle.h), emitted here

// FUNCTION: LLE 0x1000e488
// Returns whether both rectangles have the same four edges
bool GERectangle::operator==(const GERectangle& inRectangle) const
{
    return this->mTop == inRectangle.mTop && this->mLeft == inRectangle.mLeft && this->mRight == inRectangle.mRight &&
           this->mBottom == inRectangle.mBottom;
}

// SYNTHETIC: LLE 0x1000e4c0
// GERectangle::operator!=: header-inline, emitted here

// FUNCTION: LLE 0x1000e517
// Returns whether the point lies inside the rectangle, edges included
bool GERectangle::Contains(const GE2DPoint& inPoint) const
{
    return inPoint.mX >= this->mLeft && inPoint.mX <= this->mRight && inPoint.mY >= this->mTop && inPoint.mY <= this->mBottom;
}

// SYNTHETIC: LLE 0x1000e54e
// GERectangle::Contains(const GERectangle&): header-inline (lle/GERectangle.h), emitted here

// FUNCTION: LLE 0x1000e586
// Grows the rectangle just enough to contain the point
void GERectangle::Extend(const GE2DPoint& inPoint)
{
    this->mLeft = min(this->mLeft, inPoint.GetX());
    this->mRight = max(this->mRight, inPoint.GetX());
    this->mTop = min(this->mTop, inPoint.mY);
    this->mBottom = max(this->mBottom, inPoint.mY);
}

// SYNTHETIC: LLE 0x1000e5e4
// GERectangle::GetBottomLeft: header-inline (lle/GERectangle.h), emitted here
//
//
//
//

// SYNTHETIC: LLE 0x1000e5f9
// GERectangle::GetBottomRight: header-inline (lle/GERectangle.h), emitted here
//
//
//
//

// FUNCTION: LLE 0x1000e60e
// Returns the rectangle's centre point in outCenter
void GERectangle::GetCenter(GE2DPoint& outCenter) const
{
    outCenter.mX = (this->mRight - this->mLeft) * 0.5f + this->mLeft;
    outCenter.mY = (this->mBottom - this->mTop) * 0.5f + this->mTop;
}

// SYNTHETIC: LLE 0x1000e63d
// GERectangle::GetTopLeft: header-inline (lle/GERectangle.h), emitted here
//
//
//
//

// SYNTHETIC: LLE 0x1000e653
// GERectangle::GetTopRight: header-inline (lle/GERectangle.h), emitted here
//
//
//
//

// SYNTHETIC: LLE 0x1000e670
// GERectangle::Intersects: header-inline (lle/GERectangle.h), emitted here

// FUNCTION: LLE 0x1000e6a8
// Rounds each edge of the rectangle to the nearest whole number
void GERectangle::RoundToNearest()
{
    this->mTop = (float)UMFloor(this->mTop + 0.5f);
    this->mLeft = (float)UMFloor(this->mLeft + 0.5f);
    this->mRight = (float)UMFloor(this->mRight + 0.5f);
    this->mBottom = (float)UMFloor(this->mBottom + 0.5f);
}

// SYNTHETIC: LLE 0x1000e74e
// GERectangle::SetBottomRight: header-inline (lle/GERectangle.h), emitted here

// SYNTHETIC: LLE 0x1000e77d
// GERectangle::SetTopLeft: header-inline (lle/GERectangle.h), emitted here

// ---- Header-inline members first emitted here (COMDATs; markers only)
// FUNCTION: LLE 0x1000deb3
// FUNCTION: LLE 0x1000dec4
// FUNCTION: LLE 0x1000df33
// FUNCTION: LLE 0x1000df43
// SYNTHETIC: LLE 0x1000df58
// FUNCTION: LLE 0x1000dfc9
// FUNCTION: LLE 0x1000dfd3
// FUNCTION: LLE 0x1000e013
// FUNCTION: LLE 0x1000e040
// FUNCTION: LLE 0x1000e047
// FUNCTION: LLE 0x1000e04b
// FUNCTION: LLE 0x1000e06e
// FUNCTION: LLE 0x1000e075
// FUNCTION: LLE 0x1000e08c
// FUNCTION: LLE 0x1000e090
// FUNCTION: LLE 0x1000e0b3
// FUNCTION: LLE 0x1000e0bb
// FUNCTION: LLE 0x1000e0c2
// FUNCTION: LLE 0x1000e0c9
// FUNCTION: LLE 0x1000e0d4
// FUNCTION: LLE 0x1000e0da
// FUNCTION: LLE 0x1000e0e2
// FUNCTION: LLE 0x1000e0e6
// FUNCTION: LLE 0x1000e0fc
// FUNCTION: LLE 0x1000e109
// FUNCTION: LLE 0x1000e10e
// FUNCTION: LLE 0x1000e113
// FUNCTION: LLE 0x1000e11d
// FUNCTION: LLE 0x1000e1bf
// SYNTHETIC: LLE 0x1000e255
// FUNCTION: LLE 0x1000e2b0
// FUNCTION: LLE 0x1000e2e0
// FUNCTION: LLE 0x1000e311
// FUNCTION: LLE 0x1000e31a
// FUNCTION: LLE 0x1000e321
// FUNCTION: LLE 0x1000e327
// FUNCTION: LLE 0x1000e331
// SYNTHETIC: LLE 0x1000e34c
// FUNCTION: LLE 0x1000e4f8
// FUNCTION: LLE 0x1000e78f
// FUNCTION: LLE 0x1000e7a5
// FUNCTION: LLE 0x1000e7de
// FUNCTION: LLE 0x1000e7e5
// FUNCTION: LLE 0x1000e7ec
// FUNCTION: LLE 0x1000e7f3
// FUNCTION: LLE 0x1000e7fa
// FUNCTION: LLE 0x1000e815
// FUNCTION: LLE 0x1000e81c
// FUNCTION: LLE 0x1000e823
// FUNCTION: LLE 0x1000e82a
// FUNCTION: LLE 0x1000e83e
// FUNCTION: LLE 0x1000e852
// FUNCTION: LLE 0x1000e859
// FUNCTION: LLE 0x1000e860
// FUNCTION: LLE 0x1000e864
// FUNCTION: LLE 0x1000e86b
// FUNCTION: LLE 0x1000e872
// FUNCTION: LLE 0x1000e879
// FUNCTION: LLE 0x1000e87d
// FUNCTION: LLE 0x1000e884
// FUNCTION: LLE 0x1000e88b
// FUNCTION: LLE 0x1000e892
// FUNCTION: LLE 0x1000e899
// FUNCTION: LLE 0x1000e8d4
// FUNCTION: LLE 0x1000e910
// FUNCTION: LLE 0x1000e959
// FUNCTION: LLE 0x1000e970
// FUNCTION: LLE 0x1000e992
// FUNCTION: LLE 0x1000ebfa
// SYNTHETIC: LLE 0x1000ec05
// FUNCTION: LLE 0x1000ed7c
// FUNCTION: LLE 0x1000ed82
// FUNCTION: LLE 0x1000ed97
// FUNCTION: LLE 0x1000edac
// FUNCTION: LLE 0x1000edc1
// FUNCTION: LLE 0x1000edc5
// FUNCTION: LLE 0x1000edda
// FUNCTION: LLE 0x1000ee0c
// FUNCTION: LLE 0x1000ee21
// FUNCTION: LLE 0x1000ee36
// FUNCTION: LLE 0x1000ee4b
// FUNCTION: LLE 0x1000ee60
// FUNCTION: LLE 0x1000ee75
// FUNCTION: LLE 0x1000ee8a
// FUNCTION: LLE 0x1000ee9f
// FUNCTION: LLE 0x1000eeb4
// FUNCTION: LLE 0x1000ef33
// FUNCTION: LLE 0x1000ef48
// FUNCTION: LLE 0x1000ef4c
// FUNCTION: LLE 0x1000ef61
// FUNCTION: LLE 0x1000ef65
// FUNCTION: LLE 0x1000ef7a
// FUNCTION: LLE 0x1000ef8f
// FUNCTION: LLE 0x1000ef9d
// FUNCTION: LLE 0x1000efb5
// FUNCTION: LLE 0x1000efca
// FUNCTION: LLE 0x1000efdf
// FUNCTION: LLE 0x1000eff2
// FUNCTION: LLE 0x1000f007
// FUNCTION: LLE 0x1000f01c
// FUNCTION: LLE 0x1000f031
// FUNCTION: LLE 0x1000f046
// FUNCTION: LLE 0x1000f05b
// FUNCTION: LLE 0x1000f073
// FUNCTION: LLE 0x1000f088
// FUNCTION: LLE 0x1000f097
// FUNCTION: LLE 0x1000f0ac
// FUNCTION: LLE 0x1000f0c1
// FUNCTION: LLE 0x1000f0d6
// FUNCTION: LLE 0x1000f0e0
// FUNCTION: LLE 0x1000f0ea
// FUNCTION: LLE 0x1000f102
// FUNCTION: LLE 0x1000f114
// FUNCTION: LLE 0x1000f126
// FUNCTION: LLE 0x1000f144
// FUNCTION: LLE 0x1000f15b
// FUNCTION: LLE 0x1000f1e6
// SYNTHETIC: LLE 0x1000f279
// FUNCTION: LLE 0x1000f2d4
// FUNCTION: LLE 0x1000f308
// FUNCTION: LLE 0x1000f34f

// ---- GEBitmap (0x1000f3a4..). Vertex order: 0 top left, 1 top right, 2 bottom left, 3 bottom right.
//
// port (R4.3, ninth part): the bitmap is a whole Rust object (ee_ge bmpobj.rs): the constructor's quad, the
// destructor's releases, the corners, the stretched coordinates, the flips, the per-vertex values and SetTexture.
// This section is its shell: the forwarders and the host, which makes each call the original made on another
// object (the rasterizer, the vertex array, the textures). Floats cross the host as their bits.

// ---- the vertex host: the calls GEBitmap, GELine, GEPoint and GERhombus (ee_ge bmpobj.rs, prim.rs) make on other objects

static intptr_t GEBitmapFloatBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return (intptr_t)theBits;
}

static float GEBitmapBitsFloat(intptr_t inBits)
{
    uint32_t theBits = (uint32_t)inBits;
    float theValue;
    memcpy(&theValue, &theBits, 4);
    return theValue;
}

intptr_t GEVertexHost(void* inOwner, EeGeBmpOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    GEVertexArray* theVertices = (GEVertexArray*)inObj;
    GETexture* theTexture = (GETexture*)inObj;
    unsigned long i = (unsigned long)a;
    switch (inOp) {
    case EeGeBmpOp::RasterActive:
        return (intptr_t)GERasterizer::GetActiveRasterizer();
    case EeGeBmpOp::RasterScreenCount:
        return (intptr_t)((GERasterizer*)inObj)->GetScreenCount();
    case EeGeBmpOp::RasterVertexArray:
        return (intptr_t)((GERasterizer*)inObj)->CreateVertexArray((unsigned long)a, (unsigned long)b);
    case EeGeBmpOp::VertexDelete:
        delete theVertices;
        return 0;
    case EeGeBmpOp::VertexGetColor:
        theVertices->GetColor(*(GEColor*)b, i);
        return 0;
    case EeGeBmpOp::VertexSetColor:
        theVertices->SetColor(*(const GEColor*)b, i);
        return 0;
    case EeGeBmpOp::VertexGetPoint:
        theVertices->GetPoint(*(GE3DPoint*)b, i);
        return 0;
    case EeGeBmpOp::VertexSetPoint:
        theVertices->SetPoint(*(const GE3DPoint*)b, i);
        return 0;
    case EeGeBmpOp::VertexSetNormal:
        theVertices->SetNormal(*(const GEVector*)b, i);
        return 0;
    case EeGeBmpOp::VertexGetOpacity:
        return GEBitmapFloatBits(theVertices->GetOpacity(i));
    case EeGeBmpOp::VertexSetOpacity:
        theVertices->SetOpacity(GEBitmapBitsFloat(b), i);
        return 0;
    case EeGeBmpOp::VertexGetUv:
        theVertices->GetTextureCoordinate(*(GETextureCoordinate*)b, i);
        return 0;
    case EeGeBmpOp::VertexSetUv:
        theVertices->SetTextureCoordinate(*(const GETextureCoordinate*)b, i);
        return 0;
    case EeGeBmpOp::TexReference:
        theTexture->Reference();
        return 0;
    case EeGeBmpOp::TexDereference:
        theTexture->Dereference();
        return 0;
    case EeGeBmpOp::TexFrameWidth:
        return GEBitmapFloatBits(theTexture->GetFrameWidth());
    case EeGeBmpOp::TexFrameHeight:
        return GEBitmapFloatBits(theTexture->GetFrameHeight());
    case EeGeBmpOp::TexWidth:
        return (intptr_t)theTexture->GetWidth();
    case EeGeBmpOp::TexHeight:
        return (intptr_t)theTexture->GetHeight();
    case EeGeBmpOp::TexDropLevels:
        return (intptr_t)theTexture->GetNumMipLevelsToDrop();
    case EeGeBmpOp::VertexCopyFrom:
        theVertices->CopyFrom(*(const GEVertexArray*)b);
        return 0;
    }
    (void)inOwner;
    return 0;
}
// ---- end of the vertex host

// FUNCTION: LLE 0x1000f3a4
// Returns the colours of the bitmap's four corners in vertex order (top left, top right, bottom left, bottom right)
void GEBitmap::GetColorPerVertex(GEColor& outTopLeft, GEColor& outTopRight, GEColor& outBottomRight,
                                 GEColor& outBottomLeft) const
{
    const EeGeBmpQuad theOut = {&outTopLeft, &outTopRight, &outBottomRight, &outBottomLeft};
    ee_ge_bmp_colors(this->mRust, &theOut);
}

// FUNCTION: LLE 0x1000f4a4
// Returns the opacities of the bitmap's four corners in vertex order
void GEBitmap::GetOpacityPerVertex(float& outTopLeft, float& outTopRight, float& outBottomRight,
                                   float& outBottomLeft) const
{
    const EeGeBmpQuad theOut = {&outTopLeft, &outTopRight, &outBottomRight, &outBottomLeft};
    ee_ge_bmp_opacities(this->mRust, &theOut);
}

// FUNCTION: LLE 0x1000f4f8
// Returns the texture coordinates of the bitmap's four corners in vertex order
void GEBitmap::GetTextureCoordinatesPerVertex(GETextureCoordinate& outTopLeft, GETextureCoordinate& outTopRight,
                                              GETextureCoordinate& outBottomRight,
                                              GETextureCoordinate& outBottomLeft) const
{
    const EeGeBmpQuad theOut = {&outTopLeft, &outTopRight, &outBottomRight, &outBottomLeft};
    ee_ge_bmp_uvs(this->mRust, &theOut);
}

// FUNCTION: LLE 0x1000f5e4
// Gives all four corners of the bitmap the same colour
void GEBitmap::SetColor(const GEColor& inColor)
{
    this->SetColorPerVertex(inColor, inColor, inColor, inColor);
}

// FUNCTION: LLE 0x1000f61d
// Sets the colours of the bitmap's four corners in vertex order
void GEBitmap::SetColorPerVertex(const GEColor& inTopLeft, const GEColor& inTopRight, const GEColor& inBottomRight,
                                 const GEColor& inBottomLeft)
{
    const EeGeBmpQuad theColors = {(void*)&inTopLeft, (void*)&inTopRight, (void*)&inBottomRight, (void*)&inBottomLeft};
    ee_ge_bmp_set_colors(this->mRust, &theColors);
}

// FUNCTION: LLE 0x1000f673
// Gives all four corners of the bitmap the same opacity
void GEBitmap::SetOpacity(float inOpacity)
{
    this->SetOpacityPerVertex(inOpacity, inOpacity, inOpacity, inOpacity);
}

// FUNCTION: LLE 0x1000f6c2
// Sets the opacities of the bitmap's four corners in vertex order
void GEBitmap::SetOpacityPerVertex(float inTopLeft, float inTopRight, float inBottomRight, float inBottomLeft)
{
    const float theOpacities[4] = {inTopLeft, inTopRight, inBottomRight, inBottomLeft};
    ee_ge_bmp_set_opacities(this->mRust, &theOpacities);
}

// FUNCTION: LLE 0x1000f720
// Sets one scale for both axes of the bitmap and recomputes its corner positions
void GEBitmap::SetScale(float inScale)
{
    this->SetXYScale(inScale, inScale);
}

// FUNCTION: LLE 0x1000f732
// Sets the texture coordinates of the bitmap's four corners in vertex order
void GEBitmap::SetTextureCoordinates(GETextureCoordinate& inTopLeft, GETextureCoordinate& inTopRight,
                                     GETextureCoordinate& inBottomRight, GETextureCoordinate& inBottomLeft)
{
    const EeGeBmpQuad theCoordinates = {&inTopLeft, &inTopRight, &inBottomRight, &inBottomLeft};
    ee_ge_bmp_set_uvs(this->mRust, &theCoordinates);
}

// FUNCTION: LLE 0x1000f787
// Gives all four corners of the bitmap the same depth (Z)
void GEBitmap::SetZ(float inZ)
{
    this->SetZPerVertex(inZ, inZ, inZ, inZ);
}

// FUNCTION: LLE 0x1000f7af
// Sets the depths (Z) of the bitmap's four corners in vertex order, keeping their X and Y
void GEBitmap::SetZPerVertex(float inTopLeft, float inTopRight, float inBottomRight, float inBottomLeft)
{
    const float theDepths[4] = {inTopLeft, inTopRight, inBottomRight, inBottomLeft};
    ee_ge_bmp_set_depths(this->mRust, &theDepths);
}

// FUNCTION: LLE 0x1000f97f
// Creates an untextured, unscaled, unrotated white bitmap: one texture slot per screen and a four-vertex quad
GEBitmap::GEBitmap()
{
    this->mRust = ee_ge_bmp_new(&GEVertexHost, this);
}

// FUNCTION: LLE 0x1000fb74
// Destroys the bitmap, deleting its vertex array and releasing the texture of every screen
GEBitmap::~GEBitmap()
{
    ee_ge_bmp_destroy(this->mRust);
}

// FUNCTION: LLE 0x1000fda1
// Maps a source rectangle of the screen's texture onto texture coordinates and sets the matching X/Y scale
void GEBitmap::ComputeStretchedCoordsAndScale(const GERectangle& inTextureRectangle,
                                              const GERectangle& inSourceRectangle,
                                              const GERectangle& inDestinationRectangle, unsigned long inScreen)
{
    ee_ge_bmp_stretch(this->mRust, GERust(inTextureRectangle), GERust(inSourceRectangle),
                      GERust(inDestinationRectangle), inScreen);
}

// FUNCTION: LLE 0x1000ff33
// Mirrors the bitmap's texture left to right (resets the texture coordinates to the full flipped texture)
void GEBitmap::FlipHorizontally()
{
    ee_ge_bmp_flip(this->mRust, false);
}

// FUNCTION: LLE 0x10010009
// Sets the bitmap's texture for one screen, referencing the new one and releasing the old; takes its frame size
void GEBitmap::SetTexture(GETexture* inTexture, unsigned long inScreen)
{
    ee_ge_bmp_set_texture(this->mRust, inTexture, inScreen);
}

// U2DPoint/U2DRectangle key init chains (wrapper, ctor, atexit registration, dtor per key)
// SYNTHETIC: LLE 0x1000ec5c
// SYNTHETIC: LLE 0x1000ec66
// SYNTHETIC: LLE 0x1000ec76
// SYNTHETIC: LLE 0x1000ec82
// SYNTHETIC: LLE 0x1000ec8c
// SYNTHETIC: LLE 0x1000ec96
// SYNTHETIC: LLE 0x1000eca6
// SYNTHETIC: LLE 0x1000ecb2
// SYNTHETIC: LLE 0x1000ecbc
// SYNTHETIC: LLE 0x1000ecc6
// SYNTHETIC: LLE 0x1000ecd6
// SYNTHETIC: LLE 0x1000ece2
// SYNTHETIC: LLE 0x1000ecec
// SYNTHETIC: LLE 0x1000ecf6
// SYNTHETIC: LLE 0x1000ed06
// SYNTHETIC: LLE 0x1000ed12
// SYNTHETIC: LLE 0x1000ed1c
// SYNTHETIC: LLE 0x1000ed26
// SYNTHETIC: LLE 0x1000ed36
// SYNTHETIC: LLE 0x1000ed42
// SYNTHETIC: LLE 0x1000ed4c
// SYNTHETIC: LLE 0x1000ed56
// SYNTHETIC: LLE 0x1000ed66
// SYNTHETIC: LLE 0x1000ed72

// map<UString, GETexture*> (GETextureManager) and vector<GERScreenState*> (GERasterizer) COMDATs
// TEMPLATE: LLE 0x1001005c
// TEMPLATE: LLE 0x10010090
// TEMPLATE: LLE 0x100100d7
// TEMPLATE: LLE 0x10010148
// TEMPLATE: LLE 0x100101a8
// TEMPLATE: LLE 0x100101dd
// TEMPLATE: LLE 0x10010493
// TEMPLATE: LLE 0x10010516

// Header-inline/COMDAT members this unit emits (markers at the end)
// FUNCTION: LLE 0x1000de43
// FUNCTION: LLE 0x1000e637
// FUNCTION: LLE 0x1000e669
// FUNCTION: LLE 0x1000e745
// FUNCTION: LLE 0x1000e773
// FUNCTION: LLE 0x1000e75f
// FUNCTION: LLE 0x1000e769
