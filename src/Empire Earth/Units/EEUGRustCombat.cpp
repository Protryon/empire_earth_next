// New (port; docs/RUST_PLAN.md R5b.2, the twelfth and fourteenth rounds): the combat lane's goals on EEUGRust
// (ee/EEUGRust.h): the attack on the ground (kGoalAttackGround; was EEUGAttackGround 0x21ce6c..0x21d2f4), the attack
// on a unit (kGoalAttackUnit; was EEUGAttackUnit 0x21d2f4..0x21e0c1) and the heal (kGoalHealUnit; was EEUGHealUnit
// 0x22528d..0x22585a). EEUGRust's virtuals go through ee_units' goal dispatch (rust_goal.rs), which hands these kinds
// to ee_combat goal_dispatch.rs: their state and bodies are Rust's (goal_objects.rs, goal_attack.rs, goal_heal.rs; the
// trigger tests and steps goals.rs's). Here: the combat goals' operations of the
// goals' host (EEUGRust.cpp's GoalOp's default case), each the original's statement, the CBGO records (TEST ONLY,
// recorded from the original bodies first), and the accessors the callers use.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnit.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EECombatGoals.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EESaveGameFiles.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUnitKind.h"
#include "rust/sim/combat.h"
#include "rust/sim/units.h"
#include "rust/sim/combat.h"
#include "rust/sim/units.h"

namespace {

uint64_t Handle(const void* inObject)
{
    return reinterpret_cast<uintptr_t>(inObject);
}

EEComplexUnit* UnitOf(uint64_t inHandle)
{
    return reinterpret_cast<EEComplexUnit*>(static_cast<uintptr_t>(inHandle));
}

uint32_t Bits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

float Float(int64_t inBits)
{
    uint32_t theBits = (uint32_t)inBits;
    float theValue;
    memcpy(&theValue, &theBits, 4);
    return theValue;
}

EEUnitGoal* UnitGoalOf(uint64_t inHandle)
{
    return reinterpret_cast<EEUnitGoal*>(static_cast<uintptr_t>(inHandle));
}


}  // namespace

// the combat goals' operations of the goals' host (ee_combat EE_COMBAT_G_*, from 1000: EEUGRust.cpp's GoalOp
// default case), each the original's statement
int64_t EECombatGoalHostOp(int32_t inCode, uint64_t inUnit, uint64_t inB, EeUnitsGoalIo* ioArgs)
{
    int64_t* a = ioArgs->i;
    EEComplexUnit* theUnit = UnitOf(inUnit);
    switch (inCode) {
    case EE_COMBAT_G_CAN_REACH_TILE:
        return theUnit->CanReachTile((long)a[0], (long)a[1]);
    case EE_COMBAT_G_TELL_GROUP:
        if (theUnit->mActiveGroup == NULL)
            return 0;
        theUnit->mActiveGroup->TellGroup(theUnit, GE3DPoint(Float(a[0]), Float(a[1]), Float(a[2])));
        return 1;
    case EE_COMBAT_G_VERIFY_TARGET: {
        EEUnit* theTarget = reinterpret_cast<EEUnit*>(UnitOf((uint64_t)a[0]));
        ESVERIFY_AT(ee_units_uu_targetable((uint64_t)(uintptr_t)theUnit, (uint64_t)(uintptr_t)theTarget->GetClass(), 0) != 0, 729);
        ESVERIFY_AT(theTarget->GetClass()->GetWeapon() >= 0 && theTarget->GetClass()->GetWeapon() < keeucwTotal, 730);
        return 0;
    }
    // (EE_COMBAT_G_POSITION, EE_COMBAT_G_EVALUATE: ee_sim combat goal_objects.rs common_op)
    // the dispatch's (ee_combat goal_dispatch.rs; the projectile test and the TEST ONLY records: goal_objects.rs)
    case EE_COMBAT_G_STEP:
        EECombatStep((int32_t)a[0], theUnit, UnitGoalOf((uint64_t)a[2]), reinterpret_cast<EEUnit*>(UnitOf((uint64_t)a[1])));
        return 0;
    case EE_COMBAT_G_PATH_ATTEMPTS: return UnitGoalOf(inUnit)->mPathAttempts;
    case EE_COMBAT_G_CLASS_NAME: {
        EEUnitClass* theClass = reinterpret_cast<EEUnit*>(theUnit)->mUnitClass;
        return theClass != NULL ? (int64_t)(uintptr_t)(const char*)theClass->mName : 0;
    }
    // the heal goal's (ee_combat goal_heal.rs)
    case EE_COMBAT_G_RELEASE_HEAL_EFFECT: theUnit->ReleaseHealEffect(); return 0;
    case EE_COMBAT_G_CAN_HEAL: return EECanHeal(reinterpret_cast<EEUnit*>(theUnit), UnitOf((uint64_t)a[0]));
    }
    return 0;
}

EEUGRust* EEUGRust::NewAttackGround(const GE3DPoint& inTarget, bool inArg)
{
    return reinterpret_cast<EEUGRust*>(ee_units_goal_new_point(kGoalAttackGround, inArg, EE_COMBAT_GOAL_NEW_GROUND, inTarget.GetX(),
                                                               inTarget.GetY(), inTarget.GetZ()));
}

GE3DPoint EEUGRust::GetGroundTarget()
{
    return EEGoalPointOf(ee_combat_ground_target_point(Handle(this)));
}

GE3DPoint EEUGRust::GetHealLocation() const
{
    return EEGoalPointOf(ee_combat_heal_location(Handle(this)));
}

void EEUGRust::SetHealLocation(const GE3DPoint& inLocation)
{
    ee_combat_heal_set_location(Handle(this), Bits(inLocation.GetX()), Bits(inLocation.GetY()), Bits(inLocation.GetZ()));
}

// (ee_units goal_calls.rs: a location goal's, a heal goal's, else 0, 0, 0; the yaw a location goal's, else 0)
GE3DPoint EELocationGoalPoint(EEUnitGoal* inGoal)
{
    return EEGoalPointOf(ee_units_goal_location_point_any(reinterpret_cast<const EeUnitsGoalBase*>(inGoal)));
}

void EESetLocationGoalPoint(EEUnitGoal* inGoal, const GE3DPoint& inLocation)
{
    ee_units_goal_set_location_point_any(reinterpret_cast<const EeUnitsGoalBase*>(inGoal), inLocation.GetX(), inLocation.GetY(),
                                         inLocation.GetZ());
}

float EELocationGoalYaw(EEUnitGoal* inGoal)
{
    return ee_units_goal_location_yaw_any(reinterpret_cast<const EeUnitsGoalBase*>(inGoal));
}
