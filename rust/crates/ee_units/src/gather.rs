//! The gather goal whole in Rust (RUST_PLAN R5b.4, the nineteenth round): `EEUGGather`'s state (the waiting area,
//! the destination, the resource and the storage, the gathering and waiting flags) and every body (the constructor's
//! destination, `GetDestination`'s searches, `Step` and its command handlers, the triggers, `PathTaken`, the copies,
//! the checksum, the texts, `Save`, `Load`, `RepairPointers`, `ReplaceUnit`) by the goal's object. The C++ class is
//! gone: a gather goal is an `EEUGRust` of kind `kGoalGather` (ee/EEUGRust.h) over the Rust goals' host
//! (rust_goal.rs). The world's own work stays C++ host operations: the resource spot, free tile, farm tile, gather
//! spot and gather room searches, the citizen class's resource and storage searches, the gathering itself (`Work`),
//! the gather animations and effects, the hunt, the next tree of a forest, the settlements to build, the naval search,
//! the reach tests, the path length.
//!
//! Reentrancy: a host call may come back into this goal (the settlements' search sets the storage through the goal's
//! accessor), so every body reads and writes the record around its host calls instead of holding a copy across them.

use std::collections::BTreeMap;
use std::sync::Mutex;

use ee_ge::types::Ge3DPoint;

use crate::build::BuildHost;
use crate::load::um_floor;
use crate::rust_goal::{base_condition, hex8, EeUnitsGoalIo, Held};

/// `kGoalGather`
pub const GOAL_GATHER: u8 = 6;

const ACTION_ADD_RESOURCE_TO_STORAGE: i64 = 4005;
const ACTION_FIND_NEW_RESOURCE: i64 = 4006;
const ACTION_FIND_NEW_STORAGE: i64 = 4007;
const ACTION_FIND_STORAGE: i64 = 4021;
const ACTION_GATHER_RESOURCE: i64 = 4004;
const ACTION_MOVE_TO_WAITING_AREA: i64 = 4023;
const ACTION_NOTIFY_WAITING_CITIZEN: i64 = 4024;
const ACTION_WAIT_FOR_OPEN_POSITION: i64 = 4026;
const ACTION_FIND_FARM: i64 = 4029;
const ACTION_IDLE: i64 = 5000;
const ACTION_MOVE_TO_RESOURCE: i64 = 8008;
const ACTION_MOVE_TO_STORAGE: i64 = 8009;

const TRIGGER_BLOCKED_BY_WALL: i64 = 2042;
const TRIGGER_RESOURCE_CAPACITY_FILLED: i64 = 4003;
const TRIGGER_RESOURCE_CAPACITY_EMPTY: i64 = 4005;
const TRIGGER_NEW_RESOURCE_FOUND: i64 = 4006;
const TRIGGER_RESOURCE_DEPLETED: i64 = 4008;
const TRIGGER_RESOURCE_IN_RANGE: i64 = 4009;
const TRIGGER_STORAGE_IN_RANGE: i64 = 4010;
const TRIGGER_STORAGE_FOUND: i64 = 4011;
const TRIGGER_STORAGE_DESTROYED: i64 = 4020;
const TRIGGER_RESOURCE_ACCESSIBLE: i64 = 4026;
const TRIGGER_WAITING_AREA_IN_RANGE: i64 = 4027;
const TRIGGER_RESOURCE_TARGET_IS_TREE: i64 = 4028;
const TRIGGER_OPEN_POSITION_AVAILABLE: i64 = 4029;
const TRIGGER_IM_A_FARMER: i64 = 4034;
const TRIGGER_FARM_NOT_FOUND: i64 = 4035;
const TRIGGER_CAN_USE_THIS_TILE_AS_WAITING_AREA: i64 = 4038;
const TRIGGER_GOAL_IS_RESOURCE: i64 = 5007;
const TRIGGER_GOAL_IS_STORAGE: i64 = 5008;
const TRIGGER_GOAL_IS_WAITING_AREA: i64 = 5019;
const TRIGGER_CANNOT_PATH_TO_TARGET: i64 = 8011;

const RESOURCE_WOOD: i32 = 3;
const UNIT_TYPE_FARM: i32 = 0x2a;
const UNIT_TYPE_GRANARY: i32 = 0x34;
const UNIT_TYPE_GRANARY_IMPROVED: i32 = 0x2ba;
const FAMILY_ANIMAL: i32 = 0x11;
const FAMILY_FISH: i32 = 0x12;
const CATEGORY_LAND_CITIZEN: i32 = 6;
const CATEGORY_NAVAL_CITIZEN: i32 = 0x16;
/// the squared distance from the resource's centre within which a gatherer stops waiting (0x4475a0)
const WAIT_RANGE_SQUARED: f32 = 36.0;
/// the path length `EEGetPathLengthTo` reports for no path (0x447598)
const NO_PATH_DISTANCE: f32 = 10000.0;
/// the save version from which a resource out of the world is saved as its id (`EESaveHasGhostResources`: 0.272)
const VERSION_GHOST_RESOURCES: u32 = 272;

/// One gather goal's state.
#[derive(Clone, Copy, Debug, PartialEq)]
struct Gather {
    waiting_area: [f32; 3],
    destination: [f32; 3],
    resource: Held,
    storage: Held,
    gathering: bool,
    waiting: bool,
}

impl Gather {
    fn empty() -> Gather {
        Gather {
            waiting_area: [0.0; 3],
            destination: [0.0; 3],
            resource: Held::None,
            storage: Held::None,
            gathering: false,
            waiting: false,
        }
    }
}

static GATHERS: Mutex<BTreeMap<u64, Gather>> = Mutex::new(BTreeMap::new());

fn get(goal: u64) -> Gather {
    let goals = GATHERS.lock().unwrap_or_else(|e| e.into_inner());
    goals.get(&goal).copied().unwrap_or_else(Gather::empty)
}

fn put(goal: u64, state: Gather) {
    let mut goals = GATHERS.lock().unwrap_or_else(|e| e.into_inner());
    goals.insert(goal, state);
}

/// One field changed in the record (the record read and written at once).
fn change(goal: u64, f: impl FnOnce(&mut Gather)) {
    let mut goals = GATHERS.lock().unwrap_or_else(|e| e.into_inner());
    let g = goals.entry(goal).or_insert_with(Gather::empty);
    f(g);
}

fn forget(goal: u64) -> Gather {
    let mut goals = GATHERS.lock().unwrap_or_else(|e| e.into_inner());
    goals.remove(&goal).unwrap_or_else(Gather::empty)
}

fn resource(goal: u64) -> u64 {
    get(goal).resource.object()
}

