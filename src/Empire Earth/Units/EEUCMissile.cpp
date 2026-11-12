// C:\EEX\Empire Earth\Units\EEUCMissile.cpp (file name inferred: no __FILE__; the class name). The missile's unit
// class: 0x217337..0x217c6d (after EEUCMiningBuilding.cpp; EEUCNaval.cpp follows). Unit flags: the exe's module set.
// Its ctor is inline (vtable 0x448cf8); +0x74/+0x78/+0xac are also EEUCTorpedo's (ICF).
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GETransformation.h"
#include "lle/SESoundManager.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUCMissile.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EESoundObject.h"
#include "ee/EEEffectManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMiniMap.h"
#include "ee/EmpireEarth.h"
#include "ee/EECombatFormulas.h"
#include "ee/EECombatGoals.h"

// The pooled words 0x447230/0x447234 (0.3, the height over a target the missile is not on; 2.25, its squared
// proximity to an attack target) are ee_combat's (impact.rs)

// The original first argument reads only the base tile. Its sole ComplexUnit caller does not uniquely
// recover narrower pointer specificity; this declaration records the minimum proven contract.
bool EEIsOnTargetTiles(EEUnit* inUnit, EEUnit* inTarget);

// FUNCTION: EXE 0x617337
// Returns whether the missile hit the ground after its flight time (0.3 higher over its target): sets it down there
// and, for an attack-ground goal, leaves a shell hole and returns true (never for a missile base's missiles)
bool EEUCMissile::CheckGroundImpact(EEComplexUnit* inUnit)
{
    // the test is ee_combat's missile_ground (impact.rs); the placement, its tile and the shell hole stay here
    long theType = inUnit->mLauncherClass->mUnitType;
    EEUnitGoal* theGoal = inUnit->mGoalState;
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    GE3DPoint thePosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    EeCombatImpactIn in = {};
    in.kind = EE_COMBAT_IMPACT_MISSILE_GROUND;
    in.flight_time = (int32_t)inUnit->mFlightTime;
    in.has_goal = theGoal != NULL;
    in.goal_kind = theGoal != NULL ? (int32_t)theGoal->mKind : 0;
    in.launcher_cp_base = theType == kUnitTypeCpMissileBase;
    in.launcher_missile_base = theType == kUnitTypeMissileBase;
    in.shell_hole = inUnit->mUnitClass->mShellHoleEffect != NULL;
    in.position[0] = thePosition.GetX();
    in.position[1] = thePosition.GetY();
    in.position[2] = thePosition.GetZ();
    EeCombatImpactOut theOut = EECombatImpact(this, inUnit, in);
    if (theOut.place) {
        long theX;
        long theY;
        inUnit->PlaceOnGround(thePosition);
        GETransformation theTransformation(false);
        theTransformation = theLocation;
        theTransformation.SetZ(theTransformation.GetZ() + theOut.offset);
        EEUpdateUnitTile(&theTransformation, inUnit, &theX, &theY);
    }
    if (theOut.decal)
        EEPlaceImpactDecal(inUnit, thePosition);
    return theOut.result != 0;
}

// FUNCTION: EXE 0x6174fb
// Returns whether a unit stands on one of the target's tiles (every part of a multi-part target, its whole footprint)
bool EEIsOnTargetTiles(EEUnit* inUnit, EEUnit* inTarget)
{
    EEComplexUnit* theUnit = inTarget->mComplexUnit;
    RTSTile* theTile = inUnit->mTile;
    bool theResult = false;
    if (theUnit != NULL) {
        while (theUnit->mPreviousPart != NULL)
            theUnit = theUnit->mPreviousPart;
        while (theUnit != NULL && !theResult) {
            long theSize = theUnit->mUnitClass->mTileSize;
            if (theSize == 1)
                theResult = theTile == theUnit->mTile;
            else {
                unsigned long theX = theUnit->mTileX;
                unsigned long theEndX = theX + theSize;
                while (theX < theEndX && !theResult) {
                    unsigned long theY = theUnit->mTileY;
                    unsigned long theEndY = theY + theSize;
                    while (true) {
                        if (!(theY < theEndY && !theResult))
                            break;
                        theResult = theTile == ::gWorld.mMap->GetTile(theX, theY);
                        ++theY;
                    }
                    ++theX;
                }
            }
            theUnit = theUnit->mNextPart;
        }
    } else
        theResult = theTile == inTarget->mTile;
    return theResult;
}

