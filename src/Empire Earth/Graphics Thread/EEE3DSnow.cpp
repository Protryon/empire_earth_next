// C:\EEX\Empire Earth\Graphics Thread\EEE3DSnow.cpp (no __FILE__; name from the RTTI class). 0xf9b6d..0xfb088: the
// ctors .. Virtual6C with EEE3DSnowFlake's ctor and Update after their first users, then the template COMDATs of the
// two vectors.
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
#include "lle/GEModelManager.h"
#include "lle/GEPolygonArray.h"
#include <math.h>
#include <map>
#include "lle/U2DPoint.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EERenderObject.h"
#include "ee/EEE3DStorm.h"
#include "ee/EEE3DSnow.h"
#include "ee/EERenderSnapshot.h"

// the unit's named float constants (.rdata 0x440da4..0x440dc0, in this order; reloaded at each use; the spawn's,
// drift's and spin's are ee_gfx effect_storm.rs's) (provisional names)
static const float kSnowFlakeWidth = 0.054f;        // 0x440dac
static const float kSnowFlakeHeight = 0.047f;       // 0x440db0
static const float kSnowFadeDistance = 1.0f;        // 0x440db4 (flakes closer to the camera fade)

// FUNCTION: EXE 0x4f9b6d
// Creates a snow storm over the given tiles for a graphic effect, 5 s long, with no flakes yet
EEE3DSnow::EEE3DSnow(const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inGraphicEffect)
    : EEE3DStorm(0x3d, inTiles, inGraphicEffect)
{
}

// SYNTHETIC: EXE 0x4f9bf5
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4f9c11
// Creates an empty snow storm (for loading)
EEE3DSnow::EEE3DSnow()
    : EEE3DStorm(0x3d)
{
}

// FUNCTION: EXE 0x4f9c87
// Destroys the snow storm (members and base only)
EEE3DSnow::~EEE3DSnow()
{
}

// FUNCTION: EXE 0x4f9ef5
// Reads fall speed, spawn height, density and duration from the effect; on first use builds the shared snowflake quad
// model with textures\snowflake
void EEE3DSnow::Virtual64()
{
    GETextureCoordinate theCoordinate0(0.0f, 0.0f);
    GETextureCoordinate theCoordinate1(1.0f, 0.0f);
    GETextureDescription theDescription;
    GETextureCoordinate theCoordinate3(0.0f, 1.0f);
    GETextureCoordinate theCoordinate2(1.0f, 1.0f);
    theDescription.mPixelDepth = 0x10;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    theDescription.mSurfaceVolatility = 1;
    EEE3DStorm::Virtual64();
    if (this->mModel != NULL)
        return;
    GETexture* theTexture = gTextureManager->LoadTexture("textures\\snowflake", theDescription);
    theTexture->SetStageAlphaMode(kStageOpModulate, -1);
    theTexture->SetStageColorMode(kStageOpModulate, -1);
    theTexture->SetStageMinFilterMode(1, -1);
    theTexture->SetStageMagFilterMode(1, -1);
    theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
    theTexture->SetStageMipMapBias(-1.0f, -1);
    theTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
    this->mModel = gModelManager->CreateModel(2, 4, 1, 0, 1, 0, "EEE3DSnowFlakeModel", 1);
    this->mModel->GetVertexArray(0)->SetPoint(GE3DPoint(kSnowFlakeWidth * -0.5f, 0.0f, kSnowFlakeHeight), 0);
    this->mModel->GetVertexArray(0)->SetPoint(GE3DPoint(kSnowFlakeWidth * 0.5f, 0.0f, kSnowFlakeHeight), 1);
    this->mModel->GetVertexArray(0)->SetPoint(GE3DPoint(kSnowFlakeWidth * 0.5f, 0.0f, 0.0f), 2);
    this->mModel->GetVertexArray(0)->SetPoint(GE3DPoint(kSnowFlakeWidth * -0.5f, 0.0f, 0.0f), 3);
    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate0, 0);
    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate1, 1);
    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate2, 2);
    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate3, 3);
    this->mModel->GetPolygonArray(0)->SetVertexIndex1(0, 0);
    this->mModel->GetPolygonArray(0)->SetVertexIndex2(0, 1);
    this->mModel->GetPolygonArray(0)->SetVertexIndex3(0, 2);
    this->mModel->GetPolygonArray(0)->SetVertexIndex1(1, 0);
    this->mModel->GetPolygonArray(0)->SetVertexIndex2(1, 2);
    this->mModel->GetPolygonArray(0)->SetVertexIndex3(1, 3);
    this->mModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    this->mModel->GetMaterial(0)->SetPolygonRangeLength(2, 0);
    this->mModel->GetMaterial(0)->SetVertexStartingIndex(0);
    this->mModel->GetMaterial(0)->SetVertexRangeLength(4);
    this->mModel->GetMaterial(0)->SetOpacity(0.8f);
    this->mModel->GetMaterial(0)->SetTexture(theTexture, gScreenIndex);
    this->mModel->ComputeModelNormals();
    this->mModel->ComputeBoundingVolumes();
}

// FUNCTION: EXE 0x4fa404
// Draws every queued flake as the camera-facing spinning quad, alpha blended and fading within 1 unit of the camera;
// stops at the first draw error
long EEE3DSnow::DrawPass6()
{
    GETransformation theTransformation;
    long theResult = 0;
    unsigned long theScreen = gScreenIndex;
    this->BeginDraw();
    for (size_t i = 0; i < this->mDraws.size() && theResult == 0; i++) {
        const EeGfxStormDraw& theDraw = this->mDraws[i];
        theTransformation.SetX(theDraw.position.x);
        theTransformation.SetY(theDraw.position.y);
        theTransformation.SetZ(theDraw.position.z);
        theTransformation.SetOrientationYPR(gRenderSnapshot.mCameraYaw, 0.0f, theDraw.angle);
        double theDistance = sqrt(gRenderSnapshot.mCamera.DistanceSquaredTo(theTransformation));
        float theOpacity;
        if ((float)theDistance < kSnowFadeDistance)
            theOpacity = theDistance / kSnowFadeDistance;
        else
            theOpacity = 1.0f;
        this->mModel->SetOpacity(theOpacity);
        this->mModel->SetGraphicsPostModelTransform(theTransformation);
        this->mModel->PrepareTransforms();
        bool theSorted = gOrthographicView;
        theResult = gRasterizer->Draw(this->mModel, theScreen, 1.0f, theSorted);
    }
    return theResult;
}

// ICF-folded into 0xe123d
void EEE3DSnow::PrepareQueue()
{
}
