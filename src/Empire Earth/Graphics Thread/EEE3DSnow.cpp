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

// the unit's named float constants (.rdata 0x440da4..0x440dc0, in this order; reloaded at each use) (provisional names)
static const float kSnowFadeTime = 500.0f;          // 0x440da4 (ms to full strength)
static const float kSnowDensity = 0.2f;             // 0x440da8
static const float kSnowFlakeWidth = 0.054f;        // 0x440dac
static const float kSnowFlakeHeight = 0.047f;       // 0x440db0
static const float kSnowFadeDistance = 1.0f;        // 0x440db4 (flakes closer to the camera fade)
static const float kSnowTurnChance = 0.05f;         // 0x440db8
static const float kSnowDrift = 0.005f;             // 0x440dbc
static const float kSnowSpin = 0.043633234f;        // 0x440dc0 (2.5 degrees per update)

// FUNCTION: EXE 0x4f9b6d
// Creates a snow storm over the given tiles for a graphic effect, 5 s long, with no flakes yet
EEE3DSnow::EEE3DSnow(const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inGraphicEffect)
    : EEE3DStorm(0x3d, inTiles, inGraphicEffect)
{
    this->mElapsedTime = 0.0f;
    this->mL74 = 0;
    this->mModel = NULL;
    this->mTiles = inTiles;
    this->mFallSpeed = 0.0f;
    this->mFlakeCount = 0;
    this->mIsRestored = false;
    this->mSpawnHeight = 0.0f;
    this->mDuration = 5000;
}

// SYNTHETIC: EXE 0x4f9bf5
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4f9c11
// Creates an empty snow storm (for loading)
EEE3DSnow::EEE3DSnow()
    : EEE3DStorm(0x3d)
{
    this->mElapsedTime = 0.0f;
    this->mL74 = 0;
    this->mModel = NULL;
    this->mFallSpeed = 0.0f;
    this->mFlakeCount = 0;
    this->mIsRestored = false;
    this->mSpawnHeight = 0.0f;
    this->mDuration = 5000;
}

// FUNCTION: EXE 0x4f9c87
// Destroys the snow storm (members and base only)
EEE3DSnow::~EEE3DSnow()
{
}

// (a tile accessor one level down: the arm order and the 0 register follow the bool)
static inline bool IsWaterTile(EEMapTile* inTile)
{
    return inTile->mTerrainType == kTerrainDeepWater || inTile->mTerrainType == kTerrainShallowWater;
}

// FUNCTION: EXE 0x4f9caf
// Adds a snowflake at a random point of a random storm tile at spawn height, falling to the ground (0 over water)
long EEE3DSnow::AddFlake()
{
    GE3DPoint thePosition;
    EETileCoordinate& theTile = this->mTiles[GetEffectRandomLong(0, this->mTiles.size() - 1)];
    float theRandom = GetEffectRandomFloat(0.0f, 1.0f);
    float theX = theTile.GetX() + theRandom;
    theRandom = GetEffectRandomFloat(0.0f, 1.0f);
    long theTileY = theTile.GetY();
    thePosition.mX = theX;
    thePosition.mY = theTileY + theRandom;
    thePosition.SetZ(this->mSpawnHeight);
    EEMapTile* theMapTile = gWorld.GetMap()->GetTile(theTile.GetX(), theTile.GetY());
    float theGround = IsWaterTile(theMapTile) ? 0.0f : gWorld.GetMap()->mTerrainMesh.GetZ(theTile.GetX(), theTile.GetY());
    float theAngle = theRandom * 6.2831855f;
    this->mFlakes.push_back(EEE3DSnowFlake(thePosition, theGround, theAngle));
    return 0;
}

// FUNCTION: EXE 0x4f9dcf
// Creates a snowflake at a position with its ground height, spin angle and a random sideways drift
EEE3DSnowFlake::EEE3DSnowFlake(const GE3DPoint& inPosition, float inGroundZ, float inAngle)
    : mPosition(inPosition), mGroundZ(inGroundZ), mAngle(inAngle)
{
    this->mDrift.SetX(GetEffectRandomFloat(-1.0f, 1.0f));
    this->mDrift.SetY(GetEffectRandomFloat(-1.0f, 1.0f));
    this->mDrift.SetZ(0.0f);
}

// FUNCTION: EXE 0x4f9e42
// Copies the flakes into the draw list and queues the snow in render queue 4 under its texture
long EEE3DSnow::Queue(EEEffectQueue* inQueues)
{
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        this->mDraws.clear();
        for (std::vector<EEE3DSnowFlake>::iterator theFlake = this->mFlakes.begin(); theFlake != this->mFlakes.end(); theFlake++)
            this->mDraws.push_back(EE3DSnowDraw(theFlake->mPosition, theFlake->mAngle));
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[4][theTexture].push_back(this);
    }
    return theResult;
}

