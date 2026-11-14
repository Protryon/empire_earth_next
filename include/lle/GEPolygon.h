// C:\EEX\Low-Level Engine\Graphics Engine\GEPolygon.h (RECONSTRUCTION). All inline (GEBoundingBox.cpp emits the block
// first, 0x10010f3c..0x10011154); the ctor is private (made by GEModel).
#ifndef LLE_GEPOLYGON_H
#define LLE_GEPOLYGON_H

class GEMaterial;

class LLE_API GEPolygon {                   // sizeof 0x6c: a triangle
public:
    unsigned long GetIndex() const { return this->mIndex; }
    GEMaterial* GetMaterial() const { return this->mMaterial; }
    float GetOpacity(unsigned long inVertex) const { return this->mOpacities[inVertex]; }
    void GetTextureCoordinate(GETextureCoordinate& outCoordinate, unsigned long inVertex)
        { outCoordinate = this->mTextureCoordinates[inVertex]; }
    void SetOpacity(float inOpacity)
    {
        this->mOpacities[0] = inOpacity;
        this->mOpacities[1] = inOpacity;
        this->mOpacities[2] = inOpacity;
    }
    void SetOpacity(float inOpacity, unsigned long inVertex) { this->mOpacities[inVertex] = inOpacity; }
    void SetTextureCoordinate(GETextureCoordinate& inCoordinate, unsigned long inVertex)
        { this->mTextureCoordinates[inVertex] = inCoordinate; }
    void SetVertex(unsigned long inIndex, unsigned long inVertex) { this->mVertices[inVertex] = inIndex; }
private:
    GEPolygon(unsigned long inIndex)        // 0x11053
    {
        this->mMaterial = NULL;
        this->mOpacities[0] = 1.0f;
        this->mIndex = inIndex;
        this->mOpacities[1] = 1.0f;
        this->mOpacities[2] = 1.0f;
        this->mFogIntensities[0] = 1.0f;
        this->mFogIntensities[1] = 1.0f;
        this->mFogIntensities[2] = 1.0f;
    }

    GEColor mBaseColors[3];                 // +0x00
    GETextureCoordinate mTextureCoordinates[3];   // +0x24
    GEMaterial* mMaterial;                  // +0x3c
    float mOpacities[3];                    // +0x40
    float mFogIntensities[3];               // +0x4c
    unsigned long mVertices[3];             // +0x58 vertex array indices
    unsigned long mIndex;                   // +0x64
    bool mIsUsed;                           // +0x68
};

#endif
