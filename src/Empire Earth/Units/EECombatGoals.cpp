// New (port): the plain reads of the goals' and shots' trigger tests and of the goals' steps (ee/EECombatGoals.h;
// rust/crates/ee_combat/src/goals.rs; docs/RUST_PLAN.md R5b.2, sixth round), the hosts of the Rust tests and steps
// (each lazy read and each operation the original's statement), and their CBTC/CBGS capture records.
// Every read here is a field or a trivial accessor: no side effect, no random draw, nothing the test would not
// survive reading early; a missing unit, target, player or class leaves its fields 0.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mutex>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUGHealUnit.h"
#include "ee/EEUCProjectile.h"
#include "ee/EEUCMissile.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECombatFormulas.h"
#include "ee/EECombatGoals.h"
#include "ee/EEUnitKind.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEUCLandable.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUnitSetManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/RTSRegionTable.h"
#include "ee/RTSDummyHeader.h"

void EECombatFillCondition(EeCombatConditionIn& in, int32_t inKind, EEComplexUnit* inUnit, unsigned long inEvent,
                           EEUnit* inTarget)
{
    memset(&in, 0, sizeof(in));
    in.kind = inKind;
    in.event = (uint32_t)inEvent;
    in.unit_id = (uint32_t)inUnit->mUnitID;
    EEUnitGoal* theGoal = inUnit->mGoalState;
    in.goal_kind = theGoal != NULL ? (int32_t)theGoal->mKind : -1;
    EEPlayer* thePlayer = inUnit->mPlayer;
    in.human = thePlayer != NULL && thePlayer->IsHuman();
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    if (theStats != NULL) {
        in.range = theStats->mRange;
        in.reload_time = (int32_t)theStats->mReloadTime;
        in.fire_over_walls = theStats->mCanFireOverWalls;
        in.flight_time_max = (int32_t)theStats->mFlightTime;
    }
    in.reload_timer = (int32_t)inUnit->mReloadTimer;
    in.herd = inUnit->mHerd != NULL;
    EEUnitClass* theClass = inUnit->mUnitClass;
    if (theClass != NULL) {
        in.crusader = theClass->IsCrusader();
        in.paratrooper_bomber = theClass->mIsParatrooperBomber;
        in.heal_threshold = theClass->GetHealThreshold();
    }
    in.cloaked = (uint32_t)inUnit->mCloaked;
    in.flight_time = (int32_t)inUnit->mFlightTime;
    if (inTarget == NULL)
        return;
    in.has_target = 1;
    in.target_id = (uint32_t)inTarget->mUnitID;
    in.target_hp = (int32_t)inTarget->mHitPoints.Get();
    in.target_tile[0] = (int32_t)inTarget->mTileX.Get();
    in.target_tile[1] = (int32_t)inTarget->mTileY.Get();
    EEComplexUnit* theComplex = inTarget->mComplexUnit;
    if (theComplex != NULL) {
        in.target_complex = 1;
        in.target_velocity = theComplex->mVelocity;
        in.target_cloakable = theComplex->mIsCloakable;
        in.target_cloaked = theComplex->GetIsCloaked();
        in.target_container = theComplex->mContainer != NULL;
    }
    EEPlayer* theTargetPlayer = inTarget->mPlayer;
    in.target_same_player = theTargetPlayer == thePlayer;
    if (theTargetPlayer != NULL && thePlayer != NULL) {
        in.target_enemy = thePlayer->mEnemies[theTargetPlayer->mPlayerID] != 0;
        in.target_enemy_of = theTargetPlayer->mEnemies[thePlayer->mPlayerID] != 0;
    }
    EEUnitClass* theTargetClass = inTarget->mUnitClass;
    if (theTargetClass != NULL) {
        in.target_cloak_radius = (int32_t)theTargetClass->mCloakRadius;
        in.target_family = (uint32_t)theTargetClass->mFamily;
        in.target_category = (uint32_t)theTargetClass->mCategory;
        in.target_land_building = theTargetClass->IsLandBuilding();
    }
    if (inTarget->mStats != NULL) {
        in.target_attack = inTarget->mStats->mAttack;
        in.target_max_hp = (int32_t)inTarget->mStats->mMaxHitPoints;
    }
}

void EECombatFillStep(EeCombatStepIn& in, int32_t inKind, EEComplexUnit* inUnit, EEUnit* inTarget)
{
    memset(&in, 0, sizeof(in));
    in.kind = inKind;
    EEUnitAction* theCommand = inUnit->mCommand;
    in.command = theCommand != NULL ? (uint32_t)theCommand->mType : 0;
    in.unit_id = (uint32_t)inUnit->mUnitID;
    in.target_id = inTarget != NULL ? (uint32_t)inTarget->mUnitID : 0;
    if (inUnit->mStats != NULL) {
        in.reload_time = (int32_t)inUnit->mStats->mReloadTime;
        in.fire_over_walls = inUnit->mStats->mCanFireOverWalls;
    }
}

