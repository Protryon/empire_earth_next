// C:\EEX\Low-Level Engine\Graphics Engine\GEViewport.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// 0x1003680c..0x10038020: sLocalLookAtVector's init chain (GEVector's empty dtor leaves a guard-only atexit stub),
// the out-of-line GEViewport members, then the vector<GE3DPlane> COMDATs GetFrustrumWorldPlanes pulls in (resize,
// insert, erase and their helpers). The accessors and the implicit copy ctor are header-inline (GEBitmap.cpp).
#include <windows.h>
#include "lle/LowLevelEngineCore.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GEViewport.h"

// Unit-local constant (0x100980f4, away from the shared __real pool), the smallest frustum extent (provisional name)
const float kMinimumFrustrumExtent = 2.0f;

GEVector GEViewport::sLocalLookAtVector(0.0f, 1.0f, 0.0f);

// FUNCTION: LLE 0x1003684c
// Creates a default viewport: identity camera, 90-degree field of view, clipping from 1 to 10
GEViewport::GEViewport()
{
    this->mCamera.Apply(GEViewport::sLocalLookAtVector, this->mCameraLookAtVector);
    this->mFrontClippingDistance = 1.0f;
    this->mRearClippingDistance = 10.0f;
    this->mFieldOfView = 1.5707964f;
    this->mOrthographicScale = 0.0f;
    this->mOrthographicDepthScale = 0.0f;
    this->mOrthographicDepthOffset = 0.0f;
    this->mPerspectiveScale = 0.0f;
    this->mPerspectiveDepthScale = 0.0f;
    this->mPerspectiveDepthOffset = 0.0f;
    this->mAspectRatio = 1.0f;
    this->mAspectRatioMultiplier = 1.0f;
    this->mFieldOfViewMultiplier = 1.0f;
    this->mHalfWidth = 0.0f;
    this->mHalfHeight = 0.0f;
    this->mXCenter = 0.0f;
    this->mYCenter = 0.0f;
    this->mYClippingPlane = 1.0f;
    this->mNearDepth = 0.0f;
    this->mFarDepth = 1.0f;
}

// FUNCTION: LLE 0x100369b3
// Creates a viewport for a screen rectangle and camera, computing its projections and clipping planes
GEViewport::GEViewport(const GERectangle& inViewRectangle, const GERectangle& inOrthographicRectangle,
                       const GETransformation& inCamera, float inFrontClippingDistance,
                       float inRearClippingDistance, float inFieldOfView)
    : mViewRectangle(inViewRectangle), mCamera(inCamera)
{
    inCamera.Apply(GEViewport::sLocalLookAtVector, this->mCameraLookAtVector);
    this->mOrthographicRectangle = inOrthographicRectangle;
    this->mFrontClippingDistance = inFrontClippingDistance;
    this->mRearClippingDistance = inRearClippingDistance;
    this->mFieldOfView = inFieldOfView;
    this->mFieldOfViewMultiplier = UMTan(inFieldOfView * 0.5f);
    this->mHalfWidth = (inViewRectangle.GetRight() - inViewRectangle.GetLeft()) * 0.5f;
    this->mHalfHeight = (inViewRectangle.GetBottom() - inViewRectangle.GetTop()) * 0.5f;
    this->mXCenter = (inViewRectangle.GetRight() + inViewRectangle.GetLeft()) * 0.5f;
    this->mYCenter = (inViewRectangle.GetBottom() + inViewRectangle.GetTop()) * 0.5f;
    this->mYClippingPlane = this->mHalfHeight / this->mHalfWidth;
    if (inViewRectangle.GetTop() != inViewRectangle.GetBottom())
        this->mAspectRatio = (inViewRectangle.GetRight() - inViewRectangle.GetLeft()) /
            (inViewRectangle.GetBottom() - inViewRectangle.GetTop());
    else
        this->mAspectRatio = 1.0f;
    this->mAspectRatioMultiplier = this->mAspectRatio / this->mFieldOfViewMultiplier;
    this->mNearDepth = 0.0f;
    this->mFarDepth = 1.0f;
    this->ComputeOrthographicMatrix();
    this->ComputePerspectiveMatrix();
    this->ComputePlanes();
}