fn storage(goal: u64) -> u64 {
    get(goal).storage.object()
}

/// What the gatherer's search results and world calls are (the FFI adapter; the tests' mock).
pub trait GatherHost: BuildHost {
    /// the farm's footprint tile nearest `near` (the storage's tile or the gatherer's: `EETileDistanceLess`)
    fn farm_tile(&mut self, farm: u64, near: u64) -> (i32, i32);
    /// `EEFindResourceSpot(unit, resource, out)` (`out` in and out)
    fn resource_spot(&mut self, unit: u64, resource: u64, out: &mut [f32; 3]) -> bool;
    /// the citizen class's `FindNextResource(unit, resource, type, &passed)`
    fn next_resource(&mut self, unit: u64, resource: u64, kind: i32, passed: &[u64]) -> u64;
    /// the citizen class's `FindStorage(unit, new)`
    fn find_storage(&mut self, unit: u64, new: bool) -> u64;
    /// `EEFindNextFreeTile(unit, storage, out)` (`out` in and out)
    fn free_tile(&mut self, unit: u64, storage: u64, out: &mut [f32; 3]) -> bool;
    /// the height of the unit's tile
    fn tile_height(&mut self, unit: u64) -> f32;
    /// `IsAtResourceSpot`: within 4 tiles of the resource, on one of its spots with room to stand
    fn at_resource_spot(&mut self, unit: u64, resource: u64) -> bool;
    /// `SendToNextFarm`: the first free farm around the farm's drop sites worked
    fn next_farm(&mut self, unit: u64, resource: u64);
    /// `FindStorage`'s search: the fire effect released; an AI that orders a settlement for the gatherer instead
    /// (`None`); else the storage found (0: none)
    fn storage_search(&mut self, unit: u64) -> Option<u64>;
    /// `FindGatherSpot`'s search: a waiting area by the resource
    fn gather_spot(&mut self, unit: u64, resource: u64) -> Option<[f32; 3]>;
    /// `BeginGathering`'s animation, effect, facing, work timer and stop
    fn begin_gathering(&mut self, unit: u64, resource: u64);
    /// `EEDepositLoad(unit, storage)`
    fn deposit_load(&mut self, unit: u64, storage: u64);
    /// `GatherNextTree`'s world part: a carried load of another type dropped; for a tree with no free spot the
    /// first gatherable tree of its forest on the line from a nearby drop site (0: none), else the gathered
    /// resource set (but for animals)
    fn next_tree(&mut self, unit: u64, resource: u64) -> u64;
    /// `BuildFarms`' search: a build goal for each unfinished own settlement or town centre in sight, each set as
    /// the goal's storage (through the goal's accessor)
    fn build_farms(&mut self, unit: u64, goal: u64);
    /// the hunt of a living animal (`new EEUGAttackUnit`, the attack action) when the unit can (false: not)
    fn hunt(&mut self, unit: u64, resource: u64, arg: bool) -> bool;
    /// the unit's `SetGatheredResource(resource)`
    fn set_gathered(&mut self, unit: u64, resource: u64);
    /// `HasGatherRoom(unit, resource)`
    fn gather_room(&mut self, unit: u64, resource: u64) -> bool;
    /// the unit's last waypoint's location
    fn last_waypoint(&mut self, unit: u64) -> [f32; 3];
    /// `EEIsWithinReachOf(point, unit, resource)`; the storage's inner radius times sqrt 2 (2D) holds the point
    fn resource_reach(&mut self, unit: u64, resource: u64, point: [f32; 3]) -> bool;
    fn storage_reach(&mut self, storage: u64, point: [f32; 3]) -> bool;
    /// the goal's `mIsActive`
    fn set_active(&mut self, goal: u64, active: bool);
    /// the loaded ghost of an id (`EEFindLoadedGhostUnit`, or `EEFindLoadedGhost` for a complex unit)
    fn find_ghost(&mut self, id: u32, complex: bool) -> u64;
    /// `IsAtResource` (the gatherer faces it), `IsAtStorage`
    fn at_resource(&mut self, unit: u64, resource: u64) -> bool;
    fn at_storage(&mut self, unit: u64, storage: u64) -> bool;
    /// `EEIsLineToUnitBlocked(unit, target, ...)`
    fn line_to_unit_blocked(&mut self, unit: u64, target: u64) -> bool;
    /// the resource-accessible trigger (the terrain, the continent, the gather point)
    fn accessible(&mut self, unit: u64, resource: u64) -> bool;
    /// `EEGetPathLengthTo(unit, tile, 0x10, false)`
    fn path_length(&mut self, unit: u64, tile: (i32, i32)) -> f32;
    /// `Work`: one gathering tick
    fn gather_work(&mut self, unit: u64, resource: u64);
    /// `SearchNavalResource`'s world part: 0 idle, 1 another fish (the result), 2 nothing, 3 waits facing it
    fn naval_search(&mut self, unit: u64, resource: u64) -> (i64, u64);
    /// the unit's active goal's `SetPathAttempts(0)`
    fn reset_active_path(&mut self, unit: u64);
}

/// `SetResource`: the reference moved.
fn set_resource(goal: u64, unit: u64, host: &mut dyn GatherHost) {
    let old = resource(goal);
    if old != 0 {
        host.dereference(old, false);
    }
    change(goal, |g| g.resource = Held::of(unit));
    if unit != 0 {
        host.reference(unit, false);
    }
}

/// `SetStorage`: the reference moved.
fn set_storage(goal: u64, unit: u64, host: &mut dyn GatherHost) {
    let old = storage(goal);
    if old != 0 {
        host.dereference(old, false);
    }
    change(goal, |g| g.storage = Held::of(unit));
    if unit != 0 {
        host.reference(unit, false);
    }
}

/// `EEUGGather(inResource, inStorage, inArg)`: both referenced, the destination at the resource's tile (else the
/// storage's), the waiting area there.
pub fn create(goal: u64, resource: u64, storage: u64, host: &mut dyn GatherHost) {
    let mut g = Gather::empty();
    g.resource = Held::of(resource);
    g.storage = Held::of(storage);
    put(goal, g);
    if resource != 0 {
        host.reference(resource, false);
    }
    if storage != 0 {
        host.reference(storage, false);
    }
    let at = if resource != 0 { resource } else { storage };
    if at != 0 {
        let (x, y) = host.unit_tile(at);
        let d = host.tile_location(x, y);
        change(goal, |g| g.destination = d);
    }
    change(goal, |g| g.waiting_area = g.destination);
}

fn destroy(goal: u64, host: &mut dyn GatherHost) {
    let g = forget(goal);
    if let Held::Object(r) = g.resource {
        host.dereference(r, false);
    }
    if let Held::Object(s) = g.storage {
        host.dereference(s, false);
    }
}