namespace {

const unsigned int kConditionTag = EE_COMBAT_TAG('C', 'B', 'T', 'C');
const uint32_t kConditionBatch = 4096;
std::mutex sConditionLock;
uint32_t sConditionCount = 0;
uint64_t sConditionHash = 0xcbf29ce484222325ull;

void EEConditionFold(const void* inData, size_t inSize)
{
    const unsigned char* theBytes = (const unsigned char*)inData;
    for (size_t i = 0; i < inSize; i++)
        sConditionHash = (sConditionHash ^ theBytes[i]) * 0x100000001b3ull;
}

// under sConditionLock
void EEConditionWrite()
{
    EESimCaptureRecord theRecord(kConditionTag);
    theRecord.U32(sConditionCount);
    theRecord.U32((uint32_t)sConditionHash);
    theRecord.U32((uint32_t)(sConditionHash >> 32));
    theRecord.Commit();
    sConditionCount = 0;
    sConditionHash = 0xcbf29ce484222325ull;
}

bool EEConditionFull()
{
    static const bool sFull = getenv("EE_SIM_CAPTURE_CBTC_FULL") != NULL;
    return sFull;
}

}   // namespace

void EECombatCaptureCondition(const EeCombatConditionIn& in, const EeCombatConditionOut& inOut)
{
    if (!EESimCaptureRecord::Active())
        return;
    if (EEConditionFull()) {
        EECombatCapture(kConditionTag, in, inOut);
        return;
    }
    if (!EESimCaptureRecord::Keeps(kConditionTag))
        return;
    std::lock_guard<std::mutex> theGuard(sConditionLock);
    EEConditionFold(&in, sizeof(in));
    EEConditionFold(&inOut, sizeof(inOut));
    if (++sConditionCount == kConditionBatch)
        EEConditionWrite();
}

namespace {

// A folded tag: inBatch calls to a record (count, FNV-1a 64 of each call's bytes in order)
struct EECombatFolder {
    unsigned int mTag;
    const char* mFullName;
    std::mutex mLock;
    int mFull = -1;
    uint32_t mCount = 0;
    uint64_t mHash = 0xcbf29ce484222325ull;

    EECombatFolder(unsigned int inTag, const char* inFullName) : mTag(inTag), mFullName(inFullName) {}

    // under mLock
    void Write()
    {
        EESimCaptureRecord theRecord(this->mTag);
        theRecord.U32(this->mCount);
        theRecord.U32((uint32_t)this->mHash);
        theRecord.U32((uint32_t)(this->mHash >> 32));
        theRecord.Commit();
        this->mCount = 0;
        this->mHash = 0xcbf29ce484222325ull;
    }

    void Add(const void* inData, size_t inSize)
    {
        if (!EESimCaptureRecord::Active())
            return;
        if (this->mFull < 0)
            this->mFull = getenv(this->mFullName) != NULL;
        if (this->mFull) {
            EESimCaptureRecord theRecord(this->mTag);
            theRecord.Bytes(inData, (unsigned int)inSize);
            theRecord.Commit();
            return;
        }
        if (!EESimCaptureRecord::Keeps(this->mTag))
            return;
        std::lock_guard<std::mutex> theGuard(this->mLock);
        const unsigned char* theBytes = (const unsigned char*)inData;
        for (size_t i = 0; i < inSize; i++)
            this->mHash = (this->mHash ^ theBytes[i]) * 0x100000001b3ull;
        if (++this->mCount == 4096)
            this->Write();
    }

    void Flush()
    {
        std::lock_guard<std::mutex> theGuard(this->mLock);
        if (this->mCount != 0)
            this->Write();
    }
};

EECombatFolder sHostReads(EE_COMBAT_TAG('C', 'B', 'Q', 'R'), "EE_SIM_CAPTURE_CBQR_FULL");
EECombatFolder sShots(EE_COMBAT_TAG('C', 'B', 'S', 'O'), "EE_SIM_CAPTURE_CBSO_FULL");
thread_local int sHostReadDepth[4] = {0, 0, 0, 0};

uint32_t EEShotBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

}   // namespace

EECombatHostReadScope::EECombatHostReadScope(int inSlot) : mSlot(inSlot), mOuter(sHostReadDepth[inSlot]++ == 0) {}
EECombatHostReadScope::~EECombatHostReadScope() { sHostReadDepth[this->mSlot]--; }

uint32_t EECombatCommandOf(const EEComplexUnit* inUnit)
{
    return inUnit->mCommand != NULL ? (uint32_t)inUnit->mCommand->mType : 0;
}

