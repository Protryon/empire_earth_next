//! The attack-ground, attack-unit and heal goals whole in Rust (R5b.2, the twelfth round): each is an `EEUGRust` (the
//! core lane's goal class whose state and bodies are Rust's, ee/EEUGRust.h), its own state a record here keyed by the
//! goal object's address, its bodies over a host of the C++ objects' operations (ee_units' goal host, `EeCombatGoalTable`: the unit's
//! reach, location and group, the actions, the typed file calls). The trigger tests and steps were Rust already
//! (goals.rs); the C++ keeps the object (the base goal's fields and vtable) and the text formatting of the debug and
//! out-of-synch dumps.
//!
//! Determinism: the records are read and written by key only (a `BTreeMap`, never iterated in the game); the floats
//! are kept as their bits and copied, never computed, except `GetDestination`'s floors (`UMFloor`, ee_ge's).

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::sync::Mutex;

// ---- the host's operations (the goal host's, from 1000): `unit` the call's unit, `args` the arguments and answers

/// `inUnit->CanReachTile(args[0], args[1])`.
pub const EE_COMBAT_G_CAN_REACH_TILE: i32 = 1001;
/// the unit's location's x, y, z as float bits into `args[0..3]`.
pub const EE_COMBAT_G_UNIT_LOCATION: i32 = 1002;
/// `if (inUnit->mActiveGroup != NULL) inUnit->mActiveGroup->TellGroup(inUnit, point)`, the point's bits in
/// `args[0..3]`; answers whether the unit had a group.
pub const EE_COMBAT_G_TELL_GROUP: i32 = 1003;
/// `inUnit->StartAction(args[0]); inUnit->mUnitClass->ActionChanged(inUnit)`.
pub const EE_COMBAT_G_START_ACTION: i32 = 1004;
/// the file's `Write(float)` of the bits in `args[0]`; answers the result.
pub const EE_COMBAT_G_WRITE_F32: i32 = 1010;
/// the file's `Read(float)` into `args[0]` (bits; its value before the read in `args[0]` too); answers the result.
pub const EE_COMBAT_G_READ_F32: i32 = 1011;

/// `kActionInitialAttackState` (ee/EEUnitAction.h).
pub const ACTION_INITIAL_ATTACK_STATE: i64 = 2016;
/// `kActionCheckRange`: the attack goals' first action.
pub const ACTION_CHECK_RANGE: i32 = 2013;

/// The arguments and answers of one host operation.
pub type EeCombatGoalArgs = [i64; 8];

/// The C++ objects' operations behind the goals' bodies (a mock in the tests).
pub trait GoalHost {
    fn op(&mut self, code: i32, unit: u64, args: &mut EeCombatGoalArgs) -> i64;
    /// an operation with a second handle (ee_units' host's `b`)
    fn op_ab(&mut self, code: i32, a: u64, b: u64, args: &mut EeCombatGoalArgs) -> i64 {
        let _ = b;
        self.op(code, a, args)
    }
}

/// ee_units' `EeUnitsGoalIo` (rust_goal.rs), the argument block of the goals' host: the same layout.
#[repr(C)]
pub struct EeCombatGoalIo {
    pub i: [i64; 8],
    pub f: [f32; 8],
    pub text: *const u8,
    pub len: usize,
    pub sink: *mut c_void,
    /// ee_units' handle list (the load goals'; unused here)
    pub list: *const u64,
    pub list_len: usize,
}

impl Default for EeCombatGoalIo {
    fn default() -> Self {
        EeCombatGoalIo {
            i: [0; 8],
            f: [0.0; 8],
            text: std::ptr::null(),
            len: 0,
            sink: std::ptr::null_mut(),
            list: std::ptr::null(),
            list_len: 0,
        }
    }
}

/// ee_units' `EeUnitsGoalHost`: the C++ host of every Rust goal (EEUGRust.cpp's `GoalOp`; the combat goals'
/// operations are its default case, EEUGRustCombat.cpp's `EECombatGoalHostOp`).
#[repr(C)]
pub struct EeCombatGoalTable {
    pub context: *mut c_void,
    pub op: unsafe extern "C" fn(*mut c_void, i32, u64, u64, *mut EeCombatGoalIo) -> i64,
}

/// The goals' host over the C++ table: `args` are the block's ints, `unit` its first handle.
pub struct TableHost<'a>(pub &'a EeCombatGoalTable);

impl TableHost<'_> {
    /// An operation of ee_units' common host with its whole block (`a`, `b` the handles).
    pub fn call(&mut self, code: i32, a: u64, b: u64, io: &mut EeCombatGoalIo) -> i64 {
        // SAFETY: the C++ host answers over its own context for the duration of the call
        unsafe { (self.0.op)(self.0.context, code, a, b, io) }
    }
}

impl GoalHost for TableHost<'_> {
    fn op(&mut self, code: i32, unit: u64, args: &mut EeCombatGoalArgs) -> i64 {
        let mut io = EeCombatGoalIo { i: *args, ..Default::default() };
        let r = self.call(code, unit, 0, &mut io);
        *args = io.i;
        r
    }
    fn op_ab(&mut self, code: i32, a: u64, b: u64, args: &mut EeCombatGoalArgs) -> i64 {
        let mut io = EeCombatGoalIo { i: *args, ..Default::default() };
        let r = self.call(code, a, b, &mut io);
        *args = io.i;
        r
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
    pub attack: BTreeMap<u64, crate::goal_attack::EeCombatAttackGoal>,
}

impl CombatGoals {
    pub const fn new() -> Self {
        CombatGoals { ground: BTreeMap::new(), attack: BTreeMap::new() }
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

/// The attack-ground goal's target point (float bits; 0 for a goal without a record).
pub fn ground_target_bits(goal: u64) -> [u32; 3] {
    ground_of(goal).target
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

// ---- the C ABI (the accessors' reads; the bodies are goal_dispatch.rs's)

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

/// A new attack-ground goal's record (its target point).
pub fn ground_create(goal: u64, point: [f32; 3]) {
    with_goals(|g| g.ground.insert(goal, GroundGoal { target: bits(point) }));
}

/// The goal object goes: its record with it (any kind).
pub fn goal_drop(goal: u64) {
    with_goals(|g| {
        g.ground.remove(&goal);
        g.attack.remove(&goal);
    });
}

#[cfg(test)]
#[path = "goal_objects_tests.rs"]
mod tests;
