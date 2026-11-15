// C:\EEX\Low-Level Engine\Graphics Engine\GETransformation.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100349a3..0x10035941: sTransformationStringID's init chain, then the out-of-line GETransformation members. The
// small members (0x1000d5a1..0x1000d9d3) are emitted by GE3DPoint.cpp (see lle/GETransformation.h).
//
// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x100349a3
// SYNTHETIC: LLE 0x100349ad
// SYNTHETIC: LLE 0x100349bd
// SYNTHETIC: LLE 0x100349c9
// Every member forwards to Rust (rust/crates/ee_ge transform.rs, ffi.rs; GERustMath.h): the composition, the inverse,
// the look-at, Normalize, the rotations and the orientations, and (R4.3 part 12) the constructors' and operator='s
// element copies, the comparisons, the identity, scale and zero-orientation setters and the file codec.
#include <windows.h>
#include <math.h>
#include <string.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/FSFile.h"
#include "GERustMath.h"
#include "GERustIO.h"

const UString GETransformation::sTransformationStringID("GETransformation-TransformationData");

// FUNCTION: LLE 0x100349e4
// Creates an identity transformation
GETransformation::GETransformation()
{
    ee_ge_t_identity(GERust(*this));
}

GETransformation::GETransformation(const float inMatrix[4][4])
{
    ee_ge_t_copy(GERust(*this), reinterpret_cast<const float(*)[4][4]>(inMatrix));
}

// FUNCTION: LLE 0x10034a36
// Creates a copy of a transformation
GETransformation::GETransformation(const GETransformation& inTransformation)
{
    ee_ge_t_copy(GERust(*this), &GERust(inTransformation)->m);
}

GETransformation& GETransformation::operator=(const GETransformation& inTransformation)
{
    ee_ge_t_copy(GERust(*this), &GERust(inTransformation)->m);
    return *this;
}

// FUNCTION: LLE 0x10034a9d
// Returns whether all sixteen matrix elements equal the other transformation's
bool GETransformation::operator==(const GETransformation& inTransformation)
{
    return ee_ge_t_equal(GERust(*this), GERust(inTransformation));
}

// FUNCTION: LLE 0x10034b91
// Returns whether the transformation's position equals the point
bool GETransformation::operator==(const GE3DPoint& inPoint) const
{
    return ee_ge_t_at_point(GERust(*this), GERust(inPoint));
}

// NEAR: one store of b23 is scheduled one instruction later in the original; tried declaration order, GetElement
// FUNCTION: LLE 0x10034bc5
// Composes this transformation with another (this x other) into outTransformation;
// the bottom row is copied from the other
void GETransformation::Apply(const GETransformation& inTransformation, GETransformation& outTransformation) const
{
    ee_ge_t_compose(GERust(*this), GERust(inTransformation), GERust(outTransformation));
}

void GETransformation::Apply(const GE3DLine& inLine, GE3DLine& outLine) const
{
    ee_ge_t_apply_line(GERust(*this), GERust(inLine), GERust(outLine));
}

// FUNCTION: LLE 0x10034d9e
// Transforms a plane (its point and normal) into outPlane
void GETransformation::Apply(const GE3DPlane& inPlane, GE3DPlane& outPlane) const
{
    ee_ge_t_apply_plane(GERust(*this), GERust(inPlane), GERust(outPlane));
}

// FUNCTION: LLE 0x10034df8
// Extracts yaw, pitch and roll angles from the rotation part into an orientation
void GETransformation::ConvertToOrientation(GEOrientation& outOrientation) const
{
    ee_ge_t_orientation(GERust(*this), GERust(outOrientation));
}

// FUNCTION: LLE 0x10034e48
// Reads the sixteen matrix elements from a file, stopping at the first error
long GETransformation::Load(FSFile& inFile)
{
    EeGeFieldIo theIO = GERustFieldIO(inFile);
    return ee_ge_t_io(GERust(*this), &theIO, false);
}

// FUNCTION: LLE 0x10034e89
// Resets the transformation to identity
void GETransformation::MakeIdentity()
{
    ee_ge_t_identity(GERust(*this));
}