void EECombatCaptureShot(uint32_t inFunc, const EEComplexUnit* inUnit, uint32_t inBefore)
{
    if (!EESimCaptureRecord::Active())
        return;
    uint32_t theRecord[22];
    memset(theRecord, 0, sizeof(theRecord));
    theRecord[0] = inFunc;
    theRecord[1] = (uint32_t)inUnit->mUnitID;
    theRecord[2] = inBefore;
    theRecord[3] = EECombatCommandOf(inUnit);
    theRecord[4] = (uint32_t)inUnit->mAnimation;
    theRecord[5] = (uint32_t)inUnit->mCurrentPhysicsAction;
    theRecord[6] = (uint32_t)inUnit->mHitPoints.Get();
    EEUnitGoal* theGoal = inUnit->mGoalState;
    theRecord[7] = theGoal != NULL ? (uint32_t)theGoal->mKind : 0xffffffffu;
    if (theGoal != NULL && theGoal->mKind == kGoalAttackUnit) {
        EEUnit* theTarget = static_cast<EEUGAttackUnit*>(theGoal)->mTarget;
        theRecord[8] = theTarget != NULL ? (uint32_t)theTarget->mUnitID : 0;
    } else if (theGoal != NULL && theGoal->mKind == kGoalAttackGround) {
        const GE3DPoint& thePoint = static_cast<EEUGAttackGround*>(theGoal)->mTarget;
        theRecord[9] = EEShotBits(thePoint.GetX());
        theRecord[10] = EEShotBits(thePoint.GetY());
        theRecord[11] = EEShotBits(thePoint.GetZ());
    }
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    theRecord[12] = EEShotBits(theLocation.GetX());
    theRecord[13] = EEShotBits(theLocation.GetY());
    theRecord[14] = EEShotBits(theLocation.GetZ());
    theRecord[15] = (uint32_t)inUnit->mRottingAwayTime;
    theRecord[16] = (uint32_t)inUnit->mFlightTime;
    theRecord[17] = (uint32_t)inUnit->mCloaked;
    theRecord[18] = EEShotBits(inUnit->mOpacity);
    theRecord[19] = EEShotBits(inUnit->mVelocity);
    theRecord[20] = inUnit->mPlayer != NULL ? (uint32_t)inUnit->mPlayer->mPopulationCount : 0;
    theRecord[21] = (uint32_t)inUnit->mTileX.Get();
    sShots.Add(theRecord, sizeof(theRecord));
}

void EECombatCaptureHealWork(const EEComplexUnit* inUnit, const EEUnit* inTarget, unsigned long inElapsed)
{
    if (!EESimCaptureRecord::Active())
        return;
    uint32_t theRecord[5] = {EE_COMBAT_SO_HEAL_WORK, (uint32_t)inUnit->mUnitID,
                             inTarget != NULL ? (uint32_t)inTarget->mUnitID : 0,
                             inTarget != NULL ? (uint32_t)inTarget->mHitPoints.Get() : 0, (uint32_t)inElapsed};
    sShots.Add(theRecord, sizeof(theRecord));
}

void EECombatCaptureHostRead(uint32_t inFunc, const EEComplexUnit* inUnit, const EEUnit* inTarget, int32_t inResult,
                             uint32_t inExtra0, uint32_t inExtra1, uint32_t inExtra2)
{
    if (!EESimCaptureRecord::Active())
        return;
    uint32_t theRecord[7] = {inFunc, inUnit != NULL ? (uint32_t)inUnit->mUnitID : 0,
                             inTarget != NULL ? (uint32_t)inTarget->mUnitID : 0, (uint32_t)inResult,
                             inExtra0, inExtra1, inExtra2};
    sHostReads.Add(theRecord, sizeof(theRecord));
}

void EECombatCaptureFlush()
{
    if (!EESimCaptureRecord::Active())
        return;
    {
        std::lock_guard<std::mutex> theGuard(sConditionLock);
        if (sConditionCount != 0)
            EEConditionWrite();
    }
    sHostReads.Flush();
    sShots.Flush();
}

void EECombatCaptureStep(const EeCombatStepIn& in)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord theRecord(EE_COMBAT_TAG('C', 'B', 'G', 'S'));
    theRecord.Bytes(&in, sizeof(in));
    theRecord.Commit();
}

// ---- the hosts of ee_combat's trigger tests and steps

