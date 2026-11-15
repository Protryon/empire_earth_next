// The 2D effects whole in Rust (docs/RUST_PLAN.md R4.4; ee_gfx effect_2d.rs, effect_confirm.rs, effect_shockwave.rs,
// effect_sprite.rs): EEE2DRust's virtuals call Rust by the record's kind and answer its questions about the world here.
// Replaces EEE3DCommandConfirmation.cpp, EEE2DShockwave.cpp, EEE2DImpact.cpp (but EETagPointList::Scale, below),
// EEE2DImpactBlood.cpp, EEE2DMuzzleFlash.cpp, EEE2DLightning.cpp, EEE2DPlasmaPulse.cpp, EEE2DPulseWave.cpp,
// EEE2DWaterRipple.cpp, EEE2DFireTrail.cpp, EEE2DExplosion.cpp, EEE2DFire.cpp, EEE2DExhaustTrail.cpp, EEE2DSmokePlume.cpp
// EEE2DSmokeMuzzleBlast.cpp, EEE2DWinterBreath.cpp, EEE2DLinearProjectile.cpp and EEE2DVortex.cpp; since the
// twenty-first round EEE2DMarchingIcons.cpp, EEE2DLightGlow.cpp, EEE2DBattlefieldAmbient.cpp (its decal placement
// helpers kept below) and EEE2DParticle.cpp (the pool kept below).
#include <windows.h>
#include <string.h>

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
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEE3DRust.h"
#include "ee/EEE2DRust.h"
#include "ee/EECombatFormulas.h"
#include "ee/EEAEManager.h"
#include "ee/EEEffectManager.h"
#include "ee/EEEffectKindIDs.h"
#include "ee/EEE2DBattlefieldAmbient.h"
#include "lle/GE3DLine.h"
#include "lle/UMemoryPool.h"
#include "rust/ee_combat.h"