// FUNCTION: LLE 0x10036b86
// Copies every field of another viewport (rectangles, planes, camera, matrices, projection factors)
GEViewport& GEViewport::operator=(const GEViewport& inViewport)
{
    this->mOrthographicRectangle = inViewport.mOrthographicRectangle;
    this->mViewRectangle = inViewport.mViewRectangle;
    this->mFrontPlane = inViewport.mFrontPlane;
    this->mRearPlane = inViewport.mRearPlane;
    this->mBottomPlane = inViewport.mBottomPlane;
    this->mLeftPlane = inViewport.mLeftPlane;
    this->mRightPlane = inViewport.mRightPlane;
    this->mTopPlane = inViewport.mTopPlane;
    this->mCamera = inViewport.mCamera;
    this->mCameraInverse = inViewport.mCameraInverse;
    this->mOrthographicDenormalization = inViewport.mOrthographicDenormalization;
    this->mPerspectiveDenormalization = inViewport.mPerspectiveDenormalization;
    this->mOrthographicProjection = inViewport.mOrthographicProjection;
    this->mPerspectiveProjection = inViewport.mPerspectiveProjection;
    this->mCameraLookAtVector = inViewport.mCameraLookAtVector;
    this->mOrthographicScale = inViewport.mOrthographicScale;
    this->mOrthographicDepthScale = inViewport.mOrthographicDepthScale;
    this->mOrthographicDepthOffset = inViewport.mOrthographicDepthOffset;
    this->mPerspectiveScale = inViewport.mPerspectiveScale;
    this->mPerspectiveDepthScale = inViewport.mPerspectiveDepthScale;
    this->mPerspectiveDepthOffset = inViewport.mPerspectiveDepthOffset;
    this->mAspectRatio = inViewport.mAspectRatio;
    this->mAspectRatioMultiplier = inViewport.mAspectRatioMultiplier;
    this->mFieldOfView = inViewport.mFieldOfView;
    this->mFieldOfViewMultiplier = inViewport.mFieldOfViewMultiplier;
    this->mFrontClippingDistance = inViewport.mFrontClippingDistance;
    this->mHalfHeight = inViewport.mHalfHeight;
    this->mHalfWidth = inViewport.mHalfWidth;
    this->mRearClippingDistance = inViewport.mRearClippingDistance;
    this->mXCenter = inViewport.mXCenter;
    this->mYCenter = inViewport.mYCenter;
    this->mYClippingPlane = inViewport.mYClippingPlane;
    this->mNearDepth = inViewport.mNearDepth;
    this->mFarDepth = inViewport.mFarDepth;
    return *this;
}