// FUNCTION: EXE 0x4f9ef5
// Reads fall speed, spawn height, density and duration from the effect; on first use builds the shared snowflake quad
// model with textures\snowflake
void EEE3DSnow::Virtual64()
{
    GETextureCoordinate theCoordinate0(0.0f, 0.0f);
    EEGraphicEffect* theEffect = this->mGraphicEffect;
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
    this->mFallSpeed = theEffect->GetParam1();
    this->mSpawnHeight = theEffect->GetParam2();
    this->mDensity = theEffect->GetParam3();
    this->mDuration = theEffect->mDuration;
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
    {
        GE3DPoint thePoint(kSnowFlakeWidth * -0.5f, 0.0f, kSnowFlakeHeight);
        this->mModel->GetVertexArray(0)->SetPoint(thePoint, 0);
    }
    {
        GE3DPoint thePoint(kSnowFlakeWidth * 0.5f, 0.0f, kSnowFlakeHeight);
        this->mModel->GetVertexArray(0)->SetPoint(thePoint, 1);
    }
    {
        GE3DPoint thePoint(kSnowFlakeWidth * 0.5f, 0.0f, 0.0f);
        this->mModel->GetVertexArray(0)->SetPoint(thePoint, 2);
    }
    {
        GE3DPoint thePoint(kSnowFlakeWidth * -0.5f, 0.0f, 0.0f);
        this->mModel->GetVertexArray(0)->SetPoint(thePoint, 3);
    }
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

// FUNCTION: EXE 0x4fa291
// Places the storm at the mean of its tiles on the terrain, with a radius reaching its farthest tile in x or y
long EEE3DSnow::SetUp()
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

// FUNCTION: EXE 0x4fa404
// Draws every queued flake as the camera-facing spinning quad, alpha blended and fading within 1 unit of the camera;
// stops at the first draw error
long EEE3DSnow::DrawPass6()
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
    for (std::vector<EE3DSnowDraw>::iterator theDraw = this->mDraws.begin(); theDraw != this->mDraws.end() && theResult == 0;
         theDraw++) {
        theTransformation.SetX(theDraw->mPosition.GetX());
        theTransformation.SetY(theDraw->mPosition.GetY());
        theTransformation.SetZ(theDraw->mPosition.GetZ());
        theTransformation.SetOrientationYPR(gWorld.GetCameraYaw(), 0.0f, theDraw->mAngle);
        double theDistance = sqrt(gWorld.mTransformation.DistanceSquaredTo(theTransformation));
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

// FUNCTION: EXE 0x4fa579
// Reads the storm, flake count, a position (discarded), the bounding radius and the number of flakes to recreate;
// returns the first read error
long EEE3DSnow::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    GE3DPoint thePosition;
    float theRadius = 0.0f;
    long theResult = EEE3DStorm::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0)
        theResult = inFile.Read(this->mFlakeCount);
    if (theResult == 0)
        theResult = thePosition.Load(inFile);
    if (theResult == 0)
        theResult = inFile.Read(theRadius);
    if (theResult == 0) {
        this->SetBoundingRadius(theRadius);
        theResult = inFile.Read(this->mLoadedFlakes);
    }
    return theResult;
}

// FUNCTION: EXE 0x4fa5f3
// After loading, reloads the settings, recreates the saved number of flakes at random heights, marks the storm restored
long EEE3DSnow::RepairPointers()
{
    long theResult = EEE3DStorm::RepairPointers();
    if (theResult == 0) {
        this->Virtual64();
        for (unsigned long i = 0; i < this->mLoadedFlakes && theResult == 0; i++)
            theResult = this->AddFlake();
        for (std::vector<EEE3DSnowFlake>::iterator theFlake = this->mFlakes.begin(); theFlake != this->mFlakes.end(); theFlake++)
            theFlake->mPosition.SetZ(EEEffect::GetRandom() * this->mSpawnHeight);
        this->mLoadedFlakes = 0;
        this->mIsRestored = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4fa663
// Writes the storm, flake count, position, radius and
// live flake count; returns the first write error
long EEE3DSnow::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    GE3DPoint thePosition;
    long theResult = EEE3DStorm::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mFlakeCount);
    if (theResult == 0) {
        this->mTransformation.GetPosition(thePosition);
        theResult = thePosition.Save(outFile);
    }
    if (theResult == 0) {
        float theRadius = this->GetBoundingRadius();
        theResult = outFile.Write(theRadius);
    }
    if (theResult == 0) {
        unsigned long theCount = this->mFlakes.size();
        theResult = outFile.Write(theCount);
    }
    return theResult;
}