// FUNCTION: LLE 0x10034ed9
// Computes the inverse of a rigid transformation (transposed rotation, negated rotated position)
void GETransformation::MakeInverse(GETransformation& outInverse) const
{
    // element by element, as the C++ stored them (an output equal to this transposes through its own stores)
    ee_ge_t_make_inverse(GERust(*this), GERust(outInverse));
}

// FUNCTION: LLE 0x100350e5
// Rescales the three rotation columns to unit length
void GETransformation::Normalize()
{
    ee_ge_t_normalize(GERust(*this));
}

// FUNCTION: LLE 0x1003513a
// Rotates the transformation by yaw, pitch and roll applied after it (about its own axes)
void GETransformation::Revolve(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_revolve(GERust(*this), inYaw, inPitch, inRoll);
}

// FUNCTION: LLE 0x100351a1
// Rotates the transformation by yaw, pitch and roll applied before it (about the parent axes)
void GETransformation::Rotate(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_rotate(GERust(*this), inYaw, inPitch, inRoll);
}

// FUNCTION: LLE 0x10035208
// Scales the transformation by x, y and z (scale applied before it)
void GETransformation::Scale(float inX, float inY, float inZ)
{
    ee_ge_t_scale(GERust(*this), inX, inY, inZ);
}

// FUNCTION: LLE 0x1003525b
// Writes the sixteen matrix elements to a file, stopping at the first error
long GETransformation::Save(FSFile& inFile) const
{
    EeGeFieldIo theIO = GERustFieldIO(inFile);
    return ee_ge_t_io(const_cast<EeGeTransformation*>(GERust(*this)), &theIO, true);
}

// FUNCTION: LLE 0x1003529c
// Sets the rotation from yaw, pitch and roll in pitch-roll-yaw order (position kept)
void GETransformation::SetOrientationPRY(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_set_orientation(GERust(*this), 0, inYaw, inPitch, inRoll);
}

// FUNCTION: LLE 0x10035395
// Sets the rotation from yaw, pitch and roll in pitch-yaw-roll order (position kept)
void GETransformation::SetOrientationPYR(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_set_orientation(GERust(*this), 1, inYaw, inPitch, inRoll);
}

// 95.18: register lifetimes of theRP01/theSinRoll differ (the other five orders are exact with the same shape);
// tried temp order, term order, -theSinRoll inline
// FUNCTION: LLE 0x1003548e
// Sets the rotation from yaw, pitch and roll in roll-pitch-yaw order (position kept)
void GETransformation::SetOrientationRPY(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_set_orientation(GERust(*this), 2, inYaw, inPitch, inRoll);
}

// FUNCTION: LLE 0x10035580
// Sets the rotation from yaw, pitch and roll in roll-yaw-pitch order (position kept)
void GETransformation::SetOrientationRYP(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_set_orientation(GERust(*this), 3, inYaw, inPitch, inRoll);
}

// FUNCTION: LLE 0x10035675
// Sets the rotation from yaw, pitch and roll in yaw-pitch-roll order (position kept)
void GETransformation::SetOrientationYPR(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_set_orientation(GERust(*this), 4, inYaw, inPitch, inRoll);
}

// FUNCTION: LLE 0x10035769
// Sets the rotation from yaw, pitch and roll in yaw-roll-pitch order (position kept)
void GETransformation::SetOrientationYRP(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_set_orientation(GERust(*this), 5, inYaw, inPitch, inRoll);
}

// FUNCTION: LLE 0x1003585b
// Makes the transformation a pure scale by x, y and z (position column kept)
void GETransformation::SetScale(float inX, float inY, float inZ)
{
    ee_ge_t_set_scale(GERust(*this), inX, inY, inZ);
}

// FUNCTION: LLE 0x100358a4
// Returns true (no consistency check in the retail build)
bool GETransformation::Verify() const
{
    return true;
}

// ---- Exported members with no address of their own (ICF-folded: GetChecksum's `return 0` 0x32e79, Dump's empty
// body 0x80c07)
void GETransformation::Dump() const
{
}

unsigned long GETransformation::GetChecksum() const
{
    return 0;
}
