//! The build/repair goal whole in Rust (RUST_PLAN R5b.4, the nineteenth round): `EEUGBuildRepair`'s state (the
//! site's location and centre, the player, the building, its class, technology, tech tree node, type and size, the
//! first tick's amount, the builder-counted, first-tick-adjusted and repair flags) and every body (the constructor's
//! site, `GetDestination`, `Step` and its command handlers, the triggers, `Work`'s hit points, the build amount, the
//! copies, the checksum, the texts, `Save`, `Load`, `RepairPointers`, `ReplaceUnit`, `Abandon`) by the goal's object.
//! The C++ class is gone: a build/repair goal is an `EEUGRust` of kind `kGoalBuildRepair` (ee/EEUGRust.h) over the
//! Rust goals' host (rust_goal.rs). The world's own work stays C++ host operations: the footprint and blocking-unit
//! tests, the completion's research and effects, a foundation's media and wonder notice, the payments, the cancelled
//! foundation's refund, the helpers that give the citizen its next job (help build, tend a farm, gather nearby).
//!
//! Determinism (RUST_PLAN 2.1): the amounts are the original's `f32` expressions, `UMCeil` and `UMFloatToLong` over
//! `lrintf`; the visible-site test is `GE3DPoint::DistanceToSquared` (ee_ge's).

use std::collections::BTreeMap;
use std::sync::Mutex;

use ee_ge::types::Ge3DPoint;

use crate::load::{um_ceil, um_floor};
use crate::physics::lrintf;
use crate::rust_goal::{base_condition, hex8, EeUnitsBuildUnit, EeUnitsGoalIo, GoalHost, Held};

/// `kGoalBuildRepair`
pub const GOAL_BUILD_REPAIR: u8 = 4;
/// `kTechNone`
const TECH_NONE: i32 = 100_000;

const ACTION_REPAIR_BUILDING: i64 = 4002;
const ACTION_GATHER_RESOURCE: i64 = 4004;
const ACTION_WAIT_FOR_BUILD_SITE_TO_CLEAR: i64 = 4012;
const ACTION_CONVERT_TO_LOCATION_GOAL: i64 = 4019;
const ACTION_TASK_SMART_CITIZEN: i64 = 4028;
const ACTION_INSUFFICIENT_RESOURCES: i64 = 4030;
const ACTION_CHANGE_TARGET_FOR_SPECIAL_REPAIR: i64 = 4031;
const ACTION_MOVE_TO_RESOURCE: i64 = 8008;
const ACTION_MOVE_TO_REPAIR_SITE: i64 = 8017;
const ACTION_CLOSED: i32 = 15001;

const TRIGGER_BUILDING_DESTROYED: i64 = 2001;
const TRIGGER_BUILDING_REPAIRED: i64 = 4001;
const TRIGGER_REPAIR_SITE_IN_RANGE: i64 = 4014;
const TRIGGER_BUILD_SITE_CLEAR: i64 = 4021;
const TRIGGER_BUILD_SITE_VISIBLE: i64 = 4022;
const TRIGGER_BUILD_SITE_VALID: i64 = 4023;
const TRIGGER_BUILDING_NOT_PAID_FOR: i64 = 4030;
const TRIGGER_GOAL_IS_BUILD: i64 = 4031;
const TRIGGER_CANNOT_AFFORD_REPAIR: i64 = 4036;
const TRIGGER_REPAIR_IS_SPECIAL_UNIT: i64 = 4037;

const UNIT_TYPE_FARM: i32 = 0x2a;
const UNIT_TYPE_GRANARY: i32 = 0x34;
const UNIT_TYPE_GRANARY_IMPROVED: i32 = 0x2ba;
const UNIT_TYPE_ROBOTIC_FARM: i32 = 0x33a;
const CATEGORY_LAND_CITIZEN: i32 = 6;
const CATEGORY_NAVAL_CITIZEN: i32 = 0x16;
const CATEGORY_MINING_BUILDING: i32 = 0x18;
const CATEGORY_GATE: i32 = 0x19;
/// the save version from which the goal keeps its own site (`EESaveHasLiveState`: 0.254)
const VERSION_LIVE_STATE: u32 = 254;
/// the saves up to which the builder-counted flag is cleared on load (0.76)
const VERSION_WORKER_COUNT: u32 = 0x4c;

/// `EESaveHasLiveState`
pub fn save_has_live_state(major: u32, minor: u32) -> bool {
    major > 0 || minor >= VERSION_LIVE_STATE
}

/// One build/repair goal's state.
#[derive(Clone, Copy, Debug, PartialEq)]
struct Build {
    location: [f32; 3],
    center: [f32; 3],
    player: u64,
    target: Held,
    class: u64,
    node: u64,
    technology: i32,
    target_type: i32,
    scale: i32,
    first_tick_amount: i32,
    worker_counted: bool,
    first_tick_adjusted: bool,
    repair: bool,
}

impl Build {
    /// `EEUGBuildRepair(inArg)`: no player, building or class, no technology
    fn empty() -> Build {
        Build {
            location: [0.0; 3],
            center: [0.0; 3],
            player: 0,
            target: Held::None,
            class: 0,
            node: 0,
            technology: TECH_NONE,
            target_type: 0,
            scale: 0,
            first_tick_amount: 0,
            worker_counted: false,
            first_tick_adjusted: false,
            repair: false,
        }
    }

    /// `SetTargetLocation`: the site (z 0) and its centre (the location plus half the size)
    fn set_location(&mut self, x: f32, y: f32) {
        self.location = [x, y, 0.0];
        let half = self.scale as f32 * 0.5;
        self.center[0] = self.location[0] + half;
        self.center[1] = self.location[1] + half;
    }
}

static BUILDS: Mutex<BTreeMap<u64, Build>> = Mutex::new(BTreeMap::new());

fn get(goal: u64) -> Build {
    let goals = BUILDS.lock().unwrap_or_else(|e| e.into_inner());
    goals.get(&goal).copied().unwrap_or_else(Build::empty)
}

fn put(goal: u64, state: Build) {
    let mut goals = BUILDS.lock().unwrap_or_else(|e| e.into_inner());
    goals.insert(goal, state);
}

fn forget(goal: u64) {
    let mut goals = BUILDS.lock().unwrap_or_else(|e| e.into_inner());
    goals.remove(&goal);
}

