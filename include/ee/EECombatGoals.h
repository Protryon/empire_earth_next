// New (port): the attack, attack-ground and heal goals' trigger tests and steps, and the shots' trigger tests, as
// plain records (docs/RUST_PLAN.md R5b.2, sixth round; rust/crates/ee_combat/src/goals.rs). The C++ fills the plain
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

#include "rust/ee_combat.h"
#include "ee/EESimCapture.h"

class EEComplexUnit;
class EEUnit;

// CBTC, CBGS (no record while the capture is off)
void EECombatCaptureCondition(const EeCombatConditionIn& in, const EeCombatConditionOut& inOut);
void EECombatCaptureStep(const EeCombatStepIn& in);
// writes the tests' last CBTC batch (EE-SIM at the game's end, before the capture closes)
void EECombatCaptureFlush();

// The trigger test inKind (EE_COMBAT_GOAL_*) of inUnit on inEvent with the target inTarget (NULL: none): its record
// filled (ee_combat store.rs; the attack goal's last target tile and convert flag from inGoal), the test through
// ee_combat_condition over the C++ host (inGoal for the goals, inClass for the shots), the action applied (a shot's
// attack goal on the unit it hit), then the CBTC record. Returns the test's value. Defined in EECombatGoals.cpp.
class EEUnitGoal;
class EEUnitClass;
long EECombatCondition(int32_t inKind, EEComplexUnit* inUnit, unsigned long inEvent, EEUnit* inTarget,
                       EEUnitGoal* inGoal, EEUnitClass* inClass);
// The step of inGoal (inKind) through ee_combat_goal_step: the plan's operations executed on inUnit and inGoal as
// they are issued, then the CBGS record
void EECombatStep(int32_t inKind, EEComplexUnit* inUnit, EEUnitGoal* inGoal, EEUnit* inTarget);
// A shot's target: its attack-unit goal's (NULL under another goal)
EEUnit* EECombatShotTarget(EEComplexUnit* inUnit);

// CBQR (TEST ONLY): one call of a read behind the hosts (R5b.2, seventh round), its outcome: the function, the
// unit's and target's ids (0 without one), the result and what the call left behind (per function below). Folded
// like CBTC, 4096 calls to a record (count, FNV-1a 64; EE_SIM_CAPTURE_CBQR_FULL=1 writes every call). A read made
// inside another captured read (FacesTarget's FacesPoint, IsInReach's facing) records nothing: only the outer call.
enum {
    EE_COMBAT_HR_IMPACT_UNIT = 1,           // FindImpactUnit: extra 0 the unit hit's id, 1 the owner's mIsFiringHigh
    EE_COMBAT_HR_STILL_REACHABLE = 2,       // IsStillReachable: extra 0 mPathAttempts, 1 the ignored targets' count
    EE_COMBAT_HR_GATE_CLOSED = 3,           // IsGateClosed
    EE_COMBAT_HR_ENEMY_IN_RANGE = 4,        // EEIsEnemyWithinRange: extra 0 mTargetSearchTimer
    EE_COMBAT_HR_IN_REACH = 5,              // IsInReach: extra 0 mWeaponTagPoint before, 1 after
    EE_COMBAT_HR_FACES_POINT = 6,           // FacesPoint: extra 0, 1 the point's x, y bits, 2 inUseTolerance
    EE_COMBAT_HR_FACES_TARGET = 7,          // FacesTarget: extra 2 inUseTolerance
};
void EECombatCaptureHostRead(uint32_t inFunc, const EEComplexUnit* inUnit, const EEUnit* inTarget, int32_t inResult,
                             uint32_t inExtra0 = 0, uint32_t inExtra1 = 0, uint32_t inExtra2 = 0);
