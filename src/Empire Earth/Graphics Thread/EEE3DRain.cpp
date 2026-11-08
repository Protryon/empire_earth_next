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

// the unit's named float constants (.rdata 0x440be8..0x440bfc, in this order; reloaded at each use) (provisional names)
static const float kRainFadeTime = 6500.0f;     // 0x440be8 (ms to full strength)
static const float kRainDensity = 0.4f;         // 0x440bec
static const float kRainDropWidth = 0.04f;      // 0x440bf0
static const float kRainDropLength = 0.6f;      // 0x440bf4
static const float kRainUnused = 0.2f;          // 0x440bf8 (no user found)
static const float kRainRumbleDelay = 9000.0f;  // 0x440bfc

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
    this->mDropCount = 0;
    float theDelay = GetEffectRandomFloat(0.0f, 0.25f) * kRainRumbleDelay;
    this->mRumbleDelay = theDelay;
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
    this->mDropCount = 0;
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

// FUNCTION: EXE 0x4f75b7
// Adds a raindrop at a random point of a random storm tile at spawn height, falling to the ground (0 over water, where
// it may splash)
long EEE3DRain::AddDrop()
{
    EETileCoordinate& theTile = this->mTiles[GetEffectRandomLong(0, this->mTiles.size() - 1)];
    float theRandom = GetEffectRandomFloat(0.0f, 1.0f);
    float theX = theTile.mX + theRandom;
    theRandom = GetEffectRandomFloat(0.0f, 1.0f);
    float theY = theTile.mY + theRandom;
    GE3DPoint thePosition(theX, theY, this->mSpawnHeight);
    EEMapTile* theMapTile = gWorld.GetMap()->GetTile(theTile.mX, theTile.mY);
    // Test the tile pointer before reading its terrain.
    bool theWater = theMapTile && (theMapTile->mTerrainType == kTerrainDeepWater || theMapTile->mTerrainType == kTerrainShallowWater);
    float theGround = theWater ? 0.0f : gWorld.GetMap()->mTerrainMesh.GetZ(theTile.mX, theTile.mY);
    this->mDrops.push_back(EE3DRainDrop(thePosition, theGround, theRandom * 6.2831855f, theWater));
    return 0;
}

// FUNCTION: EXE 0x4f76da
// Copies the drops into the draw list and queues the rain in render queue 4 under its texture
long EEE3DRain::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDraws.clear();
        for (std::vector<EE3DRainDrop>::iterator theDrop = this->mDrops.begin(); theDrop != this->mDrops.end(); theDrop++)
            this->mDraws.push_back(EE3DRainDraw(theDrop->mPosition, theDrop->mAngle, theDrop->mWater));
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
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
    this->mFallSpeed = theEffect->GetParam1();
    this->mSpawnHeight = theEffect->GetParam2();
    this->mDensity = theEffect->GetParam3();
    this->mSoundID = theEffect->mIntParam1;
    this->mRumbleID1 = theEffect->mIntParam2;
    this->mRumbleID2 = theEffect->mIntParam3;
    this->mDuration = theEffect->mDuration;
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
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    SetCloudLighting();
    SetSceneFog();
    for (std::vector<EE3DRainDraw>::iterator theDraw = this->mDraws.begin(); theDraw != this->mDraws.end() && theResult == 0;
         theDraw++) {
        theTransformation.SetX(theDraw->mPosition.GetX());
        theTransformation.SetY(theDraw->mPosition.GetY());
        theTransformation.SetZ(theDraw->mPosition.GetZ());
        theTransformation.SetOrientationYPR(gWorld.GetCameraYaw(), 0.0f, 0.0f);
        this->mModel->SetGraphicsPostModelTransform(theTransformation);
        this->mModel->PrepareTransforms();
        bool theSorted = gOrthographicView;
        theResult = gRasterizer->Draw(this->mModel, theScreen, 1.0f, theSorted);
    }
    return theResult;
}

