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
#include "ee/EEUGRust.h"
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
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEEffectManager.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIContinent.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EERules.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERulesWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEUnitAction.h"
#include "ee/EEPathManager.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEAIPathGoalModifiers.h"

extern EEToHitTable gToHitTable;           // 0x5318e8 (Empire Earth.cpp; as EECPAttackSubGroupUnit.cpp declares it)

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
EECombatFolder sXReads(EE_COMBAT_TAG('C', 'B', 'X', 'R'), "EE_SIM_CAPTURE_CBXR_FULL");
EECombatFolder sXActions(EE_COMBAT_TAG('C', 'B', 'X', 'A'), "EE_SIM_CAPTURE_CBXA_FULL");
EECombatFolder sGoalObjects(EE_COMBAT_TAG('C', 'B', 'G', 'O'), "EE_SIM_CAPTURE_CBGO_FULL");
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
        EEUnit* theTarget = static_cast<EEUGRust*>(theGoal)->GetAttackTarget();
        theRecord[8] = theTarget != NULL ? (uint32_t)theTarget->mUnitID : 0;
    } else if (theGoal != NULL && theGoal->mKind == kGoalAttackGround) {
        const GE3DPoint& thePoint = static_cast<EEUGRust*>(theGoal)->GetGroundTarget();
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

void EECombatCaptureXRead(uint32_t inFunc, const EEComplexUnit* inUnit, const EEUnit* inTarget, int32_t inResult,
                          uint32_t inExtra0, uint32_t inExtra1)
{
    if (!EESimCaptureRecord::Active())
        return;
    uint32_t theRecord[6] = {inFunc, inUnit != NULL ? (uint32_t)inUnit->mUnitID : 0,
                             inTarget != NULL ? (uint32_t)inTarget->mUnitID : 0, (uint32_t)inResult, inExtra0, inExtra1};
    sXReads.Add(theRecord, sizeof(theRecord));
}

void EECombatCaptureIgnored(uint32_t inFunc, const EEComplexUnit* inUnit, const EEComplexUnit* inTarget, int32_t inResult,
                            uint32_t inCheckLocation)
{
    if (!EESimCaptureRecord::Active())
        return;
    uint32_t theHash = 0x811c9dc5u;
    for (size_t i = 0; i < inUnit->mIgnoredTargets.size(); i++) {
        const EEComplexUnitAttacker& theEntry = inUnit->mIgnoredTargets[i];
        uint32_t theFields[6] = {(uint32_t)theEntry.mTileX, (uint32_t)theEntry.mTileY, (uint32_t)theEntry.mFromX,
                                 (uint32_t)theEntry.mFromY, (uint32_t)theEntry.mUnitID, (uint32_t)theEntry.mTime};
        const unsigned char* theBytes = (const unsigned char*)theFields;
        for (size_t k = 0; k < sizeof(theFields); k++)
            theHash = (theHash ^ theBytes[k]) * 0x01000193u;
    }
    uint32_t theRecord[7] = {inFunc, (uint32_t)inUnit->mUnitID, inTarget != NULL ? (uint32_t)inTarget->mUnitID : 0,
                             (uint32_t)inResult, inCheckLocation, (uint32_t)inUnit->mIgnoredTargets.size(), theHash};
    sXReads.Add(theRecord, sizeof(theRecord));
}

void EECombatCaptureXAction(uint32_t inFunc, EEComplexUnit* inUnit, EEUnit* inTarget, uint32_t inBefore)
{
    if (!EESimCaptureRecord::Active())
        return;
    uint32_t theRecord[20];
    memset(theRecord, 0, sizeof(theRecord));
    theRecord[0] = inFunc;
    theRecord[1] = (uint32_t)inUnit->mUnitID;
    theRecord[2] = inTarget != NULL ? (uint32_t)inTarget->mUnitID : 0;
    theRecord[3] = inBefore;
    theRecord[4] = EECombatCommandOf(inUnit);
    theRecord[5] = (uint32_t)inUnit->mAnimation;
    theRecord[6] = (uint32_t)inUnit->GetHitPoints();
    theRecord[7] = (uint32_t)inUnit->mReloadTimer.Get();
    theRecord[8] = (uint32_t)inUnit->mWeaponTagPoint;
    theRecord[9] = (uint32_t)inUnit->mAmmunition;
    theRecord[10] = (uint32_t)inUnit->mFlightTime;
    theRecord[11] = inUnit->mIsSpecialAttack;
    theRecord[12] = inUnit->mIsFiringHigh;
    theRecord[13] = inUnit->mExperience;
    EEUnitGoal* theGoal = inUnit->mGoalState;
    theRecord[14] = theGoal != NULL ? (uint32_t)theGoal->mKind : 0xffffffffu;
    EEUnit* theGoalTarget = EECombatShotTarget(inUnit);
    theRecord[15] = theGoalTarget != NULL ? (uint32_t)theGoalTarget->mUnitID : 0;
    theRecord[16] = inTarget != NULL ? (uint32_t)inTarget->GetHitPoints() : 0;
    theRecord[17] = inTarget != NULL ? (uint32_t)inTarget->GetResourceAmount() : 0;
    theRecord[18] = inUnit->mLastAttacker != NULL ? (uint32_t)inUnit->mLastAttacker->mUnitID : 0;
    theRecord[19] = (uint32_t)inUnit->mTargetSearchTimer.Get();
    sXActions.Add(theRecord, sizeof(theRecord));
}

void EECombatCaptureGoal(uint32_t inFunc, uint32_t inKind, const EEUnit* inUnit, const EEUnit* inTarget, int32_t inResult,
                         const float* inPoint, uint32_t inExtra0, uint32_t inExtra1)
{
    if (!EESimCaptureRecord::Active())
        return;
    uint32_t theRecord[10] = {inFunc, inKind, inUnit != NULL ? (uint32_t)inUnit->mUnitID : 0,
                              inTarget != NULL ? (uint32_t)inTarget->mUnitID : 0, (uint32_t)inResult, 0, 0, 0,
                              inExtra0, inExtra1};
    if (inPoint != NULL)
        memcpy(&theRecord[5], inPoint, 12);
    sGoalObjects.Add(theRecord, sizeof(theRecord));
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
    sXReads.Flush();
    sXActions.Flush();
    sGoalObjects.Flush();
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

float EEBitsFloat(int32_t inBits)
{
    float theValue;
    memcpy(&theValue, &inBits, 4);
    return theValue;
}

// the attack goal's target (attack unit), the heal goal's (heal)
EEUnit* EEConditionTarget(EEConditionContext* inContext)
{
    if (inContext->mKind == EE_COMBAT_GOAL_HEAL)
        return static_cast<EEUGHealUnit*>(inContext->mGoal)->mTarget;
    return static_cast<EEUGRust*>(inContext->mGoal)->GetAttackTarget();
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
        return EECombatIsGateClosed(theUnit, static_cast<EEUGRust*>(theContext->mGoal));
    case EE_COMBAT_READ_STILL_REACHABLE:
        return EECombatIsStillReachable(theUnit, static_cast<EEUGRust*>(theContext->mGoal));
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
        const GE3DPoint& theTarget = static_cast<EEUGRust*>(theContext->mGoal)->GetGroundTarget();
        return theUnit->CanReachTile(UMFloor(theTarget.GetX()), UMFloor(theTarget.GetY()));
    }
    case EE_COMBAT_READ_FACES_POINT:
        return theUnit->GetUnitClass()->FacesPoint(theUnit, static_cast<EEUGRust*>(theContext->mGoal)->GetGroundTarget(), true);
    case EE_COMBAT_READ_HEAL_IN_RANGE:
        return EECombatHealInRange(theUnit, static_cast<EEUGHealUnit*>(theContext->mGoal)->mTarget);
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
        return theUnit->mUnitClass->FacesTarget(theUnit, static_cast<EEUGRust*>(theContext->mGoal)->GetAttackTarget(), false);
    case EE_COMBAT_STEP_ANIMATION:
        return (int32_t)theUnit->mAnimation;
    case EE_COMBAT_STEP_SET_ANIMATION:
        theUnit->SetAnimation(inArg, -1, 0);
        break;
    case EE_COMBAT_STEP_DROP_PATH:
        theUnit->DropPath();
        break;
    case EE_COMBAT_STEP_RETARGET_GATE:
        static_cast<EEUGRust*>(theContext->mGoal)->RetargetGatePart(theUnit);
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
    return theGoal != NULL && theGoal->mKind == kGoalAttackUnit ? static_cast<EEUGRust*>(theGoal)->GetAttackTarget() : NULL;
}

long EECombatCondition(int32_t inKind, EEComplexUnit* inUnit, unsigned long inEvent, EEUnit* inTarget,
                       EEUnitGoal* inGoal, EEUnitClass* inClass)
{
    // the record: the store's part in Rust (ee_combat store.rs: the plain fields, the stats table's, the players'
    // tables); here the unit's player's IsHuman (a virtual) and the attack goal's own fields
    EeCombatConditionIn in;
    ee_combat_fill_condition(&in, inKind, (uint64_t)(uintptr_t)inUnit, (uint32_t)inEvent, (uint64_t)(uintptr_t)inTarget,
                             inTarget != NULL ? (uint64_t)(uintptr_t)static_cast<EEUnit*>(inTarget->mComplexUnit) : 0);
    in.human = inUnit->mPlayer != NULL && inUnit->mPlayer->IsHuman();
    if (inKind == EE_COMBAT_GOAL_ATTACK_UNIT) {
        EEUGRust* theGoal = static_cast<EEUGRust*>(inGoal);
        in.goal_tile[0] = (int32_t)theGoal->GetAttackTileX();
        in.goal_tile[1] = (int32_t)theGoal->GetAttackTileY();
        in.convert = theGoal->IsConvertAttack();
    }
    EEConditionContext theContext = {in.kind, in.event, inUnit, inGoal, inClass, NULL};
    EeCombatConditionHost theHost = {&theContext, EEConditionRead};
    EeCombatConditionOut theOut = ee_combat_condition(&in, &theHost);
    // the action before the record, as the original's body set the goal before its end
    if (theOut.action == EE_COMBAT_CONDITION_ATTACK_IMPACT_UNIT)
        inUnit->SetGoal(EEUGRust::NewAttackUnit(inUnit, theContext.mImpact, false, false), 0, 0);
    EECombatCaptureCondition(in, theOut);
    return theOut.result;
}

void EECombatStep(int32_t inKind, EEComplexUnit* inUnit, EEUnitGoal* inGoal, EEUnit* inTarget)
{
    EeCombatStepIn in;
    ee_combat_fill_step(&in, inKind, (uint64_t)(uintptr_t)inUnit, (uint64_t)(uintptr_t)inTarget);
    EEStepContext theContext = {inKind, inUnit, inGoal};
    EeCombatStepHost theHost = {&theContext, EEStepOp};
    ee_combat_goal_step(&in, &theHost);
    EECombatCaptureStep(in);
}

// ---- the host of ee_combat's reads (reads.rs): FindImpactUnit, FacesPoint, IsInReach, IsStillReachable,
// IsGateClosed, EEIsEnemyWithinRange. Every handle an EEUnit* (a player's an EEPlayer*), 0 for none; each read the
// original's expression on it. The unit's plain fields, its stats and the players' tables are ee_combat store.rs's
// reads (the unit store's records, the stats table), never asked here.

namespace {

struct EEQueryContext {
    EEComplexUnit* mUnit;                   // the call's unit
    EEUGRust* mGoal;                  // IsStillReachable's, IsGateClosed's attack goal
    std::vector<EEComplexUnit*>* mRegion;   // FindImpactUnit's region, found by its count read
    EEUnitClass* mClass = NULL;             // world.rs: the class of Attacked or FireShot
    EEUnitGoal* mAttackedGoal = NULL;       // Attacked's goal (the helpers' clones)
    std::vector<EEUnit*> mNear;             // Attacked's units near the attacked unit
    U2DPoint<long> mTile;                   // Attacked's EEIsAttackReachable tile (one for all helpers, as the original)
    GE3DPoint mPoint;                       // FireShot's drop point
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
    case EE_COMBAT_Q_MODEL:
        return EEQueryComplex(inObject)->mPhysicalModel.GetModel() != NULL;
    case EE_COMBAT_Q_TARGETABLE:
        return EEUUGetIsUnitClassTargetable(inArg ? theUnit : NULL, theObject->mUnitClass);
    case EE_COMBAT_Q_FIRING_HIGH:
        return EEQueryComplex(inObject)->mIsFiringHigh;
    case EE_COMBAT_Q_PASSABLE:
        return EEQueryComplex(inObject)->IsPassableBy(theUnit);
    case EE_COMBAT_Q_UNIT_SET:
        return gUnitSetManager.Contains(theObject->mUnitClass->GetUnitType(), (unsigned long)inArg);
    case EE_COMBAT_Q_SCALE:
        return EEConditionBits(EEQueryComplex(inObject)->mPhysicalModel.GetScale());
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
    case EE_COMBAT_Q_LINE_OF_SIGHT:
        return EEConditionBits(EEQueryComplex(inObject)->GetLineOfSight());
    case EE_COMBAT_Q_SEEN:
        return EEIsSeenBy(EEQueryComplex(inObject), theUnit->GetPlayer(), true);
    case EE_COMBAT_Q_DUMMY_HEAD:
        return theObject == NULL || RTSDummyHead(theObject) != NULL;
    case EE_COMBAT_Q_SPECIAL_ATTACK:
        return EEQueryComplex(inObject)->mIsSpecialAttack;
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
        return EEAttackUnitCast(EEQueryComplex(inObject)->mGoalState) != NULL;
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
    case EE_COMBAT_Q_GAME_TIME: return (uint32_t)gPhysics->mGameTime;
    case EE_COMBAT_Q_TILE_SIZE: return (uint32_t)theObject->mUnitClass->mTileSize;
    case EE_COMBAT_Q_IGNORED_COUNT: return (uint32_t)EEQueryComplex(inObject)->mIgnoredTargets.size();
    case EE_COMBAT_Q_CLASS_MAX_VELOCITY: return EEConditionBits(theObject->mUnitClass->mMaxVelocity);
    case EE_COMBAT_Q_MAP_SIZE: return (uint32_t)::gWorld.mMap->mSize;
    case EE_COMBAT_Q_DISTANCE_SQUARED: return EEConditionBits(theUnit->DistanceSquaredTo(EEQueryComplex(inObject)));
    case EE_COMBAT_Q_TARGETS_ENEMIES: return EEQueryComplex(inObject)->mBehavior->mCanTargetEnemies;
    case EE_COMBAT_Q_MOVES_FREELY: return EEQueryComplex(inObject)->mBehavior->CanMoveFreely(EEQueryComplex(inObject));
    case EE_COMBAT_Q_GOAL_B1C: return EEQueryComplex(inObject)->mGoalState->GetB1C();
    case EE_COMBAT_Q_AMMUNITION: return (uint32_t)EEQueryComplex(inObject)->mAmmunition;
    case EE_COMBAT_Q_SHOT_CLASS: {
        EEUnitClass* theClass = theContext->mClass;
        return (theClass->mProjectileClass != NULL ? EE_COMBAT_SHOT_CLASS_PROJECTILE : 0)
            | (theClass->mUsesAmmunition ? EE_COMBAT_SHOT_CLASS_AMMUNITION : 0)
            | (theClass->mMuzzleFlashEffect != NULL ? EE_COMBAT_SHOT_CLASS_MUZZLE_FLASH : 0)
            | (theClass->mShotEffect != NULL ? EE_COMBAT_SHOT_CLASS_SHOT_EFFECT : 0);
    }
    case EE_COMBAT_Q_TURRET_PARTS: return (uint32_t)EEQueryComplex(inObject)->GetModel()->mTurretParts.size();
    case EE_COMBAT_Q_HIGH_TECH_TOOLS:
        return EEGetRules().Epoch(theObject->mPlayer->mTechnologies->mCurrentEpoch).mHighTechTools;   // epochs.yaml
    case EE_COMBAT_Q_SPARKS_EFFECT: return theObject->mUnitClass->mImpactSparksEffect != NULL;
    case EE_COMBAT_Q_RESOURCE_TYPE: return (uint32_t)theObject->mResourceType;
    // leaves.rs (R5b.2, thirteenth round)
    case EE_COMBAT_Q_INNER_SPHERE: return EEConditionBits(theObject->mUnitClass->GetInnerCollisionSphere());
    case EE_COMBAT_Q_SHOOTS_HIGH: return theObject->mUnitClass->mShootsHigh;
    case EE_COMBAT_Q_TOWER: return theObject->mUnitClass->mIsTower;
    case EE_COMBAT_Q_WAYPOINTS: return (uint32_t)EEQueryComplex(inObject)->mWaypoints.size();
    case EE_COMBAT_Q_MAX_VELOCITY: return EEConditionBits(EEQueryComplex(inObject)->GetMaxVelocity());
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
        EEUGRust* theGoal = EEAttackUnitCast(EEQueryComplex(inObject)->mGoalState);
        return theGoal != NULL ? EEQueryHandleOf(theGoal->GetAttackTarget()) : 0;
    }
    case EE_COMBAT_H_REGION_UNIT:
        return EEQueryHandleOf((*theContext->mRegion)[inArg]);
    case EE_COMBAT_H_TILE_PREV:
        return EEQueryHandleOf(static_cast<EEUnit*>(theObject->mTileNode->mPrev));
    case EE_COMBAT_H_TILE_NEXT:
        return EEQueryHandleOf(static_cast<EEUnit*>(theObject->mTileNode->mNext));
    case EE_COMBAT_H_TARGET:
        return EEQueryHandleOf(theContext->mGoal->GetAttackTarget());
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

const float kCallForHelpRadius = 5.0f;  // 0x447aec (Attacked's)

// ---- the operations of ee_combat world.rs (R5b.2, eighth round): each the original's statement on the handles
uint64_t EEWorldOp(void* inContext, int32_t inCode, uint64_t inObject, uint64_t inOther, EeCombatWorldArgs* ioArgs)
{
    EEQueryContext* theContext = (EEQueryContext*)inContext;
    EEComplexUnit* theObject = EEQueryComplex(inObject);
    EEComplexUnit* theOther = EEQueryComplex(inOther);
    int64_t* a = *ioArgs;
    EEUnitClass* theClass = theContext->mClass;
    switch (inCode) {
    case EE_COMBAT_W_IGNORED_ENTRY: {
        const EEComplexUnitAttacker& theEntry = theObject->mIgnoredTargets[(size_t)a[0]];
        a[0] = theEntry.mTileX;
        a[1] = theEntry.mTileY;
        a[2] = theEntry.mFromX;
        a[3] = theEntry.mFromY;
        a[4] = (int64_t)theEntry.mUnitID;
        a[5] = (int64_t)theEntry.mTime;
        return 0;
    }
    case EE_COMBAT_W_IGNORED_ERASE: {
        std::deque<EEComplexUnitAttacker>& theList = theObject->mIgnoredTargets;
        theList.erase(theList.begin() + (size_t)a[0], a[1] < 0 ? theList.end() : theList.begin() + (size_t)a[1]);
        return 0;
    }
    case EE_COMBAT_W_IGNORED_PUSH: {
        EEComplexUnitAttacker theEntry;
        theEntry.mTileX = (long)a[0];
        theEntry.mTileY = (long)a[1];
        theEntry.mFromX = (long)a[2];
        theEntry.mFromY = (long)a[3];
        theEntry.mUnitID = (unsigned long)a[4];
        theEntry.mTime = (unsigned long)a[5];
        theObject->mIgnoredTargets.push_front(theEntry);
        return 0;
    }
    case EE_COMBAT_W_IGNORED_TIME: theObject->mIgnoredTargets[(size_t)a[0]].mTime = (unsigned long)a[1]; return 0;
    case EE_COMBAT_W_CORNER_BLOCKED:
        return EEIsCornerBlocked((unsigned long)a[0], (unsigned long)a[1], (unsigned long)a[2], (unsigned long)a[3]);
    case EE_COMBAT_W_CAN_GET_TO_UNIT:
        return gContinentManager.CanGetToUnit(EEQueryUnit(inObject), EEQueryUnit(inOther));
    case EE_COMBAT_W_CONTINENT_AT: return (uint64_t)(uintptr_t)gContinentManager.GetContinentAt(EEQueryUnit(inObject));
    case EE_COMBAT_W_CONTINENT_LINK: {
        EEAIContinent* theFrom = (EEAIContinent*)(uintptr_t)inObject;
        EEAIContinent* theTo = (EEAIContinent*)(uintptr_t)inOther;
        return gContinentManager.mTileFlags[theFrom->mID * gContinentManager.mFlagsWidth + theTo->mID] != 0;
    }
    case EE_COMBAT_W_CONNECTED_FOR_UNIT:
        return gContinentManager.AreConnectedForUnit(EEQueryUnit((uint64_t)a[0]), (EEAIContinent*)(uintptr_t)inObject,
                                                     (EEAIContinent*)(uintptr_t)inOther);
    case EE_COMBAT_W_OTHER_CONTINENT: return theObject->IsOnOtherContinentThan(theOther);
    case EE_COMBAT_W_FIND_PATH: {
        std::vector<U2DPoint<long> > thePath;
        U2DPoint<long> theFrom((long)a[0], (long)a[1]);
        U2DPoint<long> theTo((long)a[2], (long)a[3]);
        EEAIAStarBounds theBounds;
        theBounds.mTop = (long)a[4];
        theBounds.mLeft = (long)a[5];
        theBounds.mBottom = (long)a[6];
        theBounds.mRight = (long)a[7];
        EEAIBPGModifierEnemyIsAttackableByUnit* theModifier = new EEAIBPGModifierEnemyIsAttackableByUnit(theObject, theOther);
        gPathManager.FindPath(theObject, ::gWorld.mMap, theFrom, theTo, theBounds, thePath, 0xb, (long)a[8], theModifier,
                              NULL);
        if (thePath.size() != 0) {
            a[0] = thePath[0].mX;
            a[1] = thePath[0].mY;
        }
        theModifier->Dereference();
        return thePath.size();
    }
    case EE_COMBAT_W_SEEN_BY_OWNER: return EEIsSeenByOwnerOf(theObject, EEQueryUnit(inOther));
    case EE_COMBAT_W_ATTACKED_GOAL: {
        if (a[0])
            theContext->mAttackedGoal = new EEUGLocation(theOther->mPhysicalModel.GetLocation().GetX(),
                theOther->mPhysicalModel.GetLocation().GetY(), 0.0f, false, false, false, 0.0f, false, 0);
        else
            theContext->mAttackedGoal = EEUGRust::NewAttackUnit(theObject, theOther, false, false);
        theObject->SetGoal(theContext->mAttackedGoal, 1, 0);
        return 0;
    }
    case EE_COMBAT_W_UNITS_NEAR: {
        const GETransformation& theLocation = theObject->mPhysicalModel.GetLocation();
        GE3DPoint thePosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
        EEGetUnitsNear(thePosition, kCallForHelpRadius, theContext->mNear);
        return theContext->mNear.size();
    }
    case EE_COMBAT_W_NEAR_UNIT: return EEQueryHandleOf(theContext->mNear[(size_t)a[0]]);
    case EE_COMBAT_W_CAN_ATTACK_CLASS: return theObject->CanAttackClass(EEQueryUnit(inOther)->GetUnitClass());
    case EE_COMBAT_W_IS_IGNORED: return theObject->IsTargetIgnored(theOther, true);
    case EE_COMBAT_W_ATTACK_REACHABLE: return EEIsAttackReachable(theObject, theOther, &theContext->mTile);
    case EE_COMBAT_W_HELP: {
        EEUnitGoal* theCopy = theContext->mAttackedGoal->Clone();
        if (theObject->GetClass()->FindAction(theCopy->GetStartAction()) != NULL) {
            theObject->SetGoal(theCopy, 1, 0);
            theObject->StartAction(theCopy->GetStartAction());
            theObject->GetUnitClass()->ActionChanged(theObject);
        }
        return 0;
    }
    case EE_COMBAT_W_IGNORE: theObject->IgnoreTarget(theOther); return 0;
    case EE_COMBAT_W_RELEASE_ATTACKER:
        if (theObject->mLastAttacker != NULL)
            theObject->mLastAttacker->Dereference();
        theObject->mLastAttacker = NULL;
        return 0;
    case EE_COMBAT_W_LAST_ATTACKER: return EEQueryHandleOf(theObject->mLastAttacker);
    case EE_COMBAT_W_LAUNCH_STORED_MISSILE: theClass->LaunchStoredMissile(theObject); return 0;
    case EE_COMBAT_W_RELOAD: theObject->SetRandomReloadTimer((long)a[0]); return 0;
    case EE_COMBAT_W_FIND_DROP: {
        EEUnit* theTarget;
        EEFindDropLocation(theObject, &theTarget, &theContext->mPoint, NULL);
        return EEQueryHandleOf(theTarget);
    }
    case EE_COMBAT_W_LINE_BLOCKED: {
        EEUnit* theBlocker = NULL;
        long theX = 0;
        long theY = 0;
        bool theResult = EEIsLineToUnitBlocked(theObject, EEQueryUnit(inOther), &theBlocker, &theX, &theY);
        a[0] = (int64_t)EEQueryHandleOf(theBlocker);
        a[1] = theX;
        a[2] = theY;
        return theResult;
    }
    case EE_COMBAT_W_SPARKS_ON: {
        EEUnit* theBlocker = EEQueryUnit(inObject);
        gWorld.GetEffectManager()->CreateUnitEffect(theBlocker, theBlocker->mUnitClass->mImpactSparksEffect, NULL, 0, 0, 0);
        return 0;
    }
    case EE_COMBAT_W_SPARKS_AT: {
        long theX = (long)a[0];
        long theY = (long)a[1];
        GE3DPoint theSpot((float)theX + 0.5f, (float)theY + 0.5f, gWorld.mMap->GetTile(theX, theY)->mHeight);
        gWorld.GetEffectManager()->CreatePlacedEffect(theSpot, EEGraphicEffect::Get(kGraphicEffectImpactSparks), NULL);
        return 0;
    }
    case EE_COMBAT_W_LAUNCH_SHOT:
        theClass->LaunchShot(theObject, EEQueryUnit(inOther), theContext->mPoint, NULL);
        return 0;
    case EE_COMBAT_W_LAUNCH_FROM_TURRET:
        theClass->LaunchFromTurret(theObject, EEQueryUnit(inOther), theContext->mPoint);
        return 0;
    case EE_COMBAT_W_SPECIAL_EFFECT:
        gWorld.GetEffectManager()->CreateEffectBetweenUnits(theObject, EEQueryUnit(inOther), theClass->mSpecialAttackEffect, NULL);
        return 0;
    case EE_COMBAT_W_START_ACTION: theObject->StartAction((unsigned long)a[0]); return 0;
    case EE_COMBAT_W_MUZZLE_FLASH:
        gWorld.GetEffectManager()->CreateUnitEffect(theObject, theClass->mMuzzleFlashEffect, NULL, 0, 0, 0);
        return 0;
    case EE_COMBAT_W_SHOT_EFFECT_BETWEEN:
        gWorld.GetEffectManager()->CreateEffectBetweenUnits(theObject, EEQueryUnit(inOther), theClass->mShotEffect, NULL);
        return 0;
    case EE_COMBAT_W_SHOT_EFFECT_AT:
        gWorld.GetEffectManager()->CreateEffect(theObject, theContext->mPoint, theClass->mShotEffect, NULL, 0);
        return 0;
    case EE_COMBAT_W_SET_AMMUNITION: theObject->SetAmmunition((unsigned long)a[0]); return 0;
    case EE_COMBAT_W_SET_FLIGHT_TIME: theObject->SetFlightTime((long)a[0]); return 0;
    case EE_COMBAT_W_SPECIAL_OFF: theObject->mIsSpecialAttack = false; return 0;
    // damage.rs (R5b.2, tenth round): ApplyHit, ApplyAreaDamage, ComputeDamage
    case EE_COMBAT_W_DAMAGE_IN: {
        EeCombatDamageIn& in = *(EeCombatDamageIn*)(uintptr_t)a[0];
        EEUnit* theTarget = EEQueryUnit(inOther);
        if (in.form != kDamageDirect || in.attack > 0.0f) {   // a direct hit without attack reads no factor
            in.attack_factor = gToHitTable.GetAttackFactor((long)a[1], theTarget->mUnitClass,
                                                           theObject->mPlayer->GetPlayerID(), theTarget->mPlayer->GetPlayerID());
            in.weapon_factor = gWeaponToHitFactors[theTarget->mUnitClass->mWeapon][a[2]];
        }
        const EECombatRules& theRules = EEGetRules().mCombat;
        in.minimum_damage = theRules.mMinimumDamage;
        in.fanatic_factor = EERuleFloat(theRules.mFanaticFactor);
        in.elevation_step = theRules.mElevationStep;
        in.ally_factor = theRules.mAllyFactor;
        in.herd_ally_factor = theRules.mHerdAllyFactor;
        in.trample_damage = theRules.mTrampleDamage;
        in.trample_threshold = theRules.mTrampleThreshold;
        return 0;
    }
    case EE_COMBAT_W_ELEVATION: return EEConditionBits(theClass->GetElevationFactor(theObject, theOther));
    case EE_COMBAT_W_DAMAGE_CAPTURE:
        EECombatCapture(EE_COMBAT_TAG('C', 'B', 'D', 'M'), *(const EeCombatDamageIn*)(uintptr_t)a[0],
                        *(const EeCombatDamageOut*)(uintptr_t)a[1]);
        return 0;
    case EE_COMBAT_W_HIT_CLASS: {   // a capital ship's special weapon, else the launcher's or this class; the splash's
        EEUnitClass* theHit = theObject->mUnitClass->mCategory == kUnitCategorySpaceship && theObject->mIsSpecialAttack
            ? gUnitClassTable[kUnitTypeCapitalShipSpecialWeapon]
            : theObject->mLauncherClass == NULL ? theClass : theObject->mLauncherClass;
        EEUnitClass* theSplash = theObject->mLauncherClass != NULL ? theObject->mLauncherClass : theClass;
        EEUnitClassUpgrade* theUpgrade = theObject->mPlayer->GetUpgrade(theHit->mUnitType);
        a[0] = EEConditionBits(theUpgrade != NULL ? theUpgrade->GetAttack() : theHit->mAttack);
        a[1] = EEConditionBits(theUpgrade != NULL ? theUpgrade->mAreaEffect : theHit->mAreaEffect);
        a[2] = theHit->mAmmoType;
        a[3] = theHit->mWeapon;
        a[4] = theHit->mCategory;
        a[5] = EEConditionBits(theSplash->mFriendlyDamageFactor);
        a[6] = theSplash->mCategory;
        a[7] = EEConditionBits(EEGetRules().mCombat.mDeadFoodShare);
        a[8] = theClass->mShotEffect != NULL;
        return 0;
    }
    case EE_COMBAT_W_UNIT_EFFECT: {
        EEGraphicEffect* theEffect = NULL;
        if (!theObject->GetEffect(3, theEffect))
            return 0;
        if (theEffect != NULL)
            gEffectManager->CreateUnitEffect(theObject, theEffect, NULL, false, 0, 0);
        return 1;
    }
    case EE_COMBAT_W_TAKE_HIT:
        EEQueryUnit(inObject)->TakeHit(theOther, (long)a[0], theOther->mPlayer->GetPlayerID(), 1);
        return 0;
    case EE_COMBAT_W_UNIT_DAMAGED:
        EEQueryUnit(inObject)->mUnitClass->UnitDamaged(EEQueryUnit(inObject)->GetComplexUnit(), theOther, (long)a[0]);
        return 0;
    case EE_COMBAT_W_EXPERIENCE: theObject->mExperience++; return 0;
    case EE_COMBAT_W_SET_RESOURCE: EEQueryUnit(inObject)->mResourceAmount = (long)a[0]; return 0;
    case EE_COMBAT_W_TILE_UNITS: {
        EEMapTile* theTile = ::gWorld.mMap->GetTile((long)a[0], (long)a[1]);
        return theTile != NULL ? (uint64_t)(uintptr_t)theTile->mUnits : 0;
    }
    case EE_COMBAT_W_NODE_NEXT: return (uint64_t)(uintptr_t)((RTSTileUnitNode*)(uintptr_t)inObject)->mNext;
    case EE_COMBAT_W_NODE_UNIT:
        return EEQueryHandleOf(((RTSTileUnitNode*)(uintptr_t)inObject)->mUnit->GetComplexUnit());
    case EE_COMBAT_W_AREA_CAPTURE: {   // TEST ONLY: CBAD (the victims), CBXA (their FNV-1a 32 after the hits)
        if (!EESimCaptureRecord::Active())
            return 0;
        const uint64_t* theVictims = (const uint64_t*)(uintptr_t)a[0];
        EEUnit* theTarget = EEQueryUnit(inOther);
        uint32_t theBits = (uint32_t)a[4];
        float theRadius;
        memcpy(&theRadius, &theBits, 4);
        EESimCaptureRecord theRecord(EE_COMBAT_TAG('C', 'B', 'A', 'D'));
        theRecord.I32((int32_t)theObject->GetUnitID());
        theRecord.I32((int32_t)a[2]);
        theRecord.I32((int32_t)a[3]);
        theRecord.F32(theRadius);
        theRecord.I32((int32_t)a[5]);
        theRecord.I32(theTarget != NULL && theTarget->GetComplexUnit() != NULL ? (int32_t)theTarget->GetUnitID() : -1);
        theRecord.I32((int32_t)a[1]);
        for (int64_t i = 0; i < a[1]; i++)
            theRecord.I32((int32_t)EEQueryUnit(theVictims[i])->GetUnitID());
        theRecord.Commit();
        uint32_t theHash = 0x811c9dc5u;
        for (int64_t i = 0; i < a[1]; i++) {
            EEComplexUnit* theVictim = EEQueryComplex(theVictims[i]);
            uint32_t theFields[3] = {(uint32_t)theVictim->mUnitID, (uint32_t)theVictim->GetHitPoints(),
                                     (uint32_t)theVictim->GetResourceAmount()};
            const unsigned char* theBytes = (const unsigned char*)theFields;
            for (size_t k = 0; k < sizeof(theFields); k++)
                theHash = (theHash ^ theBytes[k]) * 0x01000193u;
        }
        EECombatCaptureXAction(EE_COMBAT_XA_AREA_DAMAGE, theObject, theTarget, theHash);
        return 0;
    }
    case EE_COMBAT_W_CAPTURE:   // TEST ONLY: a formula's record (EECombatCapture's)
        if (EESimCaptureRecord::Active()) {
            EESimCaptureRecord theRecord((unsigned int)a[0]);
            theRecord.Bytes((const void*)(uintptr_t)a[1], (unsigned int)a[2]);
            theRecord.Bytes((const void*)(uintptr_t)a[3], (unsigned int)a[4]);
            theRecord.Commit();
        }
        return 0;
    case EE_COMBAT_W_PATH_START: {
        GE3DPoint theNext;
        float theIndex;
        theObject->GetPathStart(theNext, theIndex);
        a[0] = EEConditionBits(theNext.GetX());
        a[1] = EEConditionBits(theNext.GetY());
        a[2] = EEConditionBits(theNext.GetZ());
        return 0;
    }
    case EE_COMBAT_W_TILE_AT: return ::gWorld.GetMap()->GetTileAtLocation(U2DPoint<long>((long)a[0], (long)a[1])) != NULL;
    case EE_COMBAT_W_WALL_SPOT: {
        GE3DPoint thePoint(EEBitsFloat((int32_t)a[0]), EEBitsFloat((int32_t)a[1]), EEBitsFloat((int32_t)a[2]));
        bool theFound = EEGetWallSpot(theObject, EEQueryUnit(inOther), thePoint);
        a[0] = EEConditionBits(thePoint.GetX());
        a[1] = EEConditionBits(thePoint.GetY());
        a[2] = EEConditionBits(thePoint.GetZ());
        return theFound;
    }
    case EE_COMBAT_W_NEW_TARGET_NEAR_OLD: EEFindNewTargetNearOld(theObject, theOther); return 0;
    case EE_COMBAT_W_FIRING_RING: {
        std::vector<U2DPoint<long> > theTiles;
        std::vector<U2DPoint<long> > theOuterTiles;
        const GETransformation& theLocation = theObject->mPhysicalModel.GetLocation();
        GE3DPoint thePosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
        EEUnit* theTarget = EEQueryUnit(inOther);
        long theSize = theTarget->GetUnitClass()->GetTileSize();
        EEGetSortedRingTiles(thePosition, theTarget->GetTileX(), theTarget->GetTileY(), theSize, (long)a[0], 0, theOuterTiles);
        EEGetSortedRingTiles(thePosition, theTarget->GetTileX(), theTarget->GetTileY(), theSize, (long)a[0] + 1, 0, theTiles);
        theTiles.insert(theTiles.end(), theOuterTiles.begin(), theOuterTiles.end());
        std::sort(theTiles.begin(), theTiles.end(), EEUUTileNearerToUnit(theObject));
        GE3DPoint thePoint(EEBitsFloat((int32_t)a[1]), EEBitsFloat((int32_t)a[2]), EEBitsFloat((int32_t)a[3]));
        bool theFound = EEPickTileNear(theTiles, theObject, theOther, 0, thePoint);
        a[1] = EEConditionBits(thePoint.GetX());
        a[2] = EEConditionBits(thePoint.GetY());
        a[3] = EEConditionBits(thePoint.GetZ());
        return theFound;
    }
    case EE_COMBAT_W_HIT_CAPTURE:   // TEST ONLY: CBXA
        EECombatCaptureXAction(EE_COMBAT_XA_APPLY_HIT, theObject, EEQueryUnit(inOther), 0);
        return 0;
    }
    return 0;
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

int32_t EECombatWorld(int32_t inFunction, EEComplexUnit* inUnit, EEUnit* inTarget, EEUnitClass* inClass, long* ioX,
                      long* ioY)
{
    EEQueryContext theContext = {inUnit, NULL, NULL};
    theContext.mClass = inClass;
    EeCombatWorldHost theHost = {EEQueryHostOf(&theContext), EEWorldOp};
    int32_t theTile[2] = {ioX != NULL ? (int32_t)*ioX : 0, ioY != NULL ? (int32_t)*ioY : 0};
    int32_t theResult = ee_combat_world(inFunction, EEQueryHandleOf(inUnit), EEQueryHandleOf(inTarget), &theHost, &theTile);
    if (ioX != NULL) {
        *ioX = theTile[0];
        *ioY = theTile[1];
    }
    return theResult;
}

bool EECombatComputeDamage(int inForm, EEComplexUnit* inUnit, EEUnit* inTarget, float inAttack, long inAmmo, long inWeapon,
                           float inFriendlyFactor, long& outAmount)
{
    EEQueryContext theContext = {inUnit, NULL, NULL};
    theContext.mClass = inUnit->mUnitClass;
    EeCombatWorldHost theHost = {EEQueryHostOf(&theContext), EEWorldOp};
    EeCombatDamageCall theCall = {inForm, inAttack, (int32_t)inAmmo, (int32_t)inWeapon, inFriendlyFactor, 0};
    if (!ee_combat_compute_damage(&theCall, EEQueryHandleOf(inUnit), EEQueryHandleOf(inTarget), &theHost))
        return false;
    outAmount = theCall.amount;
    return true;
}

void EECombatAttackPosition(bool inFiring, EEComplexUnit* inAttacker, EEUnit* inTarget, GE3DPoint& ioLocation)
{
    EEQueryContext theContext = {inAttacker, NULL, NULL};
    EeCombatWorldHost theHost = {EEQueryHostOf(&theContext), EEWorldOp};
    float thePoint[3] = {ioLocation.GetX(), ioLocation.GetY(), ioLocation.GetZ()};
    ee_combat_attack_position(inFiring, EEQueryHandleOf(inAttacker), EEQueryHandleOf(inTarget), &theHost, &thePoint);
    ioLocation = GE3DPoint(thePoint[0], thePoint[1], thePoint[2]);
}

bool EECombatIsInReach(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    EEQueryContext theContext = {inUnit, NULL, NULL};
    EeCombatQueryHost theHost = EEQueryHostOf(&theContext);
    return ee_combat_in_reach(EEQueryHandleOf(inUnit), EEQueryHandleOf(inTarget), &theHost) != 0;
}

// EEUGAttackUnit::IsStillReachable (CBQR: the path attempts and the ignored targets' count after)
bool EECombatIsStillReachable(EEComplexUnit* inUnit, EEUGRust* inGoal)
{
    EECombatHostReadScope theScope;
    EEUnit* theTarget = inGoal->GetAttackTarget();
    EEQueryContext theContext = {inUnit, inGoal, NULL};
    EeCombatQueryHost theHost = EEQueryHostOf(&theContext);
    bool theReachable = ee_combat_still_reachable(EEQueryHandleOf(inUnit), &theHost) != 0;
    if (theScope.mOuter)
        EECombatCaptureHostRead(EE_COMBAT_HR_STILL_REACHABLE, inUnit, theTarget, theReachable, (uint32_t)inGoal->mPathAttempts,
                                (uint32_t)inUnit->mIgnoredTargets.size());
    return theReachable;
}

// EEUGAttackUnit::IsGateClosed (CBQR)
bool EECombatIsGateClosed(EEComplexUnit* inUnit, EEUGRust* inGoal)
{
    EECombatHostReadScope theScope;
    EEQueryContext theContext = {inUnit, inGoal, NULL};
    EeCombatQueryHost theHost = EEQueryHostOf(&theContext);
    bool theClosed = ee_combat_gate_closed(EEQueryHandleOf(inUnit), &theHost) != 0;
    if (theScope.mOuter)
        EECombatCaptureHostRead(EE_COMBAT_HR_GATE_CLOSED, inUnit, inGoal->GetAttackTarget(), theClosed);
    return theClosed;
}

bool EECombatHealInRange(EEComplexUnit* inHealer, EEUnit* inTarget)
{
    EEQueryContext theContext = {inHealer, NULL, NULL};
    EeCombatQueryHost theHost = EEQueryHostOf(&theContext);
    return ee_combat_heal_in_range(EEQueryHandleOf(inHealer), EEQueryHandleOf(inTarget), &theHost) != 0;
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
