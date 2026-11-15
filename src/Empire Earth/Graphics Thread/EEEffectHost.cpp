// The effects' constructors and their answers about the world (docs/RUST_PLAN.md R4.4): EEEffect's constructors by
// the manager's entry points, the operations that read the world (the storms' moves, the marching line's regions, the
// tile tests, the farm's rebuild, the 2D parts, the damage controller's step) and one host per class for the Rust
// calls: Ask answers EeGfxEffect3DHost (ee_gfx effect_rising.rs and its kinds' modules) and EeGfxEffect2DHost
// (effect_2d.rs), their shared questions once; the battlefield decals' placement (EEE2DBattlefieldAmbient.cpp,
// 0xde374..0xde6e9) and EETagPointList::Scale (0xe1814). Since R4.4's twenty-fifth round one file for what was
// EEE3DRust.cpp and EEE2DRust.cpp (the original's 60 effect leaves, deleted round by round).
#include <windows.h>
#include <math.h>

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
#include "ee/EEEffectManager.h"
#include "ee/EECombatFormulas.h"
#include "ee/EEUnitClassKinds.h"
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
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEAEManager.h"
#include "ee/EEEffectKindIDs.h"
#include "ee/EEE2DBattlefieldAmbient.h"
#include "rust/ee_combat.h"

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
void EESetRainSoundLevel(EEEffect* inRain, SESMInstanceNode* inSound, float inVolume)
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

// a unit's tile's centre on the terrain (EEE2DLightGlow::SetUp 0xe2067 and PlaceGlows 0xe266f without a complex unit)
GE3DPoint EETileCentre(EEUnit* inUnit)
{
    GE3DPoint thePosition;
    thePosition.SetX(inUnit->mUnitClass->mTileSize * 0.5f + inUnit->mTileX);
    thePosition.SetY(inUnit->mUnitClass->mTileSize * 0.5f + inUnit->mTileY);
    thePosition.SetZ(gWorld.GetMap()->mTerrainMesh.GetMeshZ(thePosition.GetX(), thePosition.GetY()));
    return thePosition;
}


