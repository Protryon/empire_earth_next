// The 3D effects whole in Rust (docs/RUST_PLAN.md R4.4; ee_gfx effect_rising.rs, effect_follow.rs, effect_shot.rs,
// effect_beam.rs, effect_aged.rs, effect_additive.rs,
// effect_orbit.rs, effect_hurricane.rs, effect_nuclear.rs, effect_multi.rs, effect_wall.rs, effect_vent.rs, effect_icon.rs,
// effect_tracer.rs, effect_volcano.rs): EEE3DRust's virtuals call Rust by the record's kind and answer its questions about the world here.
// Replaces EEE3DTeleport.cpp, EEE3DTimeWarp.cpp, EEE3DBlastWave.cpp, EEE3DShield.cpp, EEE3DPulseCannonBall.cpp,
// EEE3DTracers.cpp, EEE3DShells.cpp, EEE3DCitizenBeam.cpp, EEE3DWaterSplash.cpp, EEE3DScreenFlash.cpp,
// EEE3DAdditivePass.cpp, EEE3DLinearProjectile.cpp, EEE3DDisease.cpp, EEE3DVirus.cpp, EEE3DHurricane.cpp,
// EEE3DNuclearExplosion.cpp, EEE3DEnergyWave.cpp, EEE3DMultiEffect.cpp, EEE3DEnergyWall.cpp, EEE3DDamageVent.cpp,
// EEE3DIcon.cpp, EEE3DGoal.cpp, EEE3DTracerBeam.cpp, EEE3DVolcano.cpp, EEE3DFarm.cpp (effect_farm.rs) and
// EEE3DEarthquake.cpp (effect_quake.rs) and EEE3DSmokeGenerator.cpp (effect_smoke.rs), EEE3DStorm.cpp, EEE3DRain.cpp
// and EEE3DSnow.cpp (effect_weather.rs), EEE3DParticle.cpp, EEE3DParticleEmitter.cpp and EEE3DMarchingLine.cpp
// (effect_particles.rs).
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
#include "ee/EEEffectManager.h"
#include "ee/EEE3DRust.h"
#include "ee/EEE2DRust.h"
#include "ee/EECombatFormulas.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEGamePreferences.h"
#include "lle/GERectangle.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEModelManager.h"
#include "lle/Graphics Engine.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEEffectMesh.h"
#include "ee/EEDoppelganger.h"
#include "ee/EEAreaUtilities.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/SESMInstanceNode.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnitAction.h"
#include "ee/RTSRegionTable.h"

class EEComplexUnit;
// 0x23cee2 (EERenderLists.h): the unit's draw opacity (the farm's)
bool Fn23CEE2(EEComplexUnit* inUnit, bool inTranslucent, float& ioOpacity);

