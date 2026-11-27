// New (port): the attack, attack-ground and heal goals' trigger tests and steps, and the shots' trigger tests, as
// plain records (docs/RUST_PLAN.md R5b.2, sixth round; rust/crates/ee_sim/src/combat/goals.rs). The C++ fills the plain
// reads of a test (EeCombatConditionIn, null-safe) or a step (EeCombatStepIn); the lazy reads and the step's
// operations are logged into the record in the order they are made.
//
// With EE_SIM_CAPTURE set (ee/EESimCapture.h, tools/sim_capture.py) every test is folded into a CBTC record (some ten
// million tests a calamity game: one record per 4096 tests, their count and the FNV-1a 64 of each test's record and
// EeCombatConditionOut in order, the last batch at the game's end; EE_SIM_CAPTURE_CBTC_FULL=1 writes every test's
// bytes instead, for finding where two builds part) and every step appends a CBGS record (the record with its
// operations). A test that calls another captured test (the torpedo's other triggers, a base class's test) records
// both, the inner one first. The golden lines were recorded from the original C++ bodies (instrumented in place)
// before Rust took them.
#ifndef EE_EECOMBATGOALS_H
#define EE_EECOMBATGOALS_H

#include <stdint.h>
#include <string.h>

#include "rust/sim/combat.h"

class EEComplexUnit;
class EEUnit;
class EEUnitGoal;
class EEUnitClass;
typedef EEUnitGoal EEUGRust;
class EEPlayer;
class GE3DPoint;

// an object's handle for the Rust (a complex unit's EEUnit base is at its address: its first base)
inline uint64_t EECombatObj(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }

// The trigger test inKind (EE_COMBAT_GOAL_*) of inUnit on inEvent with the target inTarget (NULL: none): its record
// filled (ee_combat store.rs; the attack goal's last target tile and convert flag from inGoal), the test through
// ee_combat_condition over the C++ host (inGoal for the goals, inClass for the shots), the action applied (a shot's
// attack goal on the unit it hit), then the CBTC record. Returns the test's value (ee_sim combat cg_goal.rs).
inline long EECombatCondition(int32_t inKind, EEComplexUnit* inUnit, unsigned long inEvent, EEUnit* inTarget,
                              EEUnitGoal* inGoal, EEUnitClass* inClass)
    { return ee_units_cg_condition(inKind, EECombatObj(inUnit), (uint32_t)inEvent, EECombatObj(inTarget), EECombatObj(inGoal), EECombatObj(inClass)); }
// The step of inGoal (inKind) through ee_combat_goal_step: the plan's operations executed on inUnit and inGoal as
// they are issued, then the CBGS record
inline void EECombatStep(int32_t inKind, EEComplexUnit* inUnit, EEUnitGoal* inGoal, EEUnit* inTarget)
    { ee_units_cg_step(inKind, EECombatObj(inUnit), EECombatObj(inGoal), EECombatObj(inTarget)); }
// A shot's target: its attack-unit goal's (NULL under another goal; cg_query.rs)
inline EEUnit* EECombatShotTarget(EEComplexUnit* inUnit) { return (EEUnit*)(uintptr_t)ee_units_cgq_shot_target(EECombatObj(inUnit)); }

// CBQR (TEST ONLY): one call of a read behind the hosts (R5b.2, seventh round), its outcome: the function, the
// unit's and target's ids (0 without one), the result and what the call left behind (per function below). Folded
// like CBTC, 4096 calls to a record (count, FNV-1a 64; EE_SIM_CAPTURE_CBQR_FULL=1 writes every call). A read made
// inside another captured read (FacesTarget's FacesPoint, IsInReach's facing) records nothing: only the outer call.
// (the function codes: ee_sim combat cg_query.rs, rust/sim/combat.h)
// (the records' words: ee_sim combat capture_units.rs, read through the views)
inline void EECombatCaptureHostRead(uint32_t inFunc, const EEComplexUnit* inUnit, const EEUnit* inTarget, int32_t inResult,
                                    uint32_t inExtra0 = 0, uint32_t inExtra1 = 0, uint32_t inExtra2 = 0)
{
    ee_combat_capture_read_of(0, inFunc, (uint64_t)(uintptr_t)inUnit, (uint64_t)(uintptr_t)inTarget, inResult, inExtra0, inExtra1, inExtra2);
}
// The reads' bodies through ee_combat reads.rs over the C++ host (EECombatGoals.cpp): FindImpactUnit, IsInReach,
// EEUGAttackUnit's IsStillReachable and IsGateClosed (inGoal the attack goal; their CBQR records here),
// EEIsEnemyWithinRange, FacesPoint (inClass `this`), EEUGHealUnit's IsInRange (the healer's heal range)
// (ee_sim combat cg_query.rs: the marshalling and the CBQR records)
inline bool EECombatIsInReach(EEComplexUnit* inUnit, EEUnit* inTarget) { return ee_units_cgq_in_reach(EECombatObj(inUnit), EECombatObj(inTarget)); }
inline bool EECombatIsStillReachable(EEComplexUnit* inUnit, EEUGRust* inGoal)
    { return ee_units_cgq_still_reachable(EECombatObj(inUnit), EECombatObj(inGoal)); }
inline bool EECombatIsGateClosed(EEComplexUnit* inUnit, EEUGRust* inGoal)
    { return ee_units_cgq_gate_closed(EECombatObj(inUnit), EECombatObj(inGoal)); }
inline bool EECombatHealInRange(EEComplexUnit* inHealer, EEUnit* inTarget)
    { return ee_units_cgq_heal_in_range(EECombatObj(inHealer), EECombatObj(inTarget)); }

