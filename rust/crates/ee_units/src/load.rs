//! The load goals whole in Rust (RUST_PLAN R5b.4, the eighteenth round): `EEUGLoadContainer` and its two classes,
//! `EEUGLoadTransport` (`kGoalLoadTransport`: units boarding a transport) and `EEUGLoadGarrison`
//! (`kGoalLoadGarrison`: units garrisoning a building, which may then upgrade). Their state (the container, the
//! units to load, the ids between `Load` and `RepairPointers`, the destination, the two rendezvous tiles, the
//! squared loading distance; the garrison's upgrade, cost and valid flag) and every body (the constructors' tile
//! and distance work, `GetDestination`, `Step`, the triggers, `LoadUnit`, `ReplaceUnit`, `Abandon`, the copies, the
//! checksum, the texts, `Save`, `Load`, `RepairPointers`) by the goal's object. The C++ classes are gone: a load
//! goal is an `EEUGRust` of either kind (ee/EEUGRust.h) over the Rust goals' host (rust_goal.rs); the transport
//! classes' tile searches and tests, the garrison and cargo calls, the deposit, the upgrade's replacement, the tech
//! tree's upgrade lookup and the sounds stay C++ host operations.
//!
//! Determinism (RUST_PLAN 2.1): the distances are the original's `f32` expressions (`DistanceSquaredTo`: y
//! difference first, then `dx * dx + dy * dy`; `DistanceSquaredToTile`: x part first, the tile converted to `float`);
//! the transport's loading distance is `UMCeil(UMSquare((float)((double)range + (double)half_sphere)))`, `UMCeil` and
//! `UMFloor` over `lrintf`; the waiting animation's draw is the physics generator's, made where the original made it.

use std::collections::BTreeMap;
use std::sync::Mutex;

use crate::physics::lrintf;
use crate::rust_goal::{base_condition, hex8, EeUnitsGoalIo, GoalHost, Held, TRIGGER_UNIT_FACING_NOT_REQUIRED};

/// `kGoalLoadTransport`, `kGoalLoadGarrison`
pub(crate) const GOAL_LOAD_TRANSPORT: u8 = 7;
pub(crate) const GOAL_LOAD_GARRISON: u8 = 12;

const ACTION_IDLE: i64 = 5000;
const ACTION_REACQUIRE_GOAL: i64 = 5001;
const ACTION_PREPARE_TO_MOVE: i64 = 8000;
const ACTION_LOAD_INTO_CONTAINER: i64 = 12000;
const ACTION_BUMP_EXTRA_LOADABLE_UNITS: i64 = 12006;
const ACTION_GARRISON_UPGRADE: i64 = 12007;
const ACTION_WAIT_FOR_GARRISON_COST: i64 = 12008;
const ACTION_WAIT_FOR_PASSENGERS: i64 = 12010;
const ACTION_WAIT_FOR_TRANSPORT: i64 = 12011;
const ACTION_CANNOT_REACH_CONTAINER: i64 = 12012;

const TRIGGER_NO_MORE_WAYPOINTS: i64 = 9006;
const TRIGGER_COMPLETED_UNIT_FACING: i64 = 9015;
const TRIGGER_GOAL_IS_LOAD_CONTAINER: i64 = 12000;
const TRIGGER_CONTAINER_IN_RANGE: i64 = 12002;
const TRIGGER_CONTAINER_IS_FULL: i64 = 12003;
const TRIGGER_CONTAINER_IS_DEAD: i64 = 12004;
const TRIGGER_CONTAINER_IS_IDLE: i64 = 12005;
const TRIGGER_NO_MORE_LOADABLE_UNITS_TO_PICK_UP: i64 = 12006;
const TRIGGER_EXTRA_LOADABLE_UNITS_BUMPED: i64 = 12008;
const TRIGGER_GARRISON_COST_MET: i64 = 12009;
const TRIGGER_GOAL_IS_LOAD_GARRISON: i64 = 12010;
const TRIGGER_HAVE_REACHED_RENDEZVOUS_POINT: i64 = 12011;
const TRIGGER_HAVE_NOT_REACHED_RENDEZVOUS_POINT: i64 = 12012;
const TRIGGER_MORE_LOADABLE_UNITS_TO_PICK_UP: i64 = 12013;
const TRIGGER_CONTAINER_HAS_LOST_GOAL: i64 = 12015;
const TRIGGER_CONTAINER_HAS_NO_MORE_WAYPOINTS: i64 = 12016;
/// the event 0x2ee7 (no registered name): the garrison's "building alive, unit dead", the transport's "transport
/// alive and carrying the unit"
const EVENT_CARRIED: i64 = 0x2ee7;

const UNIT_TYPE_CAPITOL: i64 = 0x29;
const UNIT_TYPE_GRANARY_IMPROVED: i64 = 0x2ba;
/// `kSoundGarrison`, `kSoundTransportLoad`
const SOUND_GARRISON: i64 = 0x86;
const SOUND_TRANSPORT_LOAD: i64 = 0x183;
/// `LoadCargo`'s refusal
const CARGO_REFUSED: i64 = 7;
/// the save version before which one point stood for both rendezvous tiles (0.221)
const VERSION_TWO_TILES: u32 = 0xdd;
/// `EEUnitClassCast<EEUCTransport>` (rust_goal's transport kind bit)
const TRANSPORT_CLASS: i64 = 1;

type Tile = (i32, i32);

/// One load goal's state.
#[derive(Clone, Debug, PartialEq)]
struct Load {
    kind: u8,
    container: Held,
    /// the units to load (each referenced once)
    units: Vec<u64>,
    /// their saved ids, from `Load` to `RepairPointers`
    unit_ids: Vec<u32>,
    destination: [f32; 3],
    container_tile: Tile,
    unit_tile: Tile,
    distance: f32,
    /// the garrison's
    upgrade: i32,
    cost: i32,
    valid: bool,
}

impl Load {
    fn empty(kind: u8) -> Load {
        Load {
            kind,
            container: Held::None,
            units: Vec::new(),
            unit_ids: Vec::new(),
            destination: [0.0; 3],
            container_tile: (0, 0),
            unit_tile: (0, 0),
            distance: 0.0,
            upgrade: 0,
            cost: 0,
            valid: false,
        }
    }
    fn garrison(&self) -> bool {
        self.kind == GOAL_LOAD_GARRISON
    }
}

static LOADS: Mutex<BTreeMap<u64, Load>> = Mutex::new(BTreeMap::new());

fn get(goal: u64) -> Option<Load> {
    let goals = LOADS.lock().unwrap_or_else(|e| e.into_inner());
    goals.get(&goal).cloned()
}

fn get_or(goal: u64, kind: u8) -> Load {
    get(goal).unwrap_or_else(|| Load::empty(kind))
}

fn put(goal: u64, state: Load) {
    let mut goals = LOADS.lock().unwrap_or_else(|e| e.into_inner());
    goals.insert(goal, state);
}

/// What a container's class answers (`mUnitClass`'s fields).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LoadClass {
    pub garrison_capacity: i32,
    pub transport_capacity: i32,
    pub weight: i32,
    pub garrison_technology: i32,
    pub unit_type: i64,
    pub can_garrison: bool,
    pub tile_size: i32,
    pub load_range: f32,
    pub inner_sphere: f32,
}

/// A unit's own fields the goals read.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LoadUnit {
    pub garrison_count: i32,
    pub cargo_room: i32,
    pub hit_points: i32,
    pub disabled: bool,
    pub no_waypoints: bool,
    pub animation: i64,
}

