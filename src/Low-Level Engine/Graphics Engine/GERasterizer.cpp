// C:\EEX\Low-Level Engine\Graphics Engine\GERasterizer.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// (Unit notes follow GERasterizer::GERasterizer: its ESVERIFY is the original's line 41.)
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
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
#include "lle/GEBoundingBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/GEModel.h"
#include "lle/GEPhysicalModel.h"

std::vector<GERasterizer*> GERasterizer::sRasterizers;
std::vector<GERasterizer::TransformStack> GERasterizer::sTransformStack;
GERasterizer* GERasterizer::sActiveRasterizer = NULL;
// FUNCTION: LLE 0x10023807
// Creates a rasterizer with its drawing lock and a white-on-black Arial 9 debug text, not yet activated
GERasterizer::GERasterizer(const UString& inName) : mName(inName)
{
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mDrawingAccess) == keseOK, 41);
    this->mDebugFont = new GEFont("Arial Bold", 9, 0, kFontQualityAntialiased);
    this->mDebugText = new GEText(this->mDebugFont);
    this->mDebugText->SetAlignment(kTextAlignLeft);
    this->mDebugText->SetFgColor(GEColor(1.0f, 1.0f, 1.0f));
    this->mDebugText->SetBgColor(GEColor(0.0f, 0.0f, 0.0f));
    this->mDebugText->SetFont(this->mDebugFont);
    this->mDebugText->SetIsTransparent(false);
    this->mDebugText->SetLocation(0.0f, 0.0f);
    this->mIsActivated = false;
    this->mIsPolyAreaProfiling = false;
    this->mCulling = kCullCCW;
    this->mScreenID = 1;
    this->mWaterRipple0 = 0.0f;
    this->mWaterRipple1 = 0.0f;
    this->mWaterRipple2 = 0.0f;
}

// 0x10023655..0x100246a7: the U2D key init chains (U2DPoint.h, U2DRectangle.h), sRasterizers' and sTransformStack's
// init chains (dtors share one guard byte), the GERasterizer members with the inline COMDATs each pulls in right
// after it (GEText's scalar deleting dtor, GERScreenState's ctor/dtor, GERFogAttributes' ctor), then the
// vector<GERScreenState*>::push_back / vector<TransformStack>::_Buy COMDATs and the template-static init wrapper
// (three UList sNodePool $D calls kept in GEBoundingBox.cpp, plus this unit's atexit registrars). The accessors are
// header-inline (GEBitmap.cpp emits them).
// GERasterizer::TransformStack is defined in GERasterizer.h (its layout is proven by the DX7 plug-in's
// DrawModelHierarchy). The ESVERIFY is the original's line 41.

// The original calls GEText's scalar deleting dtor out of line (0x1002398a, ~GEText inlined into it): that needs
// ~GEText inline in GEText.h (its exported COMDAT 0x1001337b sits in GEBoundingBox.cpp's inline block)
// FUNCTION: LLE 0x10023950
// Destroys the rasterizer, releasing the debug font and text and its drawing lock
GERasterizer::~GERasterizer()
{
    this->mDebugFont->Dereference();
    delete this->mDebugText;
    TSScheduler::DestroyCriticalSection(this->mDrawingAccess);
}

// FUNCTION: LLE 0x100239b1
// Activates the rasterizer and makes it the active one on success (no active rasterizer on failure)
long GERasterizer::Activate()
{
    long theResult = this->ActivateRasterizer();
    this->mIsActivated = theResult == kErrorOK;
    GERasterizer::sActiveRasterizer = this->mIsActivated ? this : NULL;
    return theResult;
}

// FUNCTION: LLE 0x100239d2
// Adds a render state block for another screen; always succeeds
long GERasterizer::AddValidScreen(GEScreen* inScreen)
{
    this->mScreenStates.push_back(new GERScreenState(inScreen));
    return kErrorOK;
}

// FUNCTION: LLE 0x10023b18
// Deactivates the rasterizer: deletes every screen's render state and clears the active rasterizer
long GERasterizer::Deactivate()
{
    unsigned long i;
    for (i = 0; i < this->mScreenStates.size(); i++)
        delete this->mScreenStates[i];
    this->mScreenStates.clear();
    GERasterizer::sActiveRasterizer = NULL;
    this->mIsActivated = false;
    return kErrorOK;
}