// (the heal's work, EE_COMBAT_SO_HEAL_WORK, is recorded by ee_sim combat/goal_objects.rs)

// The shots' ActionChanged and Update through ee_combat shots.rs over the host in EEUCProjectile.cpp (inKind
// EE_COMBAT_SHOT_*, inClass the original's `this`)

// CBXR (TEST ONLY): one call of the eighth round's reads (R5b.2), its outcome: the function, the unit's and target's
// ids (0 without one), the result and what the call left behind (per function below); folded like CBQR
// (EE_SIM_CAPTURE_CBXR_FULL=1 writes every call). Every call records (none of them calls another).
// (the function codes: ee_sim combat cg_query.rs, rust/sim/combat.h)
inline void EECombatCaptureXRead(uint32_t inFunc, const EEComplexUnit* inUnit, const EEUnit* inTarget, int32_t inResult,
                                 uint32_t inExtra0 = 0, uint32_t inExtra1 = 0)
{
    ee_combat_capture_read_of(1, inFunc, (uint64_t)(uintptr_t)inUnit, (uint64_t)(uintptr_t)inTarget, inResult, inExtra0, inExtra1, 0);
}
// IsTargetIgnored's and IgnoreTarget's record: the list's size and FNV-1a 32 (each entry's six fields in order)
inline void EECombatCaptureIgnored(uint32_t inFunc, const EEComplexUnit* inUnit, const EEComplexUnit* inTarget, int32_t inResult,
                                   uint32_t inCheckLocation)
    { ee_units_cgq_capture_ignored(inFunc, EECombatObj(inUnit), EECombatObj(inTarget), inResult, inCheckLocation); }
// CBGO (TEST ONLY): the attack, attack-ground and heal goals' GetDestination and Suits, at each call's end: the
// function, the goal's kind, the unit's and target's ids (0 without one), the result, a point's x, y, z bits
// (GetDestination's out location) and two extras (per function below); folded like CBQR (EE_SIM_CAPTURE_CBGO_FULL=1).
// Recorded from the original C++ bodies before the goals moved to EEUGRust (R5b.2, twelfth round).
// (the function codes: ee_sim combat cg_query.rs, rust/sim/combat.h)
// (the CBGO record: ee_sim combat/goal_objects.rs capture_op)
// A body of ee_combat world.rs (EE_COMBAT_WORLD_*: IsTargetIgnored, IgnoreTarget, CanReachUnit,
// IsOnTileBorderingTarget, EEIsAttackReachable, Attacked, FireShot) of inUnit on inTarget over the world host
// (EECombatGoals.cpp); inClass the class of Attacked or FireShot (`this`), ioX, ioY EEIsAttackReachable's tile (written
// when the body sets it). Returns the body's answer (0/1).
inline int32_t EECombatWorld(int32_t inFunction, EEComplexUnit* inUnit, EEUnit* inTarget, EEUnitClass* inClass = NULL,
                             long* ioX = NULL, long* ioY = NULL)
    { return ee_units_cgq_world(inFunction, EECombatObj(inUnit), EECombatObj(inTarget), EECombatObj(inClass), ioX, ioY); }
// EEFindDropLocation (ee_combat leaves.rs find_drop_location) over the world host: the target out (NULL: none), the
// point in outLocation.
inline EEUnit* EECombatFindDropLocation(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& outLocation)
    { return (EEUnit*)(uintptr_t)ee_units_cgq_find_drop_location(EECombatObj(inUnit), EECombatObj(inTarget), (float(*)[3])&outLocation); }
// EEGetHitDistanceSquared (ee_combat leaves.rs hit_distance_squared) over the world host.
inline float EECombatHitDistanceSquared(EEComplexUnit* inUnit, EEUnit* inTarget, float inX, float inY)
    { return ee_units_cgq_hit_distance_squared(EECombatObj(inUnit), EECombatObj(inTarget), inX, inY); }
// EEIsSeenBy (ee_combat leaves.rs is_seen_by) over the world host.
inline bool EECombatIsSeenBy(EEUnit* inUnit, EEPlayer* inPlayer, bool inExplored)
    { return ee_units_cgq_is_seen_by(EECombatObj(inUnit), EECombatObj(inPlayer), inExplored); }
// The damage of one hit (ee_combat damage.rs compute_damage; ee/EERules.h EEDamageForm, combat.yaml's constants):
// inUnit hits inTarget with inAttack, attack kind inAmmo (dbfamily column) and weapon inWeapon (dbweapontohit row);
// the elevation rule is inUnit's class's. inFriendlyFactor: splash on the own player's units. False when the hit does
// not get through (outAmount untouched), else outAmount. The trample's (EEPhysicsModel); ApplyHit and the splash are
// damage.rs's.
bool EECombatComputeDamage(int inForm, EEComplexUnit* inUnit, EEUnit* inTarget, float inAttack, long inAmmo, long inWeapon,
                           float inFriendlyFactor, long& outAmount);

// The nesting guard of a captured read (per thread and slot: 0 the reads, 1 the shots' ActionChanged, 2 their
// Update, 3 the heal goal's Work): mOuter when no other captured call of the slot is running
// (the depth is ee_sim combat cg_query.rs's)
struct EECombatHostReadScope {
    explicit EECombatHostReadScope(int inSlot = 0) : mSlot(inSlot), mOuter(ee_combat_host_read_enter(inSlot)) {}
    ~EECombatHostReadScope() { ee_combat_host_read_leave(this->mSlot); }
    int mSlot;
    bool mOuter;
};

#endif
