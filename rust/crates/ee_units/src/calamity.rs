//! The cast calamity goal whole in Rust (RUST_PLAN R5b.4, the seventeenth round): `EEUGCastCalamity`'s state (the
//! casting unit, the target unit, the tile, the calamity's technology) and every body (the casting `Step` with its
//! placement and cast, the triggers, the target location, `SetOwner`, `ReplaceUnit`, the copy, the checksum, the
//! texts, `Save`, `Load`, `RepairPointers`, the destructor's releases) by the goal's object. The C++ class is gone:
//! a cast is an `EEUGRust` of kind `kGoalCastCalamity` (ee/EEUGRust.h) over the Rust goals' host (rust_goal.rs);
//! the calamity classes, the manager's casts and the units' geometry stay C++ host operations.
//!
//! Determinism (RUST_PLAN 2.1): the range is the original's `unsigned long` (wrapping sums and square), compared with
//! the `float` squared distance after the conversion to `float`; `UMFloatToLong` is `lrintf`.

use std::collections::BTreeMap;
use std::sync::Mutex;

use crate::physics::lrintf;
use crate::rust_goal::{base_condition, hex8, EeUnitsGoalIo, GoalHost, Held, TRIGGER_UNIT_FACING_NOT_REQUIRED};

/// `kActionReacquireGoal`, `kActionCalamityPreCast`, `kActionCastCalamity`, `kActionFaceEnemyUnit`
const ACTION_REACQUIRE_GOAL: i64 = 5001;
const ACTION_CALAMITY_PRE_CAST: i64 = 2030;
const ACTION_CAST_CALAMITY: i64 = 2017;
const ACTION_FACE_ENEMY_UNIT: i64 = 2012;
/// the triggers the goal answers
const TRIGGER_CALAMITY_TARGET_STILL_VALID: i64 = 15007;
const TRIGGER_FACING_ENEMY_UNIT: i64 = 2027;
const TRIGGER_UNIT_IN_WEAPON_RANGE: i64 = 2013;
const TRIGGER_ENEMY_UNIT_DESTROYED: i64 = 2003;
const TRIGGER_ENEMY_UNIT_UNDETECTABLE: i64 = 2059;
/// the unit set whose classes add their weapon range to the calamity's (0x91); the calamity that uses the weapon
/// range alone (0x17); the two calamities cast at the caster (0x77c, 0x1b6c)
const SET_RANGE_PLUS_WEAPON: i64 = 0x91;
const CALAMITY_WEAPON_RANGE: i64 = 0x17;
const TECH_AT_SOURCE_A: i32 = 0x77c;
const TECH_AT_SOURCE_B: i32 = 0x1b6c;
/// a tile off the map (`mTileX` of a removed unit)
const TILE_NONE: i64 = -10000;

/// One cast's state.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct Cast {
    source: Held,
    target: Held,
    x: i32,
    y: i32,
    technology: i32,
}

/// The empty goal of `EEUGCastCalamity(inArg)` (the load's): no units, tile -10000, technology 100000.
const EMPTY: Cast = Cast { source: Held::None, target: Held::None, x: -10000, y: -10000, technology: 100000 };

static CASTS: Mutex<BTreeMap<u64, Cast>> = Mutex::new(BTreeMap::new());

fn get(goal: u64) -> Cast {
    let casts = CASTS.lock().unwrap_or_else(|e| e.into_inner());
    casts.get(&goal).copied().unwrap_or(EMPTY)
}

fn put(goal: u64, state: Cast) {
    let mut casts = CASTS.lock().unwrap_or_else(|e| e.into_inner());
    casts.insert(goal, state);
}

/// The calamity classes' and the units' calls (the FFI adapter; the tests' mock).
pub trait CalamityHost: GoalHost {
    /// the unit's player's technology's calamity class (0: no node or class), the node's `mCalamity` (twice: the
    /// original read it as the field and through `GetCalamity()`)
    fn calamity_class(&mut self, unit: u64, technology: i32) -> (u64, i64, i64);
    /// the class's `Place(unit, x, y)`
    fn calamity_place(&mut self, class: u64, unit: u64, x: i32, y: i32);
    /// the class's `GetTargetKind()`, `GetCost()`, `mRange`
    fn calamity_info(&mut self, class: u64) -> (i64, i32, i32);
    /// the manager's cast of a target kind (3: on the target unit, 2: at the tile, 1: global) by the source for the
    /// unit's player; whether it was made
    fn create_calamity(
        &mut self,
        kind: i64,
        calamity: i64,
        source: u64,
        target: u64,
        tile: (i32, i32),
        unit: u64,
    ) -> bool;
    /// the unit's player's saved group 6 reset (`mSavedGroups.Reset(6, 1)`)
    fn reset_saved_group(&mut self, unit: u64);
    /// `SetPower(GetPower() - cost)`
    fn spend_power(&mut self, unit: u64, cost: i32);
    /// the class's `EECalamityClass::CanCastAt(unit, location, x, y, true, NULL) == 0`
    fn can_cast_at(&mut self, class: u64, unit: u64, x: i32, y: i32) -> bool;
    /// the unit class's `FacesTarget(unit, target, false)`, `FacesPoint(unit, point, false)`
    fn faces_target(&mut self, unit: u64, target: u64) -> bool;
    fn faces_point(&mut self, unit: u64, point: [f32; 3]) -> bool;
    /// whether the unit set exists and holds the unit's class type
    fn unit_in_set(&mut self, unit: u64, set: i64) -> bool;
    /// the unit's stats' `mRange`
    fn stats_range(&mut self, unit: u64) -> f32;
    /// `EEIsInContactRange(unit, target, distance)` over the two positions' squared distance
    fn contact_range(&mut self, unit: u64, target: u64) -> bool;
    /// `unit->DistanceSquaredToUnit(target)`
    fn distance_to_unit(&mut self, unit: u64, target: u64) -> f32;
    /// `mHeight`, `mHitPoints`
    fn unit_height(&mut self, unit: u64) -> f32;
    fn hit_points(&mut self, unit: u64) -> i32;
    /// the target cloaked from the unit's player: cloakable and cloaked, another player, an enemy, no cloak radius
    fn undetectable(&mut self, unit: u64, target: u64) -> bool;
    /// the unit's `mComplexUnit`
    fn complex_unit(&mut self, unit: u64) -> u64;
}