/// `GetDestination`: the waiting area, the farm tile nearest the granary (or gatherer), a spot by the resource
/// (wood and moving animals pass to the next one), a free tile by a storage (trying others); the group told.
fn destination(goal: u64, unit: u64, out: &mut [f32; 3], host: &mut dyn GatherHost) {
    let g = get(goal);
    if g.waiting {
        *out = g.waiting_area;
    } else if g.gathering && g.resource != Held::None {
        let r = g.resource.object();
        if host.build_unit(r).unit_type == UNIT_TYPE_FARM {
            let s = g.storage.object();
            let st = host.build_unit(s).unit_type;
            let near = if s != 0 && (st == UNIT_TYPE_GRANARY_IMPROVED || st == UNIT_TYPE_GRANARY) { s } else { unit };
            let (x, y) = host.farm_tile(r, near);
            out[0] = x as f32 + 0.5;
            out[1] = y as f32 + 0.5;
        } else {
            let mut passed: Vec<u64> = Vec::new();
            loop {
                let r = resource(goal);
                if r == 0 || host.resource_spot(unit, r, out) {
                    break;
                }
                let u = host.build_unit(r);
                if u.resource_type != RESOURCE_WOOD && !u.moving {
                    break;
                }
                passed.push(r);
                let next = host.next_resource(unit, r, u.resource_type, &passed);
                set_resource(goal, next, host);
            }
        }
        let d = *out;
        change(goal, |g| g.destination = d);
    } else if g.storage != Held::None {
        let mut passed: Vec<u64> = Vec::new();
        loop {
            let s = storage(goal);
            if s == 0 || host.free_tile(unit, s, out) || passed.contains(&s) {
                break;
            }
            passed.push(s);
            let next = host.find_storage(unit, true);
            set_storage(goal, next, host);
        }
        let d = *out;
        change(goal, |g| g.destination = d);
    } else {
        let (x, y) = host.unit_tile(unit);
        *out = [x as f32, y as f32, host.tile_height(unit)];
    }
    host.tell_group(unit, *out);
}

/// `FindGatherSpot`: a waiting area by the resource (true when found).
pub fn find_gather_spot(goal: u64, unit: u64, host: &mut dyn GatherHost) -> bool {
    let r = resource(goal);
    match host.gather_spot(unit, r) {
        Some(p) => {
            change(goal, |g| g.waiting_area = p);
            true
        }
        None => false,
    }
}

/// `ArriveAtResource`: at the waiting area (not for trees): hunts a living animal, else moves in when the resource
/// has room and the gatherer is within 6 tiles or nearer than the waiting area; otherwise waits (idles with no
/// gather spot).
fn arrive(goal: u64, unit: u64, host: &mut dyn GatherHost) {
    let r = resource(goal);
    let ru = host.build_unit(r);
    if ru.is_tree {
        return;
    }
    host.set_physics_action(unit, 0);
    if ru.family == FAMILY_ANIMAL || ru.family == FAMILY_FISH {
        let arg = host.goal_arg(goal);
        if !host.hunt(unit, r, arg) {
            host.start_action(unit, ACTION_MOVE_TO_RESOURCE);
        }
        return;
    }
    if !find_gather_spot(goal, unit, host) {
        host.start_action(unit, ACTION_IDLE);
        return;
    }
    let r = resource(goal);
    let ru = host.build_unit(r);
    let half = ru.tile_size as f32 * 0.5;
    let x = ru.tile_x as f32 + half;
    let y = ru.tile_y as f32 + half;
    host.set_gathered(unit, 0);
    let at = host.build_unit(unit).location;
    let dy = at[1] - y;
    let dx = at[0] - x;
    let distance = dx * dx + dy * dy;
    if host.gather_room(unit, r) {
        let wa = get(goal).waiting_area;
        let start = distance <= WAIT_RANGE_SQUARED || {
            let dy = wa[1] - y;
            let dx = wa[0] - x;
            distance < dx * dx + dy * dy
        };
        if start {
            host.start_action(unit, ACTION_MOVE_TO_RESOURCE);
            return;
        }
    }
    change(goal, |g| {
        g.gathering = false;
        g.waiting = true;
    });
}

/// `Step`: the current command's handler.
pub fn step(goal: u64, unit: u64, host: &mut dyn GatherHost) {
    match host.unit_command(unit) {
        ACTION_FIND_NEW_STORAGE | ACTION_FIND_STORAGE => {
            // FindStorage
            if let Some(found) = host.storage_search(unit) {
                set_storage(goal, 0, host);
                if found != 0 {
                    set_storage(goal, found, host);
                }
            }
        }
        ACTION_FIND_NEW_RESOURCE => {
            let r = resource(goal);
            let kind = if r != 0 { host.build_unit(r).resource_type } else { host.build_unit(unit).resource_type };
            let next = host.next_resource(unit, r, kind, &[]);
            set_resource(goal, next, host);
        }
        ACTION_ADD_RESOURCE_TO_STORAGE => {
            host.deposit_load(unit, storage(goal));
            host.reset_path_attempts(goal);
        }
        ACTION_GATHER_RESOURCE => {
            let r = resource(goal);
            if r != 0 {
                host.begin_gathering(unit, r);
            }
        }
        ACTION_MOVE_TO_WAITING_AREA => arrive(goal, unit, host),
        ACTION_MOVE_TO_STORAGE => {
            // BuildFarms
            host.set_physics_action(unit, 0);
            let r = resource(goal);
            if r == 0 || host.build_unit(r).unit_type != UNIT_TYPE_FARM {
                host.build_farms(unit, goal);
            }
            change(goal, |g| {
                g.gathering = false;
                g.waiting = false;
            });
        }
        ACTION_MOVE_TO_RESOURCE => {
            // GatherNextTree
            let found = host.next_tree(unit, resource(goal));
            if found != 0 {
                set_resource(goal, found, host);
            }
            host.set_physics_action(unit, 0);
            change(goal, |g| {
                g.waiting = false;
                g.gathering = true;
            });
        }
        ACTION_FIND_FARM => host.next_farm(unit, resource(goal)),
        ACTION_WAIT_FOR_OPEN_POSITION => {
            // SearchNavalResource
            let (code, found) = host.naval_search(unit, resource(goal));
            match code {
                0 => host.start_action(unit, ACTION_IDLE),
                1 => {
                    set_resource(goal, found, host);
                    host.start_action(unit, ACTION_MOVE_TO_WAITING_AREA);
                }
                3 => change(goal, |g| g.gathering = false),
                _ => {}
            }
        }
        ACTION_NOTIFY_WAITING_CITIZEN => {
            // FinishGathering
            let ru = host.build_unit(resource(goal));
            if ru.resource_type != RESOURCE_WOOD && ru.family != FAMILY_ANIMAL {
                host.set_gathered(unit, 0);
            }
            host.reset_active_path(unit);
            change(goal, |g| g.gathering = false);
        }
        _ => {}
    }
}

