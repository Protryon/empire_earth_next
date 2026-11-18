// GEValues.cpp (port, R4.3 part 16): the Graphics Engine's value types' out-of-line members, one unit for what were
// fifteen (GE3DPoint, GEVector, GE3DLine, GE3DPlane, GETransformation, GEOrientation, GEQuaternion, GEBoundingBox,
// GEViewport, GERectangle, GEColor, GELight, GETextureCoordinate, GEVertexArray, GEPolygonArray; their address
// markers went with them). The algorithms are Rust (rust/crates/ee_render/src/ge: point.rs, vector.rs, line.rs, transform.rs,
// bbox.rs, viewport.rs, values.rs, io.rs; GERustMath.h, GERustIO.h, GERustViewport.h); these are the forwarders, the
// statics (string ids, the quantized vector table) and the file I/O. The simulation reads these types in nearly every
// unit: the members are the same statements as before (every unit compiles with -ffp-contract=off, tools/ee.py).
#include <windows.h>
#include <math.h>
#include <string.h>
#include <vector>

#define LLE_GEVECTOR_CPP    // GEVector's statics are visible here only (see lle/GEVector.h) (provisional name)
#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/GE2DPoint.h"
#include "lle/GERectangle.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/FSFile.h"
#include "GERustMath.h"
#include "GERustIO.h"
#include "lle/UWideString.h"
#include "lle/FSMemoryFile.h"
#include "lle/UMFastMath.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GEQuaternion.h"
#include "lle/URandom.h"
#include "lle/TSScheduler.h"
#include "lle/UReferenceCounter.h"
#include "lle/FSFileSpec.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GE2DVertex.h"
#include "lle/GEBitmap.h"
#include "lle/GEBoundingBox.h"
#include "lle/GEFont.h"
#include "lle/GELight.h"
#include "lle/GELine.h"
#include "lle/GEPolygonGroup.h"
#include "lle/GEMaterial.h"
#include "lle/GEPolygon.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEVertex.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "lle/GEMovie.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GEPoint.h"
#include "lle/GERhombus.h"
#include "lle/GETextureSet.h"
#include "lle/U2DFractal.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEText.h"
#include "GERustViewport.h"

// ---- GEVector (GEVector.cpp, folded here in R4.3 part 16)

// The unit cube's six faces (-X, -Y, -Z, +X, +Y, +Z), each sampled on a 41 x 41 grid of normalized directions.
// (The original also kept the six face planes here, +0x1d8c8; the Rust search has its own.)
struct GEVector::QuantizedVectorArray {
    QuantizedVectorArray();
    GEVector mVectors[41][6 * 41];          // +0x00000
};
const UString GEVector::sVectorXStringID("GEVector-VectorX");
const UString GEVector::sVectorYStringID("GEVector-VectorY");
const UString GEVector::sVectorZStringID("GEVector-VectorZ");
const UString GEVector::sQuantizedVectorIndexStringID("GEVector-QuantizedVectorIndex");
GEVector::QuantizedVectorArray GEVector::sQuantizedVectorArray;
static GEVector sZeroVector;                // 0x1010ff60: the quantized version of (0, 0, 0) (provisional name)
// Builds the table of quantized unit directions (a 41 x 41 grid on each cube face)
GEVector::QuantizedVectorArray::QuantizedVectorArray()
{
    ee_ge_vector_quantized_table(GERust(this->mVectors[0][0]), sizeof(this->mVectors) / sizeof(GEVector));
}

// (0x10035ea3, the destructor of the six face planes: gone with them)

// Returns the nearest entry of the quantized direction table: the cube face the vector's ray hits,
// then the grid cell there (the shared zero vector for (0, 0, 0))
GEVector* GEVector::GetQuantizedVersion() const
{
    GEVector* theQuantizedVector = NULL;
    long theIndex = ee_ge_vector_quantized_index(GERust(*this));
    if (theIndex >= 0)
        theQuantizedVector = &GEVector::sQuantizedVectorArray.mVectors[0][0] + theIndex;
    else if (theIndex == -1)
        theQuantizedVector = &sZeroVector;
    ESVERIFY_AT(theQuantizedVector != NULL, 383);
    return theQuantizedVector;
}

// The zero vector's destructor (the members below have no address of their own)
// Destroys the file-static zero vector at shutdown (an empty body)

float GEVector::GetMagnitude() const { return ee_ge_vector_magnitude(GERust(*this)); }
float GEVector::GetMagnitudeSquared() const { return ee_ge_vector_magnitude_squared(GERust(*this)); }

// ---- Exported members with no address of their own (ICF-folded: Load 0xdc23, Save 0x20dc6)
long GEVector::Load(FSFile& inFile)
{
    const float* theFields[] = {&this->mX, &this->mY, &this->mZ};
    return GERustFields(inFile, theFields, 3, inFile.GetIsOpen(), false, false);
}

long GEVector::Save(FSFile& inFile) const
{
    const float* theFields[] = {&this->mX, &this->mY, &this->mZ};
    return GERustFields(inFile, theFields, 3, inFile.GetIsOpen(), true, false);
}

// ---- GE3DPoint (GE3DPoint.cpp, folded here in R4.3 part 16)

