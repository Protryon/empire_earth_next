// C:\EEX\Empire Earth\Graphics Thread\EEE2DImpactBlood.cpp (no __FILE__; name from the RTTI class). 0xe1867..0xe1a37:
// the two file-static colours' init chains (red 0.8, then white), DrawPass6; EEE2DLightGlow.cpp follows.
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
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"
#include "lle/SESMInstanceNode.h"
#include "ee/EEE2DImpact.h"
#include "ee/EEE2DImpactBlood.h"

// SYNTHETIC: EXE 0x4e1867
// SYNTHETIC: EXE 0x4e1871
// SYNTHETIC: EXE 0x4e188c
// SYNTHETIC: EXE 0x4e1898
static const GEColor kImpactBloodColor(0.8f, 0.0f, 0.0f);
// SYNTHETIC: EXE 0x4e1899
// SYNTHETIC: EXE 0x4e18a3
// SYNTHETIC: EXE 0x4e18bc
// SYNTHETIC: EXE 0x4e18c8
static const GEColor kImpactBloodWhite;

// FUNCTION: EXE 0x4e18c9
// Draws the blood splash as a red-tinted alpha-blended sprite at its start point when blood is on, then resets
// the material to white; returns the draw result (0 with blood off)
long EEE2DImpactBlood::DrawPass6()
{
    long theResult = 0;
    if (gDrawBlood) {
        GETransformation theTransformation(gWorld.mTransformation);
        GEModel* theModel = this->mModel;
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
        GETexture* theTexture = this->mGraphicObject->mTextures[0];
        theTexture->SetFrame(this->mDrawFrame);
        theMaterial->SetTexture(theTexture, theScreen);
        theTransformation.SetPosition(this->mStart.GetX(), this->mStart.GetY(), this->mStart.GetZ());
        theModel->SetGraphicsPostModelTransform(theTransformation);
        theModel->SetGraphicsScale(theScale);
        theModel->PrepareTransforms();
        theModel->GetMaterial(0)->SetColor(kImpactBloodColor);
        bool theSorted = gOrthographicView;
        theResult = gRasterizer->Draw(theModel, theScreen, 1.0f, theSorted);
        theModel->GetMaterial(0)->SetColor(kImpactBloodWhite);
    }
    return theResult;
}
