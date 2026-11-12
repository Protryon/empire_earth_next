//! The patrol goal whole in Rust (RUST_PLAN R5b.4, the sixteenth round): `EEUGPatrol`'s state (its patrol group,
//! its current point, its direction) and every body (`Step`, `Start`, `Clone`'s start point, the checksum, `Save`,
//! `Load`, `RepairPointers`, `SetPatrolGroup`, the destructor's releases) by the goal's object. The C++ class is
//! gone: a patrol goal is an `EEUGRust` of kind `kGoalPatrol` (ee/EEUGRust.h), whose virtuals call these over one
//! host table (`EePatrolHost`: the patrol group's walk, the units' reads, the references, the typed file calls).
//!
//! The group and the point are reference-counted C++ objects held by handle (their addresses); between `Load` and
//! `RepairPointers` they are the saved ids, as the original kept them in its pointer fields.

use std::collections::BTreeMap;
use std::sync::Mutex;

/// `kActionGetNextPatrolPoint`, `kActionPrepareToMove`
const ACTION_GET_NEXT_PATROL_POINT: i64 = 8020;
const ACTION_PREPARE_TO_MOVE: i64 = 8000;

/// What the group or the point field holds.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Held {
    None,
    /// a live object (referenced)
    Object(u64),
    /// a saved id read by `Load`, resolved by `RepairPointers`
    Saved(u32),
}

impl Held {
    fn object(self) -> u64 {
        match self {
            Held::Object(o) => o,
            _ => 0,
        }
    }
}

/// One patrol goal's state.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct Patrol {
    group: Held,
    target: Held,
    forward: bool,
}

/// The patrol goals by object.
#[derive(Default)]
pub struct Patrols {
    goals: BTreeMap<u64, Patrol>,
}

pub static PATROLS: Mutex<Patrols> = Mutex::new(Patrols { goals: BTreeMap::new() });

fn with_patrols<R>(f: impl FnOnce(&mut Patrols) -> R) -> R {
    let mut guard = PATROLS.lock().unwrap_or_else(|e| e.into_inner());
    f(&mut guard)
}

fn get(goal: u64) -> Patrol {
    with_patrols(|p| p.goals.get(&goal).copied()).unwrap_or(Patrol {
        group: Held::None,
        target: Held::None,
        forward: true,
    })
}

fn put(goal: u64, state: Patrol) {
    with_patrols(|p| {
        p.goals.insert(goal, state);
    });
}

/// The host's calls (the FFI table; the tests' mock).
pub trait PatrolHost {
    /// a new, empty patrol group (`new EEPPatrolGroup`: one reference, the goal's)
    fn new_group(&mut self) -> u64;
    /// `Reference()` / `Dereference()` of a group (`group` true) or a unit
    fn reference(&mut self, object: u64, group: bool);
    fn dereference(&mut self, object: u64, group: bool);
    /// the group's `GetNext(unit)`, `GetPrevious(unit)`, `GetNumPoints()`, `mID`, `GetNearest(x, y)`
    fn group_next(&mut self, group: u64, unit: u64) -> u64;
    fn group_previous(&mut self, group: u64, unit: u64) -> u64;
    fn group_count(&mut self, group: u64) -> u32;
    fn group_id(&mut self, group: u64) -> u32;
    fn group_nearest(&mut self, group: u64, x: i64, y: i64) -> u64;
    /// a unit's `mTileX`, `mTileY`, `mUnitID`, `mIsValid`, its command's type (0: none)
    fn unit_tile(&mut self, unit: u64) -> (i64, i64);
    fn unit_id(&mut self, unit: u64) -> u32;
    fn unit_valid(&mut self, unit: u64) -> bool;
    fn unit_command(&mut self, unit: u64) -> i64;
    /// the goal's `SetPathAttempts(0)`; the unit's `StartAction(a)` and its class's `ActionChanged`
    fn reset_path_attempts(&mut self, goal: u64);
    fn start_action(&mut self, unit: u64, action: i64);
    /// the world's complex unit of an id (`EEUnitCast<EEComplexUnit>(gWorld.GetUnit(id))`, 0: none); its player's
    /// patrol group of an id (0: none)
    fn find_unit(&mut self, id: u32) -> u64;
    fn player_group(&mut self, unit: u64, id: u32) -> u64;
    /// typed `FSFile` calls (`Write(unsigned long)`, `Write(bool)`, `Read(unsigned long&)`, `Read(bool&)`)
    fn write_u32(&mut self, value: u32) -> i32;
    fn write_bool(&mut self, value: bool) -> i32;
    fn read_u32(&mut self, value: &mut u32) -> i32;
    fn read_bool(&mut self, value: &mut bool) -> i32;
}