// Returns the distance from the transformation's position to a point (full-precision square root)
float GETransformation::DistanceTo(const GE3DPoint& inPoint) const
{
    return ee_ge_t_distance_to_point(GERust(*this), GERust(inPoint));
}

// Returns the distance between two transformations' positions (fast square root)
float GETransformation::DistanceTo(const GETransformation& inTransformation) const
{
    return ee_ge_t_distance_to(GERust(*this), GERust(inTransformation));
}

// Returns the squared distance from the transformation's position to a point
float GETransformation::DistanceSquaredTo(const GE3DPoint& inPoint) const
{
    return ee_ge_t_distance_squared_to_point(GERust(*this), GERust(inPoint));
}

// Returns the squared distance between two transformations' positions
float GETransformation::DistanceSquaredTo(const GETransformation& inTransformation) const
{
    return ee_ge_t_distance_squared_to(GERust(*this), GERust(inTransformation));
}

// Returns the transformation's position as a point
GE3DPoint GETransformation::GetPosition() const
{
    return GE3DPoint(this->GetX(), this->GetY(), this->GetZ());
}

// Writes the transformation's position to outPosition
void GETransformation::GetPosition(GE3DPoint& outPosition) const
{
    outPosition.mX = this->GetX();
    outPosition.mY = this->GetY();
    outPosition.mZ = this->GetZ();
}

// Returns the yaw angle taken from the matrix
float GETransformation::GetYaw() const
{
    return ee_ge_t_yaw(GERust(*this));
}

// Returns the pitch angle taken from the matrix
float GETransformation::GetPitch() const
{
    return ee_ge_t_pitch(GERust(*this));
}

// Returns the roll angle taken from the matrix
float GETransformation::GetRoll() const
{
    return ee_ge_t_roll(GERust(*this));
}

