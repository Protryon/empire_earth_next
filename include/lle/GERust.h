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

// The C++ object as its mirror
#define LLE_GERUST_MIRROR(C, R) \
    inline const R* GERust(const C& in) { return reinterpret_cast<const R*>(&in); } \
    inline R* GERust(C& in) { return reinterpret_cast<R*>(&in); }
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
    static long ReadBytes(void* inFile, uint8_t* outData, uint32_t inLength, uint32_t* outRead)
    {
        unsigned long theRead = *outRead;
        long theResult = File(inFile)->Read(reinterpret_cast<char*>(outData), inLength, theRead);
        *outRead = (uint32_t)theRead;
        return theResult;
    }
    static long WriteBytes(void* inFile, const uint8_t* inData, uint32_t inLength, uint32_t* outWritten)
    {
        unsigned long theWritten = *outWritten;
        long theResult = File(inFile)->Write(reinterpret_cast<const char*>(inData), inLength, theWritten);
        *outWritten = (uint32_t)theWritten;
        return theResult;
    }
    static long NoPosition(void*, long*) { return 9; }       // kErrorFailed: a plain FSFile has no position
    static long NoSetPosition(void*, long) { return 9; }
    static long GetPosition(void* inFile, long* outPosition) { return File(inFile)->GetPosition(*outPosition); }
    static long SetPosition(void* inFile, long inPosition) { return File(inFile)->SetPosition(inPosition, 0, false); }
};

template <class F> inline EeGeFieldIo GERustFieldIO(F& inFile)
{
    typedef GERustIO<F> IO;
    EeGeFieldIo theIO;
    theIO.file = &inFile;
    theIO.read_u8 = IO::ReadU8;
    theIO.read_u16 = IO::ReadU16;
    theIO.read_u32 = IO::ReadU32;
    theIO.read_f32 = IO::ReadF32;
    theIO.write_u8 = IO::WriteU8;
    theIO.write_u16 = IO::WriteU16;
    theIO.write_u32 = IO::WriteU32;
    theIO.write_f32 = IO::WriteF32;
    theIO.read_bytes = IO::ReadBytes;
    theIO.write_bytes = IO::WriteBytes;
    theIO.get_position = IO::NoPosition;
    theIO.set_position = IO::NoSetPosition;
    if constexpr (std::is_base_of<FSFileSpec, F>::value) {
        theIO.get_position = IO::GetPosition;
        theIO.set_position = IO::SetPosition;
    }
    return theIO;
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
