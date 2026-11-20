//! The attack-ground, attack-unit (R5b.2, the twelfth round) and heal goals (the fourteenth) whole in Rust: each is an `EEUGRust` (the
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

/// The plain reads of the combat goals' operations, over the world views (the units' lane, round 2: they were
/// `EECombatGoalHostOp`'s one-line cases); None for another operation (or no unit), which the C++ answers.
#[cfg(not(test))]
fn view_read(code: i32, unit: u64) -> Option<i64> {
    use crate::combat::goal_attack::{
        EE_COMBAT_G_AMMO_TYPE, EE_COMBAT_G_CATEGORY, EE_COMBAT_G_COMPLEX, EE_COMBAT_G_FAMILY, EE_COMBAT_G_HIT_POINTS,
        EE_COMBAT_G_ID, EE_COMBAT_G_ON_TILE, EE_COMBAT_G_PARTS, EE_COMBAT_G_PLAYER, EE_COMBAT_G_VELOCITY,
    };
    use crate::combat::goal_dispatch::{EE_COMBAT_G_HAS_GROUP, EE_COMBAT_G_MAX_HIT_POINTS};
    if !matches!(
        code,
        EE_COMBAT_G_ON_TILE
            | EE_COMBAT_G_COMPLEX
            | EE_COMBAT_G_PARTS
            | EE_COMBAT_G_FAMILY
            | EE_COMBAT_G_CATEGORY
            | EE_COMBAT_G_ID
            | EE_COMBAT_G_HIT_POINTS
            | EE_COMBAT_G_VELOCITY
            | EE_COMBAT_G_PLAYER
            | EE_COMBAT_G_AMMO_TYPE
            | EE_COMBAT_G_MAX_HIT_POINTS
            | EE_COMBAT_G_HAS_GROUP
    ) {
        return None;
    }
    // SAFETY: a goal's operation runs inside the C++ call that holds the world; its units are live
    let t = unsafe { crate::world::WorldRead::begin() };
    // SAFETY: as above
    let u = unsafe { crate::world::UnitView::of(&t, unit) }?;
    Some(match code {
        EE_COMBAT_G_ON_TILE => i64::from(u.has_tile_node()),
        EE_COMBAT_G_COMPLEX => u.complex_unit_handle() as i64,
        EE_COMBAT_G_PARTS => i64::from(u.next_part_handle() != 0) | i64::from(u.previous_part().is_some()) << 1,
        EE_COMBAT_G_FAMILY => u.class()?.family() as i64,
        EE_COMBAT_G_CATEGORY => u.class()?.category() as i64,
        EE_COMBAT_G_ID => i64::from(u.id()),
        EE_COMBAT_G_HIT_POINTS => u.hit_points(),
        EE_COMBAT_G_VELOCITY => {
            let v: f32 = crate::world_field!(u.complex(), mVelocity);
            i64::from(v.to_bits())
        }
        EE_COMBAT_G_PLAYER => u.player().map_or(0, |p| p.handle() as i64),
        EE_COMBAT_G_AMMO_TYPE => crate::world_field!(u.class()?.fields(), mAmmoType) as i64,
        EE_COMBAT_G_MAX_HIT_POINTS => u.stats()?.max_hit_points(),
        _ => i64::from(u.active_group_handle() != 0),
    })
}

#[cfg(test)]
fn view_read(_code: i32, _unit: u64) -> Option<i64> {
    None
}