/// The world calls of the build/repair goal (the FFI adapter; the tests' mock).
pub trait BuildHost: GoalHost {
    /// a unit's fields the goals read (zeros for 0)
    fn build_unit(&mut self, unit: u64) -> EeUnitsBuildUnit;
    /// the player's tech tree node of a technology (`GetTechTree()->Get(tech)`)
    fn tech_node(&mut self, player: u64, technology: i32) -> u64;
    /// a node's data id, data kind and `GetBuildTime()` (zeros for none)
    fn node_info(&mut self, node: u64) -> (i32, i32, i32);
    /// a player's `mPlayerID`, a class's `mUnitType` (0 for none)
    fn player_id(&mut self, player: u64) -> i32;
    fn class_type(&mut self, class: u64) -> i32;
    /// the unit's `mNumBuilders += delta`
    fn builders_add(&mut self, unit: u64, delta: i32);
    /// `EEGetAttackPart(unit, target)`, `EEIsInWorkReach(unit, target)`
    fn attack_part(&mut self, unit: u64, target: u64) -> u64;
    fn in_work_reach(&mut self, unit: u64, target: u64) -> bool;
    /// the target's class's `GetSurroundingTiles(target, tiles, 1, 0)`
    fn surrounding_tiles(&mut self, target: u64) -> Vec<(i32, i32)>;
    /// the unbuilt building finished by the worker's player (research, built, the space wall's energy wall, the
    /// AI's gates, the local sound, the panel), `target_type` the goal's building's class type
    fn complete_building(&mut self, worker: u64, outer: u64, target_type: i32);
    /// the unit's `PlayEffect`, `SetProductionTime`, `SetWorkTimer`, `FaceTarget`
    fn play_effect(&mut self, unit: u64, effect: i32);
    fn set_production_time(&mut self, unit: u64, time: i32);
    fn set_work_timer(&mut self, unit: u64, time: i32);
    fn face_target(&mut self, unit: u64, target: u64);
    /// the building's other parts at full hit points
    fn fill_parts(&mut self, outer: u64);
    /// `EEFindNextFreeTile(unit, target, out)`
    fn next_free_tile(&mut self, unit: u64, target: u64) -> Option<[f32; 3]>;
    /// the foundation killed, its cost refunded to the worker's player (the node's costs)
    fn cancel_foundation(&mut self, outer: u64, worker: u64, node: u64);
    /// the unit's goal: a forced move to the point (`new EEUGLocation(&point, false, true, false, 0, false, false)`)
    fn location_goal(&mut self, unit: u64, point: [f32; 3]);
    /// the static helpers of `FindNextWork`: help build nearby, tend the next farm, gather near the building
    fn help_build_nearby(&mut self, unit: u64, target: u64);
    fn tend_next_farm(&mut self, unit: u64, target: u64);
    fn gather_near(&mut self, unit: u64, target: u64);
    /// the gate class's `IsSidePart(target)`
    fn gate_side_part(&mut self, target: u64) -> bool;
    /// the player's event at the building's tile (the repair's text or the build's)
    fn build_event(&mut self, player: u64, target: u64, repair: bool);
    /// the computer player's build manager: `BuildingAbandoned(target)`
    fn abandon_building(&mut self, unit: u64, target: u64);
    /// `SetCurrentPhysicsAction(action)`, `SetVelocity(0)`, no angular velocity
    fn halt(&mut self, unit: u64, action: i64);
    /// the footprint tests of the goal's building (its player and type): `IsBuildSiteTaken`, `IsFootprintClear`,
    /// `IsSiteFreeOfUnits`
    fn site_taken(&mut self, target: u64, player: u64, target_type: i32) -> bool;
    fn footprint_clear(&mut self, target: u64, player: u64, target_type: i32) -> bool;
    fn site_free(&mut self, target: u64, player: u64, target_type: i32) -> bool;
    /// whether another goal than `goal` that is no automatic waypoint is queued in the unit's queue 0
    fn other_goal_queued(&mut self, unit: u64, goal: u64) -> bool;
    /// `gWorld.GetResourceUsers(unit)`
    fn resource_users(&mut self, unit: u64) -> i32;
    /// a new gather goal of the resource and storage
    fn new_gather(&mut self, resource: u64, storage: u64, arg: bool) -> u64;
    /// the unit's `SetGoal(goal, queue, flag)`
    fn set_goal(&mut self, unit: u64, goal: u64, queue: i32, flag: i32);
    /// the unit's player's `GetNearestConstructionSite(unit)`
    fn nearest_site(&mut self, unit: u64) -> u64;
    /// `EEStepAsideFromSite` on the building's tile
    fn clear_site(&mut self, target: u64);
    /// the target's player's `CanProduce(outer's technology, 0x20, 1, true, 3, NULL)`
    fn can_produce(&mut self, target: u64, outer: u64) -> bool;
    /// the unit's player's `CanAffordRepair(outer, unit, amount)`
    fn can_afford_repair(&mut self, unit: u64, outer: u64, amount: i32) -> bool;
    /// the unit's player is the player or not its enemy
    fn friendly(&mut self, player: u64, unit: u64) -> bool;
    /// `gWorld.mInCinematic`
    fn in_cinematic(&mut self) -> bool;
    /// the building's `mIsPaidFor = true`
    fn set_paid(&mut self, outer: u64);
    /// the player's `Pay(node, 0x20, fraction)`
    fn pay(&mut self, player: u64, node: u64, fraction: f32);
    /// the unit's `SetHitPoints`
    fn set_hit_points(&mut self, unit: u64, hit_points: i32);
    /// a foundation become a building site: the wonder's notice and sound (`wonder`), the media, the parts at 2
    /// hit points and finished, the building's part finished
    fn foundation_started(&mut self, outer: u64, target: u64, wonder: bool);
    /// the goal's `GetB1C()`
    fn goal_arg(&mut self, goal: u64) -> bool;
    /// the unit drops its resource at the building when it carries some the building takes
    fn deposit(&mut self, unit: u64, target: u64);
    /// the unit's `mGoalState`
    fn goal_state(&mut self, unit: u64) -> u64;
    /// the typed `float` file calls
    fn write_f32(&mut self, value: f32) -> i32;
    fn read_f32(&mut self, value: &mut f32) -> i32;
}

/// A unit's outermost part (the first of its linked parts; the unit itself without a complex unit).
fn outermost(target: u64, host: &mut dyn BuildHost) -> u64 {
    let first = host.build_unit(target).first;
    if first != 0 {
        first
    } else {
        target
    }
}

/// A unit's position: its complex unit's, else its tile's centre.
fn position(target: u64, host: &mut dyn BuildHost) -> [f32; 3] {
    let u = host.build_unit(target);
    if u.complex != 0 {
        host.unit_position(u.complex)
    } else {
        [u.tile_x as f32 + 0.5, u.tile_y as f32 + 0.5, 0.0]
    }
}

/// The building's class, type, technology, node and size taken (the constructor and `RepairPointers`).
fn take_target(b: &mut Build, target: u64, host: &mut dyn BuildHost) {
    let u = host.build_unit(target);
    b.class = u.unit_class;
    b.target_type = u.unit_type;
    b.technology = u.table_technology;
    b.node = host.tech_node(b.player, b.technology);
    b.scale = u.tile_size;
}

/// `EEUGBuildRepair(inPlayer, inTarget, inArg)`: the building's first part, referenced; its class data and site;
/// a repair when it is already built (never a farm).
pub fn create(goal: u64, player: u64, target: u64, host: &mut dyn BuildHost) {
    let mut b = Build::empty();
    b.player = player;
    if target == 0 {
        put(goal, b);
        return;
    }
    let first = outermost(target, host);
    b.target = Held::Object(first);
    host.reference(first, false);
    take_target(&mut b, first, host);
    let p = position(first, host);
    b.set_location(p[0], p[1]);
    let u = host.build_unit(first);
    let built = u.complex != 0 && host.build_unit(u.complex).built;
    b.repair = u.tile_node && built && u.unit_type != UNIT_TYPE_FARM;
    put(goal, b);
}

/// The destructor: the builder uncounted, the building released.
fn destroy(goal: u64, host: &mut dyn BuildHost) {
    let b = get(goal);
    forget(goal);
    if let Held::Object(t) = b.target {
        if b.worker_counted && host.build_unit(t).builders > 0 {
            host.builders_add(t, -1);
        }
        host.dereference(t, false);
    }
}

/// `IsAtBuildSite`: within work reach of the building (a multi-part one's attack part) and, but at farms, on a
/// tile next to it.
fn at_build_site(b: &Build, unit: u64, host: &mut dyn BuildHost) -> bool {
    let Held::Object(t) = b.target else { return false };
    let mut target = host.build_unit(t).complex;
    if host.build_unit(target).next != 0 {
        target = host.attack_part(unit, target);
    }
    if !host.in_work_reach(unit, target) {
        return false;
    }
    let kind = host.build_unit(target).unit_type;
    if kind == UNIT_TYPE_FARM || kind == UNIT_TYPE_ROBOTIC_FARM {
        return true;
    }
    let tiles = host.surrounding_tiles(target);
    let (x, y) = host.unit_tile(unit);
    tiles.iter().any(|&(tx, ty)| i64::from(tx) == x && i64::from(ty) == y)
}

