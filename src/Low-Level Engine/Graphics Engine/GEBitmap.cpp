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
#include "lle/UWideString.h"
#include "lle/GEMaterial.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "platform/PlatFont.h"
#include "platform/PlatWindow.h"
#include "GERustText.h"
#include "GERustMath.h"
#include <mbstring.h>
#include "lle/Graphics Engine.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// Advances the random generator one step and returns the new seed as a fraction in [0, 1)
void (*gURandomTraceHook)(URandom* inRandom) = NULL;   // port, TEST ONLY (lle/URandom.h)

float URandom::Generate()
{
    this->mSeed = (this->mMultiplier * this->mSeed + this->mIncrement) % this->mModulus;
    if (gURandomTraceHook != NULL)
        gURandomTraceHook(this);
    return (float)((double)this->mSeed / (double)this->mModulus);
}

// Advances the random generator one step and returns the new seed
unsigned long URandom::NextInSeries()
{
    this->mSeed = (this->mMultiplier * this->mSeed + this->mIncrement) % this->mModulus;
    return this->mSeed;
}

// ---- GERectangle members this unit emits (0x1000e39e..0x1000e78f)

//
//
//
//
//

// Creates a zero-size rectangle at the given point
GERectangle::GERectangle(const GE2DPoint& inPoint)
{
    this->mTop = inPoint.GetY();
    this->mLeft = inPoint.GetX();
    this->mBottom = inPoint.GetY();
    this->mRight = inPoint.GetX();
}

// Creates a rectangle from a Windows RECT
GERectangle::GERectangle(const tagRECT& inRect)
{
    this->mTop = (float)inRect.top;
    this->mLeft = (float)inRect.left;
    this->mBottom = (float)inRect.bottom;
    this->mRight = (float)inRect.right;
}

//
//
//
//
//

// Returns whether both rectangles have the same four edges
bool GERectangle::operator==(const GERectangle& inRectangle) const
{
    return this->mTop == inRectangle.mTop && this->mLeft == inRectangle.mLeft && this->mRight == inRectangle.mRight &&
           this->mBottom == inRectangle.mBottom;
}

// Returns whether the point lies inside the rectangle, edges included
bool GERectangle::Contains(const GE2DPoint& inPoint) const
{
    return inPoint.mX >= this->mLeft && inPoint.mX <= this->mRight && inPoint.mY >= this->mTop && inPoint.mY <= this->mBottom;
}

// Grows the rectangle just enough to contain the point
void GERectangle::Extend(const GE2DPoint& inPoint)
{
    this->mLeft = min(this->mLeft, inPoint.GetX());
    this->mRight = max(this->mRight, inPoint.GetX());
    this->mTop = min(this->mTop, inPoint.mY);
    this->mBottom = max(this->mBottom, inPoint.mY);
}

//
//
//

//
//
//

// Returns the rectangle's centre point in outCenter
void GERectangle::GetCenter(GE2DPoint& outCenter) const
{
    outCenter.mX = (this->mRight - this->mLeft) * 0.5f + this->mLeft;
    outCenter.mY = (this->mBottom - this->mTop) * 0.5f + this->mTop;
}

//
//
//

//
//
//

// Rounds each edge of the rectangle to the nearest whole number
void GERectangle::RoundToNearest()
{
    this->mTop = (float)UMFloor(this->mTop + 0.5f);
    this->mLeft = (float)UMFloor(this->mLeft + 0.5f);
    this->mRight = (float)UMFloor(this->mRight + 0.5f);
    this->mBottom = (float)UMFloor(this->mBottom + 0.5f);
}

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