/// `EEUGCastCalamity(source, target, x, y, technology, arg)`: both units referenced.
pub fn create(goal: u64, source: u64, target: u64, x: i32, y: i32, technology: i32, host: &mut dyn CalamityHost) {
    if source != 0 {
        host.reference(source, false);
    }
    if target != 0 {
        host.reference(target, false);
    }
    put(goal, Cast { source: Held::of(source), target: Held::of(target), x, y, technology });
}

/// `EEUGCastCalamity(inArg)`
pub fn create_empty(goal: u64) {
    put(goal, EMPTY);
}

/// The destructor: the source and the target released.
pub fn destroy(goal: u64, host: &mut dyn CalamityHost) {
    let Some(c) = CASTS.lock().unwrap_or_else(|e| e.into_inner()).remove(&goal) else { return };
    if let Held::Object(s) = c.source {
        host.dereference(s, false);
    }
    if let Held::Object(t) = c.target {
        host.dereference(t, false);
    }
}

/// The technology, the tile and the target unit (0: none).
pub fn fields(goal: u64) -> (i32, i32, i32, u64) {
    let c = get(goal);
    (c.technology, c.x, c.y, c.target.object())
}

/// `SetOwner`: a new source, the reference moved.
pub fn set_owner(goal: u64, source: u64, host: &mut dyn CalamityHost) {
    let mut c = get(goal);
    if let Held::Object(s) = c.source {
        host.dereference(s, false);
    }
    c.source = Held::of(source);
    put(goal, c);
    if source != 0 {
        host.reference(source, false);
    }
}

/// `ReplaceUnit`: a target whose complex unit is `old` becomes `new` (the reference moved).
pub fn replace_unit(goal: u64, old: u64, new: u64, host: &mut dyn CalamityHost) {
    let mut c = get(goal);
    let Held::Object(t) = c.target else { return };
    if host.complex_unit(t) != old {
        return;
    }
    host.dereference(t, false);
    c.target = Held::of(new);
    put(goal, c);
    host.reference(new, false);
}

/// `GetGoalLocation`: the target unit's position, else the tile's location.
pub fn goal_location(goal: u64, host: &mut dyn CalamityHost) -> [f32; 3] {
    let c = get(goal);
    match c.target {
        Held::Object(t) => host.unit_position(t),
        _ => host.tile_location(i64::from(c.x), i64::from(c.y)),
    }
}

/// Where the calamity goes: the target unit, the source itself for two technologies, else the tile; at the source's
/// height when it flies.
pub fn target_location(goal: u64, host: &mut dyn CalamityHost) -> [f32; 3] {
    let c = get(goal);
    let mut p = if let Held::Object(t) = c.target {
        host.unit_position(t)
    } else if c.technology == TECH_AT_SOURCE_A || c.technology == TECH_AT_SOURCE_B {
        host.unit_position(c.source.object())
    } else {
        host.tile_location(i64::from(c.x), i64::from(c.y))
    };
    // (the original read the source's height unguarded)
    if let Held::Object(s) = c.source {
        let height = host.unit_height(s);
        if height > 0.0 {
            p[2] = height;
        }
    }
    p
}

/// The pre-cast: the calamity's class places it at the tile.
fn prepare(c: &Cast, unit: u64, host: &mut dyn CalamityHost) {
    let (class, _, _) = host.calamity_class(unit, c.technology);
    if class != 0 {
        host.calamity_place(class, unit, c.x, c.y);
    }
}