// NEAR: the original keeps theDepth in a frame slot (fst/fld [ebp-4]) and the half width deeper on the FP stack;
// tried declaration order, a self-assignment and a separate copy local
// FUNCTION: LLE 0x10036d8b
// Recomputes the orthographic projection and its screen-to-world matrix (skipped with no rear clipping distance)
void GEViewport::ComputeOrthographicMatrix()
{
    float theDepth;
    float theDepthScale;
    float theHalfWidth;
    if (this->mRearClippingDistance == 0.0f)
        return;
    theDepth = this->mRearClippingDistance - this->mFrontClippingDistance;
    theDepthScale = 1.0f / theDepth;
    this->mOrthographicScale = (this->mOrthographicRectangle.GetRight() != this->mOrthographicRectangle.GetLeft()) ?
        2.0f / (this->mOrthographicRectangle.GetRight() - this->mOrthographicRectangle.GetLeft()) : 2.0f;
    this->mOrthographicDepthScale = theDepthScale;
    this->mOrthographicDepthOffset = -(theDepthScale * this->mFrontClippingDistance);
    theHalfWidth = (this->mOrthographicRectangle.GetRight() - this->mOrthographicRectangle.GetLeft()) * 0.5f;
    this->mOrthographicDenormalization.SetElement(0, 0, this->mCamera.GetElement(0, 0) * theHalfWidth);
    this->mOrthographicDenormalization.SetElement(0, 1, this->mCamera.GetElement(0, 2) * theHalfWidth);
    this->mOrthographicDenormalization.SetElement(0, 2, this->mCamera.GetElement(0, 1) * theDepth);
    this->mOrthographicDenormalization.SetElement(0, 3, this->mCamera.GetElement(0, 1) * this->mFrontClippingDistance +
        this->mCamera.GetElement(0, 3));
    this->mOrthographicDenormalization.SetElement(1, 0, this->mCamera.GetElement(1, 0) * theHalfWidth);
    this->mOrthographicDenormalization.SetElement(1, 1, this->mCamera.GetElement(1, 2) * theHalfWidth);
    this->mOrthographicDenormalization.SetElement(1, 2, this->mCamera.GetElement(1, 1) * theDepth);
    this->mOrthographicDenormalization.SetElement(1, 3, this->mCamera.GetElement(1, 1) * this->mFrontClippingDistance +
        this->mCamera.GetElement(1, 3));
    this->mOrthographicDenormalization.SetElement(2, 0, this->mCamera.GetElement(2, 0) * theHalfWidth);
    this->mOrthographicDenormalization.SetElement(2, 1, this->mCamera.GetElement(2, 2) * theHalfWidth);
    this->mOrthographicDenormalization.SetElement(2, 2, this->mCamera.GetElement(2, 1) * theDepth);
    this->mOrthographicDenormalization.SetElement(2, 3, this->mCamera.GetElement(2, 1) * this->mFrontClippingDistance +
        this->mCamera.GetElement(2, 3));
    this->mOrthographicProjection.SetElement(0, 0, this->mOrthographicScale);
    this->mOrthographicProjection.SetElement(0, 1, 0.0f);
    this->mOrthographicProjection.SetElement(0, 2, 0.0f);
    this->mOrthographicProjection.SetElement(0, 3, 0.0f);
    this->mOrthographicProjection.SetElement(1, 0, 0.0f);
    this->mOrthographicProjection.SetElement(1, 1, this->mOrthographicScale / this->mYClippingPlane);
    this->mOrthographicProjection.SetElement(1, 2, 0.0f);
    this->mOrthographicProjection.SetElement(1, 3, 0.0f);
    this->mOrthographicProjection.SetElement(2, 0, 0.0f);
    this->mOrthographicProjection.SetElement(2, 1, 0.0f);
    this->mOrthographicProjection.SetElement(2, 2, this->mOrthographicDepthScale);
    this->mOrthographicProjection.SetElement(2, 3, this->mOrthographicDepthOffset);
    this->mOrthographicProjection.SetElement(3, 0, 0.0f);
    this->mOrthographicProjection.SetElement(3, 1, 0.0f);
    this->mOrthographicProjection.SetElement(3, 2, 0.0f);
    this->mOrthographicProjection.SetElement(3, 3, 1.0f);
}