/// The world calls of the load goals (the FFI adapter; the tests' mock).
pub trait LoadHost: GoalHost {
    /// the unit's class's fields; the unit's
    fn load_class(&mut self, unit: u64) -> LoadClass;
    fn load_unit(&mut self, unit: u64) -> LoadUnit;
    /// `mPhysicalModel.GetLocation()`'s translation
    fn unit_location(&mut self, unit: u64) -> [f32; 3];
    /// `EEIsLineBlocked(from, to, ...)`
    fn line_blocked(&mut self, from: Tile, to: Tile) -> bool;
    /// which of the transport classes the unit's class is (rust_goal's `TRANSPORT_KIND` bits)
    fn transport_kind(&mut self, unit: u64) -> i64;
    /// the transport class's `CanLoad(container, unit)` (false for another class)
    fn can_load(&mut self, container: u64, unit: u64) -> bool;
    /// the transport class's `FindBoardingLocation(unit, container, location, out)` (`out` in and out)
    fn boarding_location(&mut self, unit: u64, container: u64, location: [f32; 3], out: &mut [f32; 3]);
    /// `EEGetTilesAroundUnit(unit, container, 1, 0, 0, tiles)` then `EEPickTileNear(tiles, unit, container, 0, out)`
    fn pick_tile_near(&mut self, unit: u64, container: u64, out: &mut [f32; 3]) -> bool;
    /// the transport class's `FindRendezvousTiles(units, container, &tiles.0, &tiles.1)` (its base class's when
    /// `base`; the tiles in and out)
    fn rendezvous_tiles(&mut self, container: u64, units: &[u64], base: bool, tiles: &mut [Tile; 2]);
    /// the container's `AddCenterOffset(x, y)` (in and out)
    fn center_offset(&mut self, container: u64, point: &mut [f32; 2]);
    /// the garrison technology's upgrade: whether its tech node exists, the event's new unit class, whether that
    /// class exists and its node is available
    fn garrison_upgrade(&mut self, container: u64, technology: i32) -> (bool, i32, bool);
    /// the unit's `GetGoal()` (`goal_state`) or `GetLoadGoal()`: the goal and its kind (-1: none)
    fn unit_goal(&mut self, unit: u64, goal_state: bool) -> (u64, i64);
    /// the kind of the unit's goal queue `index`'s next goal (-1: none); the unit's `ClearGoals(index)`
    fn queued_goal_kind(&mut self, unit: u64, index: i64) -> i64;
    fn clear_goals(&mut self, unit: u64, index: i64);
    /// the unit's `mContainer`
    fn holder(&mut self, unit: u64) -> u64;
    /// the unit drops its resource at the container when it carries some the container takes
    fn deposit(&mut self, unit: u64, container: u64);
    /// the container's `GarrisonUnit(unit)`, `LoadCargo(unit, 0, 0)`, `HasCargo(unit)`
    fn garrison_unit(&mut self, container: u64, unit: u64) -> bool;
    fn load_cargo(&mut self, container: u64, unit: u64) -> i64;
    fn has_cargo(&mut self, container: u64, unit: u64) -> bool;
    /// a sound at the unit (`EESoundObject::Get(id)` with the class's parameters)
    fn play_sound(&mut self, unit: u64, sound: i64);
    /// `EEReplaceUnit(container, its player, gUnitClassTable[class])` when that class exists
    fn replace_class(&mut self, container: u64, class: i32);
    /// `gPhysics->mRandom.Generate()`; the unit's `mStats->mCanFireOverWalls`
    fn generate(&mut self) -> f32;
    fn fires_over_walls(&mut self, unit: u64) -> bool;
    /// whether the two units' players are one; `mHitPoints`; `mComplexUnit`
    fn same_player(&mut self, unit: u64, other: u64) -> bool;
    fn hit_points(&mut self, unit: u64) -> i32;
    fn complex_unit(&mut self, unit: u64) -> u64;
    /// the typed `float` file calls
    fn write_f32(&mut self, value: f32) -> i32;
    fn read_f32(&mut self, value: &mut f32) -> i32;
}

/// `UMFloor`: rounds to nearest, then steps down when that went up.
fn um_floor(value: f32) -> i32 {
    let result = lrintf(value);
    if (result as f32) > value {
        result.wrapping_sub(1)
    } else {
        result
    }
}

/// `UMCeil`
fn um_ceil(value: f32) -> i32 {
    let result = lrintf(value);
    if (result as f32) < value {
        result.wrapping_add(1)
    } else {
        result
    }
}

/// `DistanceSquaredTo`: from the container to the unit, y difference first
fn distance_squared(a: [f32; 3], b: [f32; 3]) -> f32 {
    let dy = a[1] - b[1];
    let dx = a[0] - b[0];
    dx * dx + dy * dy
}

/// `DistanceSquaredToTile`
fn distance_squared_to_tile(p: [f32; 3], t: Tile) -> f32 {
    let dx = p[0] - t.0 as f32;
    let dy = p[1] - t.1 as f32;
    dx * dx + dy * dy
}

// ---- the unit list (UReferenceContainer: a linear find, each unit referenced once)

/// `AddUnit`: a unit not yet listed, referenced.
fn add_unit(goal: u64, kind: u8, unit: u64, host: &mut dyn LoadHost) {
    let mut l = get_or(goal, kind);
    if l.units.contains(&unit) {
        return;
    }
    l.units.push(unit);
    put(goal, l);
    host.reference(unit, false);
}

/// `RemoveUnit`: a listed unit dropped and released.
fn remove_unit(goal: u64, unit: u64, host: &mut dyn LoadHost) {
    let Some(mut l) = get(goal) else { return };
    let Some(at) = l.units.iter().position(|u| *u == unit) else { return };
    l.units.remove(at);
    put(goal, l);
    host.dereference(unit, false);
}

/// `ClearUnits`: every unit released, the list emptied.
fn clear_units(goal: u64, host: &mut dyn LoadHost) {
    let Some(mut l) = get(goal) else { return };
    let units = std::mem::take(&mut l.units);
    put(goal, l);
    for u in units {
        host.dereference(u, false);
    }
}

fn has_unit(goal: u64, unit: u64) -> bool {
    get(goal).is_some_and(|l| l.units.contains(&unit))
}

/// A goal object that is a load goal (by its kind).
fn is_load(kind: i64) -> bool {
    kind == i64::from(GOAL_LOAD_TRANSPORT) || kind == i64::from(GOAL_LOAD_GARRISON)
}

// ---- the capacities (the classes' virtuals)

/// `GetFreeCapacity`: the garrison's free places, the transport's cargo room.
fn free_capacity(l: &Load, host: &mut dyn LoadHost) -> i32 {
    let c = l.container.object();
    let unit = host.load_unit(c);
    if l.garrison() {
        host.load_class(c).garrison_capacity.wrapping_sub(unit.garrison_count)
    } else {
        unit.cargo_room
    }
}

/// `GetRemainingLoad`: the garrison's cost less the garrisoned, the transport's units' weights.
fn remaining_load(l: &Load, host: &mut dyn LoadHost) -> i32 {
    if l.garrison() {
        return l.cost.wrapping_sub(host.load_unit(l.container.object()).garrison_count);
    }
    l.units.iter().fold(0i32, |s, u| s.wrapping_add(host.load_class(*u).weight))
}

/// `CanContain`: a garrison takes every unit; a transport carries the unit.
fn can_contain(l: &Load, unit: u64, host: &mut dyn LoadHost) -> bool {
    if l.garrison() {
        return true;
    }
    match l.container {
        Held::Object(c) => host.has_cargo(c, unit),
        _ => false,
    }
}

/// `IsLoadable`: a listed unit within loading distance of the container with a clear line to it.
fn is_loadable(goal: u64, unit: u64, host: &mut dyn LoadHost) -> bool {
    let Some(l) = get(goal) else { return false };
    let Held::Object(c) = l.container else { return false };
    if !l.units.contains(&unit) || unit == 0 {
        return false;
    }
    let (cl, ul) = (host.unit_location(c), host.unit_location(unit));
    if distance_squared(cl, ul) > l.distance {
        return false;
    }
    let (ux, uy) = host.unit_tile(unit);
    let (cx, cy) = host.unit_tile(c);
    !host.line_blocked((ux as i32, uy as i32), (cx as i32, cy as i32))
}

// ---- creation

/// The garrison's `ComputeRendezvous`: the building's tile for both, the destination its entrance point.
fn garrison_rendezvous(l: &mut Load, host: &mut dyn LoadHost) {
    let c = l.container.object();
    let (x, y) = host.unit_tile(c);
    let mut point = [x as i32 as f32, y as i32 as f32];
    host.center_offset(c, &mut point);
    l.container_tile = (x as i32, y as i32);
    l.unit_tile = l.container_tile;
    l.destination[0] = point[0];
    l.destination[1] = point[1];
}

/// The garrison's `ComputeLoadDistance`: (1.2 x the building's tile size, at least 2) squared.
fn garrison_distance(l: &mut Load, host: &mut dyn LoadHost) {
    let d = host.load_class(l.container.object()).tile_size as f32 * 1.2f32;
    let m = if 2.0f32 > d { 2.0f32 } else { d };
    l.distance = m * m;
}

/// The garrison's `ComputeUpgrade`: the class the garrison technology turns the building into; valid (cost the
/// capacity) when it is available, or with no upgrade for a capitol or an improved granary.
fn garrison_upgrade(l: &mut Load, host: &mut dyn LoadHost) {
    let c = l.container.object();
    let class = host.load_class(c);
    l.valid = false;
    let (node, upgrade, available) = host.garrison_upgrade(c, class.garrison_technology);
    if node {
        l.upgrade = upgrade;
        if available {
            l.cost = class.garrison_capacity;
            l.valid = true;
        }
    } else if class.unit_type == UNIT_TYPE_CAPITOL || class.unit_type == UNIT_TYPE_GRANARY_IMPROVED {
        l.upgrade = 0;
        l.valid = true;
        l.cost = class.garrison_capacity;
    }
}

/// The garrison's `Initialize`.
fn garrison_initialize(l: &mut Load, host: &mut dyn LoadHost) {
    garrison_rendezvous(l, host);
    garrison_distance(l, host);
    garrison_upgrade(l, host);
}

