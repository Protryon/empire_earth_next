// C:\EEX\Low-Level Engine\Graphics Engine\GE3DPlane.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// Define, the intersection and the point-side tests forward to Rust (rust/crates/ee_ge line.rs; GERustMath.h); the
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

// FUNCTION: LLE 0x1000d24d
// Defines the plane through three points (first point plus the unit normal of their cross product)
void GE3DPlane::Define(const GE3DPoint& inPoint1, const GE3DPoint& inPoint2, const GE3DPoint& inPoint3)
{
    ee_ge_plane_define(GERust(inPoint1), GERust(inPoint2), GERust(inPoint3), GERust(*this));
}

// FUNCTION: LLE 0x1000d2c7
// Finds the line where this plane meets another; false when the planes are parallel
bool GE3DPlane::Intersects(const GE3DPlane& inPlane, GE3DLine& outLine) const
{
    // (degenerate normals: the C++ read an uninitialized parameter, Rust takes 0)
    return ee_ge_plane_intersects(GERust(*this), GERust(inPlane), GERust(outLine));
}

// FUNCTION: LLE 0x1000d3a7
// Tells whether a point lies strictly on the normal's side of the plane
bool GE3DPlane::IsPointAbovePlane(const GE3DPoint& inPoint) const
{
    return ee_ge_plane_side(GERust(*this), GERust(inPoint), 0);
}

// FUNCTION: LLE 0x1000d3ee
// Tells whether a point lies strictly behind the plane
bool GE3DPlane::IsPointBelowPlane(const GE3DPoint& inPoint) const
{
    return ee_ge_plane_side(GERust(*this), GERust(inPoint), 1);
}

// FUNCTION: LLE 0x1000d435
// Tells whether a point lies on the plane or on the normal's side
bool GE3DPlane::IsPointOnOrAbovePlane(const GE3DPoint& inPoint) const
{
    return ee_ge_plane_side(GERust(*this), GERust(inPoint), 2);
}

// FUNCTION: LLE 0x1000d47c
// Tells whether a point lies on the plane or behind it
bool GE3DPlane::IsPointOnOrBelowPlane(const GE3DPoint& inPoint) const
{
    return ee_ge_plane_side(GERust(*this), GERust(inPoint), 3);
}

// FUNCTION: LLE 0x1000d4c3
// Tells whether a point lies on the plane (direction from the plane point within 1e-6 of perpendicular to the normal)
bool GE3DPlane::IsPointOnPlane(const GE3DPoint& inPoint) const
{
    return ee_ge_plane_side(GERust(*this), GERust(inPoint), 4);
}

// FUNCTION: LLE 0x1000d52f
// Loads the plane's point and normal from a file; returns the first read error
long GE3DPlane::Load(FSFile& inFile)
{
    long theResult = this->mPoint.Load(inFile);
    if (theResult == 0)
        theResult = this->mNormal.Load(inFile);
    return theResult;
}

// FUNCTION: LLE 0x1000d54f
// Saves the plane's point and normal to a file; returns the first write error
long GE3DPlane::Save(FSFile& inFile)
{
    long theResult = this->mPoint.Save(inFile);
    if (theResult == 0)
        theResult = this->mNormal.Save(inFile);
    return theResult;
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// ---- Exported members with no address of their own (ICF-folded into GE3DLine's identical bodies 0xc61f/0xc66a)
GE3DPlane::GE3DPlane(const GE3DPoint& inPoint, const GEVector& inNormal)
{
    this->mPoint = inPoint;
    this->mNormal = inNormal;
}

GE3DPlane& GE3DPlane::operator=(const GE3DPlane& inPlane)
{
    this->mPoint = inPlane.mPoint;
    this->mNormal = inPlane.mNormal;
    return *this;
}