// FUNCTION: EXE 0x6175cb
// Returns whether the missile hit water (z 0 or below on terrain it cannot cross); if so, it attacks the ground there
bool EEUCMissile::CheckWaterImpact(EEComplexUnit* inUnit)
{
    // the test is ee_combat's missile_water (impact.rs)
    GE3DPoint thePosition(inUnit->mPhysicalModel.GetLocation());
    EeCombatImpactIn in = {};
    in.kind = EE_COMBAT_IMPACT_MISSILE_WATER;
    in.position[0] = thePosition.GetX();
    in.position[1] = thePosition.GetY();
    in.position[2] = thePosition.GetZ();
    EeCombatImpactOut theOut = EECombatImpact(this, inUnit, in);
    if (theOut.goal == EE_COMBAT_IMPACT_GOAL_WATER)
        inUnit->SetGoal(new EEUGAttackGround(GE3DPoint(theOut.goal_point[0], theOut.goal_point[1], theOut.goal_point[2]), false), 0, 0);
    return theOut.result != 0;
}

// The impact tests' lazy reads (ee/EECombatFormulas.h's EECombatImpact): the unit, the class whose terrain test runs
namespace {

struct EEImpactContext {
    EEUnitClass* mClass;
    EEComplexUnit* mUnit;
};

float EEImpactGroundZ(void* inContext, float inX, float inY, float inZ)
{
    GE3DPoint thePoint(inX, inY, inZ);
    GE3DPoint theGround;
    ::gWorld.mMap->GetGroundPoint(thePoint, theGround);
    return theGround.GetZ();
}

float EEImpactTileHeight(void* inContext)
{
    return ((EEImpactContext*)inContext)->mUnit->mTile->GetHeight();
}

void EEImpactTarget(void* inContext, EeCombatImpactTarget* outTarget)
{
    EEComplexUnit* theUnit = ((EEImpactContext*)inContext)->mUnit;
    EEUnit* theTarget = static_cast<EEUGAttackUnit*>(theUnit->mGoalState)->mTarget;
    EEUnitClass* theClass = theTarget->GetUnitClass();
    outTarget->building = theClass->IsLandBuilding() || theClass->mIsWall;
    outTarget->tile[0] = (int32_t)theTarget->GetTileX();
    outTarget->tile[1] = (int32_t)theTarget->GetTileY();
    outTarget->size = (int32_t)theClass->mTileSize;
    outTarget->unit_tile[0] = (int32_t)theUnit->GetTileX();
    outTarget->unit_tile[1] = (int32_t)theUnit->GetTileY();
}

void EEImpactGoalLocation(void* inContext, float* outPoint)
{
    GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
    ((EEImpactContext*)inContext)->mUnit->mGoalState->GetGoalLocation(theLocation);
    outPoint[0] = theLocation.GetX();
    outPoint[1] = theLocation.GetY();
    outPoint[2] = theLocation.GetZ();
}

int32_t EEImpactOnTargetTiles(void* inContext)
{
    EEComplexUnit* theUnit = ((EEImpactContext*)inContext)->mUnit;
    return EEIsOnTargetTiles(theUnit, static_cast<EEUGAttackUnit*>(theUnit->GetGoal())->mTarget);
}

int32_t EEImpactSameTile(void* inContext, float inX, float inY)
{
    EEComplexUnit* theUnit = ((EEImpactContext*)inContext)->mUnit;
    return ::gWorld.mMap->GetTile(UMFloatToLong(inX), UMFloatToLong(inY)) == theUnit->mTile;
}

int32_t EEImpactCanMove(void* inContext)
{
    EEImpactContext* theContext = (EEImpactContext*)inContext;
    return theContext->mClass->CanMoveOnTerrain(theContext->mUnit, theContext->mUnit->mTile->mTerrainType);
}

}   // namespace

