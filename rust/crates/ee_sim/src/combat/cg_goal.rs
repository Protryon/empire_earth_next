//! The combat goals' trigger tests and steps over their C++ statements (the units' lane, round 8; were
//! EECombatGoals.cpp `EECombatCondition`, `EECombatStep`, their hosts `EEConditionRead` and `EEStepOp`, the heal
//! goal's location fallback `EEHealLocationCondition` and the step's CBGS record): the records' fill (store.rs),
//! the lazy reads' choices (the goal's target, the kind's player and facing, the base condition by kind, the tile
//! floor), the impact unit kept for the targetable read and the action, the heal facing at yaw 0 (location.rs's
//! `face_yaw_with`). Each C++ statement is one `EE_UNITS_CG_*` case of `ee_units_cg_host` (EECombatGoals.cpp), asked
//! in the original's order.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_long;

use crate::combat::goals::*;
use ee_base::math::types::GeTransformation;

// ---- the statements (`unit` the goal's complex unit, `a` an object, `b`, `c` values or pointers, `f` floats out)
/// target `a`'s `mComplexUnit` (0 for none)
pub const EE_UNITS_CG_TARGET_COMPLEX: i32 = 1;
/// `unit->mPlayer != NULL && unit->mPlayer->IsHuman()`
pub const EE_UNITS_CG_HUMAN: i32 = 2;
/// attack goal `a`'s `GetAttackTileX()`
pub const EE_UNITS_CG_ATTACK_TILE_X: i32 = 3;
/// attack goal `a`'s `GetAttackTileY()`
pub const EE_UNITS_CG_ATTACK_TILE_Y: i32 = 4;
/// attack goal `a`'s `IsConvertAttack()`
pub const EE_UNITS_CG_CONVERT: i32 = 5;
/// `unit->SetGoal(EEUGRust::NewAttackUnit(unit, unit a, false, false), 0, 0)`
pub const EE_UNITS_CG_ATTACK_IMPACT: i32 = 6;
/// `f[0] = unit->GetLineOfSight()`
pub const EE_UNITS_CG_LINE_OF_SIGHT: i32 = 7;
/// heal goal `a`'s `GetHealTarget()`
pub const EE_UNITS_CG_HEAL_TARGET: i32 = 8;
/// attack goal `a`'s `GetAttackTarget()`
pub const EE_UNITS_CG_ATTACK_TARGET: i32 = 9;
/// `EECombatWorld(EE_COMBAT_WORLD_IN_RANGE_TO_HIT, unit, unit a) != 0`
pub const EE_UNITS_CG_IN_RANGE_TO_HIT: i32 = 10;
/// `unit->IsInReach(unit a)`
pub const EE_UNITS_CG_IN_REACH: i32 = 11;
/// `EECombatIsSeenBy(unit a, unit->mPlayer, b != 0)`
pub const EE_UNITS_CG_SEEN: i32 = 12;
/// `unit->mUnitClass->FacesTarget(unit, unit a, b != 0)`
pub const EE_UNITS_CG_FACES: i32 = 13;
/// `f[0] = unit->DistanceSquaredToUnit(unit a)`
pub const EE_UNITS_CG_DISTANCE_SQUARED: i32 = 14;
/// `unit->IsOnTileBorderingTarget(unit a)`
pub const EE_UNITS_CG_BORDERING: i32 = 15;
/// `EECombatIsGateClosed(unit, attack goal a)`
pub const EE_UNITS_CG_GATE_CLOSED: i32 = 16;
/// `EECombatIsStillReachable(unit, attack goal a)`
pub const EE_UNITS_CG_STILL_REACHABLE: i32 = 17;
/// `EEIsEnemyWithinRange(unit)`
pub const EE_UNITS_CG_ENEMY_IN_RANGE: i32 = 18;
/// class `a`'s `MissileEvaluateCondition(unit, b)`
pub const EE_UNITS_CG_MISSILE_CONDITION: i32 = 19;
/// the three floats at `b` = attack-ground goal `a`'s `GetGroundTarget()`
pub const EE_UNITS_CG_GROUND_TARGET: i32 = 20;
/// `unit->CanReachTile(b, c)`
pub const EE_UNITS_CG_CAN_REACH_TILE: i32 = 21;
/// `unit->GetUnitClass()->FacesPoint(unit, attack-ground goal a's GetGroundTarget(), true)`
pub const EE_UNITS_CG_FACES_POINT: i32 = 22;
/// `EECombatHealInRange(unit, unit a)`
pub const EE_UNITS_CG_HEAL_IN_RANGE: i32 = 23;
/// unit `a`'s `IsRepairable()`
pub const EE_UNITS_CG_REPAIRABLE: i32 = 24;
/// class `a`'s `FindImpactUnit(unit)`
pub const EE_UNITS_CG_FIND_IMPACT: i32 = 25;
/// unit `a`'s `GetUnitClass()`
pub const EE_UNITS_CG_CLASS: i32 = 26;
/// class `a`'s `CheckGroundImpact(unit)`
pub const EE_UNITS_CG_GROUND_IMPACT: i32 = 27;
/// class `a`'s `CheckWaterImpact(unit)`
pub const EE_UNITS_CG_WATER_IMPACT: i32 = 28;
/// the 16 floats at `b` = `unit->mPhysicalModel.GetLocation().mMatrix`
pub const EE_UNITS_CG_LOCATION: i32 = 29;
/// `unit->SetTransformation(the transformation at b, true)`
pub const EE_UNITS_CG_SET_TRANSFORMATION: i32 = 30;
/// `unit->SetCurrentPhysicsAction(b)`
pub const EE_UNITS_CG_PHYSICS_ACTION: i32 = 31;
/// `unit->mAnimation`
pub const EE_UNITS_CG_ANIMATION: i32 = 32;
/// `unit->SetAnimation(b, -1, 0)`
pub const EE_UNITS_CG_SET_ANIMATION: i32 = 33;
/// `unit->DropPath()`
pub const EE_UNITS_CG_DROP_PATH: i32 = 34;
/// attack goal `a`'s `RetargetGatePart(unit)`
pub const EE_UNITS_CG_RETARGET_GATE: i32 = 35;
/// `EEGetAttackFrameSet(unit)`
pub const EE_UNITS_CG_ATTACK_FRAME_SET: i32 = 36;
/// `unit->GetModel()->mSound1Frames[b]`
pub const EE_UNITS_CG_SOUND_FRAME: i32 = 37;
/// `unit->mPhysicalModel.GetFrame()`
pub const EE_UNITS_CG_FRAME: i32 = 38;
/// `unit->mUnitClass->FireShot(unit)`
pub const EE_UNITS_CG_FIRE_SHOT: i32 = 39;
/// `unit->SetRandomReloadTimer(b)`
pub const EE_UNITS_CG_RELOAD_TIMER: i32 = 40;
/// `unit->SetTargetSearchTimer(b)`
pub const EE_UNITS_CG_SEARCH_TIMER: i32 = 41;
/// goal `a`'s `SetPathAttempts(b)`
pub const EE_UNITS_CG_PATH_ATTEMPTS: i32 = 42;
/// `unit->mIsFiringHigh`
pub const EE_UNITS_CG_FIRING_HIGH: i32 = 43;

