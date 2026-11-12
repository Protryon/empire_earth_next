// New (port): the plain reads of the goals' and shots' trigger tests and of the goals' steps (ee/EECombatGoals.h;
// rust/crates/ee_combat/src/goals.rs; docs/RUST_PLAN.md R5b.2, sixth round), and their CBTC/CBGS capture records.
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
#include "ee/EECombatFormulas.h"
#include "ee/EECombatGoals.h"

thread_local EECombatConditionScope* EECombatConditionScope::sCurrent = NULL;

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

void EECombatCaptureFlush()
{
    if (!EESimCaptureRecord::Active())
        return;
    std::lock_guard<std::mutex> theGuard(sConditionLock);
    if (sConditionCount != 0)
        EEConditionWrite();
}

void EECombatCaptureStep(const EeCombatStepIn& in)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord theRecord(EE_COMBAT_TAG('C', 'B', 'G', 'S'));
    theRecord.Bytes(&in, sizeof(in));
    theRecord.Commit();
}