// FUNCTION: LLE 0x10036f6f
// Recomputes the perspective projection and its screen-to-world matrix (skipped with no rear clipping distance)
void GEViewport::ComputePerspectiveMatrix()
{
    float theDepth;
    float theScale;
    if (this->mRearClippingDistance == 0.0f)
        return;
    this->mPerspectiveScale = 1.0f / UMTan(this->mFieldOfView * 0.5f);
    theDepth = this->mRearClippingDistance - this->mFrontClippingDistance;
    this->mPerspectiveDepthScale = this->mRearClippingDistance / theDepth;
    this->mPerspectiveDepthOffset = -(this->mPerspectiveDepthScale * this->mFrontClippingDistance);
    theScale = 1.0f / this->mPerspectiveScale;
    theDepth = theDepth / this->mRearClippingDistance;
    this->mPerspectiveDenormalization.SetElement(0, 0, this->mCamera.GetElement(0, 0) * theScale);
    this->mPerspectiveDenormalization.SetElement(0, 1, this->mCamera.GetElement(0, 2) * theScale);
    this->mPerspectiveDenormalization.SetElement(0, 2, this->mCamera.GetElement(0, 1) * theDepth);
    this->mPerspectiveDenormalization.SetElement(0, 3, this->mCamera.GetElement(0, 1) * this->mFrontClippingDistance +
        this->mCamera.GetElement(0, 3));
    this->mPerspectiveDenormalization.SetElement(1, 0, this->mCamera.GetElement(1, 0) * theScale);
    this->mPerspectiveDenormalization.SetElement(1, 1, this->mCamera.GetElement(1, 2) * theScale);
    this->mPerspectiveDenormalization.SetElement(1, 2, this->mCamera.GetElement(1, 1) * theDepth);
    this->mPerspectiveDenormalization.SetElement(1, 3, this->mCamera.GetElement(1, 1) * this->mFrontClippingDistance +
        this->mCamera.GetElement(1, 3));
    this->mPerspectiveDenormalization.SetElement(2, 0, this->mCamera.GetElement(2, 0) * theScale);
    this->mPerspectiveDenormalization.SetElement(2, 1, this->mCamera.GetElement(2, 2) * theScale);
    this->mPerspectiveDenormalization.SetElement(2, 2, this->mCamera.GetElement(2, 1) * theDepth);
    this->mPerspectiveDenormalization.SetElement(2, 3, this->mCamera.GetElement(2, 1) * this->mFrontClippingDistance +
        this->mCamera.GetElement(2, 3));
    this->mPerspectiveProjection.SetElement(0, 0, this->mPerspectiveScale);
    this->mPerspectiveProjection.SetElement(0, 1, 0.0f);
    this->mPerspectiveProjection.SetElement(0, 2, 0.0f);
    this->mPerspectiveProjection.SetElement(0, 3, 0.0f);
    this->mPerspectiveProjection.SetElement(1, 0, 0.0f);
    this->mPerspectiveProjection.SetElement(1, 1, this->mAspectRatioMultiplier);
    this->mPerspectiveProjection.SetElement(1, 2, 0.0f);
    this->mPerspectiveProjection.SetElement(1, 3, 0.0f);
    this->mPerspectiveProjection.SetElement(2, 0, 0.0f);
    this->mPerspectiveProjection.SetElement(2, 1, 0.0f);
    this->mPerspectiveProjection.SetElement(2, 2, this->mPerspectiveDepthScale);
    this->mPerspectiveProjection.SetElement(2, 3, this->mPerspectiveDepthOffset);
    this->mPerspectiveProjection.SetElement(3, 0, 0.0f);
    this->mPerspectiveProjection.SetElement(3, 1, 0.0f);
    this->mPerspectiveProjection.SetElement(3, 2, 1.0f);
    this->mPerspectiveProjection.SetElement(3, 3, 0.0f);
}