/// The cast: the unit stops, the manager casts by the class's target kind, the source's player's saved group 6 is
/// reset and the unit pays the cost when the cast was made.
fn cast(c: &Cast, unit: u64, host: &mut dyn CalamityHost) {
    host.set_physics_action(unit, 6);
    host.drop_path(unit);
    let (class, _, calamity) = host.calamity_class(unit, c.technology);
    if class == 0 {
        return;
    }
    let (kind, cost, _) = host.calamity_info(class);
    let made = match kind {
        1..=3 => host.create_calamity(kind, calamity, c.source.object(), c.target.object(), (c.x, c.y), unit),
        _ => false,
    };
    host.reset_saved_group(c.source.object());
    if made {
        host.spend_power(unit, cost);
    }
}

/// `Step`: the pre-cast placement, the cast (animation 0xb) or facing the target (animation 0xd).
pub fn step(goal: u64, unit: u64, host: &mut dyn CalamityHost) {
    let c = get(goal);
    match host.unit_command(unit) {
        ACTION_CALAMITY_PRE_CAST => prepare(&c, unit, host),
        ACTION_CAST_CALAMITY => {
            host.set_physics_action(unit, 1);
            host.set_animation(unit, 0xb);
            cast(&c, unit, host);
        }
        ACTION_FACE_ENEMY_UNIT => {
            host.set_physics_action(unit, 1);
            host.set_animation(unit, 0xd);
            host.drop_path(unit);
        }
        _ => {}
    }
}

/// Whether the unit can cast the calamity at the tile now.
fn can_cast(c: &Cast, unit: u64, host: &mut dyn CalamityHost) -> bool {
    let (class, _, _) = host.calamity_class(unit, c.technology);
    class != 0 && host.can_cast_at(class, unit, c.x, c.y)
}

/// `EvaluateCondition`: the casting triggers; the others the base goal's.
pub fn evaluate(goal: u64, unit: u64, event: i64, host: &mut dyn CalamityHost) -> bool {
    let c = get(goal);
    match event {
        TRIGGER_CALAMITY_TARGET_STILL_VALID => can_cast(&c, unit, host),
        TRIGGER_FACING_ENEMY_UNIT => match c.target {
            Held::Object(t) => host.faces_target(unit, t),
            _ => {
                let p = goal_location(goal, host);
                host.faces_point(unit, p)
            }
        },
        TRIGGER_UNIT_IN_WEAPON_RANGE => {
            let mut range: u32 = 1000;
            let (class, calamity, _) = host.calamity_class(unit, c.technology);
            if class != 0 {
                let (_, _, class_range) = host.calamity_info(class);
                range = if host.unit_in_set(unit, SET_RANGE_PLUS_WEAPON) {
                    class_range.wrapping_add(lrintf(host.stats_range(unit))) as u32
                } else if calamity == CALAMITY_WEAPON_RANGE {
                    lrintf(host.stats_range(unit)) as u32
                } else {
                    class_range as u32
                };
            }
            let limit = range.wrapping_mul(range) as f32;
            match c.target {
                Held::Object(t) => {
                    if range < 1 {
                        host.contact_range(unit, t)
                    } else {
                        host.distance_to_unit(unit, t) <= limit
                    }
                }
                _ => {
                    let from = host.unit_position(unit);
                    let to = target_location(goal, host);
                    distance_squared(from, to) <= limit
                }
            }
        }
        TRIGGER_ENEMY_UNIT_DESTROYED | TRIGGER_ENEMY_UNIT_UNDETECTABLE => {
            let Held::Object(t) = c.target else { return false };
            if event == TRIGGER_ENEMY_UNIT_DESTROYED
                && (host.hit_points(t) == 0 || host.unit_tile(t).0 as i32 as i64 == TILE_NONE)
            {
                return true;
            }
            host.undetectable(unit, t)
        }
        TRIGGER_UNIT_FACING_NOT_REQUIRED => true,
        _ => base_condition(event),
    }
}

/// `GE3DPoint::DistanceToSquared`
fn distance_squared(a: [f32; 3], b: [f32; 3]) -> f32 {
    let p = ee_ge::types::Ge3DPoint { x: a[0], y: a[1], z: a[2] };
    p.distance_to_squared(ee_ge::types::Ge3DPoint { x: b[0], y: b[1], z: b[2] })
}

/// A unit's id, or `none` without one.
fn id_or(held: Held, none: u32, host: &mut dyn CalamityHost) -> u32 {
    match held {
        Held::Object(u) => host.unit_id(u),
        _ => none,
    }
}

/// The checksum's part: the source's and target's ids, the tile and the technology.
pub fn checksum(goal: u64, host: &mut dyn CalamityHost) -> i64 {
    let c = get(goal);
    let sum = id_or(c.source, 0, host)
        .wrapping_add(id_or(c.target, 0, host))
        .wrapping_add((c.x as u32).wrapping_add(c.y as u32).wrapping_add(c.technology as u32));
    i64::from(sum as i32)
}