// New (port): ee_combat's impact test over the unit (ee/EECombatFormulas.h); captured as CBGI or CBWI
EeCombatImpactOut EECombatImpact(EEUnitClass* inClass, EEComplexUnit* inUnit, EeCombatImpactIn& in)
{
    EEImpactContext theContext = {inClass, inUnit};
    EeCombatImpactHost theHost = {&theContext,          EEImpactGroundZ,       EEImpactTileHeight, EEImpactTarget,
                                  EEImpactGoalLocation, EEImpactOnTargetTiles, EEImpactSameTile,   EEImpactCanMove};
    EeCombatImpactOut theOut = ee_combat_impact(&in, &theHost);
    bool theWater = in.kind == EE_COMBAT_IMPACT_PROJECTILE_WATER || in.kind == EE_COMBAT_IMPACT_MISSILE_WATER;
    EECombatCapture(theWater ? EE_COMBAT_TAG('C', 'B', 'W', 'I') : EE_COMBAT_TAG('C', 'B', 'G', 'I'), in, theOut);
    return theOut;
}

// FUNCTION: EXE 0x61765d
// Handles the missile's new action: on a hit plays the splash or death effect and sound, fires its shot and dies
// (attack-unit goals, ICBM ground strikes); switches to attack ground; on retarget follows the target's container
void EEUCMissile::ActionChanged(EEComplexUnit* inUnit)
{
    float theVolume;
    float thePan;
    float thePitch;
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GE3DPoint theGround(0.0f, 0.0f, 0.0f);
    EEUnitGoal* theGoal = inUnit->mGoalState;
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionSwitchToAttackGround: {
        GE3DPoint theTarget(0.0f, 0.0f, 0.0f);
        theGoal->GetGoalLocation(theTarget);
        inUnit->SetGoal(new EEUGAttackGround(theTarget, theGoal->GetB1C()), 0, 0);
        inUnit->StartAction(kActionAdvance);
        this->ActionChanged(inUnit);
        break;
    }
    case kActionHitTarget: {
        thePosition.SetX(inUnit->mPhysicalModel.GetLocation().GetX());
        thePosition.SetY(inUnit->mPhysicalModel.GetLocation().GetY());
        thePosition.SetZ(inUnit->mPhysicalModel.GetLocation().GetZ());
        if (thePosition.GetZ() < 0.0f && inUnit->mTile->GetHeight() < 0.0f && theGoal->mKind != kGoalAttackUnit && this->mSplashEffect != NULL) {
            thePosition.SetZ(0.0f);
            gEffectManager->CreatePlacedEffect(thePosition, this->mSplashEffect, NULL);
        } else if (this->mDeathEffect != NULL) {
            if (thePosition.GetZ() < inUnit->mTile->GetHeight())
                ::gWorld.mMap->GetGroundPoint(thePosition, theGround);
            else
                theGround = thePosition;
            // the location's x, y, z set in place, as the original's const_cast stores did (New, R5b.4: through the
            // setter, which reports them to the unit store)
            inUnit->mPhysicalModel.SetLocationXYZ(theGround);
            gEffectManager->CreateUnitEffect(inUnit, this->mDeathEffect, NULL, 0, 0, 0);
        }
        EESoundObject* theSound = inUnit->mStats->mDeathSound;
        if (theSound != NULL) {
            this->GetSoundParameters(inUnit, theSound, &theVolume, &thePan, &thePitch);
            gSoundManager->Play(theSound->mSampleID, theVolume, thePan, thePitch, theSound->mLooping, NULL);
        }
        inUnit->SetCurrentPhysicsAction(2);
        inUnit->SetAnimation(4, -1, 0);
        inUnit->DropPath();
        if (inUnit->mHitPoints > 0)
            this->FireShot(inUnit);
        if (inUnit->mGoalState->mKind == kGoalAttackUnit
            || (inUnit->mUnitClass->mUnitType == kUnitTypeCpICBM && inUnit->mGoalState->mKind == kGoalAttackGround)) {
            inUnit->StartAction(kActionDead);
            this->ActionChanged(inUnit);
            if (inUnit->mUnitClass->GetPopulationCost() > 0)
                inUnit->mPlayer->mPopulationCount -= inUnit->mUnitClass->GetPopulationCost();
            gMiniMap->MoveUnit(inUnit, inUnit->mTileX, inUnit->mTileY, -10000, -10000);
        }
        break;
    }
    case kActionSelectNewTarget: {
        EEUGAttackUnit* theAttack = static_cast<EEUGAttackUnit*>(theGoal);
        if (theAttack->mTarget->mHitPoints < 1) {
            inUnit->StartAction(kActionDying);
            this->ActionChanged(inUnit);
        } else
            theAttack->SetTarget(theAttack->mTarget->mComplexUnit->mContainer);
        break;
    }
    default:
        this->HandleActionChanged(inUnit);
        break;
    }
}