// the world's answers for one call
struct Ask {
    EEEffect* mEffect = NULL;
    EEEffectQueue* mQueues = NULL;
    EETagPointList* mSpawn = NULL;   // the hurricane's spawn list, read once by SpawnCount for SpawnAt
    EeGfxEffect3DOp mOp = EeGfxEffect3DOp::Create;
    void* mArg = NULL;               // a constructor's unit (Create), point (CreateAt) or line (CreateLine)
    EETagPointList* mVentFrom = NULL;   // the damage vent's tag point lists, read once a step by VentLists
    EETagPointList* mVentTo = NULL;
    EETagPointList* mSmokeTags = NULL;  // the damage smoke's tag points, read once a step by SmokeUnit for SmokeTagPoint
    unsigned long mSmokeFrame = 0;
    GETransformation mSmokeLocation;
    // the light glow's: the unit's light tags, its frame and its scaled location while the call places the glows
    EETagPointList* mTags = NULL;
    unsigned long mFrame = 0;
    GETransformation mLocation;
    static EEEffect* E(void* inAsk) { return static_cast<Ask*>(inAsk)->mEffect; }
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
    static int32_t BaseQueue(void* inAsk) { return E(inAsk)->BaseQueue(true); }
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
        EEEffect* theEffect = E(inAsk);
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
        EEEffect* theEffect = E(inAsk);
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
        EEEffect* theEffect = E(inAsk);
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
        EEEffect* theEffect = E(inAsk);
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
        EEUnitClass* theGate = EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theUnit->GetClass());
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
                || (theType == kUnitTypeGateSpace && theOther->GetClass()->IsSidePart(theOther))) {
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
        EEEffect* theEffect = E(inAsk);
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
        EEEffect* theEffect = E(inAsk);
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
        EEEffect* theEffect = E(inAsk);
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
        EEEffect* theEffect = E(inAsk);
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
        EEEffect* theEffect = theAsk->mEffect;
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
        EEEffect* theRain = E(inAsk);
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
        EEEffect* theEffect = E(inAsk);
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
        EEEffect* theEffect = E(inAsk);
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
        EEEffect* theEffect = E(inAsk);
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
    static int32_t EffectQueue(void* inAsk) { return E(inAsk)->BaseQueue(false); }
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
    // the 2D kinds' answers
    // the base's queue: EEEffect::Queue for the confirmation (not EEEffect2D's projection), else EEEffect2D's
    static int32_t BaseQueue2D(void* inAsk) { return E(inAsk)->BaseQueue(E(inAsk)->mKind != 0x41); }
    // the graphic effect's own object (not GetGraphicObject: EEE3DCommandConfirmation::SetUp, 0xe9eb5)
    static int32_t OwnMedia(void* inAsk)
    {
        E(inAsk)->mGraphicObject = E(inAsk)->mGraphicEffect->mGraphicObject;
        return E(inAsk)->mGraphicObject->LoadMedia();
    }
    static void MoveToPoint(void* inAsk)
    {
        GE3DPoint thePosition(E(inAsk)->mPosition);
        E(inAsk)->MoveToPoint(thePosition);
    }
    static void* PlaneModel(void* inAsk) { return E(inAsk)->mModel; }
    static float SelectionSize(void* inAsk) { return E(inAsk)->mUnit->mUnitClass->mSelectionSize; }
    // an attack's target belongs to another player, not player 0, the current player's enemy
    static bool ConfirmEnemy(void* inAsk)
    {
        EEPlayer* thePlayer = E(inAsk)->mUnit->mPlayer;
        EEPlayer* theCurrent = gWorld.GetCurrentPlayer();
        return thePlayer != theCurrent && theCurrent->mEnemies[thePlayer->mPlayerID] != 0 && thePlayer->mPlayerID != 0;
    }
    // the goal a state-0 confirmation shows: the last goal of the unit's goal queue as a location goal (on the tile
    // at the effect: blocked when the unit cannot stand there and its player sees it; forced; a move), else whether
    // its active goal unloads a transport (EEE3DCommandConfirmation::SetUp)
    static EeGfxConfirmGoal ConfirmGoal(void* inAsk, uint32_t inGoalIndex, int32_t inTileX, int32_t inTileY)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        if (!theUnit || !theUnit->mComplexUnit)
            return EeGfxConfirmGoal::NoUnit;
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        EEUGRust* theGoal = theComplex->mGoalQueues[inGoalIndex].mGoals.size() == 0
                                ? NULL : EELocationGoalCast(theComplex->mGoalQueues[inGoalIndex].mGoals.back());
        if (theGoal) {
            EETileCoordinate theTile(inTileX, inTileY);
            EEMapTile* theMapTile = gWorld.mMap->GetTile(theTile.mX, theTile.mY);
            if (EECanStandOn(theTile, theComplex) == false && theMapTile && theMapTile->IsVisibleTo(theUnit->mPlayer))
                return EeGfxConfirmGoal::Blocked;
            return theGoal->GetIsForcedMove() ? EeGfxConfirmGoal::Forced : EeGfxConfirmGoal::Move;
        }
        EEUnitGoal* theLoadGoal = theComplex->mActiveGoal;
        return theLoadGoal && theLoadGoal->mKind == kGoalUnloadTransport ? EeGfxConfirmGoal::Unloading
                                                                          : EeGfxConfirmGoal::Other;
    }
    // where the unit stands for its ring: its position; a tree's id, class tile size and tile; a unit without hit
    // points with a complex unit and a model: its model's top point and scale (EEE3DCommandConfirmation::Step,
    // 0xea47e)
    static EeGfxConfirmUnit ConfirmUnit(void* inAsk)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        EeGfxConfirmUnit theFacts = {};
        GE3DPoint theLocation;
        theUnit->GetPosition(theLocation);
        theFacts.position = EeGe3DPoint{theLocation.mX, theLocation.mY, theLocation.mZ};
        if (theUnit->mUnitClass->mIsTree) {
            theFacts.tree = true;
            theFacts.id = (int32_t)(long)theUnit->mUnitID;
            theFacts.tile_size = (int32_t)theUnit->mUnitClass->mTileSize;
            theFacts.tile_x = (int32_t)(long)theUnit->mTileX;
            theFacts.tile_y = (int32_t)(long)theUnit->mTileY;
        } else if (theUnit->mHitPoints <= 0 && theUnit->mComplexUnit && theUnit->GetModel()->mModel) {
            theFacts.dead_top = true;
            const GE3DPoint& theTop = theUnit->GetModel()->mTopPoint;
            theFacts.top = EeGe3DPoint{theTop.mX, theTop.mY, theTop.mZ};
            theFacts.scale = theUnit->mComplexUnit->mPhysicalModel.GetScale();
        }
        return theFacts;
    }
    static void* CommandTexture(void*, uint32_t inIndex)
    {
        return ee_gfx_ro_units_command_texture(inIndex);   // ee_gfx ro_units.rs (0 unit, 1 path, 2 click)
    }
    // a random one of the unit's tag points of inKind (the unit itself without any) (EEE2DShockwave::SetUp, 0xe6259)
    static EeGe3DPoint SpawnPoint(void* inAsk, uint32_t inKind)
    {
        EEEffect* theEffect = E(inAsk);
        GE3DPoint thePosition;
        GETransformation theTransformation(false);
        theTransformation = theEffect->mUnit->mComplexUnit->mPhysicalModel.GetLocation();
        EEComplexUnit* theUnit = theEffect->mUnit->mComplexUnit;
        EETagPointList* theList = theUnit->GetModel()->GetTagPoints(theUnit->GetAnimation(), (long)inKind);
        float theScale = theUnit->mPhysicalModel.GetScale();
        unsigned long theFrame = theUnit->mPhysicalModel.GetFrame();
        if (theList && theList->mPoints.size() != 0) {
            long theIndex = UMFloatToLong(EEEffect::GetRandom() * theList->mPoints.size());
            if (theIndex == theList->mPoints.size())
                theIndex = theList->mPoints.size() - 1;
            EETagPointList::Scale(theTransformation, theScale);
            theList->mPoints[theIndex]->GetPoint(theFrame, theTransformation, thePosition);
        } else {
            // no tag points of the kind: at the unit itself, not at the map origin
            thePosition = GE3DPoint(theTransformation);
        }
        return EeGe3DPoint{thePosition.mX, thePosition.mY, thePosition.mZ};
    }
    // the unit's tag points of inKind at its animation: -1 without a complex unit (EEE2DImpact::PickTagPoint, 0xe158e)
    static int32_t TagCount(void* inAsk, uint32_t inKind)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        EEComplexUnit* theComplex = theUnit ? theUnit->mComplexUnit : NULL;
        if (!theComplex)
            return -1;
        EETagPointList* theList = theComplex->GetModel()->GetTagPoints(theComplex->GetAnimation(), (long)inKind);
        return theList ? (int32_t)theList->mPoints.size() : 0;
    }
    // the tag point inIndex of inKind on the unit's current frame (EEE2DImpact::UpdatePosition, 0xe15e3: the list is
    // not null-checked)
    static EeGfx2DPoint TagPoint(void* inAsk, uint32_t inKind, uint32_t inIndex)
    {
        EeGfx2DPoint thePoint = {};
        EEUnit* theUnit = E(inAsk)->mUnit;
        EEComplexUnit* theComplex = theUnit ? theUnit->mComplexUnit : NULL;
        if (!theComplex)
            return thePoint;
        unsigned long theFrame = theComplex->mPhysicalModel.GetFrame();
        float theScale = theComplex->mPhysicalModel.GetScale();
        GETransformation theTransformation(theComplex->mPhysicalModel.GetLocation());
        EETagPointList* theList = theComplex->GetModel()->GetTagPoints(theComplex->GetAnimation(), (long)inKind);
        EETagPointList::Scale(theTransformation, theScale);
        GE3DPoint thePosition;
        theList->mPoints[inIndex]->GetPoint(theFrame, theTransformation, thePosition);
        thePoint.has = true;
        thePoint.point = EeGe3DPoint{thePosition.mX, thePosition.mY, thePosition.mZ};
        return thePoint;
    }
    // the unit's weapon tag point (ee_combat's weapon_point; EEE2DMuzzleFlash::SetUp 0xe458e: site 3, the list
    // assumed; sites 1 and 2 (the 2D projectile) test it)
    static EeGe3DPoint WeaponPoint(void* inAsk, int32_t inSite)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        EEModel* theModel = theUnit->GetModel();
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        EETagPointList* theList = theUnit->GetModel()->mTagPoints[theComplex->mAnimation];
        bool theHasList = inSite <= 2 ? theList && theList->mPoints.size() : true;
        GE3DPoint theStart;
        EECombatEffectWeaponPoint(inSite, theComplex, theModel, theList, theHasList, theStart);
        return EeGe3DPoint{theStart.mX, theStart.mY, theStart.mZ};
    }
    // the kept object's animation sound for the frame at the location, kept as the effect's (EEE2DImpact::Step and
    // SetSound, 0xe166b / 0xe17ed)
    static void PartSound(void* inAsk, EeGe3DPoint inLocation, uint32_t inFrame, bool inChanged, bool inPart0)
    {
        EEEffect* theEffect = E(inAsk);
        GE3DPoint theLocation(inLocation.x, inLocation.y, inLocation.z);
        EEGraphicObject* theObject = inPart0 ? theEffect->mParts[0].mGraphicObject : theEffect->mGraphicObject;
        SESMInstanceNode* theSound = PlayAnimationSound(theLocation, theObject, 0, theEffect->mSound, inFrame, inChanged);
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
    static bool DrawBlood(void*) { return gDrawBlood; }
    static bool HasComplex(void* inAsk) { return E(inAsk)->mUnit != NULL && E(inAsk)->mUnit->mComplexUnit != NULL; }
    static EeGe3DPoint DefaultPoint(void* inAsk)
    {
        const GE3DPoint& thePoint = E(inAsk)->mUnit->GetModel()->mDefaultPoint;
        return EeGe3DPoint{thePoint.mX, thePoint.mY, thePoint.mZ};
    }
    static float StatsScale(void* inAsk) { return E(inAsk)->mUnit->mStats->mScale; }
    static bool TileBlocked(void*, int32_t inX, int32_t inY) { return gWorld.IsTileBlocked(inX, inY); }
    static EeGe3DPoint Wind(void*)
    {
        const GEVector& theWind = gWorld.mWindDirection;
        return EeGe3DPoint{theWind.mX, theWind.mY, theWind.mZ};
    }
    // the parts (EEEffect2D::mParts): made by CreateParts (0x104623), read and written one at a time
    static EeGfx2DPlace CreateParts(void* inAsk, float inLift)
    {
        GE3DPoint thePosition;
        float theHeight;
        E(inAsk)->CreateParts(thePosition, theHeight, inLift);
        return EeGfx2DPlace{EeGe3DPoint{thePosition.mX, thePosition.mY, thePosition.mZ}, theHeight};
    }
    static uint32_t PartCount(void* inAsk) { return (uint32_t)E(inAsk)->mParts.size(); }
    static EeGfx2DPart Part(void* inAsk, uint32_t inIndex)
    {
        const EEEffect2DPart& thePart = E(inAsk)->mParts[inIndex];
        return EeGfx2DPart{EeGe3DPoint{thePart.mPosition.mX, thePart.mPosition.mY, thePart.mPosition.mZ}, (uint32_t)thePart.mTag,
                           (uint32_t)thePart.mFrame, (uint32_t)thePart.mDrawFrame, thePart.mTime, thePart.mIsDone};
    }
    static void SetPart(void* inAsk, uint32_t inIndex, const EeGfx2DPart* inPart)
    {
        EEEffect2DPart& thePart = E(inAsk)->mParts[inIndex];
        thePart.mPosition = GE3DPoint(inPart->position.x, inPart->position.y, inPart->position.z);
        thePart.mTag = inPart->tag;
        thePart.mFrame = inPart->frame;
        thePart.mDrawFrame = inPart->draw_frame;
        thePart.mTime = inPart->time;
        thePart.mIsDone = inPart->done;
    }
    static EeGfxFrames PartFrames(void* inAsk, uint32_t inIndex)
    {
        EEGraphicObject* theObject = E(inAsk)->mParts[inIndex].mGraphicObject;
        return EeGfxFrames{(uint32_t)theObject->mStartFrames[0], (uint32_t)theObject->mFrameTimes[0],
                           (uint32_t)theObject->mFrameCounts[0], (uint32_t)theObject->mLastFrames[0]};
    }
    static void* PartTexture(void* inAsk, uint32_t inIndex) { return E(inAsk)->mParts[inIndex].mGraphicObject->mTextures[0]; }
    static void PartsSnapshot(void* inAsk)
    {
        std::vector<EEEffect2DPart>& theParts = E(inAsk)->mParts;
        for (unsigned long i = 0; i < theParts.size(); i++)
            theParts[i].mDrawFrame = theParts[i].mFrame;
    }
    // the part's tag point on the unit's model at its frame (the unit itself for tag 10000) under the unit's location
    // (EEE2DFire::UpdatePart, 0xe0a22)
    static EeGfx2DPoint PartFollow(void* inAsk, uint32_t inIndex)
    {
        EeGfx2DPoint thePoint = {};
        EEEffect* theEffect = E(inAsk);
        EEComplexUnit* theUnit = theEffect->mUnit->GetComplexUnit();
        if (theUnit == NULL)
            return thePoint;
        GE3DPoint theOffset;
        if (theEffect->mParts[inIndex].mTag != 10000) {
            GEModel* theModel = theUnit->GetModel()->mModel;
            unsigned long theModelFrame = theUnit->mPhysicalModel.GetFrame();
            theOffset = theModel->GetTagPoint(theEffect->mParts[inIndex].mTag, theModelFrame);
        }
        GE3DPoint thePosition;
        theUnit->GetPhysicalModel().GetLocation().Apply(theOffset, thePosition);
        thePoint.has = true;
        thePoint.point = EeGe3DPoint{thePosition.mX, thePosition.mY, thePosition.mZ};
        return thePoint;
    }
    static int32_t MaxHitPoints(void* inAsk) { return (int32_t)E(inAsk)->mUnit->mStats->mMaxHitPoints; }
    static EeGe3DPoint UnitPoint(void* inAsk)
    {
        const GE3DPoint& thePoint = E(inAsk)->GetUnitLocation();
        return EeGe3DPoint{thePoint.mX, thePoint.mY, thePoint.mZ};
    }
    // the vortex's: the kept object's own model and orbitals (EEE2DVortex::DrawPass5 0xe7949, Load 0xe7b50)
    // the marching icons' (EEE2DMarchingIcons::IsIconShown 0xe3cdf, Virtual5C 0xe3d8e, Step 0xe41ef, DrawPass6 0xe3e92)
    static EeGfxIconUnit IconUnit(void* inAsk)
    {
        EeGfxIconUnit theFacts = {};
        EEUnit* theUnit = E(inAsk)->mUnit;
        if (!theUnit)
            return theFacts;
        theFacts.has_unit = true;
        theFacts.land_building = theUnit->mUnitClass->IsLandBuilding();
        theFacts.tile_size = theUnit->mUnitClass->mTileSize;
        if (theUnit->mPlayer) {
            theFacts.has_player = true;
            theFacts.player = (uint32_t)theUnit->mPlayer->mPlayerID;
            theFacts.x = theUnit->mComplexUnit->mPhysicalModel.GetLocation().GetX();
            theFacts.y = theUnit->mComplexUnit->mPhysicalModel.GetLocation().GetY();
        }
        return theFacts;
    }
    static bool AreaEffect(void* inAsk, int32_t inKind, float inX, float inY, uint32_t inPlayer)
    {
        return gAEManager.HasEffect(inKind, inX, inY, inPlayer, false, &E(inAsk)->mUnit->mComplexUnit->mAreaEffects);
    }
    static int32_t AreaEffects(void* inAsk) { return (int32_t)E(inAsk)->mUnit->mComplexUnit->mNumAreaEffects; }
    static void SetAreaEffects(void* inAsk, int32_t inCount)
    {
        EEComplexUnit* theUnit = E(inAsk)->mUnit->mComplexUnit;
        theUnit->mNumAreaEffects = inCount;
        if (inCount == 0)
            theUnit->FnE123D();
    }
    // the unit's type is in its player's active group; a town centre or capitol also with 0x23 or 0x29 there
    static bool IconsSelected(void* inAsk)
    {
        EEComplexUnit* theUnit = E(inAsk)->mUnit->mComplexUnit;
        unsigned long theType = theUnit->mUnitClass->mUnitType;
        EESelectedGroup* theGroup = theUnit->mPlayer->mActiveGroup;
        bool theSelected = theGroup ? theGroup->HasUnitType(theType) : false;
        if (theGroup != NULL && (theType == kUnitTypeTownCenter || theType == kUnitTypeCapitol)) {
            if (theGroup->HasUnitType(0x23) || theGroup->HasUnitType(0x29))
                theSelected = true;
        }
        return theSelected;
    }
    static bool IconsOwner(void* inAsk)
    {
        EEComplexUnit* theUnit = E(inAsk)->mUnit->mComplexUnit;
        return theUnit->mPlayer == gWorld.GetCurrentPlayer() && theUnit->mIsBuilt;
    }
    static uint32_t GhostUnits(void*) { return (uint32_t)gWorld.GetNumGhostUnits(); }
    static EeGfx2DPoint GroundAt(void*, EeGe3DPoint inAt)
    {
        EeGfx2DPoint thePoint = {};
        EEMap* theMap = gWorld.mMap;
        GE3DPoint theLocation(inAt.x, inAt.y, inAt.z);
        GE3DPoint theGround;
        if (!theMap->GetTile(UMFloor(theLocation.GetX()), UMFloor(theLocation.GetY())))
            return thePoint;
        theMap->GetGroundPoint(theLocation, theGround);
        thePoint.has = true;
        thePoint.point = EeGe3DPoint{theGround.mX, theGround.mY, theGround.mZ};
        return thePoint;
    }
    // the light glow's (EEE2DLightGlow::SetUp 0xe2067, Step 0xe25dc, PlaceGlows 0xe266f): the unit's light tags at its
    // animation (the first without a complex unit), kept for their names and points
    static EeGfxGlowSetUp GlowSetUp(void* inAsk)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        EETagPointList* theTags;
        GE3DPoint thePosition;
        if (theComplex != NULL) {
            theTags = theUnit->GetModel()->GetTagPoints(theComplex->GetAnimation(), 0xd);
            const GETransformation& theLocation = theComplex->mPhysicalModel.GetLocation();
            thePosition = GE3DPoint(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
        } else {
            theTags = theUnit->GetModel()->GetTagPoints(0, 0xd);
            thePosition = EETileCentre(theUnit);
        }
        static_cast<Ask*>(inAsk)->mTags = theTags;
        EeGfxGlowSetUp theSetUp = {theTags != NULL ? (uint32_t)theTags->GetNumTagPoints() : 0u,
                                   EeGe3DPoint{thePosition.mX, thePosition.mY, thePosition.mZ}};
        return theSetUp;
    }
    static uint32_t GlowTagName(void* inAsk, uint32_t inIndex, uint8_t* outName, uint32_t inCapacity)
    {
        EETagPoint* thePoint = static_cast<Ask*>(inAsk)->mTags->mPoints[inIndex];
        UString theName = thePoint->mModel->GetTagPointName(thePoint->mIndex);
        uint32_t theLength = (uint32_t)theName.GetLength();
        memcpy(outName, (const char*)theName, theLength < inCapacity ? theLength : inCapacity);
        return theLength;
    }
    // the placing's unit: hidden for another player's cloaked unit or an unfinished land building
    static EeGfxGlowPlace GlowPlace(void* inAsk)
    {
        Ask* theAsk = static_cast<Ask*>(inAsk);
        EEUnit* theUnit = E(inAsk)->mUnit;
        EeGfxGlowPlace thePlace = {true, false, {}};
        EETagPointList* theTags;
        float theScale;
        GE3DPoint thePosition;
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        if (theComplex != NULL) {
            bool theCloaked = theComplex->mIsCloakable ? theComplex->mIsCloaked : false;
            if (theCloaked && gWorld.GetCurrentPlayer() != theComplex->mPlayer)
                thePlace.show = false;
            if (theComplex->mUnitClass->IsLandBuilding() && !theComplex->mIsBuilt)
                thePlace.show = false;
            theTags = theUnit->GetModel()->GetTagPoints(theComplex->GetAnimation(), 0xd);
            theScale = theComplex->mPhysicalModel.GetScale();
            theAsk->mFrame = theComplex->mPhysicalModel.GetFrame();
            theAsk->mLocation = theComplex->mPhysicalModel.GetLocation();
            thePosition = GE3DPoint(theAsk->mLocation.GetX(), theAsk->mLocation.GetY(), theAsk->mLocation.GetZ());
        } else {
            EEUnitClassUpgrade* theStats = theUnit->mStats;
            theTags = theUnit->GetModel()->GetTagPoints(0, 0xd);
            theScale = theStats->mScale;
            theAsk->mFrame = 0;
            thePosition = EETileCentre(theUnit);
            theAsk->mLocation.MakeIdentity();
            theAsk->mLocation.SetX(thePosition.GetX());
            theAsk->mLocation.SetY(thePosition.GetY());
            theAsk->mLocation.SetZ(thePosition.GetZ());
        }
        if (theTags != NULL)
            EETagPointList::Scale(theAsk->mLocation, theScale);
        theAsk->mTags = theTags;
        thePlace.has_tags = theTags != NULL;
        thePlace.position = EeGe3DPoint{thePosition.mX, thePosition.mY, thePosition.mZ};
        return thePlace;
    }
    static EeGe3DPoint GlowPoint(void* inAsk, uint32_t inIndex)
    {
        Ask* theAsk = static_cast<Ask*>(inAsk);
        GE3DPoint thePoint;
        theAsk->mTags->mPoints[inIndex]->GetPoint(theAsk->mFrame, theAsk->mLocation, thePoint);
        return EeGe3DPoint{thePoint.mX, thePoint.mY, thePoint.mZ};
    }
    static EeGfxGlowArea GlowArea(void* inAsk)
    {
        GETerrainMesh& theMesh = gWorld.GetMap()->mTerrainMesh;
        unsigned long theX, theY, theWidth, theHeight;
        theMesh.GetTerrainVisibleBottomLeftIndices(theX, theY);
        theMesh.GetTerrainVisibleTileDimensions(theWidth, theHeight);
        EEUnit* theUnit = E(inAsk)->mUnit;
        return EeGfxGlowArea{(uint32_t)theX, (uint32_t)theY, (uint32_t)theWidth, (uint32_t)theHeight,
                             (int32_t)theUnit->mTileX, (int32_t)theUnit->mTileY};
    }
    static uint32_t GlowRows(void*) { return (uint32_t)gWorld.GetMap()->mTerrainMesh.GetNumVisibleTileRows(); }
    // the particle's (EEE2DParticle::Update 0xe4c31, Step 0xe520d)
    static EeGfxParticleTick PositionTick(void* inAsk) { return EEParticleTick(E(inAsk)->mGraphicEffect, E(inAsk)->mPosition); }
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

// the unit's mouth through the transformation, with the offsets of the cavalry archer, chariot archer, bronze spear
// cavalry and knight (EEE2DWinterBreath::GetMouthPoint)
GE3DPoint EEMouthPoint(EEUnit* inUnit, const GETransformation& inTransformation)
{
    GE3DPoint theOffset(0.0f, 0.4f, 0.405f);
    GE3DPoint theResult;
    switch (inUnit->mUnitClass->mUnitType) {
    case kUnitTypeArch06CavalryArcher:
        theOffset.SetY(0.465f);
        theOffset.SetZ(0.385f);
        break;
    case kUnitTypeArch04ChariotArcher:
        theOffset.SetY(0.872f);
        theOffset.SetZ(0.465f);
        break;
    case kUnitTypeCav04BronzeSpearCavalry:
    case kUnitTypeCav06Knight:
        theOffset.SetY(0.272f);
        theOffset.SetZ(0.235f);
        break;
    }
    inTransformation.Apply(theOffset, theResult);
    return theResult;
}

}

