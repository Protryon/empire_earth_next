//! The heal goal whole in Rust (R5b.2, the fourteenth round; was `EEUGHealUnit` 0x22528d..0x22585a, derived from
//! `EEUGLocation`): an `EEUGRust` of kind `kGoalHealUnit`, its state a record by goal object (goal_objects.rs's table),
//! its bodies over the goals' host (goal_objects.rs's `GoalHost`).
//!
//! The original derived from the location goal: the parts of `EEUGLocation` it used are the record's `location` (the
//! healee's position when the goal was made; 0, 0, 0 for the load's empty goal, whose `Load` reads no location) and
//! constants: its yaw 0, its forced-move, facing and auto-waypoint flags false (no caller set them on a heal goal; the
//! group path state and group movement flag it could be given are read only for `kGoalLocation`). Its inherited
//! bodies are restated here: the debug line (`EEUGLocation::GetDebugLine`), the destination (`EEUnitGoal`'s: the goal
//! location told to the unit's group), the trigger test's fallback (`EEUGLocation::EvaluateCondition`: the C++ host's
//! base read, EECombatGoals.cpp). Its `Save` and `Load` write and read no base goal data (EEUGRust.cpp skips the base
//! goal's for this kind), its `Start` does nothing, its `DumpOutOfSynch` and checksum add the base goal's only.
//!
//! Units are handles (the `EEUnit*`'s address; the healee an `EEComplexUnit*`, the same address). Between `Load` and
//! `RepairPointers` the target holds the saved id. Determinism: plain copies; the work's arithmetic is shots.rs's
//! `heal_work` (the original's `Work` already called it).

use crate::goal_objects::{with_goals, EeCombatGoalArgs, GoalHost, EE_COMBAT_G_TELL_GROUP, EE_COMBAT_G_UNIT_LOCATION};

// ---- the host's operations (goal_dispatch.rs's numbering goes on)

/// `gWorld.mInCinematic` into `args[0]`, `gPhysics->mTickTimeMs` into `args[1]`.
pub const EE_COMBAT_G_HEAL_ENV: i32 = 1060;
/// the healee's `SetHitPoints(args[0])` (`EEComplexUnit`'s).
pub const EE_COMBAT_G_SET_HIT_POINTS: i32 = 1061;
/// the healee's `ReleaseHealEffect()`.
pub const EE_COMBAT_G_RELEASE_HEAL_EFFECT: i32 = 1062;
/// `EECanHeal(unit, args[0])`.
pub const EE_COMBAT_G_CAN_HEAL: i32 = 1063;
/// TEST ONLY: `EECombatCaptureHealWork(unit, args[0], args[1])` (CBSO).
pub const EE_COMBAT_G_CAPTURE_HEAL_WORK: i32 = 1064;

/// `kGoalHealUnit`.
pub const GOAL_HEAL_UNIT: u8 = 15;
/// `kActionTurnToFaceHealee`: the heal goal's first action.
pub const ACTION_TURN_TO_FACE_HEALEE: i64 = 16002;

const NO_ID: u32 = 0xffff_ffff;

/// A heal goal's own state (the original's members and the location goal's location).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCombatHealGoal {
    /// `EEUGLocation::mLocation` (float bits): where the healee was when the goal was made
    pub location: [u32; 3],
    /// `mTarget`, the healee (a handle, referenced; the saved id between Load and RepairPointers)
    pub target: u64,
    /// `mElapsedTime` (milliseconds since the last heal)
    pub elapsed: u32,
}

pub fn heal_of(goal: u64) -> EeCombatHealGoal {
    with_goals(|g| g.heal.get(&goal).copied()).unwrap_or_default()
}

fn put(goal: u64, x: EeCombatHealGoal) {
    with_goals(|g| g.heal.insert(goal, x));
}

fn op<H: GoalHost>(h: &mut H, code: i32, unit: u64, a0: i64, a1: i64) -> i64 {
    let mut args: EeCombatGoalArgs = [0; 8];
    args[0] = a0;
    args[1] = a1;
    h.op(code, unit, &mut args)
}

fn unit_location<H: GoalHost>(h: &mut H, unit: u64) -> [u32; 3] {
    let mut args: EeCombatGoalArgs = [0; 8];
    h.op(EE_COMBAT_G_UNIT_LOCATION, unit, &mut args);
    [args[0] as u32, args[1] as u32, args[2] as u32]
}