// FUNCTION: EXE 0x4f7c83
// Reads the storm, drop count, thunder delay, a position (discarded), the bounding radius and the number of drops
// to recreate; returns the first read error
long EEE3DRain::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    GE3DPoint thePosition;
    float theRadius = 0.0f;
    long theResult = EEE3DStorm::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0)
        theResult = inFile.Read(this->mDropCount);
    if (theResult == 0)
        theResult = inFile.Read(this->mRumbleDelay);
    if (theResult == 0)
        theResult = thePosition.Load(inFile);
    if (theResult == 0)
        theResult = inFile.Read(theRadius);
    if (theResult == 0) {
        this->SetBoundingRadius(theRadius);
        theResult = inFile.Read(this->mLoadedDrops);
    }
    return theResult;
}

// FUNCTION: EXE 0x4f7d0f
// After loading, reloads the settings, recreates the saved number of drops at random heights and marks the storm
// restored so its sounds restart
long EEE3DRain::RepairPointers()
{
    long theResult = EEE3DStorm::RepairPointers();
    if (theResult == 0) {
        this->Virtual64();
        for (unsigned long i = 0; i < this->mLoadedDrops && theResult == 0; i++)
            theResult = this->AddDrop();
        for (std::vector<EE3DRainDrop>::iterator theDrop = this->mDrops.begin(); theDrop != this->mDrops.end(); theDrop++)
            theDrop->mPosition.SetZ(EEEffect::GetRandom() * this->mSpawnHeight);
        this->mLoadedDrops = 0;
        this->mIsRestored = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4f7d7f
// Writes the storm, drop count, thunder delay, position,
// radius and live drop count; returns the first write error
long EEE3DRain::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    GE3DPoint thePosition;
    long theResult = EEE3DStorm::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mDropCount);
    if (theResult == 0)
        theResult = outFile.Write(this->mRumbleDelay);
    if (theResult == 0) {
        this->mTransformation.GetPosition(thePosition);
        theResult = thePosition.Save(outFile);
    }
    if (theResult == 0) {
        float theRadius = this->GetBoundingRadius();
        theResult = outFile.Write(theRadius);
    }
    if (theResult == 0) {
        unsigned long theCount = this->mDrops.size();
        theResult = outFile.Write(theCount);
    }
    return theResult;
}

// FUNCTION: EXE 0x4f7e3f
// Updates the sounds and drops and ages the storm; returns 7 once its duration has passed unless it is sustained
long EEE3DRain::Step()
{
    long theResult = 0;
    this->StartSounds();
    this->UpdateDrops();
    this->mElapsedTime += gPhysics->GetTickTime();
    if (this->mElapsedTime >= this->mDuration && !this->mIsSustained)
        theResult = 7;
    this->mIsRestored = false;
    return theResult;
}