/// `kTriggerUnitFacingNotRequired`, `kTriggerCompletedUnitFacing` (ee/EEUnitAction.h)
const TRIGGER_FACING_NOT_REQUIRED: u32 = 9014;
const TRIGGER_COMPLETED_FACING: u32 = 9015;

/// `EE_COMBAT_TAG('C', 'B', 'G', 'S')`: the first letter in the low byte.
pub(crate) const TAG_CBGS: u32 = u32::from_le_bytes(*b"CBGS");

/// The goals' C++ statements and the Rust they call.
pub trait CgHost {
    /// one statement (`EE_UNITS_CG_*`)
    fn cg(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64;
    /// store.rs `fill_condition`
    fn fill_condition(&mut self, kind: i32, unit: u64, event: u32, target: u64, complex: u64) -> EeCombatConditionIn;
    /// store.rs `fill_step`
    fn fill_step(&mut self, kind: i32, unit: u64, target: u64) -> EeCombatStepIn;
    /// `ee_units_cls_base_condition(class, unit, event)` (EEUnitClass::BaseEvaluateCondition)
    fn base_condition(&mut self, class: u64, unit: u64, event: u32) -> i32;
    /// `ee_units_uu_targetable(unit, class, 0)`
    fn targetable(&mut self, unit: u64, class: u64) -> bool;
    /// `EEIsLineToUnitBlocked(unit, target, ...)` (uu_line.rs)
    fn line_blocked(&mut self, unit: u64, target: u64) -> bool;
    /// `EECombatCaptureCondition(in, out)` (capture_fold.rs)
    fn capture_condition(&mut self, i: &EeCombatConditionIn, o: &EeCombatConditionOut);
    /// the CBGS record of a step, when the capture is on
    fn capture_step(&mut self, i: &EeCombatStepIn);
}

fn op<H: CgHost + ?Sized>(h: &mut H, code: i32, unit: u64, a: u64, b: i64) -> i64 {
    h.cg(code, unit, a, b, 0, &mut [0.0; 4])
}

fn bits<H: CgHost + ?Sized>(h: &mut H, code: i32, unit: u64, a: u64) -> u32 {
    let mut f = [0.0; 4];
    h.cg(code, unit, a, 0, 0, &mut f);
    f[0].to_bits()
}

/// `FaceYaw` at a yaw over a transformation (location.rs's arithmetic): whether it faces it within a degree; if so
/// `set` gets the transformation turned to it exactly.
fn face_yaw_with(yaw: f32, t: GeTransformation, set: impl FnOnce(&GeTransformation)) -> bool {
    crate::units::location::face_yaw_with(yaw, t, set)
}

/// The heal goal's fallback, EEUGLocation::EvaluateCondition of the location goal it derived from (its yaw 0, its
/// facing flag false): facing done is FaceYaw at yaw 0; facing not required is true; any other event false
pub fn heal_location_condition<H: CgHost + ?Sized>(h: &mut H, unit: u64, event: u32) -> bool {
    if event == TRIGGER_FACING_NOT_REQUIRED {
        return true;
    }
    if event != TRIGGER_COMPLETED_FACING {
        return false;
    }
    let mut m = [0.0f32; 16];
    op(h, EE_UNITS_CG_LOCATION, unit, 0, m.as_mut_ptr() as i64);
    let t = crate::combat::geometry::matrix(&m);
    face_yaw_with(0.0, t, |turned| {
        op(h, EE_UNITS_CG_SET_TRANSFORMATION, unit, 0, turned as *const GeTransformation as i64);
    })
}

/// A trigger test's lazy reads (the C++'s `EEConditionContext` and `EEConditionRead`).
struct Reads<'h, H: CgHost + ?Sized> {
    h: &'h mut H,
    kind: i32,
    event: u32,
    unit: u64,
    goal: u64,
    class: u64,
    impact: u64,
}

