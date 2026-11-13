//! The unload transport goal whole in Rust (RUST_PLAN R5b.4, the seventeenth round): `EEUGUnloadTransport`'s state
//! (the transport, the tile where it stops, the first cargo tile, the unload tile, the squared unload range) and
//! every body (the constructor's tile search, `Start`, `Step`'s unload, the triggers, `SetTransport`, `ReplaceUnit`,
//! `Suits`, the copy, the checksum, the texts, `Save`, `Load`, `RepairPointers`) by the goal's object. The C++ class
//! is gone: an unload is an `EEUGRust` of kind `kGoalUnloadTransport` (ee/EEUGRust.h) over the Rust goals' host
//! (rust_goal.rs); the transport classes' tile searches and unloading, the shot of an airplane's cargo and the sound
//! stay C++ host operations.
//!
//! Determinism (RUST_PLAN 2.1): `UMSquare` is one `f32` product; the range test is `U2DPoint::DistanceToSquaredF`
//! (`(float)(dx * dx) + (float)(dy * dy)` of 32-bit wrapping products).

use std::collections::BTreeMap;
use std::sync::Mutex;

use crate::rust_goal::{base_condition, hex8, EeUnitsGoalIo, GoalHost, Held};

/// `kActionPrepareToMove`, `kActionUnloadTransport`, `kActionIdle`
const ACTION_PREPARE_TO_MOVE: i64 = 8000;
const ACTION_UNLOAD_TRANSPORT: i64 = 12001;
const ACTION_IDLE: i64 = 5000;
/// the triggers the goal answers
const TRIGGER_GOAL_IS_UNLOAD_TRANSPORT: i64 = 12001;
const TRIGGER_CONTAINER_IN_RANGE: i64 = 12002;
/// `kUnitTypeTeleporter`, `kUnitCategoryLandCitizen`, `kUnitCategoryNavalCitizen`
const UNIT_TYPE_TELEPORTER: i64 = 0x313;
const CATEGORY_LAND_CITIZEN: i64 = 6;
const CATEGORY_NAVAL_CITIZEN: i64 = 0x16;
/// a tile off the map
const TILE_NONE: i32 = -10000;
/// the save version up to which the first cargo and stopping tiles are not saved (0.218)
const VERSION_TILES: u32 = 0xda;
/// the transport's classes (`EEUnitClassCast<EEUCTransport>`, `<EEUCAirplane>`)
pub const TRANSPORT_CLASS: i64 = 1;
pub const AIRPLANE_CLASS: i64 = 2;

type Tile = (i32, i32);

/// One unload goal's state.
#[derive(Clone, Copy, Debug, PartialEq)]
struct Unload {
    transport: Held,
    unload: Tile,
    first: Tile,
    destination: Tile,
    range: f32,
}

const EMPTY: Unload = Unload { transport: Held::None, unload: (0, 0), first: (0, 0), destination: (0, 0), range: 0.0 };

static UNLOADS: Mutex<BTreeMap<u64, Unload>> = Mutex::new(BTreeMap::new());

fn get(goal: u64) -> Unload {
    let goals = UNLOADS.lock().unwrap_or_else(|e| e.into_inner());
    goals.get(&goal).copied().unwrap_or(EMPTY)
}

fn put(goal: u64, state: Unload) {
    let mut goals = UNLOADS.lock().unwrap_or_else(|e| e.into_inner());
    goals.insert(goal, state);
}

/// The transport classes' calls (the FFI adapter; the tests' mock).
pub trait UnloadHost: GoalHost {
    /// the unit's class's `mUnloadRange`, `mUnitType`; its `GetNumCargoUnits()`; the first cargo's class category
    fn unload_range(&mut self, unit: u64) -> f32;
    fn class_type(&mut self, unit: u64) -> i64;
    fn cargo_count(&mut self, unit: u64) -> u32;
    fn cargo_category(&mut self, unit: u64) -> i64;
    /// which of `EEUCTransport` (`TRANSPORT_CLASS`) and `EEUCAirplane` (`AIRPLANE_CLASS`) the unit's class is
    fn transport_kind(&mut self, unit: u64) -> i64;
    /// the transport class's `CanUnloadAt(unit, x, y)`, else `EEIsTileFree(unit, x, y)`
    fn can_unload_at(&mut self, unit: u64, tile: Tile) -> bool;
    /// the transport's (else the airplane's) class's `FindUnloadTiles(unit, &destination, &unload, &first, &tile)`
    /// (each tile in and out)
    fn find_unload_tiles(&mut self, unit: u64, tiles: &mut [Tile; 4]);
    /// the transport class's `CanUnloadNear(unit, its cargo 0, &unload, &first, &destination)` (each tile in and out)
    fn can_unload_near(&mut self, unit: u64, tiles: &mut [Tile; 3]) -> bool;
    /// the unit's player's `IsHuman()`; whether two units' players are one
    fn human(&mut self, unit: u64) -> bool;
    fn same_player(&mut self, unit: u64, other: u64) -> bool;
    /// `mHitPoints`; `StartAction(a)` alone (no `ActionChanged`)
    fn hit_points(&mut self, unit: u64) -> i32;
    fn start_action_only(&mut self, unit: u64, action: i64);
    /// the airplane class's `FireShot(unit)`, the transport class's `UnloadAt(unit, x, y)`
    fn fire_cargo(&mut self, unit: u64);
    fn unload_at(&mut self, unit: u64, tile: Tile);
    /// the unload sound at the unit (the space sound when `space`)
    fn unload_sound(&mut self, unit: u64, space: bool);
    /// the unit's `mComplexUnit`
    fn complex_unit(&mut self, unit: u64) -> u64;
}