// on a unit: the destruction smoke (0x1e) at its location, spread over half its class's scale (0x4ebbfd); the 2D
// particle (0x2e) at it or its tile's centre on the ground (0 off the map) (0xe489b); the multi-effect's own unit is
// none (its linked effects are made on inUnit); the light glow keeps its unit when asked (0xe1a7a), the command
// confirmation shows goal queue inGoalIndex (0xe9d1e), the emitter follows tag point inTagPoint
EEEffect::EEEffect(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind, bool inKeepsUnit, long inTagPoint,
                   unsigned long inGoalIndex)
{
    if (inKind == 0x1e) {
        const GETransformation& theLocation = inUnit->mComplexUnit->mPhysicalModel.GetLocation();
        this->Attach(NULL, inGraphicEffect, inKind, false);
        this->PlaceAt(GE3DPoint(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ()));
        this->mRecord->smoke.radius = inUnit->mUnitClass->mScale * 0.5f;
        return;
    }
    if (inKind == 0x2e) {
        this->Attach(inUnit, inGraphicEffect, inKind, false);
        GE3DPoint theLocation;
        EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
        if (theComplexUnit) {
            const GETransformation& theTransformation = theComplexUnit->mPhysicalModel.GetLocation();
            theLocation = GE3DPoint(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
        } else {
            theLocation.SetX(inUnit->mUnitClass->GetTileSize() * 0.5f + inUnit->mTileX);
            theLocation.SetY(inUnit->mUnitClass->GetTileSize() * 0.5f + inUnit->mTileY);
            if (gWorld.GetMap()->mTerrainMesh.GetIsValidTile(inUnit->mTileX, inUnit->mTileY))
                theLocation.SetZ(gWorld.GetMap()->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()));
        }
        this->StartParticle(theLocation, NULL);
        return;
    }
    this->Attach(inKind == 0x3b ? NULL : inUnit, inGraphicEffect, inKind, inKind == 0xd && inKeepsUnit);
    if (inKind == 0x41)
        this->mRecord->confirm.goal_index = (uint32_t)inGoalIndex;
    this->Call(EeGfxEffect3DOp::Create, NULL, NULL, 0, 0, 0, inUnit);
    if (inKind == 0x3a)
        this->mRecord->emitter.tag_point = (uint32_t)inTagPoint;
}

