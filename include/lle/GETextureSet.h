// C:\EEX\Low-Level Engine\Graphics Engine\GETextureSet.h (RECONSTRUCTION). A grid of textures tiled over the terrain.
// GETextureSet.cpp (0x100346a7..0x100349a3) holds the ctor, dtor and SetTexture; the rest is inline (GEBoundingBox.cpp
// emits it first, 0x10012426..).
#ifndef LLE_GETEXTURESET_H
#define LLE_GETEXTURESET_H

#include <vector>
#include "rust/render/ge.h"       // the grid arithmetic (ee_ge texture.rs)

#include "lle/LLEForward.h"

class LLE_API GETextureSet {                // whole in Rust (ee_render ge texset.rs)
public:
    GETextureSet(unsigned long inNumTexturesInWidth, unsigned long inNumTexturesInHeight, unsigned long inTextureTileCoverage)
        { this->mRust = ee_ge_tset_new(inNumTexturesInWidth, inNumTexturesInHeight, inTextureTileCoverage, Dereference); }
    ~GETextureSet() { ee_ge_tset_free(this->mRust); }
    GETexture* GetTexture(unsigned long inX, unsigned long inY) { return static_cast<GETexture*>(ee_ge_tset_get(this->mRust, inX, inY, true)); }
    GETexture* GetTextureAt(unsigned long inColumn, unsigned long inRow)
        { return static_cast<GETexture*>(ee_ge_tset_get(this->mRust, inColumn, inRow, false)); }
    void SetTexture(unsigned long inX, unsigned long inY, GETexture* inTexture) { ee_ge_tset_set(this->mRust, inX, inY, inTexture); }

    static void Dereference(void* inTexture);   // GETexture.cpp: the texture's Dereference
    URust<EeGeTextureSet> mRust;
};

#endif