fn range_of(unit: u64, host: &mut dyn UnloadHost) -> f32 {
    let r = host.unload_range(unit);
    r * r
}

/// Whether the transport can unload at the unload tile.
fn can_unload_at_destination(u: &Unload, host: &mut dyn UnloadHost) -> bool {
    host.can_unload_at(u.transport.object(), u.destination)
}

/// `FindUnloadTiles`: when the transport can unload there, its class finds the stopping tile, the first cargo tile
/// and the final unload tile.
fn find_unload_tiles(u: &mut Unload, host: &mut dyn UnloadHost) {
    if !can_unload_at_destination(u, host) {
        return;
    }
    let mut tiles = [u.destination, u.unload, u.first, (0, 0)];
    host.find_unload_tiles(u.transport.object(), &mut tiles);
    (u.destination, u.unload, u.first) = (tiles[0], tiles[1], tiles[2]);
    u.destination = tiles[3];
}

/// `EEUGUnloadTransport(transport, x, y, arg)`: the transport referenced, its range; the unload tiles found when it
/// carries cargo.
pub fn create(goal: u64, transport: u64, x: i32, y: i32, host: &mut dyn UnloadHost) {
    host.reference(transport, false);
    let mut u = Unload { transport: Held::of(transport), destination: (x, y), ..EMPTY };
    u.range = range_of(transport, host);
    if host.cargo_count(transport) > 0 {
        find_unload_tiles(&mut u, host);
    }
    put(goal, u);
}

/// `EEUGUnloadTransport(inArg)`: no transport, every tile (0, 0), range 0.
pub fn create_empty(goal: u64) {
    put(goal, EMPTY);
}

/// The destructor: the transport released.
pub fn destroy(goal: u64, host: &mut dyn UnloadHost) {
    let Some(u) = UNLOADS.lock().unwrap_or_else(|e| e.into_inner()).remove(&goal) else { return };
    if let Held::Object(t) = u.transport {
        host.dereference(t, false);
    }
}

/// The transport (0: none), the unload tile.
pub fn fields(goal: u64) -> (u64, Tile) {
    let u = get(goal);
    (u.transport.object(), u.destination)
}

/// Whether the transport is within unload range of the unload tile (a teleporter always is).
fn in_unload_range(u: &Unload, host: &mut dyn UnloadHost) -> bool {
    let Held::Object(t) = u.transport else { return false };
    if host.class_type(t) == UNIT_TYPE_TELEPORTER {
        return true;
    }
    let (x, y) = host.unit_tile(t);
    let dx = (x as i32).wrapping_sub(u.destination.0);
    let dy = (y as i32).wrapping_sub(u.destination.1);
    (dx.wrapping_mul(dx) as f32) + (dy.wrapping_mul(dy) as f32) <= u.range
}

/// `Start`: a transport with cargo and an unload tile set starts moving; a computer player's transport carrying
/// citizens first finds its unload tiles again.
pub fn start(goal: u64, unit: u64, host: &mut dyn UnloadHost) {
    let mut u = get(goal);
    if u.destination.0 == TILE_NONE || u.destination.1 == TILE_NONE || u.transport.object() != unit {
        return;
    }
    if host.cargo_count(unit) == 0 {
        return;
    }
    let t = u.transport.object();
    if !host.human(t) {
        let category = host.cargo_category(t);
        if category == CATEGORY_LAND_CITIZEN || category == CATEGORY_NAVAL_CITIZEN {
            find_unload_tiles(&mut u, host);
            u.range = range_of(t, host);
            put(goal, u);
        }
    }
    host.start_action(unit, ACTION_PREPARE_TO_MOVE);
}