/// `FinishBuilding`: at full hit points an unbuilt building is completed; when this is the unit's goal the other
/// parts are filled, a mine starts gathering and the builder is uncounted (true).
fn finish_building(goal: u64, unit: u64, host: &mut dyn BuildHost) -> bool {
    let mut b = get(goal);
    let t = b.target.object();
    let outer = outermost(host.build_unit(t).complex, host);
    let o = host.build_unit(outer);
    if outer == 0 || o.hit_points < o.max_hit_points {
        return false;
    }
    if !o.built {
        let target_type = host.build_unit(t).unit_type;
        host.complete_building(unit, outer, target_type);
    }
    host.play_effect(unit, TECH_NONE);
    host.set_production_time(unit, 0);
    if host.goal_state(unit) != goal {
        return false;
    }
    host.fill_parts(outer);
    if host.build_unit(outer).category == CATEGORY_MINING_BUILDING {
        host.start_action(outer, ACTION_GATHER_RESOURCE);
    }
    b = get(goal);
    if b.worker_counted {
        if host.build_unit(outer).builders > 0 {
            host.builders_add(outer, -1);
        }
        b.worker_counted = false;
        put(goal, b);
    }
    true
}

/// `GetDestination`: the next free tile around the building (kept as the site), else its position, the unit's
/// group told; without a building the site, else where the unit stands.
fn destination(goal: u64, unit: u64, out: &mut [f32; 3], host: &mut dyn BuildHost) {
    let mut b = get(goal);
    if let Held::Object(t) = b.target {
        let complex = host.build_unit(t).complex;
        if let Some(p) = host.next_free_tile(unit, complex) {
            *out = p;
            b.set_location(p[0], p[1]);
            put(goal, b);
        } else {
            *out = host.unit_position(complex);
        }
        host.tell_group(unit, *out);
    } else if b.location[0] > 0.0 {
        *out = b.location;
    } else {
        *out = host.unit_position(unit);
    }
}

/// `ComputeBuildAmount`: the hit points one work tick adds (the repair rate, or the build time shared by the
/// builders; all that is missing with instant build), at least 1; the first real tick counts the builder and the
/// second is corrected by the first's. A preview leaves the counting alone (but keeps the amount).
fn build_amount(goal: u64, worker: u64, preview: bool, host: &mut dyn BuildHost) -> i32 {
    let mut b = get(goal);
    let t = b.target.object();
    let target = host.build_unit(t);
    let outer = outermost(target.complex, host);
    let outer = host.build_unit(outer);
    let max = outer.max_hit_points;
    let mut workers = outer.builders;
    if preview && workers < 2 {
        workers = 1;
    }
    let w = host.build_unit(worker);
    let mut amount = if !w.instant_build {
        if outer.built {
            let a = um_ceil((w.repair_rate as f32 * 0.01) * max as f32);
            um_ceil(3.0 / (workers as f32 + 2.0) * a as f32)
        } else {
            let count = workers as f32;
            let factor = target.build_time_scale;
            let maximum = max as f32;
            let build_time = host.node_info(b.node).2;
            let rate = maximum / ((build_time as f32 * factor) * (3.0 / (count + 2.0)));
            um_ceil(w.work_rate as f32 * rate / count).min(max)
        }
    } else {
        max.wrapping_sub(outer.hit_points)
    };
    amount = amount.max(1);
    amount = amount.min(max.wrapping_sub(outer.hit_points));
    if !b.worker_counted {
        if !preview {
            b.first_tick_adjusted = false;
            b.worker_counted = true;
        }
        b.first_tick_amount = amount;
    } else if !b.first_tick_adjusted && !preview {
        amount = amount.wrapping_mul(2).wrapping_sub(b.first_tick_amount);
        b.first_tick_adjusted = true;
    }
    put(goal, b);
    amount
}

/// `ReplaceUnit`: a building replaced by another unit is followed, dropped when the new one is an enemy's.
pub fn replace_unit(goal: u64, old: u64, new: u64, host: &mut dyn BuildHost) {
    let mut b = get(goal);
    let Held::Object(t) = b.target else { return };
    let complex = host.build_unit(t).complex;
    if complex == 0 || complex != old {
        return;
    }
    host.dereference(complex, false);
    if host.friendly(b.player, new) {
        host.reference(new, false);
        b.target = Held::Object(new);
    } else {
        b.target = Held::None;
    }
    put(goal, b);
}

/// `RetargetGatePart`: a gate's target becomes the part the unit should work on.
fn retarget_gate(goal: u64, unit: u64, host: &mut dyn BuildHost) {
    let t = get(goal).target.object();
    let u = host.build_unit(t);
    if u.category == CATEGORY_GATE {
        let part = host.attack_part(unit, u.complex);
        replace_unit(goal, u.complex, part, host);
    }
}

/// `CancelFoundation`: a living foundation is killed and refunded; the unit moves to the site.
fn cancel_foundation(goal: u64, unit: u64, host: &mut dyn BuildHost) {
    let b = get(goal);
    let complex = host.build_unit(b.target.object()).complex;
    let outer = outermost(complex, host);
    let o = host.build_unit(outer);
    if o.foundation && o.hit_points > 0 {
        host.cancel_foundation(outer, unit, b.node);
    }
    let point = if b.location[0] <= 0.0 { host.build_unit(outer).location } else { b.location };
    host.location_goal(unit, point);
}

/// `IsGateClosed`: the target is a gate's middle part whose gate is not closed (despite the name).
fn gate_open(b: &Build, host: &mut dyn BuildHost) -> bool {
    let u = host.build_unit(b.target.object());
    if u.category != CATEGORY_GATE {
        return false;
    }
    !host.gate_side_part(u.complex) && host.build_unit(u.complex).command != ACTION_CLOSED
}

/// `IsShiftQueueNext` (port): a human's shift queue, not a smart task, decides the citizen's next job.
fn shift_queue_next(goal: u64, unit: u64, host: &mut dyn BuildHost) -> bool {
    if host.other_goal_queued(unit, goal) {
        return true;
    }
    if !host.build_unit(unit).shift_queued_build {
        return false;
    }
    match get(goal).target {
        Held::Object(t) => host.build_unit(t).unit_type != UNIT_TYPE_FARM,
        _ => true,
    }
}

/// `FindNextWork`: the citizen's next job after the building (work the farm or the next free one, drop off at a
/// granary and tend a farm, build the nearest site after a wall, drop off and gather at a drop site, else help
/// build nearby).
fn find_next_work(goal: u64, unit: u64, host: &mut dyn BuildHost) {
    let Held::Object(t) = get(goal).target else { return };
    let u = host.build_unit(t);
    let target = u.complex;
    if u.unit_type == UNIT_TYPE_FARM {
        if host.resource_users(t) < 1 {
            let gather = host.new_gather(t, 0, false);
            host.set_goal(unit, gather, 0, 0);
            host.start_action(unit, ACTION_MOVE_TO_RESOURCE);
        } else {
            host.tend_next_farm(unit, target);
        }
    } else if u.unit_type == UNIT_TYPE_GRANARY_IMPROVED
        || u.unit_type == UNIT_TYPE_GRANARY
        || u.unit_type == UNIT_TYPE_ROBOTIC_FARM
    {
        host.deposit(unit, target);
        host.tend_next_farm(unit, target);
    } else if u.wall && !u.tower {
        let next = host.nearest_site(unit);
        if next != 0 {
            let player = host.build_unit(next).player;
            let copy = host.new_goal(GOAL_BUILD_REPAIR, false);
            create(copy, player, next, host);
            host.set_goal(unit, copy, 0, 0);
            host.start_action(unit, ACTION_MOVE_TO_REPAIR_SITE);
        }
    } else if u.drop_site && target != 0 {
        host.deposit(unit, target);
        if host.goal_arg(goal) {
            host.gather_near(unit, target);
        }
    } else {
        host.help_build_nearby(unit, t);
    }
}