/// `EEUGHealUnit(inTarget, inArg)`'s body after the base goal: the location goal at the target's position, the target
/// referenced, no time elapsed. A NULL target (the original faulted) leaves the location 0, 0, 0.
pub fn create<H: GoalHost>(h: &mut H, goal: u64, target: u64) {
    let mut x = EeCombatHealGoal { target, ..Default::default() };
    if target != 0 {
        x.location = unit_location(h, target);
        op(h, crate::goal_attack::EE_COMBAT_G_REFERENCE, target, 0, 0);
    }
    put(goal, x);
}

/// `EEUGHealUnit(inArg)`: the load's empty goal (no target, the location 0, 0, 0).
pub fn create_empty(goal: u64) {
    put(goal, EeCombatHealGoal::default());
}

/// `~EEUGHealUnit`: the target's heal effect ended and its reference released.
pub fn destroy<H: GoalHost>(h: &mut H, goal: u64) {
    let x = heal_of(goal);
    if x.target != 0 {
        op(h, EE_COMBAT_G_RELEASE_HEAL_EFFECT, x.target, 0, 0);
        op(h, crate::goal_attack::EE_COMBAT_G_DEREFERENCE, x.target, 0, 0);
        put(goal, EeCombatHealGoal { target: 0, elapsed: 0, ..x });
    }
}

/// `SetTarget(inTarget)`: the reference moved, the heal timer restarted.
pub fn set_target<H: GoalHost>(h: &mut H, goal: u64, target: u64) {
    let x = heal_of(goal);
    if x.target != 0 {
        op(h, crate::goal_attack::EE_COMBAT_G_DEREFERENCE, x.target, 0, 0);
    }
    put(goal, EeCombatHealGoal { target, elapsed: 0, ..heal_of(goal) });
    if target != 0 {
        op(h, crate::goal_attack::EE_COMBAT_G_REFERENCE, target, 0, 0);
    }
}

/// `ReplaceUnit(inOld, inNew, ...)`: the goal follows its target's replacement.
pub fn replace_unit<H: GoalHost>(h: &mut H, goal: u64, old: u64, new: u64) {
    if old == heal_of(goal).target {
        set_target(h, goal, new);
    }
}

/// `GetGoalLocation`: the target's position (`GetPosition`); a goal without a target leaves the point as it was (the
/// original faulted).
pub fn location<H: GoalHost>(h: &mut H, goal: u64, inout: [u32; 3]) -> [u32; 3] {
    let target = heal_of(goal).target;
    if target == 0 {
        return inout;
    }
    unit_location(h, target)
}

/// `EEUnitGoal::GetDestination` (the heal goal kept the base goal's): the goal location, told to the unit's group.
pub fn destination<H: GoalHost>(h: &mut H, goal: u64, unit: u64, inout: [u32; 3]) -> [u32; 3] {
    let out = location(h, goal, inout);
    let mut args: EeCombatGoalArgs = [0; 8];
    for (slot, b) in args.iter_mut().zip(out) {
        *slot = i64::from(b);
    }
    h.op(EE_COMBAT_G_TELL_GROUP, unit, &mut args);
    out
}

/// `GetOutOfSynchChecksum`'s own part: the target's id (0 without one) plus the elapsed time (`long`s).
pub fn checksum<H: GoalHost>(h: &mut H, goal: u64) -> i32 {
    let x = heal_of(goal);
    let id = if x.target != 0 { op(h, crate::goal_attack::EE_COMBAT_G_ID, x.target, 0, 0) as i32 } else { 0 };
    id.wrapping_add(x.elapsed as i32)
}

/// `Suits(inUnit)`: whether the unit can heal the target (`EECanHeal`); CBGO records it.
pub fn suits<H: GoalHost>(h: &mut H, goal: u64, unit: u64) -> bool {
    let target = heal_of(goal).target;
    op(h, EE_COMBAT_G_CAN_HEAL, unit, target as i64, 0) != 0
}

/// `Work(inUnit)`: while healing a living target that is not an enemy (not in cinematics), the healer's healing
/// points each full second, up to the target's maximum (shots.rs's `heal_work` over the store's reads); CBSO records
/// the outcome.
pub fn work<H: GoalHost>(h: &mut H, goal: u64, unit: u64) {
    let target = heal_of(goal).target;
    let mut input = crate::shots::EeCombatHealWorkIn::default();
    crate::store::fill_heal_work(&mut input, unit, target);
    let mut env: EeCombatGoalArgs = [0; 8];
    h.op(EE_COMBAT_G_HEAL_ENV, 0, &mut env);
    input.cinematic = env[0] as i32;
    input.elapsed = heal_of(goal).elapsed;
    input.tick_ms = env[1] as i32;
    let out = crate::shots::heal_work(&input);
    if out.work != 0 {
        put(goal, EeCombatHealGoal { elapsed: out.elapsed, ..heal_of(goal) });
        if out.heal != 0 {
            op(h, EE_COMBAT_G_SET_HIT_POINTS, target, i64::from(out.hit_points), 0);
        }
    }
    let x = heal_of(goal);
    op(h, EE_COMBAT_G_CAPTURE_HEAL_WORK, unit, x.target as i64, i64::from(x.elapsed));
}