// FUNCTION: EXE 0x617916
// Sets up a new missile: records its start point and stops it; all but the ICBM start the flight animation and advance
void EEUCMissile::UnitCreated(EEComplexUnit* inUnit)
{
    EEUnitClass::UnitCreated(inUnit);
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    inUnit->mInitialContactLocation = GE3DPoint(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    inUnit->SetVelocity(0.0f);
    if (inUnit->mUnitClass->mUnitType == kUnitTypeCpICBM)
        return;
    inUnit->SetAnimation(0xd, -1, 0);
    inUnit->StartAction(kActionAdvance);
    this->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x61799b
// Launches the missile: tilts it to its launcher's launch pitch and starts the launch delay and a 300 s lifetime; true
bool EEUCMissile::LaunchProjectile(EEComplexUnit* inUnit, const GE3DPoint& inLocation)
{
    GETransformation theRotation;
    EEUnitClass* theClass = inUnit->mLauncherClass;
    theRotation.SetOrientationYPR(0.0f, theClass->mLaunchPitch, 0.0f);
    GETransformation theTransformation(false);
    inUnit->mPhysicalModel.GetLocation().Apply(theRotation, theTransformation);
    long theX;
    long theY;
    EEUpdateUnitTile(&theTransformation, inUnit, &theX, &theY);
    inUnit->SetFlightTime(theClass->mMissileDelayTime);
    inUnit->mCloaked = 300000;
    inUnit->SetVelocity(0.0f);
    return true;
}

static EEUnit* EECombatShotTarget(EEComplexUnit* inUnit)   // TEST ONLY (CBTC): the attack-unit goal's target
{
    EEUnitGoal* theGoal = inUnit->mGoalState;
    return theGoal != NULL && theGoal->mKind == kGoalAttackUnit ? static_cast<EEUGAttackUnit*>(theGoal)->mTarget : NULL;
}

// FUNCTION: EXE 0x617a37
// Answers the missile triggers: hit unit (attacks it; also true when its lifetime ran out), ground and water impact,
// rotted or dying done (always with an attack-unit goal), target dead or in a container, new target
long EEUCMissile::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    bool theFlag = false;
    int32_t theAction = EE_COMBAT_CONDITION_NONE;   // TEST ONLY (CBTC)
    EECombatConditionScope theCap(EE_COMBAT_GOAL_MISSILE, inUnit, inEvent, EECombatShotTarget(inUnit));
    switch (inEvent) {
    case kTriggerProjectileHitUnit: {
        EEUnit* theTarget = this->FindImpactUnit(inUnit);
        if (EECombatRead(EE_COMBAT_READ_IMPACT_UNIT, theTarget != NULL)) {
            theAction = EE_COMBAT_CONDITION_ATTACK_IMPACT_UNIT;
            inUnit->SetGoal(new EEUGAttackUnit(inUnit, theTarget, false, false), 0, 0);
            theFlag = true;
        } else if (inUnit->mCloaked == 0)
            theFlag = true;
        break;
    }
    case kTriggerResourcesRottedAway:
    case kTriggerUnitRottedAway:
        if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalAttackUnit)
            theFlag = true;
        else if (EECombatRead(EE_COMBAT_READ_BASE, EEUnitClass::EvaluateCondition(inUnit, inEvent)))
            theFlag = true;
        break;
    case kTriggerDyingAnimationFinished:
        if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalAttackUnit)
            theFlag = true;
        else if (EECombatRead(EE_COMBAT_READ_BASE, EEUnitClass::EvaluateCondition(inUnit, inEvent)))
            theFlag = true;
        break;
    case kTriggerProjectileHitGround:
        theFlag = EECombatRead(EE_COMBAT_READ_GROUND_IMPACT, this->CheckGroundImpact(inUnit));
        break;
    default:
        theResult = EECombatRead(EE_COMBAT_READ_BASE, EEUnitClass::EvaluateCondition(inUnit, inEvent));
        break;
    case kTriggerUnitTargetNoLongerValid:
        if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalAttackUnit) {
            EEUnit* theTarget = static_cast<EEUGAttackUnit*>(inUnit->GetGoal())->mTarget;
            if (theTarget->mHitPoints < 1)
                theFlag = true;
            else if (theTarget->mComplexUnit != NULL && theTarget->mComplexUnit->mContainer != NULL)
                theFlag = true;
            else
                theFlag = false;
        }
        break;
    case kTriggerProjectileHitWater:
        theFlag = EECombatRead(EE_COMBAT_READ_WATER_IMPACT, this->CheckWaterImpact(inUnit));
        break;
    case kTriggerNewTargetSelected:
        theFlag = true;
        break;
    case kTriggerUnitHitSpace:
        break;
    }
    if (theFlag)
        theResult = 1;
    theCap.Commit(theResult, theAction);
    return theResult;
}

