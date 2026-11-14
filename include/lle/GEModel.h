// C:\EEX\Low-Level Engine\Graphics Engine\GEModel.h (RECONSTRUCTION). A loaded/created 3D model; reference
// counted, owned by a GEModelManager. GEModel.cpp (0x1001eb9f..) and the 'GEModel <part>.cpp' units hold its members.
// Layout from the implicit copy ctor (0x10011536) and operator= (0x10011799) and the inline accessors, all emitted
// first by GEBoundingBox.cpp (0x100112f3..0x10011a4e); provisional names unless an export names them.
//
// port (R4.3, ninth part): the model is a whole Rust object (ee_ge modobj.rs): its transforms, centre, name, shared
// frames, frame arrays, LOD polygon arrays, materials, names, sub-models and counts with every method. The class's
// only member is that object; the accessors below forward to it (the arrays they index are its own), and GEModel.cpp
// is the forwarders and the host (the calls the model makes on other objects).
#ifndef LLE_GEMODEL_H
#define LLE_GEMODEL_H

#include <map>
#include <stdint.h>
#include "lle/GEBoundingBox.h"
#include "rust/ee_ge.h"   // the model (ee_ge modobj.rs), its host's EeGeModOp

class FSFileSpec;
class GEMaterial;
class GEModelManager;
class GEPolygonArray;
class GETexture;
class GEVertexArray;
struct GETextureDescription;