// Revolves the transformation by an orientation's yaw, pitch and roll
void GETransformation::Revolve(const GEOrientation& inOrientation)
{
    this->Revolve(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// Rotates the transformation by an orientation's yaw, pitch and roll
void GETransformation::Rotate(const GEOrientation& inOrientation)
{
    this->Rotate(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// Sets the rotation from an orientation, applied pitch, roll, yaw
void GETransformation::SetOrientationPRY(const GEOrientation& inOrientation)
{
    this->SetOrientationPRY(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// Sets the rotation from an orientation, applied pitch, yaw, roll
void GETransformation::SetOrientationPYR(const GEOrientation& inOrientation)
{
    this->SetOrientationPYR(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// Sets the rotation from an orientation, applied roll, pitch, yaw
void GETransformation::SetOrientationRPY(const GEOrientation& inOrientation)
{
    this->SetOrientationRPY(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// Sets the rotation from an orientation, applied roll, yaw, pitch
void GETransformation::SetOrientationRYP(const GEOrientation& inOrientation)
{
    this->SetOrientationRYP(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// Sets the rotation from an orientation, applied yaw, pitch, roll
void GETransformation::SetOrientationYPR(const GEOrientation& inOrientation)
{
    this->SetOrientationYPR(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// Sets the rotation from an orientation, applied yaw, roll, pitch
void GETransformation::SetOrientationYRP(const GEOrientation& inOrientation)
{
    this->SetOrientationYRP(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll());
}

// Moves the transformation's position to a point
void GETransformation::SetPosition(const GE3DPoint& inPosition)
{
    this->SetPosition(inPosition.GetX(), inPosition.GetY(), inPosition.GetZ());
}

// Sets the same scale on all three axes
void GETransformation::SetScale(float inScale)
{
    this->SetScale(inScale, inScale, inScale);
}

// Returns the size of a transformation in bytes
unsigned long GETransformation::SizeOf()
{
    return sizeof(GETransformation);
}

// Moves the transformation's position by a vector
void GETransformation::Translate(const GEVector& inVector)
{
    this->Translate(inVector.GetX(), inVector.GetY(), inVector.GetZ());
}

const UString GE3DPoint::mPointXStringID("GE3DPoint-PointX");
const UString GE3DPoint::mPointYStringID("GE3DPoint-PointY");
const UString GE3DPoint::mPointZStringID("GE3DPoint-PointZ");

// Creates a point at a transformation's position
GE3DPoint::GE3DPoint(const GETransformation& inTransformation)
    : mX(inTransformation.GetX()), mY(inTransformation.GetY()), mZ(inTransformation.GetZ())
{
}

// Tells whether the point equals a transformation's position (exact compare)
bool GE3DPoint::operator==(const GETransformation& inTransformation) const
{
    return this->GetX() == inTransformation.GetX() && this->GetY() == inTransformation.GetY() &&
           this->GetZ() == inTransformation.GetZ();
}

// Fills outPoints with points every inStep along the segment to inPoint, always ending with inPoint itself
void GE3DPoint::InterpolateTo(const GE3DPoint& inPoint, float inStep, std::vector<GE3DPoint>& outPoints) const
{
    // (points within 0.0001 of each other but not equal: the C++ read an uninitialized end, Rust takes 0)
    GE3DPoint theFrom(*this);
    GE3DPoint theTo(inPoint);   // copies: either may be an element of outPoints
    outPoints.clear();
    ee_ge_3d_interpolate(GERust(theFrom), GERust(theTo), inStep, GERustPush3D, &outPoints);
}

// Reads x, y and z from an open file; 4 when the file is not open, else the first read error
// ---- GE2DPoint.cpp's (R4.3 part 15: the unit deleted, its component-wise members inline in the headers)
// The side of the line inFrom-inTo this point lies on (the 2D cross product)
float GE2DPoint::GetDirection(const GE2DPoint& inFrom, const GE2DPoint& inTo) const
{
    return ee_ge_2d_direction(GERust(*this), GERust(inFrom), GERust(inTo));
}

float GE2DPoint::DistanceTo(const GE2DPoint& inPoint) const { return ee_ge_2d_distance(GERust(*this), GERust(inPoint)); }
float GE2DPoint::DistanceToSquared(const GE2DPoint& inPoint) const { return ee_ge_2d_distance_squared(GERust(*this), GERust(inPoint)); }
float GE3DPoint::DistanceTo(const GE3DPoint& inPoint) const { return ee_ge_3d_distance(GERust(*this), GERust(inPoint)); }
float GE3DPoint::DistanceToSquared(const GE3DPoint& inPoint) const { return ee_ge_3d_distance_squared(GERust(*this), GERust(inPoint)); }

// Points every inStep along the segment to inPoint, always ending with inPoint itself
void GE2DPoint::InterpolateTo(const GE2DPoint& inPoint, float inStep, std::vector<GE2DPoint>& outPoints) const
{
    GE2DPoint theFrom(*this);
    GE2DPoint theTo(inPoint);   // copies: either may be an element of outPoints
    outPoints.clear();
    ee_ge_2d_interpolate(GERust(theFrom), GERust(theTo), inStep, GERustPush2D, &outPoints);
}

// x and y from (to) an open file; 4 when the file is not open, else the first read (write) error
long GE2DPoint::Load(FSFile& inFile)
{
    const float* theFields[] = {&this->mX, &this->mY};
    return GERustFields(inFile, theFields, 2, inFile.GetIsOpen(), false, false);
}

long GE2DPoint::Save(FSFile& inFile)
{
    const float* theFields[] = {&this->mX, &this->mY};
    return GERustFields(inFile, theFields, 2, inFile.GetIsOpen(), true, false);
}

long GE3DPoint::Load(FSFile& inFile)
{
    const float* theFields[] = {&this->mX, &this->mY, &this->mZ};
    return GERustFields(inFile, theFields, 3, inFile.GetIsOpen(), false, false);
}

// ---- Exported members with no address of their own: ICF-folded into GEVector::Load 0xdc23 / Save 0x20dc6 and the
// empty `ret 4` 0x58f8e
long GE3DPoint::Save(FSFile& inFile) const
{
    const float* theFields[] = {&this->mX, &this->mY, &this->mZ};
    return GERustFields(inFile, theFields, 3, inFile.GetIsOpen(), true, false);
}

void GE3DPoint::GetChecksum(ESChecksum& ioChecksum) const
{
}

// ---- GE3DLine (GE3DLine.cpp, folded here in R4.3 part 16)

// Creates a plane through the origin facing +z
GE3DPlane::GE3DPlane()
{
    this->mNormal.SetZ(1.0f);
}

// Creates the plane through three points
GE3DPlane::GE3DPlane(const GE3DPoint& inPoint1, const GE3DPoint& inPoint2, const GE3DPoint& inPoint3)
{
    this->Define(inPoint1, inPoint2, inPoint3);
}

// GE3DLine::Define(const GE3DPoint&, const GEVector&) is header-inline (GE3DLine.h); its exported copy is ICF-folded
// with GE3DPlane::Define(point, vector) here.

// Tests whether the line passes within inTolerance of a point; outT gets the nearer of the two crossing parameters
bool GE3DLine::Intersects(const GE3DPoint& inPoint, float inTolerance, float& outT) const
{
    return ee_ge_line_intersects_point(GERust(*this), GERust(inPoint), inTolerance, &outT);
}

// Tests whether two 3D lines meet (within 0.05 in the third axis); outT/outLineT get the parameters on each line
bool GE3DLine::Intersects(const GE3DLine& inLine, float& outT, float& outLineT) const
{
    return ee_ge_line_intersects_line(GERust(*this), GERust(inLine), &outT, &outLineT);
}

// Intersects the line with a plane; false when parallel, else outT gets the parameter of the crossing point
bool GE3DLine::Intersects(const GE3DPlane& inPlane, float& outT) const
{
    return ee_ge_line_intersects_plane(GERust(*this), GERust(inPlane), &outT);
}

// Finds the parameter where the line reaches a given x; false when the line runs parallel to x
bool GE3DLine::SolveForX(float inX, float& outT) const
{
    return ee_ge_line_solve(GERust(*this), 0, inX, &outT, NULL);
}

// Finds the parameter and the point where the line reaches a given x; false when it runs parallel to x
bool GE3DLine::SolveForX(float inX, float& outT, GE3DPoint& outPoint) const
{
    return ee_ge_line_solve(GERust(*this), 0, inX, &outT, GERust(outPoint));
}

// Finds the parameter where the line reaches a given y; false when the line runs parallel to y
bool GE3DLine::SolveForY(float inY, float& outT) const
{
    return ee_ge_line_solve(GERust(*this), 1, inY, &outT, NULL);
}

// Finds the parameter and the point where the line reaches a given y; false when it runs parallel to y
bool GE3DLine::SolveForY(float inY, float& outT, GE3DPoint& outPoint) const
{
    return ee_ge_line_solve(GERust(*this), 1, inY, &outT, GERust(outPoint));
}

// Finds the parameter where the line reaches a given z; false when the line runs parallel to z
bool GE3DLine::SolveForZ(float inZ, float& outT) const
{
    return ee_ge_line_solve(GERust(*this), 2, inZ, &outT, NULL);
}

// Finds the parameter and the point where the line reaches a given z; false when it runs parallel to z
bool GE3DLine::SolveForZ(float inZ, float& outT, GE3DPoint& outPoint) const
{
    return ee_ge_line_solve(GERust(*this), 2, inZ, &outT, GERust(outPoint));
}

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

// ---- GE3DPlane (GE3DPlane.cpp, folded here in R4.3 part 16)

// Defines the plane through three points (first point plus the unit normal of their cross product)
void GE3DPlane::Define(const GE3DPoint& inPoint1, const GE3DPoint& inPoint2, const GE3DPoint& inPoint3)
{
    ee_ge_plane_define(GERust(inPoint1), GERust(inPoint2), GERust(inPoint3), GERust(*this));
}

// Finds the line where this plane meets another; false when the planes are parallel
bool GE3DPlane::Intersects(const GE3DPlane& inPlane, GE3DLine& outLine) const
{
    // (degenerate normals: the C++ read an uninitialized parameter, Rust takes 0)
    return ee_ge_plane_intersects(GERust(*this), GERust(inPlane), GERust(outLine));
}

// Loads the plane's point and normal from a file; returns the first read error
long GE3DPlane::Load(FSFile& inFile)
{
    long theResult = this->mPoint.Load(inFile);
    if (theResult == 0)
        theResult = this->mNormal.Load(inFile);
    return theResult;
}

// Saves the plane's point and normal to a file; returns the first write error
long GE3DPlane::Save(FSFile& inFile)
{
    long theResult = this->mPoint.Save(inFile);
    if (theResult == 0)
        theResult = this->mNormal.Save(inFile);
    return theResult;
}

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

// ---- GETransformation (GETransformation.cpp, folded here in R4.3 part 16)

const UString GETransformation::sTransformationStringID("GETransformation-TransformationData");

// Creates an identity transformation
GETransformation::GETransformation()
{
    ee_ge_t_identity(GERust(*this));
}

GETransformation::GETransformation(const float inMatrix[4][4])
{
    ee_ge_t_copy(GERust(*this), reinterpret_cast<const float(*)[4][4]>(inMatrix));
}

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

// Returns whether all sixteen matrix elements equal the other transformation's
bool GETransformation::operator==(const GETransformation& inTransformation)
{
    return ee_ge_t_equal(GERust(*this), GERust(inTransformation));
}

// Returns whether the transformation's position equals the point
bool GETransformation::operator==(const GE3DPoint& inPoint) const
{
    return ee_ge_t_at_point(GERust(*this), GERust(inPoint));
}

// NEAR: one store of b23 is scheduled one instruction later in the original; tried declaration order, GetElement
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

// Transforms a plane (its point and normal) into outPlane
void GETransformation::Apply(const GE3DPlane& inPlane, GE3DPlane& outPlane) const
{
    ee_ge_t_apply_plane(GERust(*this), GERust(inPlane), GERust(outPlane));
}

// Extracts yaw, pitch and roll angles from the rotation part into an orientation
void GETransformation::ConvertToOrientation(GEOrientation& outOrientation) const
{
    ee_ge_t_orientation(GERust(*this), GERust(outOrientation));
}

// Reads the sixteen matrix elements from a file, stopping at the first error
long GETransformation::Load(FSFile& inFile)
{
    EeGeFieldIo theIO = GERustFieldIO(inFile);
    return ee_ge_t_io(GERust(*this), &theIO, false);
}

// Resets the transformation to identity
void GETransformation::MakeIdentity()
{
    ee_ge_t_identity(GERust(*this));
}

// Computes the inverse of a rigid transformation (transposed rotation, negated rotated position)
void GETransformation::MakeInverse(GETransformation& outInverse) const
{
    // element by element, as the C++ stored them (an output equal to this transposes through its own stores)
    ee_ge_t_make_inverse(GERust(*this), GERust(outInverse));
}

// Rescales the three rotation columns to unit length
void GETransformation::Normalize()
{
    ee_ge_t_normalize(GERust(*this));
}

// Rotates the transformation by yaw, pitch and roll applied after it (about its own axes)
void GETransformation::Revolve(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_revolve(GERust(*this), inYaw, inPitch, inRoll);
}

// Rotates the transformation by yaw, pitch and roll applied before it (about the parent axes)
void GETransformation::Rotate(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_rotate(GERust(*this), inYaw, inPitch, inRoll);
}

// Scales the transformation by x, y and z (scale applied before it)
void GETransformation::Scale(float inX, float inY, float inZ)
{
    ee_ge_t_scale(GERust(*this), inX, inY, inZ);
}

// Writes the sixteen matrix elements to a file, stopping at the first error
long GETransformation::Save(FSFile& inFile) const
{
    EeGeFieldIo theIO = GERustFieldIO(inFile);
    return ee_ge_t_io(const_cast<EeGeTransformation*>(GERust(*this)), &theIO, true);
}

// Sets the rotation from yaw, pitch and roll in pitch-roll-yaw order (position kept)
void GETransformation::SetOrientationPRY(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_set_orientation(GERust(*this), 0, inYaw, inPitch, inRoll);
}

// Sets the rotation from yaw, pitch and roll in pitch-yaw-roll order (position kept)
void GETransformation::SetOrientationPYR(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_set_orientation(GERust(*this), 1, inYaw, inPitch, inRoll);
}

// 95.18: register lifetimes of theRP01/theSinRoll differ (the other five orders are exact with the same shape);
// tried temp order, term order, -theSinRoll inline
// Sets the rotation from yaw, pitch and roll in roll-pitch-yaw order (position kept)
void GETransformation::SetOrientationRPY(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_set_orientation(GERust(*this), 2, inYaw, inPitch, inRoll);
}

// Sets the rotation from yaw, pitch and roll in roll-yaw-pitch order (position kept)
void GETransformation::SetOrientationRYP(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_set_orientation(GERust(*this), 3, inYaw, inPitch, inRoll);
}

// Sets the rotation from yaw, pitch and roll in yaw-pitch-roll order (position kept)
void GETransformation::SetOrientationYPR(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_set_orientation(GERust(*this), 4, inYaw, inPitch, inRoll);
}

// Sets the rotation from yaw, pitch and roll in yaw-roll-pitch order (position kept)
void GETransformation::SetOrientationYRP(float inYaw, float inPitch, float inRoll)
{
    ee_ge_t_set_orientation(GERust(*this), 5, inYaw, inPitch, inRoll);
}

// Makes the transformation a pure scale by x, y and z (position column kept)
void GETransformation::SetScale(float inX, float inY, float inZ)
{
    ee_ge_t_set_scale(GERust(*this), inX, inY, inZ);
}

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

// ---- GEOrientation (GEOrientation.cpp, folded here in R4.3 part 16)

long GEOrientation::Load(FSFile& inFile)
{
    const float* theFields[] = {&this->mYaw, &this->mPitch, &this->mRoll};
    return GERustFields(inFile, theFields, 3, inFile.GetIsOpen(), false, false);
}

// Wraps yaw, pitch and roll into the range -pi..pi
void GEOrientation::Normalize()
{
    ee_ge_orientation_normalize(GERust(*this));
}

// Writes yaw, pitch and roll to a file; kErrorNotOpen when the file is not open
long GEOrientation::Save(FSFile& inFile)
{
    const float* theFields[] = {&this->mYaw, &this->mPitch, &this->mRoll};
    return GERustFields(inFile, theFields, 3, inFile.GetIsOpen(), true, false);
}

// ---- Exported members with no address of their own: ICF-folded into GEVector's ctors (0xc240, 0xd56f), its
// operator= shape (0xd588), the empty dtor 0xc16a and the float setters 0xc56c/0xc576/0x54ae4
GEOrientation::GEOrientation()
    : mYaw(0.0f), mPitch(0.0f), mRoll(0.0f)
{
}

GEOrientation::GEOrientation(float inYaw, float inPitch, float inRoll)
    : mYaw(inYaw), mPitch(inPitch), mRoll(inRoll)
{
}

GEOrientation::GEOrientation(const GEOrientation& inOrientation)
{
    this->mYaw = inOrientation.mYaw;
    this->mPitch = inOrientation.mPitch;
    this->mRoll = inOrientation.mRoll;
}

GEOrientation::~GEOrientation()
{
}

void GEOrientation::SetPitch(float inPitch)
{
    this->mPitch = inPitch;
}

void GEOrientation::SetRoll(float inRoll)
{
    this->mRoll = inRoll;
}

void GEOrientation::SetYaw(float inYaw)
{
    this->mYaw = inYaw;
}

// ---- GEQuaternion (GEQuaternion.cpp, folded here in R4.3 part 16)

// Sets the quaternion from a transformation's rotation (vector part negated), then normalises it
void GEQuaternion::ConvertFrom(const GETransformation& inTransformation)
{
    ee_ge_quat_from(GERust(inTransformation), GERust(*this));
}

// Scales the quaternion to unit length
// NEAR: the inline UMFastMath::Sqrt gives the fstp st(4) parameter shape (plain sqrt is further off); the second
// push ecx of the double argument area is scheduled one load earlier than the original
void GEQuaternion::Normalize()
{
    ee_ge_quat_normalize(GERust(*this));
}

// ---- GEBoundingBox (GEBoundingBox.cpp, folded here in R4.3 part 16)

// Orders each axis so the first corner holds the minimum, then recomputes width (X), height (Z) and depth (Y)
void GEBoundingBox::ComputeDimensions()
{
    ee_ge_bbox_compute_dimensions(GERust(*this));
}

// Returns whether the point lies inside the box, with a 0.00001 tolerance on every face
bool GEBoundingBox::Contains(const GE3DPoint& inPoint) const
{
    return ee_ge_bbox_contains(GERust(*this), GERust(inPoint));
}

// Returns whether the two boxes overlap (touching faces count)
bool GEBoundingBox::Intersects(const GEBoundingBox& inBox) const
{
    return ee_ge_bbox_intersects(GERust(*this), GERust(inBox));
}
// Tests the line against the box's six faces; returns whether it hits any and the nearest hit distance
bool GEBoundingBox::Intersects(const GE3DLine& inLine, float& outDistance) const
{
    return ee_ge_bbox_intersects_line(GERust(*this), GERust(inLine), &outDistance);
}

// (0x10013808 LineToPlaneIntersection: Intersects(line)'s helper, in Rust)

// Reads the box's two corners from the file and recomputes its size; returns the first read error
long GEBoundingBox::Load(FSFileSpec& inFile)
{
    long theResult = this->mBottomLeftFront.Load(inFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mTopRightBack.Load(inFile);
    if (theResult != kErrorOK)
        return theResult;
    this->mWidth = this->mTopRightBack.GetX() - this->mBottomLeftFront.GetX();
    this->mHeight = this->mTopRightBack.GetZ() - this->mBottomLeftFront.GetZ();
    this->mDepth = this->mTopRightBack.GetY() - this->mBottomLeftFront.GetY();
    return theResult;
}

// Transforms all eight corners and sets outBox to the axis-aligned box around them
void GEBoundingBox::Transform(const GETransformation& inTransformation, GEBoundingBox& outBox) const
{
    ee_ge_bbox_transform(GERust(*this), GERust(inTransformation), GERust(outBox));
}

// (0x10013a65 TransformPoint: Transform's helper, in Rust)

// ---- Exported member with no address of its own (ICF-folded into GE3DPlane::Save 0xd54f)
// GEBoundingBox::Save writes the box's two corners to the file; returns the first write error
long GEBoundingBox::Save(FSFileSpec& inFile)
{
    long theResult = this->mBottomLeftFront.Save(inFile);
    if (theResult == kErrorOK)
        theResult = this->mTopRightBack.Save(inFile);
    return theResult;
}

// ---- GEViewport (GEViewport.cpp, folded here in R4.3 part 16)

// (kMinimumFrustrumExtent 0x100980f4, the smallest frustum extent, is in Rust)

GEVector GEViewport::sLocalLookAtVector(0.0f, 1.0f, 0.0f);

// Creates a default viewport: identity camera, 90-degree field of view, clipping from 1 to 10
GEViewport::GEViewport()
{
    ee_ge_viewport_default(GERust(*this));
}

// Creates a viewport for a screen rectangle and camera, computing its projections and clipping planes
GEViewport::GEViewport(const GERectangle& inViewRectangle, const GERectangle& inOrthographicRectangle,
                       const GETransformation& inCamera, float inFrontClippingDistance,
                       float inRearClippingDistance, float inFieldOfView)
{
    ee_ge_viewport_init(GERust(*this), GERust(inViewRectangle), GERust(inOrthographicRectangle), GERust(inCamera),
                        inFrontClippingDistance, inRearClippingDistance, inFieldOfView);
}

// Copies every field of another viewport (rectangles, planes, camera, matrices, projection factors)
GEViewport& GEViewport::operator=(const GEViewport& inViewport)
{
    ee_ge_viewport_copy(GERust(*this), GERust(inViewport));
    return *this;
}

// Recomputes the orthographic projection and its screen-to-world matrix (skipped with no rear clipping distance)
void GEViewport::ComputeOrthographicMatrix()
{
    ee_ge_viewport_orthographic_matrix(GERust(*this));
}

// Recomputes the perspective projection and its screen-to-world matrix (skipped with no rear clipping distance)
void GEViewport::ComputePerspectiveMatrix()
{
    ee_ge_viewport_perspective_matrix(GERust(*this));
}

// Returns the six world-space planes of the frustum under a screen rectangle (perspective or orthographic)
void GEViewport::GetFrustrumWorldPlanes(const GERectangle& inRectangle, std::vector<GE3DPlane>& outPlanes,
                                        bool inOrthographic) const
{
    outPlanes.resize(6);
    ee_ge_viewport_frustum_planes(GERust(*this), GERust(inRectangle), inOrthographic, GERust(outPlanes[0]));
}

// Appends the eight world-space corners (near and far per screen corner) of the frustum under a screen
// rectangle, widened to at least 2 pixels each way
void GEViewport::GetFrustrumWorldPoints(const GERectangle& inRectangle, std::vector<GE3DPoint>& outPoints,
                                        bool inOrthographic) const
{
    GE3DPoint thePoints[8];
    ee_ge_viewport_frustum_points(GERust(*this), GERust(inRectangle), inOrthographic, GERust(thePoints[0]));
    for (int k = 0; k < 8; k++)
        outPoints.push_back(thePoints[k]);
}

// Changes the field of view and recomputes the perspective and the clipping planes
void GEViewport::SetFieldOfView(float inFieldOfView)
{
    ee_ge_viewport_set_field_of_view(GERust(*this), inFieldOfView);
}

// Changes the front clipping distance (pushing the rear one back if needed) and recomputes everything
void GEViewport::SetFrontClippingDistance(float inDistance)
{
    ee_ge_viewport_set_front(GERust(*this), inDistance);
}

// Changes the rear clipping distance (pulling the front one in if needed) and recomputes everything
void GEViewport::SetRearClippingDistance(float inDistance)
{
    ee_ge_viewport_set_rear(GERust(*this), inDistance);
}

// Changes the screen rectangle, its centre and aspect ratio and the clipping planes and
// the projection matrices
void GEViewport::SetViewRectangle(const GERectangle& inRectangle)
{
    ee_ge_viewport_set_view_rectangle(GERust(*this), GERust(inRectangle));
}

// ---- GERectangle (GERectangle.cpp, folded here in R4.3 part 16)

const UString GERectangle::sLeftID("GERectangle-Left");
const UString GERectangle::sTopID("GERectangle-Top");
const UString GERectangle::sRightID("GERectangle-Right");
const UString GERectangle::sBottomID("GERectangle-Bottom");

// Clips the rectangle to another one; returns false and leaves it unchanged when they do not overlap
bool GERectangle::And(const GERectangle& inRectangle)
{
    return ee_ge_rect_and_in_place(GERust(*this), GERust(inRectangle));
}

// Writes the overlap of two rectangles to outRectangle; returns false (output untouched) when they do not overlap
bool GERectangle::And(const GERectangle& inRectangle, GERectangle& outRectangle) const
{
    return ee_ge_rect_and(GERust(*this), GERust(inRectangle), GERust(outRectangle));
}

// Reads the left, top, right and bottom edges from a file; returns the first read error (or not-open)
long GERectangle::Load(FSFile& inFile)
{
    const float* theFields[] = {&this->mLeft, &this->mTop, &this->mRight, &this->mBottom};
    return GERustFields(inFile, theFields, 4, inFile.GetIsOpen(), false, false);
}

// Writes the left, top, right and bottom edges to a file; returns the first write error (or not-open)
long GERectangle::Save(FSFile& inFile) const
{
    const float* theFields[] = {&this->mLeft, &this->mTop, &this->mRight, &this->mBottom};
    return GERustFields(inFile, theFields, 4, inFile.GetIsOpen(), true, false);
}

// ---- GEColor (GEColor.cpp, folded here in R4.3 part 16)

// Reads the colour's red, green and blue from the file; returns the first read error
long GEColor::Load(FSFile& inFile)
{
    const float* theFields[] = {&this->mRed, &this->mGreen, &this->mBlue};
    return GERustFields(inFile, theFields, 3, -1, false, false);
}

// Writes the colour's red, green and blue to the file; returns the first write error
long GEColor::Save(FSFile& inFile)
{
    const float* theFields[] = {&this->mRed, &this->mGreen, &this->mBlue};
    return GERustFields(inFile, theFields, 3, -1, true, false);
}

// ---- GELight (GELight.cpp, folded here in R4.3 part 16)

float GELight::GetAttenuation0() const
{
    return this->mAttenuation0;
}

void GELight::SetAttenuation0(float inAttenuation)
{
    this->mAttenuation0 = inAttenuation;
}

// ---- GETextureCoordinate (GETextureCoordinate.cpp, folded here in R4.3 part 16)

// Reads the U and V coordinates from an open file; on failure rewinds to where it started
long GETextureCoordinate::Load(FSFileSpec& inFile)
{
    const float* theFields[] = {&this->mU, &this->mV};
    return GERustFields(inFile, theFields, 2, inFile.GetIsOpen(), false, true);
}

// Writes the U and V coordinates to an open file; on failure rewinds to where it started
long GETextureCoordinate::Save(FSFileSpec& inFile)
{
    const float* theFields[] = {&this->mU, &this->mV};
    return GERustFields(inFile, theFields, 2, inFile.GetIsOpen(), true, true);
}

// ---- GEVertexArray (GEVertexArray.cpp, folded here in R4.3 part 16)

// The vertex array over its Rust array (ee_ge vertarr.rs; the renderer's two classes merged in R4.3 part 17); a
// subclass with its own storage has neither
static_assert(sizeof(GEColor) == 3 * sizeof(float) && sizeof(GE3DPoint) == 3 * sizeof(float) &&
              sizeof(GEVector) == 3 * sizeof(float) && sizeof(GETextureCoordinate) == 2 * sizeof(float), "vertarr.rs");
static float* GEFloats(const void* inValue) { return reinterpret_cast<float*>(const_cast<void*>(inValue)); }

GEVertexArray::GEVertexArray() : mVertices(NULL), mRust(NULL) {}
GEVertexArray::GEVertexArray(unsigned long inType, unsigned long inCount)
    : mRust(ee_ge_va_new(inType, inCount)) { this->mVertices = ee_ge_va_vertices(this->mRust); }
GEVertexArray::~GEVertexArray()
{
    if (this->mRust)
        ee_ge_va_destroy(this->mRust);
}
void GEVertexArray::CopyFrom(const GEVertexArray& inArray) { ee_ge_va_copy_from(this->mRust, inArray.mRust); }
void GEVertexArray::GetColor(GEColor& outColor, unsigned long inIndex) const { ee_ge_va_get(this->mRust, EeGeVaField::Color, inIndex, GEFloats(&outColor)); }
void GEVertexArray::GetNormal(GEVector& outNormal, unsigned long inIndex) const { ee_ge_va_get(this->mRust, EeGeVaField::Normal, inIndex, GEFloats(&outNormal)); }
float GEVertexArray::GetOpacity(unsigned long inIndex) const
{
    float theOpacity;
    ee_ge_va_get(this->mRust, EeGeVaField::Opacity, inIndex, &theOpacity);
    return theOpacity;
}
void GEVertexArray::GetPoint(GE3DPoint& outPoint, unsigned long inIndex) const { ee_ge_va_get(this->mRust, EeGeVaField::Point, inIndex, GEFloats(&outPoint)); }
unsigned long GEVertexArray::GetSize() const { return ee_ge_va_size(this->mRust, false); }
void GEVertexArray::GetTextureCoordinate(GETextureCoordinate& outCoordinate, unsigned long inIndex) const
{
    ee_ge_va_get(this->mRust, EeGeVaField::Uv, inIndex, GEFloats(&outCoordinate));
}
unsigned long GEVertexArray::GetType() const { return ee_ge_va_type(this->mRust); }
void GEVertexArray::SetColor(const GEColor& inColor, unsigned long inIndex) { ee_ge_va_set(this->mRust, EeGeVaField::Color, inIndex, GEFloats(&inColor)); }
void GEVertexArray::SetNormal(const GEVector& inNormal, unsigned long inIndex) { ee_ge_va_set(this->mRust, EeGeVaField::Normal, inIndex, GEFloats(&inNormal)); }
void GEVertexArray::SetOpacity(float inOpacity, unsigned long inIndex) { ee_ge_va_set(this->mRust, EeGeVaField::Opacity, inIndex, &inOpacity); }
void GEVertexArray::SetPoint(const GE3DPoint& inPoint, unsigned long inIndex) { ee_ge_va_set(this->mRust, EeGeVaField::Point, inIndex, GEFloats(&inPoint)); }
void GEVertexArray::SetTextureCoordinate(const GETextureCoordinate& inCoordinate, unsigned long inIndex)
{
    ee_ge_va_set(this->mRust, EeGeVaField::Uv, inIndex, GEFloats(&inCoordinate));
}
void GEVertexArray::SetVertex(const GEColor& inColor, const GE3DPoint& inPoint, const GEVector& inNormal,
                              const GETextureCoordinate& inCoordinate, float inOpacity, unsigned long inIndex)
{
    ee_ge_va_set_vertex(this->mRust, GEFloats(&inColor), GEFloats(&inPoint), GEFloats(&inNormal), GEFloats(&inCoordinate),
                        inOpacity, inIndex);
}
unsigned long GEVertexArray::SizeOf() const { return ee_ge_va_size(this->mRust, true); }

// ---- GEPolygonArray (GEPolygonArray.cpp, folded here in R4.3 part 16)

// The polygon array over its Rust array (ee_ge vertarr.rs; the renderer's DX7PolygonArray merged in R4.3 part 17)
GEPolygonArray::GEPolygonArray(unsigned long inCount) : mRust(ee_ge_pa_new(inCount)) { this->mIndices = ee_ge_pa_indices(this->mRust); }
unsigned long GEPolygonArray::GetSize() const { return ee_ge_pa_size(this->mRust, false); }
unsigned long GEPolygonArray::GetVertexIndex1(unsigned long inPolygon) const { return ee_ge_pa_index(this->mRust, inPolygon, 0); }
unsigned long GEPolygonArray::GetVertexIndex2(unsigned long inPolygon) const { return ee_ge_pa_index(this->mRust, inPolygon, 1); }
unsigned long GEPolygonArray::GetVertexIndex3(unsigned long inPolygon) const { return ee_ge_pa_index(this->mRust, inPolygon, 2); }
void GEPolygonArray::SetVertexIndex1(unsigned long inPolygon, unsigned long inIndex) { ee_ge_pa_set_index(this->mRust, inPolygon, 0, inIndex); }
void GEPolygonArray::SetVertexIndex2(unsigned long inPolygon, unsigned long inIndex) { ee_ge_pa_set_index(this->mRust, inPolygon, 1, inIndex); }
void GEPolygonArray::SetVertexIndex3(unsigned long inPolygon, unsigned long inIndex) { ee_ge_pa_set_index(this->mRust, inPolygon, 2, inIndex); }
void GEPolygonArray::SetVertexIndices(unsigned long inPolygon, unsigned long inIndex1, unsigned long inIndex2, unsigned long inIndex3)
{
    ee_ge_pa_set_index(this->mRust, inPolygon, 0, inIndex1);
    ee_ge_pa_set_index(this->mRust, inPolygon, 1, inIndex2);
    ee_ge_pa_set_index(this->mRust, inPolygon, 2, inIndex3);
}
unsigned long GEPolygonArray::SizeOf() const { return ee_ge_pa_size(this->mRust, true); }

// A subclass with its own storage
GEPolygonArray::GEPolygonArray() : mIndices(NULL), mRust(NULL) {}
GEPolygonArray::~GEPolygonArray()
{
    if (this->mRust)
        ee_ge_pa_destroy(this->mRust);
}