// The reads' bodies through ee_combat reads.rs over the C++ host (EECombatGoals.cpp): FindImpactUnit, IsInReach,
// EEUGAttackUnit's IsStillReachable and IsGateClosed (inGoal the attack goal; their CBQR records here),
// EEIsEnemyWithinRange, FacesPoint (inClass `this`), EEUGHealUnit's IsInRange (the healer's heal range)
class EEUGRust;
class EEPlayer;
class GE3DPoint;
EEUnit* EECombatFindImpactUnit(EEComplexUnit* inUnit);
bool EECombatIsInReach(EEComplexUnit* inUnit, EEUnit* inTarget);
bool EECombatIsStillReachable(EEComplexUnit* inUnit, EEUGRust* inGoal);
bool EECombatIsGateClosed(EEComplexUnit* inUnit, EEUGRust* inGoal);
bool EECombatIsEnemyWithinRange(EEComplexUnit* inUnit);
bool EECombatHealInRange(EEComplexUnit* inHealer, EEUnit* inTarget);
bool EECombatFacesPoint(EEUnitClass* inClass, EEComplexUnit* inUnit, const GE3DPoint& inLocation, bool inUseTolerance);

// CBSO (TEST ONLY): the outcome of a shot's ActionChanged or Update (the unit's command before and after, animation,
// physics action, hit points, goal and its target, location, rotting, flight and lifetime timers, opacity, velocity,
// its player's population) or of the heal goal's Work (the target's hit points, the elapsed time); folded like CBQR
// (EE_SIM_CAPTURE_CBSO_FULL=1: every call), only the outermost call of each kind (scope slots 1 to 3)
enum {
    EE_COMBAT_SO_PROJECTILE_ACTION = 1,
    EE_COMBAT_SO_MISSILE_ACTION = 2,
    EE_COMBAT_SO_PROJECTILE_UPDATE = 3,
    EE_COMBAT_SO_MISSILE_UPDATE = 4,
    EE_COMBAT_SO_TORPEDO_UPDATE = 5,
    EE_COMBAT_SO_HEAL_WORK = 6,
};
uint32_t EECombatCommandOf(const EEComplexUnit* inUnit);
void EECombatCaptureShot(uint32_t inFunc, const EEComplexUnit* inUnit, uint32_t inBefore);
void EECombatCaptureHealWork(const EEComplexUnit* inUnit, const EEUnit* inTarget, unsigned long inElapsed);

// The shots' ActionChanged and Update through ee_combat shots.rs over the host in EEUCProjectile.cpp (inKind
// EE_COMBAT_SHOT_*, inClass the original's `this`)
void EECombatShotAction(int32_t inKind, EEUnitClass* inClass, EEComplexUnit* inUnit);
void EECombatShotUpdate(int32_t inKind, EEUnitClass* inClass, EEComplexUnit* inUnit);

// CBXR (TEST ONLY): one call of the eighth round's reads (R5b.2), its outcome: the function, the unit's and target's
// ids (0 without one), the result and what the call left behind (per function below); folded like CBQR
// (EE_SIM_CAPTURE_CBXR_FULL=1 writes every call). Every call records (none of them calls another).
enum {
    EE_COMBAT_XR_IS_IGNORED = 1,        // IsTargetIgnored: extra 0 inCheckLocation, 1 the list's size after, 2 its hash
    EE_COMBAT_XR_IGNORE = 2,            // IgnoreTarget: extra 1 the list's size after, 2 its hash
    EE_COMBAT_XR_CAN_REACH_UNIT = 3,    // CanReachUnit
    EE_COMBAT_XR_ATTACK_REACHABLE = 4,  // EEIsAttackReachable: extra 0, 1 the tile's x, y after
    EE_COMBAT_XR_BORDERING = 5,         // IsOnTileBorderingTarget
};
void EECombatCaptureXRead(uint32_t inFunc, const EEComplexUnit* inUnit, const EEUnit* inTarget, int32_t inResult,
                          uint32_t inExtra0 = 0, uint32_t inExtra1 = 0);
// IsTargetIgnored's and IgnoreTarget's record: the list's size and FNV-1a 32 (each entry's six fields in order)
void EECombatCaptureIgnored(uint32_t inFunc, const EEComplexUnit* inUnit, const EEComplexUnit* inTarget, int32_t inResult,
                            uint32_t inCheckLocation);