/// `EEUGPatrol(inArg)`: a new empty group, no point, forward.
pub fn create_empty(goal: u64, host: &mut dyn PatrolHost) {
    let group = host.new_group();
    put(goal, Patrol { group: Held::Object(group), target: Held::None, forward: true });
}

/// `EEUGPatrol(inGroup, inTarget, inArg)`: both referenced, forward.
pub fn create(goal: u64, group: u64, target: u64, host: &mut dyn PatrolHost) {
    if group != 0 {
        host.reference(group, true);
    }
    if target != 0 {
        host.reference(target, false);
    }
    put(goal, Patrol { group: held(group), target: held(target), forward: true });
}

fn held(object: u64) -> Held {
    if object == 0 {
        Held::None
    } else {
        Held::Object(object)
    }
}

/// The destructor: the group and the point released.
pub fn destroy(goal: u64, host: &mut dyn PatrolHost) {
    let Some(p) = with_patrols(|t| t.goals.remove(&goal)) else { return };
    if let Held::Object(g) = p.group {
        host.dereference(g, true);
    }
    if let Held::Object(t) = p.target {
        host.dereference(t, false);
    }
}

/// The current point (`mTarget`; 0: none or not resolved), the group (`mPatrolGroup`), the direction.
pub fn target(goal: u64) -> u64 {
    get(goal).target.object()
}

pub fn group(goal: u64) -> u64 {
    get(goal).group.object()
}

pub fn forward(goal: u64) -> bool {
    get(goal).forward
}

/// `SetPatrolGroup(inGroup)`: the old group released, the new one referenced.
pub fn set_group(goal: u64, group: u64, host: &mut dyn PatrolHost) {
    let mut p = get(goal);
    if let Held::Object(old) = p.group {
        host.dereference(old, true);
    }
    if group != 0 {
        host.reference(group, true);
    }
    p.group = held(group);
    put(goal, p);
}

/// `Clone`'s start point: the point before the current one (none without a group).
pub fn clone_target(goal: u64, host: &mut dyn PatrolHost) -> u64 {
    let p = get(goal);
    match p.group {
        Held::Object(g) => host.group_previous(g, p.target.object()),
        _ => 0,
    }
}

/// `GetOutOfSynchChecksum`'s part: the group's id, the point's unit id, the direction (the original reads the
/// group's id unguarded; a goal always has one when checked).
pub fn checksum(goal: u64, host: &mut dyn PatrolHost) -> i64 {
    let p = get(goal);
    let id = i64::from(host.group_id(p.group.object()) as i32);
    let target = match p.target {
        Held::Object(t) => i64::from(host.unit_id(t) as i32),
        _ => 0,
    };
    id.wrapping_add(target).wrapping_add(i64::from(p.forward))
}

/// `Step`: on get-next-patrol-point, the next point in the direction, turning back at either end; the unit moves.
/// A one-point patrol already reached stays.
pub fn step(goal: u64, unit: u64, host: &mut dyn PatrolHost) {
    if host.unit_command(unit) != ACTION_GET_NEXT_PATROL_POINT {
        return;
    }
    let mut p = get(goal);
    let Held::Object(group) = p.group else { return };
    let current = p.target.object();
    if current != 0 && host.group_count(group) == 1 && host.unit_tile(unit) == host.unit_tile(current) {
        return;
    }
    let next = if p.forward {
        let n = host.group_next(group, current);
        if n == 0 {
            p.forward = false;
            host.group_previous(group, current)
        } else {
            n
        }
    } else {
        let n = host.group_previous(group, current);
        if n == 0 {
            p.forward = true;
            host.group_next(group, current)
        } else {
            n
        }
    };
    // (the direction is the goal's even when no point follows)
    put(goal, p);
    if next == 0 {
        return;
    }
    if current != 0 {
        host.dereference(current, false);
    }
    p.target = Held::Object(next);
    put(goal, p);
    host.reference(next, false);
    host.reset_path_attempts(goal);
    host.start_action(unit, ACTION_PREPARE_TO_MOVE);
}