fn write<H: GoalHost>(h: &mut H, code: i32, value: i64) -> i32 {
    op(h, code, 0, value, 0) as i32
}

fn read<H: GoalHost>(h: &mut H, code: i32, value: &mut i64) -> i32 {
    let mut args: EeCombatGoalArgs = [0; 8];
    args[0] = *value;
    let result = h.op(code, 0, &mut args) as i32;
    *value = args[0];
    result
}

/// `Save` (no base goal data): a has-target flag, then the target's id and the elapsed time; the first error.
pub fn save<H: GoalHost>(h: &mut H, goal: u64) -> i32 {
    use crate::goal_attack::{EE_COMBAT_G_ID, EE_COMBAT_G_WRITE_BOOL, EE_COMBAT_G_WRITE_U32};
    let x = heal_of(goal);
    let has = x.target != 0;
    let r = write(h, EE_COMBAT_G_WRITE_BOOL, i64::from(has));
    if r != 0 || !has {
        return r;
    }
    let id = op(h, EE_COMBAT_G_ID, x.target, 0, 0);
    let r = write(h, EE_COMBAT_G_WRITE_U32, id);
    if r != 0 {
        return r;
    }
    write(h, EE_COMBAT_G_WRITE_U32, i64::from(x.elapsed))
}

/// `Load` (no base goal data): the has-target flag, then the target's saved id and the elapsed time; a failed read
/// keeps what was read before it; the first error.
pub fn load<H: GoalHost>(h: &mut H, goal: u64) -> i32 {
    use crate::goal_attack::{EE_COMBAT_G_READ_BOOL, EE_COMBAT_G_READ_U32};
    let mut x = heal_of(goal);
    let mut has = 0i64;
    let r = read(h, EE_COMBAT_G_READ_BOOL, &mut has);
    if r != 0 || has == 0 {
        return r;
    }
    let mut id = i64::from(NO_ID);
    let mut r = read(h, EE_COMBAT_G_READ_U32, &mut id);
    if r == 0 {
        x.target = id as u32 as u64;
        let mut v = i64::from(x.elapsed);
        r = read(h, EE_COMBAT_G_READ_U32, &mut v);
        x.elapsed = v as u32;
    }
    put(goal, x);
    r
}

/// `RepairPointers`: the saved id back to the unit's complex unit, referenced; 3 and no target when it is gone (a
/// unit without a complex unit leaves 3 and no target too).
pub fn repair<H: GoalHost>(h: &mut H, goal: u64) -> i32 {
    use crate::goal_attack::{EE_COMBAT_G_COMPLEX, EE_COMBAT_G_FIND_UNIT, EE_COMBAT_G_REFERENCE};
    let mut x = heal_of(goal);
    if x.target == 0 {
        return 0;
    }
    let mut result = 3;
    let unit = op(h, EE_COMBAT_G_FIND_UNIT, 0, i64::from(x.target as u32), 0) as u64;
    if unit != 0 {
        x.target = op(h, EE_COMBAT_G_COMPLEX, unit, 0, 0) as u64;
        if x.target != 0 {
            op(h, EE_COMBAT_G_REFERENCE, x.target, 0, 0);
            result = 0;
        }
    } else {
        x.target = 0;
    }
    put(goal, x);
    result
}

// ---- the C ABI (the accessors' reads and writes; the bodies are goal_dispatch.rs's)

/// The record of the goal into `out` (zeroes for none).
///
/// # Safety
/// `out` must point to a writable record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_heal_get(goal: u64, out: *mut EeCombatHealGoal) {
    // SAFETY: the caller's contract
    unsafe { *out = heal_of(goal) };
}

/// The heal goal's location replaced by `location` (float bits; the location goal's `SetLocation`, which the carrier
/// landing applies to any goal of the location class).
#[no_mangle]
pub extern "C" fn ee_combat_heal_set_location(goal: u64, x: u32, y: u32, z: u32) {
    put(goal, EeCombatHealGoal { location: [x, y, z], ..heal_of(goal) });
}

#[cfg(test)]
#[path = "goal_heal_tests.rs"]
mod tests;
