// C:\EEX\Low-Level Engine\Graphics Engine\GEModel.h (RECONSTRUCTION). A loaded/created 3D model; reference
// counted, owned by a GEModelManager. GEModel.cpp (0x1001eb9f..) and the 'GEModel <part>.cpp' units hold its members.
// Layout from the implicit copy ctor (0x10011536) and operator= (0x10011799) and the inline accessors, all emitted
// first by GEBoundingBox.cpp (0x100112f3..0x10011a4e); provisional names unless an export names them.
#ifndef LLE_GEMODEL_H
#define LLE_GEMODEL_H

#include <map>
#include "lle/GEBoundingBox.h"

class FSFileSpec;
class GEMaterial;
class GEModelManager;
class GEPolygonArray;
class GETexture;
class GEVertexArray;
class URandomIntegers;
struct GETextureDescription;
struct EeGeCemModel;   // a model read by Rust (rust/ee_rust.h)

class LLE_API GEModel : public UReferenceCounter {   // sizeof 0x188 in the original (GEModelManager: operator new(0x188))
    friend class GEModelManager;
    friend class GEPhysicalModel;           // Intersects reads the const model's arrays (provisional name)
public:
    // inline members in declaration order (the folded ones sit at other addresses)
    const GEBoundingBox& GetBoundingBox(unsigned long inLOD) const { return this->mBoundingBoxes[inLOD]; }
    float GetBoundingRadius(unsigned long inLOD) const { return this->mBoundingRadii[inLOD]; }
    const GE3DPoint& GetCenterPoint() const { return this->mCenterPoint; }
    bool GetClipToPlane(unsigned long inPlane) { return this->mClipToPlane[inPlane]; }
    unsigned long GetFrameCount() const { return this->mFrameCount; }
    float GetFractionalFramePosition() const { return this->mFractionalFramePosition; }
    const GEBoundingBox& GetGraphicsBoundingBox() const { return *this->mGraphicsBoundingBox; }
    float GetGraphicsBoundingRadius() const { return this->mBoundingRadii[this->mGraphicsCurrentFrame]; }
    unsigned long GetGraphicsCurrentFrame() const { return this->mGraphicsCurrentFrame; }
    const GETransformation& GetGraphicsInternalTransform() const
        { return this->mInternalTransforms[this->mGraphicsCurrentFrame]; }
    unsigned long GetGraphicsNextFrame() const { return this->mGraphicsNextFrame; }
    const GETransformation& GetGraphicsPostModelTransform() const { return this->mGraphicsPostModelTransform; }
    const GETransformation& GetGraphicsPreModelTransform() const { return this->mGraphicsPreModelTransform; }
    float GetGraphicsScale() const { return this->mGraphicsScale; }
    const GETransformation& GetGraphicsTransform() const { return this->mGraphicsTransform; }
    const GETransformation& GetInternalTransform(unsigned long inFrame) const { return this->mInternalTransforms[inFrame]; }
    unsigned long GetLODCount() const { return this->mLODCount; }
    GEMaterial* GetMaterial(unsigned long inIndex) const { return this->mMaterials[inIndex]; }
    unsigned long GetMaterialCount() const { return this->mMaterialCount; }
    float GetModelOpacity() const { return this->mModelOpacity; }
    const UString& GetName() const { return this->mName; }
    GEModel* GetParentModel() const { return this->mParentModel; }
    GEPolygonArray* GetPolygonArray(unsigned long inLOD) { return this->mPolygonArrays[inLOD]; }
    unsigned long GetPolygonCount() const { return this->mPolygonCount; }
    unsigned long GetPolygonCount(unsigned long inLOD) const;   // 0x1001fab0
    unsigned long GetPolygonCountOfHierarchy(unsigned long inLOD) const;   // 0x1001fade
    GEVertexArray* GetVertexArray(unsigned long inFrame) { return this->mVertexArrays[inFrame]; }
    GEModel* GetSubModel(unsigned long inIndex) const { return this->mSubModels[inIndex]; }
    unsigned long GetSubModelCount() const { return this->mSubModelCount; }
    const GE3DPoint& GetTagPoint(unsigned long inIndex) const { return this->mTagPoints[inIndex]; }
    const GE3DPoint& GetTagPoint(unsigned long inIndex, unsigned long inFrame) const
        { return this->mFrameTagPoints[inFrame][inIndex]; }
    unsigned long GetTagPointCount() const { return this->mTagPointCount; }
    GE3DPoint* GetTagPointList() const { return this->mTagPoints; }
    const UString& GetTagPointName(unsigned long inIndex) const { return this->mTagPointNames[inIndex]; }
    void SetClipToPlane(unsigned long inPlane, bool inClip) { this->mClipToPlane[inPlane] = inClip; }
    void SetGraphicsPreModelTransform(const GETransformation& inTransform) { this->mGraphicsPreModelTransform = inTransform; }
    void SetGraphicsPostModelTransform(const GETransformation& inTransform)
        { this->mGraphicsPostModelTransform = inTransform; }
    void SetGraphicsScale(float inScale) { this->mGraphicsScale = inScale; }
    void SetInternalTransform(unsigned long inFrame, GETransformation& inTransform) const
        { this->mInternalTransforms[inFrame] = inTransform; }
    void SetOpacity(float inOpacity)
    {
        this->mModelOpacity = inOpacity;
        for (unsigned long i = 0; i < this->mSubModelCount; i++)
            this->mSubModels[i]->SetOpacity(inOpacity);
    }
    void SetTagPoint(unsigned long inIndex, const GE3DPoint& inPoint) { this->mTagPoints[inIndex] = inPoint; }
    void SetTagPoint(unsigned long inFrame, unsigned long inIndex, const GE3DPoint& inPoint)
        { this->mFrameTagPoints[inFrame][inIndex] = inPoint; }
    void SetTagPointName(const UString& inName, unsigned long inIndex) { this->mTagPointNames[inIndex] = inName; }
    void SetTextureFilename(const UString& inName, unsigned long inIndex) { this->mTextureFilenames[inIndex] = inName; }