// ---- the layouts Rust reads in place (ee_ge cppobj.rs, R4.3 part 17): a reference-counted object is its vtable
// pointer, its count and its Rust object; a text, a window and a primitive their Rust object alone; a UString and a
// UWideString their vtable pointer and ee_ustring's record
static_assert(sizeof(GEMaterial) == 3 * sizeof(void*) && sizeof(GEFont) == 3 * sizeof(void*) &&
              sizeof(GETexture) == 3 * sizeof(void*) &&
              sizeof(GEBitmap) == 3 * sizeof(void*) && sizeof(GEFontBitmap) == 3 * sizeof(void*), "cppobj.rs CppCounted");
static_assert(sizeof(GEText) == sizeof(void*) && sizeof(GEWindow) == sizeof(void*), "cppobj.rs plain");
static_assert(sizeof(UString) == sizeof(void*) + sizeof(EeUString) && sizeof(UWideString) == sizeof(void*) + sizeof(EeUWideString),
              "cppobj.rs CppUString");

// ---- the object host: the calls GEMaterial, GEFont, GEText and GEWindow (ee_ge smallobj.rs) make on other objects,
// and GEWindow's forwarders (its header does not include the Rust header)
intptr_t GEObjectHost(void* inOwner, EeGeObjOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    UString* theString = (UString*)inObj;
    UWideString* theWide = (UWideString*)inObj;
    switch (inOp) {
    case EeGeObjOp::StrNew:
        return (intptr_t)new UString;
    case EeGeObjOp::StrCopy:
        return (intptr_t)new UString(*(const UString*)a);
    case EeGeObjOp::StrDelete:
        delete theString;
        return 0;
    case EeGeObjOp::WStrNew:
        return (intptr_t)new UWideString((unsigned long)a);
    case EeGeObjOp::WStrDelete:
        delete theWide;
        return 0;
    case EeGeObjOp::TexDereference:
        ((GETexture*)inObj)->Dereference();
        return 0;
    case EeGeObjOp::RefDereference:
        ((UReferenceCounter*)inObj)->Dereference();
        return 0;
    case EeGeObjOp::PlatFontCreate:
        return (intptr_t)PlatFontCreate((const char*)inObj, (int32_t)a, (int)(b & 1), (int)((b >> 1) & 1),
                                        (int)((b >> 2) & 1), (int)((b >> 3) & 1));
    case EeGeObjOp::PlatFontDestroy:
        PlatFontDestroy((PlatFont*)inObj);
        return 0;
    case EeGeObjOp::PlatFontHeight: {
        PlatFontMetrics theMetrics;
        PlatFontGetMetrics((const PlatFont*)inObj, &theMetrics);
        return theMetrics.mHeight;
    }
    case EeGeObjOp::TextOps:
        return (intptr_t)GERustTextOps();
    case EeGeObjOp::PlatWindowDestroy:
        PlatWindowDestroy((PlatWindow*)inObj);
        return 0;
    case EeGeObjOp::PlatWindowSetSize:
        PlatWindowSetSize((PlatWindow*)inObj, (int32_t)a, (int32_t)b);
        return 0;
    case EeGeObjOp::PlatWindowPosition:
        PlatWindowGetPosition((PlatWindow*)inObj, (int32_t*)a, (int32_t*)b);
        return 0;
    }
    (void)inOwner;
    return 0;
}

GEWindow::GEWindow(PlatWindow* inWindow, unsigned long inWidth, unsigned long inHeight, unsigned long inDepth,
                   bool inIsFullScreen)
    : mRust(ee_ge_win_new(&GEObjectHost, this, inWindow, inWidth, inHeight, inDepth, inIsFullScreen)) {}
GEWindow::~GEWindow() { ee_ge_win_destroy(this->mRust); }
const GE2DPoint& GEWindow::GetClientOrigin() { return *reinterpret_cast<const GE2DPoint*>(ee_ge_win_origin(this->mRust)); }
unsigned long GEWindow::GetHeight() const { return ee_ge_win_size(this->mRust, true); }
PlatWindow* GEWindow::GetOSRepresentation() const { return static_cast<PlatWindow*>(ee_ge_win_os(this->mRust)); }
unsigned long GEWindow::GetWidth() const { return ee_ge_win_size(this->mRust, false); }
void GEWindow::Resize(unsigned long inWidth, unsigned long inHeight) { ee_ge_win_resize(this->mRust, inWidth, inHeight); }
// ---- end of the object host

