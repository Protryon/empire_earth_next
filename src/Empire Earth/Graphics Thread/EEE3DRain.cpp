// C:\EEX\Empire Earth\Graphics Thread\EEE3DRain.cpp (no __FILE__; name from the RTTI class). 0xf7394..0xf8ceb: the
// ctors .. SetCursor (the sound helpers are inline COMDATs after their first users), then the template COMDATs of the
// two drop vectors.
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
#include <map>
#include "lle/U2DPoint.h"
#include "lle/SESMInstanceNode.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EESoundObject.h"
#include "ee/EEEffectManager.h"
#include "ee/EERenderObject.h"
#include "ee/EEE3DStorm.h"
#include "ee/EEE3DRain.h"
#include "ee/EERenderSnapshot.h"

// the unit's named float constants (.rdata 0x440be8..0x440bfc, in this order; reloaded at each use; the spawn's
// and the thunder's are ee_gfx effect_storm.rs's) (provisional names)
static const float kRainFadeTime = 6500.0f;     // 0x440be8 (ms to full strength)
static const float kRainDensity = 0.4f;         // 0x440bec
static const float kRainDropWidth = 0.04f;      // 0x440bf0
static const float kRainDropLength = 0.6f;      // 0x440bf4

// FUNCTION: EXE 0x4f7394
// Creates a rain storm over the given tiles for a graphic effect, with no sounds yet and first thunder within 2.25 s
EEE3DRain::EEE3DRain(const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inGraphicEffect)
    : EEE3DStorm(0x2a, inTiles, inGraphicEffect)
{
    this->mSoundID = 0;
    this->mRumbleID1 = 0;
    this->mRumbleID2 = 0;
    this->mSound = NULL;
    this->mRumble = NULL;
    ee_gfx_rain_new(this->mRecord);
}

// SYNTHETIC: EXE 0x4f7421
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4f743d
// Creates an empty rain storm (for loading), with no sounds
EEE3DRain::EEE3DRain()
    : EEE3DStorm(0x2a)
{
    this->mSoundID = 0;
    this->mRumbleID1 = 0;
    this->mRumbleID2 = 0;
    this->mSound = NULL;
    this->mRumble = NULL;
}

// FUNCTION: EXE 0x4f74a0
// Destroys the rain, stopping the rain and thunder sounds and unloading the three sound objects' media
EEE3DRain::~EEE3DRain()
{
    this->StopSound();
    this->StopRumble();
    EESoundObject* theSound = EESoundObject::Get(this->mSoundID);
    if (theSound)
        theSound->UnloadMedia();
    theSound = EESoundObject::Get(this->mRumbleID1);
    if (theSound)
        theSound->UnloadMedia();
    theSound = EESoundObject::Get(this->mRumbleID2);
    if (theSound)
        theSound->UnloadMedia();
}

// FUNCTION: EXE 0x4f7519
// Stops the rain sound, if any, and releases it
void EEE3DRain::StopSound()
{
    if (this->mSound) {
        gSoundManager->Stop(this->mSound, false);
        this->ReleaseSound();
    }
}

// FUNCTION: EXE 0x4f753e
// Stops the thunder sound, if any, and releases it
void EEE3DRain::StopRumble()
{
    if (this->mRumble) {
        gSoundManager->Stop(this->mRumble, false);
        this->ReleaseRumble();
    }
}

// FUNCTION: EXE 0x4f7563
// Drops the reference to the rain sound
void EEE3DRain::ReleaseSound()
{
    if (this->mSound)
        this->mSound->Dereference();
    this->mSound = NULL;
}

// FUNCTION: EXE 0x4f758d
// Drops the reference to the thunder sound
void EEE3DRain::ReleaseRumble()
{
    if (this->mRumble)
        this->mRumble->Dereference();
    this->mRumble = NULL;
}

