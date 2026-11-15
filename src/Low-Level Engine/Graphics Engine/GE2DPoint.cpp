// C:\EEX\Low-Level Engine\Graphics Engine\GE2DPoint.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1000c129..0x1000c74b: the basic GE operations, in header include order (UMFastMath, GE2DPoint, GEVector,
// GE3DPoint, GE3DLine). Their emission order looks like dllexport header COMDATs, but the plug-ins import several
// of them (see lle/GE2DPoint.h), so they are defined here; the bytes are the same either way.
// The distances, the side test, the cross product, the magnitudes, RotatePoint and InterpolateTo forward to Rust
// (rust/crates/ee_ge: point.rs, vector.rs; GERustMath.h); the component-wise operators and the file I/O stay here.
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
// Creates a 2D point from integer coordinates
GE2DPoint::GE2DPoint(long inX, long inY)
    : mX((float)inX), mY((float)inY)
{
}

// Returns the 2D cross product that tells which side of the line inFrom-inTo this point lies on
float GE2DPoint::GetDirection(const GE2DPoint& inFrom, const GE2DPoint& inTo) const
{
    return ee_ge_2d_direction(GERust(*this), GERust(inFrom), GERust(inTo));
}

// Returns the distance to another 2D point (fast square root)
float GE2DPoint::DistanceTo(const GE2DPoint& inPoint) const
{
    return ee_ge_2d_distance(GERust(*this), GERust(inPoint));
}

// Returns the squared distance to another 2D point
float GE2DPoint::DistanceToSquared(const GE2DPoint& inPoint) const
{
    return ee_ge_2d_distance_squared(GERust(*this), GERust(inPoint));
}

// Returns the vector pointing the opposite way
GEVector GEVector::operator-() const
{
    GEVector theResult(-this->mX, -this->mY, -this->mZ);
    return theResult;
}

// Subtracts another vector from this one
GEVector& GEVector::operator-=(const GEVector& inVector)
{
    this->mX -= inVector.mX;
    this->mY -= inVector.mY;
    this->mZ -= inVector.mZ;
    return *this;
}

// Tells whether two vectors differ in any component (exact compare)
bool GEVector::operator!=(const GEVector& inVector) const
{
    return this->mX != inVector.mX || this->mY != inVector.mY || this->mZ != inVector.mZ;
}

// Writes the sum of this vector and another to outVector
void GEVector::Add(const GEVector& inVector, GEVector& outVector) const
{
    outVector.mX = inVector.mX + this->mX;
    outVector.mY = inVector.mY + this->mY;
    outVector.mZ = inVector.mZ + this->mZ;
}

// Returns the vector's length (fast square root)
float GEVector::GetMagnitude() const
{
    return ee_ge_vector_magnitude(GERust(*this));
}

// Returns the vector's squared length
float GEVector::GetMagnitudeSquared() const
{
    return ee_ge_vector_magnitude_squared(GERust(*this));
}

// Adds another vector to this one
GEVector& GEVector::operator+=(const GEVector& inVector)
{
    this->mX += inVector.mX;
    this->mY += inVector.mY;
    this->mZ += inVector.mZ;
    return *this;
}

// Moves the point back by a vector
GE3DPoint& GE3DPoint::operator-=(const GEVector& inVector)
{
    this->mX -= inVector.GetX();
    this->mY -= inVector.GetY();
    this->mZ -= inVector.GetZ();
    return *this;
}

// Tells whether the point's x and y equal a 2D point's (z ignored)
bool GE3DPoint::operator==(const GE2DPoint& inPoint) const
{
    return inPoint.mX == this->mX && inPoint.mY == this->mY;
}

// Moves the point to the origin
void GE3DPoint::Clear()
{
    this->mX = 0.0f;
    this->mY = 0.0f;
    this->mZ = 0.0f;
}

// Returns the distance to another 3D point (fast square root)
float GE3DPoint::DistanceTo(const GE3DPoint& inPoint) const
{
    return ee_ge_3d_distance(GERust(*this), GERust(inPoint));
}

// Returns the squared distance to another 3D point
float GE3DPoint::DistanceToSquared(const GE3DPoint& inPoint) const
{
    return ee_ge_3d_distance_squared(GERust(*this), GERust(inPoint));
}

// Loads the point's coordinates from a file (the version arguments are ignored)
long GE3DPoint::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return this->Load(inFile);
}

// Saves the point's coordinates to a file (the version arguments are ignored)
long GE3DPoint::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) const
{
    return this->Save(inFile);
}

// Sets the point's three coordinates
void GE3DPoint::SetXYZ(float inX, float inY, float inZ)
{
    this->mX = inX;
    this->mY = inY;
    this->mZ = inZ;
}