/// `PathTaken`: the goal active when the path's last waypoint reaches the waiting area, the resource or the storage.
fn path_taken(goal: u64, unit: u64, host: &mut dyn GatherHost) {
    let w = host.last_waypoint(unit);
    let g = get(goal);
    let active = if g.waiting {
        let mut target = g.waiting_area;
        if w[2] == 0.0 {
            target[2] = 0.0;
        }
        let p = Ge3DPoint { x: w[0], y: w[1], z: w[2] };
        p.distance_to_squared(Ge3DPoint { x: target[0], y: target[1], z: target[2] }) < 1.0
    } else if g.gathering {
        let r = g.resource.object();
        r != 0 && host.resource_reach(unit, r, w)
    } else {
        let s = g.storage.object();
        s != 0 && host.storage_reach(s, w)
    };
    host.set_active(goal, active);
}

/// `EvaluateCondition`: the gather triggers; others the base goal's.
pub fn evaluate(goal: u64, unit: u64, event: i64, host: &mut dyn GatherHost) -> bool {
    let g = get(goal);
    let (r, s) = (g.resource.object(), g.storage.object());
    match event {
        TRIGGER_BLOCKED_BY_WALL => {
            if g.gathering && r != 0 {
                host.line_to_unit_blocked(unit, r)
            } else if !g.gathering && s != 0 {
                host.line_to_unit_blocked(unit, s)
            } else {
                false
            }
        }
        TRIGGER_RESOURCE_CAPACITY_FILLED => {
            let u = host.build_unit(unit);
            u.resource_amount >= u.carry_amount
        }
        TRIGGER_RESOURCE_CAPACITY_EMPTY => host.build_unit(unit).resource_amount == 0,
        TRIGGER_NEW_RESOURCE_FOUND => r != 0,
        TRIGGER_RESOURCE_DEPLETED => {
            if r == 0 {
                return true;
            }
            let u = host.build_unit(r);
            !u.valid || u.resource_amount == 0
        }
        TRIGGER_RESOURCE_IN_RANGE => r != 0 && host.build_unit(r).valid && host.at_resource(unit, r),
        TRIGGER_STORAGE_IN_RANGE => s != 0 && host.build_unit(s).valid && host.at_storage(unit, s),
        TRIGGER_STORAGE_FOUND => s != 0,
        TRIGGER_STORAGE_DESTROYED => {
            if s == 0 {
                return true;
            }
            let u = host.build_unit(s);
            !u.valid || u.hit_points == 0 || u.disabled
        }
        TRIGGER_RESOURCE_ACCESSIBLE => host.accessible(unit, r),
        TRIGGER_WAITING_AREA_IN_RANGE => {
            let (x, y) = host.unit_tile(unit);
            x == i64::from(um_floor(g.waiting_area[0])) && y == i64::from(um_floor(g.waiting_area[1]))
        }
        TRIGGER_RESOURCE_TARGET_IS_TREE => r != 0 && host.build_unit(r).resource_type == RESOURCE_WOOD,
        TRIGGER_OPEN_POSITION_AVAILABLE => host.gather_room(unit, r),
        TRIGGER_IM_A_FARMER => r != 0 && host.build_unit(r).unit_type == UNIT_TYPE_FARM,
        TRIGGER_FARM_NOT_FOUND => true,
        TRIGGER_CAN_USE_THIS_TILE_AS_WAITING_AREA => host.at_resource_spot(unit, r),
        TRIGGER_GOAL_IS_RESOURCE => g.gathering,
        TRIGGER_GOAL_IS_STORAGE => !g.gathering && !g.waiting,
        TRIGGER_GOAL_IS_WAITING_AREA => g.waiting,
        TRIGGER_CANNOT_PATH_TO_TARGET => {
            let tile = if g.gathering && r != 0 {
                let u = host.build_unit(r);
                (u.tile_x, u.tile_y)
            } else if !g.gathering && s != 0 {
                let u = host.build_unit(s);
                (u.tile_x, u.tile_y)
            } else if g.waiting {
                (um_floor(g.waiting_area[0]), um_floor(g.waiting_area[1]))
            } else {
                return false;
            };
            host.path_length(unit, tile) == NO_PATH_DISTANCE
        }
        _ => base_condition(event),
    }
}

/// `ReplaceUnit`: the storage replaced by the new unit when the same player owns it, else dropped.
fn replace_unit(goal: u64, old: u64, new: u64, host: &mut dyn GatherHost) {
    let s = storage(goal);
    if s != old || old == 0 {
        return;
    }
    if host.build_unit(new).player == host.build_unit(s).player {
        change(goal, |g| g.storage = Held::Object(new));
        host.reference(new, false);
    } else {
        change(goal, |g| g.storage = Held::None);
    }
    host.dereference(old, false);
}

/// `GetGoalLocation`: the waiting area while waiting with no current target, else the destination.
fn location(goal: u64) -> [f32; 3] {
    let g = get(goal);
    let target = (g.gathering && g.resource != Held::None) || (!g.gathering && g.storage != Held::None);
    if !target && g.waiting {
        g.waiting_area
    } else {
        g.destination
    }
}

/// `GetStartAction`
fn start_action(goal: u64) -> i64 {
    let g = get(goal);
    if g.resource != Held::None {
        ACTION_MOVE_TO_WAITING_AREA
    } else if g.storage != Held::None {
        ACTION_MOVE_TO_STORAGE
    } else {
        ACTION_FIND_NEW_RESOURCE
    }
}

fn bits(v: f32) -> i32 {
    v.to_bits() as i32
}

fn id_or(unit: Held, none: u32, host: &mut dyn GatherHost) -> u32 {
    match unit {
        Held::Object(u) => host.unit_id(u),
        _ => none,
    }
}

/// The checksum after the base goal's: the waiting area's and destination's bits, the ids, the flags.
pub fn checksum(goal: u64, host: &mut dyn GatherHost) -> i64 {
    let g = get(goal);
    let mut sum = 0i32;
    for v in g.waiting_area.iter().chain(g.destination.iter()) {
        sum = sum.wrapping_add(bits(*v));
    }
    sum = sum.wrapping_add(id_or(g.resource, 0, host) as i32);
    sum = sum.wrapping_add(id_or(g.storage, 0, host) as i32);
    sum = sum.wrapping_add(i32::from(g.waiting)).wrapping_add(i32::from(g.gathering));
    i64::from(sum)
}