class LLE_API GEModel : public UReferenceCounter {   // sizeof 0x188 in the original (GEModelManager: operator new(0x188))
    friend class GEModelManager;
    friend class GEPhysicalModel;           // Intersects reads the const model's arrays (provisional name)
    friend struct GEStoreDigest;            // tools/eemodel/storegate: reads what the loaders stored
public:
    // inline members in declaration order (the folded ones sit at other addresses)
    const GEBoundingBox& GetBoundingBox(unsigned long inLOD) const { return this->GetBoundingBoxes()[inLOD]; }
    float GetBoundingRadius(unsigned long inLOD) const { return this->GetBoundingRadii()[inLOD]; }
    const GE3DPoint& GetCenterPoint() const { return *(const GE3DPoint*)this->Ptr(EeGeModPtr::Center); }
    bool GetClipToPlane(unsigned long inPlane) { return ee_ge_mod_clip(this->mRust, inPlane); }
    unsigned long GetFrameCount() const { return this->Val(EeGeModVal::FrameCount); }
    float GetFractionalFramePosition() const
        { return ee_ge_mod_getf(this->mRust, EeGeModFloat::FractionalFramePosition); }
    const GEBoundingBox& GetGraphicsBoundingBox() const { return *this->GetGraphicsBoundingBoxPointer(); }
    float GetGraphicsBoundingRadius() const { return this->GetBoundingRadius(this->GetGraphicsCurrentFrame()); }
    unsigned long GetGraphicsCurrentFrame() const { return this->Val(EeGeModVal::CurrentFrame); }
    unsigned long GetGraphicsNextFrame() const { return this->Val(EeGeModVal::NextFrame); }
    const GETransformation& GetGraphicsPostModelTransform() const
        { return *(const GETransformation*)this->Ptr(EeGeModPtr::PostTransform); }
    const GETransformation& GetGraphicsPreModelTransform() const
        { return *(const GETransformation*)this->Ptr(EeGeModPtr::PreTransform); }
    float GetGraphicsScale() const { return ee_ge_mod_getf(this->mRust, EeGeModFloat::GraphicsScale); }
    const GETransformation& GetGraphicsTransform() const
        { return *(const GETransformation*)this->Ptr(EeGeModPtr::GraphicsTransform); }
    const GETransformation& GetInternalTransform(unsigned long inFrame) const
        { return this->GetInternalTransforms()[inFrame]; }
    unsigned long GetLODCount() const { return this->Val(EeGeModVal::LodCount); }
    GEMaterial* GetMaterial(unsigned long inIndex) const { return this->GetMaterials()[inIndex]; }
    unsigned long GetMaterialCount() const { return this->Val(EeGeModVal::MaterialCount); }
    float GetModelOpacity() const { return ee_ge_mod_getf(this->mRust, EeGeModFloat::Opacity); }
    const UString& GetName() const { return *(const UString*)this->Ptr(EeGeModPtr::Name); }
    GEModel* GetParentModel() const { return (GEModel*)ee_ge_mod_get(this->mRust, EeGeModVal::Parent); }
    GEPolygonArray* GetPolygonArray(unsigned long inLOD) { return this->GetPolygonArrays()[inLOD]; }
    unsigned long GetPolygonCount() const { return this->Val(EeGeModVal::PolygonCount); }
    unsigned long GetPolygonCount(unsigned long inLOD) const;   // 0x1001fab0
    GEVertexArray* GetVertexArray(unsigned long inFrame) { return this->GetVertexArrays()[inFrame]; }
    GEModel* GetSubModel(unsigned long inIndex) const { return this->GetSubModels()[inIndex]; }
    unsigned long GetSubModelCount() const { return this->Val(EeGeModVal::SubModelCount); }
    const GE3DPoint& GetTagPoint(unsigned long inIndex) const { return this->GetTagPointList()[inIndex]; }
    const GE3DPoint& GetTagPoint(unsigned long inIndex, unsigned long inFrame) const
        { return this->GetFrameTagPoints()[inFrame][inIndex]; }
    unsigned long GetTagPointCount() const { return this->Val(EeGeModVal::TagPointCount); }
    GE3DPoint* GetTagPointList() const { return (GE3DPoint*)this->Ptr(EeGeModPtr::TagPoints); }
    const UString& GetTagPointName(unsigned long inIndex) const { return this->GetTagPointNames()[inIndex]; }
    void SetClipToPlane(unsigned long inPlane, bool inClip) { ee_ge_mod_set_clip(this->mRust, inPlane, inClip); }
    void SetGraphicsPreModelTransform(const GETransformation& inTransform)
        { ee_ge_mod_set_transform(this->mRust, EeGeModXf::Pre, 0, (const EeGeTransformation*)&inTransform); }
    void SetGraphicsPostModelTransform(const GETransformation& inTransform)
        { ee_ge_mod_set_transform(this->mRust, EeGeModXf::Post, 0, (const EeGeTransformation*)&inTransform); }
    void SetGraphicsScale(float inScale) { ee_ge_mod_set_graphics_scale(this->mRust, inScale); }
    void SetInternalTransform(unsigned long inFrame, GETransformation& inTransform) const
        { ee_ge_mod_set_transform(this->mRust, EeGeModXf::Internal, inFrame, (const EeGeTransformation*)&inTransform); }
    void SetOpacity(float inOpacity) { ee_ge_mod_set_opacity(this->mRust, inOpacity); }
    void SetTagPoint(unsigned long inIndex, const GE3DPoint& inPoint)
        { ee_ge_mod_set_tag_point(this->mRust, -1, inIndex, (const EeGe3DPoint*)&inPoint); }
    void SetTagPoint(unsigned long inFrame, unsigned long inIndex, const GE3DPoint& inPoint)
        { ee_ge_mod_set_tag_point(this->mRust, (long)inFrame, inIndex, (const EeGe3DPoint*)&inPoint); }
    void SetTagPointName(const UString& inName, unsigned long inIndex)
        { const_cast<UString*>(this->GetTagPointNames())[inIndex] = inName; }
    void SetTextureFilename(const UString& inName, unsigned long inIndex)
        { const_cast<UString*>(this->GetTextureFilenames())[inIndex] = inName; }