/// `Start`: the point nearest the unit, the unit moves (nothing without one).
pub fn start(goal: u64, unit: u64, host: &mut dyn PatrolHost) {
    let mut p = get(goal);
    if let Held::Object(t) = p.target {
        host.dereference(t, false);
    }
    p.target = Held::None;
    put(goal, p);
    let (x, y) = host.unit_tile(unit);
    let nearest = host.group_nearest(p.group.object(), x, y);
    if nearest == 0 {
        return;
    }
    p.target = Held::Object(nearest);
    put(goal, p);
    host.reference(nearest, false);
    host.start_action(unit, ACTION_PREPARE_TO_MOVE);
}

/// `Save`'s part after the base goal's: the group's id (-1: none or empty), the point's unit id (-1: none or
/// invalid), the direction; the first failing status.
pub fn save(goal: u64, host: &mut dyn PatrolHost) -> i32 {
    let p = get(goal);
    let group = match p.group {
        Held::Object(g) if host.group_count(g) != 0 => host.group_id(g),
        _ => u32::MAX,
    };
    let result = host.write_u32(group);
    if result != 0 {
        return result;
    }
    let unit = match p.target {
        Held::Object(t) if host.unit_valid(t) => host.unit_id(t),
        _ => u32::MAX,
    };
    let result = host.write_u32(unit);
    if result != 0 {
        return result;
    }
    host.write_bool(p.forward)
}

/// `Load`'s part after the base goal's: the group's id (the goal's own group released first), the point's id, the
/// direction; the first failing status. The ids wait for `repair`.
pub fn load(goal: u64, host: &mut dyn PatrolHost) -> i32 {
    let mut id = 0u32;
    let result = host.read_u32(&mut id);
    if result != 0 {
        return result;
    }
    set_group(goal, 0, host);
    let mut p = get(goal);
    p.group = Held::Saved(id);
    put(goal, p);
    let mut point = 0u32;
    let mut result = host.read_u32(&mut point);
    if result == 0 {
        p.target = Held::Saved(point);
        let mut forward = p.forward;
        result = host.read_bool(&mut forward);
        p.forward = forward;
    }
    put(goal, p);
    result
}

/// `RepairPointers`: the point's unit from its id (referenced), then the group from the point's player by its id
/// (referenced); 3 when the point or the group is gone.
pub fn repair(goal: u64, host: &mut dyn PatrolHost) -> i32 {
    let mut p = get(goal);
    let saved = |h: Held| match h {
        Held::Saved(id) => id,
        Held::Object(_) => u32::MAX - 1,
        Held::None => 0,
    };
    let point = saved(p.target);
    let mut result = 0;
    if point != u32::MAX {
        let unit = host.find_unit(point);
        p.target = held(unit);
        if unit != 0 {
            host.reference(unit, false);
            let group = saved(p.group);
            if group != u32::MAX {
                let g = host.player_group(unit, group);
                p.group = held(g);
                if g != 0 {
                    host.reference(g, true);
                } else {
                    result = 3;
                }
            } else {
                p.group = Held::None;
            }
        } else {
            result = 3;
            p.group = Held::None;
        }
    } else {
        p.target = Held::None;
        result = 3;
        p.group = Held::None;
    }
    put(goal, p);
    result
}

/// The C++ host table.
#[repr(C)]
pub struct EePatrolHost {
    pub context: *mut std::ffi::c_void,
    pub new_group: unsafe extern "C" fn(*mut std::ffi::c_void) -> u64,
    pub reference: unsafe extern "C" fn(*mut std::ffi::c_void, u64, bool),
    pub dereference: unsafe extern "C" fn(*mut std::ffi::c_void, u64, bool),
    pub group_next: unsafe extern "C" fn(*mut std::ffi::c_void, u64, u64) -> u64,
    pub group_previous: unsafe extern "C" fn(*mut std::ffi::c_void, u64, u64) -> u64,
    pub group_count: unsafe extern "C" fn(*mut std::ffi::c_void, u64) -> u32,
    pub group_id: unsafe extern "C" fn(*mut std::ffi::c_void, u64) -> u32,
    pub group_nearest: unsafe extern "C" fn(*mut std::ffi::c_void, u64, i64, i64) -> u64,
    pub unit_tile: unsafe extern "C" fn(*mut std::ffi::c_void, u64, *mut i64, *mut i64),
    pub unit_id: unsafe extern "C" fn(*mut std::ffi::c_void, u64) -> u32,
    pub unit_valid: unsafe extern "C" fn(*mut std::ffi::c_void, u64) -> bool,
    pub unit_command: unsafe extern "C" fn(*mut std::ffi::c_void, u64) -> i64,
    pub reset_path_attempts: unsafe extern "C" fn(*mut std::ffi::c_void, u64),
    pub start_action: unsafe extern "C" fn(*mut std::ffi::c_void, u64, i64),
    pub find_unit: unsafe extern "C" fn(*mut std::ffi::c_void, u32) -> u64,
    pub player_group: unsafe extern "C" fn(*mut std::ffi::c_void, u64, u32) -> u64,
    pub write_u32: unsafe extern "C" fn(*mut std::ffi::c_void, u32) -> i32,
    pub write_bool: unsafe extern "C" fn(*mut std::ffi::c_void, bool) -> i32,
    pub read_u32: unsafe extern "C" fn(*mut std::ffi::c_void, *mut u32) -> i32,
    pub read_bool: unsafe extern "C" fn(*mut std::ffi::c_void, *mut bool) -> i32,
}