// FUNCTION: LLE 0x10023bb2
// Returns a model's on-screen width in pixels (its scaled bounding diameter projected through the viewport)
float GERasterizer::GetPixelSize(GEModel* inModel, GEViewport* inViewport, GETransformation& inTransformation,
                                 float inScale, bool inOrthographic)
{
    GE3DPoint theCameraPoint;
    GE3DPoint thePoint;
    float theW;
    float theRadius;
    float theX;
    float theLeft;
    float theWidth;

    inTransformation.Apply(inModel->GetCenterPoint(), thePoint);
    inViewport->GetCameraInverse().Apply(thePoint, theCameraPoint);
    theRadius = inModel->GetBoundingRadius(0) * inScale;
    theX = theCameraPoint.GetX();
    theCameraPoint.SetX(theX - theRadius);
    if (!inOrthographic) {
        inViewport->NormalizePerspectiveCameraPoint(theCameraPoint, thePoint, theW);
        theLeft = (theW != 0.0f) ? thePoint.GetX() / theW : 0.0f;
        theCameraPoint.SetX(theX + theRadius);
        inViewport->NormalizePerspectiveCameraPoint(theCameraPoint, thePoint, theW);
        theWidth = (theW != 0.0f) ? thePoint.GetX() / theW - theLeft : 0.0f;
    } else {
        inViewport->NormalizeOrthographicCameraPoint(theCameraPoint, thePoint, theW);
        theLeft = thePoint.GetX();
        theCameraPoint.SetX(theX + theRadius);
        inViewport->NormalizeOrthographicCameraPoint(theCameraPoint, thePoint, theW);
        theWidth = thePoint.GetX() - theLeft;
    }
    if (theWidth >= 0.0f)
        theWidth *= inViewport->GetHalfWidth();
    else
        theWidth = inViewport->GetHalfWidth() * 2.0f;
    return theWidth;
}

// FUNCTION: LLE 0x10023cde
// Returns a placed model's on-screen width in pixels, from its location, frame and scale
float GERasterizer::GetPixelSize(const GEPhysicalModel& inModel, GEViewport& inViewport, bool inOrthographic)
{
    GE3DPoint theCameraPoint;
    GE3DPoint thePoint;
    float theW;
    float theScale = inModel.GetScale();
    unsigned long theFrame = inModel.GetFrame();
    GEModel* theModel = inModel.GetModel();
    float theRadius;
    float theX;
    float theLeft;
    float theWidth;
    GETransformation theTransformation(false);

    inModel.GetLocation().Apply(theModel->GetInternalTransform(theFrame), theTransformation);
    if (theScale != 1.0f) {
        theTransformation.Scale(theScale, theScale, theScale);
        theTransformation.SetX(inModel.GetLocation().GetX());
        theTransformation.SetY(inModel.GetLocation().GetY());
        theTransformation.SetZ(inModel.GetLocation().GetZ());
    }
    theTransformation.Apply(theModel->GetCenterPoint(), thePoint);
    inViewport.GetCameraInverse().Apply(thePoint, theCameraPoint);
    theRadius = theModel->GetBoundingRadius(theFrame) * theScale;
    theX = theCameraPoint.GetX();
    theCameraPoint.SetX(theX - theRadius);
    if (!inOrthographic) {
        inViewport.NormalizePerspectiveCameraPoint(theCameraPoint, thePoint, theW);
        theLeft = (theW != 0.0f) ? thePoint.GetX() / theW : 0.0f;
        theCameraPoint.SetX(theX + theRadius);
        inViewport.NormalizePerspectiveCameraPoint(theCameraPoint, thePoint, theW);
        theWidth = (theW != 0.0f) ? thePoint.GetX() / theW - theLeft : 0.0f;
    } else {
        inViewport.NormalizeOrthographicCameraPoint(theCameraPoint, thePoint, theW);
        theLeft = thePoint.GetX();
        theCameraPoint.SetX(theX + theRadius);
        inViewport.NormalizeOrthographicCameraPoint(theCameraPoint, thePoint, theW);
        theWidth = thePoint.GetX() - theLeft;
    }
    if (theWidth >= 0.0f)
        theWidth *= inViewport.GetHalfWidth();
    else
        theWidth = inViewport.GetHalfWidth() * 2.0f;
    return theWidth;
}

