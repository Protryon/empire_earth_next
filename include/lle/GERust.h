// The Graphics Engine's value types over Rust (docs/RUST_PLAN.md R4.3; LLE round 8): the C++ classes stay the types
// every caller uses and their members are inline forwarders to the C ABI of ee_base::math (rust/base/math.h) and
// ee_render::geval (rust/render/geval.h), with the objects themselves as arguments. This header gives each class its
// #[repr(C)] mirror (the same storage; the layouts are pinned in Graphics Engine/GERustMath.h) and the field stream
// over an FSFile that their loaders and savers hand to Rust. The classes need not be complete here.
#ifndef LLE_GERUST_H
#define LLE_GERUST_H

#include <stdint.h>
#include <type_traits>
#include "rust/base/math.h"
#include "rust/render/geval.h"

class GE2DPoint;
class GE3DPoint;
class GEVector;
class GE3DLine;
class GE3DPlane;
class GETransformation;
class GEOrientation;
class GEQuaternion;
class GEBoundingBox;
class GERectangle;
class GEViewport;
class FSFileSpec;

// The C++ object as its mirror, and a mirror as its C++ object
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

// A field stream over a file for the loaders and savers in Rust (ee_render geval io.rs): Rust decides the order, the
// checks and the error handling, and every field still goes through the file's own typed Read/Write, so the bytes,
// the per-class short-read rules and the position a failed load leaves are the C++'s. F is the file's static type: an
// FSFileSpec (or a class derived from it) adds its position, a plain FSFile has none. Templates, so that the file
// class is needed only where a member is used.
template <class F> struct GERustIO {
    static F* File(void* inFile) { return static_cast<F*>(inFile); }
    static long ReadU8(void* inFile, uint8_t* outValue) { return File(inFile)->Read(*outValue); }
    static long ReadU16(void* inFile, uint16_t* outValue) { return File(inFile)->Read(*outValue); }
    static long ReadU32(void* inFile, uint32_t* outValue) { return File(inFile)->Read(*outValue); }
    static long ReadF32(void* inFile, float* outValue) { return File(inFile)->Read(*outValue); }
    static long WriteU8(void* inFile, uint8_t inValue) { return File(inFile)->Write(inValue); }
    static long WriteU16(void* inFile, uint16_t inValue) { return File(inFile)->Write(inValue); }
    static long WriteU32(void* inFile, uint32_t inValue) { return File(inFile)->Write(inValue); }
    static long WriteF32(void* inFile, float inValue) { return File(inFile)->Write(inValue); }
    static long ReadBytes(void* inFile, uint8_t* outData, unsigned long inLength, unsigned long* outRead)
        { return File(inFile)->Read(reinterpret_cast<char*>(outData), inLength, *outRead); }
    static long WriteBytes(void* inFile, const uint8_t* inData, unsigned long inLength, unsigned long* outWritten)
        { return File(inFile)->Write(reinterpret_cast<const char*>(inData), inLength, *outWritten); }
    // a plain FSFile has no position: kErrorFailed
    static long GetPosition(void* inFile, long* outPosition)
    {
        if constexpr (std::is_base_of<FSFileSpec, F>::value) return File(inFile)->GetPosition(*outPosition);
        else return 9;
    }
    static long SetPosition(void* inFile, long inPosition)
    {
        if constexpr (std::is_base_of<FSFileSpec, F>::value) return File(inFile)->SetPosition(inPosition, 0, false);
        else return 9;
    }
};

template <class F> inline EeGeFieldIo GERustFieldIO(F& inFile)
{
    return {&inFile, GERustIO<F>::ReadU8, GERustIO<F>::ReadU16, GERustIO<F>::ReadU32, GERustIO<F>::ReadF32,
            GERustIO<F>::WriteU8, GERustIO<F>::WriteU16, GERustIO<F>::WriteU32, GERustIO<F>::WriteF32,
            GERustIO<F>::ReadBytes, GERustIO<F>::WriteBytes, GERustIO<F>::GetPosition, GERustIO<F>::SetPosition};
}

// The value types' codecs (ee_ge_fields_io): the fields in order through the file's float reads or writes. inAskOpen:
// kErrorNotOpen when the file is not open (GEColor does not ask); inRestore: the position read first and set back
// after a failed field (GETextureCoordinate).
template <class F>
inline long GERustFields(F& inFile, const float* const* inFields, uint32_t inCount, bool inAskOpen, bool inWrite,
                         bool inRestore = false)
{
    EeGeFieldIo theIO = GERustFieldIO(inFile);
    return ee_ge_fields_io(const_cast<float* const*>(inFields), inCount, &theIO, inAskOpen ? inFile.GetIsOpen() : -1,
                           inWrite, inRestore);
}

#endif