// SAFETY (every call): the table's contract: `context` is live for the FFI call that handed out the table
impl PatrolHost for &EePatrolHost {
    fn new_group(&mut self) -> u64 {
        unsafe { (self.new_group)(self.context) }
    }
    fn reference(&mut self, object: u64, group: bool) {
        unsafe { (self.reference)(self.context, object, group) }
    }
    fn dereference(&mut self, object: u64, group: bool) {
        unsafe { (self.dereference)(self.context, object, group) }
    }
    fn group_next(&mut self, group: u64, unit: u64) -> u64 {
        unsafe { (self.group_next)(self.context, group, unit) }
    }
    fn group_previous(&mut self, group: u64, unit: u64) -> u64 {
        unsafe { (self.group_previous)(self.context, group, unit) }
    }
    fn group_count(&mut self, group: u64) -> u32 {
        unsafe { (self.group_count)(self.context, group) }
    }
    fn group_id(&mut self, group: u64) -> u32 {
        unsafe { (self.group_id)(self.context, group) }
    }
    fn group_nearest(&mut self, group: u64, x: i64, y: i64) -> u64 {
        unsafe { (self.group_nearest)(self.context, group, x, y) }
    }
    fn unit_tile(&mut self, unit: u64) -> (i64, i64) {
        let (mut x, mut y) = (0i64, 0i64);
        unsafe { (self.unit_tile)(self.context, unit, &mut x, &mut y) };
        (x, y)
    }
    fn unit_id(&mut self, unit: u64) -> u32 {
        unsafe { (self.unit_id)(self.context, unit) }
    }
    fn unit_valid(&mut self, unit: u64) -> bool {
        unsafe { (self.unit_valid)(self.context, unit) }
    }
    fn unit_command(&mut self, unit: u64) -> i64 {
        unsafe { (self.unit_command)(self.context, unit) }
    }
    fn reset_path_attempts(&mut self, goal: u64) {
        unsafe { (self.reset_path_attempts)(self.context, goal) }
    }
    fn start_action(&mut self, unit: u64, action: i64) {
        unsafe { (self.start_action)(self.context, unit, action) }
    }
    fn find_unit(&mut self, id: u32) -> u64 {
        unsafe { (self.find_unit)(self.context, id) }
    }
    fn player_group(&mut self, unit: u64, id: u32) -> u64 {
        unsafe { (self.player_group)(self.context, unit, id) }
    }
    fn write_u32(&mut self, value: u32) -> i32 {
        unsafe { (self.write_u32)(self.context, value) }
    }
    fn write_bool(&mut self, value: bool) -> i32 {
        unsafe { (self.write_bool)(self.context, value) }
    }
    fn read_u32(&mut self, value: &mut u32) -> i32 {
        unsafe { (self.read_u32)(self.context, value) }
    }
    fn read_bool(&mut self, value: &mut bool) -> i32 {
        unsafe { (self.read_bool)(self.context, value) }
    }
}