// FUNCTION: LLE 0x10023e6f
// Tests a model's bounding box against an orthographic view volume: sets visible/clipped and per-plane clip flags
void GERasterizer::IsOrthographicModelVisible(GEModel* inModel, GEViewport* inViewport,
                                              GETransformation& inTransformation, bool& outVisible, bool& outClipped)
{
    GEBoundingBox theBox;
    float theFront = inViewport->GetFrontClippingDistance();
    float theRear = inViewport->GetRearClippingDistance();
    GETransformation theTransformation(false);

    outVisible = true;
    outClipped = false;
    inViewport->GetCameraInverse().Apply(inTransformation, theTransformation);
    inModel->GetGraphicsBoundingBox().Transform(theTransformation, theBox);
    GERectangle theRectangle;
    theRectangle = inViewport->GetOrthographicRectangle();
    float theLeft = theRectangle.GetLeft();
    float theRight = theRectangle.GetRight();
    float theYClippingPlane = -inViewport->GetYClippingPlane();
    float theTop = theRectangle.GetTop() * theYClippingPlane;
    float theBottom = theRectangle.GetBottom() * theYClippingPlane;

    if (theBox.GetTopRightBack().GetY() >= theFront) {
        if (theBox.GetBottomLeftFront().GetY() >= theFront)
            inModel->SetClipToPlane(kClipPlaneFront, false);
        else {
            inModel->SetClipToPlane(kClipPlaneFront, true);
            outClipped = true;
        }
        if (theBox.GetBottomLeftFront().GetY() <= theRear) {
            if (theBox.GetTopRightBack().GetY() <= theRear)
                inModel->SetClipToPlane(kClipPlaneRear, false);
            else {
                inModel->SetClipToPlane(kClipPlaneRear, true);
                outClipped = true;
            }
            if (theBox.GetTopRightBack().GetX() >= theLeft) {
                if (theBox.GetBottomLeftFront().GetX() >= theLeft)
                    inModel->SetClipToPlane(kClipPlaneLeft, false);
                else {
                    inModel->SetClipToPlane(kClipPlaneLeft, true);
                    outClipped = true;
                }
                if (theBox.GetBottomLeftFront().GetX() <= theRight) {
                    if (theBox.GetTopRightBack().GetX() <= theRight)
                        inModel->SetClipToPlane(kClipPlaneRight, false);
                    else {
                        inModel->SetClipToPlane(kClipPlaneRight, true);
                        outClipped = true;
                    }
                    if (theBox.GetTopRightBack().GetZ() >= theBottom) {
                        if (theBox.GetBottomLeftFront().GetZ() >= theBottom)
                            inModel->SetClipToPlane(kClipPlaneBottom, false);
                        else {
                            inModel->SetClipToPlane(kClipPlaneBottom, true);
                            outClipped = true;
                        }
                        if (theBox.GetBottomLeftFront().GetZ() <= theTop) {
                            if (theBox.GetTopRightBack().GetZ() <= theTop)
                                inModel->SetClipToPlane(kClipPlaneTop, false);
                            else {
                                inModel->SetClipToPlane(kClipPlaneTop, true);
                                outClipped = true;
                            }
                        } else
                            outVisible = false;
                    } else
                        outVisible = false;
                } else
                    outVisible = false;
            } else
                outVisible = false;
        } else
            outVisible = false;
    } else
        outVisible = false;
}

