// The Graphics Engine's value types over Rust (docs/RUST_PLAN.md R4.3; LLE round 8): the C++ classes stay the types
// every caller uses and their members are inline forwarders to the C ABI of ee_base::math (rust/base/math.h) and
// ee_render::geval (rust/render/geval.h), with the objects themselves as arguments. This header gives each class its
// #[repr(C)] mirror (the same storage; the layouts are pinned in Graphics Engine/GERustMath.h) and the file their
// loaders and savers hand to Rust. The classes need not be complete here.
#ifndef LLE_GERUST_H
#define LLE_GERUST_H

#include <stdint.h>
#include "rust/base/math.h"
#include "rust/render/geval.h"

#include "lle/LLEForward.h"

// The C++ object as its record (the classes derive from their records: the records' fields are the classes' members),
// and a record as its C++ object
#define LLE_GERUST_MIRROR(C, R) \
    inline const R* GERust(const C& in) { return reinterpret_cast<const R*>(&in); } \
    inline R* GERust(C& in) { return reinterpret_cast<R*>(&in); } \
    inline const C& GECpp(const R& in) { return reinterpret_cast<const C&>(in); }
LLE_GERUST_MIRROR(GE2DPoint, EeGe2DPoint)
LLE_GERUST_MIRROR(GE3DPoint, EeGe3DPoint)
LLE_GERUST_MIRROR(GEVector, EeGeVector)
LLE_GERUST_MIRROR(GE3DLine, EeGe3DLine)
LLE_GERUST_MIRROR(GE3DPlane, EeGe3DPlane)
LLE_GERUST_MIRROR(GETransformation, EeGeTransformation)
LLE_GERUST_MIRROR(GEOrientation, EeGeOrientation)
LLE_GERUST_MIRROR(GEQuaternion, EeGeQuaternion)
LLE_GERUST_MIRROR(GEBoundingBox, EeGeBoundingBox)
LLE_GERUST_MIRROR(GERectangle, EeGeRectangle)
LLE_GERUST_MIRROR(GEViewport, EeGeViewport)
#undef LLE_GERUST_MIRROR

// The file a value type's Load or Save hands to Rust (ee_render geval values.rs, over ee_base fs cppfile.rs): the C++
// FSFile, read and written through its virtuals. A template, so that FSFile is needed only where a member is used.
template <class F> inline void* GERustFile(F& inFile) { return static_cast<FSFile*>(&inFile); }

#endif