// The GetHalfHeight/GetHalfWidth accessors spend the inline budget that makes the right plane's Define call the
// GE3DPoint/GEVector operator= out of line (with plain member reads it copies inline)
// FUNCTION: LLE 0x10037144
// Recomputes the camera-space clipping planes (front, rear, left, right, bottom, top) from the field of view
void GEViewport::ComputePlanes()
{
    GE3DPoint thePoint;
    GEVector theNormal;
    float theAngle = this->mFieldOfView * 0.5f;
    float theHalfWidth;
    thePoint.SetY(this->mFrontClippingDistance);
    theNormal.SetY(1.0f);
    this->mFrontPlane.Define(thePoint, theNormal);
    thePoint.SetY(this->mRearClippingDistance);
    theNormal.SetY(-1.0f);
    this->mRearPlane.Define(thePoint, theNormal);
    thePoint.SetY(0.0f);
    theNormal.SetX(UMCos(theAngle));
    theNormal.SetY(UMSin(theAngle));
    theNormal.Normalize();
    this->mLeftPlane.Define(thePoint, theNormal);
    theNormal.SetX(-theNormal.GetX());
    this->mRightPlane.Define(thePoint, theNormal);
    theAngle = this->GetHalfHeight();
    theHalfWidth = this->GetHalfWidth();
    theAngle = UMArcTan(UMTan(this->mFieldOfView * 0.5f) * (theAngle - 1.0f) / (theHalfWidth - 1.0f));
    theNormal.SetX(0.0f);
    theNormal.SetY(UMSin(theAngle));
    theNormal.SetZ(UMCos(theAngle));
    theNormal.Normalize();
    this->mBottomPlane.Define(thePoint, theNormal);
    theNormal.SetZ(-theNormal.GetZ());
    this->mTopPlane.Define(thePoint, theNormal);
}

// NEAR: inline-budget split in the fifth Define (the original calls vector<GE3DPoint>::operator[](0) out of line)
// FUNCTION: LLE 0x100372ea
// Returns the six world-space planes of the frustum under a screen rectangle (perspective or orthographic)
void GEViewport::GetFrustrumWorldPlanes(const GERectangle& inRectangle, std::vector<GE3DPlane>& outPlanes,
                                        bool inOrthographic) const
{
    std::vector<GE3DPoint> thePoints;
    this->GetFrustrumWorldPoints(inRectangle, thePoints, inOrthographic);
    outPlanes.resize(6);
    outPlanes[0].Define(thePoints[4], thePoints[2], thePoints[0]);
    outPlanes[1].Define(thePoints[1], thePoints[3], thePoints[5]);
    outPlanes[2].Define(thePoints[0], thePoints[2], thePoints[3]);
    outPlanes[3].Define(thePoints[4], thePoints[5], thePoints[7]);
    outPlanes[4].Define(thePoints[0], thePoints[1], thePoints[4]);
    outPlanes[5].Define(thePoints[6], thePoints[7], thePoints[2]);
}