EEEffect::EEEffect(const GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect, long inKind)
{
    this->Attach(NULL, inGraphicEffect, inKind, false);
    this->PlaceAt(inLocation);
}

// at a point: the 2D particle's start (0xe49c7); the battlefield decal's size and corners on the terrain (hidden and
// over off the map or not level, 0xde74f); the others' point for Rust (a 2D kind's sprite.start, a 3D kind's
// aged.location)
void EEEffect::PlaceAt(const GE3DPoint& inLocation)
{
    if (this->mKind == 0x2e) {
        this->StartParticle(inLocation, NULL);
        return;
    }
    EEGraphicEffect* theEffect = this->mGraphicEffect;
    if (this->mKind == 0x32) {
        EeGfxAmbientNew theNew = {theEffect->GetStartSize(), theEffect->GetSize(), (int32_t)theEffect->mIntParam1,
                                  theEffect->mFlag1};
        EeGfxParticleGround theGround = EEParticleGround();
        ee_gfx_effect_ambient_new(this->mRecord, reinterpret_cast<const EeGe3DPoint*>(&inLocation), &theNew, &theGround);
        return;
    }
    (this->mIs2D ? this->mRecord->sprite.start : this->mRecord->aged.location) =
        EeGe3DPoint{inLocation.mX, inLocation.mY, inLocation.mZ};
    this->Call(EeGfxEffect3DOp::CreateAt, NULL, NULL, 0, 0, 0, const_cast<GE3DPoint*>(&inLocation));
}