// FUNCTION: EXE 0x4fa70d
// Updates the flakes and ages the storm; returns 7 once its duration has passed and all flakes landed, unless sustained
long EEE3DSnow::Step()
{
    long theResult = 0;
    this->UpdateFlakes();
    this->mElapsedTime += gPhysics->GetTickTime();
    if (this->mElapsedTime >= this->mDuration && this->mFlakes.size() == 0 && !this->mIsSustained)
        theResult = 7;
    this->mIsRestored = false;
    return theResult;
}

// FUNCTION: EXE 0x4fa782
// Adds flakes towards a count that ramps up over 0.5 s, then keeps growing (at most 65 a tick, none above 1400 flakes);
// lets them fall (tick divided by the game speed), spin and drift, and removes those below ground
long EEE3DSnow::UpdateFlakes()
{
    float theElapsed = gPhysics->GetTickTime() * 0.001f / gWorld.GetGameSpeed();
    long theResult = 0;
    unsigned long theExtra = 0;
    unsigned long theMissing = 0;
    float theTime = min(this->mElapsedTime, kSnowFadeTime);
    long theTarget = UMFloatToLong(theTime * 0.001f * this->mTiles.size() * this->mDensity * kSnowDensity);
    if (this->mElapsedTime > kSnowFadeTime) {
        theExtra = UMFloatToLong(max(this->mTiles.size() * (this->mDensity * ((this->mElapsedTime - kSnowFadeTime) * 0.001f)), 0.0f));
    }
    if (this->mDuration > this->mElapsedTime || this->mIsSustained) {
        theMissing = theTarget - this->mFlakeCount + theExtra;
        if ((long)theMissing < 0)
            theMissing = -(long)theMissing;
    }
    unsigned long theCount = UMFloatToLong(theMissing * this->mIntensity);
    if (theCount >= 0x41)
        theCount = 0x41;
    if (this->mFlakes.size() > 0x578)
        theCount = 0;
    for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
        theResult = this->AddFlake();
        if (theResult == 0)
            this->mFlakeCount++;
    }
    for (std::vector<EEE3DSnowFlake>::iterator theFlake = this->mFlakes.begin(); theFlake != this->mFlakes.end();) {
        theFlake->mPosition.SetZ(theElapsed * this->mFallSpeed + theFlake->mPosition.GetZ());
        theFlake->mAngle += kSnowSpin;
        theFlake->Update();
        if (theFlake->mGroundZ > theFlake->mPosition.mZ)
            theFlake = this->mFlakes.erase(theFlake);
        else
            theFlake++;
    }
    return theResult;
}

// FUNCTION: EXE 0x4fa99a
// Drifts the flake sideways, picking a new random drift direction 5% of the time
long EEE3DSnowFlake::Update()
{
    float theX = this->mPosition.mX;
    float theY = this->mPosition.mY;
    if (EEEffect::GetRandom() < kSnowTurnChance) {
        this->mDrift.SetX(GetEffectRandomFloat(-1.0f, 1.0f));
        this->mDrift.SetY(GetEffectRandomFloat(-1.0f, 1.0f));
        this->mDrift.SetZ(0.0f);
    }
    this->mPosition.mX = this->mDrift.GetX() * kSnowDrift + theX;
    this->mPosition.mY = this->mDrift.GetY() * kSnowDrift + theY;
    return 0;
}

// ICF-folded into 0xe123d
void EEE3DSnow::PrepareQueue()
{
}

// FUNCTION: EXE 0x4faa1a
// FUNCTION: EXE 0x4faa46
// FUNCTION: EXE 0x4faa72
// FUNCTION: EXE 0x4faad6
// FUNCTION: EXE 0x4fab26
// FUNCTION: EXE 0x4fab70
// FUNCTION: EXE 0x4fabca
// FUNCTION: EXE 0x4fae9d
// FUNCTION: EXE 0x4faed7
// FUNCTION: EXE 0x4faf03
// TEMPLATE: EXE 0x4faf2e
// std::vector members for the flake and draw lists (template and inline COMDATs emitted by this unit)

// FUNCTION: EXE 0x4faf45
// FUNCTION: EXE 0x4faf6e
// FUNCTION: EXE 0x4faf9c
// FUNCTION: EXE 0x4fafc8
// FUNCTION: EXE 0x4fafe5
// TEMPLATE: EXE 0x4fb011
// std::vector members for the flake and draw lists (template and inline COMDATs emitted by this unit)

// FUNCTION: EXE 0x4fb026
// FUNCTION: EXE 0x4fb038
// More of the two vectors' std::vector members (template COMDATs from <vector>)

// SYNTHETIC: EXE 0x4fb071
// (the scalar deleting destructor, emitted with the vtable)