/// The transport's `ComputeRendezvous`: its class picks both tiles (its base class's rule without units).
fn transport_rendezvous(l: &mut Load, host: &mut dyn LoadHost) {
    let mut tiles = [l.container_tile, l.unit_tile];
    host.rendezvous_tiles(l.container.object(), &l.units, l.units.is_empty(), &mut tiles);
    (l.container_tile, l.unit_tile) = (tiles[0], tiles[1]);
}

/// The transport's `ComputeLoadDistance`: the load range plus half the first unit's inner sphere, squared, rounded
/// up.
fn transport_distance(l: &mut Load, host: &mut dyn LoadHost) {
    let half = match l.units.first() {
        Some(u) => host.load_class(*u).inner_sphere * 0.5f32,
        None => 0.0f32,
    };
    let range = host.load_class(l.container.object()).load_range;
    let sum = (f64::from(range) + f64::from(half)) as f32;
    l.distance = um_ceil(sum * sum) as f32;
}

/// The list constructors (`EEUGLoadTransport` / `EEUGLoadGarrison(container, units, arg)`; the C++ sorted the list
/// by id and dropped its duplicates): the container referenced, the units added, then the class's rendezvous and
/// distance (and the garrison's upgrade).
pub fn create(goal: u64, kind: u8, container: u64, units: &[u64], host: &mut dyn LoadHost) {
    put(goal, Load { container: Held::of(container), ..Load::empty(kind) });
    host.reference(container, false);
    for u in units {
        add_unit(goal, kind, *u, host);
    }
    let mut l = get_or(goal, kind);
    if kind == GOAL_LOAD_GARRISON {
        garrison_initialize(&mut l, host);
    } else {
        transport_rendezvous(&mut l, host);
        transport_distance(&mut l, host);
    }
    put(goal, l);
}

/// `Clone`: the copy constructors (the container and units referenced; the garrison's destination its container
/// tile, the transport's none).
fn clone(goal: u64, arg: bool, host: &mut dyn LoadHost) -> u64 {
    let l = get_or(goal, GOAL_LOAD_TRANSPORT);
    let copy = host.new_goal(l.kind, arg);
    let mut c = Load::empty(l.kind);
    c.container = Held::of(l.container.object());
    c.distance = l.distance;
    put(copy, c);
    host.reference(l.container.object(), false);
    for u in &l.units {
        add_unit(copy, l.kind, *u, host);
    }
    let mut c = get_or(copy, l.kind);
    c.container_tile = l.container_tile;
    c.unit_tile = l.unit_tile;
    if l.garrison() {
        (c.upgrade, c.cost, c.valid) = (l.upgrade, l.cost, l.valid);
        c.destination = [l.container_tile.0 as f32, l.container_tile.1 as f32, 0.0];
    }
    put(copy, c);
    copy
}

/// The destructor: the container released, then every unit.
pub fn destroy(goal: u64, host: &mut dyn LoadHost) {
    let Some(l) = LOADS.lock().unwrap_or_else(|e| e.into_inner()).remove(&goal) else { return };
    if let Held::Object(c) = l.container {
        host.dereference(c, false);
    }
    for u in l.units {
        host.dereference(u, false);
    }
}

// ---- the bodies

/// `GetDestination`: the container goes to its rendezvous tile; a unit ready to board stays where it is, another
/// goes to the transport's boarding spot (or a free tile by another container, else its rendezvous tile); the
/// unit's active group told.
fn destination(goal: u64, unit: u64, out: &mut [f32; 3], host: &mut dyn LoadHost) {
    let Some(l) = get(goal) else { return };
    let Held::Object(c) = l.container else { return };
    let transport = host.transport_kind(c) & TRANSPORT_CLASS != 0;
    let tile = if unit == c { l.container_tile } else { l.unit_tile };
    let location = host.tile_location(i64::from(tile.0), i64::from(tile.1));
    if unit == c {
        *out = location;
    } else if is_loadable(goal, unit, host) && transport && host.can_load(c, unit) {
        *out = host.unit_location(unit);
    } else if transport {
        host.boarding_location(unit, c, location, out);
    } else if !host.pick_tile_near(unit, c, out) {
        *out = location;
    }
    if let Some(mut l) = get(goal) {
        l.destination = *out;
        put(goal, l);
    }
    host.tell_group(unit, *out);
}

/// `LoadUnit`: a listed unit leaves the container's own goal and boards (a garrison: drops its resource first;
/// refused, or the garrison's cost met: it idles), with the class's sound; true when it boarded.
fn load_unit(goal: u64, unit: u64, host: &mut dyn LoadHost) -> bool {
    let Some(l) = get(goal) else { return false };
    let Held::Object(c) = l.container else { return false };
    if unit == 0 || !l.units.contains(&unit) {
        return false;
    }
    let (own, own_kind) = host.unit_goal(c, true);
    if own_kind == i64::from(l.kind) {
        remove_unit(own, unit, host);
    }
    let boarded = if l.garrison() {
        host.deposit(unit, c);
        let refused = l.cost == host.load_unit(c).garrison_count || !host.garrison_unit(c, unit);
        !(refused && host.hit_points(unit) > 0)
    } else {
        host.load_cargo(c, unit) != CARGO_REFUSED
    };
    if !boarded {
        host.start_action(unit, ACTION_IDLE);
        return false;
    }
    host.play_sound(c, if l.garrison() { SOUND_GARRISON } else { SOUND_TRANSPORT_LOAD });
    true
}

/// `Step`: on the unit's action, the base's loading, bumping, giving up and waiting; the garrison's upgrade.
pub fn step(goal: u64, unit: u64, host: &mut dyn LoadHost) {
    let Some(l) = get(goal) else { return };
    let command = host.unit_command(unit);
    if l.garrison() {
        match command {
            ACTION_GARRISON_UPGRADE => {
                if l.upgrade != 0 {
                    host.replace_class(l.container.object(), l.upgrade);
                }
                return;
            }
            ACTION_WAIT_FOR_GARRISON_COST => return,
            _ => {}
        }
    }
    match command {
        ACTION_LOAD_INTO_CONTAINER => {
            if is_loadable(goal, unit, host) {
                load_unit(goal, unit, host);
            }
        }
        ACTION_BUMP_EXTRA_LOADABLE_UNITS => {
            if free_capacity(&l, host) == 0 {
                clear_units(goal, host);
            }
        }
        ACTION_CANNOT_REACH_CONTAINER => {
            let (other, kind) = host.unit_goal(l.container.object(), false);
            if is_load(kind) {
                remove_unit(other, unit, host);
            }
            remove_unit(goal, unit, host);
        }
        ACTION_REACQUIRE_GOAL | ACTION_WAIT_FOR_PASSENGERS | ACTION_WAIT_FOR_TRANSPORT => {
            host.set_physics_action(unit, 3);
            let animation = host.load_unit(unit).animation;
            if animation != 7 && animation != 8 {
                let eight = host.generate() >= 0.5f32 && !host.fires_over_walls(unit);
                host.set_animation(unit, if eight { 8 } else { 7 });
            }
        }
        _ => {}
    }
}

/// The base's triggers (`EEUGLoadContainer::EvaluateCondition`).
fn base_evaluate(goal: u64, unit: u64, event: i64, host: &mut dyn LoadHost) -> bool {
    let Some(l) = get(goal) else { return false };
    let c = l.container.object();
    match event {
        TRIGGER_CONTAINER_HAS_LOST_GOAL => {
            let (other, kind) = host.unit_goal(c, false);
            !(is_load(kind) && has_unit(other, unit))
        }
        TRIGGER_CONTAINER_HAS_NO_MORE_WAYPOINTS => {
            let u = host.load_unit(c);
            u.hit_points > 0 && u.no_waypoints
        }
        TRIGGER_CONTAINER_IS_DEAD => {
            c == 0 || {
                let u = host.load_unit(c);
                u.hit_points <= 0 || u.disabled
            }
        }
        TRIGGER_CONTAINER_IS_FULL => free_capacity(&l, host) <= 0,
        TRIGGER_CONTAINER_IS_IDLE => host.load_unit(c).hit_points > 0 && host.unit_command(c) == ACTION_IDLE,
        TRIGGER_EXTRA_LOADABLE_UNITS_BUMPED => remaining_load(&l, host) == 0,
        TRIGGER_MORE_LOADABLE_UNITS_TO_PICK_UP => free_capacity(&l, host) > 0 && !l.units.is_empty(),
        TRIGGER_NO_MORE_LOADABLE_UNITS_TO_PICK_UP => free_capacity(&l, host) == 0 || l.units.is_empty(),
        TRIGGER_HAVE_REACHED_RENDEZVOUS_POINT | TRIGGER_HAVE_NOT_REACHED_RENDEZVOUS_POINT => {
            let tile = if unit == c { l.container_tile } else { l.unit_tile };
            let d = distance_squared_to_tile(host.unit_location(unit), tile);
            if event == TRIGGER_HAVE_REACHED_RENDEZVOUS_POINT {
                d <= 1.0f32
            } else {
                d > 1.0f32
            }
        }
        _ => base_condition(event),
    }
}

