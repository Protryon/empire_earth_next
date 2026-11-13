//! The attack-ground, attack-unit and heal goals whole in Rust (R5b.2, the twelfth round): each is an `EEUGRust` (the
//! core lane's goal class whose state and bodies are Rust's, ee/EEUGRust.h), its own state a record here keyed by the
//! goal object's address, its bodies over a host of the C++ objects' operations (`EeCombatGoalHost`: the unit's
//! reach, location and group, the actions, the typed file calls). The trigger tests and steps were Rust already
//! (goals.rs); the C++ keeps the object (the base goal's fields and vtable) and the text formatting of the debug and
//! out-of-synch dumps.
//!
//! Determinism: the records are read and written by key only (a `BTreeMap`, never iterated in the game); the floats
//! are kept as their bits and copied, never computed, except `GetDestination`'s floors (`UMFloor`, ee_ge's).

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::sync::Mutex;

// ---- the host's operations (EeCombatGoalHost::op): `unit` the call's unit, `args` the arguments and answers

/// `inUnit->CanReachTile(args[0], args[1])`.
pub const EE_COMBAT_G_CAN_REACH_TILE: i32 = 1;
/// the unit's location's x, y, z as float bits into `args[0..3]`.
pub const EE_COMBAT_G_UNIT_LOCATION: i32 = 2;
/// `if (inUnit->mActiveGroup != NULL) inUnit->mActiveGroup->TellGroup(inUnit, point)`, the point's bits in
/// `args[0..3]`; answers whether the unit had a group.
pub const EE_COMBAT_G_TELL_GROUP: i32 = 3;
/// `inUnit->StartAction(args[0]); inUnit->mUnitClass->ActionChanged(inUnit)`.
pub const EE_COMBAT_G_START_ACTION: i32 = 4;
/// the file's `Write(float)` of the bits in `args[0]`; answers the result.
pub const EE_COMBAT_G_WRITE_F32: i32 = 10;
/// the file's `Read(float)` into `args[0]` (bits; its value before the read in `args[0]` too); answers the result.
pub const EE_COMBAT_G_READ_F32: i32 = 11;

/// `kActionInitialAttackState` (ee/EEUnitAction.h).
pub const ACTION_INITIAL_ATTACK_STATE: i64 = 2016;
/// `kActionCheckRange`: the attack goals' first action.
pub const ACTION_CHECK_RANGE: i32 = 2013;

/// The arguments and answers of one host operation.
pub type EeCombatGoalArgs = [i64; 8];

/// The C++ objects' operations behind the goals' bodies (a mock in the tests).
pub trait GoalHost {
    fn op(&mut self, code: i32, unit: u64, args: &mut EeCombatGoalArgs) -> i64;
}

/// The C++ host: `op` over `ctx` (the call's file for the save and load).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeCombatGoalHost {
    pub ctx: *mut c_void,
    pub op: unsafe extern "C" fn(ctx: *mut c_void, code: i32, unit: u64, args: *mut EeCombatGoalArgs) -> i64,
}

impl GoalHost for EeCombatGoalHost {
    fn op(&mut self, code: i32, unit: u64, args: &mut EeCombatGoalArgs) -> i64 {
        // SAFETY: the C++ host answers over its own context for the duration of the call
        unsafe { (self.op)(self.ctx, code, unit, args) }
    }
}

// ---- the records

/// An attack-ground goal: `mTarget`, the point attacked (float bits).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GroundGoal {
    pub target: [u32; 3],
}

/// The goals' records by goal object.
#[derive(Default)]
pub struct CombatGoals {
    pub ground: BTreeMap<u64, GroundGoal>,
}

impl CombatGoals {
    pub const fn new() -> Self {
        CombatGoals { ground: BTreeMap::new() }
    }
}

static GOALS: Mutex<CombatGoals> = Mutex::new(CombatGoals::new());

/// Runs `f` over the process's records (a poisoned lock is taken over: plain values only). Never held across a host
/// call.
pub fn with_goals<R>(f: impl FnOnce(&mut CombatGoals) -> R) -> R {
    let mut guard = GOALS.lock().unwrap_or_else(|e| e.into_inner());
    f(&mut guard)
}

fn bits(p: [f32; 3]) -> [u32; 3] {
    [p[0].to_bits(), p[1].to_bits(), p[2].to_bits()]
}

fn ground_of(goal: u64) -> GroundGoal {
    with_goals(|g| g.ground.get(&goal).copied()).unwrap_or_default()
}

// ---- the attack-ground goal's bodies