    // vtable 0x10097adc: slot 0 the deleting dtor, slot 1 Load (0x1001c3b4, 'GEModel Load.cpp': the .cem reader,
    // retired under D9; GEModelManager::LoadModel stores a glTF's model with StoreLoaded)
    void ComputeBoundingVolumes();          // 0x1001f69a
    // (ComputeLODReduction 0x1001ce5b, 'GEModel LOD.cpp', and the vertex cache optimiser of 'GEModel Caching.cpp'
    // (ComputeOptimalDataOrdering 0x10017523, ComputeVertexCacheEfficiency 0x10017580,
    // ComputeMemoryLocalityEfficiency 0x100173ba) had no caller in the game, the tools or the data path: deleted)
    void ComputeModelNormals();             // 0x1001f80c
    // Hides UReferenceCounter::Dereference: GEPhysicalModel calls this one, GEModelManager calls the base explicitly
    // (0x10004370) when it drops its own reference.
    void Dereference();                     // 0x1001fa84
    void PrepareTransforms();               // 0x1001fb1d
    // (Save 0x1001e4e8 and SaveData 0x1001eb5a wrote .cem files: retired under D9)
    void SetFractionalFramePosition(float inPosition);   // 0x1002009b
    void SetGraphicsCurrentFrame(unsigned long inFrame);   // 0x100200d7
    void SetGraphicsNextFrame(unsigned long inFrame);   // 0x1002012f
    void SetTexture(GETexture* inTexture, unsigned long inIndex);   // 0x100201b4
    unsigned long SizeOf();                 // 0x10020213
    void SortTagPoints();                   // 0x10020338
    bool Verify() const;                    // 0x10020495
    GEModel(const GEModel&) = delete;
    GEModel& operator=(const GEModel&) = delete;
protected:
    GEModel(GEModelManager* inManager);                                   // 0x1001ecbf
    GEModel(unsigned long inA, unsigned long inB, unsigned long inC, unsigned long inD, unsigned long inE,
            unsigned long inF, const UString& inName, unsigned long inG, GEModelManager* inManager);   // 0x1001edb9
    virtual ~GEModel();                     // 0x1001eed6
private:
    // (SetParentModel, RemoveModelScale 0x1001fc00, ComputeBoundingBoxes 0x1001f544 and AllocateInternalData
    // 0x1001f080 are the Rust object's)
    // Stores a model decoded by Rust (a glTF: ee_of_gltf_to_model) as the original's .cem chunk readers stored it
    // ('GEModel Load.cpp')
    long StoreLoaded(const EeGeCemModel* inModel, std::map<unsigned long, long>& ioTextures);
    void SetSubModel(GEModel* inModel, unsigned long inIndex);   // 0x10020166
    // what the friends read (the store gate's digest), as accessors
    unsigned long GetVertexCount() const { return this->Val(EeGeModVal::VertexCount); }
    unsigned long GetSharedFrameCount() const { return this->Val(EeGeModVal::SharedFrameCount); }
    void GetSharedFrame(unsigned long inIndex, unsigned long& outFrame, long& outOffset) const
        { ee_ge_mod_shared_frame(this->mRust, inIndex, &outFrame, &outOffset); }
    const GETransformation* GetInternalTransforms() const
        { return (const GETransformation*)this->Ptr(EeGeModPtr::InternalTransforms); }
    const GEBoundingBox* GetBoundingBoxes() const { return (const GEBoundingBox*)this->Ptr(EeGeModPtr::BoundingBoxes); }
    const GEBoundingBox* GetGraphicsBoundingBoxPointer() const
        { return (const GEBoundingBox*)this->Ptr(EeGeModPtr::GraphicsBoundingBox); }
    const float* GetBoundingRadii() const { return (const float*)this->Ptr(EeGeModPtr::BoundingRadii); }
    GE3DPoint* const* GetFrameTagPoints() const { return (GE3DPoint* const*)this->Ptr(EeGeModPtr::FrameTagPoints); }
    GEMaterial* const* GetMaterials() const { return (GEMaterial* const*)this->Ptr(EeGeModPtr::Materials); }
    const UString* GetTextureFilenames() const { return (const UString*)this->Ptr(EeGeModPtr::TextureFilenames); }
    const UString* GetTagPointNames() const { return (const UString*)this->Ptr(EeGeModPtr::TagPointNames); }
    GEPolygonArray* const* GetPolygonArrays() const
        { return (GEPolygonArray* const*)this->Ptr(EeGeModPtr::PolygonArrays); }
    GEVertexArray* const* GetVertexArrays() const { return (GEVertexArray* const*)this->Ptr(EeGeModPtr::VertexArrays); }
    GEModel* const* GetSubModels() const { return (GEModel* const*)this->Ptr(EeGeModPtr::SubModels); }

    void* Ptr(EeGeModPtr inField) const { return ee_ge_mod_ptr(this->mRust, inField); }
    unsigned long Val(EeGeModVal inField) const { return (unsigned long)ee_ge_mod_get(this->mRust, inField); }
    // The calls the model makes on other objects (EeGeModOp), in the original's statements
    static intptr_t Host(void* inOwner, EeGeModOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c);

    EeGeModel* mRust;                       // the model (ee_ge modobj.rs)
};

// (The LOD helper classes, lle/GELOD.h, followed GEModel here: deleted with 'GEModel LOD.cpp')

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
