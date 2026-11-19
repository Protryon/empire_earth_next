// C:\EEX\Low-Level Engine\Graphics Engine\GEViewport.h (RECONSTRUCTION). The projections, the clipping planes and
// the frustum are computed by Rust on the object itself (ee_render geval viewport.rs; lle/GERust.h).
#ifndef LLE_GEVIEWPORT_H
#define LLE_GEVIEWPORT_H

#include <vector>
#include "lle/GERectangle.h"   // by-value members

class GEViewport {                          // sizeof 0x288 (implicit copy ctor 0xe992)
public:
    // identity camera, 90-degree field of view, clipping from 1 to 10
    GEViewport() { ee_ge_viewport_default(GERust(*this)); }
    // a screen rectangle and a camera, with their projections and clipping planes
    GEViewport(const GERectangle& inViewRectangle, const GERectangle& inOrthographicRectangle,
               const GETransformation& inCamera, float inFrontClippingDistance, float inRearClippingDistance,
               float inFieldOfView)
    {
        ee_ge_viewport_init(GERust(*this), GERust(inViewRectangle), GERust(inOrthographicRectangle), GERust(inCamera),
                            inFrontClippingDistance, inRearClippingDistance, inFieldOfView);
    }
    ~GEViewport() {}
    GEViewport& operator=(const GEViewport& inViewport) { ee_ge_viewport_copy(GERust(*this), GERust(inViewport)); return *this; }

    void DenormalizeOrthographicPoint(const GE3DPoint& inPoint, GE3DPoint& outPoint) const
        { this->mOrthographicDenormalization.Apply(inPoint, outPoint); }
    void DenormalizePerspectivePoint(const GE3DPoint& inPoint, float inW, GE3DPoint& outPoint) const
    {
        GE3DPoint thePoint(inPoint.GetX() * inW, inPoint.GetY() * inW, inPoint.GetZ() * inW);
        this->mPerspectiveDenormalization.Apply(thePoint, outPoint);
    }
    const GETransformation& GetCamera() const { return this->mCamera; }
    const GETransformation& GetCameraInverse() const { return this->mCameraInverse; }
    float GetFrontClippingDistance() const { return this->mFrontClippingDistance; }
    const GE3DPlane& GetFrontPlane() const { return this->mFrontPlane; }
    void GetFrustrumWorldPlanes(std::vector<GE3DPlane>& outPlanes, bool inOrthographic) const
        { this->GetFrustrumWorldPlanes(this->mViewRectangle, outPlanes, inOrthographic); }
    // the six world-space planes of the frustum under a screen rectangle
    void GetFrustrumWorldPlanes(const GERectangle& inRectangle, std::vector<GE3DPlane>& outPlanes,
                                bool inOrthographic) const
    {
        outPlanes.resize(6);
        ee_ge_viewport_frustum_planes(GERust(*this), GERust(inRectangle), inOrthographic, GERust(outPlanes[0]));
    }
    void GetFrustrumWorldPoints(std::vector<GE3DPoint>& outPoints, bool inOrthographic) const
        { this->GetFrustrumWorldPoints(this->mViewRectangle, outPoints, inOrthographic); }
    // appends the eight world-space corners (near and far per screen corner) of the frustum under a screen
    // rectangle, widened to at least 2 pixels each way
    void GetFrustrumWorldPoints(const GERectangle& inRectangle, std::vector<GE3DPoint>& outPoints,
                                bool inOrthographic) const
    {
        GE3DPoint thePoints[8];
        ee_ge_viewport_frustum_points(GERust(*this), GERust(inRectangle), inOrthographic, GERust(thePoints[0]));
        outPoints.insert(outPoints.end(), thePoints, thePoints + 8);
    }
    float GetHalfHeight() const { return this->mHalfHeight; }
    float GetHalfWidth() const { return this->mHalfWidth; }
    const GE3DPlane& GetLeftPlane() const { return this->mLeftPlane; }
    const GERectangle& GetOrthographicRectangle() const { return this->mOrthographicRectangle; }
    float GetRearClippingDistance() const { return this->mRearClippingDistance; }
    const GE3DPlane& GetRightPlane() const { return this->mRightPlane; }
    const GERectangle& GetViewRectangle() const { return this->mViewRectangle; }
    float GetXCenter() const { return this->mXCenter; }
    float GetYCenter() const { return this->mYCenter; }
    float GetYClippingPlane() const { return this->mYClippingPlane; }
    void NormalizeOrthographicCameraPoint(const GE3DPoint& inPoint, GE3DPoint& outPoint, float& outW)
    {
        outPoint.SetX(inPoint.GetX() * this->mOrthographicScale);
        outPoint.SetY(inPoint.GetZ() * this->mOrthographicScale);
        outPoint.SetZ(inPoint.GetY() * this->mOrthographicDepthScale + this->mOrthographicDepthOffset);
        outW = 1.0f;
    }
    void NormalizePerspectiveCameraPoint(const GE3DPoint& inPoint, GE3DPoint& outPoint, float& outW)
    {
        outPoint.SetX(inPoint.GetX() * this->mPerspectiveScale);
        outPoint.SetY(inPoint.GetZ() * this->mPerspectiveScale);
        outPoint.SetZ(inPoint.GetY() * this->mPerspectiveDepthScale + this->mPerspectiveDepthOffset);
        outW = inPoint.GetY();
    }
    void SetCamera(const GETransformation& inCamera)
    {
        this->mCamera = inCamera;
        this->mCamera.Apply(GEVector(0.0f, 1.0f, 0.0f), this->mCameraLookAtVector);   // the local look-at vector
        this->mCamera.MakeInverse(this->mCameraInverse);
        this->ComputeOrthographicMatrix();
        this->ComputePerspectiveMatrix();
    }
    void SetDepthRange(float inNear, float inFar) { this->mNearDepth = inNear; this->mFarDepth = inFar; }
    // each recomputes what depends on it (the front distance pushes the rear one back if needed, the rear one pulls
    // the front one in)
    void SetFieldOfView(float inFieldOfView) { ee_ge_viewport_set_field_of_view(GERust(*this), inFieldOfView); }
    void SetFrontClippingDistance(float inDistance) { ee_ge_viewport_set_front(GERust(*this), inDistance); }
    void SetOrthographicRectangle(const GERectangle& inRectangle)
    {
        this->mOrthographicRectangle = inRectangle;
        this->ComputeOrthographicMatrix();
    }
    void SetRearClippingDistance(float inDistance) { ee_ge_viewport_set_rear(GERust(*this), inDistance); }
    void SetViewRectangle(const GERectangle& inRectangle)
        { ee_ge_viewport_set_view_rectangle(GERust(*this), GERust(inRectangle)); }
private:
    friend struct GERustViewportLayout;     // the Rust mirror's layout check (Graphics Engine/GERustMath.h)
    // skipped with no rear clipping distance
    void ComputeOrthographicMatrix() { ee_ge_viewport_orthographic_matrix(GERust(*this)); }
    void ComputePerspectiveMatrix() { ee_ge_viewport_perspective_matrix(GERust(*this)); }