// FUNCTION: EXE 0x4f7e9f
// Adds drops towards a count that ramps up over 6.5 s, then keeps growing (at most 65 a tick, none above 1400 drops),
// lets them fall and removes those that reach the ground, sometimes rippling on water
long EEE3DRain::UpdateDrops()
{
    float theElapsed = gPhysics->GetTickTime() * 0.001f;
    long theResult = 0;
    unsigned long theExtra = 0;
    long theTarget = UMFloatToLong(this->mTiles.size() * min(this->mElapsedTime, kRainFadeTime) * kRainDensity * this->mDensity * 0.001f);
    if (this->mElapsedTime > kRainFadeTime) {
        theExtra = UMFloatToLong(max(this->mTiles.size() * (this->mDensity * ((this->mElapsedTime - kRainFadeTime) * 0.001f)), 0.0f));
    }
    unsigned long theMissing;
    if (this->mDuration > this->mElapsedTime || this->mIsSustained) {
        theMissing = theTarget - this->mDropCount + theExtra;
        if ((long)theMissing < 0)
            theMissing = -(long)theMissing;
    } else
        theMissing = 0;
    unsigned long theCount = UMFloatToLong(theMissing * this->mIntensity);
    if (theCount >= 0x41)
        theCount = 0x41;
    if (this->mDrops.size() > 0x578)
        theCount = 0;
    for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
        theResult = this->AddDrop();
        if (theResult == 0)
            this->mDropCount++;
    }
    for (std::vector<EE3DRainDrop>::iterator theDrop = this->mDrops.begin(); theDrop != this->mDrops.end();) {
        double theZ = theDrop->mPosition.GetZ() + theElapsed * this->mFallSpeed;
        theDrop->mPosition.SetZ(theZ);
        if (theZ < theDrop->mGroundZ) {
            if (GetEffectRandomLong(0, 15) == 0 && theDrop->mWater) {
                GE3DPoint theSplash(theDrop->mPosition.GetX(), theDrop->mPosition.GetY(), 0.0f);
                gWorld.GetEffectManager()->CreatePlacedEffect(theSplash, EEGraphicEffect::Get(kGraphicEffectLandRipple), NULL);
            }
            theDrop = this->mDrops.erase(theDrop);
        } else
            theDrop++;
    }
    return theResult;
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
// runs out (next in 5 to 14 s), and updates both sound levels
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
    this->mRumbleDelay -= gPhysics->GetTickTime();
    if (this->mRumbleDelay <= 0.0f) {
        EESoundObject* theSound = EESoundObject::Get(GetEffectRandomLong(0, 10) > 4 ? this->mRumbleID1 : this->mRumbleID2);
        if (theSound) {
            this->SetRumble(gSoundManager->PlayAndReturnReferencedNode(theSound->GetSampleID(), theSound->GetVolume(), 0.0f,
                                                                 1.0f, theSound->GetLooping(), "3D Rain Rumble"));
            if (this->mRumble)
                this->mRumble->Dereference();
        }
        this->mRumbleDelay = (float)(UMFloor(GetEffectRandomFloat(0.0f, 1.0f) * kRainRumbleDelay) + 5000);
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

// ICF-folded into EEE3DSnow's 0xfa291 (the same body)
long EEE3DRain::SetUp()
{
    GETransformation theTransformation;
    float theRadius = 0.0f;
    long theSumX = 0;
    long theSumY = 0;
    this->Virtual64();
    for (std::vector<EETileCoordinate>::iterator theTile = this->mTiles.begin(); theTile != this->mTiles.end(); theTile++) {
        theSumX += theTile->mX;
        theSumY += theTile->mY;
    }
    float theCenterX = (float)theSumX / this->mTiles.size();
    float theCenterY = (float)theSumY / this->mTiles.size();
    theTransformation.SetPosition(theCenterX, theCenterY,
                                  gWorld.mMap->mTerrainMesh.GetZ(UMFloor(theCenterX), UMFloor(theCenterY)));
    this->SetTransformation(theTransformation);
    for (std::vector<EETileCoordinate>::iterator theOther = this->mTiles.begin(); theOther != this->mTiles.end(); theOther++) {
        long theCoordinate = theOther->mX;
        float theDistance = (float)fabs(theCoordinate - theCenterX);
        if (theDistance > theRadius)
            theRadius = theDistance;
        theCoordinate = theOther->mY;
        theDistance = fabs(theCoordinate - theCenterY);
        if (theDistance > theRadius)
            theRadius = theDistance;
    }
    this->mBoundingRadius = theRadius;
    return 0;
}

// ICF-folded into 0xfca98
long EEE3DRain::Virtual70()
{
    return 0;
}

// FUNCTION: EXE 0x4f8385
// FUNCTION: EXE 0x4f83b1
// FUNCTION: EXE 0x4f83ec
// FUNCTION: EXE 0x4f8418
// FUNCTION: EXE 0x4f847c
// FUNCTION: EXE 0x4f84d6
// FUNCTION: EXE 0x4f8520
// FUNCTION: EXE 0x4f857a
// FUNCTION: EXE 0x4f8831
// FUNCTION: EXE 0x4f8ae8
// FUNCTION: EXE 0x4f8b15
// FUNCTION: EXE 0x4f8b42
// FUNCTION: EXE 0x4f8b6b
// FUNCTION: EXE 0x4f8b8f
// FUNCTION: EXE 0x4f8bc1
// FUNCTION: EXE 0x4f8bde
// FUNCTION: EXE 0x4f8c0a
// FUNCTION: EXE 0x4f8c3c
// FUNCTION: EXE 0x4f8c59
// FUNCTION: EXE 0x4f8c85
// FUNCTION: EXE 0x4f8c9b
// FUNCTION: EXE 0x4f8cc6
// The drop and draw vectors' std::vector members (template COMDATs from <vector>)