/// `EvaluateCondition`: the class's triggers, then the base's.
pub fn evaluate(goal: u64, unit: u64, event: i64, host: &mut dyn LoadHost) -> bool {
    let Some(l) = get(goal) else { return false };
    let c = l.container.object();
    if l.garrison() {
        match event {
            TRIGGER_CONTAINER_IN_RANGE => {
                return !(unit == c
                    || host.load_unit(c).hit_points <= 0
                    || !is_loadable(goal, unit, host)
                    || !host.load_class(unit).can_garrison
                    || host.unit_command(c) == ACTION_GARRISON_UPGRADE);
            }
            TRIGGER_GARRISON_COST_MET => return l.cost == host.load_unit(c).garrison_count,
            TRIGGER_GOAL_IS_LOAD_GARRISON | TRIGGER_GOAL_IS_LOAD_CONTAINER => return true,
            EVENT_CARRIED => return host.load_unit(c).hit_points > 0 && host.hit_points(unit) <= 0,
            TRIGGER_COMPLETED_UNIT_FACING | TRIGGER_UNIT_FACING_NOT_REQUIRED => return true,
            TRIGGER_NO_MORE_WAYPOINTS => return false,
            _ => {}
        }
    } else {
        match event {
            TRIGGER_GOAL_IS_LOAD_CONTAINER => return true,
            TRIGGER_CONTAINER_IN_RANGE => {
                return unit != c
                    && c != 0
                    && host.load_unit(c).hit_points > 0
                    && is_loadable(goal, unit, host)
                    && host.can_load(c, unit);
            }
            EVENT_CARRIED => return c != 0 && host.load_unit(c).hit_points > 0 && can_contain(&l, unit, host),
            _ => {}
        }
    }
    base_evaluate(goal, unit, event, host)
}

/// `ReplaceUnit`: a replaced container of the same player is followed (the garrison's: unless the unit's next goal
/// is a garrison, which is cleared instead; then its rendezvous, distance and upgrade again); a replaced listed
/// unit is swapped (dropped when the new one changed player).
pub fn replace_unit(goal: u64, old: u64, new: u64, owner: u64, host: &mut dyn LoadHost) {
    let Some(l) = get(goal) else { return };
    let same = host.same_player(old, new);
    if l.container.object() == old && old != 0 {
        if l.garrison() {
            if owner != new && host.queued_goal_kind(owner, 1) == i64::from(GOAL_LOAD_GARRISON) {
                host.clear_goals(owner, 1);
                return;
            }
            if same && new != owner {
                host.dereference(old, false);
                let mut l = get_or(goal, GOAL_LOAD_GARRISON);
                l.container = Held::of(new);
                put(goal, l);
                host.reference(new, false);
                let mut l = get_or(goal, GOAL_LOAD_GARRISON);
                garrison_initialize(&mut l, host);
                put(goal, l);
            }
        } else if same {
            host.dereference(old, false);
            let mut l = get_or(goal, GOAL_LOAD_TRANSPORT);
            l.container = Held::of(new);
            put(goal, l);
            host.reference(new, false);
        }
    }
    if has_unit(goal, old) {
        if same {
            add_unit(goal, l.kind, new, host);
        }
        remove_unit(goal, old, host);
    }
}

/// `Abandon`: a unit that is neither the container nor loaded leaves the container's own load goal.
fn abandon(goal: u64, unit: u64, host: &mut dyn LoadHost) {
    let Some(l) = get(goal) else { return };
    let Held::Object(c) = l.container else { return };
    if unit == c || host.holder(unit) != 0 {
        return;
    }
    let (other, kind) = host.unit_goal(c, false);
    if is_load(kind) && has_unit(other, unit) {
        remove_unit(other, unit, host);
    }
}

/// `Start`: the garrison starts a living unit preparing to move; the transport the base goal's start (its action).
fn start(goal: u64, unit: u64, host: &mut dyn LoadHost) {
    let Some(l) = get(goal) else { return };
    if !l.garrison() || host.hit_points(unit) > 0 {
        host.start_action(unit, ACTION_PREPARE_TO_MOVE);
    }
}

/// The checksum's part: the destination's bits, the unit count, the container's id, the tiles, the distance's bits
/// (the garrison's valid flag, cost and upgrade).
pub fn checksum(goal: u64, host: &mut dyn LoadHost) -> i64 {
    let Some(l) = get(goal) else { return 0 };
    let id = match l.container {
        Held::Object(c) => host.unit_id(c),
        _ => 0,
    };
    let mut sum = l.destination.iter().fold(0u32, |s, v| s.wrapping_add(v.to_bits()));
    sum = sum.wrapping_add(l.units.len() as u32);
    for v in [l.unit_tile.1, l.unit_tile.0, l.container_tile.1, l.container_tile.0] {
        sum = sum.wrapping_add(v as u32);
    }
    sum = sum.wrapping_add(id).wrapping_add(l.distance.to_bits());
    if l.garrison() {
        sum = sum.wrapping_add(u32::from(l.valid)).wrapping_add(l.cost as u32).wrapping_add(l.upgrade as u32);
    }
    i64::from(sum as i32)
}

/// The out-of-synch dump (the garrison's lines, then the base's).
pub fn dump(goal: u64, host: &mut dyn LoadHost) {
    let Some(l) = get(goal) else { return };
    if l.garrison() {
        let g = "OUT OF SYNCH DUMP:  EEUGLoadGarrison - ";
        host.append(&format!("{g}Garrison upgrade is {}\n", l.upgrade));
        host.append(&format!("{g}Garrison Cost is {}\n", l.cost));
        host.append(&format!("{g}Is Valid is {}\n", i32::from(l.valid)));
    }
    let b = "OUT OF SYNCH DUMP:  EEUGLoadContainer - ";
    for (axis, v) in ["X", "Y", "Z"].iter().zip(l.destination) {
        host.append(&format!("{b}Actual Destination {axis}: {:6.6}\n", f64::from(v)));
    }
    host.append(&format!("{b}Loadable Unit List Size is {}\n", l.units.len()));
    let id = match l.container {
        Held::Object(c) => host.unit_id(c),
        _ => u32::MAX,
    };
    host.append(&format!("{b}Container Unit ID is #{}\n", hex8(id)));
    host.append(&format!("{b}Container Rendevous Tile X is {}\n", l.container_tile.0));
    host.append(&format!("{b}Container Rendevous Tile Y is {}\n", l.container_tile.1));
    host.append(&format!("{b}Loadable Unit Rendevous Tile X is {}\n", l.unit_tile.0));
    host.append(&format!("{b}Loadable Unit Rendevous Tile Y is {}\n", l.unit_tile.1));
    host.append(&format!("{b}Minimum Loading Distance Squared is {:6.6}\n", f64::from(l.distance)));
}

/// `GetDebugLine`
pub fn debug_line(goal: u64, host: &mut dyn LoadHost) {
    let garrison = get(goal).is_some_and(|l| l.garrison());
    host.append(if garrison { "Load Garrison" } else { "Load Transport" });
}

/// `GetDebugText`: the class's header, the container, the passengers, the tiles and the distance (the base's own
/// header was formatted, never appended).
pub fn debug_text(goal: u64, host: &mut dyn LoadHost) {
    let Some(l) = get(goal) else { return };
    host.append(if l.garrison() { "Load Garrison - Part of:" } else { "Load Transport. Part of " });
    if let Held::Object(c) = l.container {
        host.append("Container: ");
        host.append_class_name(c);
        let (x, y) = host.unit_tile(c);
        host.append(&format!(", ({x},{y})\n"));
    }
    host.append(&format!("Passengers: {}\n", l.units.len()));
    for u in &l.units {
        host.append_class_name(*u);
        let (x, y) = host.unit_tile(*u);
        host.append(&format!(" at {x},{y}\n"));
    }
    host.append(&format!(
        "ContainerRendezvous:({},{})\nLoadableUnitRendezvous:({},{})\nMinLoadingDistance:{:.4}\n",
        l.container_tile.0,
        l.container_tile.1,
        l.unit_tile.0,
        l.unit_tile.1,
        f64::from(l.distance)
    ));
}