/// `Unload`: the cargo dropped (an airplane fires it, a transport unloads at the tile); the unload sound when any
/// cargo left.
fn unload(u: &Unload, host: &mut dyn UnloadHost) {
    let t = u.transport.object();
    let count = host.cargo_count(t);
    let kind = host.transport_kind(t);
    if kind & AIRPLANE_CLASS != 0 {
        host.fire_cargo(t);
    } else {
        host.unload_at(t, u.destination);
    }
    if count == host.cargo_count(t) {
        return;
    }
    let space = kind & TRANSPORT_CLASS != 0 && host.class_type(t) == UNIT_TYPE_TELEPORTER;
    host.unload_sound(t, space);
}

/// `Step`: the unload on the unload-transport action.
pub fn step(goal: u64, unit: u64, host: &mut dyn UnloadHost) {
    if host.unit_command(unit) == ACTION_UNLOAD_TRANSPORT {
        unload(&get(goal), host);
    }
}

/// `EvaluateCondition`: the goal is an unload; the transport in range with cargo and, for a transport class that
/// is not a teleporter, a place to unload near the tile.
pub fn evaluate(goal: u64, unit: u64, event: i64, host: &mut dyn UnloadHost) -> bool {
    let mut u = get(goal);
    match event {
        TRIGGER_GOAL_IS_UNLOAD_TRANSPORT => true,
        TRIGGER_CONTAINER_IN_RANGE => {
            // (the original read the transport's class unguarded: a goal whose transport was not found on load)
            let Held::Object(t) = u.transport else { return false };
            let in_range = in_unload_range(&u, host) && host.cargo_count(t) > 0;
            if host.transport_kind(t) & TRANSPORT_CLASS != 0 && host.class_type(t) != UNIT_TYPE_TELEPORTER {
                if !in_range {
                    return false;
                }
                let mut tiles = [u.unload, u.first, u.destination];
                let near = host.can_unload_near(t, &mut tiles);
                (u.unload, u.first, u.destination) = (tiles[0], tiles[1], tiles[2]);
                put(goal, u);
                return near;
            }
            let _ = unit;
            in_range
        }
        _ => base_condition(event),
    }
}

/// `SetTransport`: another transport, the reference moved, its range taken.
pub fn set_transport(goal: u64, transport: u64, host: &mut dyn UnloadHost) {
    let mut u = get(goal);
    if let Held::Object(old) = u.transport {
        host.dereference(old, false);
    }
    u.transport = Held::of(transport);
    u.range = range_of(transport, host);
    put(goal, u);
    host.reference(transport, false);
}

/// `ReplaceUnit`: a replaced transport of the same player is followed (the reference moved, its range taken); one
/// that changed player and is alive goes idle.
pub fn replace_unit(goal: u64, old: u64, new: u64, host: &mut dyn UnloadHost) {
    let mut u = get(goal);
    if u.transport.object() != old || old == 0 {
        return;
    }
    if host.same_player(new, old) {
        host.dereference(old, false);
        u.transport = Held::of(new);
        u.range = range_of(new, host);
        put(goal, u);
        host.reference(new, false);
    } else if host.hit_points(old) > 0 {
        host.start_action_only(old, ACTION_IDLE);
    }
}

/// The checksum's part: the transport's id, the three tiles, the range's bits.
pub fn checksum(goal: u64, host: &mut dyn UnloadHost) -> i64 {
    let u = get(goal);
    let id = match u.transport {
        Held::Object(t) => host.unit_id(t),
        _ => 0,
    };
    let sum = [u.destination.1, u.destination.0, u.first.1, u.first.0, u.unload.1, u.unload.0]
        .iter()
        .fold(id, |s, v| s.wrapping_add(*v as u32))
        .wrapping_add(u.range.to_bits());
    i64::from(sum as i32)
}

/// The out-of-synch dump.
pub fn dump(goal: u64, host: &mut dyn UnloadHost) {
    let u = get(goal);
    let id = match u.transport {
        Held::Object(t) => host.unit_id(t),
        _ => u32::MAX,
    };
    host.append(&format!("OUT OF SYNCH DUMP:  EEUGUnloadTransport - Transport ID #{}\n", hex8(id)));
    host.append(&format!(
        "OUT OF SYNCH DUMP:  EEUGUnloadTransport - Transport Destination ({}, {})\n",
        u.unload.0, u.unload.1
    ));
    host.append(&format!(
        "OUT OF SYNCH DUMP:  EEUGUnloadTransport - First Cargo Tile ({}, {})\n",
        u.first.0, u.first.1
    ));
    host.append(&format!(
        "OUT OF SYNCH DUMP:  EEUGUnloadTransport - Unload Tile ({}, {})\n",
        u.destination.0, u.destination.1
    ));
    host.append(&format!(
        "OUT OF SYNCH DUMP:  EEUGUnloadTransport - Unload Range Squared {:6.6}\n",
        f64::from(u.range)
    ));
}