// FUNCTION: LLE 0x10024007
// Tests a model's bounding sphere against a perspective view volume: sets visible/clipped and per-plane clip flags
void GERasterizer::IsPerspectiveModelVisible(GEModel* inModel, float inScale, GEViewport* inViewport,
                                             GETransformation& inTransformation, bool& outVisible, bool& outClipped)
{
    float theRadius;
    GE3DPoint theCenter;
    GE3DPoint thePoint;
    GEVector theLeftNormal = inViewport->GetLeftPlane().GetNormal();
    GEVector theRightNormal = inViewport->GetRightPlane().GetNormal();
    GEVector theTopNormal = inViewport->GetTopPlane().GetNormal();
    GEVector theBottomNormal = inViewport->GetBottomPlane().GetNormal();
    bool theClip;
    unsigned long theClipCount;

    outVisible = false;
    outClipped = false;
    inTransformation.Apply(inModel->GetCenterPoint(), thePoint);
    inViewport->GetCameraInverse().Apply(thePoint, theCenter);
    theRadius = inModel->GetGraphicsBoundingRadius() * inScale;
    theLeftNormal.Scale(theRadius, theLeftNormal);
    theRightNormal.Scale(theRadius, theRightNormal);
    theTopNormal.Scale(theRadius, theTopNormal);
    theBottomNormal.Scale(theRadius, theBottomNormal);
    if (!(theCenter.GetY() + theRadius >= inViewport->GetFrontClippingDistance()) ||
        !(theCenter.GetY() - theRadius <= inViewport->GetRearClippingDistance()))
        return;
    thePoint.SetX(theCenter.mX + theLeftNormal.GetX());
    thePoint.SetY(theCenter.mY + theLeftNormal.GetY());
    thePoint.SetZ(theCenter.mZ + theLeftNormal.GetZ());
    if (!inViewport->GetLeftPlane().IsPointOnOrAbovePlane(thePoint))
        return;
    thePoint.SetX(theCenter.mX + theRightNormal.GetX());
    thePoint.SetY(theCenter.mY + theRightNormal.GetY());
    thePoint.SetZ(theCenter.mZ + theRightNormal.GetZ());
    if (!inViewport->GetRightPlane().IsPointOnOrAbovePlane(thePoint))
        return;
    thePoint.SetX(theCenter.mX + theTopNormal.GetX());
    thePoint.SetY(theCenter.mY + theTopNormal.GetY());
    thePoint.SetZ(theCenter.mZ + theTopNormal.GetZ());
    if (!inViewport->GetTopPlane().IsPointOnOrAbovePlane(thePoint))
        return;
    thePoint.SetX(theCenter.mX + theBottomNormal.GetX());
    thePoint.SetY(theCenter.mY + theBottomNormal.GetY());
    thePoint.SetZ(theCenter.mZ + theBottomNormal.GetZ());
    if (!inViewport->GetBottomPlane().IsPointOnOrAbovePlane(thePoint))
        return;
    outVisible = true;
    theClip = theCenter.GetY() - theRadius < inViewport->GetFrontClippingDistance();
    inModel->SetClipToPlane(kClipPlaneFront, theClip);
    theClipCount = theClip;
    theClip = theCenter.GetY() + theRadius > inViewport->GetRearClippingDistance();
    inModel->SetClipToPlane(kClipPlaneRear, theClip);
    theClipCount += theClip;
    thePoint.SetX(theCenter.mX - theLeftNormal.GetX());
    thePoint.SetY(theCenter.mY - theLeftNormal.GetY());
    thePoint.SetZ(theCenter.mZ - theLeftNormal.GetZ());
    theClip = inViewport->GetLeftPlane().IsPointOnOrBelowPlane(thePoint);
    inModel->SetClipToPlane(kClipPlaneLeft, theClip);
    theClipCount += theClip;
    thePoint.SetX(theCenter.mX - theRightNormal.GetX());
    thePoint.SetY(theCenter.mY - theRightNormal.GetY());
    thePoint.SetZ(theCenter.mZ - theRightNormal.GetZ());
    theClip = inViewport->GetRightPlane().IsPointOnOrBelowPlane(thePoint);
    inModel->SetClipToPlane(kClipPlaneRight, theClip);
    theClipCount += theClip;
    thePoint.SetX(theCenter.mX - theTopNormal.GetX());
    thePoint.SetY(theCenter.mY - theTopNormal.GetY());
    thePoint.SetZ(theCenter.mZ - theTopNormal.GetZ());
    theClip = inViewport->GetTopPlane().IsPointOnOrBelowPlane(thePoint);
    inModel->SetClipToPlane(kClipPlaneTop, theClip);
    theClipCount += theClip;
    thePoint.SetX(theCenter.mX - theBottomNormal.GetX());
    thePoint.SetY(theCenter.mY - theBottomNormal.GetY());
    thePoint.SetZ(theCenter.mZ - theBottomNormal.GetZ());
    theClip = inViewport->GetBottomPlane().IsPointOnOrBelowPlane(thePoint);
    inModel->SetClipToPlane(kClipPlaneBottom, theClip);
    theClipCount += theClip;
    outClipped = theClipCount != 0;
}

// FUNCTION: LLE 0x10024352
// Sets a screen's alpha-testing state
void GERasterizer::SetAlphaTesting(unsigned long inState, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mAlphaTesting = inState;
}

// FUNCTION: LLE 0x10024369
// Sets a screen's alpha-test comparison function
void GERasterizer::SetAlphaTestingCompareState(unsigned long inState, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mAlphaTestingCompareState = inState;
}