impl<H: CgHost + ?Sized> Reads<'_, H> {
    fn op(&mut self, code: i32, a: u64, b: i64) -> i64 {
        op(self.h, code, self.unit, a, b)
    }
    /// the attack goal's target (attack unit), the heal goal's (heal)
    fn target(&mut self) -> u64 {
        let code = if self.kind == EE_COMBAT_GOAL_HEAL { EE_UNITS_CG_HEAL_TARGET } else { EE_UNITS_CG_ATTACK_TARGET };
        self.op(code, self.goal, 0) as u64
    }
    fn read_code(&mut self, code: i32) -> u32 {
        let b = |v: i64| u32::from(v != 0);
        match code {
            EE_COMBAT_READ_LINE_OF_SIGHT => bits(self.h, EE_UNITS_CG_LINE_OF_SIGHT, self.unit, 0),
            EE_COMBAT_READ_IN_RANGE_TO_HIT => {
                let t = self.target();
                b(self.op(EE_UNITS_CG_IN_RANGE_TO_HIT, t, 0))
            }
            EE_COMBAT_READ_IN_REACH => {
                let t = self.target();
                b(self.op(EE_UNITS_CG_IN_REACH, t, 0))
            }
            EE_COMBAT_READ_SEEN => {
                let t = self.target();
                let explored = self.kind == EE_COMBAT_GOAL_HEAL;
                b(self.op(EE_UNITS_CG_SEEN, t, i64::from(explored)))
            }
            EE_COMBAT_READ_FACES_ARCH => {
                let t = self.target();
                b(self.op(EE_UNITS_CG_FACES, t, 1))
            }
            EE_COMBAT_READ_FACES => {
                let t = self.target();
                b(self.op(EE_UNITS_CG_FACES, t, 0))
            }
            EE_COMBAT_READ_FACES_MOVING => {
                let t = self.target();
                let complex = self.op(EE_UNITS_CG_TARGET_COMPLEX, t, 0) as u64;
                b(self.op(EE_UNITS_CG_FACES, complex, 1))
            }
            EE_COMBAT_READ_DISTANCE_SQUARED => {
                let t = self.target();
                bits(self.h, EE_UNITS_CG_DISTANCE_SQUARED, self.unit, t)
            }
            EE_COMBAT_READ_BORDERING => {
                let t = self.target();
                b(self.op(EE_UNITS_CG_BORDERING, t, 0))
            }
            EE_COMBAT_READ_LINE_BLOCKED => {
                let t = self.target();
                u32::from(self.h.line_blocked(self.unit, t))
            }
            EE_COMBAT_READ_GATE_CLOSED => b(self.op(EE_UNITS_CG_GATE_CLOSED, self.goal, 0)),
            EE_COMBAT_READ_STILL_REACHABLE => b(self.op(EE_UNITS_CG_STILL_REACHABLE, self.goal, 0)),
            EE_COMBAT_READ_ENEMY_IN_RANGE => b(self.op(EE_UNITS_CG_ENEMY_IN_RANGE, 0, 0)),
            EE_COMBAT_READ_BASE => match self.kind {
                EE_COMBAT_GOAL_HEAL => u32::from(heal_location_condition(self.h, self.unit, self.event)),
                EE_COMBAT_GOAL_TORPEDO => {
                    self.op(EE_UNITS_CG_MISSILE_CONDITION, self.class, i64::from(self.event)) as u32
                }
                _ => self.h.base_condition(self.class, self.unit, self.event) as u32,
            },
            EE_COMBAT_READ_CAN_REACH_TILE => {
                let mut p = [0.0f32; 3];
                self.op(EE_UNITS_CG_GROUND_TARGET, self.goal, p.as_mut_ptr() as i64);
                let (x, y) = (ee_base::math::um::floor(p[0]), ee_base::math::um::floor(p[1]));
                b(self.h.cg(
                    EE_UNITS_CG_CAN_REACH_TILE,
                    self.unit,
                    0,
                    x as c_long as i64,
                    y as c_long as i64,
                    &mut [0.0; 4],
                ))
            }
            EE_COMBAT_READ_FACES_POINT => b(self.op(EE_UNITS_CG_FACES_POINT, self.goal, 0)),
            EE_COMBAT_READ_HEAL_IN_RANGE => {
                let t = self.op(EE_UNITS_CG_HEAL_TARGET, self.goal, 0) as u64;
                b(self.op(EE_UNITS_CG_HEAL_IN_RANGE, t, 0))
            }
            EE_COMBAT_READ_REPAIRABLE => {
                let t = self.op(EE_UNITS_CG_HEAL_TARGET, self.goal, 0) as u64;
                b(self.op(EE_UNITS_CG_REPAIRABLE, t, 0))
            }
            EE_COMBAT_READ_IMPACT_UNIT => {
                self.impact = self.op(EE_UNITS_CG_FIND_IMPACT, self.class, 0) as u64;
                u32::from(self.impact != 0)
            }
            EE_COMBAT_READ_TARGETABLE => {
                let class = self.op(EE_UNITS_CG_CLASS, self.impact, 0) as u64;
                u32::from(self.h.targetable(self.unit, class))
            }
            EE_COMBAT_READ_GROUND_IMPACT => b(self.op(EE_UNITS_CG_GROUND_IMPACT, self.class, 0)),
            EE_COMBAT_READ_WATER_IMPACT => b(self.op(EE_UNITS_CG_WATER_IMPACT, self.class, 0)),
            _ => 0,
        }
    }
}

