// New (port; docs/RUST_PLAN.md R5b.2, the twelfth and fourteenth rounds): the combat lane's goals on EEUGRust
// (ee/EEUGRust.h): the attack on the ground (kGoalAttackGround; was EEUGAttackGround 0x21ce6c..0x21d2f4), the attack
// on a unit (kGoalAttackUnit; was EEUGAttackUnit 0x21d2f4..0x21e0c1) and the heal (kGoalHealUnit; was EEUGHealUnit
// 0x22528d..0x22585a). EEUGRust's virtuals go through ee_units' goal dispatch (rust_goal.rs), which hands these kinds
// to ee_combat goal_dispatch.rs: their state and bodies are Rust's (goal_objects.rs, goal_attack.rs, goal_heal.rs; the
// trigger tests and steps goals.rs's). Here: the combat goals' operations of the
// goals' host (EEUGRust.cpp's GoalOp's default case), each the original's statement, the CBGO records (TEST ONLY,
// recorded from the original bodies first), and the accessors the callers use.
#include <windows.h>

#include "lle/LowLevelEngine.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EECombatGoals.h"
#include "ee/EEAreaUtilities.h"

namespace {

EEComplexUnit* UnitOf(uint64_t inHandle) { return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(inHandle)); }

EEUnitGoal* UnitGoalOf(uint64_t inHandle) { return reinterpret_cast<EEUnitGoal*>(static_cast<uintptr_t>(inHandle)); }

}  // namespace

// the combat goals' operations of the goals' host (ee_combat EE_COMBAT_G_*, from 1000: EEUGRust.cpp's GoalOp
// default case), each the original's statement
int64_t EECombatGoalHostOp(int32_t inCode, uint64_t inUnit, uint64_t inB, EeUnitsGoalIo* ioArgs)
{
    switch (inCode) {
    case EE_COMBAT_G_CAN_REACH_TILE: return UnitOf(inUnit)->CanReachTile((long)(ioArgs->i)[0], (long)(ioArgs->i)[1]);
    case EE_COMBAT_G_TELL_GROUP:
        if (UnitOf(inUnit)->mActiveGroup == NULL)
            return 0;
        UnitOf(inUnit)->mActiveGroup->TellGroup(UnitOf(inUnit), GE3DPoint(UBitsFloat((ioArgs->i)[0]), UBitsFloat((ioArgs->i)[1]), UBitsFloat((ioArgs->i)[2])));
        return 1;
    case EE_COMBAT_G_VERIFY_TARGET: {
        EEUnit* theTarget = reinterpret_cast<EEUnit*>(UnitOf((uint64_t)(ioArgs->i)[0]));
        ESVERIFY_AT(ee_units_uu_targetable((uint64_t)(uintptr_t)UnitOf(inUnit), (uint64_t)(uintptr_t)theTarget->GetClass(), 0) != 0, 729);
        ESVERIFY_AT(theTarget->GetClass()->GetWeapon() >= 0 && theTarget->GetClass()->GetWeapon() < keeucwTotal, 730);
        return 0;
    }
    // (EE_COMBAT_G_POSITION, EE_COMBAT_G_EVALUATE: ee_sim combat goal_objects.rs common_op)
    // the dispatch's (ee_combat goal_dispatch.rs; the projectile test and the TEST ONLY records: goal_objects.rs)
    case EE_COMBAT_G_STEP: EECombatStep((int32_t)(ioArgs->i)[0], UnitOf(inUnit), UnitGoalOf((uint64_t)(ioArgs->i)[2]), reinterpret_cast<EEUnit*>(UnitOf((uint64_t)(ioArgs->i)[1]))); return 0;
    case EE_COMBAT_G_PATH_ATTEMPTS: return UnitGoalOf(inUnit)->mPathAttempts;
    case EE_COMBAT_G_CLASS_NAME: return UnitOf(inUnit)->mUnitClass != NULL ? (int64_t)(uintptr_t)(const char*)UnitOf(inUnit)->mUnitClass->mName : 0;
    // the heal goal's (ee_combat goal_heal.rs)
    case EE_COMBAT_G_RELEASE_HEAL_EFFECT: UnitOf(inUnit)->ReleaseHealEffect(); return 0;
    case EE_COMBAT_G_CAN_HEAL: return EECanHeal(reinterpret_cast<EEUnit*>(UnitOf(inUnit)), UnitOf((uint64_t)(ioArgs->i)[0]));
    }
    return 0;
}

// (the goals' accessors, NewAttackGround and the location members of any goal: inline in ee/EEUnitGoal.h and ee/EEUGRust.h)
