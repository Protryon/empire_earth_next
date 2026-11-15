// C:\EEX\Low-Level Engine\Graphics Engine\GE3DLine.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// Opens with GE3DPlane's ctors and the shared Define(point, vector) body: this unit is the first in link order to
// emit them (see GE2DPoint.cpp for the same pattern with the basic types).
// The intersections and the axis solvers forward to Rust (rust/crates/ee_ge line.rs; GERustMath.h); the plane's
// constructors and the file I/O stay here.
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
#include "GERustMath.h"

// FUNCTION: LLE 0x1000cb72
// Creates a plane through the origin facing +z
GE3DPlane::GE3DPlane()
{
    this->mNormal.SetZ(1.0f);
}

// FUNCTION: LLE 0x1000cb97
// Creates the plane through three points
GE3DPlane::GE3DPlane(const GE3DPoint& inPoint1, const GE3DPoint& inPoint2, const GE3DPoint& inPoint3)
{
    this->Define(inPoint1, inPoint2, inPoint3);
}

// FUNCTION: LLE 0x1000cbce
// GE3DLine::Define(const GE3DPoint&, const GEVector&) is header-inline (GE3DLine.h); its exported copy is ICF-folded
// with GE3DPlane::Define(point, vector) here.

// FUNCTION: LLE 0x1000cbfa
// Tests whether the line passes within inTolerance of a point; outT gets the nearer of the two crossing parameters
bool GE3DLine::Intersects(const GE3DPoint& inPoint, float inTolerance, float& outT) const
{
    return ee_ge_line_intersects_point(GERust(*this), GERust(inPoint), inTolerance, &outT);
}

// FUNCTION: LLE 0x1000cd82
// Tests whether two 3D lines meet (within 0.05 in the third axis); outT/outLineT get the parameters on each line
bool GE3DLine::Intersects(const GE3DLine& inLine, float& outT, float& outLineT) const
{
    return ee_ge_line_intersects_line(GERust(*this), GERust(inLine), &outT, &outLineT);
}

// FUNCTION: LLE 0x1000d000
// Intersects two lines in the xy plane; false when they are parallel, else the parameters on each line
bool GE3DLine::IntersectsIn2D(const GE3DLine& inLine, float& outT, float& outLineT) const
{
    return ee_ge_line_intersects_2d(GERust(*this), GERust(inLine), &outT, &outLineT);
}

// FUNCTION: LLE 0x1000d074
// Intersects the line with a plane; false when parallel, else outT gets the parameter of the crossing point
bool GE3DLine::Intersects(const GE3DPlane& inPlane, float& outT) const
{
    return ee_ge_line_intersects_plane(GERust(*this), GERust(inPlane), &outT);
}

// FUNCTION: LLE 0x1000d0e3
// Finds the parameter where the line reaches a given x; false when the line runs parallel to x
bool GE3DLine::SolveForX(float inX, float& outT) const
{
    return ee_ge_line_solve(GERust(*this), 0, inX, &outT, NULL);
}

// FUNCTION: LLE 0x1000d10c
// Finds the parameter and the point where the line reaches a given x; false when it runs parallel to x
bool GE3DLine::SolveForX(float inX, float& outT, GE3DPoint& outPoint) const
{
    return ee_ge_line_solve(GERust(*this), 0, inX, &outT, GERust(outPoint));
}

// FUNCTION: LLE 0x1000d159
// Finds the parameter where the line reaches a given y; false when the line runs parallel to y
bool GE3DLine::SolveForY(float inY, float& outT) const
{
    return ee_ge_line_solve(GERust(*this), 1, inY, &outT, NULL);
}

// FUNCTION: LLE 0x1000d183
// Finds the parameter and the point where the line reaches a given y; false when it runs parallel to y
bool GE3DLine::SolveForY(float inY, float& outT, GE3DPoint& outPoint) const
{
    return ee_ge_line_solve(GERust(*this), 1, inY, &outT, GERust(outPoint));
}

// FUNCTION: LLE 0x1000d1d4
// Finds the parameter where the line reaches a given z; false when the line runs parallel to z
bool GE3DLine::SolveForZ(float inZ, float& outT) const
{
    return ee_ge_line_solve(GERust(*this), 2, inZ, &outT, NULL);
}

// FUNCTION: LLE 0x1000d1fe
// Finds the parameter and the point where the line reaches a given z; false when it runs parallel to z
bool GE3DLine::SolveForZ(float inZ, float& outT, GE3DPoint& outPoint) const
{
    return ee_ge_line_solve(GERust(*this), 2, inZ, &outT, GERust(outPoint));
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// ---- Exported members with no address of their own (ICF-folded into GE3DPlane::Load/Save 0xd52f/0xd54f)
long GE3DLine::Load(FSFile& inFile)
{
    long theResult = this->mStart.Load(inFile);
    if (theResult == kErrorOK)
        theResult = this->mDirection.Load(inFile);
    return theResult;
}

long GE3DLine::Save(FSFile& inFile) const
{
    long theResult = this->mStart.Save(inFile);
    if (theResult == kErrorOK)
        theResult = this->mDirection.Save(inFile);
    return theResult;
}