// from a unit to a point: the command confirmation there (0xe9d91); the smoke muzzle blast and the 2D projectile from
// the unit's weapon point (0xe6a07, 0xe31dd); the pulse cannon ball from its weapon point (0xf6a26), the citizen beam
// towards it (0xe94b7), the tracer beam to it (0xfc49d)
EEEffect::EEEffect(EEUnit* inUnit, const GE3DPoint& inTarget, EEGraphicEffect* inGraphicEffect, long inKind,
                   unsigned long inGoalIndex)
{
    EeGe3DPoint theTarget = {inTarget.mX, inTarget.mY, inTarget.mZ};
    if (inKind == 0x14 || inKind == 0x30) {
        this->Attach(NULL, inGraphicEffect, inKind, false);
        GE3DPoint theStart;
        EEModel* theModel = inUnit->GetModel();
        EEComplexUnit* theUnit = inUnit->mComplexUnit;
        EETagPointList* theList = inUnit->GetModel()->mTagPoints[theUnit->mAnimation];
        if (inKind == 0x30)
            EECombatEffectWeaponPoint(2, theUnit, theModel, theList, theList && theList->mPoints.size(), theStart);
        else
            EECombatEffectWeaponPoint(5, theUnit, theModel, theList, true, theStart);
        this->mRecord->sprite.start = EeGe3DPoint{theStart.mX, theStart.mY, theStart.mZ};
        this->mRecord->sprite.draw_position = theTarget;
        this->Call(EeGfxEffect3DOp::CreateBetween);
        return;
    }
    this->Attach(inUnit, inGraphicEffect, inKind, false);
    if (inKind == 0x41) {
        this->mPosition = inTarget;
        this->mRecord->confirm.goal_index = (uint32_t)inGoalIndex;
        this->Call(EeGfxEffect3DOp::CreateAt);
        return;
    }
    if (inKind == 0x40) {
        this->mRecord->tracer.target_point = theTarget;
        this->mRecord->tracer.weapon_tag_point = inUnit->mComplexUnit->mWeaponTagPoint;
    } else if (inKind == 0x33) {
        this->mRecord->shot.end = theTarget;
    } else {
        ShotStart(6, inUnit, this->mRecord->shot.path.start);
        this->mRecord->shot.location = theTarget;
    }
    this->Call(EeGfxEffect3DOp::Create);
}

