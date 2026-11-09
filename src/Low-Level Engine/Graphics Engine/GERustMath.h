// The Graphics Engine's math in Rust (rust/crates/ee_ge, docs/RUST_PLAN.md R4.3): the C++ value classes stay the
// types every caller uses, and their algorithms forward to ee_ge's C ABI with the objects themselves as arguments.
// Each class has a #[repr(C)] mirror (EeGe*, include/rust/ee_rust.h); the layouts are pinned here.
#ifndef GERUSTMATH_H
#define GERUSTMATH_H

#include <stddef.h>
#include <vector>

#include "rust/ee_rust.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GEQuaternion.h"
#include "lle/GEBoundingBox.h"
#include "lle/GERectangle.h"

// the classes' private members are reached by this friend (declared in GE3DLine.h, GE3DPlane.h, GEQuaternion.h,
// GEBoundingBox.h)
struct GERustLayout {
    static_assert(sizeof(GE2DPoint) == sizeof(EeGe2DPoint) && offsetof(GE2DPoint, mY) == offsetof(EeGe2DPoint, y), "GE2DPoint");
    static_assert(sizeof(GE3DPoint) == sizeof(EeGe3DPoint) && offsetof(GE3DPoint, mZ) == offsetof(EeGe3DPoint, z), "GE3DPoint");
    static_assert(sizeof(GEVector) == sizeof(EeGeVector) && offsetof(GEVector, mZ) == offsetof(EeGeVector, z), "GEVector");
    static_assert(sizeof(GE3DLine) == sizeof(EeGe3DLine) && offsetof(GE3DLine, mDirection) == offsetof(EeGe3DLine, direction), "GE3DLine");
    static_assert(sizeof(GE3DPlane) == sizeof(EeGe3DPlane) && offsetof(GE3DPlane, mNormal) == offsetof(EeGe3DPlane, normal), "GE3DPlane");
    static_assert(sizeof(GETransformation) == sizeof(EeGeTransformation) && offsetof(GETransformation, mMatrix) == 0, "GETransformation");
    static_assert(sizeof(GEOrientation) == sizeof(EeGeOrientation) && offsetof(GEOrientation, mRoll) == offsetof(EeGeOrientation, roll), "GEOrientation");
    static_assert(sizeof(GEQuaternion) == sizeof(EeGeQuaternion) && offsetof(GEQuaternion, mX) == offsetof(EeGeQuaternion, x) &&
                  offsetof(GEQuaternion, mW) == offsetof(EeGeQuaternion, w), "GEQuaternion");
    static_assert(sizeof(GEBoundingBox) == sizeof(EeGeBoundingBox) &&
                  offsetof(GEBoundingBox, mTopRightBack) == offsetof(EeGeBoundingBox, top_right_back) &&
                  offsetof(GEBoundingBox, mWidth) == offsetof(EeGeBoundingBox, width) &&
                  offsetof(GEBoundingBox, mHeight) == offsetof(EeGeBoundingBox, height) &&
                  offsetof(GEBoundingBox, mDepth) == offsetof(EeGeBoundingBox, depth), "GEBoundingBox");
    static_assert(sizeof(GERectangle) == sizeof(EeGeRectangle) && offsetof(GERectangle, mBottom) == offsetof(EeGeRectangle, bottom) &&
                  offsetof(GERectangle, mLeft) == offsetof(EeGeRectangle, left) && offsetof(GERectangle, mRight) == offsetof(EeGeRectangle, right) &&
                  offsetof(GERectangle, mTop) == offsetof(EeGeRectangle, top), "GERectangle");
};

// The C++ object as its mirror (the same storage).
inline const EeGe2DPoint* GERust(const GE2DPoint& in) { return reinterpret_cast<const EeGe2DPoint*>(&in); }
inline EeGe2DPoint* GERust(GE2DPoint& in) { return reinterpret_cast<EeGe2DPoint*>(&in); }
inline const EeGe3DPoint* GERust(const GE3DPoint& in) { return reinterpret_cast<const EeGe3DPoint*>(&in); }
inline EeGe3DPoint* GERust(GE3DPoint& in) { return reinterpret_cast<EeGe3DPoint*>(&in); }
inline const EeGeVector* GERust(const GEVector& in) { return reinterpret_cast<const EeGeVector*>(&in); }
inline EeGeVector* GERust(GEVector& in) { return reinterpret_cast<EeGeVector*>(&in); }
inline const EeGe3DLine* GERust(const GE3DLine& in) { return reinterpret_cast<const EeGe3DLine*>(&in); }
inline EeGe3DLine* GERust(GE3DLine& in) { return reinterpret_cast<EeGe3DLine*>(&in); }
inline const EeGe3DPlane* GERust(const GE3DPlane& in) { return reinterpret_cast<const EeGe3DPlane*>(&in); }
inline EeGe3DPlane* GERust(GE3DPlane& in) { return reinterpret_cast<EeGe3DPlane*>(&in); }
inline const EeGeTransformation* GERust(const GETransformation& in) { return reinterpret_cast<const EeGeTransformation*>(&in); }
inline EeGeTransformation* GERust(GETransformation& in) { return reinterpret_cast<EeGeTransformation*>(&in); }
inline const EeGeOrientation* GERust(const GEOrientation& in) { return reinterpret_cast<const EeGeOrientation*>(&in); }
inline EeGeOrientation* GERust(GEOrientation& in) { return reinterpret_cast<EeGeOrientation*>(&in); }
inline const EeGeQuaternion* GERust(const GEQuaternion& in) { return reinterpret_cast<const EeGeQuaternion*>(&in); }
inline EeGeQuaternion* GERust(GEQuaternion& in) { return reinterpret_cast<EeGeQuaternion*>(&in); }
inline const EeGeBoundingBox* GERust(const GEBoundingBox& in) { return reinterpret_cast<const EeGeBoundingBox*>(&in); }
inline EeGeBoundingBox* GERust(GEBoundingBox& in) { return reinterpret_cast<EeGeBoundingBox*>(&in); }
inline const EeGeRectangle* GERust(const GERectangle& in) { return reinterpret_cast<const EeGeRectangle*>(&in); }
inline EeGeRectangle* GERust(GERectangle& in) { return reinterpret_cast<EeGeRectangle*>(&in); }

// std::vector::push_back for the interpolations and the rectangle split (the context is the vector)
inline void GERustPush2D(void* inContext, const EeGe2DPoint* inPoint)
{
    static_cast<std::vector<GE2DPoint>*>(inContext)->push_back(*reinterpret_cast<const GE2DPoint*>(inPoint));
}
inline void GERustPush3D(void* inContext, const EeGe3DPoint* inPoint)
{
    static_cast<std::vector<GE3DPoint>*>(inContext)->push_back(*reinterpret_cast<const GE3DPoint*>(inPoint));
}
inline void GERustPushRectangle(void* inContext, const EeGeRectangle* inRectangle)
{
    static_cast<std::vector<GERectangle>*>(inContext)->push_back(*reinterpret_cast<const GERectangle*>(inRectangle));
}

#endif