/// `BeginBuilding`: a work cycle (the effect, facing, the work animation, a load dropped, the work timer, the
/// remaining production time, the unit stopped).
fn begin_building(goal: u64, unit: u64, host: &mut dyn BuildHost) {
    let b = get(goal);
    let t = b.target.object();
    let u = host.build_unit(t);
    let target = u.complex;
    host.play_effect(unit, b.technology);
    host.face_target(unit, t);
    host.set_animation(unit, 1);
    let tc = host.build_unit(target);
    if target != 0 && tc.hit_points > 0 && tc.built {
        host.deposit(unit, target);
    }
    let w = host.build_unit(unit);
    let built = host.build_unit(target).built;
    host.set_work_timer(unit, if built { 1000 } else { w.work_rate });
    let u = host.build_unit(t);
    let (max, hit_points) = (u.max_hit_points, u.hit_points);
    let time = if !w.instant_build {
        let build_time = host.node_info(b.node).2;
        lrintf(build_time as f32 * (max.wrapping_sub(hit_points) as f32 / max as f32))
    } else {
        1
    };
    host.set_production_time(unit, time);
    host.drop_path(unit);
    host.halt(unit, 6);
}

/// `BeginRepair`: the unit waits for the site (animation 7) and, but at farms, units are moved off the site.
fn begin_repair(goal: u64, unit: u64, host: &mut dyn BuildHost) {
    host.halt(unit, 3);
    host.set_animation(unit, 7);
    let t = get(goal).target.object();
    let u = host.build_unit(t);
    if u.unit_type != UNIT_TYPE_FARM && u.unit_type != UNIT_TYPE_ROBOTIC_FARM && u.tile && u.valid {
        host.clear_site(t);
    }
}

/// `Step`: the current command's handler.
pub fn step(goal: u64, unit: u64, host: &mut dyn BuildHost) {
    match host.unit_command(unit) {
        ACTION_MOVE_TO_REPAIR_SITE => host.set_physics_action(unit, 0),
        ACTION_CHANGE_TARGET_FOR_SPECIAL_REPAIR => retarget_gate(goal, unit, host),
        ACTION_INSUFFICIENT_RESOURCES => {
            let b = get(goal);
            host.build_event(b.player, b.target.object(), b.repair);
        }
        ACTION_TASK_SMART_CITIZEN => {
            if host.build_unit(unit).human && !shift_queue_next(goal, unit, host) {
                find_next_work(goal, unit, host);
            }
        }
        ACTION_CONVERT_TO_LOCATION_GOAL => cancel_foundation(goal, unit, host),
        ACTION_WAIT_FOR_BUILD_SITE_TO_CLEAR => begin_repair(goal, unit, host),
        ACTION_REPAIR_BUILDING => begin_building(goal, unit, host),
        _ => {}
    }
}

/// `EvaluateCondition`: the build/repair triggers; others the base goal's.
pub fn evaluate(goal: u64, unit: u64, event: i64, host: &mut dyn BuildHost) -> bool {
    let b = get(goal);
    let t = b.target.object();
    match event {
        TRIGGER_BUILDING_DESTROYED => t == 0 || host.build_unit(t).hit_points == 0,
        TRIGGER_BUILDING_NOT_PAID_FOR => {
            // (the original read the paid flag before the building's test: no building faulted)
            if t == 0 || host.build_unit(t).paid_for {
                return false;
            }
            let outer = outermost(t, host);
            !host.can_produce(t, outer)
        }
        TRIGGER_GOAL_IS_BUILD => {
            if !b.repair {
                return true;
            }
            if t == 0 {
                return false;
            }
            let kind = host.build_unit(t).unit_type;
            kind == UNIT_TYPE_GRANARY_IMPROVED || kind == UNIT_TYPE_GRANARY
        }
        TRIGGER_CANNOT_AFFORD_REPAIR => {
            if !b.repair {
                return false;
            }
            let outer = outermost(t, host);
            let amount = build_amount(goal, unit, true, host);
            !host.can_afford_repair(unit, outer, amount)
        }
        TRIGGER_REPAIR_IS_SPECIAL_UNIT => gate_open(&b, host),
        TRIGGER_BUILDING_REPAIRED => t == 0 || finish_building(goal, unit, host),
        TRIGGER_REPAIR_SITE_IN_RANGE => at_build_site(&b, unit, host),
        TRIGGER_BUILD_SITE_CLEAR => {
            let clear = host.footprint_clear(t, b.player, b.target_type);
            if !clear && t != 0 {
                let u = host.build_unit(t);
                if u.unit_type != UNIT_TYPE_FARM && u.unit_type != UNIT_TYPE_ROBOTIC_FARM {
                    host.clear_site(t);
                }
            }
            clear
        }
        TRIGGER_BUILD_SITE_VISIBLE => {
            if t == 0 {
                return false;
            }
            let w = host.build_unit(unit);
            let scale = host.build_unit(t).tile_size;
            let mut range = w.line_of_sight * w.line_of_sight;
            range += scale as f32;
            let p = Ge3DPoint { x: w.location[0], y: w.location[1], z: w.location[2] };
            let c = Ge3DPoint { x: b.center[0], y: b.center[1], z: b.center[2] };
            p.distance_to_squared(c) <= range
        }
        TRIGGER_BUILD_SITE_VALID => host.site_free(t, b.player, b.target_type),
        _ => base_condition(event),
    }
}

/// `Work`: a work tick's hit points (a repair only when affordable, paid per tick); a foundation becomes a building
/// site; the remaining production time and the next work timer.
pub fn work(goal: u64, unit: u64, host: &mut dyn BuildHost) {
    let Held::Object(t) = get(goal).target else { return };
    let outer = outermost(host.build_unit(t).complex, host);
    let o = host.build_unit(outer);
    let mut max = o.max_hit_points;
    if o.hit_points < max && !host.in_cinematic() {
        if !get(goal).worker_counted {
            host.builders_add(outer, 1);
        }
        let amount = build_amount(goal, unit, false, host);
        let b = get(goal);
        let w = host.build_unit(unit);
        let o = host.build_unit(outer);
        if (b.target_type == UNIT_TYPE_FARM || b.target_type == UNIT_TYPE_ROBOTIC_FARM)
            && o.hit_points == 1
            && !o.paid_for
        {
            host.set_paid(outer);
            host.pay(w.player, b.node, 1.0);
        }
        if !b.repair || host.can_afford_repair(unit, outer, amount) {
            let o = host.build_unit(outer);
            let hit_points = o.hit_points.wrapping_add(amount).min(max);
            host.set_hit_points(outer, hit_points);
            if o.foundation && hit_points > 1 {
                let wonder = host.node_info(b.node).1 == 4;
                host.foundation_started(outer, t, wonder);
            }
            if b.repair {
                let player = host.build_unit(outer).player;
                host.pay(player, b.node, amount as f32 / max as f32);
            }
            let o = host.build_unit(outer);
            max = o.max_hit_points;
            let remaining = if !host.build_unit(unit).instant_build {
                let speed = o.build_time_scale;
                let cost = host.node_info(b.node).2;
                lrintf((max.wrapping_sub(o.hit_points) as f32 / max as f32) * (cost as f32 * speed))
            } else {
                1
            };
            host.set_production_time(unit, remaining);
        }
    }
    let timer = if host.build_unit(outer).built { 1000 } else { host.build_unit(unit).work_rate };
    host.set_work_timer(unit, timer);
}

/// `Abandon`: the computer player's build manager told of its unbuilt building.
fn abandon(goal: u64, unit: u64, host: &mut dyn BuildHost) {
    let t = get(goal).target.object();
    if !host.build_unit(unit).computer || t == 0 {
        return;
    }
    let complex = host.build_unit(t).complex;
    if !host.build_unit(complex).built {
        host.abandon_building(unit, complex);
    }
}

/// `Start`: to the site, or to the next job without a building.
fn start(goal: u64, unit: u64, host: &mut dyn BuildHost) {
    let action = if get(goal).target == Held::None { ACTION_TASK_SMART_CITIZEN } else { ACTION_MOVE_TO_REPAIR_SITE };
    host.start_action(unit, action);
}

fn bits(v: f32) -> i32 {
    v.to_bits() as i32
}