namespace {

struct EEConditionContext {
    int32_t mKind;
    unsigned long mEvent;
    EEComplexUnit* mUnit;
    EEUnitGoal* mGoal;                      // the goals' (attack unit, attack ground, heal)
    EEUnitClass* mClass;                    // the shots'
    EEUnit* mImpact;                        // FindImpactUnit's answer, kept for the targetable read and the action
};

uint32_t EEConditionBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

// the attack goal's target (attack unit), the heal goal's (heal)
EEUnit* EEConditionTarget(EEConditionContext* inContext)
{
    if (inContext->mKind == EE_COMBAT_GOAL_HEAL)
        return static_cast<EEUGHealUnit*>(inContext->mGoal)->mTarget;
    return static_cast<EEUGAttackUnit*>(inContext->mGoal)->mTarget;
}

// The lazy reads, each the original's call at the place the test makes it (ee_combat goals.rs EE_COMBAT_READ_*)
uint32_t EEConditionRead(void* inContext, int32_t inCode)
{
    EEConditionContext* theContext = (EEConditionContext*)inContext;
    EEComplexUnit* theUnit = theContext->mUnit;
    switch (inCode) {
    case EE_COMBAT_READ_LINE_OF_SIGHT:
        return EEConditionBits(theUnit->GetLineOfSight());
    case EE_COMBAT_READ_IN_RANGE_TO_HIT:
        return EEIsInRangeToHit(theUnit, EEConditionTarget(theContext));
    case EE_COMBAT_READ_IN_REACH:
        return theUnit->IsInReach(EEConditionTarget(theContext));
    case EE_COMBAT_READ_SEEN:
        if (theContext->mKind == EE_COMBAT_GOAL_HEAL)
            return EEIsSeenBy(EEConditionTarget(theContext), theUnit->GetPlayer(), true);
        return EEIsSeenBy(EEConditionTarget(theContext), theUnit->mPlayer, false);
    case EE_COMBAT_READ_FACES_ARCH:
        return theUnit->GetUnitClass()->FacesTarget(theUnit, EEConditionTarget(theContext), true);
    case EE_COMBAT_READ_FACES:
        return theUnit->mUnitClass->FacesTarget(theUnit, EEConditionTarget(theContext), false);
    case EE_COMBAT_READ_FACES_MOVING: {
        EEComplexUnit* theComplexUnit = EEConditionTarget(theContext)->mComplexUnit;
        return theUnit->GetUnitClass()->FacesTarget(theUnit, theComplexUnit, true);
    }
    case EE_COMBAT_READ_DISTANCE_SQUARED:
        return EEConditionBits(theUnit->DistanceSquaredToUnit(EEConditionTarget(theContext)));
    case EE_COMBAT_READ_BORDERING:
        return theUnit->IsOnTileBorderingTarget(EEConditionTarget(theContext));
    case EE_COMBAT_READ_LINE_BLOCKED: {
        EEUnit* theArg3 = NULL;
        long theArg4;
        long theArg5;
        return EEIsLineToUnitBlocked(theUnit, EEConditionTarget(theContext), &theArg3, &theArg4, &theArg5);
    }
    case EE_COMBAT_READ_GATE_CLOSED:
        return static_cast<EEUGAttackUnit*>(theContext->mGoal)->IsGateClosed(theUnit);
    case EE_COMBAT_READ_STILL_REACHABLE:
        return static_cast<EEUGAttackUnit*>(theContext->mGoal)->IsStillReachable(theUnit);
    case EE_COMBAT_READ_ENEMY_IN_RANGE:
        return EEIsEnemyWithinRange(theUnit);
    case EE_COMBAT_READ_BASE:
        switch (theContext->mKind) {
        case EE_COMBAT_GOAL_HEAL:
            return static_cast<EEUGHealUnit*>(theContext->mGoal)->EEUGLocation::EvaluateCondition(theUnit, theContext->mEvent);
        case EE_COMBAT_GOAL_TORPEDO:
            return (uint32_t)static_cast<EEUCMissile*>(theContext->mClass)->EEUCMissile::EvaluateCondition(theUnit, theContext->mEvent);
        default:
            return (uint32_t)theContext->mClass->EEUnitClass::EvaluateCondition(theUnit, theContext->mEvent);
        }
    case EE_COMBAT_READ_CAN_REACH_TILE: {
        const GE3DPoint& theTarget = static_cast<EEUGAttackGround*>(theContext->mGoal)->mTarget;
        return theUnit->CanReachTile(UMFloor(theTarget.GetX()), UMFloor(theTarget.GetY()));
    }
    case EE_COMBAT_READ_FACES_POINT:
        return theUnit->GetUnitClass()->FacesPoint(theUnit, static_cast<EEUGAttackGround*>(theContext->mGoal)->mTarget, true);
    case EE_COMBAT_READ_HEAL_IN_RANGE: {
        EEUGHealUnit* theGoal = static_cast<EEUGHealUnit*>(theContext->mGoal);
        return theGoal->IsInRange(theUnit, theGoal->mTarget);
    }
    case EE_COMBAT_READ_REPAIRABLE:
        return static_cast<EEUGHealUnit*>(theContext->mGoal)->mTarget->IsRepairable();
    case EE_COMBAT_READ_IMPACT_UNIT:
        theContext->mImpact = theContext->mClass->FindImpactUnit(theUnit);
        return theContext->mImpact != NULL;
    case EE_COMBAT_READ_TARGETABLE:
        return EEUUGetIsUnitClassTargetable(theUnit, theContext->mImpact->GetUnitClass());
    case EE_COMBAT_READ_GROUND_IMPACT:
        if (theContext->mKind == EE_COMBAT_GOAL_PROJECTILE)
            return static_cast<EEUCProjectile*>(theContext->mClass)->CheckGroundImpact(theUnit);
        return static_cast<EEUCMissile*>(theContext->mClass)->CheckGroundImpact(theUnit);
    case EE_COMBAT_READ_WATER_IMPACT:
        if (theContext->mKind == EE_COMBAT_GOAL_PROJECTILE)
            return static_cast<EEUCProjectile*>(theContext->mClass)->CheckWaterImpact(theUnit);
        return static_cast<EEUCMissile*>(theContext->mClass)->CheckWaterImpact(theUnit);
    }
    return 0;
}

struct EEStepContext {
    int32_t mKind;
    EEComplexUnit* mUnit;
    EEUnitGoal* mGoal;
};

// The step's operations, each the original's statement (ee_combat goals.rs EE_COMBAT_STEP_*); reads answer
int32_t EEStepOp(void* inContext, int32_t inOp, int32_t inArg)
{
    EEStepContext* theContext = (EEStepContext*)inContext;
    EEComplexUnit* theUnit = theContext->mUnit;
    switch (inOp) {
    case EE_COMBAT_STEP_PHYSICS_ACTION:
        theUnit->SetCurrentPhysicsAction((unsigned long)inArg);
        break;
    case EE_COMBAT_STEP_FACES:
        if (theContext->mKind == EE_COMBAT_GOAL_HEAL)
            return theUnit->GetUnitClass()->FacesTarget(theUnit, static_cast<EEUGHealUnit*>(theContext->mGoal)->mTarget, false);
        return theUnit->mUnitClass->FacesTarget(theUnit, static_cast<EEUGAttackUnit*>(theContext->mGoal)->mTarget, false);
    case EE_COMBAT_STEP_ANIMATION:
        return (int32_t)theUnit->mAnimation;
    case EE_COMBAT_STEP_SET_ANIMATION:
        theUnit->SetAnimation(inArg, -1, 0);
        break;
    case EE_COMBAT_STEP_DROP_PATH:
        theUnit->DropPath();
        break;
    case EE_COMBAT_STEP_RETARGET_GATE:
        static_cast<EEUGAttackUnit*>(theContext->mGoal)->RetargetGatePart(theUnit);
        break;
    case EE_COMBAT_STEP_ATTACK_FRAME_SET:
        return (int32_t)EEGetAttackFrameSet(theUnit);
    case EE_COMBAT_STEP_SOUND_FRAME:
        return (int32_t)theUnit->GetModel()->mSound1Frames[inArg];
    case EE_COMBAT_STEP_FRAME:
        return (int32_t)theUnit->mPhysicalModel.GetFrame();
    case EE_COMBAT_STEP_FIRE_SHOT:
        theUnit->mUnitClass->FireShot(theUnit);
        break;
    case EE_COMBAT_STEP_RELOAD_TIMER:
        theUnit->SetRandomReloadTimer(inArg);
        break;
    case EE_COMBAT_STEP_SEARCH_TIMER:
        theUnit->SetTargetSearchTimer(inArg);
        break;
    case EE_COMBAT_STEP_PATH_ATTEMPTS:
        theContext->mGoal->SetPathAttempts(inArg);
        break;
    case EE_COMBAT_STEP_FIRING_HIGH:
        return theUnit->mIsFiringHigh;
    }
    return 0;
}

}   // namespace