// between two units: the tracer beam of inFrom on inTo, referenced, aimed at a point picked on its model (0xfc3c5: the
// original tested the target for null, then used it); the tracers and shells on inTo from inFrom's weapon point to a
// point of inTo's model in inFrom's player colour (0xfd373); the winter breath at inFrom's mouth tag point (or its
// mouth by unit type, the breath then inert), its scaled location, whether the tag list was found and whether it moves
// (0xe819b); the smoke muzzle blast and the 2D projectile from inFrom's weapon tag point to inTo's model point on its
// scaled location, the blast's 15 higher for a high-arc shooter (0xe6664, 0xe2f58)
EEEffect::EEEffect(EEUnit* inFrom, EEUnit* inTo, EEGraphicEffect* inGraphicEffect, long inKind)
{
    static const float kTargetLift = 15.0f;
    if (inKind == 0x40) {
        this->Attach(inFrom, inGraphicEffect, inKind, false);
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
    if (inKind == 0x21 || inKind == 0x26) {
        this->Attach(inTo, inGraphicEffect, inKind, false);
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
        return;
    }
    this->Attach(NULL, inGraphicEffect, inKind, false);
    if (inKind == 0x16) {
        GE3DPoint thePosition;
        GETransformation theTransformation(inFrom->mComplexUnit->mPhysicalModel.GetLocation());
        EEComplexUnit* theBreather = inFrom->mComplexUnit;
        EETagPointList* theMouths = inFrom->GetModel()->GetTagPoints(theBreather->GetAnimation(), 0x16);
        float theScale = inFrom->mComplexUnit->mPhysicalModel.GetScale();
        unsigned long theFrame = theBreather->mPhysicalModel.GetFrame();
        EETagPointList::Scale(theTransformation, theScale);
        if (theMouths)
            theMouths->GetTagPoint(0)->GetPoint(theFrame, theTransformation, thePosition);
        else
            thePosition = EEMouthPoint(inFrom, theTransformation);
        this->mRecord->sprite.start = EeGe3DPoint{thePosition.mX, thePosition.mY, thePosition.mZ};
        this->mRecord->aged.transformation = *reinterpret_cast<const EeGeTransformation*>(&theTransformation);
        this->mRecord->sprite.flag = theMouths != NULL;
        this->mRecord->sprite.shrinking = inFrom->mComplexUnit->mVelocity != 0.0f;
        this->Call(EeGfxEffect3DOp::CreateBetween);
        return;
    }
    bool theProjectile = inKind == 0x30;
    GE3DPoint theStart;
    GE3DPoint theEnd;
    GE3DPoint theOffset;
    GETransformation theTargetTransformation(inTo->mComplexUnit->mPhysicalModel.GetLocation());
    EEModel* theModel = inFrom->GetModel();
    EEComplexUnit* theUnit = inFrom->mComplexUnit;
    EETagPointList* theList = inFrom->GetModel()->mTagPoints[theUnit->mAnimation];
    float theTargetScale = inTo->mComplexUnit->mPhysicalModel.GetScale();
    // the projectile's target point is on its scaled model whatever the shooter has (the same scaling either way)
    EECombatEffectWeaponPoint(theProjectile ? 1 : 4, theUnit, theModel, theList,
                              theProjectile ? theList && theList->mPoints.size() : true, theStart);
    EETagPointList::Scale(theTargetTransformation, theTargetScale);
    theOffset = inTo->GetModel()->mDefaultPoint;
    theTargetTransformation.Apply(theOffset, theEnd);
    if (!theProjectile
        && (inFrom->mUnitClass->mShootsHigh || (inFrom->mStats->mCanFireOverWalls && inFrom->mComplexUnit->mIsFiringHigh)))
        theEnd.SetZ(theEnd.GetZ() + kTargetLift);
    this->mRecord->sprite.start = EeGe3DPoint{theStart.mX, theStart.mY, theStart.mZ};
    this->mRecord->sprite.draw_position = EeGe3DPoint{theEnd.mX, theEnd.mY, theEnd.mZ};
    this->Call(EeGfxEffect3DOp::CreateBetween);
}

// between two points: the marching line (0xf359c: no set-up call), the linear projectile from inFrom to inTo
// (0xf2dd6), the earthquake's crack from inFrom towards inTo (0xec727)
EEEffect::EEEffect(const GE3DPoint& inFrom, const GE3DPoint& inTo, EEGraphicEffect* inGraphicEffect, long inKind)
{
    this->Attach(NULL, inGraphicEffect, inKind, false);
    if (inKind == 0x2d) {
        this->mRecord->march.from = EeGe3DPoint{inFrom.mX, inFrom.mY, inFrom.mZ};
        this->mRecord->march.to = EeGe3DPoint{inTo.mX, inTo.mY, inTo.mZ};
        return;
    }
    this->mRecord->shot.path.start = EeGe3DPoint{inFrom.mX, inFrom.mY, inFrom.mZ};
    this->mRecord->shot.location = EeGe3DPoint{inTo.mX, inTo.mY, inTo.mZ};
    this->Call(EeGfxEffect3DOp::Create);
}

// along a line: the 2D particle at its start with its direction as velocity (0xe4a4a), the velocity particle
// (EEE2DVelocityParticle), the 3D particle (0xf4e27) and the emitter (0xf5e82) at its start moving along it, the
// multi-effect's linked effects along it (0xf42c3)
EEEffect::EEEffect(const GE3DLine& inLine, EEGraphicEffect* inGraphicEffect, long inKind)
{
    this->Attach(NULL, inGraphicEffect, inKind, false);
    const GE3DPoint& theStart = inLine.GetStart();
    const GEVector& theDirection = inLine.GetDirection();
    EeGe3DPoint theVelocity = {theDirection.GetX(), theDirection.GetY(), theDirection.GetZ()};
    EeGe3DPoint thePoint = {theStart.GetX(), theStart.GetY(), theStart.GetZ()};
    if (inKind == 0x2e) {
        this->StartParticle(theStart, &theDirection);
        return;
    }
    if (inKind == 0xc) {
        this->mRecord->sprite.start = thePoint;
        this->mRecord->sprite.velocity = theVelocity;
    } else if (inKind == 0x39 || inKind == 0x3a) {
        this->mRecord->aged.location = thePoint;
        (inKind == 0x39 ? this->mRecord->particle.velocity : this->mRecord->emitter.velocity) = theVelocity;
    }
    this->Call(EeGfxEffect3DOp::CreateLine, NULL, NULL, 0, 0, 0, const_cast<GE3DLine*>(&inLine));
}

// the rain (0x2a) and the snow (0x3d) over the tiles, centred on them (EEE3DStorm, 0xfb0c8; the rain's first thunder,
// EEE3DRain 0xf7394)
EEEffect::EEEffect(const std::vector<U2DPoint<long> >& inTiles, EEGraphicEffect* inGraphicEffect, long inKind)
{
    this->Attach(NULL, inGraphicEffect, inKind, false);
    std::vector<int32_t> theTiles;
    for (size_t i = 0; i < inTiles.size(); i++) {
        theTiles.push_back((int32_t)inTiles[i].mX);
        theTiles.push_back((int32_t)inTiles[i].mY);
    }
    ee_gfx_storm_new(this->mRecord, theTiles.empty() ? NULL : &theTiles[0], (uint32_t)inTiles.size());
    this->Call(EeGfxEffect3DOp::Create);
}

// the water ripple (0x22) at a transformation (0xe7e50): kept in the record's aged.transformation
EEEffect::EEEffect(const GETransformation& inTransformation, EEGraphicEffect* inGraphicEffect, long inKind)
{
    this->Attach(NULL, inGraphicEffect, inKind, false);
    this->mRecord->aged.transformation = *reinterpret_cast<const EeGeTransformation*>(&inTransformation);
    this->Call(EeGfxEffect3DOp::CreateTransformation);
}

// the 2D particle's start at a point, moving along inVelocity when given, for its graphic effect's duration from now
void EEEffect::StartParticle(const GE3DPoint& inPosition, const GEVector* inVelocity)
{
    this->SetPosition(inPosition);
    *reinterpret_cast<GE3DPoint*>(&this->mRecord->particle.draw_position) = inPosition;
    if (inVelocity)
        this->mRecord->particle.velocity = EeGe3DPoint{inVelocity->GetX(), inVelocity->GetY(), inVelocity->GetZ()};
    this->mRecord->particle.last_time = gPhysics->GetGameClock();
    this->mRecord->particle.duration = this->mGraphicEffect->mDuration;
}

// the weather storms': every tile one step towards its place around the new centre (a step off the map stays), the
// storm recentred on the terrain there; the tiles' bounds; the radius half the longer side (ee_gfx effect_storm.rs)
void EEEffect::Move(const U2DPoint<long>& inCenter, U2DRectangle<long>& outBounds)
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

void EEEffect::GetBounds(U2DRectangle<long>& outBounds)
{
    int32_t theBounds[4];
    if (!ee_gfx_storm_bounds(this->mRecord, theBounds))
        return;
    outBounds.mLeft = theBounds[0];
    outBounds.mTop = theBounds[1];
    outBounds.mRight = theBounds[2];
    outBounds.mBottom = theBounds[3];
}

U2DPoint<long> EEEffect::GetCenterPoint() const
{
    return U2DPoint<long>(this->mRecord->storm.center_x, this->mRecord->storm.center_y);
}

bool EEEffect::SetFrom(const GE3DPoint& inPoint)
{
    if (this->mFromUnit != NULL)
        return false;
    this->mRecord->march.from = EeGe3DPoint{inPoint.mX, inPoint.mY, inPoint.mZ};
    return true;
}

bool EEEffect::SetTo(const GE3DPoint& inPoint)
{
    if (this->mToUnit != NULL)
        return false;
    this->mRecord->march.to = EeGe3DPoint{inPoint.mX, inPoint.mY, inPoint.mZ};
    return true;
}

void EEEffect::AddRegions(const GE3DPoint& inFrom, const GE3DPoint& inTo)
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

void EEEffect::RemoveRegions()
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

// the icon (2) and goal (0x3c): a fixed one moves to inLocation (EEE3DIcon::SetLocation, 0xf2637); others ignore it
void EEEffect::SetLocation(const GE3DPoint& inLocation)
{
    if ((this->mKind == 2 || this->mKind == 0x3c) && !this->mRecord->icon.follow_unit)
        this->mRecord->icon.location = EeGe3DPoint{inLocation.mX, inLocation.mY, inLocation.mZ};
}

// the energy wall (0x34): the current player sees the tile or holds a doppelganger of the unit there (then the wall
// is marked seen: its texture frame restarts) (EEE3DEnergyWall::IsTileVisible, 0xed98d); the others: the base's
bool EEEffect::IsTileVisible(EEMapTile* inTile)
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
                            theFarmSeen = theTile->IsSeenBy(gWorld.GetCurrentPlayer());
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
void EEEffect::ElevationChanged(long inLeft, long inBottom, long inRight, long inTop)
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

// One operation by the kind's class: the 2D kinds' (ee_gfx effect_2d.rs) over EeGfxEffect2DHost, the 3D kinds'
// (effect_rising.rs) over EeGfxEffect3DHost, both answered by Ask (their shared answers once); Load and Save read no
// facts (during a load the unit is still its saved id: RepairPointers resolves it)
long EEEffect::Call(EeGfxEffect3DOp inOp, EEEffectQueue* inQueues, const EeGfxEffectFile* inFile, long inBase,
                    unsigned long inMajorVersion, unsigned long inMinorVersion, void* inArg)
{
    Ask theAsk;
    theAsk.mEffect = this;
    theAsk.mQueues = inQueues;
    theAsk.mOp = inOp;
    theAsk.mArg = inArg;
    if (this->mIs2D) {
        EeGfxEffect2DHost theHost = {&theAsk, Ask::BaseQueue2D, Ask::Push, Ask::OwnMedia, Ask::Frames, Ask::MoveToPoint,
                                     Ask::PlaneModel, Ask::SelectionSize, Ask::ConfirmEnemy, Ask::ConfirmGoal,
                                     Ask::ConfirmUnit, Ask::UnitLocation, Ask::TerrainZ, Ask::CommandTexture,
                                     Ask::SpawnPoint, Ask::Media, Ask::TextureAt, Ask::TagCount, Ask::TagPoint,
                                     Ask::WeaponPoint, Ask::PartSound, Ask::DrawBlood, Ask::HasComplex, Ask::DefaultPoint,
                                     Ask::StatsScale, Ask::TileBlocked, Ask::Wind, Ask::CreateParts, Ask::PartCount,
                                     Ask::Part, Ask::SetPart, Ask::PartFrames, Ask::PartTexture, Ask::PartsSnapshot,
                                     Ask::PartFollow, Ask::MaxHitPoints, Ask::UnitPoint, Ask::Model, Ask::OrbitalCount,
                                     Ask::Orbital, Ask::IconUnit, Ask::AreaEffect, Ask::AreaEffects, Ask::SetAreaEffects,
                                     Ask::IconsSelected, Ask::IconsOwner, Ask::GhostUnits, Ask::GroundAt, Ask::GlowSetUp,
                                     Ask::GlowTagName, Ask::GlowPlace, Ask::GlowPoint, Ask::GlowArea, Ask::GlowRows,
                                     Ask::PositionTick, Ask::ParticleGround};
        if (inFile != NULL)
            return ee_gfx_effect2d_file(inOp, this->mRecord, inFile, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion,
                                        &theHost);
        EeGfxGraphicFacts theFacts = EEEffectFacts(this->mGraphicEffect, this->mUnit);
        return ee_gfx_effect2d(inOp, this->mRecord, &theFacts, &theHost, &gEERustRaster);
    }
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
    if (inFile != NULL)
        return ee_gfx_effect3d_file(inOp, inBase, this->mRecord, inFile, inMajorVersion, inMinorVersion, &theHost);
    EeGfxGraphicFacts theFacts = EEEffectFacts(this->mGraphicEffect, this->mUnit);
    return ee_gfx_effect3d(inOp, this->mRecord, &theFacts, &theHost, &gEERustRaster);
}

// Creates one sprite part per tag point of the effect's kind on the unit (else one part at the unit, tag 10000),
// lifting a buried position by inOffset; returns the position and the height (farthest part + effect size)
void EEEffect::CreateParts(GE3DPoint& outPosition, float& outHeight, float inOffset)
{
    GE3DPoint theTagPoint;
    GETerrainMesh* theMesh = &gWorld.mMap->mTerrainMesh;
    GE3DPoint thePosition = this->GetUnitLocation();
    float theMaximum = 0.0f;
    float theHeight = 1.0f;
    EEUnit* theUnit = this->mUnit;
    // The original reads the stats above the null test ([unit+0x34] before the je); a unitless effect (an
    // explosion at a point) has no unit, so ours reads them inside it.
    if (theUnit) {
        EEUnitClassUpgrade* theStats = theUnit->GetStats();
        EEComplexUnit* theComplexUnit = theUnit->GetComplexUnit();
        EETagPointList* theList = theUnit->GetModel()->GetTagPoints(theComplexUnit ? theComplexUnit->mAnimation : 0,
                                                                this->GetGraphicEffect()->GetKind());
        unsigned long theY = UMFloorUnsigned(thePosition.GetY());
        unsigned long theX = UMFloorUnsigned(thePosition.GetX());
        if (theMesh->GetIsValidPoint(theX, theY)) {
            float theZ = theMesh->GetMeshZ(thePosition.GetX(), thePosition.GetY());
            if (thePosition.GetZ() < theZ)
                thePosition.SetZ(theZ + inOffset);
        }
        if (theList && theList->GetNumTagPoints() != 0) {
            GETransformation theTransformation(false);
            float theScale;
            unsigned long theFrame;
            if (theComplexUnit) {
                theTransformation = theComplexUnit->mPhysicalModel.GetLocation();
                theScale = theComplexUnit->mPhysicalModel.GetScale();
                theFrame = theComplexUnit->mPhysicalModel.GetFrame();
            } else {
                theTransformation.MakeIdentity();
                theTransformation.SetPosition(thePosition.GetX(), thePosition.GetY(), thePosition.GetZ());
                theFrame = 0;
                theScale = theStats->GetScale();
            }
            EETagPointList::Scale(theTransformation, theScale);
            for (unsigned long i = 0; i < theList->GetNumTagPoints(); i++) {
                EEGraphicObject* theObject = this->GetGraphicObject();
                theObject->LoadMedia();
                theList->GetTagPoint(i)->GetPoint(theFrame, theTransformation, theTagPoint);
                this->mParts.push_back(EEEffect2DPart{theTagPoint, theObject, i, (unsigned long)theObject->mStartFrames[0], 0, 0.0f, false});
                float theDistance = thePosition.DistanceToSquared(theTagPoint);
                if (theDistance > theMaximum)
                    theMaximum = theDistance;
            }
            theHeight = (float)sqrt(theMaximum) + this->GetGraphicEffect()->GetSize();
        } else {
            EEGraphicObject* theObject = this->GetGraphicObject();
            theObject->LoadMedia();
            this->mParts.push_back(EEEffect2DPart{thePosition, theObject, 10000, (unsigned long)theObject->mStartFrames[0], 0, 0.0f, false});
            theHeight = this->GetGraphicEffect()->GetSize();
        }
    }
    outPosition = thePosition;
    outHeight = theHeight;
}

// The world the damage controller's Rust tick calls into: its children made and destroyed (from the deleted
// EEEDamagedEffect.cpp)
namespace {
struct EEDamagedHost {
    static EEEffect& Self(void* inContext) { return *static_cast<EEEffect*>(inContext); }
    static uint32_t Create(void* inContext, bool inSecondary, uint32_t inIndex)
    {
        EEEffect& theEffect = Self(inContext);
        EEEffectHandle theHandle;
        EEGraphicEffect* theLinked = inSecondary ? theEffect.GetGraphicEffect()->GetLinkedEffect2()
                                                 : theEffect.GetGraphicEffect()->GetLinkedEffect1();
        gWorld.GetEffectManager()->CreateUnitEffect(theEffect.GetUnit(), theLinked, &theHandle, 0, inIndex, 0);
        return theHandle.mValue;
    }
    static void Destroy(void*, uint32_t inHandle)
    {
        EEEffectHandle theHandle = {inHandle};
        gWorld.GetEffectManager()->DestroyEffect(theHandle);
    }
};
}

// The damage controller (0x3e; EEEDamagedEffect::Step, 0x10225c): one child damage effect (plus its secondary) per
// damage tag point, more as the unit's health drops; all hidden on a cloaked enemy unit; once finished destroys them
// and returns 7 (the counts and the children's handles are ee_gfx effect_state.rs's). The others: Rust's
long EEEffect::Step()
{
    if (this->mKind != 0x3e)
        return this->Call(EeGfxEffect3DOp::Step);
    EeGfxDamagedTick theTick = {};
    EEGraphicEffect* theEffect = this->GetGraphicEffect();
    theTick.has_linked2 = theEffect->GetLinkedEffect2() != NULL;
    theTick.param2 = theEffect->GetParam2();
    EEUnit* theUnit = this->mUnit;
    if (theUnit && !this->mRecord->damaged.finished) {
        theTick.has_unit = true;
        if (theUnit->mTileNode) {
            while (theUnit->mComplexUnit->mPreviousPart)
                theUnit = theUnit->mComplexUnit->mPreviousPart;
        }
        EETagPointList* theList;
        if (theUnit->mTileNode)
            theList = theUnit->GetModel()->GetTagPoints(theUnit->mComplexUnit->GetAnimation(),
                                                        theEffect->GetLinkedEffect1()->mTagPointKind);
        else
            theList = theUnit->GetModel()->GetTagPoints(0, theEffect->GetLinkedEffect1()->mTagPointKind);
        theTick.tag_count = theList ? (uint32_t)theList->GetNumTagPoints() : 1;
        theTick.hit_points = (int32_t)theUnit->GetHitPoints();
        theTick.max_hit_points = (int32_t)theUnit->GetStats()->GetMaxHitPoints();
        if (theUnit->GetTileNode()) {
            EEComplexUnit* theComplexUnit = theUnit->GetComplexUnit();
            if (theComplexUnit->GetIsCloaked() && !gWorld.GetCurrentPlayer()->IsAlly(theComplexUnit->GetPlayer()))
                theTick.hidden = true;
        }
    }
    EeGfxDamagedHost theHost = {this, EEDamagedHost::Create, EEDamagedHost::Destroy};
    return ee_gfx_effect_damaged_step(this->mRecord, &theTick, &theHost);
}

// The battlefield decals' placement (EEE2DBattlefieldAmbient.cpp's helpers, 0xde374..0xde6e9: ee_gfx
// effect_placement.rs) over the graphic effects, the manager's list, the map and the manager's placement
namespace {
struct EEDecalOps {
    static EeGfxDecalEffect Effect(void*, void* inEffect)
    {
        EEGraphicEffect* theEffect = static_cast<EEGraphicEffect*>(inEffect);
        return EeGfxDecalEffect{theEffect->GetParam2(), theEffect->GetParam3(), theEffect->GetParam4(), theEffect->GetParam5(),
                                (int32_t)theEffect->mIntParam2,
                                {theEffect->mLinkedEffect1, theEffect->mLinkedEffect2, theEffect->mLinkedEffect3}};
    }
    static void* Next(void*, void* inEffect)
    {
        return inEffect ? gEffectManager->Next(static_cast<EEEffect*>(inEffect)) : gEffectManager->First();
    }
    static EeGfxEffectRecord* Record(void*, void* inEffect) { return static_cast<EEEffect*>(inEffect)->mRecord; }
    static float Height(void*, const EeGe3DPoint* inPoint)
    {
        return gWorld.mMap->GetHeight(*reinterpret_cast<const GE3DPoint*>(inPoint));
    }
    static void Place(void*, void* inEffect, const EeGe3DPoint* inPoint)
    {
        gEffectManager->CreatePlacedEffect(*reinterpret_cast<const GE3DPoint*>(inPoint), static_cast<EEGraphicEffect*>(inEffect),
                                           NULL);
    }
};
const EeGfxDecalHost kDecalHost = {NULL, EEDecalOps::Effect, EEDecalOps::Next, EEDecalOps::Record, EEDecalOps::Height,
                                   EEDecalOps::Place};
}

// Places up to N shell-hole decals of the unit's class around a point (ambient effects on); stops on a failed roll
// The first one renews a decal already on that spot instead, unless its scatter roll moves it
void EEPlaceImpactDecal(EEComplexUnit* inUnit, GE3DPoint& inLocation)
{
    EEGraphicEffect* theEffect;
    if (gAmbientEffectsEnabled && (theEffect = inUnit->mUnitClass->mShellHoleEffect) != 0)
        ee_gfx_decal_impact(&kDecalHost, theEffect, reinterpret_cast<const EeGe3DPoint*>(&inLocation));
}

// Places the unit class's blood-stain decal under the unit if ambient effects and blood are on and its roll passes
// (moved forward along the heading for giraffes and horses)
void EEPlaceTrackDecal(EEComplexUnit* inUnit)
{
    EEGraphicEffect* theEffect;
    if (!gAmbientEffectsEnabled || !gDrawBlood || (theEffect = inUnit->mUnitClass->mBloodStainEffect) == 0)
        return;
    GE3DPoint theLocation;
    inUnit->GetPosition(theLocation);
    unsigned long theType = inUnit->mUnitClass->mUnitType;
    const GETransformation& theTransformation = inUnit->mPhysicalModel.GetLocation();
    ee_gfx_decal_track(&kDecalHost, theEffect, reinterpret_cast<const EeGe3DPoint*>(&theLocation),
                       theType == kUnitTypeResGiraffe ? EE_GFX_TRACK_GIRAFFE : theType == kUnitTypeResHorse ? EE_GFX_TRACK_HORSE : 0,
                       theTransformation.mMatrix[0][0], theTransformation.mMatrix[1][0]);
}

// Scales the rotation part of a transformation by a factor (the translation is kept) (from the deleted
// EEE2DImpact.cpp, whose range held it)
void EETagPointList::Scale(GETransformation& ioTransformation, float inScale)
{
    ioTransformation.SetElement(0, 0, ioTransformation.GetElement(0, 0) * inScale);
    ioTransformation.SetElement(0, 1, ioTransformation.GetElement(0, 1) * inScale);
    ioTransformation.SetElement(0, 2, ioTransformation.GetElement(0, 2) * inScale);
    ioTransformation.SetElement(1, 0, ioTransformation.GetElement(1, 0) * inScale);
    ioTransformation.SetElement(1, 1, ioTransformation.GetElement(1, 1) * inScale);
    ioTransformation.SetElement(1, 2, ioTransformation.GetElement(1, 2) * inScale);
    ioTransformation.SetElement(2, 0, ioTransformation.GetElement(2, 0) * inScale);
    ioTransformation.SetElement(2, 1, ioTransformation.GetElement(2, 1) * inScale);
    ioTransformation.SetElement(2, 2, ioTransformation.GetElement(2, 2) * inScale);
}