// ---- the FFI: one entry per body (`op`: EE_UNITS_PATROL_*)
pub const EE_UNITS_PATROL_CREATE_EMPTY: i32 = 1;
pub const EE_UNITS_PATROL_CREATE: i32 = 2;
pub const EE_UNITS_PATROL_DESTROY: i32 = 3;
pub const EE_UNITS_PATROL_SET_GROUP: i32 = 4;
pub const EE_UNITS_PATROL_CLONE_TARGET: i32 = 5;
pub const EE_UNITS_PATROL_CHECKSUM: i32 = 6;
pub const EE_UNITS_PATROL_STEP: i32 = 7;
pub const EE_UNITS_PATROL_START: i32 = 8;
pub const EE_UNITS_PATROL_SAVE: i32 = 9;
pub const EE_UNITS_PATROL_LOAD: i32 = 10;
pub const EE_UNITS_PATROL_REPAIR: i32 = 11;
pub const EE_UNITS_PATROL_TARGET: i32 = 12;
pub const EE_UNITS_PATROL_GROUP: i32 = 13;
pub const EE_UNITS_PATROL_FORWARD: i32 = 14;

/// A patrol goal's body `op` on the goal `goal` (`a`, `b`: the unit, the group or the target as the body takes
/// them); its result (a handle, a status, a checksum or a flag; 0 otherwise).
///
/// # Safety
/// `host` points to a valid table for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_patrol_goal(op: i32, goal: u64, a: u64, b: u64, host: *const EePatrolHost) -> i64 {
    // SAFETY: the caller's contract
    let mut h = unsafe { &*host };
    let h: &mut dyn PatrolHost = &mut h;
    match op {
        EE_UNITS_PATROL_CREATE_EMPTY => create_empty(goal, h),
        EE_UNITS_PATROL_CREATE => create(goal, a, b, h),
        EE_UNITS_PATROL_DESTROY => destroy(goal, h),
        EE_UNITS_PATROL_SET_GROUP => set_group(goal, a, h),
        EE_UNITS_PATROL_CLONE_TARGET => return clone_target(goal, h) as i64,
        EE_UNITS_PATROL_CHECKSUM => return checksum(goal, h),
        EE_UNITS_PATROL_STEP => step(goal, a, h),
        EE_UNITS_PATROL_START => start(goal, a, h),
        EE_UNITS_PATROL_SAVE => return i64::from(save(goal, h)),
        EE_UNITS_PATROL_LOAD => return i64::from(load(goal, h)),
        EE_UNITS_PATROL_REPAIR => return i64::from(repair(goal, h)),
        EE_UNITS_PATROL_TARGET => return target(goal) as i64,
        EE_UNITS_PATROL_GROUP => return group(goal) as i64,
        EE_UNITS_PATROL_FORWARD => return i64::from(forward(goal)),
        _ => {}
    }
    0
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A host with one group of points (unit handles in order) and a log.
    #[derive(Default)]
    struct World {
        points: Vec<u64>,
        tiles: BTreeMap<u64, (i64, i64)>,
        command: i64,
        log: Vec<String>,
        file: Vec<u32>,
        reads: Vec<u32>,
    }
    const G: u64 = 0x6000;

    impl PatrolHost for World {
        fn new_group(&mut self) -> u64 {
            self.log.push("new group".into());
            G
        }
        fn reference(&mut self, o: u64, g: bool) {
            self.log.push(format!("ref {o:x} {g}"));
        }
        fn dereference(&mut self, o: u64, g: bool) {
            self.log.push(format!("deref {o:x} {g}"));
        }
        fn group_next(&mut self, _g: u64, u: u64) -> u64 {
            if u == 0 {
                return self.points.first().copied().unwrap_or(0);
            }
            let i = self.points.iter().position(|p| *p == u).unwrap();
            self.points.get(i + 1).copied().unwrap_or(0)
        }
        fn group_previous(&mut self, _g: u64, u: u64) -> u64 {
            if u == 0 {
                return self.points.last().copied().unwrap_or(0);
            }
            let i = self.points.iter().position(|p| *p == u).unwrap();
            if i == 0 {
                0
            } else {
                self.points[i - 1]
            }
        }
        fn group_count(&mut self, _g: u64) -> u32 {
            self.points.len() as u32
        }
        fn group_id(&mut self, _g: u64) -> u32 {
            7
        }
        fn group_nearest(&mut self, _g: u64, x: i64, _y: i64) -> u64 {
            self.points.iter().copied().min_by_key(|p| (self.tiles[p].0 - x).abs()).unwrap_or(0)
        }
        fn unit_tile(&mut self, u: u64) -> (i64, i64) {
            *self.tiles.get(&u).unwrap_or(&(0, 0))
        }
        fn unit_id(&mut self, u: u64) -> u32 {
            u as u32
        }
        fn unit_valid(&mut self, _u: u64) -> bool {
            true
        }
        fn unit_command(&mut self, _u: u64) -> i64 {
            self.command
        }
        fn reset_path_attempts(&mut self, _g: u64) {
            self.log.push("attempts 0".into());
        }
        fn start_action(&mut self, u: u64, a: i64) {
            self.log.push(format!("start {u:x} {a}"));
        }
        fn find_unit(&mut self, id: u32) -> u64 {
            u64::from(id)
        }
        fn player_group(&mut self, _u: u64, id: u32) -> u64 {
            if id == 7 {
                G
            } else {
                0
            }
        }
        fn write_u32(&mut self, v: u32) -> i32 {
            self.file.push(v);
            0
        }
        fn write_bool(&mut self, v: bool) -> i32 {
            self.file.push(u32::from(v));
            0
        }
        fn read_u32(&mut self, v: &mut u32) -> i32 {
            *v = self.reads.remove(0);
            0
        }
        fn read_bool(&mut self, v: &mut bool) -> i32 {
            *v = self.reads.remove(0) != 0;
            0
        }
    }

    fn world() -> World {
        let mut w =
            World { points: vec![0x101, 0x102, 0x103], command: ACTION_GET_NEXT_PATROL_POINT, ..Default::default() };
        for (i, p) in w.points.iter().enumerate() {
            w.tiles.insert(*p, (10 * i as i64, 0));
        }
        w
    }

    #[test]
    fn the_patrol_walks_and_turns_back() {
        let goal = 0x7a7e_0001u64;
        let mut w = world();
        create(goal, G, 0x101, &mut w);
        w.log.clear();
        // forward to the end, then back
        let mut seen = vec![];
        for _ in 0..5 {
            step(goal, 0x900, &mut w);
            seen.push(target(goal));
        }
        assert_eq!(seen, vec![0x102, 0x103, 0x102, 0x101, 0x102]);
        assert!(forward(goal));
        // each move: the old point released, the new one held, the attempts reset, the unit moving
        assert_eq!(&w.log[..4], ["deref 101 false", "ref 102 false", "attempts 0", "start 900 8000"]);
        // another command: nothing
        w.command = 5000;
        w.log.clear();
        step(goal, 0x900, &mut w);
        assert!(w.log.is_empty());
        destroy(goal, &mut w);
        assert_eq!(w.log, ["deref 6000 true", "deref 102 false"]);
        assert_eq!(target(goal), 0);
    }

    #[test]
    fn a_one_point_patrol_reached_stays() {
        let goal = 0x7a7e_0002u64;
        let mut w = world();
        w.points = vec![0x101];
        w.tiles.insert(0x900, (0, 0));
        create(goal, G, 0x101, &mut w);
        w.log.clear();
        step(goal, 0x900, &mut w);
        assert!(w.log.is_empty());
        destroy(goal, &mut w);
    }

    #[test]
    fn start_clone_checksum_save_load_repair() {
        let goal = 0x7a7e_0003u64;
        let mut w = world();
        create_empty(goal, &mut w);
        assert_eq!(group(goal), G);
        // the start picks the nearest point
        w.tiles.insert(0x900, (19, 0));
        start(goal, 0x900, &mut w);
        assert_eq!(target(goal), 0x103);
        assert_eq!(clone_target(goal, &mut w), 0x102);
        assert_eq!(checksum(goal, &mut w), 7 + 0x103 + 1);
        save(goal, &mut w);
        assert_eq!(w.file, vec![7, 0x103, 1]);
        // load into another goal: the ids, then the repair resolves them
        let other = 0x7a7e_0004u64;
        create_empty(other, &mut w);
        w.reads = vec![7, 0x102, 0];
        w.log.clear();
        assert_eq!(load(other, &mut w), 0);
        assert_eq!(w.log, ["deref 6000 true"]);
        assert_eq!((target(other), group(other), forward(other)), (0, 0, false));
        assert_eq!(repair(other, &mut w), 0);
        assert_eq!((target(other), group(other)), (0x102, G));
        // a saved point of -1: none, status 3
        let third = 0x7a7e_0005u64;
        create_empty(third, &mut w);
        w.reads = vec![u32::MAX, u32::MAX, 1];
        load(third, &mut w);
        assert_eq!(repair(third, &mut w), 3);
        for g in [goal, other, third] {
            destroy(g, &mut w);
        }
    }
}