impl<H: CgHost + ?Sized> ConditionHost for &mut Reads<'_, H> {
    fn read(&mut self, code: i32) -> u32 {
        self.read_code(code)
    }
}

/// `EECombatCondition(kind, unit, event, target, goal, class)`: the trigger test (goals.rs `condition`) over the
/// record and the lazy reads; the impact unit's attack goal set before the record; CBTC.. folded (capture_fold.rs).
pub fn condition<H: CgHost + ?Sized>(
    h: &mut H,
    kind: i32,
    unit: u64,
    event: u32,
    target: u64,
    goal: u64,
    class: u64,
) -> i32 {
    // the record: the store's part (store.rs); here the unit's player's IsHuman (a virtual) and the attack goal's
    // own fields
    let complex = if target != 0 { op(h, EE_UNITS_CG_TARGET_COMPLEX, unit, target, 0) as u64 } else { 0 };
    let mut i = h.fill_condition(kind, unit, event, target, complex);
    i.human = i32::from(op(h, EE_UNITS_CG_HUMAN, unit, 0, 0) != 0);
    if kind == EE_COMBAT_GOAL_ATTACK_UNIT {
        i.goal_tile[0] = op(h, EE_UNITS_CG_ATTACK_TILE_X, unit, goal, 0) as c_long as i32;
        i.goal_tile[1] = op(h, EE_UNITS_CG_ATTACK_TILE_Y, unit, goal, 0) as c_long as i32;
        i.convert = i32::from(op(h, EE_UNITS_CG_CONVERT, unit, goal, 0) != 0);
    }
    let mut reads = Reads { kind: i.kind, event: i.event, unit, goal, class, impact: 0, h };
    let out = crate::combat::goals::condition(&mut i, &mut reads);
    let impact = reads.impact;
    // the action before the record, as the original's body set the goal before its end
    if out.action == EE_COMBAT_CONDITION_ATTACK_IMPACT_UNIT {
        op(h, EE_UNITS_CG_ATTACK_IMPACT, unit, impact, 0);
    }
    h.capture_condition(&i, &out);
    out.result
}