// FUNCTION: EXE 0x617b77
// Updates the trail of a living, moving missile when the effect detail allows it
void EEUCMissile::UpdateEffects(EEComplexUnit* inUnit)
{
    if (inUnit->mHitPoints > 0 && this->mTrailEffect != NULL && inUnit->mVelocity != 0.0f && this->mTrailEffect->mDetailLevel <= gEffectDetail)
        this->UpdateTrail(inUnit);
}

// FUNCTION: EXE 0x617bb2
// Steps the missile's physics, effects and sound, and counts down its rotting time, launch delay and lifetime
void EEUCMissile::Update(EEComplexUnit* inUnit)
{
    if (this->mPhysicsModel != NULL && inUnit->mTileX != -10000)
        this->mPhysicsModel->Step(inUnit);
    this->UpdateEffects(inUnit);
    this->RefreshSound(inUnit);
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) == kActionUnitRotting) {
        long theTime = inUnit->mRottingAwayTime - gPhysics->mTickTimeMs;
        if (theTime < 0)
            theTime = 0;
        inUnit->SetRottingAwayTime(theTime);
    }
    long theElapsed = gPhysics->mTickTimeMs;
    if (theElapsed == 0)
        theElapsed = 1;
    if (inUnit->mFlightTime != 0) {
        long theTime = inUnit->mFlightTime - theElapsed;
        if (theTime < 0)
            theTime = 0;
        inUnit->SetFlightTime(theTime);
    }
    if ((long)inUnit->mCloaked <= 0)
        return;
    long theTime = inUnit->mCloaked - theElapsed;
    if (theTime < 0)
        theTime = 0;
    inUnit->mCloaked = theTime;
}

// ICF-FOLDED: EXE 0x556dd8
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEUCMissile::AddToRegion(class EEUnit *)
{
}

// ICF-FOLDED: EXE 0x556dd8
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEUCMissile::RemoveFromRegion(class EEUnit *)
{
}
