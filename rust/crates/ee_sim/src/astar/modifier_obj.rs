//! The path goals' modifiers whole (AI Thread cluster, round 4): the C++ classes `EEAIPathGoalModifierBase` and its
//! tile modifiers (the path manager's sea and space modifiers, the bounded goal's `EEAIBPGModifierSea` / `Space`,
//! `EnemyIsAttackableByUnit`, `WallsDoNotBlock`, the computer player's wall pather, the storm's shaper) and
//! `EEAIGraphPathGoalModifierBase` with its route planners (`EEAIGPModifierEnemyAvoidance`, the computer player's
//! wall planner, the storms' rain and snow planners), as one Rust object, `EeAStarModifier`: a reference count, the
//! goal records it was attached to (the searches' `EeAStarPathGoalRec` / `EeAStarGraphGoalRec`) and its kind with its
//! members. A modifier's address is its handle (the C++ makers take one: `ee_astar_modifier_new_*`, and drop it with
//! `ee_astar_modifier_release`); the searches call it directly.
//!
//! The slots' bodies are `modifiers.rs` (the tile tests and costs, over a quiet world view) and the route planners'
//! below, over the sub sectors' records: their gates (`IsPassable`, `IsGateOpen`, the tiles' units through the world
//! views), their links, and for the enemy avoidance the influence table (`ee_sim::cp::influence`). Two facts stay C++
//! calls: a unit's reference count (the attackable modifier references both units) and the world's influence table
//! (`ee_astar_mod_host_*`, EEAIAStarBoundedPathGoalModifiers.cpp); the snow planner's hill is the continent manager's
//! `GetHillAt` (its CNTQ record).

use crate::astar::cm_io;
use crate::astar::ffi::CView;
use crate::astar::graph_search::EeAStarGraphGoalRec;
use crate::astar::modifiers::{self, EeAStarStepGoal, EeAStarStorm, Unit};
use crate::astar::path_search::EeAStarPathGoalRec;
use crate::astar::pm::EePathSavedGraph;
use crate::astar::storage::AStarRect;
use crate::astar::world_view::{self, EeAStarWorldView, CATEGORY_GATE};
use crate::world::{PlayerView, UnitView, WorldRead, WorldView};
use std::ffi::{c_long, c_void};
use std::ptr;

#[cfg(not(test))]
extern "C" {
    fn ee_astar_mod_host_unit_reference(unit: u64, add: bool);
    fn ee_astar_mod_host_influence(region_size: *mut i32) -> *const c_void;
}
use crate::astar::world_view::ee_astar_view_cm_hill_at;

#[cfg(test)]
mod cpp {
    use std::cell::RefCell;
    thread_local! {
        /// the unit references the tests' modifiers took (+1) and dropped (-1)
        pub(crate) static UNIT_REFS: RefCell<Vec<(u64, i32)>> = const { RefCell::new(Vec::new()) };
    }
    pub(super) unsafe fn ee_astar_mod_host_unit_reference(unit: u64, add: bool) {
        UNIT_REFS.with(|r| r.borrow_mut().push((unit, if add { 1 } else { -1 })));
    }
    pub(super) unsafe fn ee_astar_mod_host_influence(_region_size: *mut i32) -> *const std::ffi::c_void {
        std::ptr::null()
    }
}
#[cfg(test)]
use cpp::*;

/// `EECPASGWeights` and the player and enemy of an influence-cost route planner
/// (`EEAIGPGModiferUseInfluenceCostsBase`).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub(crate) struct Influence {
    pub weights: [f32; 15],
    pub player: u32,
    pub enemy: u32,
}

