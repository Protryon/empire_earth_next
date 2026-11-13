// New (port; docs/RUST_PLAN.md R5b.4, rounds 16 to 22; R5b.2 for the attack and heal goals): EEUGRust, the class
// every goal class moved into, is since round 22 the goal class itself (ee/EEUnitGoal.h: no base class and no
// vtable left); the name stays for its callers. Here: the kind casts and the helpers of EEUGRust.cpp and
// EEUGRustCombat.cpp.
#ifndef EE_EEUGRUST_H
#define EE_EEUGRUST_H

#include <stdint.h>

#include "ee/EEUnitGoal.h"

class FSFile;
class GE3DPoint;
struct EeUnitsGoalIo;

typedef EEUnitGoal EEUGRust;

// one body of inGoal through the kind dispatch (ee_units_rust_goal over EEUGRust.cpp's host), for the accessors
// defined outside EEUGRust.cpp (EEUGRustCombat.cpp)
int64_t EEUGRustGoal(int inOperation, const EEUGRust* inGoal, uint64_t inA = 0, uint64_t inB = 0,
                     EeUnitsGoalIo* ioArgs = NULL, FSFile* inFile = NULL);
// the combat goals' host operations (EE_COMBAT_G_*, from 1000; EEUGRustCombat.cpp), the host's other cases
int64_t EECombatGoalHostOp(FSFile* inFile, int32_t inOp, uint64_t inA, uint64_t inB, EeUnitsGoalIo* ioArgs);

// the goal of kind inKind (the old EEGoalCast<EEUGPatrol>, <EEUGCastCalamity>, <EEUGExplore>,
// <EEUGUnloadTransport>), NULL for another or none
inline EEUGRust* EERustGoalCast(EEUnitGoal* inGoal, unsigned char inKind)
{
    return inGoal != NULL && inGoal->mKind == inKind ? static_cast<EEUGRust*>(inGoal) : NULL;
}
inline EEUGRust* EEPatrolGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalPatrol);
}
inline EEUGRust* EECastCalamityGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalCastCalamity);
}
inline EEUGRust* EEExploreGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalExplore);
}
inline EEUGRust* EEUnloadGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalUnloadTransport);
}
// the old EEGoalCast<EEUGBuildRepair>
inline EEUGRust* EEBuildRepairGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalBuildRepair);
}
// the old EEGoalCast<EEUGGather> and dynamic_cast<EEUGGather*>
inline EEUGRust* EEGatherGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalGather);
}
// the attack-ground and attack-unit goals (the old EEGoalCast<EEUGAttackGround>, <EEUGAttackUnit>)
inline EEUGRust* EEAttackGroundCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalAttackGround);
}
inline EEUGRust* EEAttackUnitCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalAttackUnit);
}
// the heal goal (the old EEGoalCast<EEUGHealUnit>)
inline EEUGRust* EEHealGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalHealUnit);
}
// the location goal (the old EEGoalCast<EEUGLocation> and dynamic_cast to EEUGLocation once the heal goal no longer
// derived from it), NULL for another kind or none
inline EEUGRust* EELocationGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalLocation);
}
// the location goal's class as its callers saw it (the old EEGoalCast<EEUGLocation> != NULL): a location goal or a
// heal goal, which derived from it until it moved to Rust (R5b.2, the fourteenth round)
inline bool EEGoalHasLocation(const EEUnitGoal* inGoal)
{
    return inGoal != NULL && (inGoal->mKind == kGoalLocation || inGoal->mKind == kGoalHealUnit);
}
// such a goal's location goal members (EEUGRustCombat.cpp): its location (GetLocation, SetLocation) and yaw (GetYaw;
// a heal goal's is 0); 0, 0, 0 and 0 for another goal
GE3DPoint EELocationGoalPoint(EEUnitGoal* inGoal);
void EESetLocationGoalPoint(EEUnitGoal* inGoal, const GE3DPoint& inLocation);
float EELocationGoalYaw(EEUnitGoal* inGoal);
// the old dynamic_cast<EEUGLoadTransport*>, <EEUGLoadGarrison*> and <EEUGLoadContainer*> (either)
inline EEUGRust* EELoadTransportGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalLoadTransport);
}
inline EEUGRust* EELoadGarrisonGoalCast(EEUnitGoal* inGoal)
{
    return EERustGoalCast(inGoal, kGoalLoadGarrison);
}
inline EEUGRust* EELoadGoalCast(EEUnitGoal* inGoal)
{
    EEUGRust* theGoal = EELoadTransportGoalCast(inGoal);
    return theGoal != NULL ? theGoal : EELoadGarrisonGoalCast(inGoal);
}

#endif
