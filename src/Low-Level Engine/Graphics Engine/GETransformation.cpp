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
// The composition, the inverse, the look-at, Normalize, the rotations and the orientations forward to Rust
// (rust/crates/ee_ge transform.rs; GERustMath.h); the constructors, the comparisons, the identity/zero setters and
// the file I/O stay here.
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

const UString GETransformation::sTransformationStringID("GETransformation-TransformationData");

// FUNCTION: LLE 0x100349e4
// Creates an identity transformation
GETransformation::GETransformation()
{
    this->mMatrix[0][0] = 1.0f;
    this->mMatrix[0][1] = 0.0f;
    this->mMatrix[0][2] = 0.0f;
    this->mMatrix[0][3] = 0.0f;
    this->mMatrix[1][0] = 0.0f;
    this->mMatrix[1][1] = 1.0f;
    this->mMatrix[1][2] = 0.0f;
    this->mMatrix[1][3] = 0.0f;
    this->mMatrix[2][0] = 0.0f;
    this->mMatrix[2][1] = 0.0f;
    this->mMatrix[2][2] = 1.0f;
    this->mMatrix[2][3] = 0.0f;
    this->mMatrix[3][0] = 0.0f;
    this->mMatrix[3][1] = 0.0f;
    this->mMatrix[3][2] = 0.0f;
    this->mMatrix[3][3] = 1.0f;
}

GETransformation::GETransformation(const float inMatrix[4][4])
{
    this->mMatrix[0][0] = inMatrix[0][0];
    this->mMatrix[0][1] = inMatrix[0][1];
    this->mMatrix[0][2] = inMatrix[0][2];
    this->mMatrix[0][3] = inMatrix[0][3];
    this->mMatrix[1][0] = inMatrix[1][0];
    this->mMatrix[1][1] = inMatrix[1][1];
    this->mMatrix[1][2] = inMatrix[1][2];
    this->mMatrix[1][3] = inMatrix[1][3];
    this->mMatrix[2][0] = inMatrix[2][0];
    this->mMatrix[2][1] = inMatrix[2][1];
    this->mMatrix[2][2] = inMatrix[2][2];
    this->mMatrix[2][3] = inMatrix[2][3];
    this->mMatrix[3][0] = inMatrix[3][0];
    this->mMatrix[3][1] = inMatrix[3][1];
    this->mMatrix[3][2] = inMatrix[3][2];
    this->mMatrix[3][3] = inMatrix[3][3];
}

// FUNCTION: LLE 0x10034a36
// Creates a copy of a transformation
GETransformation::GETransformation(const GETransformation& inTransformation)
{
    this->mMatrix[0][0] = inTransformation.mMatrix[0][0];
    this->mMatrix[0][1] = inTransformation.mMatrix[0][1];
    this->mMatrix[0][2] = inTransformation.mMatrix[0][2];
    this->mMatrix[0][3] = inTransformation.mMatrix[0][3];
    this->mMatrix[1][0] = inTransformation.mMatrix[1][0];
    this->mMatrix[1][1] = inTransformation.mMatrix[1][1];
    this->mMatrix[1][2] = inTransformation.mMatrix[1][2];
    this->mMatrix[1][3] = inTransformation.mMatrix[1][3];
    this->mMatrix[2][0] = inTransformation.mMatrix[2][0];
    this->mMatrix[2][1] = inTransformation.mMatrix[2][1];
    this->mMatrix[2][2] = inTransformation.mMatrix[2][2];
    this->mMatrix[2][3] = inTransformation.mMatrix[2][3];
    this->mMatrix[3][0] = inTransformation.mMatrix[3][0];
    this->mMatrix[3][1] = inTransformation.mMatrix[3][1];
    this->mMatrix[3][2] = inTransformation.mMatrix[3][2];
    this->mMatrix[3][3] = inTransformation.mMatrix[3][3];
}