// CBXA (TEST ONLY): the outcome of FireShot, Attacked, ApplyHit and ApplyAreaDamage, at each call's end: the unit's
// command, animation, hit points, reload timer, weapon tag point, ammunition, flight time, special attack and
// firing high flags, experience, goal kind and attack target, last attacker and the target's hit points and
// resource amount (inTarget: FireShot's attack goal's target after the shot, ApplyHit's and ApplyAreaDamage's
// inTarget, none for Attacked); folded like CBQR (EE_SIM_CAPTURE_CBXA_FULL=1)
enum {
    EE_COMBAT_XA_FIRE_SHOT = 1,
    EE_COMBAT_XA_ATTACKED = 2,          // inBefore: the last attacker's id before
    EE_COMBAT_XA_APPLY_HIT = 3,
    EE_COMBAT_XA_AREA_DAMAGE = 4,       // inBefore: the victims' FNV-1a 32 (id, hit points, resource amount)
};
void EECombatCaptureXAction(uint32_t inFunc, EEComplexUnit* inUnit, EEUnit* inTarget, uint32_t inBefore);
// CBGO (TEST ONLY): the attack, attack-ground and heal goals' GetDestination and Suits, at each call's end: the
// function, the goal's kind, the unit's and target's ids (0 without one), the result, a point's x, y, z bits
// (GetDestination's out location) and two extras (per function below); folded like CBQR (EE_SIM_CAPTURE_CBGO_FULL=1).
// Recorded from the original C++ bodies before the goals moved to EEUGRust (R5b.2, twelfth round).
enum {
    EE_COMBAT_GO_DESTINATION = 1,       // extra 0, 1: the attack goal's last target tile after
    EE_COMBAT_GO_SUITS = 2,             // attack ground: the target point's bits
};
void EECombatCaptureGoal(uint32_t inFunc, uint32_t inKind, const EEUnit* inUnit, const EEUnit* inTarget, int32_t inResult,
                         const float* inPoint = NULL, uint32_t inExtra0 = 0, uint32_t inExtra1 = 0);
// A body of ee_combat world.rs (EE_COMBAT_WORLD_*: IsTargetIgnored, IgnoreTarget, CanReachUnit,
// IsOnTileBorderingTarget, EEIsAttackReachable, Attacked, FireShot) of inUnit on inTarget over the world host
// (EECombatGoals.cpp); inClass the class of Attacked or FireShot (`this`), ioX, ioY EEIsAttackReachable's tile (written
// when the body sets it). Returns the body's answer (0/1).
int32_t EECombatWorld(int32_t inFunction, EEComplexUnit* inUnit, EEUnit* inTarget, EEUnitClass* inClass = NULL,
                      long* ioX = NULL, long* ioY = NULL);
// EEGetFiringPosition (inFiring) or EEGetLeadPosition (ee_combat leaves.rs firing_position, lead_position) over the
// world host: ioLocation in and out.
void EECombatAttackPosition(bool inFiring, EEComplexUnit* inAttacker, EEUnit* inTarget, GE3DPoint& ioLocation);
// EEIsSeenBy (ee_combat leaves.rs is_seen_by) over the world host.
bool EECombatIsSeenBy(EEUnit* inUnit, EEPlayer* inPlayer, bool inExplored);
// The damage of one hit (ee_combat damage.rs compute_damage; ee/EERules.h EEDamageForm, combat.yaml's constants):
// inUnit hits inTarget with inAttack, attack kind inAmmo (dbfamily column) and weapon inWeapon (dbweapontohit row);
// the elevation rule is inUnit's class's. inFriendlyFactor: splash on the own player's units. False when the hit does
// not get through (outAmount untouched), else outAmount. The trample's (EEPhysicsModel); ApplyHit and the splash are
// damage.rs's.
bool EECombatComputeDamage(int inForm, EEComplexUnit* inUnit, EEUnit* inTarget, float inAttack, long inAmmo, long inWeapon,
                           float inFriendlyFactor, long& outAmount);

// The nesting guard of a captured read (per thread and slot: 0 the reads, 1 the shots' ActionChanged, 2 their
// Update, 3 the heal goal's Work): mOuter when no other captured call of the slot is running
struct EECombatHostReadScope {
    explicit EECombatHostReadScope(int inSlot = 0);
    ~EECombatHostReadScope();
    int mSlot;
    bool mOuter;
};

#endif