    // vtable 0x10097adc: slot 0 the deleting dtor, slot 1 Load (0x1001c3b4, 'GEModel Load.cpp')
    virtual long Load(FSFileSpec& inFile, std::map<unsigned long, long>& ioTextures, bool inLoadTextures);
    void ComputeBoundingVolumes();          // 0x1001f69a
    void ComputeLODReduction(unsigned long inLOD);   // 0x1001ce5b
    float ComputeMemoryLocalityEfficiency(unsigned long inLOD);   // 0x100173ba
    void ComputeModelNormals();             // 0x1001f80c
    void ComputeOptimalDataOrdering(long inA, long inB);   // 0x10017523
    void ComputeVertexCacheEfficiency(unsigned long inLOD, float& outA, float& outB);   // 0x10017580
    // Hides UReferenceCounter::Dereference: GEPhysicalModel calls this one, GEModelManager calls the base explicitly
    // (0x10004370) when it drops its own reference.
    void Dereference();                     // 0x1001fa84
    long LoadTexturesForScreen(const GETextureDescription& inDescription, unsigned long inFlags);   // 0x1001cad5
    void PrepareTransforms();               // 0x1001fb1d
    void RemoveScale();                     // 0x1002004b
    long Save(FSFileSpec& inFile);          // 0x1001e4e8
    void SaveData(FSFileSpec& inFile);      // 0x1001eb5a
    void SetFractionalFramePosition(float inPosition);   // 0x1002009b
    void SetGraphicsCurrentFrame(unsigned long inFrame);   // 0x100200d7
    void SetGraphicsNextFrame(unsigned long inFrame);   // 0x1002012f
    void SetTexture(GETexture* inTexture, unsigned long inIndex);   // 0x100201b4
    unsigned long SizeOf();                 // 0x10020213
    void SortTagPoints();                   // 0x10020338
    bool Verify() const;                    // 0x10020495
protected:
    GEModel(GEModelManager* inManager);                                   // 0x1001ecbf
    GEModel(unsigned long inA, unsigned long inB, unsigned long inC, unsigned long inD, unsigned long inE,
            unsigned long inF, const UString& inName, unsigned long inG, GEModelManager* inManager);   // 0x1001edb9
    virtual ~GEModel();                     // 0x1001eed6
private:
    void SetParentModel(GEModel* inModel) { this->mParentModel = inModel; }
    void RemoveModelScale(unsigned long inFrame, const GETransformation& inParent, float inX, float inY, float inZ);   // 0x1001fc00
    void ComputeBoundingBoxes();            // 0x1001f544
    long AllocateInternalData(unsigned long inA, std::map<unsigned long, long>& ioTextures);   // 0x1001f080
    // Stores a model read by Rust (ee_ge_model_load) as the chunk readers did ('GEModel Load.cpp')
    long StoreLoaded(const EeGeCemModel* inModel, std::map<unsigned long, long>& ioTextures);
    long SaveCountsChunk(FSFileSpec& inFile);       // 0x1001e651
    long SaveFrameChunk(FSFileSpec& inFile);        // 0x1001e9c2
    long SaveHeaderChunk(FSFileSpec& inFile);       // 0x1001e5fb
    long SaveMaterialsChunk(FSFileSpec& inFile);    // 0x1001e83a
    long SaveMiscellaneousChunk(FSFileSpec& inFile);   // 0x1001e6ed
    long SavePolygonsChunk(FSFileSpec& inFile);     // 0x1001e716
    long SaveTagPointsChunk(FSFileSpec& inFile);    // 0x1001e988
    long SaveTagPointsSubChunk(FSFileSpec& inFile, unsigned long inFrame);   // 0x1001eb0b
    long SaveVertexSubChunk(FSFileSpec& inFile, unsigned long inFrame);      // 0x1001ea4e
    void SetSubModel(GEModel* inModel, unsigned long inIndex);   // 0x10020166