/// `EEAIBPGModifierStormShaper`'s members.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub(crate) struct StormShaper {
    pub inner: AStarRect,
    pub outer: AStarRect,
    pub distance_squared: i32,
    pub steps: i32,
    pub max_steps: i32,
    pub snow: bool,
    pub rain: bool,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub(crate) enum Kind {
    /// the path manager's sea and space modifiers of an unbounded goal (`EEAIPathGoalModifierSea` / `Space`)
    PlainSea,
    PlainSpace,
    /// `EEAIBPGModifierSea` / `Space`
    BoundedSea,
    BoundedSpace,
    /// `EEAIBPGModifierEnemyIsAttackableByUnit`: the walker and its target (both referenced), the range squared
    Attackable { unit: u64, target: u64, range_squared: f32 },
    /// `EEAIBPGModifierWallsDoNotBlock`
    WallsDoNotBlock,
    /// `EEAIGPGModifierComputerPlayerWallPather`
    WallPather,
    /// `EEAIBPGModifierStormShaper`
    Storm(StormShaper),
    /// `EEAIGPModifierEnemyAvoidance`, `EEAIGPGModifierComputerPlayerWallPlanner`
    EnemyAvoidance(Influence),
    WallPlanner(Influence),
    /// `EEAIGPGModifierStormRoutePlannerRain` / `Snow`
    StormRain,
    StormSnow,
}

impl Kind {
    /// An `EEAIBoundedPathGoalModifierBase` (the bounded goal's `dynamic_cast` succeeds).
    fn bounded(&self) -> bool {
        matches!(
            self,
            Kind::BoundedSea | Kind::BoundedSpace | Kind::Attackable { .. } | Kind::WallsDoNotBlock | Kind::WallPather
                | Kind::Storm(_)
        )
    }
}

/// A modifier (opaque to C++).
pub struct EeAStarModifier {
    refs: u32,
    /// `mGoal` (a tile modifier's)
    goal: *mut EeAStarPathGoalRec,
    /// `mBoundedGoal`
    bounded_goal: *mut EeAStarPathGoalRec,
    /// `mGoal` (a route planner's)
    graph_goal: *mut EeAStarGraphGoalRec,
    pub(crate) kind: Kind,
}

/// The saved graph modifier kinds (pm.rs `EE_PATH_MODIFIER_*`; 3..8 were the deleted attack sub groups' planners).
pub(crate) const SAVED_NONE: i64 = 0;
pub(crate) const SAVED_ENEMY_AVOIDANCE: i64 = 1;
pub(crate) const SAVED_WALL_PLANNER: i64 = 2;
pub(crate) const SAVED_STORM_RAIN: i64 = 9;
pub(crate) const SAVED_STORM_SNOW: i64 = 10;

/// The machine types with a modifier of their own (`EEAIMachineType`: water 2, space 0x10).
const MACHINE_WATER: i64 = 2;
const MACHINE_SPACE: i64 = 0x10;

/// `kAITileLand`, `kAITileWall`, `kAITileGate`
const TILE_LAND: c_long = 1;
const TILE_WALL: c_long = 0xd;
const TILE_GATE: c_long = 0xe;
/// `kActionClosed`
const ACTION_CLOSED: u32 = 15001;

pub(crate) fn new(kind: Kind) -> *mut EeAStarModifier {
    Box::into_raw(Box::new(EeAStarModifier {
        refs: 1,
        goal: ptr::null_mut(),
        bounded_goal: ptr::null_mut(),
        graph_goal: ptr::null_mut(),
        kind,
    }))
}

/// `Reference()`.
///
/// # Safety
/// `m` null or a live modifier.
pub(crate) unsafe fn reference(m: *mut EeAStarModifier) {
    if let Some(m) = unsafe { m.as_mut() } {
        m.refs += 1;
    }
}

/// `Dereference()`: the last reference frees it (the attackable modifier drops its units).
///
/// # Safety
/// `m` null or a live modifier; not used after its last reference.
pub(crate) unsafe fn release(m: *mut EeAStarModifier) {
    let Some(r) = (unsafe { m.as_mut() }) else { return };
    r.refs -= 1;
    if r.refs != 0 {
        return;
    }
    // SAFETY: made by `new`, its last reference
    let b = unsafe { Box::from_raw(m) };
    if let Kind::Attackable { unit, target, .. } = b.kind {
        // SAFETY: the units the modifier referenced
        unsafe {
            ee_astar_mod_host_unit_reference(unit, false);
            ee_astar_mod_host_unit_reference(target, false);
        }
    }
}