EEUnit* EECombatShotTarget(EEComplexUnit* inUnit)
{
    EEUnitGoal* theGoal = inUnit->mGoalState;
    return theGoal != NULL && theGoal->mKind == kGoalAttackUnit ? static_cast<EEUGAttackUnit*>(theGoal)->mTarget : NULL;
}

long EECombatCondition(EeCombatConditionIn& in, EEComplexUnit* inUnit, EEUnitGoal* inGoal, EEUnitClass* inClass)
{
    EEConditionContext theContext = {in.kind, in.event, inUnit, inGoal, inClass, NULL};
    EeCombatConditionHost theHost = {&theContext, EEConditionRead};
    EeCombatConditionOut theOut = ee_combat_condition(&in, &theHost);
    // the action before the record, as the original's body set the goal before its end
    if (theOut.action == EE_COMBAT_CONDITION_ATTACK_IMPACT_UNIT)
        inUnit->SetGoal(new EEUGAttackUnit(inUnit, theContext.mImpact, false, false), 0, 0);
    EECombatCaptureCondition(in, theOut);
    return theOut.result;
}

void EECombatStep(int32_t inKind, EEComplexUnit* inUnit, EEUnitGoal* inGoal, EEUnit* inTarget)
{
    EeCombatStepIn in;
    EECombatFillStep(in, inKind, inUnit, inTarget);
    EEStepContext theContext = {inKind, inUnit, inGoal};
    EeCombatStepHost theHost = {&theContext, EEStepOp};
    ee_combat_goal_step(&in, &theHost);
    EECombatCaptureStep(in);
}

// ---- the host of ee_combat's reads (reads.rs): FindImpactUnit, FacesPoint, IsInReach, IsStillReachable,
// IsGateClosed, EEIsEnemyWithinRange. Every handle an EEUnit* (a player's an EEPlayer*), 0 for none; each read the
// original's expression on it.