/// The plain reads and stores with arguments over the world views (the units behaviour lane, round 1: they were
/// `EECombatGoalHostOp`'s cases): the location, the tiles, the attackers' count, the players' reads and attack count,
/// the class's flare flag, the heal's clock, the save's version flags. None for another operation.
#[cfg(not(test))]
fn view_op(code: i32, unit: u64, a: &mut EeCombatGoalArgs) -> Option<i64> {
    use crate::combat::goal_attack::{
        EE_COMBAT_G_ATTACKER_FLAGS, EE_COMBAT_G_CAN_BE_ATTACKED, EE_COMBAT_G_HUMAN, EE_COMBAT_G_NUM_ATTACKERS,
        EE_COMBAT_G_PLAYER_ATTACK, EE_COMBAT_G_PLAYER_READ, EE_COMBAT_G_PRIEST, EE_COMBAT_G_PROJECTILE,
        EE_COMBAT_G_TILE,
    };
    use crate::combat::goal_dispatch::EE_COMBAT_G_SAVE_FLAGS;
    use crate::combat::goal_heal::EE_COMBAT_G_HEAL_ENV;
    use crate::world::{PlayerView, UnitView, WorldRead, WorldView};
    if let Some(r) = capture_op(code, unit, a) {
        return Some(r);
    }
    if code == EE_COMBAT_G_SAVE_FLAGS {
        let (major, minor) = (a[0] as u32, a[1] as u32);
        return Some(
            i64::from(crate::units::build::save_has_live_state(major, minor))
                | i64::from(major > 0 || minor >= SAVE_GOAL_GHOSTS) << 1,
        );
    }
    if !matches!(
        code,
        EE_COMBAT_G_UNIT_LOCATION
            | EE_COMBAT_G_TILE
            | EE_COMBAT_G_NUM_ATTACKERS
            | EE_COMBAT_G_PLAYER_READ
            | EE_COMBAT_G_PLAYER_ATTACK
            | EE_COMBAT_G_CAN_BE_ATTACKED
            | EE_COMBAT_G_HEAL_ENV
            | EE_COMBAT_G_ATTACKER_FLAGS
    ) {
        return None;
    }
    if code == EE_COMBAT_G_ATTACKER_FLAGS && unit == 0 {
        return Some(0);
    }
    // SAFETY: a goal's operation runs inside the C++ call that holds the world; its objects are live
    let t = unsafe { WorldRead::begin() };
    if code == EE_COMBAT_G_HEAL_ENV {
        let w = WorldView::global(&t)?;
        a[0] = i64::from(w.in_cinematic());
        a[1] = w.tick_ms();
        return Some(0);
    }
    if code == EE_COMBAT_G_PLAYER_READ {
        // SAFETY: as above (the handle is a player's: EE_COMBAT_G_PLAYER's answer)
        let p = unsafe { PlayerView::of(&t, unit) }?;
        return Some(if a[0] == 1 { p.enemy(a[1] as usize) as i64 } else { i64::from(p.id()) });
    }
    // SAFETY: as above
    let u = unsafe { UnitView::of(&t, unit) }?;
    match code {
        EE_COMBAT_G_UNIT_LOCATION => {
            let l = u.location();
            a[0] = i64::from(l[0].to_bits());
            a[1] = i64::from(l[1].to_bits());
            a[2] = i64::from(l[2].to_bits());
            Some(0)
        }
        EE_COMBAT_G_TILE => {
            let (x, y) = u.tile_xy();
            a[0] = x;
            a[1] = y;
            a[2] = x;
            a[3] = y;
            Some(0)
        }
        EE_COMBAT_G_NUM_ATTACKERS => {
            // the record is the C++ object's: the store is its member store
            let cx = u.complex().ptr() as *mut crate::world::unit_fields::EeUnitsComplexUnitFields;
            // SAFETY: a live complex unit's record, no other reference held
            unsafe {
                (*cx).mNumAttackers = ((*cx).mNumAttackers as i64).wrapping_add(a[0]) as _;
                Some((*cx).mNumAttackers as i64)
            }
        }
        EE_COMBAT_G_ATTACKER_FLAGS => {
            // the player's IsHuman() (a virtual: the unit utilities' host), the class's projectile flag, IsPriest()
            let player = u.player().map_or(0, |p| p.handle());
            let human = crate::units::uu::UuHost::is_human(&mut crate::units::uu_host::Game, player);
            let class = u.class()?;
            let projectile: bool = crate::world_field!(class.fields(), mIsProjectile);
            // IsPriest: the land, naval, helicopter and building priests (0xd, 0xe, 0x1a, 0x1e)
            let priest = matches!(class.category(), 0xd | 0xe | 0x1a | 0x1e);
            Some(
                if human { EE_COMBAT_G_HUMAN } else { 0 }
                    | if projectile { EE_COMBAT_G_PROJECTILE } else { 0 }
                    | if priest { EE_COMBAT_G_PRIEST } else { 0 },
            )
        }
        EE_COMBAT_G_CAN_BE_ATTACKED => {
            let flare: bool = crate::world_field!(u.class()?.fields(), mIsFlare);
            Some(i64::from(!flare))
        }
        _ => {
            // the attacker's player's attack count and combat state
            let p = u.player()?;
            let fields = p.fields().ptr() as *mut crate::world::player_fields::EeUnitsPlayerFields;
            let saved = p.saved().ptr() as *mut crate::world::player_fields::EeUnitsPlayerSaved;
            // SAFETY: a live player's records, no other reference held
            unsafe {
                let attacks = (*fields).mNumAttacks;
                if a[0] == 1 {
                    if attacks == 0 {
                        (*saved).mCombatState = 2;
                    }
                    (*fields).mNumAttacks = attacks.wrapping_add(1);
                } else if a[0] == -1 {
                    (*fields).mNumAttacks = attacks.wrapping_sub(1);
                    if (*fields).mNumAttacks == 0 {
                        (*saved).mCombatState = 1;
                    }
                } else {
                    if attacks == 0 && a[1] == 0 {
                        (*saved).mCombatState = 2;
                    }
                    (*fields).mNumAttacks = attacks.wrapping_add(1);
                }
            }
            Some(0)
        }
    }
}

