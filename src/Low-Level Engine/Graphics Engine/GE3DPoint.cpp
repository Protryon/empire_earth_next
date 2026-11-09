// C:\EEX\Low-Level Engine\Graphics Engine\GE3DPoint.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// Opens with the GEVector(x, y, z)/operator= COMDATs and GETransformation's small members (0x1000d5a1..), emitted
// first by this unit (same pattern as GE2DPoint.cpp; see lle/GETransformation.h).
// The distances, the angles and InterpolateTo forward to Rust (rust/crates/ee_ge: transform.rs, point.rs;
// GERustMath.h); the accessors, the orientation overloads and the file I/O stay here.
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSMemoryFile.h"
#include "lle/UMFastMath.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "GERustMath.h"
// header-inline members emitted by this unit: GEVector(float, float, float) 0x1000d56f (ICF representative),
// GEVector::operator= 0x1000d588, GETransformation::GetZ 0x1000d7c4, SetPosition(float, float, float) 0x1000d955,
// Translate(float, float, float) 0x1000d9b2
// FUNCTION: LLE 0x1000d56f
// FUNCTION: LLE 0x1000d588
// FUNCTION: LLE 0x1000d7c4
// FUNCTION: LLE 0x1000d955
// FUNCTION: LLE 0x1000d9b2

// SYNTHETIC: LLE 0x1000d5a1
// GETransformation(bool): header-inline (lle/GETransformation.h; the exe never imports it), emitted here

// FUNCTION: LLE 0x1000d680
// Returns the distance from the transformation's position to a point (full-precision square root)
float GETransformation::DistanceTo(const GE3DPoint& inPoint) const
{
    return ee_ge_t_distance_to_point(GERust(*this), GERust(inPoint));
}

// FUNCTION: LLE 0x1000d702
// Returns the distance between two transformations' positions (fast square root)
float GETransformation::DistanceTo(const GETransformation& inTransformation) const
{
    return ee_ge_t_distance_to(GERust(*this), GERust(inTransformation));
}

// FUNCTION: LLE 0x1000d71a
// Returns the squared distance from the transformation's position to a point
float GETransformation::DistanceSquaredTo(const GE3DPoint& inPoint) const
{
    return ee_ge_t_distance_squared_to_point(GERust(*this), GERust(inPoint));
}

// FUNCTION: LLE 0x1000d74e
// Returns the squared distance between two transformations' positions
float GETransformation::DistanceSquaredTo(const GETransformation& inTransformation) const
{
    return ee_ge_t_distance_squared_to(GERust(*this), GERust(inTransformation));
}

// SYNTHETIC: LLE 0x1000d783
// GETransformation::GetElement: header-inline (lle/GETransformation.h), emitted here

// FUNCTION: LLE 0x1000d794
// Returns the transformation's position as a point
GE3DPoint GETransformation::GetPosition() const
{
    return GE3DPoint(this->GetX(), this->GetY(), this->GetZ());
}

// FUNCTION: LLE 0x1000d7ac
// Writes the transformation's position to outPosition
void GETransformation::GetPosition(GE3DPoint& outPosition) const
{
    outPosition.mX = this->GetX();
    outPosition.mY = this->GetY();
    outPosition.mZ = this->GetZ();
}

// FUNCTION: LLE 0x1000d7c8
// Returns the yaw angle taken from the matrix
float GETransformation::GetYaw() const
{
    return ee_ge_t_yaw(GERust(*this));
}

// FUNCTION: LLE 0x1000d7dd
// Returns the pitch angle taken from the matrix
float GETransformation::GetPitch() const
{
    return ee_ge_t_pitch(GERust(*this));
}

// FUNCTION: LLE 0x1000d7f3
// Returns the roll angle taken from the matrix
float GETransformation::GetRoll() const
{
    return ee_ge_t_roll(GERust(*this));
}