namespace {

struct EEQueryContext {
    EEComplexUnit* mUnit;                   // the call's unit
    EEUGAttackUnit* mGoal;                  // IsStillReachable's, IsGateClosed's attack goal
    std::vector<EEComplexUnit*>* mRegion;   // FindImpactUnit's region, found by its count read
};

EEUnit* EEQueryUnit(uint64_t inHandle)
{
    return (EEUnit*)(uintptr_t)inHandle;
}

// a handle whose complex members are read (every one the original reads them of is a complex unit)
EEComplexUnit* EEQueryComplex(uint64_t inHandle)
{
    return static_cast<EEComplexUnit*>(EEQueryUnit(inHandle));
}

EEPlayer* EEQueryPlayer(uint64_t inHandle)
{
    return (EEPlayer*)(uintptr_t)inHandle;
}

uint64_t EEQueryHandleOf(const EEUnit* inUnit)
{
    return (uint64_t)(uintptr_t)inUnit;
}

uint32_t EEQueryRead(void* inContext, int32_t inCode, uint64_t inObject, int32_t inArg)
{
    EEQueryContext* theContext = (EEQueryContext*)inContext;
    EEComplexUnit* theUnit = theContext->mUnit;
    EEUnit* theObject = EEQueryUnit(inObject);
    switch (inCode) {
    case EE_COMBAT_Q_TILE_X:
        return (uint32_t)theObject->GetTileX();
    case EE_COMBAT_Q_TILE_Y:
        return (uint32_t)theObject->GetTileY();
    case EE_COMBAT_Q_HIT_POINTS:
        return (uint32_t)theObject->GetHitPoints();
    case EE_COMBAT_Q_MAX_HIT_POINTS:
        return (uint32_t)theObject->GetStats()->mMaxHitPoints;
    case EE_COMBAT_Q_STATS_SCALE:
        return EEConditionBits(theObject->GetStats()->GetScale());
    case EE_COMBAT_Q_MODEL:
        return EEQueryComplex(inObject)->mPhysicalModel.GetModel() != NULL;
    case EE_COMBAT_Q_BUILT:
        return EEQueryComplex(inObject)->mIsBuilt;
    case EE_COMBAT_Q_UNIT_TYPE:
        return (uint32_t)theObject->mUnitClass->GetUnitType();
    case EE_COMBAT_Q_CATEGORY:
        return (uint32_t)theObject->mUnitClass->GetCategory();
    case EE_COMBAT_Q_CLASS_FLAGS: {
        if (theObject == NULL)
            return 0;
        EEUnitClass* theClass = theObject->mUnitClass;
        return (theClass->mIsWall ? EE_COMBAT_CLASS_WALL : 0) | (theClass->mIsProjectile ? EE_COMBAT_CLASS_PROJECTILE : 0)
            | (theClass->mIsParatrooperBomber ? EE_COMBAT_CLASS_PARATROOPER_BOMBER : 0)
            | (theClass->mIsMelee ? EE_COMBAT_CLASS_MELEE : 0);
    }
    case EE_COMBAT_Q_TARGETABLE:
        return EEUUGetIsUnitClassTargetable(inArg ? theUnit : NULL, theObject->mUnitClass);
    case EE_COMBAT_Q_PLAYER_ID:
        return (uint32_t)EEQueryPlayer(inObject)->GetPlayerID();
    case EE_COMBAT_Q_ENEMY:
        return EEQueryPlayer(inObject)->mEnemies[inArg] != 0;
    case EE_COMBAT_Q_FIRING_HIGH:
        return EEQueryComplex(inObject)->mIsFiringHigh;
    case EE_COMBAT_Q_FIRE_OVER_WALLS:
        return theObject->GetStats()->mCanFireOverWalls;
    case EE_COMBAT_Q_PASSABLE:
        return EEQueryComplex(inObject)->IsPassableBy(theUnit);
    case EE_COMBAT_Q_UNIT_SET:
        return gUnitSetManager.Contains(theObject->mUnitClass->GetUnitType(), (unsigned long)inArg);
    case EE_COMBAT_Q_SCALE:
        return EEConditionBits(EEQueryComplex(inObject)->mPhysicalModel.GetScale());
    case EE_COMBAT_Q_COMMAND: {
        EEComplexUnit* theComplex = EEQueryComplex(inObject);
        return theComplex->mCommand ? (uint32_t)theComplex->mCommand->mType : 0;
    }
    case EE_COMBAT_Q_SIDE_PART:
        return EEUnitClassCast<EEUCGate>(theObject->GetUnitClass())->IsSidePart(theObject->mComplexUnit);
    case EE_COMBAT_Q_PATH_ATTEMPTS:
        return (uint32_t)theContext->mGoal->mPathAttempts;
    case EE_COMBAT_Q_CAN_REACH_UNIT:
        return theUnit->CanReachUnit(theObject);
    case EE_COMBAT_Q_ATTACK_REACHABLE: {
        U2DPoint<long> theTile;
        return EEIsAttackReachable(theUnit, EEQueryComplex(inObject), &theTile);
    }
    case EE_COMBAT_Q_IGNORED:
        return theUnit->IsTargetIgnored(EEQueryComplex(inObject), true);
    case EE_COMBAT_Q_SEARCH_TIMER:
        return (uint32_t)EEQueryComplex(inObject)->mTargetSearchTimer;
    case EE_COMBAT_Q_LINE_OF_SIGHT:
        return EEConditionBits(EEQueryComplex(inObject)->GetLineOfSight());
    case EE_COMBAT_Q_SEEN:
        return EEIsSeenBy(EEQueryComplex(inObject), theUnit->GetPlayer(), true);
    case EE_COMBAT_Q_DUMMY_HEAD:
        return theObject == NULL || RTSDummyHead(theObject) != NULL;
    case EE_COMBAT_Q_SPECIAL_ATTACK:
        return EEQueryComplex(inObject)->mIsSpecialAttack;
    case EE_COMBAT_Q_RANGE:
        return EEConditionBits(theObject->GetStats()->GetRange());
    case EE_COMBAT_Q_HEAL_RANGE:
        return EEConditionBits(theObject->GetUnitClass()->mHealRange);
    case EE_COMBAT_Q_PROJECTILE_CATEGORY: {
        EEUnitClass* theProjectile = theObject->GetUnitClass()->mProjectileClass;
        return theProjectile != NULL ? (uint32_t)theProjectile->mCategory : EE_COMBAT_Q_NONE;
    }
    case EE_COMBAT_Q_COLLISION_RADIUS:
        return EEConditionBits(EEQueryComplex(inObject)->mCollisionRadius);
    case EE_COMBAT_Q_HIT_DISTANCE_SQUARED:
        return EEConditionBits(EEGetHitDistanceSquared(theUnit, theObject, -10000.0f, -10000.0f));
    case EE_COMBAT_Q_TAG_COUNT: {
        EEComplexUnit* theComplex = EEQueryComplex(inObject);
        EETagPointList* theTags = theComplex->GetModel()->mTagPoints[theComplex->mAnimation];
        return theTags != NULL ? (uint32_t)theTags->GetNumTagPoints() : 0;
    }
    case EE_COMBAT_Q_WEAPON_TAG:
        return (uint32_t)EEQueryComplex(inObject)->mWeaponTagPoint;
    case EE_COMBAT_Q_IN_RANGE_TO_HIT:
        return EEIsInRangeToHit(theUnit, theObject);
    case EE_COMBAT_Q_BALLISTIC_HIT:
        return EECanBallisticHit(theUnit, theObject);
    case EE_COMBAT_Q_CONTACT_RANGE: {
        float theDistance;
        memcpy(&theDistance, &inArg, 4);
        return EEIsInContactRange(theUnit, theObject, theDistance);
    }
    case EE_COMBAT_Q_ATTACK_GOAL:
        return EEGoalCast<EEUGAttackUnit>(EEQueryComplex(inObject)->mGoalState) != NULL;
    case EE_COMBAT_Q_REGION_COUNT: {
        RTSRegionTable<EEComplexUnit*>* theTable = ::gWorld.mMap->mUnitRegions;
        long theRegionX = theObject->GetTileX() / theTable->mRegionSize;
        long theRegionY = theObject->GetTileY() / theTable->mRegionSize;
        theContext->mRegion = theTable->GetRegion(theRegionX, theRegionY);
        return theContext->mRegion != NULL ? (uint32_t)theContext->mRegion->size() : EE_COMBAT_Q_NONE;
    }
    case EE_COMBAT_Q_CONTACT_X:
        return EEConditionBits(EEQueryComplex(inObject)->mInitialContactLocation.GetX());
    case EE_COMBAT_Q_CONTACT_Y:
        return EEConditionBits(EEQueryComplex(inObject)->mInitialContactLocation.GetY());
    case EE_COMBAT_Q_CONTACT_Z:
        return EEConditionBits(EEQueryComplex(inObject)->mInitialContactLocation.GetZ());
    }
    return 0;
}

uint64_t EEQueryHandle(void* inContext, int32_t inCode, uint64_t inObject, int32_t inArg)
{
    EEQueryContext* theContext = (EEQueryContext*)inContext;
    EEUnit* theObject = EEQueryUnit(inObject);
    switch (inCode) {
    case EE_COMBAT_H_COMPLEX:
        return theObject != NULL ? EEQueryHandleOf(theObject->GetComplexUnit()) : 0;
    case EE_COMBAT_H_PREVIOUS_PART:
        return EEQueryHandleOf(EEQueryComplex(inObject)->mPreviousPart);
    case EE_COMBAT_H_OWNER:
        return EEQueryHandleOf(EEQueryComplex(inObject)->mOwner);
    case EE_COMBAT_H_GOAL_TARGET: {
        EEUGAttackUnit* theGoal = EEGoalCast<EEUGAttackUnit>(EEQueryComplex(inObject)->mGoalState);
        return theGoal != NULL ? EEQueryHandleOf(theGoal->mTarget) : 0;
    }
    case EE_COMBAT_H_PLAYER:
        return (uint64_t)(uintptr_t)theObject->mPlayer;
    case EE_COMBAT_H_REGION_UNIT:
        return EEQueryHandleOf((*theContext->mRegion)[inArg]);
    case EE_COMBAT_H_TILE_PREV:
        return EEQueryHandleOf(static_cast<EEUnit*>(theObject->mTileNode->mPrev));
    case EE_COMBAT_H_TILE_NEXT:
        return EEQueryHandleOf(static_cast<EEUnit*>(theObject->mTileNode->mNext));
    case EE_COMBAT_H_TARGET:
        return EEQueryHandleOf(theContext->mGoal->mTarget);
    }
    return 0;
}

void EEQueryAct(void* inContext, int32_t inCode, uint64_t inObject, int32_t inArg)
{
    EEQueryContext* theContext = (EEQueryContext*)inContext;
    switch (inCode) {
    case EE_COMBAT_A_FIRING_HIGH:
        EEQueryComplex(inObject)->mIsFiringHigh = true;
        break;
    case EE_COMBAT_A_WEAPON_TAG:
        EEQueryComplex(inObject)->mWeaponTagPoint = (unsigned long)(uint32_t)inArg;
        break;
    case EE_COMBAT_A_IGNORE:
        theContext->mUnit->IgnoreTarget(EEQueryComplex(inObject));
        break;
    }
}

void EEQueryLocation(void* inContext, uint64_t inObject, float (*outRows)[16])
{
    memcpy(*outRows, EEQueryComplex(inObject)->mPhysicalModel.GetLocation().mMatrix, sizeof(*outRows));
}

int32_t EEQueryIntersects(void* inContext, uint64_t inCandidate, const float (*inLine)[6], int32_t inSegment,
                          uint32_t inLOD, float inScale)
{
    EEComplexUnit* theUnit = ((EEQueryContext*)inContext)->mUnit;
    GE3DLine theLine;
    if (inSegment) {
        theLine.SetStart(GE3DPoint((*inLine)[0], (*inLine)[1], (*inLine)[2]));
        theLine.SetDirection(GEVector((*inLine)[3], (*inLine)[4], (*inLine)[5]));
    }
    return theUnit->mPhysicalModel.Intersects(&EEQueryComplex(inCandidate)->mPhysicalModel, theLine, inSegment != 0,
                                              inLOD, 1.0f, 1.0f, inScale);
}

void EEQueryFootprintTile(void* inContext, uint64_t inObject, int32_t (*outTile)[2])
{
    long theX;
    long theY;
    ((EEQueryContext*)inContext)->mUnit->GetNearestFootprintTile(EEQueryUnit(inObject), &theX, &theY);
    (*outTile)[0] = (int32_t)theX;
    (*outTile)[1] = (int32_t)theY;
}

EeCombatQueryHost EEQueryHostOf(EEQueryContext* inContext)
{
    EeCombatQueryHost theHost = {inContext, EEQueryRead, EEQueryHandle, EEQueryAct, EEQueryLocation, EEQueryIntersects,
                                 EEQueryFootprintTile};
    return theHost;
}

struct EEFacingContext {
    EEComplexUnit* mUnit;
    EEGraphicObject* mModel;
};

void EEFacingTurret(void* inContext, int32_t inIndex, EeCombatTurret* outTurret)
{
    EEFacingContext* theContext = (EEFacingContext*)inContext;
    EETurretPart* thePart = *(theContext->mModel->mTurretParts.begin() + inIndex);
    outTurret->position[0] = thePart->mPosition.GetX();
    outTurret->position[1] = thePart->mPosition.GetY();
    outTurret->position[2] = thePart->mPosition.GetZ();
    memcpy(outTurret->transformation, theContext->mModel->GetTurretPartTransformation(inIndex).mMatrix,
           sizeof(outTurret->transformation));
    outTurret->angle = theContext->mUnit->mTurretAngles[inIndex];
}

}   // namespace