/// `GetDebugLine`: "unloadtransport (transport id ..., pointx ... pointy ...):".
pub fn debug_line(goal: u64, host: &mut dyn UnloadHost) {
    let u = get(goal);
    let id = match u.transport {
        Held::Object(t) => host.unit_id(t),
        _ => u32::MAX,
    } & 0xff_ffff;
    host.append("unloadtransport");
    host.append(&format!(" (transport id {id}, pointx {} pointy {}):", u.destination.0, u.destination.1));
}

/// `GetDebugText`: "UnLoad Transport", the transport's class name and the unload tile.
pub fn debug_text(goal: u64, host: &mut dyn UnloadHost) {
    let u = get(goal);
    host.append("UnLoad Transport\n Transport:");
    if let Held::Object(t) = u.transport {
        host.append_class_name(t);
    }
    host.append(&format!("  at({},{}):", u.destination.0, u.destination.1));
}

/// `U2DPoint<long>::Save`
fn save_tile(t: Tile, host: &mut dyn UnloadHost) -> i32 {
    if !host.file_open() {
        return 4;
    }
    let result = host.write_i32(t.0);
    if result != 0 {
        return result;
    }
    host.write_i32(t.1)
}

/// `U2DPoint<long>::Load` (the fields read so far kept)
fn load_tile(t: &mut Tile, host: &mut dyn UnloadHost) -> i32 {
    if !host.file_open() {
        return 4;
    }
    let result = host.read_i32(&mut t.0);
    if result != 0 {
        return result;
    }
    host.read_i32(&mut t.1)
}

/// `Save`'s part after the base goal's: the transport's id (-1: none), the unload tile, the first cargo tile, the
/// stopping tile; the first failing status.
pub fn save(goal: u64, host: &mut dyn UnloadHost) -> i32 {
    let u = get(goal);
    let id = match u.transport {
        Held::Object(t) => host.unit_id(t),
        _ => u32::MAX,
    };
    let mut result = host.write_u32(id);
    if result == 0 {
        result = host.write_i32(u.destination.0);
        if result == 0 {
            result = host.write_i32(u.destination.1);
            if result == 0 {
                result = save_tile(u.first, host);
            }
            if result == 0 {
                result = save_tile(u.unload, host);
            }
        }
    }
    result
}

/// `Load`'s part after the base goal's: the transport's id (resolved by `repair`), the unload tile and, after
/// version 0.218, the first cargo and stopping tiles; the first failing status.
pub fn load(goal: u64, major: u32, minor: u32, host: &mut dyn UnloadHost) -> i32 {
    let mut u = get(goal);
    let mut id = u32::MAX;
    let mut result = host.read_u32(&mut id);
    u.transport = Held::Saved(id);
    if result == 0 {
        result = host.read_i32(&mut u.destination.0);
        if result == 0 {
            result = host.read_i32(&mut u.destination.1);
        }
    }
    if !(major == 0 && minor <= VERSION_TILES) && result == 0 {
        result = load_tile(&mut u.first, host);
        if result == 0 {
            result = load_tile(&mut u.unload, host);
        }
    }
    put(goal, u);
    result
}

/// `RepairPointers`: the transport from its id (the unit's complex unit, referenced) and its range; saves up to
/// 0.218 take the unload tile for all three tiles; 0.
pub fn repair(goal: u64, major: u32, minor: u32, host: &mut dyn UnloadHost) -> i32 {
    let mut u = get(goal);
    let unit = host.find_unit(u.transport.saved_id(), false);
    if unit != 0 {
        let t = host.complex_unit(unit);
        u.transport = Held::of(t);
        host.reference(t, false);
        u.range = range_of(t, host);
        if major == 0 && minor <= VERSION_TILES {
            u.first = u.destination;
            u.unload = u.destination;
        }
    } else {
        u.transport = Held::None;
    }
    put(goal, u);
    0
}

