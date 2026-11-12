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

#include "rust/ee_rust.h"
#include "ee/EESimCapture.h"

class EEComplexUnit;
class EEUnit;

// The plain reads of a test of inUnit on inEvent with the target inTarget (NULL: none; the record's target fields
// stay 0). The attack goal's own fields (goal_tile, convert) are the caller's. Defined in EECombatGoals.cpp.
void EECombatFillCondition(EeCombatConditionIn& in, int32_t inKind, EEComplexUnit* inUnit, unsigned long inEvent,
                           EEUnit* inTarget);
// The plain reads of a step of inUnit (the target only gives its id)
void EECombatFillStep(EeCombatStepIn& in, int32_t inKind, EEComplexUnit* inUnit, EEUnit* inTarget);
// CBTC, CBGS (no record while the capture is off)
void EECombatCaptureCondition(const EeCombatConditionIn& in, const EeCombatConditionOut& inOut);
void EECombatCaptureStep(const EeCombatStepIn& in);
// writes the tests' last CBTC batch (EE-SIM at the game's end, before the capture closes)
void EECombatCaptureFlush();

// The test of in (filled by EECombatFillCondition) through ee_combat_condition over the C++ host (inGoal for the
// goals, inClass for the shots); the action applied (a shot's attack goal on the unit it hit), then the CBTC record.
// Returns the test's value. Defined in EECombatGoals.cpp.
class EEUnitGoal;
class EEUnitClass;
long EECombatCondition(EeCombatConditionIn& in, EEComplexUnit* inUnit, EEUnitGoal* inGoal, EEUnitClass* inClass);
// The step of inGoal (inKind) through ee_combat_goal_step: the plan's operations executed on inUnit and inGoal as
// they are issued, then the CBGS record
void EECombatStep(int32_t inKind, EEComplexUnit* inUnit, EEUnitGoal* inGoal, EEUnit* inTarget);
// A shot's target: its attack-unit goal's (NULL under another goal)
EEUnit* EECombatShotTarget(EEComplexUnit* inUnit);

#endif