// FUNCTION: EXE 0x4f7792
// Reads fall speed, spawn height, density, sound ids and duration from the effect and loads the sounds; on first use
// builds the shared raindrop triangle model with textures\raindrop
void EEE3DRain::Virtual64()
{
    static GEColor sColor(0.71f, 0.87f, 1.0f);
    GETextureDescription theDescription;
    GETextureCoordinate theCoordinate2(0.5f, 0.0f);
    GETextureCoordinate theCoordinate0(0.0f, 1.0f);
    GETextureCoordinate theCoordinate1(1.0f, 1.0f);
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mPixelDepth = 0x10;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    EEE3DStorm::Virtual64();
    this->mSoundID = theEffect->mIntParam1;
    this->mRumbleID1 = theEffect->mIntParam2;
    this->mRumbleID2 = theEffect->mIntParam3;
    EESoundObject* theSound = EESoundObject::Get(this->mSoundID);
    if (theSound)
        theSound->LoadMedia();
    theSound = EESoundObject::Get(this->mRumbleID1);
    if (theSound)
        theSound->LoadMedia();
    theSound = EESoundObject::Get(this->mRumbleID2);
    if (theSound)
        theSound->LoadMedia();
    if (this->mModel != NULL)
        return;
    GETexture* theTexture = gTextureManager->LoadTexture("textures\\raindrop", theDescription);
    theTexture->SetStageAlphaMode(kStageOpModulate, -1);
    theTexture->SetStageColorMode(kStageOpModulate, -1);
    theTexture->SetStageMinFilterMode(1, -1);
    theTexture->SetStageMagFilterMode(1, -1);
    theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
    theTexture->SetStageMipMapBias(-1.0f, -1);
    theTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
    this->mModel = gModelManager->CreateModel(1, 3, 1, 0, 1, 0, "EEE3DRainDropModel", 1);
    this->mModel->GetVertexArray(0)->SetPoint(GE3DPoint(kRainDropWidth * -0.5f, 0.0f, 0.0f), 0);
    this->mModel->GetVertexArray(0)->SetPoint(GE3DPoint(kRainDropWidth * 0.5f, 0.0f, 0.0f), 1);
    this->mModel->GetVertexArray(0)->SetPoint(GE3DPoint(0.0f, 0.0f, kRainDropLength), 2);
    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate0, 0);
    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate1, 1);
    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate2, 2);
    this->mModel->GetPolygonArray(0)->SetVertexIndex1(0, 0);
    this->mModel->GetPolygonArray(0)->SetVertexIndex2(0, 1);
    this->mModel->GetPolygonArray(0)->SetVertexIndex3(0, 2);
    this->mModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    this->mModel->GetMaterial(0)->SetPolygonRangeLength(1, 0);
    this->mModel->GetMaterial(0)->SetVertexStartingIndex(0);
    this->mModel->GetMaterial(0)->SetVertexRangeLength(3);
    this->mModel->GetMaterial(0)->SetOpacity(0.47f);
    this->mModel->GetMaterial(0)->SetColor(sColor);
    this->mModel->GetMaterial(0)->SetTexture(theTexture, gScreenIndex);
    this->mModel->ComputeModelNormals();
    this->mModel->ComputeBoundingVolumes();
}

// FUNCTION: EXE 0x4f7b54
// Destroys Virtual64's static raindrop colour at exit

// FUNCTION: EXE 0x4f7b55
// Draws every queued drop as the camera-facing drop model, alpha blended; stops at the first draw error
long EEE3DRain::DrawPass6()
{
    GETransformation theTransformation;
    long theResult = 0;
    unsigned long theScreen = gScreenIndex;
    this->BeginDraw();
    for (size_t i = 0; i < this->mDraws.size() && theResult == 0; i++) {
        const EeGe3DPoint& thePosition = this->mDraws[i].position;
        theTransformation.SetX(thePosition.x);
        theTransformation.SetY(thePosition.y);
        theTransformation.SetZ(thePosition.z);
        theTransformation.SetOrientationYPR(gRenderSnapshot.mCameraYaw, 0.0f, 0.0f);
        this->mModel->SetGraphicsPostModelTransform(theTransformation);
        this->mModel->PrepareTransforms();
        bool theSorted = gOrthographicView;
        theResult = gRasterizer->Draw(this->mModel, theScreen, 1.0f, theSorted);
    }
    return theResult;
}

