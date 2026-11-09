// C:\EEX\Empire Earth\Graphics Thread\EEE3DShells.cpp (no __FILE__, no key chain; name from the RTTI class
// EEE3DShells > EEE3DTracers). 0xf90e9..0xf923f: only Virtual14 (its inline ctor and ??_G are emitted by
// EEEffectManager.cpp); between EEE3DScreenFlash.cpp and EEE3DShield.cpp.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/SESoundManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEMedia.h"
#include "ee/EEGameSound.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"
#include "ee/EEEffect3D.h"
#include "ee/EEE3DTracers.h"

// FUNCTION: EXE 0x4f90e9
// Draws the ejected shell model at its snapshot location, alpha blended, fading out over the second half of its flight
long EEE3DShells::DrawPass5()
{
    GETransformation theTransformation(this->mTransformation);
    GEModel* theModel = this->mGraphicObject->mModel;
    GEMaterial* theMaterial = theModel->GetMaterial(0);
    unsigned long theScreen = gScreenIndex;
    float theScale = this->mGraphicEffect->mSize;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    SetUnitLighting();
    theMaterial->SetTexture(this->mGraphicObject->mTextures[0], theScreen);
    theTransformation.SetPosition(this->mLocation);
    theModel->SetGraphicsPostModelTransform(theTransformation);
    theModel->SetGraphicsScale(theScale);
    theModel->SetOpacity(this->mDrawPosition >= 0.5f ? (1.0f - this->mDrawPosition) * 2.0f : 1.0f);
    theModel->PrepareTransforms();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
    theModel->SetOpacity(1.0f);
    return theResult;
}