// Returns the colours of the bitmap's four corners in vertex order (top left, top right, bottom left, bottom right)
void GEBitmap::GetColorPerVertex(GEColor& outTopLeft, GEColor& outTopRight, GEColor& outBottomRight,
                                 GEColor& outBottomLeft) const
{
    const EeGeBmpQuad theOut = {&outTopLeft, &outTopRight, &outBottomRight, &outBottomLeft};
    ee_ge_bmp_colors(this->mRust, &theOut);
}

// Returns the opacities of the bitmap's four corners in vertex order
void GEBitmap::GetOpacityPerVertex(float& outTopLeft, float& outTopRight, float& outBottomRight,
                                   float& outBottomLeft) const
{
    const EeGeBmpQuad theOut = {&outTopLeft, &outTopRight, &outBottomRight, &outBottomLeft};
    ee_ge_bmp_opacities(this->mRust, &theOut);
}

// Returns the texture coordinates of the bitmap's four corners in vertex order
void GEBitmap::GetTextureCoordinatesPerVertex(GETextureCoordinate& outTopLeft, GETextureCoordinate& outTopRight,
                                              GETextureCoordinate& outBottomRight,
                                              GETextureCoordinate& outBottomLeft) const
{
    const EeGeBmpQuad theOut = {&outTopLeft, &outTopRight, &outBottomRight, &outBottomLeft};
    ee_ge_bmp_uvs(this->mRust, &theOut);
}

// Gives all four corners of the bitmap the same colour
void GEBitmap::SetColor(const GEColor& inColor)
{
    this->SetColorPerVertex(inColor, inColor, inColor, inColor);
}

// Sets the colours of the bitmap's four corners in vertex order
void GEBitmap::SetColorPerVertex(const GEColor& inTopLeft, const GEColor& inTopRight, const GEColor& inBottomRight,
                                 const GEColor& inBottomLeft)
{
    const EeGeBmpQuad theColors = {(void*)&inTopLeft, (void*)&inTopRight, (void*)&inBottomRight, (void*)&inBottomLeft};
    ee_ge_bmp_set_colors(this->mRust, &theColors);
}

// Gives all four corners of the bitmap the same opacity
void GEBitmap::SetOpacity(float inOpacity)
{
    this->SetOpacityPerVertex(inOpacity, inOpacity, inOpacity, inOpacity);
}

// Sets the opacities of the bitmap's four corners in vertex order
void GEBitmap::SetOpacityPerVertex(float inTopLeft, float inTopRight, float inBottomRight, float inBottomLeft)
{
    const float theOpacities[4] = {inTopLeft, inTopRight, inBottomRight, inBottomLeft};
    ee_ge_bmp_set_opacities(this->mRust, &theOpacities);
}

// Sets one scale for both axes of the bitmap and recomputes its corner positions
void GEBitmap::SetScale(float inScale)
{
    this->SetXYScale(inScale, inScale);
}

// Sets the texture coordinates of the bitmap's four corners in vertex order
void GEBitmap::SetTextureCoordinates(GETextureCoordinate& inTopLeft, GETextureCoordinate& inTopRight,
                                     GETextureCoordinate& inBottomRight, GETextureCoordinate& inBottomLeft)
{
    const EeGeBmpQuad theCoordinates = {&inTopLeft, &inTopRight, &inBottomRight, &inBottomLeft};
    ee_ge_bmp_set_uvs(this->mRust, &theCoordinates);
}

// Gives all four corners of the bitmap the same depth (Z)
void GEBitmap::SetZ(float inZ)
{
    this->SetZPerVertex(inZ, inZ, inZ, inZ);
}