GETransformation& GETransformation::operator=(const GETransformation& inTransformation)
{
    this->mMatrix[0][0] = inTransformation.mMatrix[0][0];
    this->mMatrix[0][1] = inTransformation.mMatrix[0][1];
    this->mMatrix[0][2] = inTransformation.mMatrix[0][2];
    this->mMatrix[0][3] = inTransformation.mMatrix[0][3];
    this->mMatrix[1][0] = inTransformation.mMatrix[1][0];
    this->mMatrix[1][1] = inTransformation.mMatrix[1][1];
    this->mMatrix[1][2] = inTransformation.mMatrix[1][2];
    this->mMatrix[1][3] = inTransformation.mMatrix[1][3];
    this->mMatrix[2][0] = inTransformation.mMatrix[2][0];
    this->mMatrix[2][1] = inTransformation.mMatrix[2][1];
    this->mMatrix[2][2] = inTransformation.mMatrix[2][2];
    this->mMatrix[2][3] = inTransformation.mMatrix[2][3];
    this->mMatrix[3][0] = inTransformation.mMatrix[3][0];
    this->mMatrix[3][1] = inTransformation.mMatrix[3][1];
    this->mMatrix[3][2] = inTransformation.mMatrix[3][2];
    this->mMatrix[3][3] = inTransformation.mMatrix[3][3];
    return *this;
}

// FUNCTION: LLE 0x10034a9d
// Returns whether all sixteen matrix elements equal the other transformation's
bool GETransformation::operator==(const GETransformation& inTransformation)
{
    return this->GetElement(0, 0) == inTransformation.GetElement(0, 0) && this->GetElement(0, 1) == inTransformation.GetElement(0, 1) &&
           this->GetElement(0, 2) == inTransformation.GetElement(0, 2) && this->GetElement(0, 3) == inTransformation.GetElement(0, 3) &&
           this->GetElement(1, 0) == inTransformation.GetElement(1, 0) && this->GetElement(1, 1) == inTransformation.GetElement(1, 1) &&
           this->GetElement(1, 2) == inTransformation.GetElement(1, 2) && this->GetElement(1, 3) == inTransformation.GetElement(1, 3) &&
           this->GetElement(2, 0) == inTransformation.GetElement(2, 0) && this->GetElement(2, 1) == inTransformation.GetElement(2, 1) &&
           this->GetElement(2, 2) == inTransformation.GetElement(2, 2) && this->GetElement(2, 3) == inTransformation.GetElement(2, 3) &&
           this->GetElement(3, 0) == inTransformation.GetElement(3, 0) && this->GetElement(3, 1) == inTransformation.GetElement(3, 1) &&
           this->GetElement(3, 2) == inTransformation.GetElement(3, 2) && this->GetElement(3, 3) == inTransformation.GetElement(3, 3);
}

// FUNCTION: LLE 0x10034b91
// Returns whether the transformation's position equals the point
bool GETransformation::operator==(const GE3DPoint& inPoint) const
{
    return this->GetX() == inPoint.GetX() && this->GetY() == inPoint.GetY() && this->GetZ() == inPoint.GetZ();
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
    long theResult = kErrorOK;
    int i;
    unsigned long j;
    for (i = 0; i < 4 && theResult == kErrorOK; i++)
        for (j = 0; j < 4 && theResult == kErrorOK; j++)
            theResult = inFile.Read(this->mMatrix[i][j]);
    return theResult;
}

// FUNCTION: LLE 0x10034e89
// Resets the transformation to identity
void GETransformation::MakeIdentity()
{
    this->mMatrix[0][0] = 1.0f;
    this->mMatrix[0][1] = 0.0f;
    this->mMatrix[0][2] = 0.0f;
    this->mMatrix[0][3] = 0.0f;
    this->mMatrix[1][0] = 0.0f;
    this->mMatrix[1][1] = 1.0f;
    this->mMatrix[1][2] = 0.0f;
    this->mMatrix[1][3] = 0.0f;
    this->mMatrix[2][0] = 0.0f;
    this->mMatrix[2][1] = 0.0f;
    this->mMatrix[2][2] = 1.0f;
    this->mMatrix[2][3] = 0.0f;
    this->mMatrix[3][0] = 0.0f;
    this->mMatrix[3][1] = 0.0f;
    this->mMatrix[3][2] = 0.0f;
    this->mMatrix[3][3] = 1.0f;
}

