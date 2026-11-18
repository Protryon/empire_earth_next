// C:\EEX\Low-Level Engine\Graphics Engine\GEMaterial.h (RECONSTRUCTION). A model's material: a name, a colour, a
// texture per screen and a polygon range per level of detail.
// port (R4.3, sixteenth part): the material is a whole Rust object (ee_ge smallobj.rs; GEMaterial.cpp deleted): its
// only member besides the reference count is the object, the methods forward, and GEObjectHost (GEBitmap.cpp) makes
// the calls on the name and the textures.
#ifndef LLE_GEMATERIAL_H
#define LLE_GEMATERIAL_H

#include "rust/render/ge.h"   // the material (ee_ge smallobj.rs)

class GETexture;
LLE_API intptr_t GEObjectHost(void* inOwner, EeGeObjOp inOp, void* inObj, intptr_t a, intptr_t b);   // GEBitmap.cpp

class LLE_API GEMaterial : public UReferenceCounter {
public:
    GEColor& GetColor() { return *reinterpret_cast<GEColor*>(ee_ge_mat_color(this->mRust)); }
    unsigned long GetLODCount() { return ee_ge_mat_get(this->mRust, EeGeMatField::LodCount, 0); }
    UString& GetName() { return *static_cast<UString*>(ee_ge_mat_name(this->mRust)); }
    float GetOpacity() { return ee_ge_mat_opacity(this->mRust); }
    unsigned long GetPolygonStartingIndex(unsigned long inLOD) { return ee_ge_mat_get(this->mRust, EeGeMatField::PolygonStart, inLOD); }
    unsigned long GetPolygonRangeLength(unsigned long inLOD) { return ee_ge_mat_get(this->mRust, EeGeMatField::PolygonLength, inLOD); }
    GETexture* GetTexture(unsigned long inScreen) const { return static_cast<GETexture*>(ee_ge_mat_texture(this->mRust, inScreen)); }
    unsigned long GetTextureIndex() { return ee_ge_mat_get(this->mRust, EeGeMatField::TextureIndex, 0); }
    unsigned long GetVertexRangeLength() { return ee_ge_mat_get(this->mRust, EeGeMatField::VertexLength, 0); }
    unsigned long GetVertexStartingIndex() { return ee_ge_mat_get(this->mRust, EeGeMatField::VertexStart, 0); }
    void SetColor(const GEColor& inColor) { this->GetColor() = inColor; }
    void SetName(const UString& inName) { ee_ge_mat_set_name(this->mRust, &inName); }
    void SetOpacity(float inOpacity) { ee_ge_mat_set_opacity(this->mRust, inOpacity); }
    void SetPolygonStartingIndex(unsigned long inIndex, unsigned long inLOD) { ee_ge_mat_set(this->mRust, EeGeMatField::PolygonStart, inLOD, inIndex); }
    void SetPolygonRangeLength(unsigned long inLength, unsigned long inLOD) { ee_ge_mat_set(this->mRust, EeGeMatField::PolygonLength, inLOD, inLength); }
    void SetTexture(GETexture* inTexture, unsigned long inScreen) { ee_ge_mat_set_texture(this->mRust, inTexture, inScreen); }
    void SetVertexRangeLength(unsigned long inLength) { ee_ge_mat_set(this->mRust, EeGeMatField::VertexLength, 0, inLength); }
    void SetVertexStartingIndex(unsigned long inIndex) { ee_ge_mat_set(this->mRust, EeGeMatField::VertexStart, 0, inIndex); }
    // the footprint of the original object (its members by value: a UString, a GEColor, three arrays, five fields)
    unsigned long SizeOf()
    {
        struct Layout : UReferenceCounter { UString n; GEColor c; void* t; unsigned long s; void* p[2]; unsigned long f[4]; float o; };
        return ee_ge_mat_size_of(this->mRust, sizeof(Layout), sizeof(UString), sizeof(GETexture*), sizeof(unsigned long));
    }
    EeGeMat* GetRust() const { return this->mRust; }
protected:
    virtual ~GEMaterial() { ee_ge_mat_destroy(this->mRust); }
private:
    friend class GEModel;                   // AllocateInternalData news materials
    friend struct GEStoreDigest;            // tools/eemodel/storegate
    GEMaterial(unsigned long inScreenCount = 1, unsigned long inLODCount = 1)
        : mRust(ee_ge_mat_new(&GEObjectHost, this, inScreenCount, inLODCount)) {}
    GEMaterial(const GEMaterial&) = delete;
    GEMaterial& operator=(const GEMaterial&) = delete;

    EeGeMat* mRust;                         // the material (ee_ge smallobj.rs)
};

#endif