// ---- the tile modifiers (the path searches: path_search.rs)

/// `EE_PATH_TILE_MODIFIER_NEW` before: the machine type's modifier over `goal` (one reference, the caller's), null
/// for a type without one.
pub(crate) fn new_for_machine(machine_type: i64, bounded: bool, goal: *mut EeAStarPathGoalRec) -> *mut EeAStarModifier {
    let kind = match (machine_type, bounded) {
        (MACHINE_WATER, false) => Kind::PlainSea,
        (MACHINE_WATER, true) => Kind::BoundedSea,
        (MACHINE_SPACE, false) => Kind::PlainSpace,
        (MACHINE_SPACE, true) => Kind::BoundedSpace,
        _ => return ptr::null_mut(),
    };
    let m = new(kind);
    // SAFETY: just made
    let r = unsafe { &mut *m };
    r.goal = goal;
    if bounded {
        r.bounded_goal = goal;
    }
    m
}

/// `SetModifier`'s attach: the goal (`mGoal`); as the bounded goal's (`SetGoal`, after its `dynamic_cast`) both
/// goals, and the bounded modifier (null for one that is not).
///
/// # Safety
/// `m` a live modifier.
pub(crate) unsafe fn attach(m: *mut EeAStarModifier, goal: *mut EeAStarPathGoalRec, as_bounded: bool) -> *mut EeAStarModifier {
    let r = unsafe { &mut *m };
    if !as_bounded {
        r.goal = goal;
        return m;
    }
    if !r.kind.bounded() {
        return ptr::null_mut();
    }
    r.bounded_goal = goal;
    r.goal = goal;
    m
}

/// A quiet view of the world map (the modifiers' `EEAIAStarView(gWorld.GetMap(), true)`).
fn quiet_view() -> EeAStarWorldView {
    // SAFETY: a search runs on a thread that holds the world
    let t = unsafe { WorldRead::begin() };
    let map = WorldView::global(&t).and_then(|w| w.map()).map_or(0, |m| m.handle());
    EeAStarWorldView { map: map as *mut c_void, quiet: true }
}