// Sets the depths (Z) of the bitmap's four corners in vertex order, keeping their X and Y
void GEBitmap::SetZPerVertex(float inTopLeft, float inTopRight, float inBottomRight, float inBottomLeft)
{
    const float theDepths[4] = {inTopLeft, inTopRight, inBottomRight, inBottomLeft};
    ee_ge_bmp_set_depths(this->mRust, &theDepths);
}

// Creates an untextured, unscaled, unrotated white bitmap: one texture slot per screen and a four-vertex quad
GEBitmap::GEBitmap()
{
    this->mRust = ee_ge_bmp_new(&GEVertexHost, this);
}

// Destroys the bitmap, deleting its vertex array and releasing the texture of every screen
GEBitmap::~GEBitmap()
{
    ee_ge_bmp_destroy(this->mRust);
}

// Maps a source rectangle of the screen's texture onto texture coordinates and sets the matching X/Y scale
void GEBitmap::ComputeStretchedCoordsAndScale(const GERectangle& inTextureRectangle,
                                              const GERectangle& inSourceRectangle,
                                              const GERectangle& inDestinationRectangle, unsigned long inScreen)
{
    ee_ge_bmp_stretch(this->mRust, GERust(inTextureRectangle), GERust(inSourceRectangle),
                      GERust(inDestinationRectangle), inScreen);
}

// Sets the bitmap's texture for one screen, referencing the new one and releasing the old; takes its frame size
void GEBitmap::SetTexture(GETexture* inTexture, unsigned long inScreen)
{
    ee_ge_bmp_set_texture(this->mRust, inTexture, inScreen);
}

// ---- GEFontBitmap (GEFontBitmap.cpp, folded here in R4.3 part 16)

// ---- GEFontBitmap.
//
// port (R4.3, tenth part): the font bitmap is a whole Rust object (ee_ge fbobj.rs): the constructor's glyph rendering,
// the destructor, CreateNewFontTexture, CreateSurfaceAlpha and the two draws. This unit is its shell: the forwarders
// and the host, which makes each call the original made on another object (the rasterizer, the font, the working
// text, the drawing bitmap, the textures, surfaces and screens). Floats cross the host as their bits.

static_assert(sizeof(EeGeSurfaceDescription) == sizeof(GESurfaceDescription), "ee_ge texobj.rs EeGeSurfaceDescription");