/// The checksum after the base goal's: the site's bits, the player, the building, class and technology ids, the
/// size, the first tick's amount and the flags.
pub fn checksum(goal: u64, host: &mut dyn BuildHost) -> i64 {
    let b = get(goal);
    let player = host.player_id(b.player);
    let target = match b.target {
        Held::Object(t) => host.unit_id(t) as i32,
        _ => 0,
    };
    let class_type = host.class_type(b.class);
    let node = host.node_info(b.node).0;
    let mut sum = bits(b.location[0]);
    for v in [b.location[1], b.location[2], b.center[0]] {
        sum = sum.wrapping_add(bits(v));
    }
    sum = sum.wrapping_add(bits(b.center[1]).wrapping_add(player));
    for v in [
        target,
        b.technology,
        b.target_type,
        b.scale,
        b.first_tick_amount,
        class_type,
        i32::from(b.worker_counted),
        i32::from(b.first_tick_adjusted),
        i32::from(b.repair),
        node,
    ] {
        sum = sum.wrapping_add(v);
    }
    i64::from(sum)
}

/// The out-of-synch dump.
pub fn dump(goal: u64, host: &mut dyn BuildHost) {
    let b = get(goal);
    let g = "OUT OF SYNCH DUMP:  EEUGBuildRepair";
    let [x, y, z] = b.location.map(f64::from);
    host.append(&format!("{g} Target Location ({x:6.6}, {y:6.6}, {z:6.6})\n"));
    let [x, y, z] = b.center.map(f64::from);
    host.append(&format!("{g} Target Center ({x:6.6}, {y:6.6}, {z:6.6})\n"));
    let player = host.player_id(b.player);
    host.append(&format!("{g} Player Number {player}\n"));
    let id = match b.target {
        Held::Object(t) => host.unit_id(t),
        _ => u32::MAX,
    };
    host.append(&format!("{g} Target Unit ID #{}\n", hex8(id)));
    let class_type = host.class_type(b.class);
    host.append(&format!("{g} Target Class {class_type}\n"));
    let node = host.node_info(b.node).0;
    host.append(&format!("{g} Target Tech Tree Node ID {node}\n"));
    host.append(&format!("{g} Target Tech Tree ID {}\n", b.technology));
    host.append(&format!("{g} Target Type {}\n", b.target_type));
    host.append(&format!("{g} Target Scale {}\n", b.scale));
    host.append(&format!("{g} Amount added to First Tick {}\n", b.first_tick_amount));
    host.append(&format!("{g} Construction Worker Count Incremented Flag Value {}\n", i32::from(b.worker_counted)));
    host.append(&format!("{g} First Tick Adjustment Made Flag Value {}\n", i32::from(b.first_tick_adjusted)));
    host.append(&format!("{g} Is Repair Flag Value {}\n", i32::from(b.repair)));
}

fn flag(value: bool) -> &'static str {
    if value {
        "true"
    } else {
        "false"
    }
}

/// `GetDebugLine`: the site's tile, the building's type and the builder-counted flag.
pub fn debug_line(goal: u64, host: &mut dyn BuildHost) {
    let b = get(goal);
    host.append(&format!(
        "build at {},{}, obj {} ({}):",
        um_floor(b.location[0]),
        um_floor(b.location[1]),
        b.target_type,
        flag(b.worker_counted)
    ));
}

/// `GetDebugText`: the site's tile, the building's name and the builder-counted flag.
pub fn debug_text(goal: u64, host: &mut dyn BuildHost) {
    let b = get(goal);
    host.append(&format!("BuildRepair: at {},{}\nObject:", um_floor(b.location[0]), um_floor(b.location[1])));
    match b.target {
        Held::Object(t) => host.append_class_name(t),
        _ => host.append("None"),
    }
    host.append(&format!("\nConstructionWorkerCountIncremented:{}\n", flag(b.worker_counted)));
}

