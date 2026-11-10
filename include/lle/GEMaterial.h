// C:\EEX\Low-Level Engine\Graphics Engine\GEMaterial.h (RECONSTRUCTION). GEMaterial.cpp (~0x10016b9b) holds the ctor,
// dtor, SetTexture and SizeOf; the rest is inline (GEBoundingBox.cpp emits it first, 0x10010d9d..).
#ifndef LLE_GEMATERIAL_H
#define LLE_GEMATERIAL_H

class GETexture;

class LLE_API GEMaterial : public UReferenceCounter {   // sizeof 0x48 (vector deleting dtor 0x10ee2)
public:
    GEColor& GetColor() { return this->mColor; }
    unsigned long GetLODCount() { return this->mLODCount; }
    UString& GetName() { return this->mName; }
    float GetOpacity() { return this->mOpacity; }
    unsigned long GetPolygonStartingIndex(unsigned long inLOD) { return this->mPolygonStartingIndices[inLOD]; }
    unsigned long GetPolygonRangeLength(unsigned long inLOD) { return this->mPolygonRangeLengths[inLOD]; }
    GETexture* GetTexture(unsigned long inScreen) const { return this->mTextures[inScreen]; }
    unsigned long GetTextureIndex() { return this->mTextureIndex; }
    unsigned long GetVertexRangeLength() { return this->mVertexRangeLength; }
    unsigned long GetVertexStartingIndex() { return this->mVertexStartingIndex; }
    void SetColor(const GEColor& inColor) { this->mColor = inColor; }
    void SetName(const UString& inName) { this->mName = inName; }
    void SetOpacity(float inOpacity) { this->mOpacity = inOpacity; }
    void SetPolygonStartingIndex(unsigned long inIndex, unsigned long inLOD) { this->mPolygonStartingIndices[inLOD] = inIndex; }
    void SetPolygonRangeLength(unsigned long inLength, unsigned long inLOD) { this->mPolygonRangeLengths[inLOD] = inLength; }
    void SetTexture(GETexture* inTexture, unsigned long inScreen);
    void SetTextureIndex(unsigned long inIndex) { this->mTextureIndex = inIndex; }
    void SetVertexRangeLength(unsigned long inLength) { this->mVertexRangeLength = inLength; }
    void SetVertexStartingIndex(unsigned long inIndex) { this->mVertexStartingIndex = inIndex; }
    unsigned long SizeOf();
protected:
    virtual ~GEMaterial();
private:
    friend class GEModel;                   // AllocateInternalData news materials
    friend struct GEStoreDigest;            // tools/eemodel/storegate: reads what the loaders stored
    GEMaterial(unsigned long inScreenCount = 1, unsigned long inLODCount = 1);   // ??_F closure 0x10ed8: (1, 1)

    UString mName;                          // +0x08
    GEColor mColor;                         // +0x18
    GETexture** mTextures;                  // +0x24 one per screen
    unsigned long mScreenCount;             // +0x28 proven by the ctor (sizes mTextures) and dtor loop
    unsigned long* mPolygonStartingIndices; // +0x2c one per LOD
    unsigned long* mPolygonRangeLengths;    // +0x30
    unsigned long mVertexStartingIndex;     // +0x34
    unsigned long mVertexRangeLength;       // +0x38
    unsigned long mTextureIndex;            // +0x3c
    unsigned long mLODCount;                // +0x40
    float mOpacity;                         // +0x44
};

#endif