/// A goal step's operations (the C++'s `EEStepOp`).
struct Steps<'h, H: CgHost + ?Sized> {
    h: &'h mut H,
    kind: i32,
    unit: u64,
    goal: u64,
}

impl<H: CgHost + ?Sized> StepHost for &mut Steps<'_, H> {
    fn op(&mut self, code: i32, arg: i32) -> i32 {
        let (unit, goal, arg) = (self.unit, self.goal, i64::from(arg));
        let h = &mut *self.h;
        match code {
            EE_COMBAT_STEP_PHYSICS_ACTION => {
                op(h, EE_UNITS_CG_PHYSICS_ACTION, unit, 0, arg);
                0
            }
            EE_COMBAT_STEP_FACES => {
                let target = if self.kind == EE_COMBAT_GOAL_HEAL {
                    op(h, EE_UNITS_CG_HEAL_TARGET, unit, goal, 0)
                } else {
                    op(h, EE_UNITS_CG_ATTACK_TARGET, unit, goal, 0)
                } as u64;
                i32::from(op(h, EE_UNITS_CG_FACES, unit, target, 0) != 0)
            }
            EE_COMBAT_STEP_ANIMATION => op(h, EE_UNITS_CG_ANIMATION, unit, 0, 0) as i32,
            EE_COMBAT_STEP_SET_ANIMATION => {
                op(h, EE_UNITS_CG_SET_ANIMATION, unit, 0, arg);
                0
            }
            EE_COMBAT_STEP_DROP_PATH => {
                op(h, EE_UNITS_CG_DROP_PATH, unit, 0, 0);
                0
            }
            EE_COMBAT_STEP_RETARGET_GATE => {
                op(h, EE_UNITS_CG_RETARGET_GATE, unit, goal, 0);
                0
            }
            EE_COMBAT_STEP_ATTACK_FRAME_SET => op(h, EE_UNITS_CG_ATTACK_FRAME_SET, unit, 0, 0) as i32,
            EE_COMBAT_STEP_SOUND_FRAME => op(h, EE_UNITS_CG_SOUND_FRAME, unit, 0, arg) as i32,
            EE_COMBAT_STEP_FRAME => op(h, EE_UNITS_CG_FRAME, unit, 0, 0) as i32,
            EE_COMBAT_STEP_FIRE_SHOT => {
                op(h, EE_UNITS_CG_FIRE_SHOT, unit, 0, 0);
                0
            }
            EE_COMBAT_STEP_RELOAD_TIMER => {
                op(h, EE_UNITS_CG_RELOAD_TIMER, unit, 0, arg);
                0
            }
            EE_COMBAT_STEP_SEARCH_TIMER => {
                op(h, EE_UNITS_CG_SEARCH_TIMER, unit, 0, arg);
                0
            }
            EE_COMBAT_STEP_PATH_ATTEMPTS => {
                op(h, EE_UNITS_CG_PATH_ATTEMPTS, unit, goal, arg);
                0
            }
            EE_COMBAT_STEP_FIRING_HIGH => i32::from(op(h, EE_UNITS_CG_FIRING_HIGH, unit, 0, 0) != 0),
            _ => 0,
        }
    }
}