// FUNCTION: EXE 0x4f7e3f
// Updates the sounds, then the storm's drops and age (ee_gfx effect_storm.rs); returns 7 once its duration has
// passed unless it is sustained
long EEE3DRain::Step()
{
    this->StartSounds();
    return EEE3DStorm::Step();
}

// FUNCTION: EXE 0x4f80e8
// Sets a storm sound's volume, pan and pitch from the camera's distance to the storm (range 5x its radius), scaled by
// inVolume, and by 0.4 during the first 6.5 s
void EEE3DRain::SetSoundLevel(SESMInstanceNode* inSound, float inVolume)
{
    GE3DPoint thePosition;
    if (!inSound)
        return;
    float theVolume;
    float thePan;
    float thePitch;
    this->mTransformation.GetPosition(thePosition);
    EEComputeSoundParameters(1.0f, thePosition, theVolume, thePan, thePitch, this->mBoundingRadius * 5.0f);
    theVolume *= inVolume;
    if (this->mElapsedTime < kRainFadeTime)
        theVolume = kRainDensity * theVolume;
    gSoundManager->SetSoundParameters(inSound, theVolume, thePan, thePitch);
}

// FUNCTION: EXE 0x4f8195
// Starts the rain sound when the storm begins or is restored, plays one of the two thunder sounds whenever the delay
// runs out (the timer and its draws: ee_gfx effect_storm.rs), and updates both sound levels
long EEE3DRain::StartSounds()
{
    if (this->mElapsedTime == 0.0f || this->mIsRestored) {
        EESoundObject* theSound = EESoundObject::Get(this->mSoundID);
        if (theSound) {
            this->SetSound(gSoundManager->PlayAndReturnReferencedNode(theSound->GetSampleID(), theSound->GetVolume(), 0.0f,
                                                                1.0f, theSound->GetLooping(), "3DRain"));
            if (this->mSound)
                this->mSound->Dereference();
        }
    }
    long theRumble = ee_gfx_rain_rumble(this->mRecord, gPhysics->GetTickTime());
    if (theRumble != 0) {
        EESoundObject* theSound = EESoundObject::Get(theRumble == 1 ? this->mRumbleID1 : this->mRumbleID2);
        if (theSound) {
            this->SetRumble(gSoundManager->PlayAndReturnReferencedNode(theSound->GetSampleID(), theSound->GetVolume(), 0.0f,
                                                                 1.0f, theSound->GetLooping(), "3D Rain Rumble"));
            if (this->mRumble)
                this->mRumble->Dereference();
        }
    }
    this->SetSoundLevel(this->mSound, 0.5f);
    this->SetSoundLevel(this->mRumble, 1.0f);
    return 0;
}

// FUNCTION: EXE 0x4f8337
// Replaces the rain sound, referencing the new one and releasing the old
void EEE3DRain::SetSound(SESMInstanceNode* inSound)
{
    if (inSound == this->mSound)
        return;
    this->ReleaseSound();
    this->mSound = inSound;
    if (inSound)
        inSound->Reference();
}

// FUNCTION: EXE 0x4f835e
// Replaces the thunder sound, referencing the new one and releasing the old
void EEE3DRain::SetRumble(SESMInstanceNode* inSound)
{
    if (inSound == this->mRumble)
        return;
    this->ReleaseRumble();
    this->mRumble = inSound;
    if (inSound)
        inSound->Reference();
}

// ICF-folded into 0xe123d
void EEE3DRain::PrepareQueue()
{
}
