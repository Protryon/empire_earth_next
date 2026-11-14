// The 2D effects whole in Rust (docs/RUST_PLAN.md R4.4; ee_gfx effect_2d.rs, effect_confirm.rs, effect_shockwave.rs,
// effect_sprite.rs): EEE2DRust's virtuals call Rust by the record's kind and answer its questions about the world here.
// Replaces EEE3DCommandConfirmation.cpp, EEE2DShockwave.cpp, EEE2DImpact.cpp (but EETagPointList::Scale, below),
// EEE2DImpactBlood.cpp, EEE2DMuzzleFlash.cpp, EEE2DLightning.cpp, EEE2DPlasmaPulse.cpp, EEE2DPulseWave.cpp,
// EEE2DWaterRipple.cpp, EEE2DFireTrail.cpp, EEE2DExplosion.cpp, EEE2DFire.cpp, EEE2DExhaustTrail.cpp, EEE2DSmokePlume.cpp
// and EEE2DSmokeMuzzleBlast.cpp.
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
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEE3DRust.h"
#include "ee/EEE2DRust.h"
#include "ee/EECombatFormulas.h"
#include "rust/ee_combat.h"

namespace {
// the world's answers for one call
struct Ask2D {
    EEE2DRust* mEffect;
    EEEffectQueue* mQueues;
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
};

// the kind's shared model: the shockwave's, confirmation's, pulse wave's, ripple's and vortex's 0x4e8df4, the
// others' 0x4e8df8
GEModel* EE2DModel(long inKind)
{
    return inKind == 0x12 || inKind == 0x41 || inKind == 0x2f || inKind == 0x22 || inKind == 0x31 ? gU4E8DF4
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

// at a point (EEE2DShockwave 0xe61e2, EEE2DImpact 0xe10af, ...): the point for Rust in the record's sprite.start
EEE2DRust::EEE2DRust(const GE3DPoint& inPosition, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(0, inGraphicEffect, EE2DModel(inKind), inKind, false)
{
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

// from the shooter's weapon tag point (turret-aware) to the target's model point on its scaled location, 15 higher for
// a high-arc shooter (EEE2DSmokeMuzzleBlast 0xe6664): the two ends for Rust
EEE2DRust::EEE2DRust(EEUnit* inUnit, EEUnit* inTarget, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(0, inGraphicEffect, EE2DModel(inKind), inKind, false)
{
    static const float kTargetLift = 15.0f;   // 0x440014
    GE3DPoint theStart;
    GE3DPoint theEnd;
    GE3DPoint theOffset;
    GETransformation theTargetTransformation(inTarget->mComplexUnit->mPhysicalModel.GetLocation());
    EEModel* theModel = inUnit->GetModel();
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    EETagPointList* theList = inUnit->GetModel()->mTagPoints[theUnit->mAnimation];
    float theTargetScale = inTarget->mComplexUnit->mPhysicalModel.GetScale();
    EECombatEffectWeaponPoint(4, theUnit, theModel, theList, true, theStart);
    EETagPointList::Scale(theTargetTransformation, theTargetScale);
    theOffset = inTarget->GetModel()->mDefaultPoint;
    theTargetTransformation.Apply(theOffset, theEnd);
    if (inUnit->mUnitClass->mShootsHigh || (inUnit->mStats->mCanFireOverWalls && inUnit->mComplexUnit->mIsFiringHigh))
        theEnd.SetZ(theEnd.GetZ() + kTargetLift);
    this->mRecord->sprite.start = EeGe3DPoint{theStart.mX, theStart.mY, theStart.mZ};
    this->mRecord->sprite.draw_position = EeGe3DPoint{theEnd.mX, theEnd.mY, theEnd.mZ};
    this->Call(EeGfxEffect3DOp::CreateBetween);
}

// to a point (EEE2DSmokeMuzzleBlast 0xe6a07)
EEE2DRust::EEE2DRust(EEUnit* inUnit, const GE3DPoint& inTarget, EEGraphicEffect* inGraphicEffect, long inKind)
    : EEEffect2D(0, inGraphicEffect, EE2DModel(inKind), inKind, false)
{
    GE3DPoint theStart;
    EEModel* theModel = inUnit->GetModel();
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    EETagPointList* theList = inUnit->GetModel()->mTagPoints[theUnit->mAnimation];
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
                                 Ask2D::MaxHitPoints, Ask2D::UnitPoint};
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
    // the explosion has no state of its own: the base's result (EEE2DExplosion::Load)
    if (this->mKind == 4)
        return EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    EEEffect2D::Load(inMajorVersion, inMinorVersion, inFile);
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    return this->Call(EeGfxEffect3DOp::Load, NULL, &theFile, inMajorVersion, inMinorVersion);
}

long EEE2DRust::RepairPointers()
{
    if (this->mKind == 0x41)
        return this->Call(EeGfxEffect3DOp::Repair);
    return EEEffect2D::RepairPointers();
}

long EEE2DRust::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    if (this->mKind == 0x41)
        return this->Call(EeGfxEffect3DOp::Save);
    if (this->mKind == 4)
        return EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    EEEffect2D::Save(inMajorVersion, inMinorVersion, outFile);
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    return this->Call(EeGfxEffect3DOp::Save, NULL, &theFile);
}

// FUNCTION: EXE 0x4e1814
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
