// C:\EEX\Low-Level Engine\Graphics Engine\GEBitmap.cpp (file name inferred: no __FILE__; alphabetically
// GE3DPoint < GEBitmap < GEBoundingBox, and GEBitmap's own code is the only non-inline code in the range)
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1000dd9e..0x10010550. The unit opens with the header-inline members of every class it is the first to include,
// in include order (URandom, GEColor, GESurface, GETextureManager, GETexture, GEWindow, GEScreen, GERectangle,
// GEViewport, GEVertexArray, the U2DPoint/U2DRectangle key statics, GERasterizer, GE2DVertex), then GEBitmap's own
// code (0x1000f3a4) and the map<UString, GETexture*> / vector<GERScreenState*> COMDATs.
#include <windows.h>

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
#include "GERustMath.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// header-inline members emitted by this unit: URandom(unsigned long) 0x1000dd9e, operator= 0x1000ddca (ICF pair
// with URandomIntegers'), Generate(float, float) 0x1000de1a, the default-ctor closure 0x1000de43
// FUNCTION: LLE 0x1000dd9e
// FUNCTION: LLE 0x1000ddca
// FUNCTION: LLE 0x1000de1a

// FUNCTION: LLE 0x1000dde9
// Advances the random generator one step and returns the new seed as a fraction in [0, 1)
float URandom::Generate()
{
    this->mSeed = (this->mMultiplier * this->mSeed + this->mIncrement) % this->mModulus;
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

// FUNCTION: LLE 0x1000f3a4
// Returns the colours of the bitmap's four corners in vertex order (top left, top right, bottom left, bottom right)
void GEBitmap::GetColorPerVertex(GEColor& outTopLeft, GEColor& outTopRight, GEColor& outBottomRight,
                                 GEColor& outBottomLeft) const
{
    this->mVertices->GetColor(outTopLeft, 0);
    this->mVertices->GetColor(outTopRight, 1);
    this->mVertices->GetColor(outBottomRight, 2);
    this->mVertices->GetColor(outBottomLeft, 3);
}

// FUNCTION: LLE 0x1000f3e3
// Returns the bitmap's drawn height (frame height times the vertical scale)
float GEBitmap::GetHeight() const
{
    return this->mYScale * this->mHeight;
}

// FUNCTION: LLE 0x1000f3ea
// Returns the screen positions of the bitmap's four corners in vertex order
void GEBitmap::GetLocationPerVertex(GE2DPoint& outTopLeft, GE2DPoint& outTopRight, GE2DPoint& outBottomRight,
                                    GE2DPoint& outBottomLeft) const
{
    GE3DPoint thePoint0;
    GE3DPoint thePoint1;
    GE3DPoint thePoint2;
    GE3DPoint thePoint3;
    this->mVertices->GetPoint(thePoint0, 0);
    this->mVertices->GetPoint(thePoint1, 1);
    this->mVertices->GetPoint(thePoint2, 2);
    this->mVertices->GetPoint(thePoint3, 3);
    outTopLeft.SetX(thePoint0.GetX());
    outTopLeft.SetY(thePoint0.GetY());
    outTopRight.SetX(thePoint1.GetX());
    outTopRight.SetY(thePoint1.GetY());
    outBottomRight.SetX(thePoint2.GetX());
    outBottomRight.SetY(thePoint2.GetY());
    outBottomLeft.SetX(thePoint3.GetX());
    outBottomLeft.SetY(thePoint3.GetY());
}

// FUNCTION: LLE 0x1000f4a4
// Returns the opacities of the bitmap's four corners in vertex order
void GEBitmap::GetOpacityPerVertex(float& outTopLeft, float& outTopRight, float& outBottomRight,
                                   float& outBottomLeft) const
{
    outTopLeft = this->mVertices->GetOpacity(0);
    outTopRight = this->mVertices->GetOpacity(1);
    outBottomRight = this->mVertices->GetOpacity(2);
    outBottomLeft = this->mVertices->GetOpacity(3);
}

// SYNTHETIC: LLE 0x1000f4eb
// GEBitmap::GetTexture: header-inline (lle/GEBitmap.h), emitted here

// FUNCTION: LLE 0x1000f4f8
// Returns the texture coordinates of the bitmap's four corners in vertex order
void GEBitmap::GetTextureCoordinatesPerVertex(GETextureCoordinate& outTopLeft, GETextureCoordinate& outTopRight,
                                              GETextureCoordinate& outBottomRight,
                                              GETextureCoordinate& outBottomLeft) const
{
    this->mVertices->GetTextureCoordinate(outTopLeft, 0);
    this->mVertices->GetTextureCoordinate(outTopRight, 1);
    this->mVertices->GetTextureCoordinate(outBottomRight, 2);
    this->mVertices->GetTextureCoordinate(outBottomLeft, 3);
}

// FUNCTION: LLE 0x1000f537
// Returns the bitmap's drawn width (frame width times the horizontal scale)
float GEBitmap::GetWidth() const
{
    return this->mXScale * this->mWidth;
}

// SYNTHETIC: LLE 0x1000f53e
// GEBitmap::GetYScale: header-inline (lle/GEBitmap.h), emitted here

// FUNCTION: LLE 0x1000f542
// Returns the depths (Z) of the bitmap's four corners in vertex order
void GEBitmap::GetZPerVertex(float& outTopLeft, float& outTopRight, float& outBottomRight,
                             float& outBottomLeft) const
{
    GE3DPoint thePoint0;
    GE3DPoint thePoint1;
    GE3DPoint thePoint2;
    GE3DPoint thePoint3;
    this->mVertices->GetPoint(thePoint0, 0);
    this->mVertices->GetPoint(thePoint1, 1);
    this->mVertices->GetPoint(thePoint2, 2);
    this->mVertices->GetPoint(thePoint3, 3);
    outTopLeft = thePoint0.GetZ();
    outTopRight = thePoint1.GetZ();
    outBottomRight = thePoint2.GetZ();
    outBottomLeft = thePoint3.GetZ();
}

// FUNCTION: LLE 0x1000f5e4
// Gives all four corners of the bitmap the same colour
void GEBitmap::SetColor(const GEColor& inColor)
{
    this->mVertices->SetColor(inColor, 0);
    this->mVertices->SetColor(inColor, 1);
    this->mVertices->SetColor(inColor, 2);
    this->mVertices->SetColor(inColor, 3);
}

// FUNCTION: LLE 0x1000f61d
// Sets the colours of the bitmap's four corners in vertex order
void GEBitmap::SetColorPerVertex(const GEColor& inTopLeft, const GEColor& inTopRight, const GEColor& inBottomRight,
                                 const GEColor& inBottomLeft)
{
    this->mVertices->SetColor(inTopLeft, 0);
    this->mVertices->SetColor(inTopRight, 1);
    this->mVertices->SetColor(inBottomRight, 2);
    this->mVertices->SetColor(inBottomLeft, 3);
}

// SYNTHETIC: LLE 0x1000f65c
// GEBitmap::SetLocation: header-inline (lle/GEBitmap.h), emitted here

// FUNCTION: LLE 0x1000f673
// Gives all four corners of the bitmap the same opacity
void GEBitmap::SetOpacity(float inOpacity)
{
    this->mVertices->SetOpacity(inOpacity, 0);
    this->mVertices->SetOpacity(inOpacity, 1);
    this->mVertices->SetOpacity(inOpacity, 2);
    this->mVertices->SetOpacity(inOpacity, 3);
}

// FUNCTION: LLE 0x1000f6c2
// Sets the opacities of the bitmap's four corners in vertex order
void GEBitmap::SetOpacityPerVertex(float inTopLeft, float inTopRight, float inBottomRight, float inBottomLeft)
{
    this->mVertices->SetOpacity(inTopLeft, 0);
    this->mVertices->SetOpacity(inTopRight, 1);
    this->mVertices->SetOpacity(inBottomRight, 2);
    this->mVertices->SetOpacity(inBottomLeft, 3);
}

// FUNCTION: LLE 0x1000f711
// Sets the bitmap's rotation angle and recomputes its corner positions
void GEBitmap::SetRotation(float inRotation)
{
    this->mRotation = inRotation;
    this->ComputeRotation();
}

// FUNCTION: LLE 0x1000f720
// Sets one scale for both axes of the bitmap and recomputes its corner positions
void GEBitmap::SetScale(float inScale)
{
    this->mXScale = inScale;
    this->mYScale = inScale;
    this->ComputeRotation();
}

// FUNCTION: LLE 0x1000f732
// Sets the texture coordinates of the bitmap's four corners in vertex order
void GEBitmap::SetTextureCoordinates(GETextureCoordinate& inTopLeft, GETextureCoordinate& inTopRight,
                                     GETextureCoordinate& inBottomRight, GETextureCoordinate& inBottomLeft)
{
    this->mVertices->SetTextureCoordinate(inTopLeft, 0);
    this->mVertices->SetTextureCoordinate(inTopRight, 1);
    this->mVertices->SetTextureCoordinate(inBottomRight, 2);
    this->mVertices->SetTextureCoordinate(inBottomLeft, 3);
}

// SYNTHETIC: LLE 0x1000f771
// GEBitmap::SetXYScale: header-inline (lle/GEBitmap.h), emitted here

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
    GE3DPoint thePoint0;
    GE3DPoint thePoint1;
    GE3DPoint thePoint2;
    GE3DPoint thePoint3;
    this->mVertices->GetPoint(thePoint0, 0);
    this->mVertices->GetPoint(thePoint1, 1);
    this->mVertices->GetPoint(thePoint2, 2);
    this->mVertices->GetPoint(thePoint3, 3);
    thePoint0.SetZ(inTopLeft);
    thePoint1.SetZ(inTopRight);
    thePoint2.SetZ(inBottomRight);
    thePoint3.SetZ(inBottomLeft);
    this->mVertices->SetPoint(thePoint0, 0);
    this->mVertices->SetPoint(thePoint1, 1);
    this->mVertices->SetPoint(thePoint2, 2);
    this->mVertices->SetPoint(thePoint3, 3);
}