/// `EEUGAttackGround::GetDestination(inUnit, outLocation)`: where the unit stands when it can reach the target's tile,
/// else the target; the unit's group told. Returns the location's bits.
pub fn ground_destination<H: GoalHost>(h: &mut H, goal: u64, unit: u64) -> [u32; 3] {
    let target = ground_of(goal).target;
    let y = ee_ge::um::floor(f32::from_bits(target[1]));
    let x = ee_ge::um::floor(f32::from_bits(target[0]));
    let mut args = [0i64; 8];
    args[0] = i64::from(x);
    args[1] = i64::from(y);
    let out = if h.op(EE_COMBAT_G_CAN_REACH_TILE, unit, &mut args) != 0 {
        let mut a = [0i64; 8];
        h.op(EE_COMBAT_G_UNIT_LOCATION, unit, &mut a);
        [a[0] as u32, a[1] as u32, a[2] as u32]
    } else {
        target
    };
    let mut a = [0i64; 8];
    for (slot, b) in a.iter_mut().zip(out) {
        *slot = i64::from(b);
    }
    h.op(EE_COMBAT_G_TELL_GROUP, unit, &mut a);
    out
}

/// `EEUGAttackGround::GetOutOfSynchChecksum`'s own part: the target's coordinates' raw bits summed (`long`s).
pub fn ground_checksum(goal: u64) -> i32 {
    let t = ground_of(goal).target;
    (t[2] as i32).wrapping_add((t[0] as i32).wrapping_add(t[1] as i32))
}

/// `Save`'s own part after the base goal's: the target point (`GE3DPoint::Save`: x, y, z as floats, the first error).
pub fn ground_save<H: GoalHost>(h: &mut H, goal: u64) -> i32 {
    let t = ground_of(goal).target;
    let mut result = 0;
    for b in t {
        let mut args = [0i64; 8];
        args[0] = i64::from(b);
        result = h.op(EE_COMBAT_G_WRITE_F32, 0, &mut args) as i32;
        if result != 0 {
            break;
        }
    }
    result
}

/// `Load`'s own part after the base goal's: the target point (`GE3DPoint::Load`: each coordinate read in place, the
/// first error stops; the ones read are kept).
pub fn ground_load<H: GoalHost>(h: &mut H, goal: u64) -> i32 {
    let mut t = ground_of(goal).target;
    let mut result = 0;
    for slot in t.iter_mut() {
        let mut args = [0i64; 8];
        args[0] = i64::from(*slot);
        result = h.op(EE_COMBAT_G_READ_F32, 0, &mut args) as i32;
        *slot = args[0] as u32;
        if result != 0 {
            break;
        }
    }
    with_goals(|g| g.ground.insert(goal, GroundGoal { target: t }));
    result
}

// ---- the C ABI

/// A new attack-ground goal's record (its target point).
#[no_mangle]
pub extern "C" fn ee_combat_ground_create(goal: u64, x: f32, y: f32, z: f32) {
    with_goals(|g| g.ground.insert(goal, GroundGoal { target: bits([x, y, z]) }));
}

/// The goal object goes: its record with it (any kind).
#[no_mangle]
pub extern "C" fn ee_combat_goal_drop(goal: u64) {
    with_goals(|g| {
        g.ground.remove(&goal);
    });
}

/// The attack-ground goal's target into `out` (0, 0, 0 for a goal without a record).
///
/// # Safety
/// `out` must point to three writable floats.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_ground_target(goal: u64, out: *mut [f32; 3]) {
    let t = ground_of(goal).target;
    // SAFETY: the caller's contract
    unsafe { *out = [f32::from_bits(t[0]), f32::from_bits(t[1]), f32::from_bits(t[2])] };
}

/// `GetDestination` of the attack-ground goal into `out`.
///
/// # Safety
/// `host` must be a valid host; `out` three writable floats.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_ground_destination(
    goal: u64,
    unit: u64,
    host: *const EeCombatGoalHost,
    out: *mut [f32; 3],
) {
    let mut h = unsafe { *host };
    let t = ground_destination(&mut h, goal, unit);
    unsafe { *out = [f32::from_bits(t[0]), f32::from_bits(t[1]), f32::from_bits(t[2])] };
}

/// The attack-ground goal's own checksum part.
#[no_mangle]
pub extern "C" fn ee_combat_ground_checksum(goal: u64) -> i32 {
    ground_checksum(goal)
}

/// The attack-ground goal's own `Save` part over `host`'s file.
///
/// # Safety
/// `host` must be a valid host.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_ground_save(goal: u64, host: *const EeCombatGoalHost) -> i32 {
    let mut h = unsafe { *host };
    ground_save(&mut h, goal)
}

/// The attack-ground goal's own `Load` part over `host`'s file.
///
/// # Safety
/// `host` must be a valid host.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_ground_load(goal: u64, host: *const EeCombatGoalHost) -> i32 {
    let mut h = unsafe { *host };
    ground_load(&mut h, goal)
}

#[cfg(test)]
#[path = "goal_objects_tests.rs"]
mod tests;