// (the working text's and the drawing bitmap's members and the two draws are the Rust object's own: fbobj.rs
// direct_op, on the 2D draw list)
intptr_t GEFontBitmap::Host(void* inOwner, EeGeFbOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    GEText* theText = (GEText*)inObj;
    GEBitmap* theBitmap = (GEBitmap*)inObj;
    GESurface* theSurface = (GESurface*)inObj;
    GERasterizer* theRasterizer = (GERasterizer*)inObj;
    GETexture* theTexture = (GETexture*)inObj;
    GEScreen* theScreen = (GEScreen*)inObj;
    unsigned long* theLongs = (unsigned long*)a;
    switch (inOp) {
    case EeGeFbOp::TextNew:
        return (intptr_t)new GEText((GEFont*)inObj);
    case EeGeFbOp::TextDelete:
        delete theText;
        return 0;
    case EeGeFbOp::TextSetByte: {
        // a cell is a Windows-1252 byte's character (decision D6: the text is UTF-8, so the cell holds that
        // character's UTF-8 and Draw maps a drawn character back to its cell)
        char theByte = (char)a;
        char theCell[8];
        PlatCp1252ToUtf8(&theByte, 1, theCell, sizeof(theCell));
        UWideString theString;
        theString = theCell;
        theText->SetText(theString);
        return 0;
    }
    case EeGeFbOp::TextOps:
        return (intptr_t)GERustTextOps();
    case EeGeFbOp::FontRange: {
        PlatFontMetrics theMetrics;   // the font's character range (TEXTMETRIC's tmFirstChar..tmLastChar)
        PlatFontGetMetrics(((GEFont*)inObj)->GetOSFont(), &theMetrics);
        theLongs[0] = theMetrics.mFirstChar;
        theLongs[1] = theMetrics.mLastChar;
        return 0;
    }
    case EeGeFbOp::FontSetBitmap:
        ((GEFont*)inObj)->SetFontBitmap((GEFontBitmap*)inOwner);
        return 0;
    case EeGeFbOp::RasterScreen:
        return (intptr_t)theRasterizer->GetScreen((unsigned long)a);
    case EeGeFbOp::RasterGetBlending:
        theRasterizer->GetBlending(theLongs[0], theLongs[1], (unsigned long)b);
        return 0;
    case EeGeFbOp::RasterSetBlending:
        theRasterizer->SetBlending(theLongs[0], theLongs[1], (unsigned long)b);
        return 0;
    case EeGeFbOp::BmpNew:
        return (intptr_t)new GEBitmap;
    case EeGeFbOp::BmpDereference:
        theBitmap->Dereference();
        return 0;
    case EeGeFbOp::TexSurface:
        return (intptr_t)theTexture->GetSurface();
    case EeGeFbOp::SurfTextUnlock:
        return theSurface->TextUnlock();
    case EeGeFbOp::SurfLock:
        return theSurface->Lock();
    case EeGeFbOp::SurfUnlock:
        return theSurface->Unlock();
    case EeGeFbOp::SurfPixels:
        theLongs[0] = theSurface->GetWidth();
        theLongs[1] = theSurface->GetHeight();
        theLongs[2] = theSurface->GetBytesPerRow();
        return (intptr_t)theSurface->GetMemoryAddress();
    case EeGeFbOp::SurfClear: {
        GEColor theBlack(0.0f, 0.0f, 0.0f);
        return theSurface->Clear(theBlack, 1.0f);
    }
    case EeGeFbOp::ScreenCreateSurface:
        return theScreen->CreateSurface(*(GESurfaceDescription*)a, (GESurface**)b);
    case EeGeFbOp::ScreenMakeTexture:
        return (intptr_t)theScreen->GetTextureManager()->MakeTexture((GESurface*)a);
    case EeGeFbOp::Verify:
        ESVerify(__FILE__, a == 0 ? 228 : 235, a == 0 ? "theResult == keseOK" : "theTexture != NULL");
        return 0;
    default:
        return 0;
    }
}

// Always returns true (any font matches)
bool GEFontBitmap::operator==(GEFont* inFont)
{
    return true;
}

// Renders every glyph of the font into 256x256 alpha textures and records each glyph's cell (Rust: ee_ge fbobj.rs)
GEFontBitmap::GEFontBitmap(GERasterizer* inRasterizer, GEFont* inFont, unsigned long inScreen)
{
    this->mRust = NULL;
    this->mRust = ee_ge_fb_new(&GEFontBitmap::Host, this, inRasterizer, inFont, inScreen);
}

// Destroys the glyph bitmap, releasing its drawing bitmap
GEFontBitmap::~GEFontBitmap()
{
    ee_ge_fb_destroy(this->mRust);
}

// Draws the text glyph by glyph at its location in its colour; returns the last draw's result (Rust: ee_ge fbobj.rs)
long GEFontBitmap::Draw(GEText* inText, GERasterizer* inRasterizer, unsigned long inScreen)
{
    return ee_ge_fb_draw(this->mRust, inText, NULL, inRasterizer, inScreen);
}

// Draws the text glyph by glyph clipped to a rectangle (Rust: ee_ge fbobj.rs)
long GEFontBitmap::Draw(GEText* inText, const GERectangle& inRectangle, GERasterizer* inRasterizer,
                        unsigned long inScreen)
{
    return ee_ge_fb_draw(this->mRust, inText, GERust(inRectangle), inRasterizer, inScreen);
}