fn save_point(p: [f32; 3], host: &mut dyn BuildHost) -> i32 {
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

fn load_point(p: &mut [f32; 3], host: &mut dyn BuildHost) -> i32 {
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

/// `Save`'s part after the base goal's: the building's id (-1 once gone), the flags, the first tick's amount, the
/// repair flag and, from 0.254, the site and its centre; the first failing status.
pub fn save(goal: u64, major: u32, minor: u32, host: &mut dyn BuildHost) -> i32 {
    let b = get(goal);
    let id = match b.target {
        Held::Object(t) => host.unit_id(t),
        _ => u32::MAX,
    };
    let mut result = host.write_u32(id);
    if result != 0 {
        return result;
    }
    result = host.write_bool(b.worker_counted);
    if result != 0 {
        return result;
    }
    result = host.write_bool(b.first_tick_adjusted);
    if result != 0 {
        return result;
    }
    result = host.write_i32(b.first_tick_amount);
    if result == 0 {
        result = host.write_bool(b.repair);
    }
    if result == 0 && save_has_live_state(major, minor) {
        result = save_point(b.location, host);
        if result == 0 {
            result = save_point(b.center, host);
        }
    }
    result
}

/// `Load`'s part after the base goal's (the building's id resolved by `repair`).
pub fn load(goal: u64, major: u32, minor: u32, host: &mut dyn BuildHost) -> i32 {
    let mut b = get(goal);
    let mut id = 0u32;
    let mut result = host.read_u32(&mut id);
    if result != 0 {
        return result;
    }
    b.target = Held::Saved(id);
    put(goal, b);
    result = host.read_bool(&mut b.worker_counted);
    if result == 0 {
        result = host.read_bool(&mut b.first_tick_adjusted);
    }
    if result == 0 {
        result = host.read_i32(&mut b.first_tick_amount);
        if result == 0 {
            result = host.read_bool(&mut b.repair);
        }
        if result == 0 && save_has_live_state(major, minor) {
            result = load_point(&mut b.location, host);
            if result == 0 {
                result = load_point(&mut b.center, host);
            }
        }
    }
    put(goal, b);
    result
}

/// `RepairPointers`: the building from its id (referenced) and its player, class, technology, node, size and (before
/// 0.254) site; 3 when the building or its technology is missing. Saves up to 0.76 clear the builder-counted flag.
pub fn repair(goal: u64, major: u32, minor: u32, host: &mut dyn BuildHost) -> i32 {
    let mut b = get(goal);
    let mut result = 3;
    if b.target.saved_id() != u32::MAX {
        let t = host.find_unit(b.target.saved_id(), false);
        b.target = Held::of(t);
        if t != 0 {
            host.reference(t, false);
            b.player = host.build_unit(t).player;
            take_target(&mut b, t, host);
            result = if b.technology != TECH_NONE { 0 } else { 3 };
            let p = position(t, host);
            if !save_has_live_state(major, minor) {
                b.set_location(p[0], p[1]);
            }
        }
    } else {
        b.target = Held::None;
    }
    if minor <= VERSION_WORKER_COUNT {
        b.worker_counted = false;
    }
    put(goal, b);
    result
}

/// `Suits`: citizens only
fn suits(unit: u64, host: &mut dyn BuildHost) -> bool {
    let category = host.build_unit(unit).category;
    category == CATEGORY_LAND_CITIZEN || category == CATEGORY_NAVAL_CITIZEN
}

/// The goal's body `op` (rust_goal.rs `EE_UNITS_GOAL_*`).
pub fn body(op: i32, goal: u64, a: u64, b: u64, io: &mut EeUnitsGoalIo, host: &mut dyn BuildHost) -> i64 {
    use crate::rust_goal::*;
    match op {
        EE_UNITS_GOAL_NEW_BUILD => create(goal, a, b, host),
        EE_UNITS_GOAL_BUILD_NEW_EMPTY => put(goal, Build::empty()),
        EE_UNITS_GOAL_DESTROY => destroy(goal, host),
        EE_UNITS_GOAL_CLONE => {
            let s = get(goal);
            let copy = host.new_goal(GOAL_BUILD_REPAIR, b != 0);
            create(copy, s.player, s.target.object(), host);
            return copy as i64;
        }
        EE_UNITS_GOAL_CHECKSUM => return checksum(goal, host),
        EE_UNITS_GOAL_DUMP => dump(goal, host),
        EE_UNITS_GOAL_DEBUG_LINE => debug_line(goal, host),
        EE_UNITS_GOAL_DEBUG_TEXT => debug_text(goal, host),
        EE_UNITS_GOAL_LOCATION => io.f[..3].copy_from_slice(&get(goal).location),
        EE_UNITS_GOAL_DESTINATION => {
            let mut out = [io.f[0], io.f[1], io.f[2]];
            destination(goal, a, &mut out, host);
            io.f[..3].copy_from_slice(&out);
        }
        EE_UNITS_GOAL_START_ACTION => return ACTION_MOVE_TO_REPAIR_SITE,
        EE_UNITS_GOAL_MAX_PATH_ATTEMPTS => return 3,
        EE_UNITS_GOAL_SUITS => return i64::from(suits(a, host)),
        EE_UNITS_GOAL_STEP => step(goal, a, host),
        EE_UNITS_GOAL_WORK => work(goal, a, host),
        EE_UNITS_GOAL_START => start(goal, a, host),
        EE_UNITS_GOAL_SAVE => return i64::from(save(goal, a as u32, b as u32, host)),
        EE_UNITS_GOAL_LOAD => return i64::from(load(goal, a as u32, b as u32, host)),
        EE_UNITS_GOAL_REPAIR => return i64::from(repair(goal, a as u32, b as u32, host)),
        EE_UNITS_GOAL_EVALUATE => return i64::from(evaluate(goal, a, b as i64, host)),
        EE_UNITS_GOAL_REPLACE_UNIT => replace_unit(goal, a, b, host),
        EE_UNITS_GOAL_ABANDON => abandon(goal, a, host),
        EE_UNITS_GOAL_BUILD_FIELDS => {
            let s = get(goal);
            io.i[0] = i64::from(s.target_type);
            io.i[1] = i64::from(s.technology);
            io.i[2] = s.node as i64;
            io.i[3] = s.class as i64;
            return s.target.object() as i64;
        }
        EE_UNITS_GOAL_BUILD_SITE_TAKEN => {
            let s = get(goal);
            return i64::from(host.site_taken(s.target.object(), s.player, s.target_type));
        }
        // SetOwner, AddCreatedUnit: the base's (empty)
        _ => {}
    }
    0
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::rust_goal::*;
    use std::collections::BTreeMap as Map;

    /// Units by handle, the goals' nodes, a log of the world calls and the file.
    #[derive(Default)]
    struct World {
        log: Vec<String>,
        text: String,
        file: Vec<String>,
        reads: Vec<i64>,
        units: Map<u64, EeUnitsBuildUnit>,
        refs: Map<u64, i64>,
        commands: Map<u64, i64>,
        positions: Map<u64, [f32; 3]>,
        free_tile: Option<[f32; 3]>,
        surrounding: Vec<(i32, i32)>,
        work_reach: bool,
        afford: bool,
        friendly: bool,
        clear: bool,
        queued: bool,
        users: i32,
        goal_state: u64,
    }

    const GOAL: u64 = 0x9000;
    const P: u64 = 0x50;
    const NODE: u64 = 0x60;
    const CLASS: u64 = 0x70;
    const B: u64 = 0x100;
    const W: u64 = 0x200;

    impl World {
        fn unit(&mut self, u: u64) -> &mut EeUnitsBuildUnit {
            self.units.entry(u).or_default()
        }
    }

    impl GoalHost for World {
        fn reference(&mut self, o: u64, _g: bool) {
            *self.refs.entry(o).or_default() += 1;
        }
        fn dereference(&mut self, o: u64, _g: bool) {
            *self.refs.entry(o).or_default() -= 1;
        }
        fn unit_id(&mut self, u: u64) -> u32 {
            (u >> 8) as u32 | 0x0100_0000
        }
        fn unit_tile(&mut self, u: u64) -> (i64, i64) {
            let x = self.unit(u);
            (i64::from(x.tile_x), i64::from(x.tile_y))
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
            if id == 0x0100_0001 {
                B
            } else {
                0
            }
        }
        fn unit_position(&mut self, u: u64) -> [f32; 3] {
            self.positions.get(&u).copied().unwrap_or([0.0; 3])
        }
        fn tile_location(&mut self, x: i64, y: i64) -> [f32; 3] {
            [x as f32, y as f32, 0.0]
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
        fn drop_path(&mut self, u: u64) {
            self.log.push(format!("drop path {u:x}"));
        }
        fn new_goal(&mut self, kind: u8, arg: bool) -> u64 {
            self.log.push(format!("new goal {kind} {arg}"));
            fresh()
        }
        fn file_open(&mut self) -> bool {
            true
        }
        fn write_u32(&mut self, v: u32) -> i32 {
            self.file.push(format!("u{v:x}"));
            0
        }
        fn write_i32(&mut self, v: i32) -> i32 {
            self.file.push(format!("i{v}"));
            0
        }
        fn write_bool(&mut self, v: bool) -> i32 {
            self.file.push(format!("b{}", i32::from(v)));
            0
        }
        fn read_u32(&mut self, v: &mut u32) -> i32 {
            *v = self.reads.remove(0) as u32;
            0
        }
        fn read_i32(&mut self, v: &mut i32) -> i32 {
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
        fn append_class_name(&mut self, _u: u64) {
            self.text.push_str("House");
        }
    }

    impl BuildHost for World {
        fn build_unit(&mut self, u: u64) -> EeUnitsBuildUnit {
            if u == 0 {
                return EeUnitsBuildUnit::default();
            }
            *self.unit(u)
        }
        fn tech_node(&mut self, _p: u64, t: i32) -> u64 {
            if t == TECH_NONE {
                0
            } else {
                NODE
            }
        }
        fn node_info(&mut self, n: u64) -> (i32, i32, i32) {
            if n == NODE {
                (77, 3, 40)
            } else {
                (0, 0, 0)
            }
        }
        fn player_id(&mut self, p: u64) -> i32 {
            if p == P {
                2
            } else {
                0
            }
        }
        fn class_type(&mut self, c: u64) -> i32 {
            if c == CLASS {
                11
            } else {
                0
            }
        }
        fn builders_add(&mut self, u: u64, d: i32) {
            self.unit(u).builders += d;
            self.log.push(format!("builders {u:x} {d}"));
        }
        fn attack_part(&mut self, _u: u64, t: u64) -> u64 {
            t
        }
        fn in_work_reach(&mut self, _u: u64, _t: u64) -> bool {
            self.work_reach
        }
        fn surrounding_tiles(&mut self, _t: u64) -> Vec<(i32, i32)> {
            self.surrounding.clone()
        }
        fn complete_building(&mut self, w: u64, o: u64, t: i32) {
            self.unit(o).built = true;
            self.log.push(format!("complete {w:x} {o:x} {t}"));
        }
        fn play_effect(&mut self, u: u64, e: i32) {
            self.log.push(format!("effect {u:x} {e}"));
        }
        fn set_production_time(&mut self, u: u64, t: i32) {
            self.log.push(format!("production {u:x} {t}"));
        }
        fn set_work_timer(&mut self, u: u64, t: i32) {
            self.log.push(format!("timer {u:x} {t}"));
        }
        fn face_target(&mut self, u: u64, t: u64) {
            self.log.push(format!("face {u:x} {t:x}"));
        }
        fn fill_parts(&mut self, o: u64) {
            self.log.push(format!("fill {o:x}"));
        }
        fn next_free_tile(&mut self, _u: u64, _t: u64) -> Option<[f32; 3]> {
            self.free_tile
        }
        fn cancel_foundation(&mut self, o: u64, w: u64, n: u64) {
            self.log.push(format!("cancel {o:x} {w:x} {n:x}"));
        }
        fn location_goal(&mut self, u: u64, p: [f32; 3]) {
            self.log.push(format!("location goal {u:x} {p:?}"));
        }
        fn help_build_nearby(&mut self, u: u64, t: u64) {
            self.log.push(format!("help {u:x} {t:x}"));
        }
        fn tend_next_farm(&mut self, u: u64, t: u64) {
            self.log.push(format!("tend {u:x} {t:x}"));
        }
        fn gather_near(&mut self, u: u64, t: u64) {
            self.log.push(format!("gather near {u:x} {t:x}"));
        }
        fn gate_side_part(&mut self, _t: u64) -> bool {
            false
        }
        fn build_event(&mut self, p: u64, t: u64, r: bool) {
            self.log.push(format!("event {p:x} {t:x} {r}"));
        }
        fn abandon_building(&mut self, u: u64, t: u64) {
            self.log.push(format!("abandoned {u:x} {t:x}"));
        }
        fn halt(&mut self, u: u64, a: i64) {
            self.log.push(format!("halt {u:x} {a}"));
        }
        fn site_taken(&mut self, _t: u64, _p: u64, _k: i32) -> bool {
            true
        }
        fn footprint_clear(&mut self, _t: u64, _p: u64, _k: i32) -> bool {
            self.clear
        }
        fn site_free(&mut self, _t: u64, _p: u64, _k: i32) -> bool {
            self.clear
        }
        fn other_goal_queued(&mut self, _u: u64, _g: u64) -> bool {
            self.queued
        }
        fn resource_users(&mut self, _u: u64) -> i32 {
            self.users
        }
        fn new_gather(&mut self, r: u64, s: u64, a: bool) -> u64 {
            self.log.push(format!("new gather {r:x} {s:x} {a}"));
            0x7777
        }
        fn set_goal(&mut self, u: u64, g: u64, q: i32, f: i32) {
            self.log.push(format!("set goal {u:x} {g:x} {q} {f}"));
        }
        fn nearest_site(&mut self, _u: u64) -> u64 {
            0
        }
        fn clear_site(&mut self, t: u64) {
            self.log.push(format!("clear site {t:x}"));
        }
        fn can_produce(&mut self, _t: u64, _o: u64) -> bool {
            false
        }
        fn can_afford_repair(&mut self, _u: u64, _o: u64, a: i32) -> bool {
            self.log.push(format!("afford {a}"));
            self.afford
        }
        fn friendly(&mut self, _p: u64, _u: u64) -> bool {
            self.friendly
        }
        fn in_cinematic(&mut self) -> bool {
            false
        }
        fn set_paid(&mut self, o: u64) {
            self.unit(o).paid_for = true;
            self.log.push(format!("paid {o:x}"));
        }
        fn pay(&mut self, p: u64, n: u64, f: f32) {
            self.log.push(format!("pay {p:x} {n:x} {f}"));
        }
        fn set_hit_points(&mut self, u: u64, h: i32) {
            self.unit(u).hit_points = h;
            self.log.push(format!("hit points {u:x} {h}"));
        }
        fn foundation_started(&mut self, o: u64, t: u64, w: bool) {
            self.unit(o).foundation = false;
            self.log.push(format!("foundation {o:x} {t:x} {w}"));
        }
        fn goal_arg(&mut self, _g: u64) -> bool {
            true
        }
        fn deposit(&mut self, u: u64, t: u64) {
            self.log.push(format!("deposit {u:x} {t:x}"));
        }
        fn goal_state(&mut self, _u: u64) -> u64 {
            self.goal_state
        }
        fn write_f32(&mut self, v: f32) -> i32 {
            self.file.push(format!("f{v}"));
            0
        }
        fn read_f32(&mut self, v: &mut f32) -> i32 {
            *v = f32::from_bits(self.reads.remove(0) as u32);
            0
        }
    }

    /// A house at (10, 20) of size 2, 100 of 400 hit points, unbuilt; a citizen worker.
    fn world() -> World {
        let mut w = World { work_reach: true, afford: true, friendly: true, ..Default::default() };
        *w.unit(B) = EeUnitsBuildUnit {
            complex: B,
            unit_class: CLASS,
            player: P,
            first: B,
            unit_type: 11,
            tile_size: 2,
            category: 3,
            table_technology: 500,
            hit_points: 100,
            max_hit_points: 400,
            tile_x: 10,
            tile_y: 20,
            build_time_scale: 1.0,
            tile_node: true,
            tile: true,
            valid: true,
            paid_for: true,
            ..Default::default()
        };
        *w.unit(W) = EeUnitsBuildUnit {
            complex: W,
            player: P,
            first: W,
            category: CATEGORY_LAND_CITIZEN,
            tile_x: 9,
            tile_y: 20,
            repair_rate: 2,
            work_rate: 1000,
            line_of_sight: 4.0,
            location: [9.5, 20.5, 0.0],
            human: true,
            ..Default::default()
        };
        w.positions.insert(B, [10.5, 20.5, 0.0]);
        w
    }

    /// a goal handle no other test uses (the tests run at once over the one table)
    fn fresh() -> u64 {
        static NEXT: std::sync::atomic::AtomicU64 = std::sync::atomic::AtomicU64::new(GOAL);
        NEXT.fetch_add(0x10, std::sync::atomic::Ordering::Relaxed)
    }

    fn goal(w: &mut World) -> u64 {
        let g = fresh();
        create(g, P, B, w);
        g
    }

    #[test]
    fn creation_copy_and_texts() {
        let mut w = world();
        let g = goal(&mut w);
        let b = get(g);
        assert_eq!(b.target, Held::Object(B));
        assert_eq!(b.location, [10.5, 20.5, 0.0]);
        assert_eq!(b.center, [11.5, 21.5, 0.0]);
        assert_eq!((b.technology, b.target_type, b.scale, b.node, b.repair), (500, 11, 2, NODE, false));
        assert_eq!(w.refs[&B], 1);
        // a built building is repaired (never a farm)
        w.unit(B).built = true;
        let r = fresh();
        create(r, P, B, &mut w);
        assert!(get(r).repair);
        destroy(r, &mut w);
        let mut io = EeUnitsGoalIo::default();
        let copy = body(EE_UNITS_GOAL_CLONE, g, 0, 1, &mut io, &mut w) as u64;
        assert_eq!(get(copy).target, Held::Object(B));
        assert_eq!(w.refs[&B], 2);
        debug_line(g, &mut w);
        assert_eq!(w.text, "build at 10,20, obj 11 (false):");
        w.text.clear();
        debug_text(g, &mut w);
        assert_eq!(w.text, "BuildRepair: at 10,20\nObject:House\nConstructionWorkerCountIncremented:false\n");
        w.text.clear();
        dump(g, &mut w);
        assert!(w
            .text
            .starts_with("OUT OF SYNCH DUMP:  EEUGBuildRepair Target Location (10.500000, 20.500000, 0.000000)\n"));
        assert!(w.text.contains("EEUGBuildRepair Target Unit ID #01000001\n"));
        assert!(w.text.contains("EEUGBuildRepair Target Tech Tree Node ID 77\n"));
        let expected = (10.5f32.to_bits() as i32)
            .wrapping_add(20.5f32.to_bits() as i32)
            .wrapping_add(11.5f32.to_bits() as i32)
            .wrapping_add(21.5f32.to_bits() as i32)
            .wrapping_add(2 + 0x0100_0001 + 500 + 11 + 2 + 11 + 77);
        assert_eq!(checksum(g, &mut w), i64::from(expected));
        destroy(copy, &mut w);
        destroy(g, &mut w);
        assert_eq!(w.refs[&B], 0);
    }

    #[test]
    fn work_ticks_and_the_first_tick_correction() {
        let mut w = world();
        let g = goal(&mut w);
        // 400 hp over 40 s, factor 1, 1 builder: 400 / (40 * 1) = 10 a second, the work rate 1000: 10000 -> 400 max
        work(g, W, &mut w);
        assert_eq!(w.unit(B).builders, 1);
        assert!(get(g).worker_counted);
        let first = get(g).first_tick_amount;
        // rate = 400 / (40 * (3 / 3)) = 10; ceil(1000 * 10 / 1) = 10000, capped by max 400, then by the 300 missing
        assert_eq!(first, 300);
        assert_eq!(w.unit(B).hit_points, 400);
        assert!(w.log.contains(&"production 200 0".to_string()));
        assert!(w.log.contains(&"timer 200 1000".to_string()));
        // a second worker's preview leaves the counting alone
        w.unit(B).hit_points = 200;
        let amount = build_amount(g, W, true, &mut w);
        assert!(get(g).worker_counted && !get(g).first_tick_adjusted);
        assert_eq!(amount, 200);
        // the second real tick is corrected by the first's
        let amount = build_amount(g, W, false, &mut w);
        assert_eq!(amount, 200 * 2 - 300);
        assert!(get(g).first_tick_adjusted);
        destroy(g, &mut w);
        assert_eq!(w.unit(B).builders, 0);
    }

    #[test]
    fn repairs_pay_and_foundations_start() {
        let mut w = world();
        w.unit(B).built = true;
        let g = goal(&mut w);
        assert!(get(g).repair);
        // repair: ceil(2 * 0.01 * 400) = 8; ceil(3 / (1 + 2) * 8) with this builder counted = 8
        work(g, W, &mut w);
        assert!(w.log.contains(&"afford 8".to_string()));
        assert_eq!(w.unit(B).hit_points, 108);
        assert!(w.log.contains(&format!("pay 50 60 {}", 8.0f32 / 400.0)));
        destroy(g, &mut w);
        // a foundation at 1 hit point of a farm: paid for, started
        let mut w = world();
        let u = w.unit(B);
        u.unit_type = UNIT_TYPE_FARM;
        u.hit_points = 1;
        u.paid_for = false;
        u.foundation = true;
        let g = goal(&mut w);
        work(g, W, &mut w);
        let i = w.log.iter().position(|l| l == "paid 100").unwrap();
        assert_eq!(w.log[i + 1], "pay 50 60 1");
        assert!(w.log.contains(&"foundation 100 100 false".to_string()));
        destroy(g, &mut w);
    }

    #[test]
    fn steps_and_triggers() {
        let mut w = world();
        let g = goal(&mut w);
        w.commands.insert(W, ACTION_REPAIR_BUILDING);
        step(g, W, &mut w);
        assert_eq!(
            w.log,
            [
                "effect 200 500",
                "face 200 100",
                "animation 200 1",
                "timer 200 1000",
                // 40 * (300 / 400)
                "production 200 30",
                "drop path 200",
                "halt 200 6"
            ]
        );
        w.log.clear();
        w.commands.insert(W, ACTION_WAIT_FOR_BUILD_SITE_TO_CLEAR);
        step(g, W, &mut w);
        assert_eq!(w.log, ["halt 200 3", "animation 200 7", "clear site 100"]);
        w.log.clear();
        w.commands.insert(W, ACTION_TASK_SMART_CITIZEN);
        step(g, W, &mut w);
        assert_eq!(w.log, ["help 200 100"]);
        w.log.clear();
        w.queued = true;
        step(g, W, &mut w);
        assert!(w.log.is_empty());
        w.commands.insert(W, ACTION_CONVERT_TO_LOCATION_GOAL);
        step(g, W, &mut w);
        assert_eq!(w.log, ["location goal 200 [10.5, 20.5, 0.0]"]);
        w.log.clear();
        // triggers
        assert!(!evaluate(g, W, TRIGGER_BUILDING_DESTROYED, &mut w));
        assert!(evaluate(g, W, TRIGGER_GOAL_IS_BUILD, &mut w));
        w.surrounding = vec![(9, 20)];
        assert!(evaluate(g, W, TRIGGER_REPAIR_SITE_IN_RANGE, &mut w));
        w.surrounding = vec![(9, 21)];
        assert!(!evaluate(g, W, TRIGGER_REPAIR_SITE_IN_RANGE, &mut w));
        // within 4 * 4 + 2 of the centre (11.5, 21.5): 4 + 1 = 5
        assert!(evaluate(g, W, TRIGGER_BUILD_SITE_VISIBLE, &mut w));
        assert!(!evaluate(g, W, TRIGGER_BUILD_SITE_CLEAR, &mut w));
        assert_eq!(w.log, ["clear site 100"]);
        assert!(evaluate(g, W, TRIGGER_UNIT_FACING_NOT_REQUIRED, &mut w));
        // not finished yet
        assert!(!evaluate(g, W, TRIGGER_BUILDING_REPAIRED, &mut w));
        w.unit(B).hit_points = 400;
        w.goal_state = g;
        w.log.clear();
        assert!(evaluate(g, W, TRIGGER_BUILDING_REPAIRED, &mut w));
        assert_eq!(w.log, ["complete 200 100 11", "effect 200 100000", "production 200 0", "fill 100"]);
        destroy(g, &mut w);
    }

    #[test]
    fn replacing_the_building_and_next_work() {
        let mut w = world();
        let g = goal(&mut w);
        let n: u64 = 0x300;
        *w.unit(n) = EeUnitsBuildUnit { complex: n, ..Default::default() };
        replace_unit(g, B, n, &mut w);
        assert_eq!(get(g).target, Held::Object(n));
        assert_eq!((w.refs[&B], w.refs[&n]), (0, 1));
        w.friendly = false;
        replace_unit(g, n, B, &mut w);
        assert_eq!(get(g).target, Held::None);
        destroy(g, &mut w);
        // a farm nobody works: a gather goal
        let mut w = world();
        w.unit(B).unit_type = UNIT_TYPE_FARM;
        let g = goal(&mut w);
        w.commands.insert(W, ACTION_TASK_SMART_CITIZEN);
        step(g, W, &mut w);
        assert_eq!(w.log, ["new gather 100 0 false", "set goal 200 7777 0 0", "start 200 8008"]);
        destroy(g, &mut w);
    }

    #[test]
    fn save_load_and_repair() {
        let mut w = world();
        let g = goal(&mut w);
        assert_eq!(save(g, 0, 254, &mut w), 0);
        assert_eq!(w.file, ["u1000001", "b0", "b0", "i0", "b0", "f10.5", "f20.5", "f0", "f11.5", "f21.5", "f0"]);
        w.file.clear();
        assert_eq!(save(g, 0, 253, &mut w), 0);
        assert_eq!(w.file.len(), 5);
        destroy(g, &mut w);
        // a 0.253 save: the site from the building's position
        let l = fresh();
        put(l, Build::empty());
        w.reads = vec![0x0100_0001, 1, 0, 5, 0];
        assert_eq!(load(l, 0, 253, &mut w), 0);
        assert_eq!(get(l).target, Held::Saved(0x0100_0001));
        assert_eq!(repair(l, 0, 253, &mut w), 0);
        let b = get(l);
        assert_eq!((b.target, b.player, b.node, b.first_tick_amount), (Held::Object(B), P, NODE, 5));
        assert!(b.worker_counted);
        assert_eq!(b.location, [10.5, 20.5, 0.0]);
        destroy(l, &mut w);
        // a 0.254 save keeps the saved site; an old one (0.76) clears the count; a gone building: 3
        let l = fresh();
        put(l, Build::empty());
        let f = |v: f32| i64::from(v.to_bits());
        w.reads = vec![0x0100_0001, 1, 0, 5, 0, f(3.0), f(4.0), f(0.0), f(4.0), f(5.0), f(0.0)];
        assert_eq!(load(l, 0, 254, &mut w), 0);
        assert_eq!(repair(l, 0, 254, &mut w), 0);
        assert_eq!(get(l).location, [3.0, 4.0, 0.0]);
        destroy(l, &mut w);
        let l = fresh();
        put(l, Build::empty());
        w.reads = vec![0x0100_0099, 1, 0, 5, 0];
        assert_eq!(load(l, 0, 0x4c, &mut w), 0);
        assert_eq!(repair(l, 0, 0x4c, &mut w), 3);
        assert_eq!((get(l).target, get(l).worker_counted), (Held::None, false));
        destroy(l, &mut w);
    }
}
