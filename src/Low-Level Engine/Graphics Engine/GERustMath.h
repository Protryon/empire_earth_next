// The Graphics Engine's value classes, which derive from their Rust records (EeGe*: rust/base/math.h,
// rust/render/geval.h); the mirrors and the forwarders are lle/GERust.h.
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

// the vertex arrays' values as floats (ee_ge_va_get/set; lle/GEVertexArray.h)
static_assert(sizeof(GEColor) == 3 * sizeof(float) && sizeof(GE3DPoint) == 3 * sizeof(float) &&
              sizeof(GEVector) == 3 * sizeof(float) && sizeof(GETextureCoordinate) == 2 * sizeof(float), "vertarr.rs");

#endif
