// C:\EEX\Low-Level Engine\Graphics Engine\GEViewport.h (RECONSTRUCTION). The projections, the clipping planes and
// the frustum are computed by Rust on the object itself (ee_render geval viewport.rs; lle/GERust.h).
#ifndef LLE_GEVIEWPORT_H
#define LLE_GEVIEWPORT_H

#include <vector>
#include "lle/GERectangle.h"   // by-value members

class GEViewport : public EeGeViewport {   // sizeof 0x288 (implicit copy ctor 0xe992); the record is Rust's
public:
    // identity camera, 90-degree field of view, clipping from 1 to 10
    GEViewport() { ee_ge_viewport_default(this); }
    // a screen rectangle and a camera, with their projections and clipping planes
    GEViewport(const GERectangle& inViewRectangle, const GERectangle& inOrthographicRectangle,
               const GETransformation& inCamera, float inFrontClippingDistance, float inRearClippingDistance,
               float inFieldOfView)
    {
        ee_ge_viewport_init(this, GERust(inViewRectangle), GERust(inOrthographicRectangle), GERust(inCamera),
                            inFrontClippingDistance, inRearClippingDistance, inFieldOfView);
    }
    ~GEViewport() {}
    GEViewport& operator=(const GEViewport& inViewport) { ee_ge_viewport_copy(this, &inViewport); return *this; }

    void DenormalizeOrthographicPoint(const GE3DPoint& inPoint, GE3DPoint& outPoint) const
        { GECpp(this->mOrthographicDenormalization).Apply(inPoint, outPoint); }
    void DenormalizePerspectivePoint(const GE3DPoint& inPoint, float inW, GE3DPoint& outPoint) const
        { ee_ge_viewport_denormalize_perspective(this, GERust(inPoint), inW, GERust(outPoint)); }
    const GETransformation& GetCamera() const { return GECpp(this->mCamera); }
    const GETransformation& GetCameraInverse() const { return GECpp(this->mCameraInverse); }
    float GetFrontClippingDistance() const { return this->mFrontClippingDistance; }
    void GetFrustrumWorldPlanes(std::vector<GE3DPlane>& outPlanes, bool inOrthographic) const
        { this->GetFrustrumWorldPlanes(this->GetViewRectangle(), outPlanes, inOrthographic); }
    // the six world-space planes of the frustum under a screen rectangle (the vector resized to them)
    void GetFrustrumWorldPlanes(const GERectangle& inRectangle, std::vector<GE3DPlane>& outPlanes,
                                bool inOrthographic) const
    {
        outPlanes.resize(6);
        ee_ge_viewport_frustum_planes(this, GERust(inRectangle), inOrthographic, GERust(outPlanes[0]));
    }
    float GetHalfHeight() const { return this->mHalfHeight; }
    float GetHalfWidth() const { return this->mHalfWidth; }
    const GERectangle& GetOrthographicRectangle() const { return GECpp(this->mOrthographicRectangle); }
    float GetRearClippingDistance() const { return this->mRearClippingDistance; }
    const GERectangle& GetViewRectangle() const { return GECpp(this->mViewRectangle); }
    float GetXCenter() const { return this->mXCenter; }
    float GetYCenter() const { return this->mYCenter; }
    float GetYClippingPlane() const { return this->mYClippingPlane; }
    // the camera-space point to the screen (x, z) and the depth range (y); w 1, or the point's y in perspective
    void NormalizeOrthographicCameraPoint(const GE3DPoint& inPoint, GE3DPoint& outPoint, float& outW)
        { ee_ge_viewport_normalize_camera_point(this, GERust(inPoint), false, GERust(outPoint), &outW); }
    void NormalizePerspectiveCameraPoint(const GE3DPoint& inPoint, GE3DPoint& outPoint, float& outW)
        { ee_ge_viewport_normalize_camera_point(this, GERust(inPoint), true, GERust(outPoint), &outW); }
    // the camera, its look-at vector and inverse, and the projections
    void SetCamera(const GETransformation& inCamera) { ee_ge_viewport_set_camera(this, GERust(inCamera)); }
    void SetOrthographicRectangle(const GERectangle& inRectangle) { ee_ge_viewport_set_orthographic_rectangle(this, GERust(inRectangle)); }
    void SetViewRectangle(const GERectangle& inRectangle) { ee_ge_viewport_set_view_rectangle(this, GERust(inRectangle)); }
};

#endif