namespace {
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
struct Ask2D {
    EEE2DRust* mEffect;
    EEEffectQueue* mQueues;
    // the light glow's: the unit's light tags, its frame and its scaled location while the call places the glows
    EETagPointList* mTags;
    unsigned long mFrame;
    GETransformation mLocation;
    static EEE2DRust* E(void* inAsk) { return static_cast<Ask2D*>(inAsk)->mEffect; }
    static int32_t BaseQueue(void* inAsk)
    {
        EEE2DRust* theEffect = E(inAsk);
        EEEffectQueue* theQueues = static_cast<Ask2D*>(inAsk)->mQueues;
        return theEffect->mKind == 0x41 ? theEffect->EEEffect::Queue(theQueues) : theEffect->EEEffect2D::Queue(theQueues);
    }
    static void Push(void* inAsk, uint32_t inIndex)
    {
        EEGraphicObject* theObject = E(inAsk)->mGraphicEffect->mGraphicObject;
        GETexture* theTexture = theObject ? theObject->mTextures[gScreenIndex] : NULL;
        static_cast<Ask2D*>(inAsk)->mQueues[inIndex][theTexture].push_back(E(inAsk));
    }
    // the graphic effect's own object (not GetGraphicObject: EEE3DCommandConfirmation::SetUp, 0xe9eb5)
    static int32_t Media(void* inAsk)
    {
        E(inAsk)->mGraphicObject = E(inAsk)->mGraphicEffect->mGraphicObject;
        return E(inAsk)->mGraphicObject->LoadMedia();
    }
    static EeGfxFrames Frames(void* inAsk, uint32_t inIndex)
    {
        EEGraphicObject* theObject = E(inAsk)->mGraphicObject;
        return EeGfxFrames{(uint32_t)theObject->mStartFrames[inIndex], (uint32_t)theObject->mFrameTimes[inIndex],
                           (uint32_t)theObject->mFrameCounts[inIndex], (uint32_t)theObject->mLastFrames[inIndex]};
    }
    static void MoveToPoint(void* inAsk)
    {
        GE3DPoint thePosition(E(inAsk)->mPosition);
        E(inAsk)->MoveToPoint(thePosition);
    }
    static void* Model(void* inAsk) { return E(inAsk)->mModel; }
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
    static EeGeTransformation UnitLocation(void* inAsk)
    {
        const GETransformation& theLocation = E(inAsk)->mUnit->mComplexUnit->mPhysicalModel.GetLocation();
        return *reinterpret_cast<const EeGeTransformation*>(&theLocation);
    }
    static float TerrainZ(void*, float inX, float inY) { return gWorld.GetMap()->mTerrainMesh.GetMeshZ(inX, inY); }
    static void* CommandTexture(void*, uint32_t inIndex)
    {
        return inIndex == 0 ? sTexture4E0 : inIndex == 1 ? sTexture4E4 : sTexture4E8;
    }
    // a random one of the unit's tag points of inKind (the unit itself without any) (EEE2DShockwave::SetUp, 0xe6259)
    static EeGe3DPoint SpawnPoint(void* inAsk, uint32_t inKind)
    {
        EEE2DRust* theEffect = E(inAsk);
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
    static int32_t ObjectMedia(void* inAsk)
    {
        E(inAsk)->mGraphicObject = E(inAsk)->GetGraphicObject();
        return E(inAsk)->mGraphicObject->LoadMedia();
    }
    static void* ObjectTexture(void* inAsk, uint32_t inIndex) { return E(inAsk)->mGraphicObject->mTextures[inIndex]; }
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
    static void AnimationSound(void* inAsk, EeGe3DPoint inLocation, uint32_t inFrame, bool inChanged, bool inPart0)
    {
        EEE2DRust* theEffect = E(inAsk);
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
        EEE2DRust* theEffect = E(inAsk);
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
    static void* ObjectModel(void* inAsk) { return E(inAsk)->mGraphicObject->mModel; }
    static uint32_t OrbitalCount(void* inAsk) { return (uint32_t)E(inAsk)->mGraphicObject->GetOrbitalCount(); }
    static EeGfxOrbital Orbital(void* inAsk, uint32_t inIndex)
    {
        EEOrbital* theOrbital = E(inAsk)->mGraphicObject->GetOrbital(inIndex);
        return EeGfxOrbital{theOrbital->mModel, theOrbital->mSpeed, theOrbital->mAngle};
    }
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
    static EeGfx2DPoint GroundPoint(void*, EeGe3DPoint inAt)
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
        static_cast<Ask2D*>(inAsk)->mTags = theTags;
        EeGfxGlowSetUp theSetUp = {theTags != NULL ? (uint32_t)theTags->GetNumTagPoints() : 0u,
                                   EeGe3DPoint{thePosition.mX, thePosition.mY, thePosition.mZ}};
        return theSetUp;
    }
    static uint32_t GlowTagName(void* inAsk, uint32_t inIndex, uint8_t* outName, uint32_t inCapacity)
    {
        EETagPoint* thePoint = static_cast<Ask2D*>(inAsk)->mTags->mPoints[inIndex];
        UString theName = thePoint->mModel->GetTagPointName(thePoint->mIndex);
        uint32_t theLength = (uint32_t)theName.GetLength();
        memcpy(outName, (const char*)theName, theLength < inCapacity ? theLength : inCapacity);
        return theLength;
    }
    // the placing's unit: hidden for another player's cloaked unit or an unfinished land building
    static EeGfxGlowPlace GlowPlace(void* inAsk)
    {
        Ask2D* theAsk = static_cast<Ask2D*>(inAsk);
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
        Ask2D* theAsk = static_cast<Ask2D*>(inAsk);
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
    static EeGfxParticleTick ParticleTick(void* inAsk) { return EEParticleTick(E(inAsk)->mGraphicEffect, E(inAsk)->mPosition); }
    static EeGfxParticleGround ParticleGround(void*) { return EEParticleGround(); }
};

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

// the kind's shared model: the shockwave's, confirmation's, pulse wave's, ripple's and vortex's 0x4e8df4, the
// others' 0x4e8df8
GEModel* EE2DModel(long inKind)
{
    return inKind == 0x12 || inKind == 0x41 || inKind == 0x2f || inKind == 0x22 || inKind == 0x31 || inKind == 0x32
               ? gU4E8DF4
               : gEffect2DModel;
}
}

// the command confirmation (0x41) on a unit (EEE3DCommandConfirmation, 0xe9d1e)
EEE2DRust::EEE2DRust(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, unsigned long inGoalIndex, long inKind)
    : EEEffect2D(inUnit, inGraphicEffect, gU4E8DF4, inKind, false)
{
    this->mRecord->confirm.goal_index = (uint32_t)inGoalIndex;
    this->Call(EeGfxEffect3DOp::Create);
}

// at a point (0xe9d91)
EEE2DRust::EEE2DRust(EEUnit* inUnit, const GE3DPoint& inLocation, EEGraphicEffect* inGraphicEffect,
                     unsigned long inGoalIndex, long inKind)
    : EEEffect2D(inUnit, inGraphicEffect, gU4E8DF4, inKind, false)
{
    this->mPosition = inLocation;
    this->mRecord->confirm.goal_index = (uint32_t)inGoalIndex;
    this->Call(EeGfxEffect3DOp::CreateAt);
}

// the other kinds on a unit (EEE2DShockwave 0xe6192, EEE2DImpact 0xe103d, ...)
EEE2DRust::EEE2DRust(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(inUnit, inGraphicEffect, EE2DModel(inKind), inKind, false)
{
    this->Call(EeGfxEffect3DOp::Create);
}

// the light glow on a unit, keeping it when asked (EEE2DLightGlow 0xe1a7a)
EEE2DRust::EEE2DRust(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind, KeepsUnit inKeepsUnit)
    : EEEffect2D(inUnit, inGraphicEffect, EE2DModel(inKind), inKind, inKeepsUnit.mValue)
{
    this->Call(EeGfxEffect3DOp::Create);
}

// at a point (EEE2DShockwave 0xe61e2, EEE2DImpact 0xe10af, ...): the point for Rust in the record's sprite.start
EEE2DRust::EEE2DRust(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(0, inGraphicEffect, EE2DModel(inKind), inKind, false)
{
    // the battlefield decal (0xde74f): its size and corners on the terrain (hidden and over off the map or not level)
    if (inKind == 0x32) {
        EeGfxAmbientNew theNew = {inGraphicEffect->GetStartSize(), inGraphicEffect->GetSize(),
                                  (int32_t)inGraphicEffect->mIntParam1, inGraphicEffect->mFlag1};
        EeGfxParticleGround theGround = EEParticleGround();
        ee_gfx_effect_ambient_new(this->mRecord, reinterpret_cast<const EeGe3DPoint*>(&inPosition), &theNew, &theGround);
        return;
    }
    this->mRecord->sprite.start = EeGe3DPoint{inPosition.mX, inPosition.mY, inPosition.mZ};
    this->Call(EeGfxEffect3DOp::CreateAt);
}

// at a transformation (EEE2DWaterRipple 0xe7e50): kept in the record's aged.transformation
EEE2DRust::EEE2DRust(const GETransformation& inTransformation, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(0, inGraphicEffect, EE2DModel(inKind), inKind, false)
{
    this->mRecord->aged.transformation = *reinterpret_cast<const EeGeTransformation*>(&inTransformation);
    this->Call(EeGfxEffect3DOp::CreateTransformation);
}

// along a line (the velocity particle, the original's EEE2DVelocityParticle): its start and direction for Rust
EEE2DRust::EEE2DRust(const GE3DLine& inLine, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(0, inGraphicEffect, EE2DModel(inKind), inKind, false)
{
    GE3DPoint theStart = inLine.GetStart();
    GEVector theDirection = inLine.GetDirection();
    this->mRecord->sprite.start = EeGe3DPoint{theStart.GetX(), theStart.GetY(), theStart.GetZ()};
    this->mRecord->sprite.velocity = EeGe3DPoint{theDirection.GetX(), theDirection.GetY(), theDirection.GetZ()};
    this->Call(EeGfxEffect3DOp::CreateLine);
}

// from the shooter's weapon tag point (turret-aware) to the target's model point on its scaled location, the muzzle
// blast's 15 higher for a high-arc shooter (EEE2DSmokeMuzzleBlast 0xe6664, EEE2DLinearProjectile 0xe2f58): the two
// ends for Rust; the winter breath (0xe819b) at the unit's mouth tag point (or its mouth by unit type, the breath then
// inert), the unit's scaled location, whether the tag list was found and whether the unit moves
EEE2DRust::EEE2DRust(EEUnit* inUnit, EEUnit* inTarget, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(0, inGraphicEffect, EE2DModel(inKind), inKind, false)
{
    static const float kTargetLift = 15.0f;
    if (inKind == 0x16) {
        GE3DPoint thePosition;
        GETransformation theTransformation(inUnit->mComplexUnit->mPhysicalModel.GetLocation());
        EEComplexUnit* theBreather = inUnit->mComplexUnit;
        EETagPointList* theMouths = inUnit->GetModel()->GetTagPoints(theBreather->GetAnimation(), 0x16);
        float theScale = inUnit->mComplexUnit->mPhysicalModel.GetScale();
        unsigned long theFrame = theBreather->mPhysicalModel.GetFrame();
        EETagPointList::Scale(theTransformation, theScale);
        if (theMouths)
            theMouths->GetTagPoint(0)->GetPoint(theFrame, theTransformation, thePosition);
        else
            thePosition = EEMouthPoint(inUnit, theTransformation);
        this->mRecord->sprite.start = EeGe3DPoint{thePosition.mX, thePosition.mY, thePosition.mZ};
        this->mRecord->aged.transformation = *reinterpret_cast<const EeGeTransformation*>(&theTransformation);
        this->mRecord->sprite.flag = theMouths != NULL;
        this->mRecord->sprite.shrinking = inUnit->mComplexUnit->mVelocity != 0.0f;
        this->Call(EeGfxEffect3DOp::CreateBetween);
        return;
    }
    bool theProjectile = inKind == 0x30;
    GE3DPoint theStart;
    GE3DPoint theEnd;
    GE3DPoint theOffset;
    GETransformation theTargetTransformation(inTarget->mComplexUnit->mPhysicalModel.GetLocation());
    EEModel* theModel = inUnit->GetModel();
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    EETagPointList* theList = inUnit->GetModel()->mTagPoints[theUnit->mAnimation];
    float theTargetScale = inTarget->mComplexUnit->mPhysicalModel.GetScale();
    // the projectile's target point is on its scaled model whatever the shooter has (the same scaling either way)
    EECombatEffectWeaponPoint(theProjectile ? 1 : 4, theUnit, theModel, theList,
                              theProjectile ? theList && theList->mPoints.size() : true, theStart);
    EETagPointList::Scale(theTargetTransformation, theTargetScale);
    theOffset = inTarget->GetModel()->mDefaultPoint;
    theTargetTransformation.Apply(theOffset, theEnd);
    if (!theProjectile
        && (inUnit->mUnitClass->mShootsHigh || (inUnit->mStats->mCanFireOverWalls && inUnit->mComplexUnit->mIsFiringHigh)))
        theEnd.SetZ(theEnd.GetZ() + kTargetLift);
    this->mRecord->sprite.start = EeGe3DPoint{theStart.mX, theStart.mY, theStart.mZ};
    this->mRecord->sprite.draw_position = EeGe3DPoint{theEnd.mX, theEnd.mY, theEnd.mZ};
    this->Call(EeGfxEffect3DOp::CreateBetween);
}

// to a point (EEE2DSmokeMuzzleBlast 0xe6a07, EEE2DLinearProjectile 0xe31dd)
EEE2DRust::EEE2DRust(EEUnit* inUnit, const GE3DPoint& inTarget, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(0, inGraphicEffect, EE2DModel(inKind), inKind, false)
{
    GE3DPoint theStart;
    EEModel* theModel = inUnit->GetModel();
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    EETagPointList* theList = inUnit->GetModel()->mTagPoints[theUnit->mAnimation];
    if (inKind == 0x30)
        EECombatEffectWeaponPoint(2, theUnit, theModel, theList, theList && theList->mPoints.size(), theStart);
    else
        EECombatEffectWeaponPoint(5, theUnit, theModel, theList, true, theStart);
    this->mRecord->sprite.start = EeGe3DPoint{theStart.mX, theStart.mY, theStart.mZ};
    this->mRecord->sprite.draw_position = EeGe3DPoint{inTarget.mX, inTarget.mY, inTarget.mZ};
    this->Call(EeGfxEffect3DOp::CreateBetween);
}

// for loading (EEE2DShockwave 0xe621f, ...)
EEE2DRust::EEE2DRust(long inKind)
    : EEEffect2D(EE2DModel(inKind), inKind, false)
{
}

// the pooled 2D particle (EEE2DParticle, kind 0x2e): the pool's objects are made cleared with no references
// (0xe47b1), taken by NewParticle (0xe489b, 0xe49c7, 0xe4a4a, 0xe4adf) and given back by LastDereference (0xe4f32)
UMemoryPool<EEE2DRust> EEE2DRust::sParticlePool;

EEE2DRust::EEE2DRust()
    : EEEffect2D(0, 0x2e, false)
{
    this->Reinit(NULL, false);
    this->ClearParticle();
    this->mReferenceCount = 0;
}

bool EEE2DRust::SetParticlePoolSize(unsigned long inChunkSize, unsigned long inMaxChunks)
{
    return EEE2DRust::sParticlePool.SetSize(inChunkSize, inMaxChunks, 1);
}

void EEE2DRust::ReleaseParticlePool()
{
    EEE2DRust::sParticlePool.DumpStats(false);
    EEE2DRust::sParticlePool.Release();
}

// Clear (0xe4be3): the particle's motion, age, look and timing to zero, with no graphic
void EEE2DRust::ClearParticle()
{
    ee_gfx_effect_particle_clear(this->mRecord);
    this->mGraphicObject = NULL;
}

// a particle from the pool for loading (0xe4adf)
EEE2DRust* EEE2DRust::NewParticle()
{
    EEE2DRust* theParticle = EEE2DRust::sParticlePool.New();
    if (theParticle->mReferenceCount == 0)
        theParticle->mReferenceCount = 1;
    theParticle->Reinit(gEffect2DModel, false);
    theParticle->ClearParticle();
    return theParticle;
}

// a particle from the pool for a unit's or no unit's effect
EEE2DRust* EEE2DRust::TakeParticle(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
{
    EEE2DRust* theParticle = EEE2DRust::sParticlePool.New();
    if (theParticle->mReferenceCount == 0)
        theParticle->mReferenceCount = 1;
    theParticle->Reinit(inUnit, inGraphicEffect, gEffect2DModel, false);
    theParticle->ClearParticle();
    return theParticle;
}

// its start at a point, moving along inVelocity when given, for its graphic effect's duration from now
EEE2DRust* EEE2DRust::StartParticle(const GE3DPoint& inPosition, const GEVector* inVelocity)
{
    this->SetPosition(inPosition);
    *reinterpret_cast<GE3DPoint*>(&this->mRecord->particle.draw_position) = inPosition;
    if (inVelocity)
        this->mRecord->particle.velocity = EeGe3DPoint{inVelocity->GetX(), inVelocity->GetY(), inVelocity->GetZ()};
    this->mRecord->particle.last_time = gPhysics->GetGameClock();
    this->mRecord->particle.duration = this->mGraphicEffect->mDuration;
    return this;
}

// at the unit, or its tile's centre on the ground (0 off the map) (0xe489b)
EEE2DRust* EEE2DRust::NewParticle(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
{
    EEE2DRust* theParticle = EEE2DRust::TakeParticle(inUnit, inGraphicEffect);
    GE3DPoint theLocation;
    EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
    if (theComplexUnit) {
        const GETransformation& theTransformation = theComplexUnit->mPhysicalModel.GetLocation();
        theLocation.SetX(theTransformation.GetX());
        theLocation.SetY(theTransformation.GetY());
        theLocation.SetZ(theTransformation.GetZ());
    } else {
        theLocation.SetX(inUnit->mUnitClass->GetTileSize() * 0.5f + inUnit->mTileX);
        theLocation.SetY(inUnit->mUnitClass->GetTileSize() * 0.5f + inUnit->mTileY);
        if (gWorld.GetMap()->mTerrainMesh.GetIsValidTile(inUnit->mTileX, inUnit->mTileY))
            theLocation.SetZ(gWorld.GetMap()->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()));
        else
            theLocation.SetZ(0.0f);
    }
    return theParticle->StartParticle(theLocation, NULL);
}

// at a point (0xe49c7)
EEE2DRust* EEE2DRust::NewParticle(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect)
{
    return EEE2DRust::TakeParticle(NULL, inGraphicEffect)->StartParticle(inPosition, NULL);
}

// at the line's start with its direction as velocity (0xe4a4a)
EEE2DRust* EEE2DRust::NewParticle(const GE3DLine& inLine, EEGraphicEffect* inGraphicEffect)
{
    EEE2DRust* theParticle = EEE2DRust::TakeParticle(NULL, inGraphicEffect);
    GEVector theDirection = inLine.GetDirection();
    return theParticle->StartParticle(inLine.GetStart(), &theDirection);
}

// the particle unloads its graphic (0xe4b2b); the others keep it to the destructor
void EEE2DRust::ReleaseResources()
{
    if (this->mKind == 0x2e && this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
    EEEffect2D::ReleaseResources();
}

// the particle goes back to the pool (0xe4f32); the others are dereferenced
void EEE2DRust::LastDereference()
{
    if (this->mKind != 0x2e) {
        EEEffect2D::LastDereference();
        return;
    }
    this->ReleaseResources();
    this->mReferenceCount = 0;
    EEE2DRust::sParticlePool.Delete(this);
}

// stops the sound and unloads the kept graphic object's media
EEE2DRust::~EEE2DRust()
{
    if (this->mSound) {
        gSoundManager->Stop(this->mSound, false);
        this->mSound->Dereference();
        this->mSound = NULL;
    }
    if (this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
}

long EEE2DRust::Call(EeGfxEffect3DOp inOp, EEEffectQueue* inQueues, const EeGfxEffectFile* inFile,
                     unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    Ask2D theAsk = {this, inQueues};
    EeGfxEffect2DHost theHost = {&theAsk, Ask2D::BaseQueue, Ask2D::Push, Ask2D::Media, Ask2D::Frames, Ask2D::MoveToPoint,
                                 Ask2D::Model, Ask2D::SelectionSize, Ask2D::ConfirmEnemy, Ask2D::ConfirmGoal,
                                 Ask2D::ConfirmUnit, Ask2D::UnitLocation, Ask2D::TerrainZ, Ask2D::CommandTexture,
                                 Ask2D::SpawnPoint, Ask2D::ObjectMedia, Ask2D::ObjectTexture, Ask2D::TagCount,
                                 Ask2D::TagPoint, Ask2D::WeaponPoint, Ask2D::AnimationSound, Ask2D::DrawBlood,
                                 Ask2D::HasComplex, Ask2D::DefaultPoint, Ask2D::StatsScale, Ask2D::TileBlocked,
                                 Ask2D::Wind, Ask2D::CreateParts, Ask2D::PartCount, Ask2D::Part, Ask2D::SetPart,
                                 Ask2D::PartFrames, Ask2D::PartTexture, Ask2D::PartsSnapshot, Ask2D::PartFollow,
                                 Ask2D::MaxHitPoints, Ask2D::UnitPoint, Ask2D::ObjectModel, Ask2D::OrbitalCount,
                                 Ask2D::Orbital, Ask2D::IconUnit, Ask2D::AreaEffect, Ask2D::AreaEffects,
                                 Ask2D::SetAreaEffects, Ask2D::IconsSelected, Ask2D::IconsOwner, Ask2D::GhostUnits,
                                 Ask2D::GroundPoint, Ask2D::GlowSetUp, Ask2D::GlowTagName, Ask2D::GlowPlace,
                                 Ask2D::GlowPoint, Ask2D::GlowArea, Ask2D::GlowRows, Ask2D::ParticleTick,
                                 Ask2D::ParticleGround};
    // Load and Save read no facts: during a load the unit is still its saved id (RepairPointers resolves it)
    if (inFile != NULL)
        return ee_gfx_effect2d_file(inOp, this->mRecord, inFile, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion,
                                    &theHost);
    EeGfxGraphicFacts theFacts = EEEffectFacts(this->mGraphicEffect, this->mUnit);
    return ee_gfx_effect2d(inOp, this->mRecord, &theFacts, &theHost, &gEERustRaster);
}

// the confirmation (0x41): 0 without the base's (it is never saved); the others: EEEffect2D's, then the kind's (the
// base's result ignored, as the shockwave's)
long EEE2DRust::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    if (this->mKind == 0x41)
        return this->Call(EeGfxEffect3DOp::Load);
    // the damage controller's children's handles and, from 0.249 on, its finished flag (EEEDamagedEffect::Load
    // 0x1020e2; the base's result ignored)
    if (this->mKind == 0x3e) {
        EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
        EeGfxEffectFile theFile = EEEffectFile(inFile);
        return ee_gfx_effect_load_damaged(this->mRecord, &theFile, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
    }
    // the explosion has no state of its own: the base's result (EEE2DExplosion::Load)
    if (this->mKind == 4)
        return EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    return this->Call(EeGfxEffect3DOp::Load, NULL, &theFile, inMajorVersion, inMinorVersion);
}

// the light glow sets itself up again after the base's (EEE2DLightGlow::RepairPointers 0xe25a1)
long EEE2DRust::RepairPointers()
{
    if (this->mKind == 0x41)
        return this->Call(EeGfxEffect3DOp::Repair);
    long theResult = EEEffect2D::RepairPointers();
    if (this->mKind == 0xd && theResult == 0)
        theResult = this->SetUp();
    return theResult;
}

long EEE2DRust::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    if (this->mKind == 0x41)
        return this->Call(EeGfxEffect3DOp::Save);
    // the damage controller's (EEEDamagedEffect::Save 0x1021a2; the base's result ignored)
    if (this->mKind == 0x3e) {
        EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
        EeGfxEffectFile theFile = EEEffectFile(outFile);
        return ee_gfx_effect_save_damaged(this->mRecord, &theFile, this->mGraphicEffect->mLinkedEffect2 != NULL);
    }
    if (this->mKind == 4)
        return EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    // the particle is saved where it was drawn (EEE2DParticle::Save 0xe517c)
    GE3DPoint& theDrawn = *reinterpret_cast<GE3DPoint*>(&this->mRecord->particle.draw_position);
    if (this->mKind == 0x2e && this->mPosition != theDrawn)
        this->SetPosition(theDrawn);
    long theBase = EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    // the marching icons stop at the base's error (0xe4182)
    if ((this->mKind == 0xf || this->mKind == 0x10) && theBase != 0)
        return theBase;
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    return this->Call(EeGfxEffect3DOp::Save, NULL, &theFile);
}

// The world the damage controller's Rust tick calls into: its children made and destroyed (from the deleted
// EEEDamagedEffect.cpp)
namespace {
struct EEDamagedHost {
    static EEE2DRust& Self(void* inContext) { return *static_cast<EEE2DRust*>(inContext); }
    static uint32_t Create(void* inContext, bool inSecondary, uint32_t inIndex)
    {
        EEE2DRust& theEffect = Self(inContext);
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
long EEE2DRust::Step()
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

// The battlefield decals' placement (from the deleted EEE2DBattlefieldAmbient.cpp, 0xde374..0xde6e9): the physics
// models' and the units' shell holes and blood stains
static EEGraphicEffect* AmbientPickVariant(EEGraphicEffect* inEffect);
static bool AmbientRollChance(EEGraphicEffect* inEffect);
static EeGfxEffectAmbient* AmbientFindNear(unsigned long inType, GE3DPoint& inLocation);
static bool AmbientScatter(EEGraphicEffect* inEffect, GE3DPoint& ioLocation);

// NEAR: register choice (the original keeps the effect in edi, &mL68 in esi and the ambient in ebx, reloading
// inLocation; ours gives the variant a parameter slot)
// Places up to N shell-hole decals of the unit's class around a point (ambient effects on); stops on a failed roll
// The first one renews a decal already on that spot instead, unless its scatter roll moves it
void EEPlaceImpactDecal(EEComplexUnit* inUnit, GE3DPoint& inLocation)
{
    long i;
    EEGraphicEffect* theVariant;
    GE3DPoint theLocation = inLocation;
    EEGraphicEffect* theEffect;
    if (!gAmbientEffectsEnabled || (theEffect = inUnit->mUnitClass->mShellHoleEffect) == 0)
        return;
    theLocation.SetZ(theLocation.GetZ() + 0.005f);
    for (i = 0; i < theEffect->mIntParam2; i++) {
        theVariant = AmbientPickVariant(theEffect);
        if (!AmbientRollChance(theVariant))
            return;
        if (i == 0) {
            EeGfxEffectAmbient* theAmbient = AmbientFindNear(0, theLocation);
            if (theAmbient && !AmbientScatter(theVariant, theLocation)) {
                theAmbient->age = 0.0f;
                continue;
            }
        } else {
            theLocation.SetX(EEEffect::GetRandomSpread(theEffect->GetParam5(), inLocation.GetX()));
            theLocation.SetY(EEEffect::GetRandomSpread(theEffect->GetParam5(), inLocation.GetY()));
            theLocation.SetZ(gWorld.mMap->GetHeight(theLocation) + 0.005f);
        }
        gEffectManager->CreatePlacedEffect(theLocation, theVariant, NULL);
    }
}

// Places the unit class's blood-stain decal under the unit if ambient effects and blood are on and its roll passes
// (moved forward along the heading for giraffes and horses)
void EEPlaceTrackDecal(EEComplexUnit* inUnit)
{
    GE3DPoint theLocation;
    EEGraphicEffect* theEffect;
    if (!gAmbientEffectsEnabled || !gDrawBlood || (theEffect = inUnit->mUnitClass->mBloodStainEffect) == 0)
        return;
    EEGraphicEffect* theVariant = AmbientPickVariant(theEffect);
    if (!AmbientRollChance(theVariant))
        return;
    inUnit->GetPosition(theLocation);
    unsigned long theType = inUnit->mUnitClass->mUnitType;
    if (theType == kUnitTypeResGiraffe || theType == kUnitTypeResHorse) {
        const GETransformation& theTransformation = inUnit->mPhysicalModel.GetLocation();
        float theAngle = UMArcTan2(theTransformation.mMatrix[1][0], theTransformation.mMatrix[0][0]);
        float theDistance = 0.4f;
        if (theType == kUnitTypeResHorse) {
            theDistance = 0.3f;
            theAngle -= 0.7853982f;
        }
        theLocation.SetX(theLocation.GetX() + UMCos(theAngle) * theDistance);
        theLocation.SetY(theLocation.GetY() + UMCos(1.5707964f - theAngle) * theDistance);
    }
    theLocation.SetZ(theLocation.GetZ() + 0.005f);
    gEffectManager->CreatePlacedEffect(theLocation, theVariant, NULL);
}

// Returns the first battlefield decal of the given type within 0.05 tiles of a point, or NULL
// NEAR: registers only
static EeGfxEffectAmbient* AmbientFindNear(unsigned long inType, GE3DPoint& inLocation)
{
    EEEffect* theEffect = gEffectManager->First();
    while (theEffect) {
        if (theEffect->mKind == kEffect2DBattlefieldAmbient) {
            EeGfxEffectAmbient& theAmbient = theEffect->mRecord->ambient;
            if (theAmbient.kind == inType) {
                GE3DPoint theOffset = inLocation;
                theOffset -= *reinterpret_cast<const GE3DPoint*>(&theAmbient.center);
                if (theOffset.GetX() < 0.05 && theOffset.GetX() > -0.05 &&
                    theOffset.GetY() < 0.05 && theOffset.GetY() > -0.05)
                    return &theAmbient;
            }
        }
        theEffect = gEffectManager->Next(theEffect);
    }
    return NULL;
}

// Rolls the effect's chance (parameter 2); always succeeds when it is 1 or more
static bool AmbientRollChance(EEGraphicEffect* inEffect)
{
    if (inEffect->GetParam2() >= 1.0f)
        return true;
    float theChance = inEffect->GetParam2();
    return EEEffect::GetRandom() <= theChance;
}

// Picks the effect or one of its up to three linked variants, each equally likely
static EEGraphicEffect* AmbientPickVariant(EEGraphicEffect* inEffect)
{
    long theCount;
    if (inEffect->mLinkedEffect3)
        theCount = 4;
    else if (inEffect->mLinkedEffect2)
        theCount = 3;
    else if (inEffect->mLinkedEffect1)
        theCount = 2;
    else
        return inEffect;
    switch (GetEffectRandomLong(1, theCount)) {
    case 2:
        return inEffect->mLinkedEffect1;
    case 3:
        return inEffect->mLinkedEffect2;
    case 4:
        return inEffect->mLinkedEffect3;
    }
    return inEffect;
}

// With the effect's scatter chance (parameter 3), moves the point randomly within parameter 4; returns whether it moved
static bool AmbientScatter(EEGraphicEffect* inEffect, GE3DPoint& ioLocation)
{
    if (EEEffect::GetRandom() >= inEffect->GetParam3())
        return false;
    ioLocation.SetX(ioLocation.GetX() + inEffect->GetParam4() * (0.5f - EEEffect::GetRandom()));
    ioLocation.SetY(ioLocation.GetY() + inEffect->GetParam4() * (0.5f - EEEffect::GetRandom()));
    return true;
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