    // 'GEModel Caching.cpp' types (defined there; mangled as GEModel::<name>)
    struct TriListGenome;
    struct VertexListGenome;
    void ComputeInitialTriOrdering(TriListGenome& ioGenome);   // 0x10016dd8
    void ComputeInitialVertexOrdering(VertexListGenome& ioGenome, unsigned long inMaterial);   // 0x1001711c
    unsigned long ComputeMemoryCoherency(const VertexListGenome& inGenome, unsigned long inMaterial);   // 0x1001720e
    unsigned long ComputeVertexCacheMisses(const TriListGenome& inGenome);   // 0x10017686
    void MutateTriGenes(TriListGenome& ioGenome, URandomIntegers& inRandom, unsigned long inA, unsigned long inB);   // 0x10017735
    void MutateVertexGenes(VertexListGenome& ioGenome, URandomIntegers& inRandom, unsigned long inA, unsigned long inB);   // 0x1001784f
    void OptimizeMemoryCoherency(long inMaterial);   // 0x10017912
    void OptimizeVertexCaching(long inMaterial);     // 0x100180e1

    GETransformation mGraphicsPreModelTransform;    // +0x08
    GETransformation mGraphicsPostModelTransform;   // +0x48
    GETransformation mGraphicsTransform;            // +0x88
    GE3DPoint mCenterPoint;                 // +0xc8
    UString mName;                          // +0xd4 (CreateSubModel passes the parent's name)
    std::map<unsigned long, long> mSharedFrames;    // +0xe4 (provisional name; ~GEModel frees only frames not in it)
    GETransformation* mInternalTransforms;  // +0xf0 one per frame
    GEModelManager* mManager;               // +0xf4 (LoadCurrentVersion: new GEModel(mManager))
    GEModel* mParentModel;                  // +0xf8
    GEModel** mSubModels;                   // +0xfc
    GEPolygonArray** mPolygonArrays;        // +0x100 one per LOD
    GEVertexArray** mVertexArrays;          // +0x104 one per frame
    GEMaterial** mMaterials;                // +0x108
    GE3DPoint** mFrameTagPoints;            // +0x10c [frame][index]
    GEBoundingBox* mBoundingBoxes;          // +0x110
    GEBoundingBox* mGraphicsBoundingBox;    // +0x114
    GE3DPoint* mTagPoints;                  // +0x118
    UString* mTextureFilenames;             // +0x11c
    UString* mTagPointNames;                // +0x120
    float* mBoundingRadii;                  // +0x124
    float mGraphicsScale;                   // +0x128
    float mModelOpacity;                    // +0x12c
    float mFractionalFramePosition;         // +0x130
    unsigned long mGraphicsCurrentFrame;    // +0x134
    unsigned long mGraphicsNextFrame;       // +0x138
    unsigned long mSubModelCount;           // +0x13c
    unsigned long mFrameCount;              // +0x140
    unsigned long mMaterialCount;           // +0x144
    unsigned long mPolygonCount;            // +0x148
    unsigned long mVertexCount;             // +0x14c (SaveCountsChunk, SaveVertexSubChunk)
    unsigned long mTagPointCount;           // +0x150
    unsigned long mLODCount;                // +0x154
    bool mClipToPlane[6];                   // +0x158
    // (+0x160..+0x188 held the version-1 data while 'GEModel Legacy.cpp' converted it; the reader is Rust now)
};

// The LOD helper classes follow GEModel (GEBoundingBox.cpp emits their inline members right after GEModel's)
#ifndef LLE_EXE_HEADERS
#include "lle/GELOD.h"
#endif

// GEModel::SetClipToPlane plane index: GERasterizer::IsOrthographic/IsPerspectiveModelVisible test the viewport plane named here before each call
enum GEClipPlane {
    kClipPlaneFront = 0,  // GERasterizer::IsPerspectiveModelVisible tests GEViewport::GetFrontClippingDistance
    kClipPlaneRear = 1,  // GERasterizer::IsPerspectiveModelVisible tests GEViewport::GetRearClippingDistance
    kClipPlaneTop = 2,   // GERasterizer::IsPerspectiveModelVisible tests GEViewport::GetTopPlane
    kClipPlaneBottom = 3,  // GERasterizer::IsPerspectiveModelVisible tests GEViewport::GetBottomPlane
    kClipPlaneLeft = 4,  // GERasterizer::IsPerspectiveModelVisible tests GEViewport::GetLeftPlane
    kClipPlaneRight = 5,  // GERasterizer::IsPerspectiveModelVisible tests GEViewport::GetRightPlane
};

#endif
