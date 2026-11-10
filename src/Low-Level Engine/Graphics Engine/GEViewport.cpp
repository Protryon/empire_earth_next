// C:\EEX\Low-Level Engine\Graphics Engine\GEViewport.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// 0x1003680c..0x10038020: sLocalLookAtVector's init chain (GEVector's empty dtor leaves a guard-only atexit stub),
// the out-of-line GEViewport members, then the vector<GE3DPlane> COMDATs GetFrustrumWorldPlanes pulls in (resize,
// insert, erase and their helpers). The accessors and the implicit copy ctor are header-inline (GEBitmap.cpp).
//
// The matrices, the clipping planes and the frustum are Rust (rust/crates/ee_ge viewport.rs, docs/RUST_PLAN.md
// R4.3), computed on the object itself through its mirror (GERustViewport.h pins the layout); the members forward.
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
#include "GERustMath.h"
#include "GERustViewport.h"

// (kMinimumFrustrumExtent 0x100980f4, the smallest frustum extent, is in Rust)

GEVector GEViewport::sLocalLookAtVector(0.0f, 1.0f, 0.0f);

// FUNCTION: LLE 0x1003684c
// Creates a default viewport: identity camera, 90-degree field of view, clipping from 1 to 10
GEViewport::GEViewport()
{
    ee_ge_viewport_default(GERust(*this));
}

// FUNCTION: LLE 0x100369b3
// Creates a viewport for a screen rectangle and camera, computing its projections and clipping planes
GEViewport::GEViewport(const GERectangle& inViewRectangle, const GERectangle& inOrthographicRectangle,
                       const GETransformation& inCamera, float inFrontClippingDistance,
                       float inRearClippingDistance, float inFieldOfView)
{
    ee_ge_viewport_init(GERust(*this), GERust(inViewRectangle), GERust(inOrthographicRectangle), GERust(inCamera),
                        inFrontClippingDistance, inRearClippingDistance, inFieldOfView);
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

// FUNCTION: LLE 0x10036d8b
// Recomputes the orthographic projection and its screen-to-world matrix (skipped with no rear clipping distance)
void GEViewport::ComputeOrthographicMatrix()
{
    ee_ge_viewport_orthographic_matrix(GERust(*this));
}

// FUNCTION: LLE 0x10036f6f
// Recomputes the perspective projection and its screen-to-world matrix (skipped with no rear clipping distance)
void GEViewport::ComputePerspectiveMatrix()
{
    ee_ge_viewport_perspective_matrix(GERust(*this));
}

// FUNCTION: LLE 0x10037144
// Recomputes the camera-space clipping planes (front, rear, left, right, bottom, top) from the field of view
void GEViewport::ComputePlanes()
{
    ee_ge_viewport_planes(GERust(*this));
}

// FUNCTION: LLE 0x100372ea
// Returns the six world-space planes of the frustum under a screen rectangle (perspective or orthographic)
void GEViewport::GetFrustrumWorldPlanes(const GERectangle& inRectangle, std::vector<GE3DPlane>& outPlanes,
                                        bool inOrthographic) const
{
    outPlanes.resize(6);
    ee_ge_viewport_frustum_planes(GERust(*this), GERust(inRectangle), inOrthographic, GERust(outPlanes[0]));
}

// FUNCTION: LLE 0x100375a6
// Appends the eight world-space corners (near and far per screen corner) of the frustum under a screen
// rectangle, widened to at least 2 pixels each way
void GEViewport::GetFrustrumWorldPoints(const GERectangle& inRectangle, std::vector<GE3DPoint>& outPoints,
                                        bool inOrthographic) const
{
    GE3DPoint thePoints[8];
    ee_ge_viewport_frustum_points(GERust(*this), GERust(inRectangle), inOrthographic, GERust(thePoints[0]));
    for (int k = 0; k < 8; k++)
        outPoints.push_back(thePoints[k]);
}

// FUNCTION: LLE 0x10037941
// Changes the field of view and recomputes the perspective and the clipping planes
void GEViewport::SetFieldOfView(float inFieldOfView)
{
    ee_ge_viewport_set_field_of_view(GERust(*this), inFieldOfView);
}

// FUNCTION: LLE 0x1003799b
// Changes the front clipping distance (pushing the rear one back if needed) and recomputes everything
void GEViewport::SetFrontClippingDistance(float inDistance)
{
    ee_ge_viewport_set_front(GERust(*this), inDistance);
}

// FUNCTION: LLE 0x100379dc
// Changes the rear clipping distance (pulling the front one in if needed) and recomputes everything
void GEViewport::SetRearClippingDistance(float inDistance)
{
    ee_ge_viewport_set_rear(GERust(*this), inDistance);
}

// FUNCTION: LLE 0x10037a1d
// Changes the screen rectangle, its centre and aspect ratio and the clipping planes and
// the projection matrices
void GEViewport::SetViewRectangle(const GERectangle& inRectangle)
{
    ee_ge_viewport_set_view_rectangle(GERust(*this), GERust(inRectangle));
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
