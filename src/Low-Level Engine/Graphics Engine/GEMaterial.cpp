// C:\EEX\Low-Level Engine\Graphics Engine\GEMaterial.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10016a7b..0x10016cb8: the U2D key init chains, the ctor, dtor, SetTexture and SizeOf. Every other GEMaterial
// member is header-inline, emitted first by GEBoundingBox.cpp.
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/TSScheduler.h"
#include "lle/UReferenceCounter.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GEMaterial.h"
#include "rust/ee_ge.h"       // ee_ge_material_size_of (ee_ge texture.rs)

// Creates an untextured, opaque material with a texture slot per screen and a polygon range per detail level
GEMaterial::GEMaterial(unsigned long inScreenCount, unsigned long inLODCount)
    : mScreenCount(inScreenCount)
{
    unsigned long i;
    this->mTextures = new GETexture*[inScreenCount];
    this->mPolygonStartingIndices = new unsigned long[inLODCount];
    this->mPolygonRangeLengths = new unsigned long[inLODCount];
    for (i = 0; i < this->mScreenCount; i++)
        this->mTextures[i] = NULL;
    this->mLODCount = inLODCount;
    this->mOpacity = 1.0f;
    this->mTextureIndex = 0;
}

// Destroys the material, releasing the texture of every screen and freeing its polygon ranges
GEMaterial::~GEMaterial()
{
    unsigned long i;
    for (i = 0; i < this->mScreenCount; i++)
        if (this->mTextures[i])
            this->mTextures[i]->Dereference();
    delete[] this->mTextures;
    delete[] this->mPolygonStartingIndices;
    delete[] this->mPolygonRangeLengths;
}

// Sets the material's texture for one screen, releasing the old one and referencing the new one
void GEMaterial::SetTexture(GETexture* inTexture, unsigned long inScreen)
{
    if (this->mTextures[inScreen])
        this->mTextures[inScreen]->Dereference();
    this->mTextures[inScreen] = inTexture;
    if (inTexture)
        inTexture->Reference();
}

// Returns the material's memory footprint: the object, its name, texture slots and per-level polygon ranges
unsigned long GEMaterial::SizeOf()
{
    return ee_ge_material_size_of(sizeof(GEMaterial), sizeof(UString), this->mName.SizeOf(), this->mScreenCount,
                                  sizeof(GETexture*), this->mLODCount, sizeof(unsigned long));
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
