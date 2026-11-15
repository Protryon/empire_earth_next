// C:\EEX\Low-Level Engine\Graphics Engine\GE3DPoint.h (RECONSTRUCTION). Requires GE2DPoint.h, GEVector.h, <vector>.
#ifndef LLE_GE3DPOINT_H
#define LLE_GE3DPOINT_H

class UString;
class FSFile;
class GETransformation;
class ESChecksum;

class LLE_API GE3DPoint {                   // sizeof 0xc (GE3DLine: start +0, direction +0xc)
public:
    // no user copy ctor: GE3DLine's copy ctor 0xc725 copies its GE3DPoint with movsd (trivial copy)
    GE3DPoint() : mX(0.0f), mY(0.0f), mZ(0.0f) {}
    // inline (GE2DPoint::InterpolateTo stores its locals directly); its exported copy is the ICF representative
    // at 0x1000d56f, folded with GEVector's
    GE3DPoint(float inX, float inY, float inZ) : mX(inX), mY(inY), mZ(inZ) {}
    GE3DPoint(const GETransformation& inTransformation);
    ~GE3DPoint() {}
    // user-defined (field moves): GE3DLine's assignments 0xc5da/0xc66a copy the start with three movs, not movsd
    GE3DPoint& operator=(const GE3DPoint& inPoint) { this->mX = inPoint.mX; this->mY = inPoint.mY; this->mZ = inPoint.mZ; return *this; }
    GE3DPoint& operator+=(const GE3DPoint& inPoint);
#ifdef LLE_EXE_HEADERS
    // exe: inline (the engine body, GE2DPoint.cpp); the /Os post-check undoes it, so each site calls [imp] directly
    // instead of caching the import in a register (EEMap 0x51c75a/0x51c847; LEARNINGS)
    GE3DPoint& operator+=(const GEVector& inVector)
    {
        this->mX += inVector.GetX();
        this->mY += inVector.GetY();
        this->mZ += inVector.GetZ();
        return *this;
    }
#else
    GE3DPoint& operator+=(const GEVector& inVector);
#endif
    GE3DPoint& operator-=(const GE3DPoint& inPoint);
    GE3DPoint& operator-=(const GEVector& inVector);
    bool operator==(const GE2DPoint& inPoint) const;
    // inline (GEModel::Verify, GELODMetaVertex::BuildMetaVertexList/CollapseEdge expand it); GE2DPoint.cpp's block
    // emits the exported copy 0x1000c47e (ICF-folded with GEVector's/GEColor's ==)
    bool operator==(const GE3DPoint& inPoint) const { return mX == inPoint.mX && mY == inPoint.mY && mZ == inPoint.mZ; }
    bool operator==(const GETransformation& inTransformation) const;
    // inline (InterpolateTo 0xdae4 expands it); its exported copy is ICF-folded into GEVector's 0x1000c29f
    bool operator!=(const GE3DPoint& inPoint) const { return mX != inPoint.mX || mY != inPoint.mY || mZ != inPoint.mZ; }
    void Add(const GEVector& inVector, GE3DPoint& outPoint) const;
    void Clear();
    float DistanceTo(const GE3DPoint& inPoint) const;
    float DistanceToSquared(const GE3DPoint& inPoint) const;
    GE2DPoint Get2DPoint() const { return GE2DPoint(this->mX, this->mY); }  // inline (InterpolateTo 0xc80e)
    void GetChecksum(ESChecksum& ioChecksum) const;
    float GetX() const { return this->mX; }
    float GetY() const { return this->mY; }
    float GetZ() const { return this->mZ; }
    void InterpolateTo(const GE3DPoint& inPoint, float inStep, std::vector<GE3DPoint>& outPoints) const;
    long Load(FSFile& inFile);
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    long Save(FSFile& inFile) const;
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) const;
    void SetX(float inX) { this->mX = inX; }
    void SetY(float inY) { this->mY = inY; }
    void SetZ(float inZ) { this->mZ = inZ; }
#ifdef LLE_EXE_HEADERS
    // inline in the exe's view: the exe never imports ?SetXYZ@GE3DPoint@@QAEXMMM@Z and expands it (three stores,
    // EECPASGUProphetCalamityAnalyzerVolcano::Fn09318 0x409483)
    void SetXYZ(float inX, float inY, float inZ) { this->mX = inX; this->mY = inY; this->mZ = inZ; }
#else
    void SetXYZ(float inX, float inY, float inZ);
#endif

    float mX;                               // +0x00
    float mY;                               // +0x04
    float mZ;                               // +0x08
#ifdef LOWLEVELENGINE_EXPORTS
    // Inferred guard: these are class members (exported ?m..@GE3DPoint@@0VUString@@B), but every static data
    // member a TU sees takes a VC6 $E number and the plug-ins' init-chain numbering shows they saw none
private:
    static const UString mPointXStringID;   // GE3DPoint.cpp $E chain 0x1000d9d3
    static const UString mPointYStringID;
    static const UString mPointZStringID;
#endif
};

#endif