// FUNCTION: LLE 0x1000d808
// Revolves the transformation by an orientation's yaw, pitch and roll
void GETransformation::Revolve(const GEOrientation& inOrientation)
{
    this->Revolve(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// FUNCTION: LLE 0x1000d828
// Rotates the transformation by an orientation's yaw, pitch and roll
void GETransformation::Rotate(const GEOrientation& inOrientation)
{
    this->Rotate(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// SYNTHETIC: LLE 0x1000d848
// GETransformation::Scale(float): header-inline (lle/GETransformation.h), emitted here

// SYNTHETIC: LLE 0x1000d868
// GETransformation::SetElement: header-inline (lle/GETransformation.h), emitted here

// FUNCTION: LLE 0x1000d87d
// Sets the rotation from an orientation, applied pitch, roll, yaw
void GETransformation::SetOrientationPRY(const GEOrientation& inOrientation)
{
    this->SetOrientationPRY(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// FUNCTION: LLE 0x1000d89d
// Sets the rotation from an orientation, applied pitch, yaw, roll
void GETransformation::SetOrientationPYR(const GEOrientation& inOrientation)
{
    this->SetOrientationPYR(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// FUNCTION: LLE 0x1000d8bd
// Sets the rotation from an orientation, applied roll, pitch, yaw
void GETransformation::SetOrientationRPY(const GEOrientation& inOrientation)
{
    this->SetOrientationRPY(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// FUNCTION: LLE 0x1000d8dd
// Sets the rotation from an orientation, applied roll, yaw, pitch
void GETransformation::SetOrientationRYP(const GEOrientation& inOrientation)
{
    this->SetOrientationRYP(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// FUNCTION: LLE 0x1000d8fd
// Sets the rotation from an orientation, applied yaw, pitch, roll
void GETransformation::SetOrientationYPR(const GEOrientation& inOrientation)
{
    this->SetOrientationYPR(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// FUNCTION: LLE 0x1000d91d
// Sets the rotation from an orientation, applied yaw, roll, pitch
void GETransformation::SetOrientationYRP(const GEOrientation& inOrientation)
{
    this->SetOrientationYRP(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// FUNCTION: LLE 0x1000d93d
// Moves the transformation's position to a point
void GETransformation::SetPosition(const GE3DPoint& inPosition)
{
    this->SetPosition(inPosition.GetX(), inPosition.GetY(), inPosition.GetZ());
}

// FUNCTION: LLE 0x1000d96d
// Sets the same scale on all three axes
void GETransformation::SetScale(float inScale)
{
    this->SetScale(inScale, inScale, inScale);
}

// FUNCTION: LLE 0x1000d98d
// Returns the size of a transformation in bytes
unsigned long GETransformation::SizeOf()
{
    return sizeof(GETransformation);
}

// FUNCTION: LLE 0x1000d991
// Moves the transformation's position by a vector
void GETransformation::Translate(const GEVector& inVector)
{
    this->Translate(inVector.GetX(), inVector.GetY(), inVector.GetZ());
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x1000dccd
// TEMPLATE: LLE 0x1000dd17
// TEMPLATE: LLE 0x1000dd42
// TEMPLATE: LLE 0x1000dd6d

const UString GE3DPoint::mPointXStringID("GE3DPoint-PointX");
const UString GE3DPoint::mPointYStringID("GE3DPoint-PointY");
const UString GE3DPoint::mPointZStringID("GE3DPoint-PointZ");

// FUNCTION: LLE 0x1000da96
// Creates a point at a transformation's position
GE3DPoint::GE3DPoint(const GETransformation& inTransformation)
    : mX(inTransformation.GetX()), mY(inTransformation.GetY()), mZ(inTransformation.GetZ())
{
}

// FUNCTION: LLE 0x1000dab0
// Tells whether the point equals a transformation's position (exact compare)
bool GE3DPoint::operator==(const GETransformation& inTransformation) const
{
    return this->GetX() == inTransformation.GetX() && this->GetY() == inTransformation.GetY() &&
           this->GetZ() == inTransformation.GetZ();
}

// FUNCTION: LLE 0x1000dae4
// Fills outPoints with points every inStep along the segment to inPoint, always ending with inPoint itself
void GE3DPoint::InterpolateTo(const GE3DPoint& inPoint, float inStep, std::vector<GE3DPoint>& outPoints) const
{
    // (points within 0.0001 of each other but not equal: the C++ read an uninitialized end, Rust takes 0)
    GE3DPoint theFrom(*this);
    GE3DPoint theTo(inPoint);   // copies: either may be an element of outPoints
    outPoints.clear();
    ee_ge_3d_interpolate(GERust(theFrom), GERust(theTo), inStep, GERustPush3D, &outPoints);
}

// FUNCTION: LLE 0x1000dc23
// Reads x, y and z from an open file; 4 when the file is not open, else the first read error
long GE3DPoint::Load(FSFile& inFile)
{
    long theResult;
    if (inFile.GetIsOpen()) {
        theResult = inFile.Read(this->mX);
        if (theResult == 0) {
            theResult = inFile.Read(this->mY);
            if (theResult == 0)
                theResult = inFile.Read(this->mZ);
        }
    } else
        theResult = 4;
    return theResult;
}

// Header-inline/COMDAT members this unit emits (markers at the end)
// FUNCTION: LLE 0x1000dc66

// Init chains (names in analysis/lle/names.txt; markers at the end)
// SYNTHETIC: LLE 0x1000d9d3
// SYNTHETIC: LLE 0x1000d9dd
// SYNTHETIC: LLE 0x1000d9ed
// SYNTHETIC: LLE 0x1000d9f9
// SYNTHETIC: LLE 0x1000da14
// SYNTHETIC: LLE 0x1000da1e
// SYNTHETIC: LLE 0x1000da2e
// SYNTHETIC: LLE 0x1000da3a
// SYNTHETIC: LLE 0x1000da55
// SYNTHETIC: LLE 0x1000da5f
// SYNTHETIC: LLE 0x1000da6f
// SYNTHETIC: LLE 0x1000da7b

// ---- Exported members with no address of their own: ICF-folded into GEVector::Load 0xdc23 / Save 0x20dc6 and the
// empty `ret 4` 0x58f8e
long GE3DPoint::Save(FSFile& inFile) const
{
    long theResult;
    if (inFile.GetIsOpen()) {
        theResult = inFile.Write(this->mX);
        if (theResult == 0) {
            theResult = inFile.Write(this->mY);
            if (theResult == 0)
                theResult = inFile.Write(this->mZ);
        }
    } else
        theResult = 4;
    return theResult;
}

void GE3DPoint::GetChecksum(ESChecksum& ioChecksum) const
{
}

// TEMPLATE: LLE 0x1000dc92