namespace {
// the map a weather storm reads and calls into (ee_gfx effect_storm.rs; the storms' and EEEMStorm's moves)
struct EEStormHostOps {
    static bool OnMap(void*, int32_t inX, int32_t inY) { return gWorld.GetMap()->GetTile(inX, inY) != NULL; }
    static bool IsWater(void*, int32_t inX, int32_t inY)
    {
        EEMapTile* theTile = gWorld.GetMap()->GetTile(inX, inY);
        return theTile && (theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater);
    }
    static float GroundZ(void*, int32_t inX, int32_t inY) { return gWorld.GetMap()->mTerrainMesh.GetZ(inX, inY); }
    static void StepToward(void*, int32_t inFromX, int32_t inFromY, int32_t inToX, int32_t inToY, int32_t* outTile)
    {
        EETileCoordinate theFrom;
        EETileCoordinate theTile;
        theFrom.mX = inFromX;
        theFrom.mY = inFromY;
        EEGetNextTile(theFrom, theTile, EEGetTileDirection(inFromX, inFromY, inToX, inToY), 1);
        outTile[0] = (int32_t)theTile.mX;
        outTile[1] = (int32_t)theTile.mY;
    }
    static void Ripple(void*, float inX, float inY)
    {
        GE3DPoint theSplash(inX, inY, 0.0f);
        gWorld.GetEffectManager()->CreatePlacedEffect(theSplash, EEGraphicEffect::Get(kGraphicEffectLandRipple), NULL);
    }
};
const EeGfxStormHost kStormHost = {NULL, EEStormHostOps::OnMap, EEStormHostOps::IsWater, EEStormHostOps::GroundZ,
                                   EEStormHostOps::StepToward, EEStormHostOps::Ripple};

// a sound replaced by a referenced node (EEE3DRain::SetSound 0xf8337, SetRumble 0xf835e, EEE3DParticleEmitter::SetSound
// 0xf69bc), the node's own reference then dropped
void EESetEffectSound(SESMInstanceNode*& ioSound, SESMInstanceNode* inSound)
{
    if (inSound != ioSound) {
        if (ioSound)
            ioSound->Dereference();
        ioSound = inSound;
        if (inSound)
            inSound->Reference();
    }
    if (ioSound)
        ioSound->Dereference();
}

// a rain sound's volume, pan and pitch from the camera's distance to the storm (range 5 times its radius), scaled by
// inVolume and by 0.4 in the first 6.5 s (EEE3DRain::SetSoundLevel, 0xf80e8)
void EESetRainSoundLevel(EEE3DRust* inRain, SESMInstanceNode* inSound, float inVolume)
{
    GE3DPoint thePosition;
    if (!inSound)
        return;
    float theVolume;
    float thePan;
    float thePitch;
    inRain->mTransformation.GetPosition(thePosition);
    EEComputeSoundParameters(1.0f, thePosition, theVolume, thePan, thePitch, inRain->mBoundingRadius * 5.0f);
    theVolume *= inVolume;
    if (inRain->mRecord->storm.elapsed < EE_GFX_RAIN_FADE_TIME)
        theVolume = EE_GFX_RAIN_FADE_SHARE * theVolume;
    gSoundManager->SetSoundParameters(inSound, theVolume, thePan, thePitch);
}

// the map's questions for ee_gfx volcano.rs (the volcano's build)
float EEVolcanoZ(void* inMap, uint32_t inX, uint32_t inY)
{
    return static_cast<EEMap*>(inMap)->mTerrainMesh.GetZ(inX, inY);
}
EeGfxColor EEVolcanoColor(void* inMap, uint32_t inX, uint32_t inY)
{
    const GEColor& theColor = EETerrainTable::Get(static_cast<EEMap*>(inMap)->mTerrainMesh.GetTextureSetID(inX, inY))->mColor;
    EeGfxColor theResult = {theColor.mRed, theColor.mGreen, theColor.mBlue};
    return theResult;
}
bool EEVolcanoValidTile(void* inMap, uint32_t inX, uint32_t inY)
{
    return static_cast<EEMap*>(inMap)->mTerrainMesh.GetIsValidTile(inX, inY);
}
EeGfxColor EEVolcanoLight(void* inMap, int32_t inX, int32_t inY, int32_t inCorner)
{
    const GEColor& theColor = static_cast<EEMap*>(inMap)->GetVertexColor(inX, inY, inCorner);
    EeGfxColor theResult = {theColor.mRed, theColor.mGreen, theColor.mBlue};
    return theResult;
}

// the terrain's questions for ee_gfx farm.rs (the farm's meshes)
float EEFarmZ(void* inMesh, uint32_t inX, uint32_t inY)
{
    return static_cast<GETerrainMesh*>(inMesh)->GetZ(inX, inY);
}
EeGeVector EEFarmNormal(void* inMesh, uint32_t inX, uint32_t inY)
{
    const GEVector* theNormal = static_cast<GETerrainMesh*>(inMesh)->GetVertexNormal(inX, inY);
    EeGeVector theResult = {theNormal->GetX(), theNormal->GetY(), theNormal->GetZ()};
    return theResult;
}
float EEFarmMeshZ(void* inMesh, float inX, float inY)
{
    return static_cast<GETerrainMesh*>(inMesh)->GetMeshZ(inX, inY);
}

// the world's answers for one call
struct Ask {
    EEE3DRust* mEffect;
    EEEffectQueue* mQueues;
    EETagPointList* mSpawn;   // the hurricane's spawn list, read once by SpawnCount for SpawnAt
    EeGfxEffect3DOp mOp;
    void* mArg;               // a constructor's unit (Create), point (CreateAt) or line (CreateLine)
    EETagPointList* mVentFrom;   // the damage vent's tag point lists, read once a step by VentLists
    EETagPointList* mVentTo;
    EETagPointList* mSmokeTags;  // the damage smoke's tag points, read once a step by SmokeUnit for SmokeTagPoint
    unsigned long mSmokeFrame;
    GETransformation mSmokeLocation;
    static EEE3DRust* E(void* inAsk) { return static_cast<Ask*>(inAsk)->mEffect; }
    static EeGe3DPoint UnitPosition(void* inAsk)
    {
        GE3DPoint thePoint;
        E(inAsk)->mUnit->GetPosition(thePoint);
        return EeGe3DPoint{thePoint.mX, thePoint.mY, thePoint.mZ};
    }
    static float ModelHeight(void* inAsk)
    {
        return E(inAsk)->mGraphicEffect->mGraphicObject->mModels[0]->GetBoundingBox(0).GetHeight();
    }
    static void MoveTo(void* inAsk)
    {
        GETransformation theTransformation(E(inAsk)->mTransformation);
        E(inAsk)->SetTransformation(theTransformation);
    }
    static void Line(void* inAsk, const EeGe3DPoint* inStart, const EeGeVector* inDirection, uint32_t inLinked)
    {
        GE3DLine theLine;
        theLine.SetStart(GE3DPoint(inStart->x, inStart->y, inStart->z));
        theLine.SetDirection(GEVector(inDirection->x, inDirection->y, inDirection->z));
        EEGraphicEffect* theEffect = E(inAsk)->mGraphicEffect;
        gEffectManager->CreateLineEffect(theLine, inLinked == 1 ? theEffect->mLinkedEffect1 : theEffect->mLinkedEffect2, NULL);
    }
    static int32_t BaseQueue(void* inAsk) { return E(inAsk)->EEEffect3D::Queue(static_cast<Ask*>(inAsk)->mQueues); }
    static void Push(void* inAsk, uint32_t inIndex)
    {
        EEGraphicObject* theObject = E(inAsk)->mGraphicEffect->mGraphicObject;
        GETexture* theTexture = theObject ? theObject->mTextures[gScreenIndex] : NULL;
        static_cast<Ask*>(inAsk)->mQueues[inIndex][theTexture].push_back(E(inAsk));
    }
    static int32_t Media(void* inAsk)
    {
        E(inAsk)->mGraphicObject = E(inAsk)->GetGraphicObject();
        return E(inAsk)->mGraphicObject->LoadMedia();
    }
    static void* Model(void* inAsk) { return E(inAsk)->mGraphicObject->mModel; }
    static void* Texture(void* inAsk) { return E(inAsk)->mGraphicObject->mTextures[0]; }
    // to the unit's first tag point of type 0x12 (keeping the unit's orientation), else to the unit (EEE3DBlastWave's
    // and EEE3DShield's FollowUnit, 0xe904a)
    static void Follow(void* inAsk)
    {
        EEE3DRust* theEffect = E(inAsk);
        EEComplexUnit* theUnit = theEffect->mUnit->mComplexUnit;
        GE3DPoint thePoint;
        GETransformation theLocation(theUnit->mPhysicalModel.GetLocation());
        float theScale = theUnit->mPhysicalModel.GetScale();
        unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
        EETagPointList* theList = theUnit->GetModel()->GetTagPoints(theUnit->GetAnimation(), 0x12);
        if (theList != NULL && !theList->mPoints.empty()) {
            EETagPointList::Scale(theLocation, theScale);
            theList->mPoints.front()->GetPoint(theFrame, theLocation, thePoint);
            GETransformation theTransformation(false);
            theTransformation = theLocation;
            theTransformation.SetPosition(thePoint.GetX(), thePoint.GetY(), thePoint.GetZ());
            theEffect->SetTransformation(theTransformation);
        } else {
            theEffect->SetTransformation(theLocation);
        }
    }
    static void* ModelAt(void* inAsk, uint32_t inIndex) { return E(inAsk)->mGraphicObject->mModels[inIndex]; }
    // a tracer's end: its impact effect at its position, then the target's hit (EEE3DTracers::Step, 0xfdad4)
    static void ShotHit(void* inAsk)
    {
        EEE3DRust* theEffect = E(inAsk);
        EEGraphicEffect* theImpact = theEffect->mGraphicEffect->mLinkedEffect1;
        if (theImpact) {
            GE3DPoint theLocation(theEffect->mTransformation);
            gEffectManager->CreatePlacedEffect(theLocation, theImpact, NULL);
        }
        if (theEffect->mUnit)
            theEffect->mUnit->mUnitClass->UnitDamaged(theEffect->mUnit->mComplexUnit, NULL, 0);
    }
    // the citizen beam's start: the unit's first tag point (EEE3DCitizenBeam::Aim, 0xe9606)
    static EeGe3DPoint BeamStart(void* inAsk)
    {
        GE3DPoint theStart;
        EEUnit* theOwner = E(inAsk)->mUnit;
        EEModel* theModel = theOwner->GetModel();
        EEComplexUnit* theUnit = theOwner->mComplexUnit;
        GETransformation theUnitTransformation(theUnit->mPhysicalModel.GetLocation());
        float theScale = theUnit->mPhysicalModel.GetScale();
        unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
        EETagPointList* theList = theModel->mTagPoints[theUnit->mAnimation];
        EETagPointList::Scale(theUnitTransformation, theScale);
        theList->GetTagPoint(0)->GetPoint(theFrame, theUnitTransformation, theStart);
        return EeGe3DPoint{theStart.mX, theStart.mY, theStart.mZ};
    }
    // the beam's unit as a complex unit (EEE3DCitizenBeam::Step, 0xe9aec)
    static EeGfxBeamUnit BeamUnit(void* inAsk)
    {
        EeGfxBeamUnit theFacts = {};
        EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(E(inAsk)->mUnit);
        if (theUnit) {
            theFacts.complex = true;
            theFacts.animation = (int32_t)theUnit->mAnimation;
            theFacts.frame = (int32_t)(long)theUnit->mPhysicalModel.GetFrame();
        }
        return theFacts;
    }
    static bool BeamHidden(void* inAsk)
    {
        EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(E(inAsk)->mUnit);
        bool theCloaked = theUnit->mIsCloakable ? theUnit->mIsCloaked : false;
        EEPlayer* thePlayer;
        EEPlayer* theCurrent;
        return theCloaked && (thePlayer = theUnit->mPlayer) != (theCurrent = gWorld.GetCurrentPlayer())
            && theCurrent->mEnemies[thePlayer->mPlayerID] != 0;
    }
    static void PlaceLinked(void* inAsk, const EeGe3DPoint* inPoint)
    {
        EEGraphicEffect* theLinked = E(inAsk)->mGraphicEffect->mLinkedEffect1;
        if (theLinked) {
            GE3DPoint thePoint(inPoint->x, inPoint->y, inPoint->z);
            gEffectManager->CreatePlacedEffect(thePoint, theLinked, NULL);
        }
    }
    // the screen flash's place: its unit's complex unit's position (EEE3DScreenFlash::SetUp, ICF-folded into 0xee3a4)
    static EeGe3DPoint ComplexPosition(void* inAsk)
    {
        GE3DPoint thePoint;
        E(inAsk)->mUnit->mComplexUnit->GetPosition(thePoint);
        return EeGe3DPoint{thePoint.mX, thePoint.mY, thePoint.mZ};
    }
    // the additive pass's unit model (EEE3DAdditivePass::SetUp, 0xe8b78, and RepairPointers, 0xe8dcc)
    static int32_t UnitModelMedia(void* inAsk)
    {
        E(inAsk)->mUnitModel = E(inAsk)->mUnit->GetModel();
        return E(inAsk)->mUnitModel->LoadMedia();
    }
    static float UnitScale(void* inAsk) { return E(inAsk)->mUnit->mStats->GetScale(); }
    static EeGeTransformation UnitLocation(void* inAsk)
    {
        const GETransformation& theLocation = E(inAsk)->mUnit->mComplexUnit->mPhysicalModel.GetLocation();
        return *reinterpret_cast<const EeGeTransformation*>(&theLocation);
    }
    static float UnitModelRadius(void* inAsk) { return E(inAsk)->mUnitModel->mModels[0]->GetBoundingRadius(0); }
    static EeGfxUnitFrame UnitFrame(void* inAsk)
    {
        EEComplexUnit* theUnit = E(inAsk)->mUnit->mComplexUnit;
        return EeGfxUnitFrame{(uint32_t)theUnit->mPhysicalModel.GetFrame(), (uint32_t)theUnit->mAnimation};
    }
    static void* UnitModelAt(void* inAsk, uint32_t inIndex) { return E(inAsk)->mUnitModel->mModels[inIndex]; }
    // the graphic object's orbitals (EEE3DLinearProjectile::SetUp, 0xf30ba, copies them)
    static uint32_t OrbitalCount(void* inAsk) { return (uint32_t)E(inAsk)->mGraphicObject->GetOrbitalCount(); }
    static EeGfxOrbital Orbital(void* inAsk, uint32_t inIndex)
    {
        EEOrbital* theOrbital = E(inAsk)->mGraphicObject->GetOrbital(inIndex);
        return EeGfxOrbital{theOrbital->mModel, theOrbital->mSpeed, theOrbital->mAngle};
    }
    // the disease's and virus's animation 13 (EEE3DDisease::Step, 0xec4d7)
    static EeGfxAnimation Animation(void* inAsk)
    {
        EEGraphicObject* theObject = E(inAsk)->mGraphicObject;
        return EeGfxAnimation{(uint32_t)theObject->mStartFrames[13], (uint32_t)theObject->mFrameTimes[13],
                              (uint32_t)theObject->mFrameCounts[13]};
    }
    // the frame's sound at the unit, kept as the effect's (EEE3DDisease::Step and SetSound, 0xec6bd)
    static void AnimationSound(void* inAsk, uint32_t inFrame, bool inChanged)
    {
        EEE3DRust* theEffect = E(inAsk);
        GE3DPoint theLocation;
        theEffect->mUnit->mComplexUnit->mPhysicalModel.GetLocation().GetPosition(theLocation);
        SESMInstanceNode* theSound =
            PlayAnimationSound(theLocation, theEffect->mGraphicObject, 0, theEffect->mSound, inFrame, inChanged);
        if (theSound != theEffect->mSound) {
            if (theEffect->mSound)
                theEffect->mSound->Dereference();
            theEffect->mSound = theSound;
            if (theSound)
                theSound->Reference();
        }
        if (theEffect->mSound)
            theEffect->mSound->Dereference();
    }
    // the hurricane's spawn points: its graphic's tag points of kind 6 (animation 0) (EEE3DHurricane::GetSpawnPoint,
    // 0xf188a); none without a list
    static uint32_t SpawnCount(void* inAsk)
    {
        EETagPointList* theList = E(inAsk)->mGraphicEffect->mGraphicObject->GetTagPoints(0, 6);
        static_cast<Ask*>(inAsk)->mSpawn = theList;
        return theList == NULL ? 0 : (uint32_t)theList->mPoints.size();
    }
    // tag point inIndex at inFrame around the unit, scaled by the graphic effect's size (after SpawnCount)
    static EeGe3DPoint SpawnAt(void* inAsk, uint32_t inIndex, uint32_t inFrame)
    {
        EEE3DRust* theEffect = E(inAsk);
        GETransformation theLocation(theEffect->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
        EETagPointList* theList = static_cast<Ask*>(inAsk)->mSpawn;
        GE3DPoint thePoint;
        EETagPointList::Scale(theLocation, theEffect->mGraphicEffect->mSize);
        theList->mPoints[inIndex]->GetPoint((unsigned long)inFrame, theLocation, thePoint);
        return EeGe3DPoint{thePoint.mX, thePoint.mY, thePoint.mZ};
    }
    // the linked effect 1 placed at inPoint; the placement's result
    static int32_t Spawn(void* inAsk, const EeGe3DPoint* inPoint)
    {
        GE3DPoint thePoint(inPoint->x, inPoint->y, inPoint->z);
        return gEffectManager->CreatePlacedEffect(thePoint, E(inAsk)->mGraphicEffect->mLinkedEffect1, NULL);
    }
    static void* TextureAt(void* inAsk, uint32_t inIndex) { return E(inAsk)->mGraphicObject->mTextures[inIndex]; }
    // the nuclear explosion's place off the map: its tile's (EEE3DNuclearExplosion::SetUp, 0xf4524)
    static EeGfxUnitTile UnitTile(void* inAsk)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        return EeGfxUnitTile{theUnit->mTileNode != NULL, (float)theUnit->GetClass()->GetTileSize(),
                             (float)(long)theUnit->mTileX, (float)(long)theUnit->mTileY};
    }
    static float TerrainZ(void* inAsk, float inX, float inY) { return gWorld.mMap->mTerrainMesh.GetMeshZ(inX, inY); }
    // the energy wave's unit is a cloaked enemy's (EEE3DEnergyWave::Queue, 0xee287; the port's R5a.3 test)
    static bool UnitHidden(void* inAsk)
    {
        EEComplexUnit* theUnit = E(inAsk)->mUnit->mComplexUnit;
        bool theCloaked = theUnit->mIsCloakable ? theUnit->mIsCloaked : false;
        if (!theCloaked)
            return false;
        EEPlayer* thePlayer = theUnit->mPlayer;
        EEPlayer* theCurrent = gWorld.GetCurrentPlayer();
        return thePlayer != theCurrent && theCurrent->mEnemies[thePlayer->mPlayerID] != 0;
    }
    static bool SpaceTile(void*, uint32_t inX, uint32_t inY)
    {
        return gWorld.mMap->mTerrainMesh.GetIsSpaceTile((unsigned long)inX, (unsigned long)inY);
    }
    // the multi-effect's linked effect inIndex through its constructor's entry point (EEE3DMultiEffect's three
    // constructors, 0xf4247, 0xf42c3, 0xf4339)
    static int32_t CreateLinked(void* inAsk, uint32_t inIndex)
    {
        Ask* theAsk = static_cast<Ask*>(inAsk);
        EEGraphicEffect* theEffect = theAsk->mEffect->mGraphicEffect;
        EEGraphicEffect* theLinked = inIndex == 1 ? theEffect->mLinkedEffect1
                                     : inIndex == 2 ? theEffect->mLinkedEffect2 : theEffect->mLinkedEffect3;
        if (theAsk->mOp == EeGfxEffect3DOp::CreateLine)
            return gEffectManager->CreateLineEffect(*static_cast<GE3DLine*>(theAsk->mArg), theLinked, NULL);
        if (theAsk->mOp == EeGfxEffect3DOp::CreateAt)
            return gEffectManager->CreatePlacedEffect(*static_cast<GE3DPoint*>(theAsk->mArg), theLinked, NULL);
        return gEffectManager->CreateUnitEffect(static_cast<EEUnit*>(theAsk->mArg), theLinked, NULL, 0, 0, 0);
    }
    // the energy wall's posts on a gate: its two end parts (EEE3DEnergyWall::Place, 0xed784)
    static bool WallGatePosts(void* inAsk, EeGe3DPoint* outFirst, EeGe3DPoint* outSecond)
    {
        EEComplexUnit* theUnit = E(inAsk)->mUnit->mComplexUnit;
        if (theUnit->GetClass()->mCategory != kUnitCategoryGate)
            return false;
        EEUCGate* theGate = dynamic_cast<EEUCGate*>(theUnit->GetClass());
        EEComplexUnit* theFirst = theGate->GetPart(theUnit, 0);
        EEComplexUnit* theSecond = theGate->GetPart(theUnit, 4);
        GE3DPoint theFrom;
        GE3DPoint theTo;
        theFirst->GetPosition(theFrom);
        theSecond->GetPosition(theTo);
        *outFirst = EeGe3DPoint{theFrom.mX, theFrom.mY, theFrom.mZ};
        *outSecond = EeGe3DPoint{theTo.mX, theTo.mY, theTo.mZ};
        return true;
    }
    // the height of the last living, finished wall piece or gate side part of the wall's owner on a tile (0xee00c)
    static bool WallPiece(void* inAsk, int32_t inX, int32_t inY, float* outZ)
    {
        unsigned long theOwner = E(inAsk)->mUnit->mComplexUnit->mPlayer->mPlayerID;
        bool theFound = false;
        std::vector<EEUnit*> theUnits;
        EEGetUnitsOnTile(inX, inY, theUnits);
        for (unsigned long j = 0; j < theUnits.size(); j++) {
            EEComplexUnit* theOther = dynamic_cast<EEComplexUnit*>(theUnits[j]);
            if (!theOther || theOther->mPlayer->mPlayerID != theOwner || !(theOther->mHitPoints > 0) ||
                theOther->IsFoundation())
                continue;
            long theType = theOther->GetClass()->mUnitType;
            if (theType == kUnitTypeWallSpace
                || (theType == kUnitTypeGateSpace && static_cast<EEUCGate*>(theOther->GetClass())->IsSidePart(theOther))) {
                GE3DPoint thePosition;
                theOther->GetPosition(thePosition);
                *outZ = thePosition.GetZ();
                theFound = true;
            }
        }
        return theFound;
    }
    // a gate's opening (EEE3DEnergyWall::Queue, 0xed9d0)
    static bool GateOpen(void* inAsk, float* outOpen)
    {
        EEComplexUnit* theUnit = E(inAsk)->mUnit->mComplexUnit;
        if (theUnit->GetClass()->mCategory != kUnitCategoryGate)
            return false;
        *outOpen = theUnit->GetOpenFraction();
        return true;
    }
    static EeGfxColor PlayerColor(void* inAsk)
    {
        const GEColor& theColor = E(inAsk)->mUnit->mComplexUnit->mPlayer->mColor;
        return EeGfxColor{theColor.mRed, theColor.mGreen, theColor.mBlue};
    }
    // the damage vent's vents (kind 5) and aims (0x38) on the unit's model at its animation (EEE3DDamageVent::Step,
    // 0xeae33)
    static void VentLists(void* inAsk)
    {
        Ask* theAsk = static_cast<Ask*>(inAsk);
        EEUnit* theUnit = theAsk->mEffect->mUnit;
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        theAsk->mVentFrom = theUnit->GetModel()->GetTagPoints(theComplex->GetAnimation(), 5);
        theAsk->mVentTo = theUnit->GetModel()->GetTagPoints(theComplex->GetAnimation(), 0x38);
    }
    static EeGfxVentUnit VentUnit(void* inAsk)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        return EeGfxVentUnit{(theComplex->mCommand ? theComplex->mCommand->mType : 0) == kActionDead,
                             (float)theUnit->mStats->mMaxHitPoints};
    }
    // the vents' count; none on a model without both tag lists (the original read the first list's count through a
    // null pointer there: a crash the effects sweep met on seed 4, a unit without vents; no vent, no jet now)
    static uint32_t VentCount(void* inAsk)
    {
        Ask* theAsk = static_cast<Ask*>(inAsk);
        if (theAsk->mVentFrom == NULL || theAsk->mVentTo == NULL)
            return 0;
        return (uint32_t)theAsk->mVentFrom->GetNumTagPoints();
    }
    // a jet's vent's and aim's offsets (EEE3DDamageVentPart::Aim, 0xeb4e9)
    static void VentOffsets(void* inAsk, int32_t inTag, EeGe3DPoint* outPoints)
    {
        Ask* theAsk = static_cast<Ask*>(inAsk);
        GE3DPoint theFrom;
        GE3DPoint theTo;
        theAsk->mVentFrom->mPoints[inTag]->GetOffset(0, theFrom);
        theAsk->mVentTo->mPoints[inTag]->GetOffset(0, theTo);
        outPoints[0] = EeGe3DPoint{theFrom.mX, theFrom.mY, theFrom.mZ};
        outPoints[1] = EeGe3DPoint{theTo.mX, theTo.mY, theTo.mZ};
    }
    // a jet's vent and aim at frame 0 around the unit's location scaled by its stats, and the vent's offset from the
    // model's centre turned by that location (EEE3DDamageVent::Step)
    static EeGfxVentPoints VentPoints(void* inAsk, int32_t inTag)
    {
        Ask* theAsk = static_cast<Ask*>(inAsk);
        EEComplexUnit* theComplex = theAsk->mEffect->mUnit->mComplexUnit;
        GETransformation theUnitTransformation(theComplex->mPhysicalModel.GetLocation());
        theAsk->mVentFrom->Scale(theUnitTransformation, theComplex->mStats->GetScale());
        GE3DPoint theFrom;
        GE3DPoint theTo;
        (theAsk->mVentFrom->GetTagPoint(inTag))->GetPoint(0, theUnitTransformation, theFrom);
        (theAsk->mVentTo->GetTagPoint(inTag))->GetPoint(0, theUnitTransformation, theTo);
        GEModel* theModel = theComplex->GetModel()->mModels[0];
        GE3DPoint theOffset;
        theAsk->mVentFrom->mPoints[inTag]->GetOffset(0, theOffset);
        GE3DPoint theCenter = theModel->GetCenterPoint();
        GEVector theTranslation;
        theUnitTransformation.Apply(GEVector(theOffset.GetX() - theCenter.GetX(), theOffset.GetY() - theCenter.GetY(),
                                             theOffset.GetZ() - theCenter.GetZ()), theTranslation);
        return EeGfxVentPoints{EeGe3DPoint{theFrom.mX, theFrom.mY, theFrom.mZ}, EeGe3DPoint{theTo.mX, theTo.mY, theTo.mZ},
                               EeGeVector{theTranslation.mX, theTranslation.mY, theTranslation.mZ}};
    }
    static EeGfxFrames Frames(void* inAsk, uint32_t inIndex)
    {
        EEGraphicObject* theObject = E(inAsk)->mGraphicObject;
        return EeGfxFrames{(uint32_t)theObject->mStartFrames[inIndex], (uint32_t)theObject->mFrameTimes[inIndex],
                           (uint32_t)theObject->mFrameCounts[inIndex], (uint32_t)theObject->mLastFrames[inIndex]};
    }
    static float UnitClassScale(void* inAsk) { return E(inAsk)->mUnit->GetClass()->GetScale(); }
    // the icon's set-up: the graphic effect's object loaded, then the one it replaces unloaded (EEE3DIcon::SetUp,
    // 0xf23e2: after the new load, so shared media stays loaded)
    static int32_t ReloadMedia(void* inAsk)
    {
        EEE3DRust* theEffect = E(inAsk);
        EEGraphicObject* thePrevious = theEffect->mGraphicObject;
        theEffect->mGraphicObject = theEffect->GetGraphicObject();
        long theResult = theEffect->mGraphicObject->LoadMedia();
        if (thePrevious)
            thePrevious->UnloadMedia();
        return theResult;
    }
    // the icon's place at its unit (EEE3DIcon::UpdatePosition, 0xf2c95)
    static EeGe3DPoint UnitRaisedPosition(void* inAsk)
    {
        GE3DPoint theLocation;
        EEUnit* theUnit = E(inAsk)->mUnit;
        if (theUnit) {
            theUnit->GetPosition(theLocation);
            if (theUnit->mTileNode && theUnit->mHitPoints > 0)
                theLocation.SetZ(theLocation.GetZ() + theUnit->GetComplexUnit()->GetEffectHeight(false));
        }
        return EeGe3DPoint{theLocation.mX, theLocation.mY, theLocation.mZ};
    }
    // the icon is not shown over an unbuilt market (EEE3DIcon::Queue, 0xf21e5)
    static bool MarketUnbuilt(void* inAsk)
    {
        EEUnit* theOwner = E(inAsk)->mUnit;
        EEComplexUnit* theUnit;
        return theOwner && (theUnit = dynamic_cast<EEComplexUnit*>(theOwner)) != 0
            && theUnit->mUnitClass->mUnitType == kUnitTypeMarketCP && !theUnit->mIsBuilt && !theUnit->IsFoundation();
    }
    static void QueueScreen(void* inAsk, uint32_t inIndex)
    {
        EEGraphicObject* theObject = E(inAsk)->GetGraphicObject();
        GETexture* theTexture = theObject ? theObject->GetTexture(gGamePreferences.mSettings.GetScreenID()) : NULL;
        static_cast<Ask*>(inAsk)->mQueues[inIndex][theTexture].push_back(E(inAsk));
    }
    // the tracer beam's ends (EEE3DTracerBeam::Aim, 0xfc5a4): from the shooter's weapon tag point (on its turret part
    // when the tag point's model is one), or its tag point of kind 6 (int param 1 is 1), or its stats' default point,
    // to the target point on the target's location (scaled by the target's scale when the weapon tag point was used)
    // or the fixed point
    static EeGfxTracerEnds TracerEnds(void* inAsk)
    {
        EEE3DRust* theEffect = E(inAsk);
        GE3DPoint theEnd;
        GE3DPoint theStart;
        GETransformation theFrom(theEffect->mUnit->mComplexUnit->mPhysicalModel.GetLocation());
        GETransformation theTo(false);
        GETransformation thePart(false);
        EEGraphicObject* theGraphic = theEffect->mUnit->GetModel();
        EEComplexUnit* theUnit = theEffect->mUnit->mComplexUnit;
        float theTargetScale = 1.0f;
        EETagPointList* theTags = theGraphic->mTagPoints[theUnit->GetAnimation()];
        float theScale = theUnit->mPhysicalModel.GetScale();
        unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
        unsigned long thePoint = theEffect->mRecord->tracer.weapon_tag_point;
        EEUnit* theTarget = theEffect->mTarget;
        GE3DPoint theTargetPoint(theEffect->mRecord->tracer.target_point.x, theEffect->mRecord->tracer.target_point.y,
                                 theEffect->mRecord->tracer.target_point.z);
        if (theTarget) {
            theTo = theTarget->mComplexUnit->mPhysicalModel.GetLocation();
            theTargetScale = theTarget->mComplexUnit->mPhysicalModel.GetScale();
        } else
            theTo.MakeIdentity();
        if (theEffect->mGraphicEffect->mIntParam1 == 1) {
            EETagPointList* theOtherTags = theUnit->GetModel()->GetTagPoints(theUnit->GetAnimation(), 6);
            if (theOtherTags && theOtherTags->GetNumTagPoints() != 0) {
                EETagPointList::Scale(theFrom, theScale);
                theOtherTags->GetTagPoint(0)->GetPoint(theFrame, theFrom, theStart);
            }
        } else if (theEffect->mGraphicEffect->mIntParam1 == 0 && theTags && theTags->GetNumTagPoints() != 0) {
            EETagPointList::Scale(theFrom, theScale);
            if (theGraphic->GetNumParts() > 0) {
                unsigned long thePartIndex;
                thePart.MakeIdentity();
                if (theGraphic->FindTurretPart(theTags->GetTagPoint(thePoint)->mModel, thePartIndex) == 0)
                    thePart.SetOrientationYPR(theUnit->mTurretAngles[thePartIndex], 0.0f, 0.0f);
                theTags->GetTagPoint(thePoint)->GetPoint(theFrame, theFrom, thePart, theStart);
            } else
                theTags->GetTagPoint(thePoint)->GetPoint(theFrame, theFrom, theStart);
            EETagPointList::Scale(theTo, theTargetScale);
        } else
            theFrom.Apply(theEffect->mUnit->mStats->mModel->mDefaultPoint, theStart);
        if (theTarget)
            theTo.Apply(theTargetPoint, theEnd);
        else
            theEnd = theTargetPoint;
        return EeGfxTracerEnds{EeGe3DPoint{theStart.mX, theStart.mY, theStart.mZ}, EeGe3DPoint{theEnd.mX, theEnd.mY, theEnd.mZ},
                               theTarget != NULL};
    }
    // the tracer beam's impact effect at its target point (EEE3DTracerBeam::Step, 0xfd140)
    static int32_t TracerImpact(void* inAsk)
    {
        EEE3DRust* theEffect = E(inAsk);
        GETransformation theTransformation;
        GE3DPoint theLocation;
        GE3DPoint theTargetPoint(theEffect->mRecord->tracer.target_point.x, theEffect->mRecord->tracer.target_point.y,
                                 theEffect->mRecord->tracer.target_point.z);
        if (theEffect->mTarget) {
            theTransformation = theEffect->mTarget->mComplexUnit->mPhysicalModel.GetLocation();
            theTransformation.Apply(theTargetPoint, theLocation);
        } else
            theLocation = theTargetPoint;
        return gEffectManager->CreatePlacedEffect(theLocation, theEffect->mGraphicEffect->mLinkedEffect1, NULL);
    }
    static void TracerHit(void* inAsk)
    {
        EEUnit* theTarget = E(inAsk)->mTarget;
        theTarget->mUnitClass->UnitDamaged(theTarget->mComplexUnit, NULL, 0);
    }
    // serialized ID; RepairPointers resolves it through gWorld.GetUnit
    static void TracerSavedTarget(void* inAsk, uint32_t inID) { E(inAsk)->mTarget = USavedPointer<EEUnit>(inID); }
    static uint32_t TracerTargetID(void* inAsk)
    {
        EEUnit* theTarget = E(inAsk)->mTarget;
        return theTarget ? (uint32_t)theTarget->GetUnitID() : 0xffffffffu;
    }
    // the volcano's model over the record's mesh lists (EEE3DVolcano::BuildModel, 0xfe79d): each tile of its area
    // split 3x3, following the terrain, coloured by terrain and lighting and blended into textures\Volcano.png near
    // the centre, and the cone heights from the image's alpha (ee_gfx volcano.rs); the model made and referenced,
    // placed at its centre, the bounding radius to the cone's top, the texture
    static int32_t VolcanoBuild(void* inAsk, EeGe3DPoint* outPoints, float* outHeights, uint32_t inCount)
    {
        EEE3DRust* theEffect = E(inAsk);
        EeGfxEffectVolcano& theVolcano = theEffect->mRecord->volcano;
        GETransformation theTransformation;
        GE3DPoint theTop;
        FSFileSpec* theFile = NULL;
        GESurface* theSurface = NULL;
        GESurfaceDescription theDescription;
        float theHeightScale = theEffect->mGraphicEffect->mSize;
        EeGfxVolcanoSizes theSizes = ee_gfx_volcano_sizes(theVolcano.area.right - theVolcano.area.left);
        std::vector<EeGfxColor> theColors(theSizes.vertices);
        theDescription.mWidth = 19;
        theDescription.mHeight = 19;
        theDescription.mIs3DRenderable = false;
        theDescription.mIsTransparencyEnabled = false;
        theDescription.mPixelFormat = kPixelFormatA8R8G8B8;
        theDescription.mSurfaceVolatility = 0;
        theDescription.m14 = 0;
        theDescription.mMipLevelCount = 0;
        theDescription.mType = kSurfaceSystemMemory;
        theDescription.mPixelDepth = 32;
        long theResult = gRasterizer->GetScreen(gScreenIndex)->CreateSurface(theDescription, &theSurface);
        ESVERIFY_AT(theResult == keseOK, 233);

        UString theFileName = UString("textures\\Volcano.png");   // the original's .tga as PNG (RUST_PLAN D9)
        theResult = gTextureManager->GetTexturePath()->FindFile(theFileName, theFile);
        ESVERIFY_AT(theResult == keseOK, 238);
        ESVERIFY_AT(theFile != NULL, 239);
        theSurface->Load(*theFile, kImageFilePNG);
        ESVERIFY_AT(theResult == keseOK, 241);
        if (theFile != NULL)
            delete theFile;
        theSurface->Lock();
        const uint32_t* thePixels = reinterpret_cast<const uint32_t*>(theSurface->GetMemoryAddress());
        ee_gfx_volcano_image(theHeightScale, theSizes.side, thePixels, theSurface->GetBytesPerRow() / 4,
                             theColors.empty() ? NULL : &theColors[0], inCount == 0 ? NULL : outHeights, theColors.size());
        theSurface->Unlock();
        theSurface->Dereference();
        GEModel* theModel = gModelManager->CreateModel(theSizes.polygons, theSizes.vertices, 1, 0, 1, 0, "volcano model", 1);
        theEffect->mVolcanoModel = theModel;
        theModel->Reference();
        theModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
        theModel->GetMaterial(0)->SetPolygonRangeLength(theSizes.polygons, 0);
        theModel->GetMaterial(0)->SetVertexStartingIndex(0);
        theModel->GetMaterial(0)->SetVertexRangeLength(theSizes.vertices);
        EeGfxVolcanoTerrain theTerrain;
        theTerrain.context = ::gWorld.mMap;
        theTerrain.z = EEVolcanoZ;
        theTerrain.color = EEVolcanoColor;
        theTerrain.valid_tile = EEVolcanoValidTile;
        theTerrain.light = EEVolcanoLight;
        EEEffectMesh theMesh(theSizes.vertices, theSizes.polygons);
        EeGfxVolcanoArea theArea = theVolcano.area;
        EeGfxVolcanoBuilt theBuilt = ee_gfx_volcano_build(
            &theArea, theColors.empty() ? NULL : &theColors[0], theTerrain, inCount == 0 ? NULL : outPoints,
            inCount == 0 ? NULL : outHeights, theColors.size(), theMesh.Vertices(), theMesh.mVertices.size(),
            theMesh.Triangles(), theMesh.mTriangles.size());
        theMesh.Store(theModel);
        theVolcano.area.center.z = theBuilt.center_z;
        float theMaximum = theBuilt.maximum;
        theModel->ComputeModelNormals();
        theModel->ComputeBoundingVolumes();
        theModel->SetGraphicsPostModelTransform(theTransformation);
        theModel->PrepareTransforms();
        theTransformation.SetPosition(theModel->GetCenterPoint().GetX(), theModel->GetCenterPoint().GetY(),
                                      theModel->GetCenterPoint().GetZ());
        theEffect->SetTransformation(theTransformation);
        theTop.SetX(theModel->GetCenterPoint().GetX());
        theTop.SetY(theModel->GetCenterPoint().GetY());
        theTop.SetZ(theHeightScale + theMaximum);
        float theRadius = theModel->GetCenterPoint().DistanceTo(theTop);
        if (theRadius <= theModel->GetBoundingRadius(0))
            theRadius = theModel->GetBoundingRadius(0);
        theEffect->mBoundingRadius = theRadius;
        if (theResult == kErrorOK)
            theModel->GetMaterial(0)->SetTexture(theEffect->mGraphicObject->mTextures[0], gScreenIndex);
        return (int32_t)theResult;
    }
    static void* VolcanoModel(void* inAsk) { return E(inAsk)->mVolcanoModel; }
    // the farm's (EEE3DFarm, 0xee5c0..0xf1327): its unit, the terrain, its first drop site's tile, its crop variant
    // (the unit's mCloaked), the graphic's loops, the doppelgangers, the draw opacity, the scaled model
    static EeGfxFarmUnit FarmUnit(void* inAsk)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        return EeGfxFarmUnit{(int32_t)theUnit->GetTileX(), (int32_t)theUnit->GetTileY(),
                             (int32_t)theUnit->GetClass()->GetTileSize(), theComplex->mIsBuilt, (int32_t)theComplex->mCloaked};
    }
    static EeGfxFarmTerrain FarmTerrain(void*)
    {
        EeGfxFarmTerrain theTerrain;
        theTerrain.context = &gWorld.mMap->mTerrainMesh;
        theTerrain.z = EEFarmZ;
        theTerrain.normal = EEFarmNormal;
        theTerrain.mesh_z = EEFarmMeshZ;
        return theTerrain;
    }
    static bool FarmDropSite(void* inAsk, int32_t* outX, int32_t* outY)
    {
        std::vector<EEUnit*> theUnits;
        if (!EEGetFarmDropSites(E(inAsk)->mUnit->mComplexUnit, theUnits))
            return false;
        *outX = (int32_t)theUnits[0]->mComplexUnit->GetTileX();
        *outY = (int32_t)theUnits[0]->mComplexUnit->GetTileY();
        return true;
    }
    static void FarmSetVariant(void* inAsk, int32_t inVariant) { E(inAsk)->mUnit->mComplexUnit->mCloaked = inVariant; }
    static bool FarmLoops(void* inAsk, uint32_t inIndex) { return E(inAsk)->mGraphicObject->mLoops[inIndex]; }
    static bool FarmDoppelgangerAny(void* inAsk)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        return gDoppelgangerManager.HasDoppelganger(static_cast<EETile*>(theUnit->mTile), theUnit) == true;
    }
    static int32_t FarmDoppelganger(void* inAsk)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        EEPlayer* theCurrent = gWorld.GetCurrentPlayer();
        EEDoppelganger* theDoppelganger = NULL;
        if (theUnit->mTile)
            theDoppelganger = gDoppelgangerManager.GetDoppelganger(static_cast<EETile*>(theUnit->mTile), theUnit, theCurrent->mPlayerID);
        if (theDoppelganger && !gFreeCamera && !theCurrent->mSeesAll)
            return theDoppelganger->IsBuilt() ? 1 : 0;
        return -1;
    }
    static float FarmOpacity(void* inAsk, float inOpacity)
    {
        Fn23CEE2(E(inAsk)->mUnit->mComplexUnit, false, inOpacity);
        return inOpacity;
    }
    static void SetScaledModel(void* inAsk, void* inModel)
    {
        E(inAsk)->mUnit->mComplexUnit->SetScaledModel(static_cast<GEModel*>(inModel));
    }
    // the smoke generators' (EEE3DSmokeGenerator::Step, 0xf9a1b): the unit's place and command; the damage smoke's
    // (0xea75c) whole unit: its health and its tag points of kind 0 at its frame in its scaled location
    static EeGfxSmokeUnit SmokeUnit(void* inAsk)
    {
        Ask* theAsk = static_cast<Ask*>(inAsk);
        EEE3DRust* theEffect = theAsk->mEffect;
        EeGfxSmokeUnit theFacts = {};
        if (theEffect->mKind == 0x2c) {
            EEComplexUnit* theUnit = theEffect->mUnit->mComplexUnit;
            theAsk->mSmokeLocation = theUnit->mPhysicalModel.GetLocation();
            theAsk->mSmokeTags = theEffect->mUnit->GetModel()->GetTagPoints(theUnit->GetAnimation(), 0);
            theAsk->mSmokeFrame = theUnit->mPhysicalModel.GetFrame();
            // Scale only a list that exists (a model without type-0 tag points has none).
            if (theAsk->mSmokeTags)
                EETagPointList::Scale(theAsk->mSmokeLocation, theUnit->mPhysicalModel.GetScale());
            const GETransformation& theUnitLocation = theUnit->mPhysicalModel.GetLocation();
            theFacts.has_unit = true;
            theFacts.position = EeGe3DPoint{theUnitLocation.GetX(), theUnitLocation.GetY(), theUnitLocation.GetZ()};
            theFacts.command = theUnit->mCommand ? (int32_t)theUnit->mCommand->mType : 0;
            theFacts.hit_points = (int32_t)theEffect->mUnit->mHitPoints;
            theFacts.max_hit_points = (int32_t)theEffect->mUnit->mStats->mMaxHitPoints;
            theFacts.has_tag_points = theAsk->mSmokeTags != NULL;
            theFacts.tag_point_count = theAsk->mSmokeTags ? (uint32_t)theAsk->mSmokeTags->mPoints.size() : 0;
        } else if (theEffect->mUnit) {
            GE3DPoint theLocation;
            theEffect->mUnit->mComplexUnit->mPhysicalModel.GetLocation().GetPosition(theLocation);
            theFacts.has_unit = true;
            theFacts.position = EeGe3DPoint{theLocation.mX, theLocation.mY, theLocation.mZ};
            theFacts.command = (int32_t)theEffect->mUnit->mComplexUnit->GetCommandType();
        }
        return theFacts;
    }
    static EeGe3DPoint SmokeTagPoint(void* inAsk, uint32_t inIndex)
    {
        Ask* theAsk = static_cast<Ask*>(inAsk);
        GE3DPoint thePoint;
        theAsk->mSmokeTags->mPoints[inIndex]->GetPoint(theAsk->mSmokeFrame, theAsk->mSmokeLocation, thePoint);
        return EeGe3DPoint{thePoint.mX, thePoint.mY, thePoint.mZ};
    }
    static int32_t SmokePlace(void* inAsk, const EeGe3DPoint* inPoint)
    {
        GE3DPoint thePoint(inPoint->x, inPoint->y, inPoint->z);
        return (int32_t)gEffectManager->CreatePlacedEffect(thePoint, E(inAsk)->mGraphicEffect->mLinkedEffect1, NULL);
    }
    // the earthquake's (EEE3DEarthquake, 0xec6e4..0xed606): the ground under a point
    static EeGe3DPoint GroundPoint(void*, const EeGe3DPoint* inPoint)
    {
        GE3DPoint theGround(inPoint->x, inPoint->y, inPoint->z);
        gWorld.mMap->GetGroundPoint(theGround, theGround);
        return EeGe3DPoint{theGround.mX, theGround.mY, theGround.mZ};
    }
    // the rain's (EEE3DRain::Virtual64, 0xf7792): its three sound objects' media
    static void RainMedia(void* inAsk)
    {
        for (int i = 0; i < 3; i++) {
            EESoundObject* theSound = EESoundObject::Get(E(inAsk)->mRecord->storm.sounds[i]);
            if (theSound)
                theSound->LoadMedia();
        }
    }
    // the rain's sounds (EEE3DRain::StartSounds, 0xf8195, after its thunder timer): the loop at the start or after a
    // load, the thunder inRumble (1 or 2), both levels
    static void RainSounds(void* inAsk, bool inStart, int32_t inRumble)
    {
        EEE3DRust* theRain = E(inAsk);
        const uint32_t* theIDs = theRain->mRecord->storm.sounds;
        EESoundObject* theSound;
        if (inStart && (theSound = EESoundObject::Get(theIDs[0])) != NULL)
            EESetEffectSound(theRain->mSound, gSoundManager->PlayAndReturnReferencedNode(
                theSound->GetSampleID(), theSound->GetVolume(), 0.0f, 1.0f, theSound->GetLooping(), "3DRain"));
        if (inRumble != 0 && (theSound = EESoundObject::Get(theIDs[inRumble])) != NULL)
            EESetEffectSound(theRain->mRumble, gSoundManager->PlayAndReturnReferencedNode(
                theSound->GetSampleID(), theSound->GetVolume(), 0.0f, 1.0f, theSound->GetLooping(), "3D Rain Rumble"));
        EESetRainSoundLevel(theRain, theRain->mSound, 0.5f);
        EESetRainSoundLevel(theRain, theRain->mRumble, 1.0f);
    }
    // the particles' tick and ground (EEE3DParticle::Update 0xf50b2, EEE3DParticleEmitter::Move 0xf60f5), the graphic
    // effect's duration (EEE3DParticle::Load 0xf53bc)
    static EeGfxParticleTick ParticleTick(void* inAsk, const EeGe3DPoint* inPosition)
    {
        return EEParticleTick(E(inAsk)->mGraphicEffect, *reinterpret_cast<const GE3DPoint*>(inPosition));
    }
    static EeGfxParticleGround ParticleGround(void*) { return EEParticleGround(); }
    static uint32_t EffectDuration(void* inAsk) { return (uint32_t)E(inAsk)->mGraphicEffect->mDuration; }
    // the emitter on a unit (0xf5f23): at its location unless it follows the tag point; a unit without a complex unit
    // at its tile's centre on the terrain
    static void EmitterPlace(void* inAsk)
    {
        EEE3DRust* theEffect = E(inAsk);
        EEUnit* theOwner = theEffect->mUnit;
        GETransformation theTransformation;
        EEComplexUnit* theUnit = theOwner->GetComplexUnit();
        if (theUnit) {
            if (!theEffect->mGraphicEffect->mFlag1)
                theTransformation = theUnit->mPhysicalModel.GetLocation();
        } else {
            GE3DPoint theLocation;
            float theHalf = theOwner->mUnitClass->mTileSize * 0.5f;
            theLocation.SetX(theOwner->mTileX + theHalf);
            theLocation.SetY(theOwner->mTileY + theHalf);
            if (::gWorld.mMap->mTerrainMesh.GetIsValidTile(theOwner->mTileX, theOwner->mTileY))
                theLocation.SetZ(::gWorld.mMap->GetHeight(theLocation));
            else
                theLocation.SetZ(0.0f);
            theTransformation.MakeIdentity();
            theTransformation.SetPosition(theLocation);
        }
        theEffect->SetTransformation(theTransformation);
    }
    // the emitter's unit for its step (0xf6417): its death and hit points; followed (the graphic effect's flag 1), the
    // tag point at its frame in its scaled location (turned by its turret part), else its position
    static EeGfxEmitterUnit EmitterUnit(void* inAsk)
    {
        EEE3DRust* theEffect = E(inAsk);
        EEGraphicEffect* theEffectData = theEffect->mGraphicEffect;
        EeGfxEmitterUnit theFacts = {};
        if (theEffect->mUnit == NULL)
            return theFacts;
        theFacts.has_unit = true;
        EEComplexUnit* theUnit = theEffect->mUnit->mComplexUnit;
        if (theUnit == NULL) {
            theFacts.hit_points = (int32_t)theEffect->mUnit->GetHitPoints();
            return theFacts;
        }
        theFacts.complex = true;
        if (theEffectData->mFlag1) {
            GE3DPoint theLocation;
            GETransformation theTransformation(theUnit->mPhysicalModel.GetLocation());
            GETransformation thePart(false);
            EEGraphicObject* theGraphic = theEffect->mUnit->GetModel();
            EETagPointList* theTags = theGraphic->GetTagPoints(theUnit->GetAnimation(), theEffectData->mTagPointKind);
            uint32_t theTag = theEffect->mRecord->emitter.tag_point;
            if (theEffectData->mTagPointKind != 0x15 && theTags) {
                unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
                theTags->Scale(theTransformation, theUnit->mPhysicalModel.GetScale());
                if (theGraphic->GetNumParts() > 0) {
                    unsigned long thePartIndex;
                    thePart.MakeIdentity();
                    theFacts.follow_result = (int32_t)theGraphic->FindTurretPart(theTags->GetTagPoint(theTag)->mModel, thePartIndex);
                    if (theFacts.follow_result == 0)
                        thePart.SetOrientationYPR(theUnit->mTurretAngles[thePartIndex], 0.0f, 0.0f);
                    theTags->GetTagPoint(theTag)->GetPoint(theFrame, theTransformation, thePart, theLocation);
                } else
                    theTags->GetTagPoint(theTag)->GetPoint(theFrame, theTransformation, theLocation);
            } else
                theTransformation.GetPosition(theLocation);
            theFacts.location = EeGe3DPoint{theLocation.mX, theLocation.mY, theLocation.mZ};
            theFacts.transformation = *reinterpret_cast<const EeGeTransformation*>(&theTransformation);
        }
        theFacts.dead = theUnit->GetCommandType() == kActionDead;
        theFacts.hit_points = (int32_t)theUnit->GetHitPoints();
        return theFacts;
    }
    // the emitter's animation sound at its place (its graphic effect's object, if any)
    static void EmitterSound(void* inAsk, const EeGe3DPoint* inLocation, bool inFirst)
    {
        EEE3DRust* theEffect = E(inAsk);
        EEGraphicObject* theObject = theEffect->mGraphicEffect->mGraphicObject;
        if (theObject)
            EESetEffectSound(theEffect->mSound, PlayAnimationSound(*reinterpret_cast<const GE3DPoint*>(inLocation), theObject, 0,
                                                                   theEffect->mSound, 0, inFirst));
    }
    // a line particle of the emitter's burst (CreateLineEffect of the linked effect 1)
    static int32_t EmitterLine(void* inAsk, const EeGe3DPoint* inStart, const EeGe3DPoint* inDirection)
    {
        GE3DLine theLine;
        theLine.SetStart(*reinterpret_cast<const GE3DPoint*>(inStart));
        theLine.SetDirection(GEVector(inDirection->x, inDirection->y, inDirection->z));
        return (int32_t)gEffectManager->CreateLineEffect(theLine, E(inAsk)->mGraphicEffect->mLinkedEffect1, NULL);
    }
    // the marching line's: the base effect's queue (not the 3D one's); its graphic effect's object's media; its ends
    // (the units' positions, else the points); its units' saved ids (as the original, kept as pointers until
    // RepairPointers) and their ids for a save (-1: none)
    static int32_t EffectQueue(void* inAsk) { return E(inAsk)->EEEffect::Queue(static_cast<Ask*>(inAsk)->mQueues); }
    static void MarchMedia(void* inAsk)
    {
        E(inAsk)->mGraphicObject = E(inAsk)->mGraphicEffect->mGraphicObject;
        E(inAsk)->mGraphicObject->LoadMedia();
    }
    static void MarchEnds(void* inAsk, const EeGe3DPoint* inFrom, const EeGe3DPoint* inTo, EeGe3DPoint* outEnds)
    {
        EEUnit* theUnits[2] = {E(inAsk)->mFromUnit, E(inAsk)->mToUnit};
        const EeGe3DPoint* thePoints[2] = {inFrom, inTo};
        for (int i = 0; i < 2; i++) {
            GE3DPoint thePoint(thePoints[i]->x, thePoints[i]->y, thePoints[i]->z);
            if (theUnits[i])
                theUnits[i]->GetPosition(thePoint);
            outEnds[i] = EeGe3DPoint{thePoint.mX, thePoint.mY, thePoint.mZ};
        }
    }
    static void MarchSetUnit(void* inAsk, uint32_t inWhich, uint32_t inID)
    {
        (inWhich == 0 ? E(inAsk)->mFromUnit : E(inAsk)->mToUnit) = reinterpret_cast<EEUnit*>((uintptr_t)inID);
    }
    static int32_t MarchUnitID(void* inAsk, uint32_t inWhich)
    {
        EEUnit* theUnit = inWhich == 0 ? E(inAsk)->mFromUnit : E(inAsk)->mToUnit;
        return theUnit ? (int32_t)theUnit->GetUnitID() : -1;
    }
};