// Creates a line from a start point through an end point
GE3DLine::GE3DLine(const GE3DPoint& inStart, const GE3DPoint& inEnd)
{
    this->mStart = inStart;
    this->ComputeDirection(inEnd);
}

// Creates a line from a start point and a direction vector
GE3DLine::GE3DLine(const GE3DPoint& inStart, const GEVector& inDirection)
{
    this->mStart = inStart;
    this->mDirection = inDirection;
}

// Copies another line's start and direction
GE3DLine& GE3DLine::operator=(const GE3DLine& inLine)
{
    this->mStart = inLine.mStart;
    this->mDirection = inLine.mDirection;
    return *this;
}

// Writes the point at parameter t along the line (start + t * direction) to outPoint
void GE3DLine::GetPointAt(float inT, GE3DPoint& outPoint) const
{
    outPoint.mX = this->mDirection.GetX() * inT + this->mStart.GetX();
    outPoint.mY = this->mDirection.GetY() * inT + this->mStart.GetY();
    outPoint.mZ = this->mDirection.GetZ() * inT + this->mStart.GetZ();
}

// Sets the direction to the vector from the start point to inEnd
void GE3DLine::ComputeDirection(const GE3DPoint& inEnd)
{
    this->mDirection.mX = inEnd.GetX() - this->mStart.GetX();
    this->mDirection.mY = inEnd.GetY() - this->mStart.GetY();
    this->mDirection.mZ = inEnd.GetZ() - this->mStart.GetZ();
}

// Creates a copy of a line
GE3DLine::GE3DLine(const GE3DLine& inLine)
    : mStart(inLine.mStart), mDirection(inLine.mDirection)
{
}

const UString GE2DPoint::sPointCoordXStringID("GE2DPoint-X");
const UString GE2DPoint::sPointCoordYStringID("GE2DPoint-Y");
const UString GE2DPoint::sPointCoordZStringID("GE2DPoint-Z");

// Fills outPoints with points every inStep along the segment to inPoint, always ending with inPoint itself
void GE2DPoint::InterpolateTo(const GE2DPoint& inPoint, float inStep, std::vector<GE2DPoint>& outPoints) const
{
    // (points within 0.0001 of each other but not equal: the C++ read an uninitialized end, Rust takes 0)
    GE2DPoint theFrom(*this);
    GE2DPoint theTo(inPoint);   // copies: either may be an element of outPoints
    outPoints.clear();
    ee_ge_2d_interpolate(GERust(theFrom), GERust(theTo), inStep, GERustPush2D, &outPoints);
}

// Reads x and y from an open file; 4 when the file is not open, else the first read error
long GE2DPoint::Load(FSFile& inFile)
{
    long theResult;
    if (inFile.GetIsOpen()) {
        theResult = inFile.Read(this->mX);
        if (theResult == 0)
            theResult = inFile.Read(this->mY);
    } else
        theResult = 4;
    return theResult;
}

// Writes x and y to an open file; 4 when the file is not open, else the first write error
long GE2DPoint::Save(FSFile& inFile)
{
    long theResult;
    if (inFile.GetIsOpen()) {
        theResult = inFile.Write(this->mX);
        if (theResult == 0)
            theResult = inFile.Write(this->mY);
    } else
        theResult = 4;
    return theResult;
}

// ---- Exported members with no address of their own: the linker's ICF folded each into an identical body above
// (GEVector::operator+= 0xc41a, GE3DPoint::operator-=(GEVector) 0xc43b, GEVector::Add 0xc2cc, the GE3DPoint ==
// 0xc47e). Natural out-of-line definitions; no markers.
GE3DPoint& GE3DPoint::operator+=(const GE3DPoint& inPoint)
{
    this->mX += inPoint.mX;
    this->mY += inPoint.mY;
    this->mZ += inPoint.mZ;
    return *this;
}

GE3DPoint& GE3DPoint::operator+=(const GEVector& inVector)
{
    this->mX += inVector.GetX();
    this->mY += inVector.GetY();
    this->mZ += inVector.GetZ();
    return *this;
}

GE3DPoint& GE3DPoint::operator-=(const GE3DPoint& inPoint)
{
    this->mX -= inPoint.mX;
    this->mY -= inPoint.mY;
    this->mZ -= inPoint.mZ;
    return *this;
}

void GE3DPoint::Add(const GEVector& inVector, GE3DPoint& outPoint) const
{
    outPoint.mX = inVector.GetX() + this->mX;
    outPoint.mY = inVector.GetY() + this->mY;
    outPoint.mZ = inVector.GetZ() + this->mZ;
}

bool GEVector::operator==(const GEVector& inVector) const
{
    return this->mX == inVector.mX && this->mY == inVector.mY && this->mZ == inVector.mZ;
}