EEUnit* EECombatFindImpactUnit(EEComplexUnit* inUnit)
{
    EEQueryContext theContext = {inUnit, NULL, NULL};
    EeCombatQueryHost theHost = EEQueryHostOf(&theContext);
    return EEQueryUnit(ee_combat_impact_unit(EEQueryHandleOf(inUnit), &theHost));
}

bool EECombatIsInReach(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    EEQueryContext theContext = {inUnit, NULL, NULL};
    EeCombatQueryHost theHost = EEQueryHostOf(&theContext);
    return ee_combat_in_reach(EEQueryHandleOf(inUnit), EEQueryHandleOf(inTarget), &theHost) != 0;
}

bool EECombatIsStillReachable(EEComplexUnit* inUnit, EEUGAttackUnit* inGoal)
{
    EEQueryContext theContext = {inUnit, inGoal, NULL};
    EeCombatQueryHost theHost = EEQueryHostOf(&theContext);
    return ee_combat_still_reachable(EEQueryHandleOf(inUnit), &theHost) != 0;
}

bool EECombatIsGateClosed(EEComplexUnit* inUnit, EEUGAttackUnit* inGoal)
{
    EEQueryContext theContext = {inUnit, inGoal, NULL};
    EeCombatQueryHost theHost = EEQueryHostOf(&theContext);
    return ee_combat_gate_closed(EEQueryHandleOf(inUnit), &theHost) != 0;
}