// FUNCTION: LLE 0x10024380
// Sets a screen's alpha-test reference value
void GERasterizer::SetAlphaTestingRefValue(float inValue, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mAlphaTestingRefValue = inValue;
}

// FUNCTION: LLE 0x10024397
// Sets the ambient light colour and recomputes the effective ambient colour (the screen is ignored)
void GERasterizer::SetAmbientColor(const GEColor& inColor, unsigned long inScreen)
{
    this->mAmbientColor = inColor;
    this->mComputedAmbientColor.SetRed(inColor.GetRed() * this->mAmbientIntensity);
    this->mComputedAmbientColor.SetGreen(inColor.GetGreen() * this->mAmbientIntensity);
    this->mComputedAmbientColor.SetBlue(inColor.GetBlue() * this->mAmbientIntensity);
}

// FUNCTION: LLE 0x100243c9
// Sets the ambient light intensity and recomputes the effective ambient colour (the screen is ignored)
void GERasterizer::SetAmbientIntensity(float inIntensity, unsigned long inScreen)
{
    this->mAmbientIntensity = inIntensity;
    this->mComputedAmbientColor.SetRed(this->mAmbientColor.GetRed() * inIntensity);
    this->mComputedAmbientColor.SetGreen(this->mAmbientColor.GetGreen() * inIntensity);
    this->mComputedAmbientColor.SetBlue(this->mAmbientColor.GetBlue() * inIntensity);
}

// FUNCTION: LLE 0x100243ed
// Sets a screen's anti-aliasing state
void GERasterizer::SetAntiAliasing(unsigned long inState, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mAntiAliasing = inState;
}

// FUNCTION: LLE 0x10024404
// Sets a screen's source and destination blend factors
void GERasterizer::SetBlending(unsigned long inSource, unsigned long inDestination, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mBlendSource = inSource;
    this->mScreenStates[inScreen]->mBlendDestination = inDestination;
}

// FUNCTION: LLE 0x10024430
// Sets a screen's colour-keying state
void GERasterizer::SetColorKeying(unsigned long inState, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mColorKeying = inState;
}

// FUNCTION: LLE 0x10024447
// Sets the face-culling mode
void GERasterizer::SetCulling(unsigned long inState)
{
    this->mCulling = inState;
}

// FUNCTION: LLE 0x10024451
// Does nothing (no-texture display mode is not supported by the base rasterizer)
void GERasterizer::SetDisplayNoTextureMode(bool inState, unsigned long inScreen)
{
}

// SetDisplayOverdrawMode: does nothing either (the same empty body, ICF-folded into 0x10024451)
void GERasterizer::SetDisplayOverdrawMode(bool inState, unsigned long inScreen)
{
}

// FUNCTION: LLE 0x10024454
// Sets a screen's dithering state
void GERasterizer::SetDithering(unsigned long inState, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mDithering = inState;
}

// FUNCTION: LLE 0x1002446b
// Sets a screen's draw style
void GERasterizer::SetDrawStyle(unsigned long inStyle, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mDrawStyle = inStyle;
}

// FUNCTION: LLE 0x10024482
// Turns a screen's fog on or off
void GERasterizer::SetFog(unsigned long inState, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mFogState = inState;
}

// FUNCTION: LLE 0x10024499
// Sets a screen's fog colour
void GERasterizer::SetFogColor(const GEColor& inColor, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mFog.mColor = inColor;
}

// FUNCTION: LLE 0x100244bf
// Sets a screen's fog start and end distances and the derived 1/(end - start) scale
void GERasterizer::SetFogRange(float inStart, float inEnd, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mFog.mStart = inStart;
    this->mScreenStates[inScreen]->mFog.mEnd = inEnd;
    this->mScreenStates[inScreen]->mFog.mScale = 1.0f / (inEnd - inStart);
}

// FUNCTION: LLE 0x10024504
// Sets a screen's fog type
void GERasterizer::SetFogType(unsigned long inType, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mFog.mType = inType;
}

// FUNCTION: LLE 0x1002451b
// Sets a screen's fog density
void GERasterizer::SetFogValue(float inValue, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mFog.mDensity = inValue;
}

// FUNCTION: LLE 0x10024532
// Turns a screen's lighting on or off
void GERasterizer::SetLighting(unsigned long inState, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mLighting = inState;
}