/// The goal's body `op` (rust_goal.rs `EE_UNITS_GOAL_*`).
pub fn body(op: i32, goal: u64, a: u64, b: u64, io: &mut EeUnitsGoalIo, host: &mut dyn UnloadHost) -> i64 {
    use crate::rust_goal::*;
    match op {
        EE_UNITS_GOAL_NEW_UNLOAD => create(goal, a, io.i[0] as i32, io.i[1] as i32, host),
        EE_UNITS_GOAL_UNLOAD_NEW_EMPTY => create_empty(goal),
        EE_UNITS_GOAL_DESTROY => destroy(goal, host),
        EE_UNITS_GOAL_CLONE => {
            let u = get(goal);
            let copy = host.new_goal(GOAL_UNLOAD_TRANSPORT, b != 0);
            create(copy, u.transport.object(), u.destination.0, u.destination.1, host);
            return copy as i64;
        }
        EE_UNITS_GOAL_CHECKSUM => return checksum(goal, host),
        EE_UNITS_GOAL_DUMP => dump(goal, host),
        EE_UNITS_GOAL_DEBUG_LINE => debug_line(goal, host),
        EE_UNITS_GOAL_DEBUG_TEXT => debug_text(goal, host),
        EE_UNITS_GOAL_LOCATION | EE_UNITS_GOAL_DESTINATION => {
            let u = get(goal);
            // the destination is where the transport stops (no group is told), the location the unload tile
            let (x, y) = if op == EE_UNITS_GOAL_DESTINATION { u.unload } else { u.destination };
            let p = host.tile_location(i64::from(x), i64::from(y));
            io.f[..3].copy_from_slice(&p);
        }
        EE_UNITS_GOAL_START_ACTION => return ACTION_PREPARE_TO_MOVE,
        EE_UNITS_GOAL_MAX_PATH_ATTEMPTS => return 9,
        EE_UNITS_GOAL_STEP => step(goal, a, host),
        EE_UNITS_GOAL_START => start(goal, a, host),
        EE_UNITS_GOAL_SAVE => return i64::from(save(goal, host)),
        EE_UNITS_GOAL_LOAD => return i64::from(load(goal, a as u32, b as u32, host)),
        EE_UNITS_GOAL_REPAIR => return i64::from(repair(goal, a as u32, b as u32, host)),
        EE_UNITS_GOAL_EVALUATE => return i64::from(evaluate(goal, a, b as i64, host)),
        EE_UNITS_GOAL_REPLACE_UNIT => replace_unit(goal, a, b, host),
        EE_UNITS_GOAL_SUITS => return i64::from(get(goal).transport.object() != 0 && a != 0),
        EE_UNITS_GOAL_UNLOAD_SET_TRANSPORT => set_transport(goal, a, host),
        EE_UNITS_GOAL_UNLOAD_CAN_UNLOAD => return i64::from(can_unload_at_destination(&get(goal), host)),
        EE_UNITS_GOAL_UNLOAD_FORGET_TRANSPORT => {
            let mut u = get(goal);
            u.transport = Held::None;
            put(goal, u);
        }
        EE_UNITS_GOAL_UNLOAD_SET_DESTINATION => {
            let mut u = get(goal);
            u.destination = (io.i[0] as i32, io.i[1] as i32);
            find_unload_tiles(&mut u, host);
            put(goal, u);
        }
        EE_UNITS_GOAL_UNLOAD_FIELDS => {
            let (t, (x, y)) = fields(goal);
            io.i[0] = i64::from(x);
            io.i[1] = i64::from(y);
            return t as i64;
        }
        // SetOwner: the original's is empty
        _ => {}
    }
    0
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::rust_goal::*;

    /// A transport (its tile, cargo, class) and a log of the world calls.
    #[derive(Default)]
    struct World {
        log: Vec<String>,
        text: String,
        command: i64,
        file: Vec<i64>,
        reads: Vec<i64>,
        tile: (i64, i64),
        cargo: u32,
        /// the cargo after an unload
        cargo_after: u32,
        category: i64,
        kind: i64,
        class_type: i64,
        range: f32,
        free: bool,
        near: bool,
        human: bool,
        same_player: bool,
        hp: i32,
    }
    const T: u64 = 0x7700;
    const OTHER: u64 = 0x8800;

    impl GoalHost for World {
        fn reference(&mut self, o: u64, _g: bool) {
            self.log.push(format!("ref {o:x}"));
        }
        fn dereference(&mut self, o: u64, _g: bool) {
            self.log.push(format!("deref {o:x}"));
        }
        fn unit_id(&mut self, u: u64) -> u32 {
            (u >> 8) as u32 | 0x0100_0000
        }
        fn unit_tile(&mut self, _u: u64) -> (i64, i64) {
            self.tile
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
            self.log.push(format!("find {id:x} {complex}"));
            if id == u32::MAX {
                0
            } else {
                u64::from(id) << 8
            }
        }
        fn unit_position(&mut self, _u: u64) -> [f32; 3] {
            [0.0; 3]
        }
        fn tile_location(&mut self, x: i64, y: i64) -> [f32; 3] {
            [x as f32, y as f32, 3.0]
        }
        fn tell_group(&mut self, _u: u64, _p: [f32; 3]) {
            self.log.push("tell".into());
        }
        fn set_physics_action(&mut self, _u: u64, _a: i64) {}
        fn set_animation(&mut self, _u: u64, _a: i64) {}
        fn drop_path(&mut self, _u: u64) {}
        fn new_goal(&mut self, k: u8, a: bool) -> u64 {
            self.log.push(format!("new goal {k} {a}"));
            0x0a10_0f00
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

    impl UnloadHost for World {
        fn unload_range(&mut self, _u: u64) -> f32 {
            self.range
        }
        fn class_type(&mut self, _u: u64) -> i64 {
            self.class_type
        }
        fn cargo_count(&mut self, _u: u64) -> u32 {
            self.cargo
        }
        fn cargo_category(&mut self, _u: u64) -> i64 {
            self.category
        }
        fn transport_kind(&mut self, _u: u64) -> i64 {
            self.kind
        }
        fn can_unload_at(&mut self, _u: u64, t: Tile) -> bool {
            self.log.push(format!("can unload at {t:?}"));
            self.free
        }
        fn find_unload_tiles(&mut self, _u: u64, tiles: &mut [Tile; 4]) {
            self.log.push(format!("find tiles {tiles:?}"));
            tiles[1] = (tiles[0].0 - 1, tiles[0].1);
            tiles[2] = (tiles[0].0 - 2, tiles[0].1);
            tiles[3] = (tiles[0].0 + 1, tiles[0].1);
        }
        fn can_unload_near(&mut self, _u: u64, tiles: &mut [Tile; 3]) -> bool {
            self.log.push(format!("near {tiles:?}"));
            tiles[2].1 += 1;
            self.near
        }
        fn human(&mut self, _u: u64) -> bool {
            self.human
        }
        fn same_player(&mut self, _u: u64, _o: u64) -> bool {
            self.same_player
        }
        fn hit_points(&mut self, _u: u64) -> i32 {
            self.hp
        }
        fn start_action_only(&mut self, u: u64, a: i64) {
            self.log.push(format!("start only {u:x} {a}"));
        }
        fn fire_cargo(&mut self, u: u64) {
            self.log.push(format!("fire {u:x}"));
            self.cargo = self.cargo_after;
        }
        fn unload_at(&mut self, u: u64, t: Tile) {
            self.log.push(format!("unload {u:x} {t:?}"));
            self.cargo = self.cargo_after;
        }
        fn unload_sound(&mut self, u: u64, space: bool) {
            self.log.push(format!("sound {u:x} {space}"));
        }
        fn complex_unit(&mut self, u: u64) -> u64 {
            u
        }
    }

    fn world() -> World {
        World { range: 3.0, cargo: 2, cargo_after: 0, kind: TRANSPORT_CLASS, free: true, hp: 1, ..Default::default() }
    }

    #[test]
    fn creation_finds_the_tiles_and_the_unload_drops_the_cargo() {
        let goal = 0x0a10_0001u64;
        let mut w = world();
        create(goal, T, 10, 20, &mut w);
        assert_eq!(w.log, ["ref 7700", "can unload at (10, 20)", "find tiles [(10, 20), (0, 0), (0, 0), (0, 0)]"]);
        let u = get(goal);
        assert_eq!((u.destination, u.unload, u.first, u.range), ((11, 20), (9, 20), (8, 20), 9.0));
        // no cargo: no search; a tile it cannot unload at: no search either
        let bare = 0x0a10_0002u64;
        w.cargo = 0;
        w.log.clear();
        create(bare, T, 10, 20, &mut w);
        assert_eq!(w.log, ["ref 7700"]);
        // the unload: a transport unloads at the tile, the sound when cargo left (the space sound for a teleporter)
        w.cargo = 2;
        w.command = ACTION_UNLOAD_TRANSPORT;
        w.log.clear();
        step(goal, T, &mut w);
        assert_eq!(w.log, ["unload 7700 (11, 20)", "sound 7700 false"]);
        w.cargo = 2;
        w.class_type = UNIT_TYPE_TELEPORTER;
        w.log.clear();
        step(goal, T, &mut w);
        assert_eq!(w.log, ["unload 7700 (11, 20)", "sound 7700 true"]);
        // an airplane fires its cargo; nothing left the transport: no sound
        w.kind = AIRPLANE_CLASS;
        w.cargo = 2;
        w.cargo_after = 2;
        w.log.clear();
        step(goal, T, &mut w);
        assert_eq!(w.log, ["fire 7700"]);
        // another action: nothing
        w.command = 1;
        w.log.clear();
        step(goal, T, &mut w);
        assert!(w.log.is_empty());
        destroy(goal, &mut w);
        destroy(bare, &mut w);
    }

    #[test]
    fn the_triggers_and_the_start() {
        let goal = 0x0a10_0003u64;
        let mut w = world();
        w.cargo = 0;
        create(goal, T, 10, 20, &mut w);
        assert!(evaluate(goal, T, TRIGGER_GOAL_IS_UNLOAD_TRANSPORT, &mut w));
        // in range: (dx 1)^2 + (dy 1)^2 = 2 <= 9, with cargo, a place near: the tiles updated
        w.tile = (11, 21);
        w.cargo = 1;
        w.near = true;
        w.log.clear();
        assert!(evaluate(goal, T, TRIGGER_CONTAINER_IN_RANGE, &mut w));
        assert_eq!(w.log, ["near [(0, 0), (0, 0), (10, 20)]"]);
        assert_eq!(fields(goal).1, (10, 21));
        // out of range (3^2 + 0 = 9 <= 9 is in; 4^2 is not): false without asking the class
        w.tile = (14, 21);
        w.log.clear();
        assert!(!evaluate(goal, T, TRIGGER_CONTAINER_IN_RANGE, &mut w));
        assert!(w.log.is_empty());
        // a teleporter is always in range and asks nothing; no cargo: false
        w.class_type = UNIT_TYPE_TELEPORTER;
        assert!(evaluate(goal, T, TRIGGER_CONTAINER_IN_RANGE, &mut w));
        w.cargo = 0;
        assert!(!evaluate(goal, T, TRIGGER_CONTAINER_IN_RANGE, &mut w));
        assert!(evaluate(goal, T, TRIGGER_UNIT_FACING_NOT_REQUIRED, &mut w));
        // the start: a human's transport with cargo moves; a computer's carrying citizens searches the tiles again
        w.cargo = 1;
        w.human = true;
        w.log.clear();
        start(goal, T, &mut w);
        assert_eq!(w.log, ["start 7700 8000"]);
        w.human = false;
        w.category = CATEGORY_LAND_CITIZEN;
        w.log.clear();
        start(goal, T, &mut w);
        assert_eq!(
            w.log,
            ["can unload at (10, 21)", "find tiles [(10, 21), (0, 0), (0, 0), (0, 0)]", "start 7700 8000"]
        );
        // another unit, no cargo or no tile: nothing
        w.log.clear();
        start(goal, OTHER, &mut w);
        w.cargo = 0;
        start(goal, T, &mut w);
        assert!(w.log.is_empty());
        destroy(goal, &mut w);
    }

    #[test]
    fn transport_changes_copy_texts() {
        let goal = 0x0a10_0004u64;
        let mut w = world();
        w.cargo = 0;
        create(goal, T, -4, 5, &mut w);
        w.log.clear();
        w.range = 2.0;
        set_transport(goal, OTHER, &mut w);
        assert_eq!(w.log, ["deref 7700", "ref 8800"]);
        assert_eq!(get(goal).range, 4.0);
        // a replaced transport of the same player is followed; of another, alive, it idles
        w.log.clear();
        replace_unit(goal, T, 0x9900, &mut w);
        assert!(w.log.is_empty());
        w.same_player = true;
        replace_unit(goal, OTHER, T, &mut w);
        assert_eq!(w.log, ["deref 8800", "ref 7700"]);
        w.same_player = false;
        w.log.clear();
        replace_unit(goal, T, OTHER, &mut w);
        assert_eq!(w.log, ["start only 7700 5000"]);
        w.hp = 0;
        w.log.clear();
        replace_unit(goal, T, OTHER, &mut w);
        assert!(w.log.is_empty());
        let mut io = EeUnitsGoalIo::default();
        w.log.clear();
        let copy = body(EE_UNITS_GOAL_CLONE, goal, 0, 1, &mut io, &mut w) as u64;
        assert_eq!(w.log, ["new goal 8 true", "ref 7700"]);
        assert_eq!(fields(copy), (T, (-4, 5)));
        // forgotten: no release at the destructor
        let lone = 0x0a10_0009u64;
        create(lone, T, 1, 1, &mut w);
        body(EE_UNITS_GOAL_UNLOAD_FORGET_TRANSPORT, lone, 0, 0, &mut io, &mut w);
        w.log.clear();
        destroy(lone, &mut w);
        assert!(w.log.is_empty());
        // a new unload tile: the tiles searched for it
        w.log.clear();
        io.i[..2].copy_from_slice(&[6, 7]);
        body(EE_UNITS_GOAL_UNLOAD_SET_DESTINATION, copy, 0, 0, &mut io, &mut w);
        assert_eq!(w.log, ["can unload at (6, 7)", "find tiles [(6, 7), (0, 0), (0, 0), (0, 0)]"]);
        assert_eq!(fields(copy).1, (7, 7));
        assert_eq!(body(EE_UNITS_GOAL_SUITS, goal, 0x5, 0, &mut io, &mut w), 1);
        assert_eq!(body(EE_UNITS_GOAL_SUITS, goal, 0, 0, &mut io, &mut w), 0);
        assert_eq!(body(EE_UNITS_GOAL_MAX_PATH_ATTEMPTS, goal, 0, 0, &mut io, &mut w), 9);
        assert_eq!(body(EE_UNITS_GOAL_START_ACTION, goal, 0, 0, &mut io, &mut w), 8000);
        // the destination: the stopping tile, no group told; the location: the unload tile
        w.log.clear();
        body(EE_UNITS_GOAL_DESTINATION, goal, T, 0, &mut io, &mut w);
        assert_eq!(io.f[..3], [0.0, 0.0, 3.0]);
        body(EE_UNITS_GOAL_LOCATION, goal, 0, 0, &mut io, &mut w);
        assert_eq!(io.f[..3], [-4.0, 5.0, 3.0]);
        assert!(w.log.is_empty());
        // the checksum: id + the tiles + the range's bits
        assert_eq!(checksum(goal, &mut w), i64::from((0x0100_0077u32 - 4 + 5).wrapping_add(4.0f32.to_bits()) as i32));
        dump(goal, &mut w);
        assert_eq!(
            w.text,
            "OUT OF SYNCH DUMP:  EEUGUnloadTransport - Transport ID #01000077\n\
             OUT OF SYNCH DUMP:  EEUGUnloadTransport - Transport Destination (0, 0)\n\
             OUT OF SYNCH DUMP:  EEUGUnloadTransport - First Cargo Tile (0, 0)\n\
             OUT OF SYNCH DUMP:  EEUGUnloadTransport - Unload Tile (-4, 5)\n\
             OUT OF SYNCH DUMP:  EEUGUnloadTransport - Unload Range Squared 4.000000\n"
        );
        w.text.clear();
        debug_line(goal, &mut w);
        debug_text(goal, &mut w);
        assert_eq!(
            w.text,
            "unloadtransport (transport id 119, pointx -4 pointy 5):UnLoad Transport\n Transport:class of 7700  at(-4,5):"
        );
        destroy(goal, &mut w);
        destroy(copy, &mut w);
    }

    #[test]
    fn save_load_repair_by_version() {
        let goal = 0x0a10_0005u64;
        let mut w = world();
        create(goal, T, 10, 20, &mut w);
        assert_eq!(save(goal, &mut w), 0);
        assert_eq!(w.file, [0x0100_0077, 11, 20, 8, 20, 9, 20]);
        let other = 0x0a10_0006u64;
        create_empty(other);
        w.reads = w.file.clone();
        assert_eq!(load(other, 0, 0x10c, &mut w), 0);
        w.log.clear();
        w.range = 2.5;
        assert_eq!(repair(other, 0, 0x10c, &mut w), 0);
        assert_eq!(w.log, ["find 1000077 false", "ref 100007700"]);
        let u = get(other);
        assert_eq!((u.destination, u.first, u.unload, u.range), ((11, 20), (8, 20), (9, 20), 6.25));
        // an old save (0.218): no tiles read; the repair takes the unload tile for all three
        let old = 0x0a10_0007u64;
        create_empty(old);
        w.reads = vec![0x77, 3, 4, 99];
        assert_eq!(load(old, 0, 0xda, &mut w), 0);
        assert_eq!(w.reads, [99]);
        repair(old, 0, 0xda, &mut w);
        let u = get(old);
        assert_eq!((u.destination, u.first, u.unload), ((3, 4), (3, 4), (3, 4)));
        // a transport gone: none, 0; a short file: the failing status
        let gone = 0x0a10_0008u64;
        create_empty(gone);
        w.reads = vec![0xffff_ffff, 1, 2, 3, 4, 5, 6];
        load(gone, 0, 0x10c, &mut w);
        assert_eq!(repair(gone, 0, 0x10c, &mut w), 0);
        assert_eq!(fields(gone).0, 0);
        w.reads = vec![0x77, 1];
        assert_eq!(load(gone, 0, 0x10c, &mut w), 5);
        // no transport: the id saved as -1
        w.file.clear();
        create_empty(gone);
        save(gone, &mut w);
        assert_eq!(w.file[0], 0xffff_ffff);
        for g in [goal, other, old, gone] {
            destroy(g, &mut w);
        }
    }
}