/// The out-of-synch dump.
pub fn dump(goal: u64, host: &mut dyn GatherHost) {
    let g = get(goal);
    let p = "OUT OF SYNCH DUMP:  EEUGGather - ";
    for (name, point) in [("Waiting Area", g.waiting_area), ("Actual Destination", g.destination)] {
        for (axis, v) in ["X", "Y", "Z"].iter().zip(point) {
            host.append(&format!("{p}{name} {axis}: {:6.6}\n", f64::from(v)));
        }
    }
    let r = id_or(g.resource, u32::MAX, host);
    host.append(&format!("{p}Resource Target ID: #{}\n", hex8(r)));
    let s = id_or(g.storage, u32::MAX, host);
    host.append(&format!("{p}Storage Target ID: #{}\n", hex8(s)));
    host.append(&format!("{p}Gathering Flag {}\n", i32::from(g.gathering)));
    host.append(&format!("{p}Waiting Flag {}\n", i32::from(g.waiting)));
}

/// `GetDebugLine`: "gather" or "dropoff", the resource's id and tile (its type and amount when gathering).
pub fn debug_line(goal: u64, host: &mut dyn GatherHost) {
    let g = get(goal);
    host.append(if g.gathering { "gather" } else { "dropoff" });
    if let Held::Object(r) = g.resource {
        let id = host.unit_id(r) & 0xff_ffff;
        let u = host.build_unit(r);
        if g.gathering {
            host.append(&format!(
                "(uid {id}, at {},{}, type {}, amount {})",
                u.tile_x, u.tile_y, u.resource_type, u.resource_amount
            ));
        } else {
            host.append(&format!("(uid {id}, at {},{})", u.tile_x, u.tile_y));
        }
    }
    host.append(":");
}

/// `GetDebugText`: "Gathering" or "Dropping Off", the resource's name and tile (its type and amount when gathering).
pub fn debug_text(goal: u64, host: &mut dyn GatherHost) {
    let g = get(goal);
    host.append(if g.gathering { "Gathering\n" } else { "Dropping Off\n" });
    if let Held::Object(r) = g.resource {
        let u = host.build_unit(r);
        host.append("Unit: ");
        host.append_class_name(r);
        if g.gathering {
            host.append(&format!(
                " at ({},{})\nType:{}\nAmount:{}\n",
                u.tile_x, u.tile_y, u.resource_type, u.resource_amount
            ));
        } else {
            host.append(&format!(" at ({},{})\n", u.tile_x, u.tile_y));
        }
    }
}

fn save_point(p: [f32; 3], host: &mut dyn GatherHost) -> i32 {
    if !host.file_open() {
        return 4;
    }
    for v in p {
        let result = host.write_f32(v);
        if result != 0 {
            return result;
        }
    }
    0
}

fn load_point(p: &mut [f32; 3], host: &mut dyn GatherHost) -> i32 {
    if !host.file_open() {
        return 4;
    }
    for v in p.iter_mut() {
        let result = host.read_f32(v);
        if result != 0 {
            return result;
        }
    }
    0
}

/// `Save`'s part after the base goal's: the resource's and storage's ids (-1 out of the world before 0.272), the
/// gathering flag, the waiting area, the waiting flag, the destination; the first failing status.
pub fn save(goal: u64, major: u32, minor: u32, host: &mut dyn GatherHost) -> i32 {
    let g = get(goal);
    let ghosts = major > 0 || minor >= VERSION_GHOST_RESOURCES;
    let mut ids = [u32::MAX; 2];
    for (k, unit) in [g.resource, g.storage].into_iter().enumerate() {
        if let Held::Object(u) = unit {
            if host.unit_valid(u) || ghosts {
                ids[k] = host.unit_id(u);
            }
        }
    }
    let mut result = host.write_u32(ids[0]);
    if result != 0 {
        return result;
    }
    result = host.write_u32(ids[1]);
    if result != 0 {
        return result;
    }
    result = host.write_bool(g.gathering);
    if result != 0 {
        return result;
    }
    result = save_point(g.waiting_area, host);
    if result != 0 {
        return result;
    }
    result = host.write_bool(g.waiting);
    if result == 0 {
        result = save_point(g.destination, host);
    }
    result
}

/// `Load`'s part after the base goal's (the ids resolved by `repair`).
pub fn load(goal: u64, host: &mut dyn GatherHost) -> i32 {
    let mut g = get(goal);
    let mut id = 0u32;
    let mut result = host.read_u32(&mut id);
    if result != 0 {
        return result;
    }
    g.resource = Held::Saved(id);
    put(goal, g);
    result = host.read_u32(&mut id);
    if result != 0 {
        return result;
    }
    g.storage = Held::Saved(id);
    put(goal, g);
    result = host.read_bool(&mut g.gathering);
    if result == 0 {
        result = load_point(&mut g.waiting_area, host);
    }
    if result == 0 {
        result = host.read_bool(&mut g.waiting);
        if result == 0 {
            result = load_point(&mut g.destination, host);
        }
    }
    put(goal, g);
    result
}

/// `RepairPointers`: the resource and the storage from their ids (or their loaded ghosts), referenced; 3 when one is
/// gone (a missing resource leaves the storage cleared).
pub fn repair(goal: u64, host: &mut dyn GatherHost) -> i32 {
    let g = get(goal);
    let mut result = 0;
    let id = g.resource.saved_id();
    if id != u32::MAX {
        let mut r = host.find_unit(id, false);
        if r == 0 {
            r = host.find_ghost(id, false);
        }
        change(goal, |g| g.resource = Held::of(r));
        if r != 0 {
            host.reference(r, false);
        } else {
            result = 3;
        }
    } else {
        change(goal, |g| {
            g.resource = Held::None;
            g.waiting = false;
            g.gathering = false;
        });
    }
    let id = g.storage.saved_id();
    if result == 0 && id != u32::MAX {
        let mut s = host.find_unit(id, true);
        if s == 0 {
            s = host.find_ghost(id, true);
        }
        change(goal, |g| g.storage = Held::of(s));
        if s != 0 {
            host.reference(s, false);
        } else {
            result = 3;
        }
    } else {
        change(goal, |g| g.storage = Held::None);
    }
    result
}

/// `Start`: to the waiting area with a resource, else idle.
fn start(goal: u64, unit: u64, host: &mut dyn GatherHost) {
    let action = if resource(goal) != 0 { ACTION_MOVE_TO_WAITING_AREA } else { ACTION_IDLE };
    host.start_action(unit, action);
}