fn with_view<R>(f: impl FnOnce(&mut CView<'_>) -> R) -> R {
    let mut v = quiet_view();
    // SAFETY: the view lives through the calls
    let cb = unsafe { world_view::ee_astar_world_view_callbacks(&mut v) };
    f(&mut CView(&cb))
}

fn goal_unit(g: *mut EeAStarPathGoalRec) -> Unit {
    // SAFETY: an attached goal record lives while its search uses the modifier
    unsafe { g.as_ref() }.map_or(ptr::null_mut(), |g| g.unit)
}

/// `GetStepCost(sea, ..)` over `mGoal`.
#[allow(clippy::too_many_arguments)]
fn step_cost(r: &EeAStarModifier, sea: bool, fx: i32, fy: i32, x: i32, y: i32, px: i32, py: i32) -> i32 {
    // SAFETY: as `goal_unit`
    let g = unsafe { &*r.goal };
    let goal = EeAStarStepGoal {
        walker: g.unit,
        avoid_units: g.avoid_units,
        is_bounded: g.is_bounded,
        goal_bounds: g.goal_bounds,
    };
    with_view(|v| modifiers::step_cost(v, &goal, sea, fx, fy, x, y, px, py))
}

fn storm_state(s: &StormShaper, bounded_goal: *mut EeAStarPathGoalRec) -> EeAStarStorm {
    // SAFETY: as `goal_unit`
    let start = unsafe { &*bounded_goal }.start;
    EeAStarStorm {
        inner: s.inner,
        outer: s.outer,
        distance_squared: s.distance_squared,
        snow: s.snow,
        rain: s.rain,
        start_x: start.x,
        start_y: start.y,
    }
}

/// The tile modifier's `GetCost` (slot +0x04).
///
/// # Safety
/// `m` a live tile modifier attached to a live goal.
pub(crate) unsafe fn cost(m: *mut EeAStarModifier, fx: i32, fy: i32, x: i32, y: i32, px: i32, py: i32) -> i32 {
    let r = unsafe { &*m };
    match r.kind {
        Kind::PlainSea => step_cost(r, true, fx, fy, x, y, px, py),
        // the bounded ones share the space slot (ICF): GetStepCost(false, ..) at sea too
        Kind::PlainSpace | Kind::BoundedSea | Kind::BoundedSpace => step_cost(r, false, fx, fy, x, y, px, py),
        Kind::Attackable { .. } => 10,
        Kind::WallsDoNotBlock | Kind::WallPather => modifiers::straight_or_diagonal(fx, fy, x, y),
        Kind::Storm(s) => {
            let st = storm_state(&s, r.bounded_goal);
            with_view(|v| modifiers::storm_cost(v, &st, x, y))
        }
        // (a route planner is no tile modifier: the base's cost)
        _ => 1,
    }
}

/// The tile modifier's `CanEnter` (slot +0x08; `tile` the search map's tile, 0 off it).
///
/// # Safety
/// As [`cost`].
pub(crate) unsafe fn can_enter(m: *mut EeAStarModifier, tile: u64, x: i32, y: i32, _at_goal: bool) -> bool {
    let r = unsafe { &*m };
    match r.kind {
        Kind::PlainSea | Kind::PlainSpace => {
            let w = goal_unit(r.goal);
            with_view(|v| modifiers::is_passable(v, w, x, y))
        }
        Kind::BoundedSea | Kind::BoundedSpace => {
            let w = goal_unit(r.bounded_goal);
            with_view(|v| modifiers::is_passable(v, w, x, y))
        }
        Kind::Attackable { unit, .. } => with_view(|v| modifiers::attacker_can_enter(v, unit as Unit, x, y)),
        Kind::WallsDoNotBlock => {
            let w = goal_unit(r.bounded_goal);
            with_view(|v| modifiers::walls_can_enter(v, w, x, y))
        }
        Kind::WallPather => {
            let w = goal_unit(r.bounded_goal);
            with_view(|v| modifiers::wall_pather_can_enter(v, w, x, y))
        }
        Kind::Storm(s) => {
            let st = storm_state(&s, r.bounded_goal);
            with_view(|v| modifiers::storm_can_enter(v, &st, x, y))
        }
        _ => tile != 0,
    }
}

/// `U2DPoint<long>::DistanceToSquaredF`: the x term squared as a float plus the y term's.
pub(crate) fn distance_squared_f(ax: i32, ay: i32, bx: i32, by: i32) -> f32 {
    let dx = ax.wrapping_sub(bx);
    let dy = ay.wrapping_sub(by);
    dx.wrapping_mul(dx) as f32 + dy.wrapping_mul(dy) as f32
}

/// The bounded modifier's `CheckNode` (slot +0x0c; its result unused): the attackable modifier moves the
/// destination and the goal tile onto a node within range of the destination.
///
/// # Safety
/// `m` null or a live bounded modifier attached to a live goal.
pub(crate) unsafe fn check_node(m: *mut EeAStarModifier, node: Option<(i32, i32)>) -> bool {
    let Some(r) = (unsafe { m.as_mut() }) else { return node.is_none() };
    match r.kind {
        Kind::Attackable { range_squared, .. } => {
            let Some((x, y)) = node else { return true };
            // SAFETY: as `goal_unit`
            let g = unsafe { &mut *r.bounded_goal };
            if distance_squared_f(x, y, g.destination.x, g.destination.y) <= range_squared {
                g.destination.x = x;
                g.destination.y = y;
                g.goal = g.destination;
                return true;
            }
            false
        }
        Kind::Storm(s) => node.is_none() || s.steps >= s.max_steps,
        _ => node.is_none(),
    }
}

// ---- the route planners (the graph searches: graph_search.rs)

/// The route planner takes the graph goal `goal` (`mGoal`).
///
/// # Safety
/// `m` a live modifier.
pub(crate) unsafe fn attach_graph(m: *mut EeAStarModifier, goal: *mut EeAStarGraphGoalRec) {
    unsafe { (*m).graph_goal = goal };
}

/// A sub sector's record (`EEAIPathSubSector*` `h`).
fn sub<'a>(h: u64) -> &'a crate::astar::sector_rec::EeAStarSubSectorRecord {
    // SAFETY: a listed sub sector is a live object
    unsafe { cm_io::sub(h) }
}

/// The center of sub sector `h`'s path sector.
fn sector_center(h: u64) -> (c_long, c_long) {
    // SAFETY: a sub sector's sector is a live object
    let s = unsafe { cm_io::sector(sub(h).sector) };
    (s.center.x, s.center.y)
}

/// The default step between sub sectors: 1 inside one sector, else 10 straight or 14 diagonal between the
/// sectors' centers (the wall planner's GetCost and the graph goal's, ICF-folded).
pub(crate) fn sector_step(from: u64, to: u64) -> i32 {
    if sub(from).sector != sub(to).sector {
        let (a, b) = (sector_center(from), sector_center(to));
        if a.0 == b.0 || a.1 == b.1 {
            10
        } else {
            14
        }
    } else {
        1
    }
}

/// `EEAIGraphHasPassableLink`: no link of `sector` to `from`, or one with an end of a kind the goal's flags pass.
///
/// # Safety
/// `goal` a live graph goal record; `sector` and `from` live sub sectors.
pub(crate) unsafe fn has_passable_link(goal: *const EeAStarGraphGoalRec, sector: u64, from: u64) -> bool {
    let flags = unsafe { &*goal }.path_flags;
    let id = sub(from).id;
    // SAFETY: a sub sector's lists
    let links: Vec<_> = unsafe { sub(sector).lists.as_ref() }.map_or_else(Vec::new, |l| l.links_of(id).to_vec());
    let mut result = links.is_empty();
    for l in links.iter().rev() {
        if result {
            break;
        }
        let pass = |h: u64| flags.get(sub(h).kind as usize).copied().unwrap_or(false);
        if pass(l.first) || pass(l.second) {
            result = true;
        }
    }
    result
}

/// The first gate unit's complex unit on one of the tiles of sub sector `h`, from its last tile (`theGate` of the
/// gate tests), and what the tests read of it; a tile with no gate keeps the gate before it (the C++'s variable).
struct Gate {
    alive: bool,
    side_part: bool,
    closed: bool,
    locked: bool,
    player: u64,
}

fn gate_of(t: &WorldRead, u: &UnitView<'_>) -> Gate {
    // `IsSidePart`: the last of several parts, or the middle one of three
    let next = u.next_part_handle();
    let prev = u.previous_part_handle();
    // SAFETY: a gate's parts are live units
    let next_next = unsafe { UnitView::of(t, next) }.map_or(0, |n| n.next_part_handle());
    let middle = next != 0 && next_next == 0 && prev != 0;
    Gate {
        alive: u.hit_points() > 0,
        side_part: (next == 0 && prev != 0) || middle,
        closed: u.command_type() == ACTION_CLOSED,
        locked: crate::world_field!(u.complex(), mIsLocked),
        player: u.player().map_or(0, |p| p.handle()),
    }
}

/// The sub sector's tiles from the last, each tile's first gate (its complex unit; None a tile without one, which
/// keeps the gate before it), until `step` says stop.
fn walk_gates(h: u64, mut step: impl FnMut(Option<&Gate>) -> Option<bool>) -> Option<bool> {
    // SAFETY: a search runs on a thread that holds the world
    let t = unsafe { WorldRead::begin() };
    let map = WorldView::global(&t).and_then(|w| w.map());
    // SAFETY: a sub sector's tile set
    let tiles = unsafe { cm_io::sub_tiles(h) };
    let mut gate: Option<Gate> = None;
    let mut i = tiles.set.size() - 1;
    while i >= 0 {
        let p = tiles.set.get(i);
        i -= 1;
        let Some(tile) = map.and_then(|m| m.tile(i64::from(p.x), i64::from(p.y))) else { continue };
        for u in tile.units().flatten() {
            if u.class().map_or(0, |c| c.category()) != CATEGORY_GATE {
                continue;
            }
            // (a NULL complex unit: no gate)
            gate = u.complex_unit().map(|g| gate_of(&t, &g));
            break;
        }
        if let Some(r) = step(gate.as_ref()) {
            return Some(r);
        }
    }
    None
}

fn player_at(index: u32) -> u64 {
    // SAFETY: a search runs on a thread that holds the world
    let t = unsafe { WorldRead::begin() };
    WorldView::global(&t).and_then(|w| w.player(index as usize)).map_or(0, |p| p.handle())
}

/// Whether `player` (a handle) is `owner` or not its enemy (`owner->mEnemies[player->mPlayerID] == 0`).
fn friendly(player: u64, owner: u64) -> bool {
    if player == owner {
        return true;
    }
    // SAFETY: live players
    let t = unsafe { WorldRead::begin() };
    let (Some(p), Some(o)) = (unsafe { PlayerView::of(&t, player) }, unsafe { PlayerView::of(&t, owner) }) else {
        return false;
    };
    o.enemy(p.id() as usize) == 0
}

/// `EEAIPathSubSector::IsPassable(player)`: whether the sub sector's gate lets the player (an index) through.
pub(crate) fn gate_passable(h: u64, player: u32) -> bool {
    let me = player_at(player);
    let mut passable = false;
    walk_gates(h, |g| {
        let Some(g) = g.filter(|g| g.alive) else {
            passable = true;
            return Some(true);
        };
        passable = if g.side_part {
            false
        } else {
            !g.closed || (friendly(me, g.player) && !g.locked)
        };
        passable.then_some(true)
    });
    passable
}

/// `EEAIGPGModifierComputerPlayerWallPlanner::IsGateOpen(sector, player)`: true when its gate is missing or
/// destroyed, or is the player's or an ally's unlocked main part.
pub(crate) fn gate_open(h: u64, player: u32) -> bool {
    let me = player_at(player);
    let mut blocked = false;
    walk_gates(h, |g| {
        let Some(g) = g.filter(|g| g.alive) else {
            blocked = true;
            return Some(true);
        };
        blocked = friendly(me, g.player) && !g.side_part && !g.locked;
        blocked.then_some(true)
    });
    blocked
}

/// The planners' `CanEnter`: a kind the goal's flags pass, or a gate open to the player (`open`), then a passable
/// link from `from`.
fn planner_can_enter(goal: *const EeAStarGraphGoalRec, sector: u64, from: u64, open: impl FnOnce() -> bool) -> bool {
    // SAFETY: an attached goal lives while its search runs
    let flags = unsafe { &*goal }.path_flags;
    let kind = sub(sector).kind;
    let mut result = flags.get(kind as usize).copied().unwrap_or(false);
    if !result {
        if kind != TILE_GATE {
            return result;
        }
        result = open();
    }
    // SAFETY: a sub sector's lists
    let links = unsafe { sub(sector).lists.as_ref() }.map_or(0, |l| l.links.len());
    if result && from != 0 && links > 0 {
        // SAFETY: as above
        result = unsafe { has_passable_link(goal, sector, from) };
    }
    result
}

/// The route planner's `CanEnter(sector, from)` (slot +0x08).
///
/// # Safety
/// `m` a live route planner attached to a live graph goal; `sector` and `from` (0: none) live sub sectors.
pub(crate) unsafe fn graph_can_enter(m: *mut EeAStarModifier, sector: u64, from: u64) -> bool {
    let r = unsafe { &*m };
    match r.kind {
        Kind::EnemyAvoidance(i) => planner_can_enter(r.graph_goal, sector, from, || gate_passable(sector, i.player)),
        Kind::WallPlanner(i) => planner_can_enter(r.graph_goal, sector, from, || gate_open(sector, i.player)),
        Kind::StormRain | Kind::StormSnow => true,
        _ => sector != 0,
    }
}

/// The enemy influence of the 3x3 regions around tile (x, y) for `player` (`SumEnemyValue`).
fn enemy_near(player: u32, x: c_long, y: c_long) -> c_long {
    let mut size = 0i32;
    // SAFETY: the world's live influence table
    let t = unsafe { ee_astar_mod_host_influence(&mut size) } as *const crate::cp::influence::EeCpInfluence;
    if t.is_null() || size == 0 {
        return 0;
    }
    let (rx, ry) = ((x / size as c_long) as i32, (y / size as c_long) as i32);
    let rect = crate::cp::influence::EeCpRect {
        left: rx.wrapping_sub(1),
        top: ry.wrapping_sub(1),
        right: rx.wrapping_add(1),
        bottom: ry.wrapping_add(1),
    };
    // SAFETY: a search runs on a thread that holds the world
    let world = {
        let tk = unsafe { WorldRead::begin() };
        WorldView::global(&tk).map_or(0, |w| w.handle())
    };
    // SAFETY: the live table and world
    unsafe { crate::cp::ffi::ee_cp_influence_enemies(t, world, player, rect, true, true) }
}

/// The route planner's `GetCost(parent, to)` (slot +0x04; `from` the parent node's sub sector).
///
/// # Safety
/// As [`graph_can_enter`].
pub(crate) unsafe fn graph_cost(m: *mut EeAStarModifier, from: u64, to: u64) -> i32 {
    let r = unsafe { &*m };
    match r.kind {
        Kind::EnemyAvoidance(i) => {
            let loc = sub(to).location;
            let mut cost = sector_step(from, to);
            if enemy_near(i.player, loc.x, loc.y) > 0 {
                cost += 350;
            }
            cost.max(1)
        }
        Kind::WallPlanner(_) => sector_step(from, to),
        Kind::StormRain | Kind::StormSnow => {
            let snow = r.kind == Kind::StormSnow;
            let other = sub(from).sector != sub(to).sector;
            let (a, b) = (sector_center(from), sector_center(to));
            let straight = a.0 == b.0 || a.1 == b.1;
            let kind = sub(to).kind;
            let loc = sub(to).location;
            // SAFETY: the manager's query (its CNTQ record)
            let hill = snow
                && (kind == TILE_LAND || kind == TILE_WALL || kind == TILE_GATE)
                && unsafe { ee_astar_view_cm_hill_at(loc.x as i32, loc.y as i32) } != 0;
            crate::app::storm_ffi::ee_app_storm_route_cost(snow, other, straight, kind, hill) as i32
        }
        _ => 1,
    }
}

/// The route planner's kind in a saved game, with an influence planner's members into `out`.
///
/// # Safety
/// `m` null or a live modifier.
pub(crate) unsafe fn save(m: *mut EeAStarModifier, out: &mut EePathSavedGraph) -> i64 {
    let Some(r) = (unsafe { m.as_ref() }) else { return SAVED_NONE };
    let (kind, i) = match r.kind {
        Kind::EnemyAvoidance(i) => (SAVED_ENEMY_AVOIDANCE, i),
        Kind::WallPlanner(i) => (SAVED_WALL_PLANNER, i),
        Kind::StormRain => return SAVED_STORM_RAIN,
        Kind::StormSnow => return SAVED_STORM_SNOW,
        _ => return -1,
    };
    out.weights = i.weights;
    out.modifier_player = i.player;
    out.modifier_enemy = i.enemy;
    kind
}

/// A saved route planner back (one reference, the caller's), null for a kind that cannot be made.
pub(crate) fn from_saved(s: &EePathSavedGraph) -> *mut EeAStarModifier {
    let i = Influence { weights: s.weights, player: s.modifier_player, enemy: s.modifier_enemy };
    match i64::from(s.modifier) {
        SAVED_ENEMY_AVOIDANCE => new(Kind::EnemyAvoidance(i)),
        SAVED_WALL_PLANNER => new(Kind::WallPlanner(i)),
        SAVED_STORM_RAIN => new(Kind::StormRain),
        SAVED_STORM_SNOW => new(Kind::StormSnow),
        _ => ptr::null_mut(),
    }
}

// ---- the C makers

/// `new EEAIBPGModifierEnemyIsAttackableByUnit(unit, target)`: both units referenced, the range the walker's
/// stats' squared.
///
/// # Safety
/// `unit` and `target` live complex units.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_modifier_new_attackable(unit: u64, target: u64) -> *mut EeAStarModifier {
    unsafe { ee_astar_mod_host_unit_reference(unit, true) };
    unsafe { ee_astar_mod_host_unit_reference(target, true) };
    let range = {
        // SAFETY: the caller's contract
        let t = unsafe { WorldRead::begin() };
        unsafe { UnitView::of(&t, unit) }.and_then(|u| u.stats()).map_or(0.0, |s| s.range())
    };
    new(Kind::Attackable { unit, target, range_squared: range * range })
}