// implicit copy ctor 0x1000f881, operator= 0x1000f8d8, vector deleting dtor 0x1000f923
// FUNCTION: LLE 0x1000f881
// FUNCTION: LLE 0x1000f8d8
// SYNTHETIC: LLE 0x1000f923

// FUNCTION: LLE 0x1000f97f
// Creates an untextured, unscaled, unrotated white bitmap: one texture slot per screen and a four-vertex quad
GEBitmap::GEBitmap()
{
    GEColor theColor;
    GE3DPoint thePoint;
    GEVector theNormal(0.0f, 0.0f, -1.0f);
    GERasterizer* theRasterizer = GERasterizer::GetActiveRasterizer();
    unsigned long i;
    this->mTextureCount = theRasterizer->GetScreenCount();
    this->mTextures = new GETexture*[this->mTextureCount];
    for (i = 0; i < this->mTextureCount; i++)
        this->mTextures[i] = NULL;
    this->mVertices = theRasterizer->CreateVertexArray(1, 4);
    this->mRotation = 0.0f;
    this->mXScale = 1.0f;
    this->mYScale = 1.0f;
    this->mHeight = 0.0f;
    this->mWidth = 0.0f;
    this->mVertices->SetPoint(thePoint, 0);
    this->mVertices->SetPoint(thePoint, 1);
    this->mVertices->SetPoint(thePoint, 2);
    this->mVertices->SetPoint(thePoint, 3);
    this->mVertices->SetNormal(theNormal, 0);
    this->mVertices->SetNormal(theNormal, 1);
    this->mVertices->SetNormal(theNormal, 2);
    this->mVertices->SetNormal(theNormal, 3);
    this->mVertices->SetColor(theColor, 0);
    this->mVertices->SetColor(theColor, 1);
    this->mVertices->SetColor(theColor, 2);
    this->mVertices->SetColor(theColor, 3);
    this->mVertices->SetOpacity(1.0f, 0);
    this->mVertices->SetOpacity(1.0f, 1);
    this->mVertices->SetOpacity(1.0f, 2);
    this->mVertices->SetOpacity(1.0f, 3);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(0.0f, 0.0f), 0);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(1.0f, 0.0f), 1);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(0.0f, 1.0f), 2);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(1.0f, 1.0f), 3);
    this->ComputeRotation();
}

