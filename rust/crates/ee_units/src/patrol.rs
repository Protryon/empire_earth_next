//! The patrol goal whole in Rust (RUST_PLAN R5b.4, the sixteenth round): `EEUGPatrol`'s state (its patrol group,
//! its current point, its direction) and every body (`Step`, `Start`, `Clone`'s start point, the checksum, `Save`,
//! `Load`, `RepairPointers`, `SetPatrolGroup`, the destructor's releases) by the goal's object. The C++ class is
//! gone: a patrol goal is an `EEUGRust` of kind `kGoalPatrol` (ee/EEUGRust.h), whose virtuals call these over the
//! Rust goals' host (rust_goal.rs: the units' reads, the references, the typed file calls) and the patrol group's
//! walk (`PatrolHost`). The debug texts and the out-of-synch dump are Rust's too (the seventeenth round).
//!
//! The group and the point are reference-counted C++ objects held by handle (their addresses); between `Load` and
//! `RepairPointers` they are the saved ids, as the original kept them in its pointer fields.

use std::collections::BTreeMap;
use std::sync::Mutex;

use crate::rust_goal::{hex8, EeUnitsGoalIo, GoalHost, Held};

/// `kActionGetNextPatrolPoint`, `kActionPrepareToMove`
const ACTION_GET_NEXT_PATROL_POINT: i64 = 8020;
const ACTION_PREPARE_TO_MOVE: i64 = 8000;

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