/// The out-of-synch dump.
pub fn dump(goal: u64, host: &mut dyn CalamityHost) {
    let c = get(goal);
    let source = id_or(c.source, u32::MAX, host);
    let target = id_or(c.target, u32::MAX, host);
    host.append(&format!("OUT OF SYNCH DUMP:  EEUGCastCalamity Source Unit ID #{}\n", hex8(source)));
    host.append(&format!("OUT OF SYNCH DUMP:  EEUGCastCalamity Target Unit ID #{}\n", hex8(target)));
    host.append(&format!("OUT OF SYNCH DUMP:  EEUGCastCalamity Tile X: {}\n", c.x));
    host.append(&format!("OUT OF SYNCH DUMP:  EEUGCastCalamity Tile Y: {}\n", c.y));
    host.append(&format!("OUT OF SYNCH DUMP:  EEUGCastCalamity Calamity Type: {}\n", c.technology));
}

/// `GetDebugLine`: "cc: source,target,x,y,technology".
pub fn debug_line(goal: u64, host: &mut dyn CalamityHost) {
    let c = get(goal);
    let source = id_or(c.source, u32::MAX, host) as i32;
    let target = id_or(c.target, u32::MAX, host) as i32;
    host.append(&format!("cc: {source},{target},{},{},{}", c.x, c.y, c.technology));
}

/// `GetDebugText`: the target's class name, the tile and the technology.
pub fn debug_text(goal: u64, host: &mut dyn CalamityHost) {
    let c = get(goal);
    host.append("Casting Calamity");
    if let Held::Object(t) = c.target {
        host.append("Cast On Unit: ");
        host.append_class_name(t);
        host.append("\n");
    }
    host.append(&format!("Calamity Location/Type: ({},{}) - Type:{}", c.x, c.y, c.technology));
}

/// `Save`'s part after the base goal's: the source's and target's ids (-1: none), the tile, the technology; the first
/// failing status.
pub fn save(goal: u64, host: &mut dyn CalamityHost) -> i32 {
    let c = get(goal);
    let source = id_or(c.source, u32::MAX, host);
    let mut result = host.write_u32(source);
    if result != 0 {
        return result;
    }
    let target = id_or(c.target, u32::MAX, host);
    result = host.write_u32(target);
    if result != 0 {
        return result;
    }
    result = host.write_i32(c.x);
    if result != 0 {
        return result;
    }
    result = host.write_i32(c.y);
    if result == 0 {
        result = host.write_i32(c.technology);
    }
    result
}

/// `Load`'s part after the base goal's: the two ids (resolved by `repair`), the tile, the technology; the first
/// failing status (the fields read so far kept).
pub fn load(goal: u64, host: &mut dyn CalamityHost) -> i32 {
    let mut c = get(goal);
    let mut id = u32::MAX;
    let mut result = host.read_u32(&mut id);
    if result == 0 {
        c.source = Held::Saved(id);
        result = host.read_u32(&mut id);
        if result == 0 {
            c.target = Held::Saved(id);
            result = host.read_i32(&mut c.x);
            if result == 0 {
                result = host.read_i32(&mut c.y);
                if result == 0 {
                    result = host.read_i32(&mut c.technology);
                }
            }
        }
    }
    put(goal, c);
    result
}

/// `RepairPointers`: the source (a complex unit) and the target (any unit) from their ids, referenced; 3 and both
/// cleared when the source is gone.
pub fn repair(goal: u64, host: &mut dyn CalamityHost) -> i32 {
    let mut c = get(goal);
    let mut result = 3;
    let source = c.source.saved_id();
    if source != u32::MAX {
        let unit = host.find_unit(source, true);
        c.source = Held::of(unit);
        if unit != 0 {
            host.reference(unit, false);
            result = 0;
            let target = c.target.saved_id();
            if target != u32::MAX {
                let t = host.find_unit(target, false);
                c.target = Held::of(t);
                if t != 0 {
                    host.reference(t, false);
                }
            } else {
                c.target = Held::None;
            }
        } else {
            c.target = Held::None;
        }
    } else {
        c.source = Held::None;
        c.target = Held::None;
    }
    put(goal, c);
    result
}