// FUNCTION: LLE 0x10034ed9
// Computes the inverse of a rigid transformation (transposed rotation, negated rotated position)
void GETransformation::MakeInverse(GETransformation& outInverse) const
{
    // element by element, as the C++ stored them (an output equal to this transposes through its own stores)
    ee_ge_t_make_inverse(GERust(*this), GERust(outInverse));
}

// NEAR: only the sqrt argument differs (the original keeps x and y on the FPU stack); tried float/double sums,
// accessor/member operands in both orders, UMFastMath::Sqrt
// FUNCTION: LLE 0x10034f87
// Builds a camera transformation at the eye looking toward the target (special case for straight up or down)
void GETransformation::MakeLookAtTransform(GE3DPoint& inEye, GE3DPoint& inTarget)
{
    ee_ge_t_make_look_at(GERust(*this), GERust(inEye), GERust(inTarget));
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
    long theResult = kErrorOK;
    int i;
    unsigned long j;
    for (i = 0; i < 4 && theResult == kErrorOK; i++)
        for (j = 0; j < 4 && theResult == kErrorOK; j++)
            theResult = inFile.Write(this->mMatrix[i][j]);
    return theResult;
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
    this->mMatrix[0][0] = inX;
    this->mMatrix[0][1] = 0.0f;
    this->mMatrix[0][2] = 0.0f;
    this->mMatrix[1][0] = 0.0f;
    this->mMatrix[1][1] = inY;
    this->mMatrix[1][2] = 0.0f;
    this->mMatrix[2][0] = 0.0f;
    this->mMatrix[2][1] = 0.0f;
    this->mMatrix[2][2] = inZ;
    this->mMatrix[3][0] = 0.0f;
    this->mMatrix[3][1] = 0.0f;
    this->mMatrix[3][2] = 0.0f;
    this->mMatrix[3][3] = 1.0f;
}

// FUNCTION: LLE 0x100358a4
// Returns true (no consistency check in the retail build)
bool GETransformation::Verify() const
{
    return true;
}

// FUNCTION: LLE 0x100358a7
// Resets the rotation to identity, keeping the position
void GETransformation::ZeroOrientation()
{
    this->mMatrix[0][0] = 1.0f;
    this->mMatrix[0][1] = 0.0f;
    this->mMatrix[0][2] = 0.0f;
    this->mMatrix[1][0] = 0.0f;
    this->mMatrix[1][1] = 1.0f;
    this->mMatrix[1][2] = 0.0f;
    this->mMatrix[2][0] = 0.0f;
    this->mMatrix[2][1] = 0.0f;
    this->mMatrix[2][2] = 1.0f;
    this->mMatrix[3][0] = 0.0f;
    this->mMatrix[3][1] = 0.0f;
    this->mMatrix[3][2] = 0.0f;
    this->mMatrix[3][3] = 1.0f;
}

// FUNCTION: LLE 0x100358e8
// Writes a copy with identity rotation and this transformation's position into outTransformation
void GETransformation::ZeroOrientation(GETransformation& outTransformation) const
{
    outTransformation.mMatrix[0][0] = 1.0f;
    outTransformation.mMatrix[0][1] = 0.0f;
    outTransformation.mMatrix[0][2] = 0.0f;
    outTransformation.mMatrix[0][3] = this->mMatrix[0][3];
    outTransformation.mMatrix[1][0] = 0.0f;
    outTransformation.mMatrix[1][1] = 1.0f;
    outTransformation.mMatrix[1][2] = 0.0f;
    outTransformation.mMatrix[1][3] = this->mMatrix[1][3];
    outTransformation.mMatrix[2][0] = 0.0f;
    outTransformation.mMatrix[2][1] = 0.0f;
    outTransformation.mMatrix[2][2] = 1.0f;
    outTransformation.mMatrix[2][3] = this->mMatrix[2][3];
    outTransformation.mMatrix[3][0] = 0.0f;
    outTransformation.mMatrix[3][1] = 0.0f;
    outTransformation.mMatrix[3][2] = 0.0f;
    outTransformation.mMatrix[3][3] = 1.0f;
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