/// The patrol group's calls (the FFI adapter; the tests' mock).
pub trait PatrolHost: GoalHost {
    /// a new, empty patrol group (`new EEPPatrolGroup`: one reference, the goal's)
    fn new_group(&mut self) -> u64;
    /// the group's `GetNext(unit)`, `GetPrevious(unit)`, `GetNumPoints()`, `mID`, `GetNearest(x, y)`
    fn group_next(&mut self, group: u64, unit: u64) -> u64;
    fn group_previous(&mut self, group: u64, unit: u64) -> u64;
    fn group_count(&mut self, group: u64) -> u32;
    fn group_id(&mut self, group: u64) -> u32;
    fn group_nearest(&mut self, group: u64, x: i64, y: i64) -> u64;
    /// the unit's player's patrol group of an id (0: none)
    fn player_group(&mut self, unit: u64, id: u32) -> u64;
    /// the group's `mPatrolPoints.size()` and its point `i` (0: an empty slot)
    fn group_points(&mut self, group: u64) -> u32;
    fn group_point(&mut self, group: u64, index: u32) -> u64;
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
    put(goal, Patrol { group: Held::of(group), target: Held::of(target), forward: true });
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
    p.group = Held::of(group);
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
    let point = p.target.saved_id();
    let mut result = 0;
    if point != u32::MAX {
        let unit = host.find_unit(point, true);
        p.target = Held::of(unit);
        if unit != 0 {
            host.reference(unit, false);
            let group = p.group.saved_id();
            if group != u32::MAX {
                let g = host.player_group(unit, group);
                p.group = Held::of(g);
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

/// The out-of-synch dump (the group's id, the point's id, the direction; the base goal's dump is empty).
pub fn dump(goal: u64, host: &mut dyn PatrolHost) {
    let p = get(goal);
    let id = host.group_id(p.group.object()) as i32;
    host.append(&format!("OUT OF SYNCH DUMP:  EEUGPatrol - Patrol Group ID is {id}\n"));
    let point = match p.target {
        Held::Object(t) => host.unit_id(t),
        _ => u32::MAX,
    };
    host.append(&format!("OUT OF SYNCH DUMP:  EEUGPatrol - Current Patrol Point #{}\n", hex8(point)));
    host.append(&format!("OUT OF SYNCH DUMP:  EEUGPatrol - Going Forward Flag is {}\n", i32::from(p.forward)));
}

/// `GetDebugLine`: "pat <points>:".
pub fn debug_line(goal: u64, host: &mut dyn PatrolHost) {
    let count = match get(goal).group {
        Held::Object(g) => host.group_count(g) as i32,
        _ => 0,
    };
    host.append(&format!("pat {count}:"));
}

/// `GetDebugText`: the point count, then each point's tile.
pub fn debug_text(goal: u64, host: &mut dyn PatrolHost) {
    let group = get(goal).group.object();
    let count = if group != 0 { host.group_count(group) as i32 } else { 0 };
    host.append(&format!("Patrol\nPatrol Points: {count}"));
    if group == 0 {
        return;
    }
    host.append("Patrol Points: ( ");
    for i in 0..host.group_points(group) {
        let point = host.group_point(group, i);
        if point == 0 {
            continue;
        }
        let (x, y) = host.unit_tile(point);
        host.append(&format!("{{{},{}}} ", x as i32, y as i32));
    }
    host.append(" )\n");
}

/// The goal's body `op` (rust_goal.rs `EE_UNITS_GOAL_*`).
pub fn body(op: i32, goal: u64, a: u64, b: u64, io: &mut EeUnitsGoalIo, host: &mut dyn PatrolHost) -> i64 {
    use crate::rust_goal::*;
    match op {
        EE_UNITS_GOAL_PATROL_NEW_EMPTY => create_empty(goal, host),
        EE_UNITS_GOAL_NEW_PATROL => create(goal, a, b, host),
        EE_UNITS_GOAL_DESTROY => destroy(goal, host),
        EE_UNITS_GOAL_PATROL_SET_GROUP => set_group(goal, a, host),
        EE_UNITS_GOAL_CLONE => {
            let group = self::group(goal);
            let start = if group != 0 { clone_target(goal, host) } else { 0 };
            let copy = host.new_goal(GOAL_PATROL, b != 0);
            create(copy, group, start, host);
            return copy as i64;
        }
        EE_UNITS_GOAL_CHECKSUM => return checksum(goal, host),
        EE_UNITS_GOAL_DUMP => dump(goal, host),
        EE_UNITS_GOAL_DEBUG_LINE => debug_line(goal, host),
        EE_UNITS_GOAL_DEBUG_TEXT => debug_text(goal, host),
        EE_UNITS_GOAL_LOCATION | EE_UNITS_GOAL_DESTINATION => {
            let target = target(goal);
            if target != 0 {
                let p = host.unit_position(target);
                io.f[..3].copy_from_slice(&p);
            }
            if op == EE_UNITS_GOAL_DESTINATION {
                host.tell_group(a, [io.f[0], io.f[1], io.f[2]]);
            }
        }
        EE_UNITS_GOAL_START_ACTION => return ACTION_GET_NEXT_PATROL_POINT,
        EE_UNITS_GOAL_STEP => step(goal, a, host),
        EE_UNITS_GOAL_START => start(goal, a, host),
        EE_UNITS_GOAL_SAVE => return i64::from(save(goal, host)),
        EE_UNITS_GOAL_LOAD => return i64::from(load(goal, host)),
        EE_UNITS_GOAL_REPAIR => return i64::from(repair(goal, host)),
        EE_UNITS_GOAL_EVALUATE => return i64::from(base_condition(b as i64)),
        EE_UNITS_GOAL_PATROL_TARGET => return target(goal) as i64,
        EE_UNITS_GOAL_PATROL_GROUP => return self::group(goal) as i64,
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
        text: String,
    }
    const G: u64 = 0x6000;

    impl GoalHost for World {
        fn reference(&mut self, o: u64, g: bool) {
            self.log.push(format!("ref {o:x} {g}"));
        }
        fn dereference(&mut self, o: u64, g: bool) {
            self.log.push(format!("deref {o:x} {g}"));
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
        fn find_unit(&mut self, id: u32, _c: bool) -> u64 {
            u64::from(id)
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
        fn unit_position(&mut self, u: u64) -> [f32; 3] {
            let (x, y) = self.unit_tile(u);
            [x as f32, y as f32, 0.0]
        }
        fn tile_location(&mut self, x: i64, y: i64) -> [f32; 3] {
            [x as f32, y as f32, 0.0]
        }
        fn tell_group(&mut self, u: u64, p: [f32; 3]) {
            self.log.push(format!("tell {u:x} {p:?}"));
        }
        fn set_physics_action(&mut self, _u: u64, _a: i64) {}
        fn set_animation(&mut self, _u: u64, _a: i64) {}
        fn drop_path(&mut self, _u: u64) {}
        fn new_goal(&mut self, _k: u8, _a: bool) -> u64 {
            0x7a7e_0f00
        }
        fn file_open(&mut self) -> bool {
            true
        }
        fn write_i32(&mut self, v: i32) -> i32 {
            self.file.push(v as u32);
            0
        }
        fn read_i32(&mut self, v: &mut i32) -> i32 {
            *v = self.reads.remove(0) as i32;
            0
        }
        fn append(&mut self, t: &str) {
            self.text.push_str(t);
        }
        fn append_class_name(&mut self, _u: u64) {}
    }

    impl PatrolHost for World {
        fn new_group(&mut self) -> u64 {
            self.log.push("new group".into());
            G
        }
        fn group_points(&mut self, _g: u64) -> u32 {
            self.points.len() as u32 + 1
        }
        fn group_point(&mut self, _g: u64, i: u32) -> u64 {
            // a NULL slot at the end
            self.points.get(i as usize).copied().unwrap_or(0)
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
        fn player_group(&mut self, _u: u64, id: u32) -> u64 {
            if id == 7 {
                G
            } else {
                0
            }
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
    fn the_texts_and_the_dispatcher() {
        use crate::rust_goal::*;
        let goal = 0x7a7e_0006u64;
        let mut w = world();
        let mut io = EeUnitsGoalIo::default();
        body(EE_UNITS_GOAL_NEW_PATROL, goal, G, 0x102, &mut io, &mut w);
        body(EE_UNITS_GOAL_DUMP, goal, 0, 0, &mut io, &mut w);
        assert_eq!(
            w.text,
            "OUT OF SYNCH DUMP:  EEUGPatrol - Patrol Group ID is 7\n\
             OUT OF SYNCH DUMP:  EEUGPatrol - Current Patrol Point #00000102\n\
             OUT OF SYNCH DUMP:  EEUGPatrol - Going Forward Flag is 1\n"
        );
        w.text.clear();
        body(EE_UNITS_GOAL_DEBUG_LINE, goal, 0, 0, &mut io, &mut w);
        body(EE_UNITS_GOAL_DEBUG_TEXT, goal, 0, 0, &mut io, &mut w);
        assert_eq!(w.text, "pat 3:Patrol\nPatrol Points: 3Patrol Points: ( {0,0} {10,0} {20,0}  )\n");
        assert_eq!(body(EE_UNITS_GOAL_START_ACTION, goal, 0, 0, &mut io, &mut w), 8020);
        assert_eq!(body(EE_UNITS_GOAL_EVALUATE, goal, 0x900, 9014, &mut io, &mut w), 1);
        assert_eq!(body(EE_UNITS_GOAL_EVALUATE, goal, 0x900, 2003, &mut io, &mut w), 0);
        body(EE_UNITS_GOAL_DESTINATION, goal, 0x900, 0, &mut io, &mut w);
        assert_eq!(io.f[..3], [10.0, 0.0, 0.0]);
        assert_eq!(w.log.last().unwrap(), "tell 900 [10.0, 0.0, 0.0]");
        // the copy starts at the point before the current one, in the same group
        let copy = body(EE_UNITS_GOAL_CLONE, goal, 0, 1, &mut io, &mut w) as u64;
        assert_eq!((group(copy), target(copy)), (G, 0x101));
        // no group: no text past the count
        let empty = 0x7a7e_0007u64;
        create(empty, 0, 0, &mut w);
        w.text.clear();
        debug_text(empty, &mut w);
        assert_eq!(w.text, "Patrol\nPatrol Points: 0");
        for g in [goal, copy, empty] {
            destroy(g, &mut w);
        }
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
