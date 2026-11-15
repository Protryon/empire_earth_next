// C:\EEX\Low-Level Engine\Graphics Engine\GETextureSet.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100346a7..0x100349a3: ctor, dtor, SetTexture, then the vector<vector<GETexture*> > COMDATs (resize, erase,
// _Construct). The accessors are header-inline (GEBoundingBox.cpp emits them). No init chains.
//
// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x1003484f
// TEMPLATE: LLE 0x10034875
// TEMPLATE: LLE 0x10034928
// TEMPLATE: LLE 0x10034991
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UReferenceCounter.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GETextureSet.h"

// FUNCTION: LLE 0x100346a7
// Creates a width x height grid of empty texture slots
GETextureSet::GETextureSet(unsigned long inNumTexturesInWidth, unsigned long inNumTexturesInHeight,
                           unsigned long inTextureTileCoverage)
{
    this->mNumTexturesInWidth = inNumTexturesInWidth;
    this->mNumTexturesInHeight = inNumTexturesInHeight;
    this->mTextureTileCoverage = inTextureTileCoverage;
    this->mTextures.resize(inNumTexturesInWidth);
    for (unsigned long i = 0; i < inNumTexturesInWidth; i++)
        this->mTextures[i].resize(inNumTexturesInHeight, NULL);
}

// FUNCTION: LLE 0x100346f8
// Destroys the texture grid, releasing every texture in it
GETextureSet::~GETextureSet()
{
    for (unsigned long i = 0; i < this->mNumTexturesInWidth; i++) {
        for (unsigned long j = 0; j < this->mNumTexturesInHeight; j++) {
            if (this->mTextures[i][j] != NULL)
                this->mTextures[i][j]->Dereference();
        }
    }
}

// FUNCTION: LLE 0x1003474b
// Puts a texture into a grid slot, releasing the old one and referencing the new one
void GETextureSet::SetTexture(unsigned long inX, unsigned long inY, GETexture* inTexture)
{
    if (this->mTextures[inX][inY] != NULL)
        this->mTextures[inX][inY]->Dereference();
    this->mTextures[inX][inY] = inTexture;
    if (this->mTextures[inX][inY] != NULL)
        this->mTextures[inX][inY]->Reference();
}