// FUNCTION: LLE 0x1000fb74
// Destroys the bitmap, deleting its vertex array and releasing the texture of every screen
GEBitmap::~GEBitmap()
{
    unsigned long i;
    if (this->mVertices)
        delete this->mVertices;
    for (i = 0; i < this->mTextureCount; i++)
        if (this->mTextures[i])
            this->mTextures[i]->Dereference();
    delete[] this->mTextures;
}

// FUNCTION: LLE 0x1000fbb9
// Recomputes the four corner positions from location, frame size, scale and rotation (about the centre)
// (the corners are Rust: ee_ge bitmap.rs)
void GEBitmap::ComputeRotation()
{
    GE2DPoint theCorners[4];
    GE3DPoint thePoints[4];
    ee_ge_bitmap_corners(GERust(this->mLocation), this->mWidth, this->mHeight, this->mXScale, this->mYScale,
                         this->mRotation, GERust(theCorners[0]));
    for (int i = 0; i < 4; i++) {
        this->mVertices->GetPoint(thePoints[i], i);
        thePoints[i].SetX(theCorners[i].GetX());
        thePoints[i].SetY(theCorners[i].GetY());
        this->mVertices->SetPoint(thePoints[i], i);
    }
}

// 94.7: the original gives the dead parameter slots +0xc/+8 to theTextureYRange/theDestinationYRange (ours +8/+0x10)
// and schedules the "+ 1.0f" before the unsigned conversion; the (double) divisors give its `fild qword; fdivp`
// FUNCTION: LLE 0x1000fda1
// Maps a source rectangle of the screen's texture onto texture coordinates and sets the matching X/Y scale
// (the arithmetic is Rust: ee_ge bitmap.rs)
void GEBitmap::ComputeStretchedCoordsAndScale(const GERectangle& inTextureRectangle,
                                              const GERectangle& inSourceRectangle,
                                              const GERectangle& inDestinationRectangle, unsigned long inScreen)
{
    GETexture* theTexture = this->mTextures[inScreen];
    EeGeBitmapStretch theStretch;
    ee_ge_bitmap_stretch(GERust(inTextureRectangle), GERust(inSourceRectangle), GERust(inDestinationRectangle),
                         theTexture->GetWidth(), theTexture->GetHeight(), theTexture->GetNumMipLevelsToDrop(),
                         &theStretch);
    GETextureCoordinate theTopLeft(theStretch.uv[0][0], theStretch.uv[0][1]);
    GETextureCoordinate theTopRight(theStretch.uv[1][0], theStretch.uv[1][1]);
    GETextureCoordinate theBottomLeft(theStretch.uv[2][0], theStretch.uv[2][1]);
    GETextureCoordinate theBottomRight(theStretch.uv[3][0], theStretch.uv[3][1]);
    // (positionally as the original: the third argument is the bottom left corner)
    this->SetTextureCoordinates(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
    this->SetXYScale(theStretch.x_scale, theStretch.y_scale);
}

// FUNCTION: LLE 0x1000ff33
// Mirrors the bitmap's texture left to right (resets the texture coordinates to the full flipped texture)
void GEBitmap::FlipHorizontally()
{
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(1.0f, 0.0f), 0);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(0.0f, 0.0f), 1);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(1.0f, 1.0f), 2);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(0.0f, 1.0f), 3);
}

// FUNCTION: LLE 0x1000ff9e
// Mirrors the bitmap's texture top to bottom (resets the texture coordinates to the full flipped texture)
void GEBitmap::FlipVertically()
{
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(0.0f, 1.0f), 0);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(1.0f, 1.0f), 1);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(0.0f, 0.0f), 2);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(1.0f, 0.0f), 3);
}

// FUNCTION: LLE 0x10010009
// Sets the bitmap's texture for one screen, referencing the new one and releasing the old; takes its frame size
void GEBitmap::SetTexture(GETexture* inTexture, unsigned long inScreen)
{
    if (this->mTextures[inScreen] == inTexture)
        return;
    if (this->mTextures[inScreen])
        this->mTextures[inScreen]->Dereference();
    this->mTextures[inScreen] = inTexture;
    if (inTexture) {
        inTexture->Reference();
        this->mHeight = inTexture->GetFrameHeight();
        this->mWidth = inTexture->GetFrameWidth();
    }
    this->ComputeRotation();
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
