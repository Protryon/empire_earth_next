// The layouts of the Graphics Engine's value classes and their Rust mirrors (EeGe*: rust/base/math.h,
// rust/render/geval.h), pinned here; the mirrors and the forwarders are lle/GERust.h.
#ifndef GERUSTMATH_H
#define GERUSTMATH_H

#include <stddef.h>
#include <vector>

#include "lle/GERust.h"
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
#include "lle/GEViewport.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"

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

// the vertex arrays' values as floats (ee_ge_va_get/set; lle/GEVertexArray.h)
static_assert(sizeof(GEColor) == 3 * sizeof(float) && sizeof(GE3DPoint) == 3 * sizeof(float) &&
              sizeof(GEVector) == 3 * sizeof(float) && sizeof(GETextureCoordinate) == 2 * sizeof(float), "vertarr.rs");

#endif