// NEAR: argument push scheduling in two of the corner blocks
// FUNCTION: LLE 0x100375a6
// Appends the eight world-space corners (near and far per screen corner) of the frustum under a screen
// rectangle, widened to at least 2 pixels each way
void GEViewport::GetFrustrumWorldPoints(const GERectangle& inRectangle, std::vector<GE3DPoint>& outPoints,
                                        bool inOrthographic) const
{
    GE3DPoint theWorldPoint;
    GERectangle theRectangle(inRectangle);
    float theScale = 1.0f / this->mHalfWidth;
    GE3DPoint thePoint;
    if (theRectangle.GetRight() - theRectangle.GetLeft() <= kMinimumFrustrumExtent)
        theRectangle.SetRight(theRectangle.GetLeft() + kMinimumFrustrumExtent);
    if (theRectangle.GetBottom() - theRectangle.GetTop() <= kMinimumFrustrumExtent)
        theRectangle.SetBottom(theRectangle.GetTop() + kMinimumFrustrumExtent);
    thePoint.SetX((theRectangle.GetLeft() - this->mXCenter) * theScale);
    thePoint.SetY(-((theRectangle.GetTop() - this->mYCenter) * theScale));
    thePoint.SetZ(0.0f);
    if (!inOrthographic) {
        this->DenormalizePerspectivePoint(thePoint, this->mFrontClippingDistance, theWorldPoint);
        outPoints.push_back(theWorldPoint);
        thePoint.SetZ(1.0f);
        this->DenormalizePerspectivePoint(thePoint, this->mRearClippingDistance, theWorldPoint);
        outPoints.push_back(theWorldPoint);
        thePoint.SetX((theRectangle.GetRight() - this->mXCenter) * theScale);
        thePoint.SetY(-((theRectangle.GetTop() - this->mYCenter) * theScale));
        thePoint.SetZ(0.0f);
        this->DenormalizePerspectivePoint(thePoint, this->mFrontClippingDistance, theWorldPoint);
        outPoints.push_back(theWorldPoint);
        thePoint.SetZ(1.0f);
        this->DenormalizePerspectivePoint(thePoint, this->mRearClippingDistance, theWorldPoint);
        outPoints.push_back(theWorldPoint);
        thePoint.SetX((theRectangle.GetLeft() - this->mXCenter) * theScale);
        thePoint.SetY(-((theRectangle.GetBottom() - this->mYCenter) * theScale));
        thePoint.SetZ(0.0f);
        this->DenormalizePerspectivePoint(thePoint, this->mFrontClippingDistance, theWorldPoint);
        outPoints.push_back(theWorldPoint);
        thePoint.SetZ(1.0f);
        this->DenormalizePerspectivePoint(thePoint, this->mRearClippingDistance, theWorldPoint);
        outPoints.push_back(theWorldPoint);
        thePoint.SetX((theRectangle.GetRight() - this->mXCenter) * theScale);
        thePoint.SetY(-((theRectangle.GetBottom() - this->mYCenter) * theScale));
        thePoint.SetZ(0.0f);
        this->DenormalizePerspectivePoint(thePoint, this->mFrontClippingDistance, theWorldPoint);
        outPoints.push_back(theWorldPoint);
        thePoint.SetZ(1.0f);
        this->DenormalizePerspectivePoint(thePoint, this->mRearClippingDistance, theWorldPoint);
        outPoints.push_back(theWorldPoint);
    } else {
        this->mOrthographicDenormalization.Apply(thePoint, theWorldPoint);
        outPoints.push_back(theWorldPoint);
        thePoint.SetZ(1.0f);
        this->mOrthographicDenormalization.Apply(thePoint, theWorldPoint);
        outPoints.push_back(theWorldPoint);
        thePoint.SetX((theRectangle.GetRight() - this->mXCenter) * theScale);
        thePoint.SetY(-((theRectangle.GetTop() - this->mYCenter) * theScale));
        thePoint.SetZ(0.0f);
        this->mOrthographicDenormalization.Apply(thePoint, theWorldPoint);
        outPoints.push_back(theWorldPoint);
        thePoint.SetZ(1.0f);
        this->mOrthographicDenormalization.Apply(thePoint, theWorldPoint);
        outPoints.push_back(theWorldPoint);
        thePoint.SetX((theRectangle.GetLeft() - this->mXCenter) * theScale);
        thePoint.SetY(-((theRectangle.GetBottom() - this->mYCenter) * theScale));
        thePoint.SetZ(0.0f);
        this->mOrthographicDenormalization.Apply(thePoint, theWorldPoint);
        outPoints.push_back(theWorldPoint);
        thePoint.SetZ(1.0f);
        this->mOrthographicDenormalization.Apply(thePoint, theWorldPoint);
        outPoints.push_back(theWorldPoint);
        thePoint.SetX((theRectangle.GetRight() - this->mXCenter) * theScale);
        thePoint.SetY(-((theRectangle.GetBottom() - this->mYCenter) * theScale));
        thePoint.SetZ(0.0f);
        this->mOrthographicDenormalization.Apply(thePoint, theWorldPoint);
        outPoints.push_back(theWorldPoint);
        thePoint.SetZ(1.0f);
        this->mOrthographicDenormalization.Apply(thePoint, theWorldPoint);
        outPoints.push_back(theWorldPoint);
    }
}