// FUNCTION: LLE 0x10024549
// Sets a screen's projection mode
void GERasterizer::SetProjectionMode(unsigned long inMode, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mProjectionMode = inMode;
}

// FUNCTION: LLE 0x10024560
// Sets a screen's shading mode
void GERasterizer::SetShading(unsigned long inState, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mShading = inState;
}

// FUNCTION: LLE 0x10024577
// Sets a screen's shading colour source
void GERasterizer::SetShadingColorSource(unsigned long inSource, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mShadingColorSource = inSource;
}

// FUNCTION: LLE 0x1002458e
// Turns a screen's specular highlights on or off
void GERasterizer::SetSpecular(unsigned long inState, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mSpecular = inState;
}

// FUNCTION: LLE 0x100245a5
// Sets a screen's viewport
void GERasterizer::SetViewport(const GEViewport& inViewport, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mViewport = inViewport;
}

// FUNCTION: LLE 0x100245bb
// Sets whether a screen waits for vertical sync
void GERasterizer::SetWaitForVSync(bool inState, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mWaitForVSync = inState;
}

// FUNCTION: LLE 0x100245d2
// Sets a screen's z-buffer state
void GERasterizer::SetZBuffer(unsigned long inState, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mZBuffer = inState;
}

// FUNCTION: LLE 0x100245e9
// Sets a screen's z-buffer comparison function
void GERasterizer::SetZCompare(unsigned long inState, unsigned long inScreen)
{
    this->mScreenStates[inScreen]->mZCompare = inState;
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x10023655
// SYNTHETIC: LLE 0x1002365f
// SYNTHETIC: LLE 0x1002366f
// SYNTHETIC: LLE 0x1002367b
// SYNTHETIC: LLE 0x10023685
// SYNTHETIC: LLE 0x1002368f
// SYNTHETIC: LLE 0x1002369f
// SYNTHETIC: LLE 0x100236ab
// SYNTHETIC: LLE 0x100236b5
// SYNTHETIC: LLE 0x100236bf
// SYNTHETIC: LLE 0x100236cf
// SYNTHETIC: LLE 0x100236db
// SYNTHETIC: LLE 0x100236e5
// SYNTHETIC: LLE 0x100236ef
// SYNTHETIC: LLE 0x100236ff
// SYNTHETIC: LLE 0x1002370b
// SYNTHETIC: LLE 0x10023715
// SYNTHETIC: LLE 0x1002371f
// SYNTHETIC: LLE 0x1002372f
// SYNTHETIC: LLE 0x1002373b
// SYNTHETIC: LLE 0x10023745
// SYNTHETIC: LLE 0x1002374f
// SYNTHETIC: LLE 0x1002375f
// SYNTHETIC: LLE 0x1002376b
// SYNTHETIC: LLE 0x10023775
// SYNTHETIC: LLE 0x1002377f
// SYNTHETIC: LLE 0x10023797
// SYNTHETIC: LLE 0x100237a3
// SYNTHETIC: LLE 0x100237be
// SYNTHETIC: LLE 0x100237c8
// SYNTHETIC: LLE 0x100237e0
// SYNTHETIC: LLE 0x100237ec
// TEMPLATE: LLE 0x10023a0e
// TEMPLATE: LLE 0x10023aca
// TEMPLATE: LLE 0x10023b6a
// TEMPLATE: LLE 0x10024600
// TEMPLATE: LLE 0x1002462c
// SYNTHETIC: LLE 0x10024665
// SYNTHETIC: LLE 0x10024683
// SYNTHETIC: LLE 0x1002468f
// SYNTHETIC: LLE 0x1002469b
//
// 0x1002398a: GEText's scalar deleting dtor (see ~GERasterizer). A `delete p` compiles to a ??_G call whenever the
//   dtor body is inline-visible (even when the caller's budget is spent), and to ??1 + ??3 only when it is not;
//   UIForm.cpp has both shapes for the same field (~UIForm ??1+??3, RelayoutForm ??_G).
// 0x10024665..0x100246a7: the template-static init wrapper and three atexit registrars of
//   UList<GELODMetaVertex*>/<GELODPolygon*>/<GELODVertex*>::sNodePool (their $D ctors are GEBoundingBox.cpp's
//   0x10014ed4/0x10014efb/0x10014f22).

// Header-inline/COMDAT members this unit emits (markers at the end)
// FUNCTION: LLE 0x1002398a