/// The goal's body `op` (rust_goal.rs `EE_UNITS_GOAL_*`).
pub fn body(op: i32, goal: u64, a: u64, b: u64, io: &mut EeUnitsGoalIo, host: &mut dyn CalamityHost) -> i64 {
    use crate::rust_goal::*;
    let point = |io: &mut EeUnitsGoalIo, p: [f32; 3]| io.f[..3].copy_from_slice(&p);
    match op {
        EE_UNITS_GOAL_NEW_CALAMITY => create(goal, a, b, io.i[0] as i32, io.i[1] as i32, io.i[2] as i32, host),
        EE_UNITS_GOAL_CALAMITY_NEW_EMPTY => create_empty(goal),
        EE_UNITS_GOAL_DESTROY => destroy(goal, host),
        EE_UNITS_GOAL_CLONE => {
            let c = get(goal);
            let copy = host.new_goal(GOAL_CAST_CALAMITY, b != 0);
            create(copy, c.source.object(), c.target.object(), c.x, c.y, c.technology, host);
            return copy as i64;
        }
        EE_UNITS_GOAL_CHECKSUM => return checksum(goal, host),
        EE_UNITS_GOAL_DUMP => dump(goal, host),
        EE_UNITS_GOAL_DEBUG_LINE => debug_line(goal, host),
        EE_UNITS_GOAL_DEBUG_TEXT => debug_text(goal, host),
        EE_UNITS_GOAL_LOCATION => point(io, goal_location(goal, host)),
        EE_UNITS_GOAL_CALAMITY_TARGET_LOCATION => point(io, target_location(goal, host)),
        EE_UNITS_GOAL_DESTINATION => {
            let p = target_location(goal, host);
            point(io, p);
            host.tell_group(a, p);
        }
        EE_UNITS_GOAL_START_ACTION => return ACTION_REACQUIRE_GOAL,
        EE_UNITS_GOAL_SET_OWNER => set_owner(goal, a, host),
        EE_UNITS_GOAL_STEP => step(goal, a, host),
        EE_UNITS_GOAL_SAVE => return i64::from(save(goal, host)),
        EE_UNITS_GOAL_LOAD => return i64::from(load(goal, host)),
        EE_UNITS_GOAL_REPAIR => return i64::from(repair(goal, host)),
        EE_UNITS_GOAL_EVALUATE => return i64::from(evaluate(goal, a, b as i64, host)),
        EE_UNITS_GOAL_REPLACE_UNIT => replace_unit(goal, a, b, host),
        EE_UNITS_GOAL_CALAMITY_FIELDS => {
            let (technology, x, y, target) = fields(goal);
            io.i[0] = i64::from(technology);
            io.i[1] = i64::from(x);
            io.i[2] = i64::from(y);
            return target as i64;
        }
        // EE_UNITS_GOAL_START: the original's is empty
        _ => {}
    }
    0
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::rust_goal::*;

    /// A world of units by handle (tile, position, id) with a calamity class and a log of the world calls.
    #[derive(Default)]
    struct World {
        log: Vec<String>,
        text: String,
        command: i64,
        file: Vec<i64>,
        reads: Vec<i64>,
        /// the class handle the technology resolves to (0: none), its mCalamity, GetCalamity()
        class: (u64, i64, i64),
        /// GetTargetKind, GetCost, mRange
        info: (i64, i32, i32),
        made: bool,
        in_set: bool,
        stats_range: f32,
        distance: f32,
        height: f32,
        hp: i32,
        positions: BTreeMap<u64, [f32; 3]>,
    }
    const SOURCE: u64 = 0x1100;
    const TARGET: u64 = 0x2200;

    impl GoalHost for World {
        fn reference(&mut self, o: u64, _g: bool) {
            self.log.push(format!("ref {o:x}"));
        }
        fn dereference(&mut self, o: u64, _g: bool) {
            self.log.push(format!("deref {o:x}"));
        }
        fn unit_id(&mut self, u: u64) -> u32 {
            (u >> 8) as u32
        }
        fn unit_tile(&mut self, u: u64) -> (i64, i64) {
            let p = self.positions.get(&u).copied().unwrap_or_default();
            (p[0] as i64, p[1] as i64)
        }
        fn unit_valid(&mut self, _u: u64) -> bool {
            true
        }
        fn unit_command(&mut self, _u: u64) -> i64 {
            self.command
        }
        fn reset_path_attempts(&mut self, _g: u64) {}
        fn start_action(&mut self, u: u64, a: i64) {
            self.log.push(format!("start {u:x} {a}"));
        }
        fn find_unit(&mut self, id: u32, complex: bool) -> u64 {
            self.log.push(format!("find {id} {complex}"));
            if id == 0x55 {
                0
            } else {
                u64::from(id) << 8
            }
        }
        fn unit_position(&mut self, u: u64) -> [f32; 3] {
            self.positions.get(&u).copied().unwrap_or_default()
        }
        fn tile_location(&mut self, x: i64, y: i64) -> [f32; 3] {
            [x as f32 + 0.5, y as f32 + 0.5, 2.0]
        }
        fn tell_group(&mut self, u: u64, p: [f32; 3]) {
            self.log.push(format!("tell {u:x} {p:?}"));
        }
        fn set_physics_action(&mut self, u: u64, a: i64) {
            self.log.push(format!("physics {u:x} {a}"));
        }
        fn set_animation(&mut self, u: u64, a: i64) {
            self.log.push(format!("animation {u:x} {a:x}"));
        }
        fn drop_path(&mut self, u: u64) {
            self.log.push(format!("drop path {u:x}"));
        }
        fn new_goal(&mut self, k: u8, a: bool) -> u64 {
            self.log.push(format!("new goal {k} {a}"));
            0xca57_0f00
        }
        fn file_open(&mut self) -> bool {
            true
        }
        fn write_u32(&mut self, v: u32) -> i32 {
            self.file.push(i64::from(v));
            0
        }
        fn write_i32(&mut self, v: i32) -> i32 {
            self.file.push(i64::from(v));
            0
        }
        fn write_bool(&mut self, v: bool) -> i32 {
            self.file.push(i64::from(v));
            0
        }
        fn read_u32(&mut self, v: &mut u32) -> i32 {
            if self.reads.is_empty() {
                return 5;
            }
            *v = self.reads.remove(0) as u32;
            0
        }
        fn read_i32(&mut self, v: &mut i32) -> i32 {
            if self.reads.is_empty() {
                return 5;
            }
            *v = self.reads.remove(0) as i32;
            0
        }
        fn read_bool(&mut self, v: &mut bool) -> i32 {
            *v = self.reads.remove(0) != 0;
            0
        }
        fn append(&mut self, t: &str) {
            self.text.push_str(t);
        }
        fn append_class_name(&mut self, u: u64) {
            self.text.push_str(&format!("class of {u:x}"));
        }
    }

    impl CalamityHost for World {
        fn calamity_class(&mut self, _u: u64, t: i32) -> (u64, i64, i64) {
            self.log.push(format!("class {t}"));
            self.class
        }
        fn calamity_place(&mut self, c: u64, u: u64, x: i32, y: i32) {
            self.log.push(format!("place {c:x} {u:x} {x} {y}"));
        }
        fn calamity_info(&mut self, _c: u64) -> (i64, i32, i32) {
            self.info
        }
        fn create_calamity(&mut self, k: i64, c: i64, s: u64, t: u64, (x, y): (i32, i32), u: u64) -> bool {
            self.log.push(format!("create {k} {c} {s:x} {t:x} {x} {y} {u:x}"));
            self.made
        }
        fn reset_saved_group(&mut self, u: u64) {
            self.log.push(format!("saved group {u:x}"));
        }
        fn spend_power(&mut self, u: u64, c: i32) {
            self.log.push(format!("power {u:x} -{c}"));
        }
        fn can_cast_at(&mut self, _c: u64, _u: u64, x: i32, _y: i32) -> bool {
            x > 0
        }
        fn faces_target(&mut self, _u: u64, t: u64) -> bool {
            self.log.push(format!("faces {t:x}"));
            true
        }
        fn faces_point(&mut self, _u: u64, p: [f32; 3]) -> bool {
            self.log.push(format!("faces {p:?}"));
            false
        }
        fn unit_in_set(&mut self, _u: u64, s: i64) -> bool {
            assert_eq!(s, 0x91);
            self.in_set
        }
        fn stats_range(&mut self, _u: u64) -> f32 {
            self.stats_range
        }
        fn contact_range(&mut self, _u: u64, _t: u64) -> bool {
            self.log.push("contact".into());
            true
        }
        fn distance_to_unit(&mut self, _u: u64, _t: u64) -> f32 {
            self.distance
        }
        fn unit_height(&mut self, _u: u64) -> f32 {
            self.height
        }
        fn hit_points(&mut self, _u: u64) -> i32 {
            self.hp
        }
        fn undetectable(&mut self, _u: u64, _t: u64) -> bool {
            self.log.push("undetectable".into());
            false
        }
        fn complex_unit(&mut self, u: u64) -> u64 {
            u
        }
    }

    fn world() -> World {
        let mut w = World { class: (0xc1a5, 4, 40), info: (2, 75, 12), made: true, hp: 100, ..Default::default() };
        w.positions.insert(SOURCE, [10.0, 10.0, 0.0]);
        w.positions.insert(TARGET, [20.0, 10.0, 0.0]);
        w
    }

    #[test]
    fn the_cast_steps_place_cast_and_face() {
        let goal = 0xca57_0001u64;
        let mut w = world();
        create(goal, SOURCE, 0, 30, 40, 0x43d, &mut w);
        assert_eq!(w.log, ["ref 1100"]);
        w.log.clear();
        w.command = ACTION_CALAMITY_PRE_CAST;
        step(goal, SOURCE, &mut w);
        assert_eq!(w.log, ["class 1085", "place c1a5 1100 30 40"]);
        w.log.clear();
        w.command = ACTION_CAST_CALAMITY;
        step(goal, SOURCE, &mut w);
        assert_eq!(
            w.log,
            [
                "physics 1100 1",
                "animation 1100 b",
                "physics 1100 6",
                "drop path 1100",
                "class 1085",
                "create 2 40 1100 0 30 40 1100",
                "saved group 1100",
                "power 1100 -75"
            ]
        );
        // not made: no cost; an unknown target kind: no cast, the saved group still reset; no class: nothing more
        w.log.clear();
        w.made = false;
        step(goal, SOURCE, &mut w);
        assert!(!w.log.iter().any(|l| l.starts_with("power")));
        w.log.clear();
        w.info.0 = 7;
        step(goal, SOURCE, &mut w);
        assert_eq!(w.log[5..], ["saved group 1100"]);
        w.log.clear();
        w.class.0 = 0;
        step(goal, SOURCE, &mut w);
        assert_eq!(w.log, ["physics 1100 1", "animation 1100 b", "physics 1100 6", "drop path 1100", "class 1085"]);
        w.log.clear();
        w.command = ACTION_FACE_ENEMY_UNIT;
        step(goal, SOURCE, &mut w);
        assert_eq!(w.log, ["physics 1100 1", "animation 1100 d", "drop path 1100"]);
        destroy(goal, &mut w);
    }

    #[test]
    fn the_triggers() {
        let goal = 0xca57_0002u64;
        let mut w = world();
        create(goal, SOURCE, TARGET, 3, 4, 0x43d, &mut w);
        // still valid: the class's test at the tile
        assert!(evaluate(goal, SOURCE, TRIGGER_CALAMITY_TARGET_STILL_VALID, &mut w));
        // facing: the target unit
        w.log.clear();
        assert!(evaluate(goal, SOURCE, TRIGGER_FACING_ENEMY_UNIT, &mut w));
        assert_eq!(w.log, ["faces 2200"]);
        // in range: the class's range 12 squared against the unit distance
        w.distance = 144.0;
        assert!(evaluate(goal, SOURCE, TRIGGER_UNIT_IN_WEAPON_RANGE, &mut w));
        w.distance = 144.5;
        assert!(!evaluate(goal, SOURCE, TRIGGER_UNIT_IN_WEAPON_RANGE, &mut w));
        // the set adds the weapon range (lrintf: 2.5 -> 2): 14 squared
        w.in_set = true;
        w.stats_range = 2.5;
        w.distance = 196.0;
        assert!(evaluate(goal, SOURCE, TRIGGER_UNIT_IN_WEAPON_RANGE, &mut w));
        // calamity 0x17: the weapon range alone; range 0 (lrintf 0.4): the contact test
        w.in_set = false;
        w.class.1 = 0x17;
        w.stats_range = 0.4;
        w.log.clear();
        assert!(evaluate(goal, SOURCE, TRIGGER_UNIT_IN_WEAPON_RANGE, &mut w));
        assert_eq!(w.log, ["class 1085", "contact"]);
        // destroyed: hit points 0 or off the map; else the cloak test
        w.hp = 0;
        assert!(evaluate(goal, SOURCE, TRIGGER_ENEMY_UNIT_DESTROYED, &mut w));
        w.hp = 5;
        w.log.clear();
        assert!(!evaluate(goal, SOURCE, TRIGGER_ENEMY_UNIT_DESTROYED, &mut w));
        assert_eq!(w.log, ["undetectable"]);
        w.positions.insert(TARGET, [-10000.0, 0.0, 0.0]);
        assert!(evaluate(goal, SOURCE, TRIGGER_ENEMY_UNIT_DESTROYED, &mut w));
        assert!(evaluate(goal, SOURCE, TRIGGER_UNIT_FACING_NOT_REQUIRED, &mut w));
        assert!(!evaluate(goal, SOURCE, 1234, &mut w));
        destroy(goal, &mut w);

        // no target: the point tests; the source's height raises the target location
        let tile = 0xca57_0003u64;
        create(tile, SOURCE, 0, 13, 10, 0x43d, &mut w);
        w.class = (0xc1a5, 4, 40);
        w.info = (2, 75, 3);
        w.height = 0.0;
        // from (10, 10, 0) to the tile's (13.5, 10.5, 2): 12.25 + 0.25 + 4 = 16.5 > 9
        assert!(!evaluate(tile, SOURCE, TRIGGER_UNIT_IN_WEAPON_RANGE, &mut w));
        w.info.2 = 5;
        assert!(evaluate(tile, SOURCE, TRIGGER_UNIT_IN_WEAPON_RANGE, &mut w));
        w.height = 7.0;
        assert_eq!(target_location(tile, &mut w), [13.5, 10.5, 7.0]);
        assert_eq!(goal_location(tile, &mut w), [13.5, 10.5, 2.0]);
        w.log.clear();
        assert!(!evaluate(tile, SOURCE, TRIGGER_FACING_ENEMY_UNIT, &mut w));
        assert_eq!(w.log, ["faces [13.5, 10.5, 2.0]"]);
        assert!(!evaluate(tile, SOURCE, TRIGGER_ENEMY_UNIT_DESTROYED, &mut w));
        // technology 0x77c: at the source
        let at_source = 0xca57_0004u64;
        create(at_source, SOURCE, 0, 13, 10, 0x77c, &mut w);
        w.height = 0.0;
        assert_eq!(target_location(at_source, &mut w), [10.0, 10.0, 0.0]);
        destroy(tile, &mut w);
        destroy(at_source, &mut w);
    }

    #[test]
    fn owner_replace_clone_texts() {
        let goal = 0xca57_0005u64;
        let mut w = world();
        create(goal, SOURCE, TARGET, 3, 4, 0x43d, &mut w);
        w.log.clear();
        set_owner(goal, 0x3300, &mut w);
        assert_eq!(w.log, ["deref 1100", "ref 3300"]);
        w.log.clear();
        replace_unit(goal, 0x9999, 0x4400, &mut w);
        assert!(w.log.is_empty());
        replace_unit(goal, TARGET, 0x4400, &mut w);
        assert_eq!(w.log, ["deref 2200", "ref 4400"]);
        assert_eq!(fields(goal), (0x43d, 3, 4, 0x4400));
        let mut io = EeUnitsGoalIo::default();
        w.log.clear();
        let copy = body(EE_UNITS_GOAL_CLONE, goal, 0, 1, &mut io, &mut w) as u64;
        assert_eq!(w.log, ["new goal 11 true", "ref 3300", "ref 4400"]);
        assert_eq!(fields(copy), (0x43d, 3, 4, 0x4400));
        assert_eq!(checksum(goal, &mut w), 0x33 + 0x44 + 3 + 4 + 0x43d);
        dump(goal, &mut w);
        assert_eq!(
            w.text,
            "OUT OF SYNCH DUMP:  EEUGCastCalamity Source Unit ID #00000033\n\
             OUT OF SYNCH DUMP:  EEUGCastCalamity Target Unit ID #00000044\n\
             OUT OF SYNCH DUMP:  EEUGCastCalamity Tile X: 3\n\
             OUT OF SYNCH DUMP:  EEUGCastCalamity Tile Y: 4\n\
             OUT OF SYNCH DUMP:  EEUGCastCalamity Calamity Type: 1085\n"
        );
        w.text.clear();
        debug_line(goal, &mut w);
        debug_text(goal, &mut w);
        assert_eq!(
            w.text,
            "cc: 51,68,3,4,1085Casting CalamityCast On Unit: class of 4400\nCalamity Location/Type: (3,4) - Type:1085"
        );
        assert_eq!(body(EE_UNITS_GOAL_START_ACTION, goal, 0, 0, &mut io, &mut w), 5001);
        w.log.clear();
        body(EE_UNITS_GOAL_DESTINATION, goal, 0x77, 0, &mut io, &mut w);
        assert_eq!(w.log, ["tell 77 [0.0, 0.0, 0.0]"]);
        // no target: -1 in the texts, the dump's id ffffffff
        let lone = 0xca57_0006u64;
        create(lone, SOURCE, 0, -3, 4, 7, &mut w);
        w.text.clear();
        dump(lone, &mut w);
        debug_line(lone, &mut w);
        assert!(w.text.contains("Target Unit ID #ffffffff\n"));
        assert!(w.text.ends_with("cc: 17,-1,-3,4,7"));
        assert_eq!(checksum(lone, &mut w), 0x11 - 3 + 4 + 7);
        for g in [goal, copy, lone] {
            destroy(g, &mut w);
        }
    }

    #[test]
    fn save_load_repair() {
        let goal = 0xca57_0007u64;
        let mut w = world();
        create(goal, SOURCE, TARGET, -10, 20, 0x43d, &mut w);
        assert_eq!(save(goal, &mut w), 0);
        assert_eq!(w.file, [0x11, 0x22, -10, 20, 0x43d]);
        // a load into an empty goal, then the repair
        let other = 0xca57_0008u64;
        create_empty(other);
        assert_eq!(fields(other), (100000, -10000, -10000, 0));
        w.reads = vec![0x11, 0x22, -10, 20, 0x43d];
        assert_eq!(load(other, &mut w), 0);
        assert_eq!(fields(other), (0x43d, -10, 20, 0));
        w.log.clear();
        assert_eq!(repair(other, &mut w), 0);
        assert_eq!(w.log, ["find 17 true", "ref 1100", "find 34 false", "ref 2200"]);
        assert_eq!(fields(other).3, TARGET);
        // a target id of -1: none; a source gone: 3 and both cleared; a source id of -1: 3
        let third = 0xca57_0009u64;
        create_empty(third);
        w.reads = vec![0x11, 0xffff_ffff, 1, 2, 3];
        load(third, &mut w);
        assert_eq!(repair(third, &mut w), 0);
        assert_eq!(fields(third).3, 0);
        let fourth = 0xca57_000au64;
        create_empty(fourth);
        w.reads = vec![0x55, 0x22, 1, 2, 3];
        load(fourth, &mut w);
        assert_eq!(repair(fourth, &mut w), 3);
        assert_eq!(fields(fourth).3, 0);
        let fifth = 0xca57_000bu64;
        create_empty(fifth);
        w.reads = vec![0xffff_ffff, 0x22, 1, 2, 3];
        load(fifth, &mut w);
        w.log.clear();
        assert_eq!(repair(fifth, &mut w), 3);
        assert!(w.log.is_empty());
        // a short file: the first failing status, the fields read so far kept
        let sixth = 0xca57_000cu64;
        create_empty(sixth);
        w.reads = vec![0x11, 0x22, 9];
        assert_eq!(load(sixth, &mut w), 5);
        assert_eq!(fields(sixth), (100000, 9, -10000, 0));
        for g in [goal, other, third, fourth, fifth, sixth] {
            destroy(g, &mut w);
        }
    }
}
