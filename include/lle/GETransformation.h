// C:\EEX\Low-Level Engine\Graphics Engine\GETransformation.h (RECONSTRUCTION). Requires GE2DPoint.h, GEVector.h,
// GE3DPoint.h, GEOrientation.h. The matrix algorithms are Rust (ee_base transform.rs, ee_render geval values.rs;
// lle/GERust.h): the simulation copies transformations everywhere, as the sixteen floats as they are (bit for bit),
// and compares them as IEEE does (a NaN never equals).
#ifndef LLE_GETRANSFORMATION_H
#define LLE_GETRANSFORMATION_H

#include "lle/GERust.h"

class FSFile;
class GE3DLine;
class GE3DPlane;

class GETransformation {                    // sizeof 0x40 (SizeOf 0xd98d): row-major 4x4, translation in column 3
public:
    GETransformation() { ee_ge_t_identity(GERust(*this)); }
    // inline (COMDAT 0x1000d5a1 in GE3DPoint.cpp's block): leaves the matrix uninitialized; Revolve/Rotate/Scale
    // (0x3513a..) construct their temporaries with no call, and no module imports it
    GETransformation(bool inInitialize) {}
    GETransformation(const float inMatrix[4][4])
        { ee_ge_t_copy(GERust(*this), reinterpret_cast<const float(*)[4][4]>(inMatrix)); }
    GETransformation(const GETransformation& inTransformation)
        { ee_ge_t_copy(GERust(*this), &GERust(inTransformation)->m); }
    GETransformation& operator=(const GETransformation& inTransformation)
        { ee_ge_t_copy(GERust(*this), &GERust(inTransformation)->m); return *this; }
    bool operator==(const GETransformation& inTransformation)   // all sixteen elements
        { return ee_ge_t_equal(GERust(*this), GERust(inTransformation)); }
    bool operator==(const GE3DPoint& inPoint) const { return ee_ge_t_at_point(GERust(*this), GERust(inPoint)); }
    void Apply(const GE2DPoint& inPoint, GE2DPoint& outPoint) const
    {
        outPoint.mX = inPoint.GetY() * this->mMatrix[0][1] + inPoint.GetX() * this->mMatrix[0][0] + this->mMatrix[0][3];
        outPoint.mY = inPoint.GetX() * this->mMatrix[1][0] + inPoint.GetY() * this->mMatrix[1][1] + this->mMatrix[1][3];
    }
    void Apply(const GE3DPoint& inPoint, GE3DPoint& outPoint) const
    {
        outPoint.mX = inPoint.GetZ() * this->mMatrix[0][2] + inPoint.GetY() * this->mMatrix[0][1] + inPoint.GetX() * this->mMatrix[0][0] + this->mMatrix[0][3];
        outPoint.mY = inPoint.GetX() * this->mMatrix[1][0] + inPoint.GetZ() * this->mMatrix[1][2] + inPoint.GetY() * this->mMatrix[1][1] + this->mMatrix[1][3];
        outPoint.mZ = inPoint.GetX() * this->mMatrix[2][0] + inPoint.GetZ() * this->mMatrix[2][2] + inPoint.GetY() * this->mMatrix[2][1] + this->mMatrix[2][3];
    }
    void Apply(const GEVector& inVector, GEVector& outVector) const
    {
        outVector.mX = inVector.GetZ() * this->mMatrix[0][2] + inVector.GetY() * this->mMatrix[0][1] + inVector.GetX() * this->mMatrix[0][0];
        outVector.mY = inVector.GetX() * this->mMatrix[1][0] + inVector.GetZ() * this->mMatrix[1][2] + inVector.GetY() * this->mMatrix[1][1];
        outVector.mZ = inVector.GetX() * this->mMatrix[2][0] + inVector.GetZ() * this->mMatrix[2][2] + inVector.GetY() * this->mMatrix[2][1];
    }
    // this x other into outTransformation (the bottom row copied from the other)
    void Apply(const GETransformation& inTransformation, GETransformation& outTransformation) const
        { ee_ge_t_compose(GERust(*this), GERust(inTransformation), GERust(outTransformation)); }
    void Apply(const GE3DLine& inLine, GE3DLine& outLine) const
        { ee_ge_t_apply_line(GERust(*this), GERust(inLine), GERust(outLine)); }
    void Apply(const GE3DPlane& inPlane, GE3DPlane& outPlane) const   // its point and normal
        { ee_ge_t_apply_plane(GERust(*this), GERust(inPlane), GERust(outPlane)); }
    // yaw, pitch and roll from the rotation part
    void ConvertToOrientation(GEOrientation& outOrientation) const
        { ee_ge_t_orientation(GERust(*this), GERust(outOrientation)); }
    // the distances between positions: to a point with the full-precision square root, between transformations
    // with the fast one
    float DistanceTo(const GE3DPoint& inPoint) const { return ee_ge_t_distance_to_point(GERust(*this), GERust(inPoint)); }
    float DistanceTo(const GETransformation& inTransformation) const
        { return ee_ge_t_distance_to(GERust(*this), GERust(inTransformation)); }
    float DistanceSquaredTo(const GE3DPoint& inPoint) const
        { return ee_ge_t_distance_squared_to_point(GERust(*this), GERust(inPoint)); }
    float DistanceSquaredTo(const GETransformation& inTransformation) const
        { return ee_ge_t_distance_squared_to(GERust(*this), GERust(inTransformation)); }
    void Dump() const {}
    unsigned long GetChecksum() const { return 0; }
    // inline: the exported COMDAT sits in GE3DPoint.cpp's GETransformation block (0x1000d783); GEQuaternion
    // (0x10023219) reads every element through it (fld + faddp/fsubp, no memory operands)
    float GetElement(unsigned long inRow, unsigned long inColumn) const { return this->mMatrix[inRow][inColumn]; }
    float GetPitch() const { return ee_ge_t_pitch(GERust(*this)); }   // the angles taken from the matrix
    GE3DPoint GetPosition() const { return GE3DPoint(this->GetX(), this->GetY(), this->GetZ()); }
    void GetPosition(GE3DPoint& outPosition) const
        { outPosition.mX = this->GetX(); outPosition.mY = this->GetY(); outPosition.mZ = this->GetZ(); }
    float GetRoll() const { return ee_ge_t_roll(GERust(*this)); }
    float GetX() const { return this->mMatrix[0][3]; }  // exported, ICF-folded (0x10058d38)
    float GetY() const { return this->mMatrix[1][3]; }
    float GetYaw() const { return ee_ge_t_yaw(GERust(*this)); }
    float GetZ() const { return this->mMatrix[2][3]; }
    // the sixteen elements from (to) a file, stopping at the first error
    template <class F> long Load(F& inFile) { EeGeFieldIo theIO = GERustFieldIO(inFile); return ee_ge_t_io(GERust(*this), &theIO, false); }
    void MakeIdentity() { ee_ge_t_identity(GERust(*this)); }
    // the inverse of a rigid transformation (transposed rotation, negated rotated position), element by element as
    // the C++ stored them (an output equal to this transposes through its own stores)
    void MakeInverse(GETransformation& outInverse) const { ee_ge_t_make_inverse(GERust(*this), GERust(outInverse)); }
    void Normalize() { ee_ge_t_normalize(GERust(*this)); }   // the three rotation columns to unit length
    // Revolve: yaw, pitch and roll applied after it (about its own axes); Rotate: before it (the parent axes)
    void Revolve(const GEOrientation& inOrientation)
        { this->Revolve(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll()); }
    void Revolve(float inYaw, float inPitch, float inRoll) { ee_ge_t_revolve(GERust(*this), inYaw, inPitch, inRoll); }
    void Rotate(const GEOrientation& inOrientation)
        { this->Rotate(inOrientation.GetYaw(), inOrientation.GetPitch(), inOrientation.GetRoll()); }
    void Rotate(float inYaw, float inPitch, float inRoll) { ee_ge_t_rotate(GERust(*this), inYaw, inPitch, inRoll); }
    template <class F> long Save(F& inFile) const
    {
        EeGeFieldIo theIO = GERustFieldIO(inFile);
        return ee_ge_t_io(const_cast<EeGeTransformation*>(GERust(*this)), &theIO, true);
    }
    // inline: EE-AOC.exe imports only Scale(x, y, z); GEPhysicalModel::Intersects expands it (GE3DPoint.cpp emits the
    // exported copy 0x1000d848)
    void Scale(float inScale) { this->Scale(inScale, inScale, inScale); }
    void Scale(float inX, float inY, float inZ) { ee_ge_t_scale(GERust(*this), inX, inY, inZ); }   // applied before it
    // inline: exported COMDAT in GE3DPoint.cpp's GETransformation block (0x1000d868)
    void SetElement(unsigned long inRow, unsigned long inColumn, float inValue)
        { this->mMatrix[inRow][inColumn] = inValue; }
    // the rotation from yaw, pitch and roll applied in the order the name gives (the position kept)
    void SetOrientationPRY(const GEOrientation& inO) { this->SetOrientationPRY(inO.GetYaw(), inO.GetPitch(), inO.GetRoll()); }
    void SetOrientationPRY(float inYaw, float inPitch, float inRoll) { this->SetOrientation(0, inYaw, inPitch, inRoll); }
    void SetOrientationPYR(const GEOrientation& inO) { this->SetOrientationPYR(inO.GetYaw(), inO.GetPitch(), inO.GetRoll()); }
    void SetOrientationPYR(float inYaw, float inPitch, float inRoll) { this->SetOrientation(1, inYaw, inPitch, inRoll); }
    void SetOrientationRPY(const GEOrientation& inO) { this->SetOrientationRPY(inO.GetYaw(), inO.GetPitch(), inO.GetRoll()); }
    void SetOrientationRPY(float inYaw, float inPitch, float inRoll) { this->SetOrientation(2, inYaw, inPitch, inRoll); }
    void SetOrientationRYP(const GEOrientation& inO) { this->SetOrientationRYP(inO.GetYaw(), inO.GetPitch(), inO.GetRoll()); }
    void SetOrientationRYP(float inYaw, float inPitch, float inRoll) { this->SetOrientation(3, inYaw, inPitch, inRoll); }
    void SetOrientationYPR(const GEOrientation& inO) { this->SetOrientationYPR(inO.GetYaw(), inO.GetPitch(), inO.GetRoll()); }
    void SetOrientationYPR(float inYaw, float inPitch, float inRoll) { this->SetOrientation(4, inYaw, inPitch, inRoll); }
    void SetOrientationYRP(const GEOrientation& inO) { this->SetOrientationYRP(inO.GetYaw(), inO.GetPitch(), inO.GetRoll()); }
    void SetOrientationYRP(float inYaw, float inPitch, float inRoll) { this->SetOrientation(5, inYaw, inPitch, inRoll); }
    void SetOrientation(uint32_t inOrder, float inYaw, float inPitch, float inRoll)
        { ee_ge_t_set_orientation(GERust(*this), inOrder, inYaw, inPitch, inRoll); }
    void SetPosition(const GE3DPoint& inPosition)
        { this->SetPosition(inPosition.GetX(), inPosition.GetY(), inPosition.GetZ()); }
    // inline: SetPosition(const GE3DPoint&) 0xd93d loads z, y, x (call arguments) then stores
    void SetPosition(float inX, float inY, float inZ) { mMatrix[0][3] = inX; mMatrix[1][3] = inY; mMatrix[2][3] = inZ; }
    void SetScale(float inScale) { this->SetScale(inScale, inScale, inScale); }
    // a pure scale by x, y and z (the position column kept)
    void SetScale(float inX, float inY, float inZ) { ee_ge_t_set_scale(GERust(*this), inX, inY, inZ); }
    void SetX(float inX) { this->mMatrix[0][3] = inX; }  // exported, ICF-folded
    void SetY(float inY) { this->mMatrix[1][3] = inY; }
    void SetZ(float inZ) { this->mMatrix[2][3] = inZ; }
    unsigned long SizeOf() { return sizeof(GETransformation); }
    void Translate(const GEVector& inVector) { this->Translate(inVector.GetX(), inVector.GetY(), inVector.GetZ()); }
    void Translate(float inX, float inY, float inZ) { this->mMatrix[0][3] += inX; this->mMatrix[1][3] += inY; this->mMatrix[2][3] += inZ; }
    bool Verify() const { return true; }    // (no consistency check in the retail build)

    float mMatrix[4][4];                    // +0x00
};

inline GE3DPoint::GE3DPoint(const GETransformation& inTransformation)
    : mX(inTransformation.GetX()), mY(inTransformation.GetY()), mZ(inTransformation.GetZ())
{
}
inline bool GE3DPoint::operator==(const GETransformation& inTransformation) const
{
    return this->GetX() == inTransformation.GetX() && this->GetY() == inTransformation.GetY() &&
           this->GetZ() == inTransformation.GetZ();
}

#endif