// FUNCTION: LLE 0x10037941
// Changes the field of view and recomputes the perspective and the clipping planes
void GEViewport::SetFieldOfView(float inFieldOfView)
{
    if (inFieldOfView == this->mFieldOfView)
        return;
    this->mFieldOfView = inFieldOfView;
    this->mFieldOfViewMultiplier = UMTan(inFieldOfView * 0.5f);
    this->mAspectRatioMultiplier = this->mAspectRatio / this->mFieldOfViewMultiplier;
    this->ComputePerspectiveMatrix();
    this->ComputePlanes();
}

// FUNCTION: LLE 0x1003799b
// Changes the front clipping distance (pushing the rear one back if needed) and recomputes everything
void GEViewport::SetFrontClippingDistance(float inDistance)
{
    if (inDistance >= this->mRearClippingDistance)
        this->mRearClippingDistance = inDistance + 1.0f;
    this->mFrontClippingDistance = inDistance;
    this->ComputeOrthographicMatrix();
    this->ComputePerspectiveMatrix();
    this->ComputePlanes();
}

// FUNCTION: LLE 0x100379dc
// Changes the rear clipping distance (pulling the front one in if needed) and recomputes everything
void GEViewport::SetRearClippingDistance(float inDistance)
{
    if (inDistance <= this->mFrontClippingDistance)
        this->mFrontClippingDistance = inDistance * 0.5f;
    this->mRearClippingDistance = inDistance;
    this->ComputeOrthographicMatrix();
    this->ComputePerspectiveMatrix();
    this->ComputePlanes();
}

// FUNCTION: LLE 0x10037a1d
// Changes the screen rectangle, its centre and aspect ratio and the clipping planes and
// the projection matrices
void GEViewport::SetViewRectangle(const GERectangle& inRectangle)
{
    if (this->mViewRectangle != inRectangle) {
        this->mViewRectangle = inRectangle;
        this->mHalfWidth = (inRectangle.GetRight() - inRectangle.GetLeft()) * 0.5f;
        this->mHalfHeight = (inRectangle.GetBottom() - inRectangle.GetTop()) * 0.5f;
        this->mXCenter = (inRectangle.GetRight() + inRectangle.GetLeft()) * 0.5f;
        this->mYCenter = (inRectangle.GetBottom() + inRectangle.GetTop()) * 0.5f;
        this->mYClippingPlane = this->mHalfHeight / this->mHalfWidth;
        if (inRectangle.GetTop() != inRectangle.GetBottom())
            this->mAspectRatio = (inRectangle.GetRight() - inRectangle.GetLeft()) /
                (inRectangle.GetBottom() - inRectangle.GetTop());
        else
            this->mAspectRatio = 1.0f;
        this->mAspectRatioMultiplier = this->mAspectRatio / this->mFieldOfViewMultiplier;
        // The projections depend on the aspect ratio and clipping plane: recompute them for the new size.
        this->ComputeOrthographicMatrix();
        this->ComputePerspectiveMatrix();
        this->ComputePlanes();
    }
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x1003680c
// SYNTHETIC: LLE 0x10036816
// SYNTHETIC: LLE 0x1003682f
// SYNTHETIC: LLE 0x1003683b
// TEMPLATE: LLE 0x10037b00
// TEMPLATE: LLE 0x10037b10
// TEMPLATE: LLE 0x10037b2c
// TEMPLATE: LLE 0x10037b3c
// TEMPLATE: LLE 0x10037b48
// TEMPLATE: LLE 0x10037bfc
// TEMPLATE: LLE 0x10037ed9
// TEMPLATE: LLE 0x10037f23
// TEMPLATE: LLE 0x10037f4a
// TEMPLATE: LLE 0x10037f61
// TEMPLATE: LLE 0x10037f89
// TEMPLATE: LLE 0x10037fa6
// TEMPLATE: LLE 0x10037fd2
// TEMPLATE: LLE 0x10037ff6