/// `U2DPoint<long>::Save`
fn save_tile(t: Tile, host: &mut dyn LoadHost) -> i32 {
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
fn load_tile(t: &mut Tile, host: &mut dyn LoadHost) -> i32 {
    if !host.file_open() {
        return 4;
    }
    let result = host.read_i32(&mut t.0);
    if result != 0 {
        return result;
    }
    host.read_i32(&mut t.1)
}

/// `GE3DPoint::Save` / `GE2DPoint::Load` and `GE3DPoint::Load` (the axes read so far kept)
fn save_point(p: [f32; 3], host: &mut dyn LoadHost) -> i32 {
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

fn load_point(p: &mut [f32], host: &mut dyn LoadHost) -> i32 {
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

/// `Save`'s part after the base goal's: the container's id, the units' count and ids, the distance, the
/// destination, the two tiles (the garrison's cost, upgrade and valid flag); the first failing status.
pub fn save(goal: u64, host: &mut dyn LoadHost) -> i32 {
    let Some(l) = get(goal) else { return 0 };
    // (the original read the container unguarded: a goal whose container was not found on load writes -1)
    let id = match l.container {
        Held::Object(c) => host.unit_id(c),
        _ => u32::MAX,
    };
    let mut result = host.write_u32(id);
    if result != 0 {
        return result;
    }
    result = host.write_u32(l.units.len() as u32);
    if result != 0 {
        return result;
    }
    for u in &l.units {
        let id = host.unit_id(*u);
        result = host.write_u32(id);
        if result != 0 {
            return result;
        }
    }
    result = host.write_f32(l.distance);
    if result != 0 {
        return result;
    }
    result = save_point(l.destination, host);
    if result != 0 {
        return result;
    }
    result = save_tile(l.container_tile, host);
    if result == 0 {
        result = save_tile(l.unit_tile, host);
    }
    if result != 0 || !l.garrison() {
        return result;
    }
    result = host.write_i32(l.cost);
    if result != 0 {
        return result;
    }
    result = host.write_i32(l.upgrade);
    if result == 0 {
        result = host.write_bool(l.valid);
    }
    result
}

/// `Load`'s part after the base goal's: the container's and units' ids (resolved by `repair`), the distance, and
/// before 0.221 one point for both tiles (floored) then the destination, after it the destination and the two
/// tiles (the garrison's cost, upgrade and valid flag); the first failing status.
pub fn load(goal: u64, kind: u8, major: u32, minor: u32, host: &mut dyn LoadHost) -> i32 {
    let mut id = u32::MAX;
    let mut result = host.read_u32(&mut id);
    let mut l = get_or(goal, kind);
    l.container = Held::Saved(id);
    put(goal, l);
    if result == 0 {
        let mut count = 0u32;
        result = host.read_u32(&mut count);
        if result == 0 {
            clear_units(goal, host);
            let mut l = get_or(goal, kind);
            for _ in 0..count {
                if result != 0 {
                    break;
                }
                let mut id = u32::MAX;
                result = host.read_u32(&mut id);
                if result == 0 {
                    l.unit_ids.push(id);
                }
            }
            if result == 0 {
                result = host.read_f32(&mut l.distance);
            }
            put(goal, l);
        }
    }
    let mut l = get_or(goal, kind);
    if major == 0 && minor < VERSION_TWO_TILES {
        if result == 0 {
            let mut point = [0.0f32; 2];
            result = load_point(&mut point, host);
            l.container_tile = (um_floor(point[0]), um_floor(point[1]));
            l.unit_tile = l.container_tile;
            if result == 0 {
                result = load_point(&mut l.destination, host);
            }
        }
    } else if result == 0 {
        result = load_point(&mut l.destination, host);
        if result == 0 {
            result = load_tile(&mut l.container_tile, host);
            if result == 0 {
                result = load_tile(&mut l.unit_tile, host);
            }
        }
    }
    if result == 0 && l.garrison() {
        result = host.read_i32(&mut l.cost);
        if result == 0 {
            result = host.read_i32(&mut l.upgrade);
            if result == 0 {
                result = host.read_bool(&mut l.valid);
            }
        }
    }
    put(goal, l);
    result
}

/// `RepairPointers`: the container from its id (the unit's complex unit, referenced) and the units found (their
/// complex units, added); 3 and no container when it is gone. The ids are forgotten.
pub fn repair(goal: u64, kind: u8, host: &mut dyn LoadHost) -> i32 {
    let mut l = get_or(goal, kind);
    let ids = std::mem::take(&mut l.unit_ids);
    let unit = host.find_unit(l.container.saved_id(), false);
    if unit == 0 {
        l.container = Held::None;
        put(goal, l);
        return 3;
    }
    let c = host.complex_unit(unit);
    l.container = Held::of(c);
    put(goal, l);
    host.reference(c, false);
    for id in ids {
        let u = host.find_unit(id, false);
        if u == 0 {
            continue;
        }
        let complex = host.complex_unit(u);
        if complex != 0 {
            add_unit(goal, kind, complex, host);
        }
    }
    0
}

/// The goal's body `op` (rust_goal.rs `EE_UNITS_GOAL_*`).
pub fn body(op: i32, kind: u8, goal: u64, a: u64, b: u64, io: &mut EeUnitsGoalIo, host: &mut dyn LoadHost) -> i64 {
    use crate::rust_goal::*;
    match op {
        EE_UNITS_GOAL_NEW_LOAD => {
            let units = if io.list.is_null() {
                Vec::new()
            } else {
                // SAFETY: the C++'s list of `io.list_len` handles, live for the call
                unsafe { std::slice::from_raw_parts(io.list, io.list_len) }.to_vec()
            };
            create(goal, kind, a, &units, host);
        }
        EE_UNITS_GOAL_LOAD_NEW_EMPTY => put(goal, Load::empty(kind)),
        EE_UNITS_GOAL_DESTROY => destroy(goal, host),
        EE_UNITS_GOAL_CLONE => return clone(goal, b != 0, host) as i64,
        EE_UNITS_GOAL_CHECKSUM => return checksum(goal, host),
        EE_UNITS_GOAL_DUMP => dump(goal, host),
        EE_UNITS_GOAL_DEBUG_LINE => debug_line(goal, host),
        EE_UNITS_GOAL_DEBUG_TEXT => debug_text(goal, host),
        EE_UNITS_GOAL_LOCATION => {
            let d = get_or(goal, kind).destination;
            io.f[..3].copy_from_slice(&d);
        }
        EE_UNITS_GOAL_DESTINATION => {
            let mut out = [io.f[0], io.f[1], io.f[2]];
            destination(goal, a, &mut out, host);
            io.f[..3].copy_from_slice(&out);
        }
        EE_UNITS_GOAL_START_ACTION => return ACTION_PREPARE_TO_MOVE,
        EE_UNITS_GOAL_MAX_PATH_ATTEMPTS => return 6,
        EE_UNITS_GOAL_SUITS => return 1,
        EE_UNITS_GOAL_STEP => step(goal, a, host),
        EE_UNITS_GOAL_START => start(goal, a, host),
        EE_UNITS_GOAL_SAVE => return i64::from(save(goal, host)),
        EE_UNITS_GOAL_LOAD => return i64::from(load(goal, kind, a as u32, b as u32, host)),
        EE_UNITS_GOAL_REPAIR => return i64::from(repair(goal, kind, host)),
        EE_UNITS_GOAL_EVALUATE => return i64::from(evaluate(goal, a, b as i64, host)),
        EE_UNITS_GOAL_REPLACE_UNIT => replace_unit(goal, a, b, io.i[0] as u64, host),
        EE_UNITS_GOAL_ADD_CREATED_UNIT | EE_UNITS_GOAL_LOAD_ADD_UNIT => add_unit(goal, kind, a, host),
        EE_UNITS_GOAL_ABANDON => abandon(goal, a, host),
        EE_UNITS_GOAL_LOAD_HAS_UNIT => return i64::from(has_unit(goal, a)),
        EE_UNITS_GOAL_LOAD_UNIT_COUNT => return get(goal).map_or(0, |l| l.units.len() as i64),
        EE_UNITS_GOAL_LOAD_UNIT_AT => {
            return get(goal).and_then(|l| l.units.get(a as usize).copied()).unwrap_or(0) as i64;
        }
        EE_UNITS_GOAL_LOAD_FIELDS => {
            let l = get_or(goal, kind);
            io.i[..6].copy_from_slice(&[
                i64::from(l.container_tile.0),
                i64::from(l.container_tile.1),
                i64::from(l.unit_tile.0),
                i64::from(l.unit_tile.1),
                i64::from(l.valid),
                l.units.len() as i64,
            ]);
            return l.container.object() as i64;
        }
        // Suits: true; SetOwner: the base's (empty)
        _ => {}
    }
    0
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::rust_goal::*;
    use std::collections::BTreeMap as Map;

    /// Units by handle (their class and own fields, locations, tiles), the goals of each unit and a log.
    #[derive(Default)]
    struct World {
        log: Vec<String>,
        text: String,
        file: Vec<String>,
        reads: Vec<String>,
        classes: Map<u64, LoadClass>,
        units: Map<u64, LoadUnit>,
        locations: Map<u64, [f32; 3]>,
        tiles: Map<u64, (i64, i64)>,
        commands: Map<u64, i64>,
        goals: Map<u64, (u64, i64)>,
        refs: Map<u64, i64>,
        transport: bool,
        can_load: bool,
        blocked: bool,
        garrison_ok: bool,
        cargo_result: i64,
        upgrade: (bool, i32, bool),
        random: f32,
        same_player: bool,
        queued_kind: i64,
        next_goal: u64,
    }
    const C: u64 = 0xc00;
    const U1: u64 = 0x100;
    const U2: u64 = 0x200;
    const U3: u64 = 0x300;

    impl GoalHost for World {
        fn reference(&mut self, o: u64, _g: bool) {
            *self.refs.entry(o).or_default() += 1;
        }
        fn dereference(&mut self, o: u64, _g: bool) {
            *self.refs.entry(o).or_default() -= 1;
        }
        fn unit_id(&mut self, u: u64) -> u32 {
            (u >> 8) as u32 | 0x0200_0000
        }
        fn unit_tile(&mut self, u: u64) -> (i64, i64) {
            self.tiles.get(&u).copied().unwrap_or((0, 0))
        }
        fn unit_valid(&mut self, _u: u64) -> bool {
            true
        }
        fn unit_command(&mut self, u: u64) -> i64 {
            self.commands.get(&u).copied().unwrap_or(0)
        }
        fn reset_path_attempts(&mut self, _g: u64) {}
        fn start_action(&mut self, u: u64, a: i64) {
            self.log.push(format!("start {u:x} {a}"));
        }
        fn find_unit(&mut self, id: u32, _complex: bool) -> u64 {
            if id == u32::MAX || id == 0x0200_0099 {
                0
            } else {
                u64::from(id & 0xff_ffff) << 8
            }
        }
        fn unit_position(&mut self, _u: u64) -> [f32; 3] {
            [0.0; 3]
        }
        fn tile_location(&mut self, x: i64, y: i64) -> [f32; 3] {
            [x as f32 + 0.5, y as f32 + 0.5, 1.0]
        }
        fn tell_group(&mut self, u: u64, p: [f32; 3]) {
            self.log.push(format!("tell {u:x} {p:?}"));
        }
        fn set_physics_action(&mut self, u: u64, a: i64) {
            self.log.push(format!("physics {u:x} {a}"));
        }
        fn set_animation(&mut self, u: u64, a: i64) {
            self.log.push(format!("animation {u:x} {a}"));
        }
        fn drop_path(&mut self, _u: u64) {}
        fn new_goal(&mut self, k: u8, a: bool) -> u64 {
            self.log.push(format!("new goal {k} {a}"));
            self.next_goal += 1;
            self.next_goal
        }
        fn file_open(&mut self) -> bool {
            true
        }
        fn write_u32(&mut self, v: u32) -> i32 {
            self.file.push(format!("u{v}"));
            0
        }
        fn write_i32(&mut self, v: i32) -> i32 {
            self.file.push(format!("i{v}"));
            0
        }
        fn write_bool(&mut self, v: bool) -> i32 {
            self.file.push(format!("b{}", u8::from(v)));
            0
        }
        fn read_u32(&mut self, v: &mut u32) -> i32 {
            if self.reads.is_empty() {
                return 5;
            }
            *v = self.reads.remove(0)[1..].parse().unwrap();
            0
        }
        fn read_i32(&mut self, v: &mut i32) -> i32 {
            if self.reads.is_empty() {
                return 5;
            }
            *v = self.reads.remove(0)[1..].parse().unwrap();
            0
        }
        fn read_bool(&mut self, v: &mut bool) -> i32 {
            if self.reads.is_empty() {
                return 5;
            }
            *v = &self.reads.remove(0)[1..] == "1";
            0
        }
        fn append(&mut self, t: &str) {
            self.text.push_str(t);
        }
        fn append_class_name(&mut self, u: u64) {
            self.text.push_str(&format!("class{u:x}"));
        }
    }

    impl LoadHost for World {
        fn load_class(&mut self, u: u64) -> LoadClass {
            self.classes.get(&u).copied().unwrap_or_default()
        }
        fn load_unit(&mut self, u: u64) -> LoadUnit {
            self.units.get(&u).copied().unwrap_or_default()
        }
        fn unit_location(&mut self, u: u64) -> [f32; 3] {
            self.locations.get(&u).copied().unwrap_or([0.0; 3])
        }
        fn line_blocked(&mut self, f: Tile, t: Tile) -> bool {
            self.log.push(format!("line {f:?} {t:?}"));
            self.blocked
        }
        fn transport_kind(&mut self, _u: u64) -> i64 {
            i64::from(self.transport)
        }
        fn can_load(&mut self, c: u64, u: u64) -> bool {
            self.log.push(format!("can load {c:x} {u:x}"));
            self.transport && self.can_load
        }
        fn boarding_location(&mut self, u: u64, c: u64, l: [f32; 3], out: &mut [f32; 3]) {
            self.log.push(format!("boarding {u:x} {c:x} {l:?}"));
            *out = [l[0] + 1.0, l[1], l[2]];
        }
        fn pick_tile_near(&mut self, u: u64, c: u64, out: &mut [f32; 3]) -> bool {
            self.log.push(format!("pick {u:x} {c:x}"));
            *out = [7.0, 7.0, 0.0];
            !self.blocked
        }
        fn rendezvous_tiles(&mut self, c: u64, units: &[u64], base: bool, tiles: &mut [Tile; 2]) {
            self.log.push(format!("rendezvous {c:x} {units:x?} {base}"));
            *tiles = [(30, 31), (32, 33)];
        }
        fn center_offset(&mut self, _c: u64, p: &mut [f32; 2]) {
            p[0] += 0.5;
            p[1] += 1.5;
        }
        fn garrison_upgrade(&mut self, _c: u64, t: i32) -> (bool, i32, bool) {
            self.log.push(format!("upgrade of {t}"));
            self.upgrade
        }
        fn unit_goal(&mut self, u: u64, state: bool) -> (u64, i64) {
            self.log.push(format!("goal of {u:x} {state}"));
            self.goals.get(&u).copied().unwrap_or((0, -1))
        }
        fn queued_goal_kind(&mut self, _u: u64, _i: i64) -> i64 {
            self.queued_kind
        }
        fn clear_goals(&mut self, u: u64, i: i64) {
            self.log.push(format!("clear {u:x} {i}"));
        }
        fn holder(&mut self, _u: u64) -> u64 {
            0
        }
        fn deposit(&mut self, u: u64, c: u64) {
            self.log.push(format!("deposit {u:x} {c:x}"));
        }
        fn garrison_unit(&mut self, c: u64, u: u64) -> bool {
            self.log.push(format!("garrison {c:x} {u:x}"));
            self.garrison_ok
        }
        fn load_cargo(&mut self, c: u64, u: u64) -> i64 {
            self.log.push(format!("cargo {c:x} {u:x}"));
            self.cargo_result
        }
        fn has_cargo(&mut self, _c: u64, u: u64) -> bool {
            u == U1
        }
        fn play_sound(&mut self, u: u64, s: i64) {
            self.log.push(format!("sound {u:x} {s:#x}"));
        }
        fn replace_class(&mut self, c: u64, k: i32) {
            self.log.push(format!("replace {c:x} {k}"));
        }
        fn generate(&mut self) -> f32 {
            self.log.push("draw".into());
            self.random
        }
        fn fires_over_walls(&mut self, _u: u64) -> bool {
            false
        }
        fn same_player(&mut self, _u: u64, _o: u64) -> bool {
            self.same_player
        }
        fn hit_points(&mut self, u: u64) -> i32 {
            self.units.get(&u).map_or(0, |x| x.hit_points)
        }
        fn complex_unit(&mut self, u: u64) -> u64 {
            u
        }
        fn write_f32(&mut self, v: f32) -> i32 {
            self.file.push(format!("f{v}"));
            0
        }
        fn read_f32(&mut self, v: &mut f32) -> i32 {
            if self.reads.is_empty() {
                return 5;
            }
            *v = self.reads.remove(0)[1..].parse().unwrap();
            0
        }
    }

    fn world() -> World {
        let mut w = World { garrison_ok: true, random: 0.25, next_goal: 0x9000, ..Default::default() };
        w.classes.insert(
            C,
            LoadClass {
                garrison_capacity: 5,
                transport_capacity: 10,
                garrison_technology: 77,
                unit_type: 0x29,
                tile_size: 3,
                load_range: 2.5,
                ..Default::default()
            },
        );
        w.units.insert(C, LoadUnit { hit_points: 100, garrison_count: 1, cargo_room: 4, ..Default::default() });
        for (u, x) in [(U1, 10.0f32), (U2, 11.0), (U3, 40.0)] {
            w.classes.insert(u, LoadClass { weight: 2, inner_sphere: 1.0, can_garrison: true, ..Default::default() });
            w.units.insert(u, LoadUnit { hit_points: 20, animation: 3, ..Default::default() });
            w.locations.insert(u, [x, 10.0, 0.0]);
            w.tiles.insert(u, (x as i64, 10));
        }
        w.locations.insert(C, [12.0, 10.0, 0.0]);
        w.tiles.insert(C, (12, 10));
        w
    }

    #[test]
    fn creation_of_both_kinds() {
        // a garrison: the entrance point, (1.2 x 3)^2, a capitol with no upgrade node is valid at its capacity
        let mut w = world();
        let g = 0x5001u64;
        create(g, GOAL_LOAD_GARRISON, C, &[U1, U2], &mut w);
        let l = get(g).unwrap();
        assert_eq!((l.container_tile, l.unit_tile), ((12, 10), (12, 10)));
        assert_eq!(l.destination, [12.5, 11.5, 0.0]);
        assert_eq!(l.distance, (3.0f32 * 1.2f32) * (3.0f32 * 1.2f32));
        assert_eq!((l.valid, l.upgrade, l.cost), (true, 0, 5));
        assert_eq!((w.refs[&C], w.refs[&U1], w.refs[&U2]), (1, 1, 1));
        // the upgrade node: the class taken, valid only when available
        w.upgrade = (true, 55, false);
        let g2 = 0x5002u64;
        create(g2, GOAL_LOAD_GARRISON, C, &[], &mut w);
        let l = get(g2).unwrap();
        assert_eq!((l.valid, l.upgrade, l.cost), (false, 55, 0));
        // a small building: at least 2 (2 x 2)
        w.classes.get_mut(&C).unwrap().tile_size = 1;
        w.upgrade = (true, 56, true);
        let g3 = 0x5003u64;
        create(g3, GOAL_LOAD_GARRISON, C, &[U1], &mut w);
        let l = get(g3).unwrap();
        assert_eq!((l.distance, l.valid, l.upgrade, l.cost), (4.0, true, 56, 5));
        // a transport: its class's tiles, ceil((2.5 + 0.5)^2) = 9; no units: the base rule, the range alone
        let t = 0x5004u64;
        w.log.clear();
        create(t, GOAL_LOAD_TRANSPORT, C, &[U1, U2], &mut w);
        assert_eq!(w.log, ["rendezvous c00 [100, 200] false"]);
        let l = get(t).unwrap();
        assert_eq!((l.container_tile, l.unit_tile, l.distance, l.destination), ((30, 31), (32, 33), 9.0, [0.0; 3]));
        let t2 = 0x5005u64;
        w.log.clear();
        create(t2, GOAL_LOAD_TRANSPORT, C, &[], &mut w);
        assert_eq!(w.log, ["rendezvous c00 [] true"]);
        assert_eq!(get(t2).unwrap().distance, 7.0);
        for x in [g, g2, g3, t, t2] {
            destroy(x, &mut w);
        }
        assert!(w.refs.values().all(|r| *r == 0));
    }

    #[test]
    fn the_unit_list_the_copy_and_the_texts() {
        let mut w = world();
        let g = 0x5101u64;
        create(g, GOAL_LOAD_GARRISON, C, &[U1, U1, U2], &mut w);
        assert_eq!(get(g).unwrap().units, [U1, U2]);
        let mut io = EeUnitsGoalIo::default();
        body(EE_UNITS_GOAL_LOAD_ADD_UNIT, GOAL_LOAD_GARRISON, g, U3, 0, &mut io, &mut w);
        assert_eq!(body(EE_UNITS_GOAL_LOAD_UNIT_COUNT, GOAL_LOAD_GARRISON, g, 0, 0, &mut io, &mut w), 3);
        assert_eq!(body(EE_UNITS_GOAL_LOAD_UNIT_AT, GOAL_LOAD_GARRISON, g, 2, 0, &mut io, &mut w), U3 as i64);
        assert_eq!(body(EE_UNITS_GOAL_LOAD_HAS_UNIT, GOAL_LOAD_GARRISON, g, U2, 0, &mut io, &mut w), 1);
        // the base goal's answers the classes keep: Suits true (a replaced building's goal follows it), 6 attempts
        assert_eq!(body(EE_UNITS_GOAL_SUITS, GOAL_LOAD_GARRISON, g, U2, 0, &mut io, &mut w), 1);
        assert_eq!(body(EE_UNITS_GOAL_MAX_PATH_ATTEMPTS, GOAL_LOAD_TRANSPORT, g, 0, 0, &mut io, &mut w), 6);
        assert_eq!(body(EE_UNITS_GOAL_LOAD_FIELDS, GOAL_LOAD_GARRISON, g, 0, 0, &mut io, &mut w), C as i64);
        assert_eq!(io.i[..6], [12, 10, 12, 10, 1, 3]);
        // the copy: everything referenced again, the destination the container tile
        let copy = clone(g, true, &mut w);
        let c = get(copy).unwrap();
        assert_eq!((c.units.len(), c.destination, c.cost, c.valid), (3, [12.0, 10.0, 0.0], 5, true));
        assert_eq!((w.refs[&C], w.refs[&U1]), (2, 2));
        // the checksum: the destination's bits, 3 units, the tiles, the id, the distance's bits, valid, cost, upgrade
        let l = get(g).unwrap();
        let mut want = 12.5f32.to_bits().wrapping_add(11.5f32.to_bits()).wrapping_add(3);
        want = want.wrapping_add(12 + 10 + 12 + 10).wrapping_add(0x0200_000c).wrapping_add(l.distance.to_bits());
        want = want.wrapping_add(1 + 5);
        assert_eq!(checksum(g, &mut w), i64::from(want as i32));
        w.text.clear();
        debug_line(g, &mut w);
        debug_text(g, &mut w);
        assert_eq!(
            w.text,
            "Load GarrisonLoad Garrison - Part of:Container: classc00, (12,10)\nPassengers: 3\nclass100 at 10,10\n\
             class200 at 11,10\nclass300 at 40,10\nContainerRendezvous:(12,10)\nLoadableUnitRendezvous:(12,10)\n\
             MinLoadingDistance:12.9600\n"
        );
        w.text.clear();
        dump(g, &mut w);
        assert!(w.text.starts_with(
            "OUT OF SYNCH DUMP:  EEUGLoadGarrison - Garrison upgrade is 0\n\
             OUT OF SYNCH DUMP:  EEUGLoadGarrison - Garrison Cost is 5\n\
             OUT OF SYNCH DUMP:  EEUGLoadGarrison - Is Valid is 1\n\
             OUT OF SYNCH DUMP:  EEUGLoadContainer - Actual Destination X: 12.500000\n"
        ));
        assert!(w.text.contains("Container Unit ID is #0200000c\n"));
        assert!(w.text.ends_with("Minimum Loading Distance Squared is 12.960001\n"));
        destroy(g, &mut w);
        destroy(copy, &mut w);
        assert!(w.refs.values().all(|r| *r == 0));
    }

    #[test]
    fn steps_triggers_and_loading() {
        let mut w = world();
        let t = 0x5201u64;
        w.transport = true;
        w.can_load = true;
        create(t, GOAL_LOAD_TRANSPORT, C, &[U1, U2, U3], &mut w);
        // loadable: 10 -> 12 (4 <= 9) with a clear line; U3 is too far (784)
        w.log.clear();
        assert!(is_loadable(t, U1, &mut w));
        assert_eq!(w.log, ["line (10, 10) (12, 10)"]);
        assert!(!is_loadable(t, U3, &mut w));
        // the triggers
        assert!(evaluate(t, U1, TRIGGER_GOAL_IS_LOAD_CONTAINER, &mut w));
        assert!(evaluate(t, U1, TRIGGER_CONTAINER_IN_RANGE, &mut w));
        assert!(!evaluate(t, C, TRIGGER_CONTAINER_IN_RANGE, &mut w));
        assert!(evaluate(t, U1, EVENT_CARRIED, &mut w));
        assert!(!evaluate(t, U2, EVENT_CARRIED, &mut w));
        assert!(!evaluate(t, U1, TRIGGER_CONTAINER_IS_FULL, &mut w));
        assert!(evaluate(t, U1, TRIGGER_MORE_LOADABLE_UNITS_TO_PICK_UP, &mut w));
        assert!(!evaluate(t, U1, TRIGGER_EXTRA_LOADABLE_UNITS_BUMPED, &mut w));
        assert!(!evaluate(t, U1, TRIGGER_CONTAINER_IS_DEAD, &mut w));
        assert!(evaluate(t, U1, TRIGGER_CONTAINER_HAS_LOST_GOAL, &mut w));
        w.goals.insert(C, (t, i64::from(GOAL_LOAD_TRANSPORT)));
        assert!(!evaluate(t, U1, TRIGGER_CONTAINER_HAS_LOST_GOAL, &mut w));
        // the rendezvous: (10 - 32)^2 + (10 - 33)^2 > 1
        assert!(evaluate(t, U1, TRIGGER_HAVE_NOT_REACHED_RENDEZVOUS_POINT, &mut w));
        assert!(evaluate(t, U1, TRIGGER_UNIT_FACING_NOT_REQUIRED, &mut w));
        // the destinations: the container its tile; a loadable unit stays; another boards at the class's spot
        let mut out = [0.0f32; 3];
        w.log.clear();
        destination(t, C, &mut out, &mut w);
        assert_eq!(out, [30.5, 31.5, 1.0]);
        destination(t, U1, &mut out, &mut w);
        assert_eq!(out, [10.0, 10.0, 0.0]);
        destination(t, U3, &mut out, &mut w);
        assert_eq!(out, [33.5, 33.5, 1.0]);
        assert_eq!(get(t).unwrap().destination, out);
        assert_eq!(w.log.last().unwrap(), "tell 300 [33.5, 33.5, 1.0]");
        // the load: the unit leaves the container's own goal, boards, the sound
        w.commands.insert(U1, ACTION_LOAD_INTO_CONTAINER);
        w.log.clear();
        step(t, U1, &mut w);
        assert_eq!(w.log, ["line (10, 10) (12, 10)", "goal of c00 true", "cargo c00 100", "sound c00 0x183"]);
        assert!(!has_unit(t, U1), "the container's goal (this one) dropped it");
        // refused: idle
        w.cargo_result = CARGO_REFUSED;
        add_unit(t, GOAL_LOAD_TRANSPORT, U1, &mut w);
        w.goals.clear();
        w.log.clear();
        step(t, U1, &mut w);
        assert_eq!(w.log[2..], ["cargo c00 100".to_string(), "start 100 5000".to_string()]);
        // giving up: off both goals; waiting: the animation by the draw
        w.commands.insert(U2, ACTION_CANNOT_REACH_CONTAINER);
        step(t, U2, &mut w);
        assert!(!has_unit(t, U2));
        w.commands.insert(U3, ACTION_WAIT_FOR_TRANSPORT);
        w.log.clear();
        step(t, U3, &mut w);
        assert_eq!(w.log, ["physics 300 3", "draw", "animation 300 7"]);
        w.random = 0.75;
        w.log.clear();
        step(t, U3, &mut w);
        assert_eq!(w.log, ["physics 300 3", "draw", "animation 300 8"]);
        // bumped when full
        w.units.get_mut(&C).unwrap().cargo_room = 0;
        w.commands.insert(U3, ACTION_BUMP_EXTRA_LOADABLE_UNITS);
        step(t, U3, &mut w);
        assert_eq!(get(t).unwrap().units.len(), 0);
        destroy(t, &mut w);
        assert!(w.refs.values().all(|r| *r == 0));

        // a garrison: cost met idles; the upgrade replaces the building; its triggers
        let g = 0x5202u64;
        w.upgrade = (true, 61, true);
        create(g, GOAL_LOAD_GARRISON, C, &[U1], &mut w);
        w.commands.insert(U1, ACTION_LOAD_INTO_CONTAINER);
        w.log.clear();
        step(g, U1, &mut w);
        assert_eq!(
            w.log,
            ["line (10, 10) (12, 10)", "goal of c00 true", "deposit 100 c00", "garrison c00 100", "sound c00 0x86"]
        );
        w.garrison_ok = false;
        w.log.clear();
        step(g, U1, &mut w);
        assert_eq!(w.log[3..], ["garrison c00 100".to_string(), "start 100 5000".to_string()]);
        w.commands.insert(C, ACTION_GARRISON_UPGRADE);
        w.log.clear();
        step(g, C, &mut w);
        assert_eq!(w.log, ["replace c00 61"]);
        assert!(!evaluate(g, U1, TRIGGER_CONTAINER_IN_RANGE, &mut w), "upgrading");
        w.commands.insert(C, 0);
        assert!(evaluate(g, U1, TRIGGER_CONTAINER_IN_RANGE, &mut w));
        assert!(!evaluate(g, U1, TRIGGER_GARRISON_COST_MET, &mut w));
        assert!(evaluate(g, U1, TRIGGER_GOAL_IS_LOAD_GARRISON, &mut w));
        assert!(!evaluate(g, U1, TRIGGER_NO_MORE_WAYPOINTS, &mut w));
        assert!(!evaluate(g, U1, EVENT_CARRIED, &mut w));
        // the start: a living unit
        w.log.clear();
        start(g, U1, &mut w);
        w.units.get_mut(&U1).unwrap().hit_points = 0;
        start(g, U1, &mut w);
        assert_eq!(w.log, ["start 100 8000"]);
        destroy(g, &mut w);
    }

    #[test]
    fn replacing_units() {
        let mut w = world();
        let t = 0x5301u64;
        create(t, GOAL_LOAD_TRANSPORT, C, &[U1, U2], &mut w);
        // another player: the listed unit is only dropped
        replace_unit(t, U1, U3, 0, &mut w);
        assert_eq!(get(t).unwrap().units, [U2]);
        w.same_player = true;
        replace_unit(t, U2, U1, 0, &mut w);
        assert_eq!(get(t).unwrap().units, [U1]);
        replace_unit(t, C, 0xd00, 0, &mut w);
        assert_eq!(get(t).unwrap().container, Held::Object(0xd00));
        destroy(t, &mut w);
        // the garrison: a unit whose next goal is a garrison clears it instead
        let g = 0x5302u64;
        create(g, GOAL_LOAD_GARRISON, C, &[U1], &mut w);
        w.queued_kind = i64::from(GOAL_LOAD_GARRISON);
        w.log.clear();
        replace_unit(g, C, 0xd00, U1, &mut w);
        assert_eq!(w.log, ["clear 100 1"]);
        assert_eq!(get(g).unwrap().container, Held::Object(C));
        w.queued_kind = -1;
        w.tiles.insert(0xd00, (50, 60));
        replace_unit(g, C, 0xd00, U1, &mut w);
        let l = get(g).unwrap();
        assert_eq!((l.container, l.container_tile), (Held::Object(0xd00), (50, 60)));
        destroy(g, &mut w);
        assert_eq!(w.refs[&C], 0);
        assert!(w.refs.values().all(|r| *r == 0));
    }

    #[test]
    fn save_load_repair_by_version() {
        let mut w = world();
        let g = 0x5401u64;
        create(g, GOAL_LOAD_GARRISON, C, &[U1, U2], &mut w);
        assert_eq!(save(g, &mut w), 0);
        assert_eq!(
            w.file,
            [
                "u33554444",
                "u2",
                "u33554433",
                "u33554434",
                "f12.960001",
                "f12.5",
                "f11.5",
                "f0",
                "i12",
                "i10",
                "i12",
                "i10",
                "i5",
                "i0",
                "b1"
            ]
        );
        let other = 0x5402u64;
        put(other, Load::empty(GOAL_LOAD_GARRISON));
        w.reads = w.file.clone();
        assert_eq!(load(other, GOAL_LOAD_GARRISON, 0, 0x10c, &mut w), 0);
        assert!(w.reads.is_empty());
        assert_eq!(repair(other, GOAL_LOAD_GARRISON, &mut w), 0);
        let (a, b) = (get(g).unwrap(), get(other).unwrap());
        assert_eq!(a, b);
        // before 0.221: one point (floored) for both tiles, then the destination
        let old = 0x5403u64;
        put(old, Load::empty(GOAL_LOAD_TRANSPORT));
        w.reads = ["u33554444", "u1", "u33554433", "f4", "f-2.5", "f7.25", "f1", "f2", "f3"]
            .iter()
            .map(|s| s.to_string())
            .collect();
        assert_eq!(load(old, GOAL_LOAD_TRANSPORT, 0, 0xdc, &mut w), 0);
        let l = get(old).unwrap();
        assert_eq!((l.container_tile, l.unit_tile, l.destination), ((-3, 7), (-3, 7), [1.0, 2.0, 3.0]));
        assert_eq!(l.unit_ids, [0x0200_0001]);
        // a container gone: 3, none, the ids forgotten
        let gone = 0x5404u64;
        put(gone, Load::empty(GOAL_LOAD_TRANSPORT));
        w.reads =
            ["u33554585", "u0", "f1", "f0", "f0", "f0", "i0", "i0", "i0", "i0"].iter().map(|s| s.to_string()).collect();
        assert_eq!(load(gone, GOAL_LOAD_TRANSPORT, 0, 0x10c, &mut w), 0);
        assert_eq!(repair(gone, GOAL_LOAD_TRANSPORT, &mut w), 3);
        assert_eq!(get(gone).unwrap().container, Held::None);
        // a short file: the failing status
        w.reads = vec!["u33554444".into()];
        assert_eq!(load(gone, GOAL_LOAD_TRANSPORT, 0, 0x10c, &mut w), 5);
        for x in [g, other, old, gone] {
            destroy(x, &mut w);
        }
    }
}