/// The goal host's projectile test and its TEST ONLY records (the units' behaviour lane, round 2; they were
/// EEUGRustCombat.cpp `EECombatGoalHostOp`'s cases): the class's projectile class set, the CBGO record of a goal
/// call (`EECombatCaptureGoal`) and the CBSO record of a heal's work (`EECombatCaptureHealWork`), folded by
/// capture_fold.rs.
#[cfg(not(test))]
fn capture_op(code: i32, unit: u64, a: &mut EeCombatGoalArgs) -> Option<i64> {
    use crate::combat::capture_fold::{fold as fold_bytes, EE_COMBAT_FOLD_GOAL_OBJECTS, EE_COMBAT_FOLD_SHOTS};
    use crate::combat::goal_dispatch::{EE_COMBAT_G_CAPTURE, EE_COMBAT_G_PROJECTILE_CLASS};
    use crate::combat::goal_heal::EE_COMBAT_G_CAPTURE_HEAL_WORK;
    use crate::world::{UnitView, WorldRead};
    /// `EE_COMBAT_SO_HEAL_WORK`
    const SO_HEAL_WORK: u32 = 6;
    if !matches!(code, EE_COMBAT_G_PROJECTILE_CLASS | EE_COMBAT_G_CAPTURE | EE_COMBAT_G_CAPTURE_HEAL_WORK) {
        return None;
    }
    // SAFETY: the goal host's call runs inside the C++ caller's hold of the world; its handles are live units
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above (0: none)
    let view = |h: u64| unsafe { UnitView::of(&t, h) };
    let id = |h: u64| view(h).map_or(0, |u| u.id());
    let fold = |which: u32, words: &[u32]| {
        let bytes: Vec<u8> = words.iter().flat_map(|w| w.to_le_bytes()).collect();
        fold_bytes(which, &bytes);
    };
    match code {
        EE_COMBAT_G_PROJECTILE_CLASS => Some(i64::from(view(unit)?.class()?.projectile_class_handle() != 0)),
        EE_COMBAT_G_CAPTURE => {
            let words = [
                a[0] as u32,
                a[1] as u32,
                id(unit),
                id(a[2] as u64),
                a[3] as u32,
                a[4] as u32,
                a[5] as u32,
                a[6] as u32,
                a[7] as u32,
                (a[7] as u64 >> 32) as u32,
            ];
            fold(EE_COMBAT_FOLD_GOAL_OBJECTS, &words);
            Some(0)
        }
        _ => {
            let target = view(a[0] as u64);
            let words = [
                SO_HEAL_WORK,
                id(unit),
                target.map_or(0, |t| t.id()),
                target.map_or(0, |t| t.hit_points() as u32),
                a[1] as u32,
            ];
            fold(EE_COMBAT_FOLD_SHOTS, &words);
            Some(0)
        }
    }
}

#[cfg(test)]
fn view_op(_code: i32, _unit: u64, _args: &mut EeCombatGoalArgs) -> Option<i64> {
    None
}

/// `EESaveHasGoalGhosts`' first version (0.275)
#[cfg(not(test))]
const SAVE_GOAL_GHOSTS: u32 = 275;