/// `EECombatStep(kind, unit, goal, target)`: the goal's step (goals.rs `step`) over the record and the operations;
/// its CBGS record.
pub fn step<H: CgHost + ?Sized>(h: &mut H, kind: i32, unit: u64, goal: u64, target: u64) {
    let mut i = h.fill_step(kind, unit, target);
    crate::combat::goals::step(&mut i, &mut Steps { h, kind, unit, goal });
    h.capture_step(&i);
}

#[cfg(not(test))]
pub use game::ee_units_cg_condition;

#[cfg(not(test))]
mod game {
    use super::*;

    extern "C" {
        fn ee_units_cg_host(op: i32, unit: u64, a: u64, b: i64, c: i64, f: *mut [f32; 4]) -> i64;
    }

    struct Game;

    impl CgHost for Game {
        fn cg(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            // SAFETY: inside the C++ call that handed over its live objects
            unsafe { ee_units_cg_host(op, unit, a, b, c, f) }
        }
        fn fill_condition(
            &mut self,
            kind: i32,
            unit: u64,
            event: u32,
            target: u64,
            complex: u64,
        ) -> EeCombatConditionIn {
            let mut i = EeCombatConditionIn::default();
            crate::combat::store::fill_condition(&mut i, kind, unit, event, target, complex);
            i
        }
        fn fill_step(&mut self, kind: i32, unit: u64, target: u64) -> EeCombatStepIn {
            let mut i = EeCombatStepIn::default();
            crate::combat::store::fill_step(&mut i, kind, unit, target);
            i
        }
        fn base_condition(&mut self, class: u64, unit: u64, event: u32) -> i32 {
            crate::units::cls_host::ee_units_cls_base_condition(class, unit, event)
        }
        fn targetable(&mut self, unit: u64, class: u64) -> bool {
            crate::units::uu_ffi::ee_units_uu_targetable(unit, class, 0) != 0
        }
        fn line_blocked(&mut self, unit: u64, target: u64) -> bool {
            crate::units::uu_line::line_blocked_in_game(unit, target)
        }
        fn capture_condition(&mut self, i: &EeCombatConditionIn, o: &EeCombatConditionOut) {
            crate::combat::capture_fold::ee_combat_capture_condition(i, o);
        }
        fn capture_step(&mut self, i: &EeCombatStepIn) {
            if !crate::app::capture_record::active() {
                return;
            }
            // SAFETY: a repr(C) record of 4-byte fields (no padding) read as its bytes, as the C++ wrote it
            let bytes = unsafe {
                std::slice::from_raw_parts(
                    (i as *const EeCombatStepIn).cast::<u8>(),
                    std::mem::size_of::<EeCombatStepIn>(),
                )
            };
            crate::app::capture_record::commit(TAG_CBGS, bytes);
        }
    }

    /// `EECombatCondition`
    #[no_mangle]
    pub extern "C" fn ee_units_cg_condition(
        kind: i32,
        unit: u64,
        event: u32,
        target: u64,
        goal: u64,
        class: u64,
    ) -> i32 {
        condition(&mut Game, kind, unit, event, target, goal, class)
    }

    /// `EECombatStep`
    #[no_mangle]
    pub extern "C" fn ee_units_cg_step(kind: i32, unit: u64, goal: u64, target: u64) {
        step(&mut Game, kind, unit, goal, target)
    }
}

#[cfg(test)]
#[path = "cg_goal_tests.rs"]
mod tests;
