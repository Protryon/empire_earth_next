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

EECombatFolder sHostReads(EE_COMBAT_TAG('C', 'B', 'H', 'R'), "EE_SIM_CAPTURE_CBHR_FULL");
thread_local int sHostReadDepth = 0;

}   // namespace

EECombatHostReadScope::EECombatHostReadScope() : mOuter(sHostReadDepth++ == 0) {}
EECombatHostReadScope::~EECombatHostReadScope() { sHostReadDepth--; }

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