/// `Suits`: citizens only
fn suits(unit: u64, host: &mut dyn GatherHost) -> bool {
    let category = host.build_unit(unit).category;
    category == CATEGORY_LAND_CITIZEN || category == CATEGORY_NAVAL_CITIZEN
}

/// The goal's body `op` (rust_goal.rs `EE_UNITS_GOAL_*`).
pub fn body(op: i32, goal: u64, a: u64, b: u64, io: &mut EeUnitsGoalIo, host: &mut dyn GatherHost) -> i64 {
    use crate::rust_goal::*;
    match op {
        EE_UNITS_GOAL_NEW_GATHER => create(goal, a, b, host),
        EE_UNITS_GOAL_GATHER_NEW_EMPTY => put(goal, Gather::empty()),
        EE_UNITS_GOAL_DESTROY => destroy(goal, host),
        EE_UNITS_GOAL_CLONE => {
            let g = get(goal);
            let copy = host.new_goal(GOAL_GATHER, b != 0);
            create(copy, g.resource.object(), g.storage.object(), host);
            change(copy, |c| {
                c.gathering = g.gathering;
                c.waiting = g.waiting;
                c.destination = g.destination;
                c.waiting_area = g.waiting_area;
            });
            return copy as i64;
        }
        EE_UNITS_GOAL_CHECKSUM => return checksum(goal, host),
        EE_UNITS_GOAL_DUMP => dump(goal, host),
        EE_UNITS_GOAL_DEBUG_LINE => debug_line(goal, host),
        EE_UNITS_GOAL_DEBUG_TEXT => debug_text(goal, host),
        EE_UNITS_GOAL_LOCATION => io.f[..3].copy_from_slice(&location(goal)),
        EE_UNITS_GOAL_DESTINATION => {
            let mut out = [io.f[0], io.f[1], io.f[2]];
            destination(goal, a, &mut out, host);
            io.f[..3].copy_from_slice(&out);
        }
        EE_UNITS_GOAL_START_ACTION => return start_action(goal),
        EE_UNITS_GOAL_MAX_PATH_ATTEMPTS => return 3,
        EE_UNITS_GOAL_SUITS => return i64::from(suits(a, host)),
        EE_UNITS_GOAL_STEP => step(goal, a, host),
        EE_UNITS_GOAL_WORK => {
            let r = resource(goal);
            if r != 0 {
                host.gather_work(a, r);
            }
        }
        EE_UNITS_GOAL_PATH_TAKEN => path_taken(goal, a, host),
        EE_UNITS_GOAL_START => start(goal, a, host),
        EE_UNITS_GOAL_SAVE => return i64::from(save(goal, a as u32, b as u32, host)),
        EE_UNITS_GOAL_LOAD => return i64::from(load(goal, host)),
        EE_UNITS_GOAL_REPAIR => return i64::from(repair(goal, host)),
        EE_UNITS_GOAL_EVALUATE => return i64::from(evaluate(goal, a, b as i64, host)),
        EE_UNITS_GOAL_REPLACE_UNIT => replace_unit(goal, a, b, host),
        EE_UNITS_GOAL_GATHER_FIELDS => {
            let g = get(goal);
            io.i[0] = g.storage.object() as i64;
            io.i[1] = i64::from(g.gathering);
            io.i[2] = i64::from(g.waiting);
            io.f[..3].copy_from_slice(&g.destination);
            return g.resource.object() as i64;
        }
        EE_UNITS_GOAL_GATHER_SET_RESOURCE => set_resource(goal, a, host),
        EE_UNITS_GOAL_GATHER_SET_STORAGE => set_storage(goal, a, host),
        EE_UNITS_GOAL_GATHER_FIND_SPOT => return i64::from(find_gather_spot(goal, a, host)),
        // SetOwner, Abandon, AddCreatedUnit: the base's (empty)
        _ => {}
    }
    0
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::build::tests::{fresh, world, World, B, P, W};
    use crate::rust_goal::*;

    const R: u64 = 0x300;
    const R2: u64 = 0x400;
    const S2: u64 = 0x500;

    fn knob(w: &World, name: &str) -> i64 {
        w.knobs.get(name).copied().unwrap_or(0)
    }

    impl GatherHost for World {
        fn farm_tile(&mut self, f: u64, n: u64) -> (i32, i32) {
            self.log.push(format!("farm tile {f:x} {n:x}"));
            (7, 8)
        }
        fn resource_spot(&mut self, _u: u64, r: u64, out: &mut [f32; 3]) -> bool {
            if knob(self, "spot") as u64 == r {
                *out = [1.0, 2.0, 3.0];
                return true;
            }
            false
        }
        fn next_resource(&mut self, _u: u64, r: u64, k: i32, passed: &[u64]) -> u64 {
            self.log.push(format!("next resource {r:x} {k} {}", passed.len()));
            knob(self, "next") as u64
        }
        fn find_storage(&mut self, _u: u64, n: bool) -> u64 {
            self.log.push(format!("find storage {n}"));
            knob(self, "storage") as u64
        }
        fn free_tile(&mut self, _u: u64, s: u64, out: &mut [f32; 3]) -> bool {
            if knob(self, "free") as u64 == s {
                *out = [4.0, 5.0, 6.0];
                return true;
            }
            false
        }
        fn tile_height(&mut self, _u: u64) -> f32 {
            2.5
        }
        fn at_resource_spot(&mut self, _u: u64, _r: u64) -> bool {
            true
        }
        fn next_farm(&mut self, u: u64, r: u64) {
            self.log.push(format!("next farm {u:x} {r:x}"));
        }
        fn storage_search(&mut self, _u: u64) -> Option<u64> {
            match knob(self, "search") {
                -1 => None,
                s => Some(s as u64),
            }
        }
        fn gather_spot(&mut self, _u: u64, _r: u64) -> Option<[f32; 3]> {
            (knob(self, "gather spot") != 0).then_some([30.5, 40.5, 1.0])
        }
        fn begin_gathering(&mut self, u: u64, r: u64) {
            self.log.push(format!("begin {u:x} {r:x}"));
        }
        fn deposit_load(&mut self, u: u64, s: u64) {
            self.log.push(format!("deposit load {u:x} {s:x}"));
        }
        fn next_tree(&mut self, _u: u64, _r: u64) -> u64 {
            knob(self, "tree") as u64
        }
        fn build_farms(&mut self, u: u64, g: u64) {
            self.log.push(format!("build farms {u:x}"));
            // the C++ sets the storage through the goal's accessor
            set_storage(g, S2, self);
        }
        fn hunt(&mut self, _u: u64, r: u64, a: bool) -> bool {
            self.log.push(format!("hunt {r:x} {a}"));
            knob(self, "hunt") != 0
        }
        fn set_gathered(&mut self, u: u64, r: u64) {
            self.log.push(format!("gathered {u:x} {r:x}"));
        }
        fn gather_room(&mut self, _u: u64, _r: u64) -> bool {
            knob(self, "room") != 0
        }
        fn last_waypoint(&mut self, _u: u64) -> [f32; 3] {
            [30.5, 40.5, 0.0]
        }
        fn resource_reach(&mut self, _u: u64, _r: u64, _p: [f32; 3]) -> bool {
            true
        }
        fn storage_reach(&mut self, _s: u64, _p: [f32; 3]) -> bool {
            false
        }
        fn set_active(&mut self, _g: u64, a: bool) {
            self.log.push(format!("active {a}"));
        }
        fn find_ghost(&mut self, id: u32, c: bool) -> u64 {
            if id == 0x0100_0004 && !c {
                R2
            } else {
                0
            }
        }
        fn at_resource(&mut self, _u: u64, _r: u64) -> bool {
            true
        }
        fn at_storage(&mut self, _u: u64, _s: u64) -> bool {
            false
        }
        fn line_to_unit_blocked(&mut self, _u: u64, t: u64) -> bool {
            t == B
        }
        fn accessible(&mut self, _u: u64, _r: u64) -> bool {
            true
        }
        fn path_length(&mut self, _u: u64, t: (i32, i32)) -> f32 {
            if t == (5, 6) {
                10000.0
            } else {
                3.0
            }
        }
        fn gather_work(&mut self, u: u64, r: u64) {
            self.log.push(format!("work {u:x} {r:x}"));
        }
        fn naval_search(&mut self, _u: u64, _r: u64) -> (i64, u64) {
            (knob(self, "naval"), R2)
        }
        fn reset_active_path(&mut self, u: u64) {
            self.log.push(format!("reset path {u:x}"));
        }
    }

    /// The house B as the storage, a stone mine R at (5, 6) of size 2 with 40 left.
    fn gather_world() -> World {
        let mut w = world();
        *w.unit(R) = EeUnitsBuildUnit {
            complex: R,
            tile_size: 2,
            category: 3,
            tile_x: 5,
            tile_y: 6,
            resource_type: 4,
            resource_amount: 40,
            valid: true,
            ..Default::default()
        };
        *w.unit(R2) = EeUnitsBuildUnit { complex: R2, tile_x: 9, tile_y: 9, resource_type: 3, ..Default::default() };
        *w.unit(S2) = EeUnitsBuildUnit { complex: S2, player: P, ..Default::default() };
        w
    }

    fn goal(w: &mut World) -> u64 {
        let g = fresh();
        create(g, R, B, w);
        g
    }

    #[test]
    fn creation_copy_texts_and_checksum() {
        let mut w = gather_world();
        let g = goal(&mut w);
        let s = get(g);
        assert_eq!((s.resource, s.storage), (Held::Object(R), Held::Object(B)));
        assert_eq!((s.destination, s.waiting_area), ([5.0, 6.0, 0.0], [5.0, 6.0, 0.0]));
        assert_eq!((w.refs[&R], w.refs[&B]), (1, 1));
        change(g, |s| s.gathering = true);
        let mut io = EeUnitsGoalIo::default();
        let copy = body(EE_UNITS_GOAL_CLONE, g, 0, 1, &mut io, &mut w) as u64;
        assert_eq!(get(copy), get(g));
        assert_eq!(w.refs[&R], 2);
        debug_line(g, &mut w);
        assert_eq!(w.text, "gather(uid 3, at 5,6, type 4, amount 40):");
        w.text.clear();
        debug_text(g, &mut w);
        assert_eq!(w.text, "Gathering\nUnit: House at (5,6)\nType:4\nAmount:40\n");
        w.text.clear();
        dump(g, &mut w);
        assert!(w.text.starts_with("OUT OF SYNCH DUMP:  EEUGGather - Waiting Area X: 5.000000\n"));
        assert!(w.text.contains("Resource Target ID: #01000003\n"));
        assert!(w.text.contains("Storage Target ID: #01000001\n"));
        let expected = (5.0f32.to_bits() as i32)
            .wrapping_mul(2)
            .wrapping_add((6.0f32.to_bits() as i32).wrapping_mul(2))
            .wrapping_add(0x0100_0003 + 0x0100_0001 + 1);
        assert_eq!(checksum(g, &mut w), i64::from(expected));
        assert_eq!(start_action(g), ACTION_MOVE_TO_WAITING_AREA);
        destroy(copy, &mut w);
        destroy(g, &mut w);
        assert_eq!((w.refs[&R], w.refs[&B]), (0, 0));
    }

    #[test]
    fn steps() {
        let mut w = gather_world();
        let g = goal(&mut w);
        w.commands.insert(W, ACTION_FIND_STORAGE);
        w.knobs.insert("search", -1);
        step(g, W, &mut w);
        assert_eq!(get(g).storage, Held::Object(B));
        w.knobs.insert("search", S2 as i64);
        step(g, W, &mut w);
        assert_eq!(get(g).storage, Held::Object(S2));
        assert_eq!((w.refs[&B], w.refs[&S2]), (0, 1));
        w.commands.insert(W, ACTION_FIND_NEW_RESOURCE);
        w.knobs.insert("next", R2 as i64);
        step(g, W, &mut w);
        assert_eq!(get(g).resource, Held::Object(R2));
        assert_eq!(w.log.last().unwrap(), "next resource 300 4 0");
        w.log.clear();
        w.commands.insert(W, ACTION_MOVE_TO_STORAGE);
        step(g, W, &mut w);
        assert_eq!(w.log, ["physics 200 0", "build farms 200"]);
        w.commands.insert(W, ACTION_MOVE_TO_RESOURCE);
        w.knobs.insert("tree", R as i64);
        step(g, W, &mut w);
        let s = get(g);
        assert_eq!((s.resource, s.gathering, s.waiting), (Held::Object(R), true, false));
        w.log.clear();
        w.commands.insert(W, ACTION_WAIT_FOR_OPEN_POSITION);
        w.knobs.insert("naval", 3);
        step(g, W, &mut w);
        assert!(!get(g).gathering);
        w.knobs.insert("naval", 0);
        step(g, W, &mut w);
        assert_eq!(w.log, ["start 200 5000"]);
        w.log.clear();
        w.commands.insert(W, ACTION_NOTIFY_WAITING_CITIZEN);
        step(g, W, &mut w);
        assert_eq!(w.log, ["gathered 200 0", "reset path 200"]);
        destroy(g, &mut w);
    }

    #[test]
    fn arriving_at_the_resource() {
        let mut w = gather_world();
        let g = goal(&mut w);
        w.commands.insert(W, ACTION_MOVE_TO_WAITING_AREA);
        // no gather spot: idle
        step(g, W, &mut w);
        assert_eq!(w.log, ["physics 200 0", "start 200 5000"]);
        w.log.clear();
        // a spot and room; the gatherer at (9.5, 20.5) is 14 tiles off the mine's centre (6, 7), nearer than the spot
        w.knobs.insert("gather spot", 1);
        w.knobs.insert("room", 1);
        step(g, W, &mut w);
        assert_eq!(get(g).waiting_area, [30.5, 40.5, 1.0]);
        assert_eq!(w.log, ["physics 200 0", "gathered 200 0", "start 200 8008"]);
        w.log.clear();
        // no room: it waits
        w.knobs.insert("room", 0);
        step(g, W, &mut w);
        assert!(get(g).waiting && !get(g).gathering);
        assert_eq!(location(g), [5.0, 6.0, 0.0]);
        // an animal is hunted
        w.log.clear();
        w.unit(R).family = FAMILY_ANIMAL;
        w.knobs.insert("hunt", 1);
        step(g, W, &mut w);
        assert_eq!(w.log, ["physics 200 0", "hunt 300 true"]);
        destroy(g, &mut w);
    }

    #[test]
    fn destinations() {
        let mut w = gather_world();
        let g = goal(&mut w);
        // dropping off: the free tile by the storage
        w.knobs.insert("free", B as i64);
        let mut out = [0.0; 3];
        destination(g, W, &mut out, &mut w);
        assert_eq!(out, [4.0, 5.0, 6.0]);
        assert_eq!(get(g).destination, out);
        // gathering wood with no spot: the next resource, which has one
        change(g, |s| s.gathering = true);
        w.unit(R).resource_type = RESOURCE_WOOD;
        w.knobs.insert("next", R2 as i64);
        w.knobs.insert("spot", R2 as i64);
        destination(g, W, &mut out, &mut w);
        assert_eq!(out, [1.0, 2.0, 3.0]);
        assert_eq!(get(g).resource, Held::Object(R2));
        // a farm: the tile nearest the gatherer
        w.unit(R2).unit_type = UNIT_TYPE_FARM;
        destination(g, W, &mut out, &mut w);
        assert_eq!(out, [7.5, 8.5, 3.0]);
        assert!(w.log.contains(&"farm tile 400 200".to_string()));
        // nothing: the gatherer's tile at its height
        change(g, |s| {
            s.gathering = false;
            s.storage = Held::None;
        });
        destination(g, W, &mut out, &mut w);
        assert_eq!(out, [9.0, 20.0, 2.5]);
        change(g, |s| s.storage = Held::Object(B));
        destroy(g, &mut w);
    }

    #[test]
    fn triggers_and_the_path() {
        let mut w = gather_world();
        let g = goal(&mut w);
        assert!(evaluate(g, W, TRIGGER_BLOCKED_BY_WALL, &mut w));
        assert!(evaluate(g, W, TRIGGER_RESOURCE_CAPACITY_EMPTY, &mut w));
        assert!(evaluate(g, W, TRIGGER_RESOURCE_CAPACITY_FILLED, &mut w));
        assert!(!evaluate(g, W, TRIGGER_RESOURCE_DEPLETED, &mut w));
        assert!(evaluate(g, W, TRIGGER_RESOURCE_IN_RANGE, &mut w));
        assert!(evaluate(g, W, TRIGGER_STORAGE_FOUND, &mut w));
        assert!(!evaluate(g, W, TRIGGER_STORAGE_DESTROYED, &mut w));
        assert!(evaluate(g, W, TRIGGER_GOAL_IS_STORAGE, &mut w));
        assert!(!evaluate(g, W, TRIGGER_CANNOT_PATH_TO_TARGET, &mut w));
        change(g, |s| s.gathering = true);
        assert!(evaluate(g, W, TRIGGER_CANNOT_PATH_TO_TARGET, &mut w));
        assert!(evaluate(g, W, TRIGGER_UNIT_FACING_NOT_REQUIRED, &mut w));
        path_taken(g, W, &mut w);
        change(g, |s| {
            s.gathering = false;
            s.waiting = true;
            s.waiting_area = [30.5, 40.5, 2.0];
        });
        path_taken(g, W, &mut w);
        change(g, |s| s.waiting = false);
        path_taken(g, W, &mut w);
        assert_eq!(w.log, ["active true", "active true", "active false"]);
        // the storage taken over by another player: dropped
        replace_unit(g, B, S2, &mut w);
        assert_eq!(get(g).storage, Held::Object(S2));
        w.unit(R2).player = 0x51;
        replace_unit(g, S2, R2, &mut w);
        assert_eq!(get(g).storage, Held::None);
        assert_eq!((w.refs[&B], w.refs[&S2]), (0, 0));
        destroy(g, &mut w);
    }

    #[test]
    fn save_load_and_repair() {
        let mut w = gather_world();
        let g = goal(&mut w);
        assert_eq!(save(g, 0, 272, &mut w), 0);
        assert_eq!(w.file, ["u1000003", "u1000001", "b0", "f5", "f6", "f0", "b0", "f5", "f6", "f0"]);
        destroy(g, &mut w);
        // the resource gone from the world: its loaded ghost; the storage found
        let l = fresh();
        put(l, Gather::empty());
        let f = |v: f32| i64::from(v.to_bits());
        w.reads = vec![0x0100_0004, 0x0100_0001, 1, f(1.0), f(2.0), f(3.0), 0, f(4.0), f(5.0), f(6.0)];
        assert_eq!(load(l, &mut w), 0);
        assert_eq!(repair(l, &mut w), 0);
        let s = get(l);
        assert_eq!((s.resource, s.storage, s.gathering), (Held::Object(R2), Held::Object(B), true));
        assert_eq!((s.waiting_area, s.destination), ([1.0, 2.0, 3.0], [4.0, 5.0, 6.0]));
        destroy(l, &mut w);
        // no resource saved: the flags cleared, the storage kept
        let l = fresh();
        put(l, Gather::empty());
        w.reads = vec![i64::from(u32::MAX), 0x0100_0001, 1, 0, 0, 0, 1, 0, 0, 0];
        assert_eq!(load(l, &mut w), 0);
        assert_eq!(repair(l, &mut w), 0);
        let s = get(l);
        assert_eq!((s.resource, s.storage, s.gathering, s.waiting), (Held::None, Held::Object(B), false, false));
        destroy(l, &mut w);
        assert_eq!(w.refs[&B], 0);
    }
}
