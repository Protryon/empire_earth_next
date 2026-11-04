// C:\EEX\Low-Level Engine\Graphics Engine\GETransformation.h (RECONSTRUCTION). Requires GE2DPoint.h, GEVector.h,
// GE3DPoint.h, GEOrientation.h. GETransformation.cpp sits at ~0x100349e4; the small members (0x1000d5a1..0x1000d9d3)
// are emitted first by GE3DPoint.cpp, in the same pattern as the GE2DPoint.cpp block (see lle/GE2DPoint.h): they
// look header-inline, but the plug-ins import Apply(GEVector) and the ctor, so they are defined in GE3DPoint.cpp.
#ifndef LLE_GETRANSFORMATION_H
#define LLE_GETRANSFORMATION_H

class UString;
class FSFile;
class GE3DLine;
class GE3DPlane;

class LLE_API GETransformation {            // sizeof 0x40 (SizeOf 0xd98d): row-major 4x4, translation in column 3
public:
    GETransformation();                     // identity (imported by Default.dll)
    // inline (COMDAT 0x1000d5a1 in GE3DPoint.cpp's block): leaves the matrix uninitialized; Revolve/Rotate/Scale
    // (0x3513a..) construct their temporaries with no call, and no module imports it
    GETransformation(bool inInitialize) {}
    GETransformation(const float inMatrix[4][4]);
    // exported out of line (0x10034a36, ICF-folded with operator= and the matrix ctor); GEViewport's implicit
    // copy ctor calls it
    GETransformation(const GETransformation& inTransformation);
    GETransformation& operator=(const GETransformation& inTransformation);   // out of line (0x10034a36)
    bool operator==(const GETransformation& inTransformation);
    bool operator==(const GE3DPoint& inPoint) const;
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
    void Apply(const GETransformation& inTransformation, GETransformation& outTransformation) const;
    void Apply(const GE3DLine& inLine, GE3DLine& outLine) const;
    void Apply(const GE3DPlane& inPlane, GE3DPlane& outPlane) const;
    void ConvertToOrientation(GEOrientation& outOrientation) const;
    float DistanceTo(const GE3DPoint& inPoint) const;
    float DistanceTo(const GETransformation& inTransformation) const;
    float DistanceSquaredTo(const GE3DPoint& inPoint) const;
    float DistanceSquaredTo(const GETransformation& inTransformation) const;
    void Dump() const;
    unsigned long GetChecksum() const;
    // inline: the exported COMDAT sits in GE3DPoint.cpp's GETransformation block (0x1000d783); GEQuaternion
    // (0x10023219) reads every element through it (fld + faddp/fsubp, no memory operands)
    float GetElement(unsigned long inRow, unsigned long inColumn) const { return this->mMatrix[inRow][inColumn]; }
    float GetPitch() const;
    GE3DPoint GetPosition() const;
    void GetPosition(GE3DPoint& outPosition) const;
    float GetRoll() const;
    float GetX() const { return this->mMatrix[0][3]; }  // exported, ICF-folded (0x10058d38)
    float GetY() const { return this->mMatrix[1][3]; }
    float GetYaw() const;
    float GetZ() const { return this->mMatrix[2][3]; }  // 0x1000d7c4
    long Load(FSFile& inFile);
    void MakeIdentity();
    void MakeInverse(GETransformation& outInverse) const;
    void MakeLookAtTransform(GE3DPoint& inEye, GE3DPoint& inTarget);
    void Normalize();
    void Revolve(const GEOrientation& inOrientation);
    void Revolve(float inYaw, float inPitch, float inRoll);
    void Rotate(const GEOrientation& inOrientation);
    void Rotate(float inYaw, float inPitch, float inRoll);
    long Save(FSFile& inFile) const;
    // inline: EE-AOC.exe imports only Scale(x, y, z); GEPhysicalModel::Intersects expands it (GE3DPoint.cpp emits the
    // exported copy 0x1000d848)
    void Scale(float inScale) { this->Scale(inScale, inScale, inScale); }
    void Scale(float inX, float inY, float inZ);
    // inline: exported COMDAT in GE3DPoint.cpp's GETransformation block (0x1000d868)
    void SetElement(unsigned long inRow, unsigned long inColumn, float inValue)
        { this->mMatrix[inRow][inColumn] = inValue; }
    void SetOrientationPRY(const GEOrientation& inOrientation);
    void SetOrientationPRY(float inYaw, float inPitch, float inRoll);
    void SetOrientationPYR(const GEOrientation& inOrientation);
    void SetOrientationPYR(float inYaw, float inPitch, float inRoll);
    void SetOrientationRPY(const GEOrientation& inOrientation);
    void SetOrientationRPY(float inYaw, float inPitch, float inRoll);
    void SetOrientationRYP(const GEOrientation& inOrientation);
    void SetOrientationRYP(float inYaw, float inPitch, float inRoll);
    void SetOrientationYPR(const GEOrientation& inOrientation);
    void SetOrientationYPR(float inYaw, float inPitch, float inRoll);
    void SetOrientationYRP(const GEOrientation& inOrientation);
    void SetOrientationYRP(float inYaw, float inPitch, float inRoll);
#ifdef LLE_EXE_HEADERS
    // inline in the exe's view: the exe imports only SetPosition(MMM) and calls it where this inline's nested
    // expansion is refused (EEE3DTeleport 0xfb9f6, EEE3DTimeWarp 0xfbec6)
    void SetPosition(const GE3DPoint& inPosition)
        { this->SetPosition(inPosition.GetX(), inPosition.GetY(), inPosition.GetZ()); }
#else
    void SetPosition(const GE3DPoint& inPosition);
#endif
    // inline: SetPosition(const GE3DPoint&) 0xd93d loads z, y, x (call arguments) then stores
    void SetPosition(float inX, float inY, float inZ) { mMatrix[0][3] = inX; mMatrix[1][3] = inY; mMatrix[2][3] = inZ; }
    void SetScale(float inScale);
    void SetScale(float inX, float inY, float inZ);
    void SetX(float inX) { this->mMatrix[0][3] = inX; }  // exported, ICF-folded
    void SetY(float inY) { this->mMatrix[1][3] = inY; }
    void SetZ(float inZ) { this->mMatrix[2][3] = inZ; }
    unsigned long SizeOf();
    void Translate(const GEVector& inVector);
    void Translate(float inX, float inY, float inZ) { this->mMatrix[0][3] += inX; this->mMatrix[1][3] += inY; this->mMatrix[2][3] += inZ; }
    bool Verify() const;
    void ZeroOrientation();
    void ZeroOrientation(GETransformation& outTransformation) const;

    float mMatrix[4][4];                    // +0x00
#ifdef LOWLEVELENGINE_EXPORTS
    // Inferred guard: a class member (exported ?sTransformationStringID@GETransformation@@0VUString@@B), but every
    // static data member a TU sees takes a VC6 $E number and the plug-ins' init-chain numbering shows they saw none
private:
    static const UString sTransformationStringID;
#endif
};

#ifdef LLE_EXE_HEADERS
// inline in the game's view (EE-AOC.exe imports neither and expands both: EEUGHealUnit's ctor 0x22531b reads the
// matrix through one base register); the engine exports out-of-line copies (GE3DPoint.cpp 0x1000d794/0x1000d7ac)
inline GE3DPoint GETransformation::GetPosition() const
{
    return GE3DPoint(this->GetX(), this->GetY(), this->GetZ());
}
inline void GETransformation::GetPosition(GE3DPoint& outPosition) const
{
    outPosition.mX = this->GetX();
    outPosition.mY = this->GetY();
    outPosition.mZ = this->GetZ();
}
#endif

#endif