/// `new EEAIBPGModifierWallsDoNotBlock`.
#[no_mangle]
pub extern "C" fn ee_astar_modifier_new_walls() -> *mut EeAStarModifier {
    new(Kind::WallsDoNotBlock)
}

/// `new EEAIGPGModifierComputerPlayerWallPather`.
#[no_mangle]
pub extern "C" fn ee_astar_modifier_new_wall_pather() -> *mut EeAStarModifier {
    new(Kind::WallPather)
}

/// `new EEAIBPGModifierStormShaper` with its rectangles, step limit and kind (no steps, a near radius of 8).
#[no_mangle]
pub extern "C" fn ee_astar_modifier_new_storm_shaper(
    inner: AStarRect,
    outer: AStarRect,
    max_steps: i32,
    rain: bool,
    snow: bool,
) -> *mut EeAStarModifier {
    new(Kind::Storm(StormShaper { inner, outer, distance_squared: 0x40, steps: 0, max_steps, snow, rain }))
}

/// `new EEAIGPModifierEnemyAvoidance(player)` (no weights, no enemy).
#[no_mangle]
pub extern "C" fn ee_astar_modifier_new_enemy_avoidance(player: u32) -> *mut EeAStarModifier {
    new(Kind::EnemyAvoidance(Influence { player, ..Default::default() }))
}

/// `new EEAIGPGModifierComputerPlayerWallPlanner(player, enemy)` (no weights).
#[no_mangle]
pub extern "C" fn ee_astar_modifier_new_wall_planner(player: u32, enemy: u32) -> *mut EeAStarModifier {
    new(Kind::WallPlanner(Influence { player, enemy, ..Default::default() }))
}

/// `new EEAIGPGModifierStormRoutePlannerRain` (`rain`) or `Snow`.
#[no_mangle]
pub extern "C" fn ee_astar_modifier_new_storm_planner(rain: bool) -> *mut EeAStarModifier {
    new(if rain { Kind::StormRain } else { Kind::StormSnow })
}

/// `Dereference()` of the maker's reference.
///
/// # Safety
/// `m` null or a live modifier.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_modifier_release(m: *mut EeAStarModifier) {
    unsafe { release(m) };
}

#[cfg(test)]
#[path = "modifier_obj_tests.rs"]
mod tests;