impl TableHost<'_> {
    /// The operations the goals' common host answers (the units' `GoalOp`): the typed file calls, the references,
    /// the action started, the unit and ghost found, the hit points set. None for another.
    fn common_op(&mut self, code: i32, unit: u64, a: &mut EeCombatGoalArgs) -> Option<i64> {
        use crate::combat::goal_attack::{
            EE_COMBAT_G_DEREFERENCE, EE_COMBAT_G_FIND_GHOST, EE_COMBAT_G_FIND_UNIT, EE_COMBAT_G_READ_BOOL,
            EE_COMBAT_G_READ_I32, EE_COMBAT_G_READ_U32, EE_COMBAT_G_REFERENCE, EE_COMBAT_G_WRITE_BOOL,
            EE_COMBAT_G_WRITE_I32, EE_COMBAT_G_WRITE_U32,
        };
        use crate::combat::goal_heal::EE_COMBAT_G_SET_HIT_POINTS;
        use crate::units::rust_goal as g;
        let mut io = EeCombatGoalIo::default();
        // the typed file calls: GoalOp's kind 0 u32, 1 i32, 2 bool, 3 float bits, the value in i[1]
        let kind = match code {
            EE_COMBAT_G_WRITE_U32 | EE_COMBAT_G_READ_U32 => Some(0),
            EE_COMBAT_G_WRITE_I32 | EE_COMBAT_G_READ_I32 => Some(1),
            EE_COMBAT_G_WRITE_BOOL | EE_COMBAT_G_READ_BOOL => Some(2),
            EE_COMBAT_G_WRITE_F32 | EE_COMBAT_G_READ_F32 => Some(3),
            _ => None,
        };
        if let Some(kind) = kind {
            // GE3DPoint's Save and Load: 4 on a closed file
            if kind == 3 && self.call(g::EE_UNITS_GOAL_HOST_FILE_OPEN, 0, 0, &mut io) == 0 {
                return Some(4);
            }
            io.i[0] = kind;
            io.i[1] = a[0];
            let write = matches!(
                code,
                EE_COMBAT_G_WRITE_U32 | EE_COMBAT_G_WRITE_I32 | EE_COMBAT_G_WRITE_BOOL | EE_COMBAT_G_WRITE_F32
            );
            if write {
                return Some(self.call(g::EE_UNITS_GOAL_HOST_WRITE, 0, 0, &mut io));
            }
            let r = self.call(g::EE_UNITS_GOAL_HOST_READ, 0, 0, &mut io);
            a[0] = io.i[1];
            return Some(r);
        }
        let (op, ha, hb) = match code {
            EE_COMBAT_G_REFERENCE => (g::EE_UNITS_GOAL_HOST_REFERENCE, unit, 0),
            EE_COMBAT_G_DEREFERENCE => (g::EE_UNITS_GOAL_HOST_DEREFERENCE, unit, 0),
            // StartAction, then the class's ActionChanged (i[1] 0)
            EE_COMBAT_G_START_ACTION => (g::EE_UNITS_GOAL_HOST_START_ACTION, unit, 0),
            EE_COMBAT_G_FIND_UNIT => (g::EE_UNITS_GOAL_HOST_FIND_UNIT, a[0] as u64, u64::from(a[1] != 0)),
            EE_COMBAT_G_FIND_GHOST => (g::EE_UNITS_GOAL_HOST_FIND_GHOST, a[0] as u64, 0),
            EE_COMBAT_G_SET_HIT_POINTS => (g::EE_UNITS_GOAL_HOST_SET_HIT_POINTS, unit, 0),
            // EEGetAttackPart: the unit utilities' noted entry point
            crate::combat::goal_attack::EE_COMBAT_G_ATTACK_PART => {
                return Some(crate::units::uu_direct::attack_part(unit, a[0] as u64) as i64)
            }
            _ => return None,
        };
        io.i[0] = a[0];
        Some(self.call(op, ha, hb, &mut io))
    }
}

impl GoalHost for TableHost<'_> {
    fn op(&mut self, code: i32, unit: u64, args: &mut EeCombatGoalArgs) -> i64 {
        if let Some(r) = view_read(code, unit) {
            return r;
        }
        if let Some(r) = view_op(code, unit, args) {
            return r;
        }
        if let Some(r) = self.common_op(code, unit, args) {
            return r;
        }
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
    pub attack: BTreeMap<u64, crate::combat::goal_attack::EeCombatAttackGoal>,
    pub heal: BTreeMap<u64, crate::combat::goal_heal::EeCombatHealGoal>,
}

impl CombatGoals {
    pub const fn new() -> Self {
        CombatGoals { ground: BTreeMap::new(), attack: BTreeMap::new(), heal: BTreeMap::new() }
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
    let y = ee_base::math::um::floor(f32::from_bits(target[1]));
    let x = ee_base::math::um::floor(f32::from_bits(target[0]));
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
        g.heal.remove(&goal);
    });
}

#[cfg(test)]
#[path = "goal_objects_tests.rs"]
mod tests;
