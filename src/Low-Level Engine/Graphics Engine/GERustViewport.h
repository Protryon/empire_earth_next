// GEViewport's Rust mirror (rust/crates/ee_ge viewport.rs, docs/RUST_PLAN.md R4.3): the class stays the type callers
// use and its matrices, planes and frustum are computed by Rust on the object itself. The layout is pinned here.
// Include after lle/GEViewport.h and GERustMath.h.
#ifndef GERUSTVIEWPORT_H
#define GERUSTVIEWPORT_H

#include <stddef.h>

// the private members are reached by this friend (declared in GEViewport.h)
struct GERustViewportLayout {
    static_assert(sizeof(GEViewport) == sizeof(EeGeViewport), "GEViewport");
    static_assert(offsetof(GEViewport, mViewRectangle) == offsetof(EeGeViewport, view_rectangle) &&
                  offsetof(GEViewport, mFrontPlane) == offsetof(EeGeViewport, front_plane) &&
                  offsetof(GEViewport, mBottomPlane) == offsetof(EeGeViewport, bottom_plane) &&
                  offsetof(GEViewport, mCamera) == offsetof(EeGeViewport, camera) &&
                  offsetof(GEViewport, mCameraInverse) == offsetof(EeGeViewport, camera_inverse) &&
                  offsetof(GEViewport, mOrthographicDenormalization) == offsetof(EeGeViewport, orthographic_denormalization) &&
                  offsetof(GEViewport, mPerspectiveProjection) == offsetof(EeGeViewport, perspective_projection) &&
                  offsetof(GEViewport, mCameraLookAtVector) == offsetof(EeGeViewport, camera_look_at_vector) &&
                  offsetof(GEViewport, mOrthographicScale) == offsetof(EeGeViewport, orthographic_scale) &&
                  offsetof(GEViewport, mAspectRatio) == offsetof(EeGeViewport, aspect_ratio) &&
                  offsetof(GEViewport, mFrontClippingDistance) == offsetof(EeGeViewport, front_clipping_distance) &&
                  offsetof(GEViewport, mRearClippingDistance) == offsetof(EeGeViewport, rear_clipping_distance) &&
                  offsetof(GEViewport, mYClippingPlane) == offsetof(EeGeViewport, y_clipping_plane) &&
                  offsetof(GEViewport, mFarDepth) == offsetof(EeGeViewport, far_depth),
                  "GEViewport's members");
};

inline const EeGeViewport* GERust(const GEViewport& in) { return reinterpret_cast<const EeGeViewport*>(&in); }
inline EeGeViewport* GERust(GEViewport& in) { return reinterpret_cast<EeGeViewport*>(&in); }

#endif