    GERectangle mOrthographicRectangle;     // +0x000
    GERectangle mViewRectangle;             // +0x010
    GE3DPlane mFrontPlane;                  // +0x020
    GE3DPlane mRearPlane;                   // +0x038
    GE3DPlane mLeftPlane;                   // +0x050
    GE3DPlane mRightPlane;                  // +0x068
    GE3DPlane mTopPlane;                    // +0x080
    GE3DPlane mBottomPlane;                 // +0x098
    GETransformation mCamera;               // +0x0b0
    GETransformation mCameraInverse;        // +0x0f0
    GETransformation mOrthographicDenormalization;   // +0x130
    GETransformation mPerspectiveDenormalization;    // +0x170
    GETransformation mOrthographicProjection;        // +0x1b0
    GETransformation mPerspectiveProjection;         // +0x1f0
    GEVector mCameraLookAtVector;           // +0x230
    float mOrthographicScale;               // +0x23c provisional names from here
    float mOrthographicDepthScale;          // +0x240
    float mOrthographicDepthOffset;         // +0x244
    float mPerspectiveScale;                // +0x248
    float mPerspectiveDepthScale;           // +0x24c
    float mPerspectiveDepthOffset;          // +0x250
    float mAspectRatio;                     // +0x254 view width / height
    float mAspectRatioMultiplier;           // +0x258
    float mFieldOfView;                     // +0x25c
    float mFieldOfViewMultiplier;           // +0x260
    float mFrontClippingDistance;           // +0x264
    float mHalfHeight;                      // +0x268
    float mHalfWidth;                       // +0x26c
    float mRearClippingDistance;            // +0x270
    float mXCenter;                         // +0x274
    float mYCenter;                         // +0x278
    float mYClippingPlane;                  // +0x27c
    float mNearDepth;                       // +0x280
    float mFarDepth;                        // +0x284
};

#endif