// a shot's start: the unit's weapon point (ee_combat's weapon_point; inMode 6 the pulse cannon's, 7 the tracers');
// whether the unit's model has tag points at its animation
bool ShotStart(long inMode, EEUnit* inUnit, EeGe3DPoint& outStart)
{
    GE3DPoint theStart;
    EEModel* theModel = inUnit->GetModel();
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    EETagPointList* theList = inUnit->GetModel()->mTagPoints[theUnit->GetAnimation()];
    bool theTagged = theList != NULL && theList->GetNumTagPoints() != 0;
    EECombatEffectWeaponPoint(inMode, theUnit, theModel, theList, theTagged, theStart);
    outStart = EeGe3DPoint{theStart.mX, theStart.mY, theStart.mZ};
    return theTagged;
}
}

// on a unit (the multi-effect's own unit is none: its linked effects are made on inUnit)
EEE3DRust::EEE3DRust(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inKind == 0x3b ? NULL : inUnit, inGraphicEffect, inKind)
{
    this->Call(EeGfxEffect3DOp::Create, NULL, NULL, 0, 0, 0, inUnit);
}

EEE3DRust::EEE3DRust(const GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(0, inGraphicEffect, inKind)
{
    this->mRecord->aged.location = EeGe3DPoint{inLocation.mX, inLocation.mY, inLocation.mZ};
    this->Call(EeGfxEffect3DOp::CreateAt, NULL, NULL, 0, 0, 0, const_cast<GE3DPoint*>(&inLocation));
}

// the multi-effect (0x3b) along a line (EEE3DMultiEffect, 0xf42c3)
EEE3DRust::EEE3DRust(GE3DLine& inLine, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(0, inGraphicEffect, inKind)
{
    if (inKind == 0x39 || inKind == 0x3a) {   // the 3D particle (0xf4e27), the emitter (0xf5e82)
        const GE3DPoint& theStart = inLine.GetStart();
        const GEVector& theDirection = inLine.GetDirection();
        EeGe3DPoint theVelocity = {theDirection.GetX(), theDirection.GetY(), theDirection.GetZ()};
        this->mRecord->aged.location = EeGe3DPoint{theStart.mX, theStart.mY, theStart.mZ};
        (inKind == 0x39 ? this->mRecord->particle.velocity : this->mRecord->emitter.velocity) = theVelocity;
    }
    this->Call(EeGfxEffect3DOp::CreateLine, NULL, NULL, 0, 0, 0, &inLine);
}

// the pulse cannon ball (0x43): from the unit's weapon point to the target (EEE3DPulseCannonBall, 0xf6a26); the
// citizen beam (0x33): towards the target (EEE3DCitizenBeam, 0xe94b7); the tracer beam (0x40) to it
EEE3DRust::EEE3DRust(EEUnit* inUnit, const GE3DPoint& inTarget, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inUnit, inGraphicEffect, inKind)
{
    EeGe3DPoint theTarget = {inTarget.mX, inTarget.mY, inTarget.mZ};
    if (inKind == 0x40) {   // the tracer beam to a fixed point (EEE3DTracerBeam, 0xfc49d)
        this->mRecord->tracer.target_point = theTarget;
        this->mRecord->tracer.weapon_tag_point = inUnit->mComplexUnit->mWeaponTagPoint;
    } else if (inKind == 0x33) {   // the citizen beam
        this->mRecord->shot.end = theTarget;
    } else {
        ShotStart(6, inUnit, this->mRecord->shot.path.start);
        this->mRecord->shot.location = theTarget;
    }
    this->Call(EeGfxEffect3DOp::Create);
}

// the tracers (0x21) and shells (0x26) on inTo: from inFrom's weapon point to a point of inTo's model, in inFrom's
// player colour (EEE3DTracers, 0xfd373); the tracer beam (0x40) of inFrom on inTo, referenced, aimed at a point picked
// on its model (EEE3DTracerBeam, 0xfc3c5: the original tested the target for null, then used it)
EEE3DRust::EEE3DRust(EEUnit* inFrom, EEUnit* inTo, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(inKind == 0x40 ? inFrom : inTo, inGraphicEffect, inKind)
{
    if (inKind == 0x40) {
        this->mTarget = inTo;
        if (this->mTarget) {
            this->mTarget->Reference();
            const GE3DPoint& thePick = this->mTarget->GetModel()->PickPoint();
            this->mRecord->tracer.target_point = EeGe3DPoint{thePick.mX, thePick.mY, thePick.mZ};
        }
        this->mRecord->tracer.weapon_tag_point = inFrom->mComplexUnit->mWeaponTagPoint;
        this->Call(EeGfxEffect3DOp::Create);
        return;
    }
    GE3DPoint theEnd;
    GETransformation theToTransformation(inTo->mComplexUnit->mPhysicalModel.GetLocation());
    float theToScale = inTo->mComplexUnit->mPhysicalModel.GetScale();
    if (ShotStart(7, inFrom, this->mRecord->shot.path.start))
        EETagPointList::Scale(theToTransformation, theToScale);
    GE3DPoint theOffset = inTo->GetModel()->PickPoint();
    theToTransformation.Apply(theOffset, theEnd);
    const GEColor& theColor = inFrom->mPlayer->mColor;
    this->mRecord->shot.location = EeGe3DPoint{theEnd.mX, theEnd.mY, theEnd.mZ};
    this->mRecord->shot.color = EeGfxColor{theColor.mRed, theColor.mGreen, theColor.mBlue};
    this->Call(EeGfxEffect3DOp::Create);
}

// the linear projectile (0x25): from inFrom to inTo (EEE3DLinearProjectile, 0xf2dd6); the earthquake (0x1a): a crack
// from inFrom towards inTo (EEE3DEarthquake, 0xec727)
EEE3DRust::EEE3DRust(const GE3DPoint& inFrom, const GE3DPoint& inTo, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(0, inGraphicEffect, inKind)
{
    if (inKind == 0x2d) {   // the marching line between the points (EEE3DMarchingLine, 0xf359c)
        this->mRecord->march.from = EeGe3DPoint{inFrom.mX, inFrom.mY, inFrom.mZ};
        this->mRecord->march.to = EeGe3DPoint{inTo.mX, inTo.mY, inTo.mZ};
        return;
    }
    this->mRecord->shot.path.start = EeGe3DPoint{inFrom.mX, inFrom.mY, inFrom.mZ};
    this->mRecord->shot.location = EeGe3DPoint{inTo.mX, inTo.mY, inTo.mZ};
    this->Call(EeGfxEffect3DOp::Create);
}

EEE3DRust::EEE3DRust(long inKind)
    : EEEffect3D(inKind)
{
}

// the rain (0x2a) and the snow (0x3d) over the tiles, centred on them (EEE3DStorm, 0xfb0c8; the rain's first thunder,
// EEE3DRain 0xf7394)
EEE3DRust::EEE3DRust(const std::vector<U2DPoint<long> >& inTiles, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect3D(0, inGraphicEffect, inKind)
{
    std::vector<int32_t> theTiles;
    for (size_t i = 0; i < inTiles.size(); i++) {
        theTiles.push_back((int32_t)inTiles[i].mX);
        theTiles.push_back((int32_t)inTiles[i].mY);
    }
    ee_gfx_storm_new(this->mRecord, theTiles.empty() ? NULL : &theTiles[0], (uint32_t)inTiles.size());
    this->Call(EeGfxEffect3DOp::Create);
}

// the weather storms': every tile one step towards its place around the new centre (a step off the map stays), the
// storm recentred on the terrain there; the tiles' bounds; the radius half the longer side (ee_gfx effect_storm.rs)
void EEE3DRust::Move(const U2DPoint<long>& inCenter, U2DRectangle<long>& outBounds)
{
    int32_t theBounds[4] = {(int32_t)outBounds.mLeft, (int32_t)outBounds.mTop, (int32_t)outBounds.mRight,
                            (int32_t)outBounds.mBottom};
    EeGe3DPoint thePoint;
    if (!ee_gfx_storm_move(this->mRecord, (int32_t)inCenter.mX, (int32_t)inCenter.mY, theBounds, &kStormHost, &thePoint))
        return;
    outBounds.mLeft = theBounds[0];
    outBounds.mTop = theBounds[1];
    outBounds.mRight = theBounds[2];
    outBounds.mBottom = theBounds[3];
    GETransformation theTransformation;
    theTransformation.SetPosition(thePoint.x, thePoint.y, thePoint.z);
    this->SetTransformation(theTransformation);
}

void EEE3DRust::GetBounds(U2DRectangle<long>& outBounds)
{
    int32_t theBounds[4];
    if (!ee_gfx_storm_bounds(this->mRecord, theBounds))
        return;
    outBounds.mLeft = theBounds[0];
    outBounds.mTop = theBounds[1];
    outBounds.mRight = theBounds[2];
    outBounds.mBottom = theBounds[3];
}

U2DPoint<long> EEE3DRust::GetCenterPoint() const
{
    return U2DPoint<long>(this->mRecord->storm.center_x, this->mRecord->storm.center_y);
}

// stops the sound (the disease's and virus's), unloads the graphic object's media and the unit model's (the
// additive pass's), releases the tracer beam's target
EEE3DRust::~EEE3DRust()
{
    if (this->mKind == 0x2d)   // the marching line's regions (0xf3736)
        this->RemoveRegions();
    if (this->mSound) {
        gSoundManager->Stop(this->mSound, false);
        this->mSound->Dereference();
        this->mSound = NULL;
    }
    if (this->mRumble) {   // the rain's thunder, then its three sound objects' media and its drop model (0xf74a0)
        gSoundManager->Stop(this->mRumble, false);
        this->mRumble->Dereference();
        this->mRumble = NULL;
    }
    if (this->mKind == 0x2a) {
        for (int i = 0; i < 3; i++) {
            EESoundObject* theSound = EESoundObject::Get(this->mRecord->storm.sounds[i]);
            if (theSound)
                theSound->UnloadMedia();
        }
    }
    if ((this->mKind == 0x2a || this->mKind == 0x3d) && this->mRecord->storm.model)
        static_cast<GEModel*>(this->mRecord->storm.model)->Dereference();
    if (this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
    if (this->mUnitModel) {
        this->mUnitModel->UnloadMedia();
        this->mUnitModel = NULL;
    }
    if (this->mTarget != NULL && this->mTarget != (EEUnit*)-1)
        this->mTarget->Dereference();
    if (this->mFromUnit)   // the marching line's units (not referenced by a repair: as the original)
        this->mFromUnit->Dereference();
    if (this->mToUnit)
        this->mToUnit->Dereference();
    if (this->mVolcanoModel)   // the original dereferenced it without a test
        this->mVolcanoModel->Dereference();
    if (this->mKind == 0x29) {   // the farm's ground model and draw copies (EEE3DFarm's destructor, 0xee775)
        EeGfxEffectFarm& theFarm = this->mRecord->farm;
        if (theFarm.ground)
            static_cast<GEModel*>(theFarm.ground)->Dereference();
        if (theFarm.ground_copy)
            static_cast<GEModel*>(theFarm.ground_copy)->Dereference();
        if (theFarm.crops_copy)
            static_cast<GEModel*>(theFarm.crops_copy)->Dereference();
    }
    // the earthquake's model (EEE3DEarthquake's destructor, 0xec88f: not null-checked there)
    if (this->mKind == 0x1a && this->mRecord->quake.model)
        static_cast<GEModel*>(this->mRecord->quake.model)->Dereference();
}

// the facts of an effect's graphic effect (inEffect), unit (inUnit) and the tick the Rust effects read (EEE3DRust's and
// EEE2DRust's)
EeGfxGraphicFacts EEEffectFacts(EEGraphicEffect* inEffect, EEUnit* inUnit)
{
    EEGraphicEffect* theEffect = inEffect;
    EEGraphicObject* theObject = theEffect ? theEffect->mGraphicObject : NULL;
    EeGfxGraphicFacts theFacts = {};
    if (theEffect != NULL) {
        theFacts.size = theEffect->mSize;
        theFacts.start_size = theEffect->mStartSize;
        theFacts.param1 = theEffect->mParam1;
        theFacts.param2 = theEffect->mParam2;
        theFacts.param3 = theEffect->mParam3;
        theFacts.param4 = theEffect->mParam4;
        theFacts.param5 = theEffect->mParam5;
        theFacts.param6 = theEffect->mParam6;
        theFacts.flag1 = theEffect->mFlag1;
        theFacts.flag2 = theEffect->mFlag2;
        theFacts.param7 = theEffect->mParam7;
        theFacts.param8 = theEffect->mParam8;
        theFacts.duration = (uint32_t)theEffect->mDuration;
        theFacts.has_object = theObject != NULL;
        theFacts.alpha_ref = theObject != NULL ? theObject->mAlphaReference : 0.0f;
    }
    theFacts.tick_seconds = gPhysics->GetTickSeconds();
    theFacts.tick_time = gPhysics->GetTickTime();
    theFacts.has_unit = inUnit != NULL;
    theFacts.unit_hit_points = inUnit != NULL ? (int32_t)inUnit->mHitPoints : 0;
    if (theEffect != NULL) {
        theFacts.int_param1 = (int32_t)theEffect->mIntParam1;
        theFacts.int_param2 = (int32_t)theEffect->mIntParam2;
        theFacts.int_param3 = (int32_t)theEffect->mIntParam3;
        theFacts.has_linked1 = theEffect->mLinkedEffect1 != NULL;
        theFacts.has_linked3 = theEffect->mLinkedEffect3 != NULL;
    }
    theFacts.game_time_bits = (uint32_t)(unsigned long)gPhysics->mGameTime;
    theFacts.free_camera = gFreeCamera;
    theFacts.speed = gPhysics->mSpeed;
    if (theEffect != NULL) {
        theFacts.alpha = theEffect->mAlpha;
        theFacts.param9 = theEffect->mParam9;
        theFacts.param10 = theEffect->mParam10;
        theFacts.param11 = theEffect->mParam11;
    }
    theFacts.has_effect = theEffect != NULL;
    if (theEffect != NULL) {
        theFacts.time1 = (uint32_t)theEffect->mTime1;
        theFacts.time2 = (int32_t)theEffect->mTime2;
        theFacts.ends_with_unit = theEffect->mEndsWithUnitOrDuration;
        theFacts.emit_spread = theEffect->mEmitPositionSpread;
        theFacts.emit_flat = theEffect->mIsEmitFlat;
    }
    theFacts.world_speed = gWorld.GetGameSpeed();
    return theFacts;
}

long EEE3DRust::Call(EeGfxEffect3DOp inOp, EEEffectQueue* inQueues, const EeGfxEffectFile* inFile, long inBase,
                     unsigned long inMajorVersion, unsigned long inMinorVersion, void* inArg)
{
    Ask theAsk = {this, inQueues, NULL, inOp, inArg, NULL, NULL};
    EeGfxEffect3DHost theHost = {&theAsk, Ask::UnitPosition, Ask::ModelHeight, Ask::MoveTo, Ask::Line, Ask::BaseQueue,
                                 Ask::Push, Ask::Media, Ask::Model, Ask::Texture, Ask::Follow, Ask::ModelAt,
                                 Ask::ShotHit, Ask::BeamStart, Ask::BeamUnit, Ask::BeamHidden, Ask::PlaceLinked,
                                 Ask::ComplexPosition, Ask::UnitModelMedia, Ask::UnitScale, Ask::UnitLocation,
                                 Ask::UnitModelRadius, Ask::UnitFrame, Ask::UnitModelAt, Ask::OrbitalCount,
                                 Ask::Orbital, Ask::Animation, Ask::AnimationSound, Ask::SpawnCount, Ask::SpawnAt,
                                 Ask::Spawn, Ask::TextureAt, Ask::UnitTile, Ask::TerrainZ, Ask::UnitHidden,
                                 Ask::SpaceTile, Ask::CreateLinked, Ask::WallGatePosts, Ask::WallPiece,
                                 Ask::GateOpen, Ask::PlayerColor, Ask::VentLists, Ask::VentUnit, Ask::VentCount,
                                 Ask::VentOffsets, Ask::VentPoints, Ask::Frames, Ask::UnitClassScale,
                                 Ask::ReloadMedia, Ask::UnitRaisedPosition, Ask::MarketUnbuilt, Ask::QueueScreen,
                                 Ask::TracerEnds, Ask::TracerImpact, Ask::TracerHit, Ask::TracerSavedTarget,
                                 Ask::TracerTargetID, Ask::VolcanoBuild, Ask::VolcanoModel, Ask::FarmUnit,
                                 Ask::FarmTerrain, Ask::FarmDropSite, Ask::FarmSetVariant, Ask::FarmLoops,
                                 Ask::FarmDoppelgangerAny, Ask::FarmDoppelganger, Ask::FarmOpacity, Ask::SetScaledModel,
                                 Ask::GroundPoint, Ask::SmokeUnit, Ask::SmokeTagPoint, Ask::SmokePlace, kStormHost,
                                 Ask::RainMedia, Ask::RainSounds, Ask::ParticleTick, Ask::ParticleGround,
                                 Ask::EffectDuration, Ask::EmitterPlace, Ask::EmitterUnit, Ask::EmitterSound,
                                 Ask::EmitterLine, Ask::EffectQueue, Ask::MarchMedia, Ask::MarchEnds, Ask::MarchSetUnit,
                                 Ask::MarchUnitID};
    // Load and Save read no facts: during a load the unit is still its saved id (RepairPointers resolves it)
    if (inFile != NULL)
        return ee_gfx_effect3d_file(inOp, inBase, this->mRecord, inFile, inMajorVersion, inMinorVersion, &theHost);
    EeGfxGraphicFacts theFacts = EEEffectFacts(this->mGraphicEffect, this->mUnit);
    return ee_gfx_effect3d(inOp, this->mRecord, &theFacts, &theHost, &gEERustRaster);
}

// the base 3D effect, then the kind's own (ee_gfx); the first error, as each original
long EEE3DRust::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theBase = EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    if (this->mKind == 0x40)
        this->mTarget = (EEUnit*)-1;   // the tracer beam's: no target saved (RepairPointers)
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    return this->Call(EeGfxEffect3DOp::Load, NULL, &theFile, theBase, inMajorVersion, inMinorVersion);
}

// after loading: the base effect's pointers, then the kind's own (the media of the teleport and time warp); the tracer
// beam's saved target id back to a referenced unit, 7 when that unit is gone (EEE3DTracerBeam::RepairPointers, 0xfd000)
long EEE3DRust::RepairPointers()
{
    long theResult = EEEffect::RepairPointers();
    if (this->mKind == 0x40) {
        if (theResult == 0 && this->mTarget != (EEUnit*)-1) {
            this->mTarget = dynamic_cast<EEUnit*>(gWorld.GetUnit(USavedID(this->mTarget)));
            if (this->mTarget == NULL)
                theResult = 7;
            else
                this->mTarget->Reference();
        } else
            this->mTarget = NULL;
        return theResult;
    }
    if (this->mKind == 0x2d && theResult == 0) {   // the marching line's saved unit ids back to units (0xf3ee8)
        if (USavedID(this->mFromUnit) > 0)
            this->mFromUnit = static_cast<EEUnit*>(::gWorld.GetUnit(USavedID(this->mFromUnit)));
        if (USavedID(this->mToUnit) > 0)
            this->mToUnit = static_cast<EEUnit*>(::gWorld.GetUnit(USavedID(this->mToUnit)));
    }
    return theResult != 0 ? theResult : this->Call(EeGfxEffect3DOp::Repair);
}

void EEE3DRust::ReleaseResources()
{
    if (this->mKind == 0x39 && this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
    if (this->mKind == 0x2d)
        this->RemoveRegions();
    EEEffect3D::ReleaseResources();
}

void EEE3DRust::MoveToTransformation(const GETransformation& inTransformation)
{
    if (this->mKind != 0x2d) {
        EEEffect::MoveToTransformation(inTransformation);
        return;
    }
    GE3DPoint theLocation(inTransformation.GetX(), inTransformation.GetY(), inTransformation.GetZ());
    this->RemoveRegions();
    EEEffect::MoveToTransformation(inTransformation);
    this->AddRegions(theLocation, *reinterpret_cast<const GE3DPoint*>(&this->mRecord->march.to));
}

void EEE3DRust::MoveToPoint(const GE3DPoint& inLocation)
{
    if (this->mKind != 0x2d) {
        EEEffect::MoveToPoint(inLocation);
        return;
    }
    this->RemoveRegions();
    EEEffect::MoveToPoint(inLocation);
    this->AddRegions(inLocation, *reinterpret_cast<const GE3DPoint*>(&this->mRecord->march.to));
}

bool EEE3DRust::SetFrom(const GE3DPoint& inPoint)
{
    if (this->mFromUnit != NULL)
        return false;
    this->mRecord->march.from = EeGe3DPoint{inPoint.mX, inPoint.mY, inPoint.mZ};
    return true;
}

bool EEE3DRust::SetTo(const GE3DPoint& inPoint)
{
    if (this->mToUnit != NULL)
        return false;
    this->mRecord->march.to = EeGe3DPoint{inPoint.mX, inPoint.mY, inPoint.mZ};
    return true;
}

void EEE3DRust::AddRegions(const GE3DPoint& inFrom, const GE3DPoint& inTo)
{
    std::vector<U2DPoint<long> > theTiles;
    long theFromY = UMFloor(inFrom.GetY());
    long theFromX = UMFloor(inFrom.GetX());
    long theToY = UMFloor(inTo.GetY());
    long theToX = UMFloor(inTo.GetX());
    U2DPoint<long> theTile(this->mTileX, this->mTileY);
    RTSRegionTable<EEEffect*>* theTable = ::gWorld.mMap->mEffectRegions;
    U2DPoint<long> theStart(theFromX / theTable->mRegionSize, theFromY / theTable->mRegionSize);
    U2DPoint<long> theEnd(theToX / theTable->mRegionSize, theToY / theTable->mRegionSize);
    theStart.GetLineTiles(theEnd, 1.0f, theTiles);   // the step along the line in tiles (.rdata 0x440994)
    for (long i = theTiles.size() - 1; i >= 0; i--) {
        if (theTiles[i] == theStart || theTiles[i] == theEnd || theTiles[i] == theTile)
            continue;
        if (std::find(this->mRegions.begin(), this->mRegions.end(), theTiles[i]) != this->mRegions.end())
            continue;
        this->mRegions.push_back(theTiles[i]);
        EEEffect* theEffect = this;
        std::vector<EEEffect*>* theRegion = theTable->GetRegion(theTiles[i]);
        if (theRegion)
            theRegion->push_back(theEffect);
    }
}

void EEE3DRust::RemoveRegions()
{
    RTSRegionTable<EEEffect*>* theTable;
    if (::gWorld.mMap && (theTable = ::gWorld.mMap->mEffectRegions) != 0) {
        for (long i = this->mRegions.size() - 1; i >= 0; i--) {
            EEEffect* theEffect = this;
            RTSRegion<EEEffect*>* theRegion = theTable->GetRegion(this->mRegions[i]);
            if (theRegion)
                theRegion->Remove(theEffect);
        }
    }
    this->mRegions.clear();
}

// the base's, then the tracer beam's target unless the effect keeps its unit (EEE3DTracerBeam::ReplaceUnit, 0xfd10f)
void EEE3DRust::ReplaceUnit(EEUnit* inOldUnit, EEUnit* inNewUnit)
{
    if (this->mKind == 0x29)   // the farm's is empty (ICF-folded into 0x163d6f)
        return;
    EEEffect::ReplaceUnit(inOldUnit, inNewUnit);
    EEUnit** theEnds[2] = {&this->mFromUnit, &this->mToUnit};   // the marching line's ends (0xf4017)
    for (int i = 0; i < 2; i++) {
        if (this->mKind == 0x2d && inOldUnit == *theEnds[i]) {
            (*theEnds[i])->Dereference();
            *theEnds[i] = inNewUnit;
            inNewUnit->Reference();
        }
    }
    if (this->mKind != 0x40 || this->mTarget != inOldUnit || this->mKeepsUnit)
        return;
    this->mTarget->Dereference();
    this->mTarget = inNewUnit;
    this->mTarget->Reference();
}

// the icon (2) and goal (0x3c): a fixed one moves to inLocation (EEE3DIcon::SetLocation, 0xf2637); others ignore it
void EEE3DRust::SetLocation(const GE3DPoint& inLocation)
{
    if ((this->mKind == 2 || this->mKind == 0x3c) && !this->mRecord->icon.follow_unit)
        this->mRecord->icon.location = EeGe3DPoint{inLocation.mX, inLocation.mY, inLocation.mZ};
}

// the energy wall (0x34): the current player sees the tile or holds a doppelganger of the unit there (then the wall
// is marked seen: its texture frame restarts) (EEE3DEnergyWall::IsTileVisible, 0xed98d); the others: the base's
bool EEE3DRust::IsTileVisible(EEMapTile* inTile)
{
    if (this->mKind == 0x29) {   // the farm (EEE3DFarm::IsTileVisible, 0xf0b1c)
        bool theFarmSeen = false;
        if (gWorld.mMap) {
            if (this->mUnit->mHitPoints > 0) {
                unsigned long theSize = this->mUnit->GetClass()->GetTileSize();
                for (unsigned long y = 0; y < theSize && !theFarmSeen; y++) {
                    for (unsigned long x = 0; x < theSize && !theFarmSeen; x++) {
                        EEMapTile* theTile = gWorld.mMap->GetTile(this->mUnit->GetTileX() + x, this->mUnit->GetTileY() + y);
                        if (theTile)
                            theFarmSeen = EEEffect3D::IsTileVisible(theTile);
                    }
                }
            }
            if (!theFarmSeen)
                theFarmSeen = gDoppelgangerManager.HasDoppelganger(inTile, this->mUnit->mComplexUnit, gWorld.mCurrentPlayer);
        }
        return theFarmSeen;
    }
    bool theResult = inTile->IsSeenBy(gWorld.GetCurrentPlayer());
    if (theResult || this->mKind != 0x34)
        return theResult;
    long thePlayer = gWorld.mCurrentPlayer;
    EEUnit* theUnit = this->mUnit;
    theResult = gDoppelgangerManager.HasDoppelganger(inTile, theUnit, thePlayer);
    if (theResult)
        this->mRecord->wall.seen = true;
    return theResult;
}

// the farm (0x29): rebuilt on its unit when the changed terrain rectangle overlaps its field (EEE3DFarm::ElevationChanged,
// 0xf128a); the others: the base's (nothing)
void EEE3DRust::ElevationChanged(long inLeft, long inBottom, long inRight, long inTop)
{
    if (this->mKind != 0x29)
        return;
    GERectangle theRectangle(inLeft, inTop, inRight, inBottom);
    const EeGfxVolcanoArea& theArea = this->mRecord->farm.area;
    if (!(theRectangle.mLeft <= theArea.right) || !(theRectangle.mRight >= theArea.left) ||
        !(theRectangle.mTop <= theArea.bottom) || !(theRectangle.mBottom >= theArea.top))
        return;
    this->Call(EeGfxEffect3DOp::Rebuild);
}

long EEE3DRust::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theBase = EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    return this->Call(EeGfxEffect3DOp::Save, NULL, &theFile, theBase, inMajorVersion, inMinorVersion);
}