bool EECombatIsEnemyWithinRange(EEComplexUnit* inUnit)
{
    EEQueryContext theContext = {inUnit, NULL, NULL};
    EeCombatQueryHost theHost = EEQueryHostOf(&theContext);
    return ee_combat_enemy_in_range(EEQueryHandleOf(inUnit), &theHost) != 0;
}

bool EECombatFacesPoint(EEUnitClass* inClass, EEComplexUnit* inUnit, const GE3DPoint& inLocation, bool inUseTolerance)
{
    EeCombatFacingIn in;
    memset(&in, 0, sizeof(in));
    memcpy(in.location, inUnit->mPhysicalModel.GetLocation().mMatrix, sizeof(in.location));
    in.point[0] = inLocation.GetX();
    in.point[1] = inLocation.GetY();
    in.use_tolerance = inUseTolerance;
    in.firing_arc = inClass->mFiringArc;
    in.facing_offset = inClass->mFacingOffset;
    in.fires_broadside = inClass->mFiresBroadside;
    in.unit_type = (int32_t)inUnit->mUnitClass->GetUnitType();
    in.special_attack = inUnit->mIsSpecialAttack;
    EEGraphicObject* theModel = inUnit->GetModel();
    in.turrets = (int32_t)theModel->GetNumParts();
    if (in.turrets > 0)
        in.scale = inUnit->mStats->GetScale();
    EEFacingContext theContext = {inUnit, theModel};
    return ee_combat_faces_point(&in, &theContext, EEFacingTurret) != 0;
}
