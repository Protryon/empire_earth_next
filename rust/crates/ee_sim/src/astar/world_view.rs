//! `EEAIAStarView` whole (AI Thread cluster, round 4): the world as the AI's tile A* searches see it. Every read the
//! goals and the modifiers make of the live world is one callback of `EeAStarViewCallbacks` over an
//! `EeAStarWorldView` (the map and whether the view is quiet), one query per fact of one tile. The searches' capture
//! (ASRN) notes each answer of a view that is not quiet, as the C++ view did (`Note`, the query numbers below).
//!
//! The reads go through the world views (`crate::world`) and the continent manager's Rust record. These facts stay
//! C++ calls: a unit's `IsPassableBy` (a virtual, `ee_units_pm_host_passable_by`), the continent manager's
//! `GetPathAreaAt` and `GetHillAt` (their CNTQ capture records the outermost call: `ee_astar_view_cm_*`), the
//! physics stream's draw (`ee_units_pm_host_random`). The goals' modifiers are Rust objects (`modifier_obj.rs`).
//!
//! The decisions are pure functions over the facts read (`blocks_area`, `resource_kind`, `crowd_step`, ...), which
//! the unit tests check without a world.

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use crate::astar::area_goals::{EeAStarBlocker, EeAStarContinentInfo, EeAStarPathAreaInfo};
use crate::astar::cm::EeAStarCmRecord;
use crate::astar::cm_io;
use crate::astar::ffi::EeAStarViewCallbacks;
use crate::astar::modifier_obj::{self, EeAStarModifier};
use crate::astar::modifiers::{Skip, Unit};
use crate::astar::search_note;
use crate::world::{MapView, TileView, UnitView, WorldRead, WorldView};
use std::ffi::{c_long, c_void};
use std::sync::atomic::{AtomicPtr, Ordering};

// ---- the C++ facts

#[cfg(not(test))]
use crate::units::pm_host::{ee_units_pm_host_passable_by, ee_units_pm_host_random};

#[cfg(not(test))]
extern "C" {
    fn ee_astar_view_cm_path_area_at(x: i32, y: i32) -> u64;
    pub(crate) fn ee_astar_view_cm_hill_at(x: i32, y: i32) -> u64;
}

#[cfg(test)]
pub(crate) mod cpp {
    pub(super) unsafe fn ee_units_pm_host_passable_by(_u: u64, _w: u64) -> u8 {
        1
    }
    pub(super) unsafe fn ee_units_pm_host_random() -> f32 {
        0.5
    }
    pub(super) unsafe fn ee_astar_view_cm_path_area_at(_x: i32, _y: i32) -> u64 {
        0
    }
    pub(crate) unsafe fn ee_astar_view_cm_hill_at(_x: i32, _y: i32) -> u64 {
        0
    }
}
#[cfg(test)]
pub(crate) use cpp::*;

/// A view of the world: `map` the search's map (an `EEMap*`), `quiet` a modifier's view whose answers are not noted.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAStarWorldView {
    pub map: *mut c_void,
    pub quiet: bool,
}

/// The continent manager's record (`&gContinentManager.mRecord`), registered once.
static CM_RECORD: AtomicPtr<EeAStarCmRecord> = AtomicPtr::new(std::ptr::null_mut());

/// Registers the continent manager's record the views read (EEAIRegisterContinentState).
///
/// # Safety
/// `r` the global manager's record, live for the program.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_world_view_set_manager(r: *mut EeAStarCmRecord) {
    CM_RECORD.store(r, Ordering::Release);
}

pub(crate) fn cm() -> Option<&'static EeAStarCmRecord> {
    let p = CM_RECORD.load(Ordering::Acquire);
    // SAFETY: the registered global record (it outlives every search)
    unsafe { p.as_ref() }
}

// ---- the query numbers the capture notes (the C++ view's enum)
const Q_HAS_TILE: i32 = 1;
const Q_TERRAIN: i32 = 2;
const Q_EDGE: i32 = 3;
const Q_HAS_UNITS: i32 = 4;
const Q_AI_VALUE: i32 = 5;
const Q_HEIGHT_LEVEL: i32 = 6;
const Q_CONTINENT_ID: i32 = 7;
const Q_CONTINENT_AT: i32 = 8;
const Q_TILE_FLAG: i32 = 9;
const Q_TILE_VALUE: i32 = 10;
const Q_PATH_AREA_AT: i32 = 11;
const Q_PATH_AREA: i32 = 12;
const Q_STORED_PATH_AREA_ID: i32 = 13;
const Q_CHOKE_TILE: i32 = 14;
const Q_RESOURCE_KIND: i32 = 15;
const Q_HAS_BLOCKER: i32 = 16;
const Q_FIRST_BLOCKER_ID: i32 = 17;
const Q_FIRST_BLOCKER: i32 = 18;
const Q_FIRST_OBSTACLE_ID: i32 = 19;
const Q_OWN_WALL_FIRST: i32 = 20;
const Q_AREA_GATE_OWNER: i32 = 21;
const Q_SECTOR_GATE_OWNER: i32 = 22;
const Q_LIVE_DOCK: i32 = 23;
const Q_LIVE_SPACE_DOCK: i32 = 24;
const Q_CAN_MOVE_ON: i32 = 25;
const Q_ARE_PASSABLE: i32 = 26;
const Q_CROWD_COST: i32 = 27;
const Q_IS_WIDE: i32 = 28;
const Q_FORMATION_RANK: i32 = 29;
const Q_IGNORES_FORMATION: i32 = 30;
const Q_RANK_PROBE: i32 = 31;
const Q_CAN_ENTER: i32 = 32;
const Q_MODIFIER_COST: i32 = 33;

/// `kAStarEventView`
const EV_VIEW: u8 = b'V';

// ---- the game's constants the reads test
pub(crate) const CATEGORY_RESOURCE: u64 = 1;
pub(crate) const CATEGORY_SUBMERGED_NAVAL: u64 = 0xa;
pub(crate) const CATEGORY_NAVAL_BUILDING: u64 = 0x15;
pub(crate) const CATEGORY_GATE: u64 = 0x19;
pub(crate) const CATEGORY_SEA_ANIMAL: u64 = 0x1b;
const TYPE_DOCK: u64 = 0x24;
const TYPE_NAVY_YARD: u64 = 0x41;
const TYPE_SPACE_DOCK: u64 = 0x2d8;
const TYPE_SPACE_TURRET: u64 = 0x302;
const TYPE_ORBITAL_SPACE_STATION: u64 = 0x31f;
pub(crate) const RESOURCE_WOOD: i64 = 3;
const TERRAIN_DEEP_WATER: i64 = 1;
const MOVEMENT_SEA: u8 = 4;
const MOVEMENT_SPACE: u8 = 8;
const ACTION_ATTACK_ENEMY_UNIT: u32 = 2001;
const ACTION_IDLE: u32 = 5000;
/// the path goals' full hit point fraction (.rdata 0x439ff4)
const FULL: f32 = 1.0;

// ---- the pure decisions

/// What the area fills read of a unit on a tile.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub(crate) struct TileUnit {
    pub id: u32,
    pub hit_points: i64,
    pub category: u64,
    pub unit_type: u64,
    pub blocks_tile: bool,
    pub is_wall: bool,
    pub foundation: bool,
    pub player_id: i32,
}

impl TileUnit {
    fn alive_or_resource(&self) -> bool {
        self.hit_points > 0 || self.category == CATEGORY_RESOURCE
    }
    /// The area fills' blocker: (it blocks tiles or is a gate), not a foundation, alive or a resource.
    pub(crate) fn blocks_area(&self) -> bool {
        (self.blocks_tile || self.category == CATEGORY_GATE) && !self.foundation && self.alive_or_resource()
    }
    /// `GetFirstObstacleID`'s test (the same facts in its own order).
    pub(crate) fn is_obstacle(&self) -> bool {
        self.alive_or_resource() && ((self.blocks_tile && !self.foundation) || self.category == CATEGORY_GATE)
    }
}

/// `EEAIGetResourceKind`: the wood kind of a tree type (1 maples, spruces and cacti; 3 the palms and late trees; 2
/// others).
pub(crate) fn resource_kind(unit_type: u64) -> i32 {
    match unit_type {
        0xee | 0xef | 0x151..=0x158 => 1,
        0x159 | 0x15a | 0x15e..=0x161 => 3,
        _ => 2,
    }
}

/// `GetHeightLevel`: `UMFloor(height / (step * 0.5f) + 0.00001f)`.
pub(crate) fn height_level(height: f32, step: f32) -> i32 {
    ee_base::math::um::floor(height / (step * 0.5f32) + 0.00001f32)
}

/// What the crowd cost and the step blockers read of a unit with a complex unit.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub(crate) struct Mover {
    pub handle: u64,
    pub collidable: bool,
    pub foundation: bool,
    pub active_group: u64,
    pub collision_radius: f32,
    pub command_type: u32,
    pub velocity: f32,
    pub domains: u8,
    pub category: u64,
    pub air_traffic_type: i64,
}

/// One unit's part of `GetCrowdCost` for `walker` (its group and whether its class's inner sphere is small).
pub(crate) fn crowd_step(u: &Mover, walker: u64, walker_group: u64, walker_small: bool) -> i32 {
    if u.handle == walker || !u.collidable || u.foundation {
        return 0;
    }
    if u.active_group != 0 && u.active_group == walker_group && u.collision_radius < FULL {
        return 0;
    }
    if u.command_type != ACTION_IDLE && u.command_type != ACTION_ATTACK_ENEMY_UNIT {
        return 0;
    }
    if u.collision_radius >= FULL && walker_small {
        150
    } else {
        50
    }
}

fn submerged(category: u64) -> bool {
    category == CATEGORY_SUBMERGED_NAVAL || category == CATEGORY_SEA_ANIMAL
}

/// `EEAStarIsSeaBlocker` (the walker's handle, group and category).
pub(crate) fn sea_blocker(u: &Mover, walker: u64, walker_group: u64, walker_category: u64) -> bool {
    if u.velocity != 0.0 || u.handle == walker {
        return false;
    }
    if u.domains & MOVEMENT_SEA == 0 || !u.collidable {
        return false;
    }
    (u.active_group == 0
        || u.active_group != walker_group
        || (u.category == CATEGORY_NAVAL_BUILDING && u.air_traffic_type != 0))
        && submerged(u.category) == submerged(walker_category)
}

/// `EEAStarIsLandBlocker` (the space walkers' step blocker).
pub(crate) fn land_blocker(u: &Mover, walker: u64, walker_group: u64) -> bool {
    if u.velocity != 0.0 || u.handle == walker {
        return false;
    }
    u.domains & MOVEMENT_SPACE != 0 && u.collidable && (u.active_group == 0 || u.active_group != walker_group)
}

// ---- the reads

fn token() -> WorldRead {
    // SAFETY: a search runs on a thread that holds the world (the C++ view read it unlocked the same way)
    unsafe { WorldRead::begin() }
}

fn view<'a>(ctx: *mut c_void) -> &'a EeAStarWorldView {
    // SAFETY: the callbacks' context is the view they were made for, alive while they are called
    unsafe { &*(ctx as *const EeAStarWorldView) }
}

fn note(v: &EeAStarWorldView, q: i32, x: i32, y: i32, answer: i32, arg: i32, arg2: i32) -> i32 {
    if v.quiet {
        return answer;
    }
    search_note::note(EV_VIEW, q, x, y, arg, arg2, answer)
}

fn note_bool(v: &EeAStarWorldView, q: i32, x: i32, y: i32, answer: bool, arg: i32, arg2: i32) -> bool {
    note(v, q, x, y, i32::from(answer), arg, arg2) != 0
}

fn map<'t>(t: &'t WorldRead, v: &EeAStarWorldView) -> Option<MapView<'t>> {
    // SAFETY: the view's map is the live map of the search
    unsafe { MapView::of(t, v.map as u64) }
}

fn world_map(t: &WorldRead) -> Option<MapView<'_>> {
    WorldView::global(t).and_then(|w| w.map())
}

fn tile<'t>(t: &'t WorldRead, v: &EeAStarWorldView, x: i32, y: i32) -> Option<TileView<'t>> {
    map(t, v).and_then(|m| m.tile(i64::from(x), i64::from(y)))
}

fn unit<'t>(t: &'t WorldRead, h: Unit) -> Option<UnitView<'t>> {
    // SAFETY: a walker or a tile's unit is a live unit object
    unsafe { UnitView::of(t, h as u64) }
}

/// `IsFoundation()` (the complex unit's member; a plain unit is none).
fn foundation(u: &UnitView<'_>) -> bool {
    u.is_complex() && crate::world_field!(u.complex(), mIsFoundation)
}

fn tile_unit(u: &UnitView<'_>) -> TileUnit {
    let c = u.class();
    TileUnit {
        id: u.id(),
        hit_points: u.hit_points(),
        category: c.map_or(0, |c| c.category()),
        unit_type: c.map_or(0, |c| c.unit_type()),
        blocks_tile: c.is_some_and(|c| c.blocks_tile()),
        is_wall: c.is_some_and(|c| c.wall()),
        foundation: foundation(u),
        player_id: u.player().map_or(0, |p| p.id() as i32),
    }
}

/// The tile's units' facts in the list's order (a node without a unit skipped).
fn tile_units(t: &WorldRead, v: &EeAStarWorldView, x: i32, y: i32) -> Vec<TileUnit> {
    tile(t, v, x, y).map_or_else(Vec::new, |tl| tl.units().flatten().map(|u| tile_unit(&u)).collect())
}

fn mover(u: &UnitView<'_>) -> Mover {
    let c = u.class();
    let f = c.map(|c| c.fields());
    Mover {
        handle: u.handle(),
        collidable: f.is_some_and(|f| crate::world_field!(f, mIsCollidable)),
        foundation: foundation(u),
        active_group: u.active_group_handle(),
        collision_radius: crate::world_field!(u.complex(), mCollisionRadius),
        command_type: u.command_type(),
        velocity: crate::world_field!(u.complex(), mVelocity),
        domains: f.map_or(0, |f| crate::world_field!(f, mDomains)),
        category: c.map_or(0, |c| c.category()),
        air_traffic_type: f.map_or(0, |f| {
            let v: c_long = crate::world_field!(f, mAirTrafficType);
            v as i64
        }),
    }
}

/// The complex units of the tile's units (`theNode->mUnit->mComplexUnit`, none skipped), in order.
fn tile_movers<'t>(t: &'t WorldRead, v: &EeAStarWorldView, x: i32, y: i32) -> Vec<UnitView<'t>> {
    tile(t, v, x, y).map_or_else(Vec::new, |tl| tl.units().flatten().filter_map(|u| u.complex_unit()).collect())
}

fn walker_sphere(w: &UnitView<'_>) -> f32 {
    w.class().map_or(0.0, |c| crate::world_field!(c.fields(), mInnerCollisionSphere))
}

fn walker_small(w: &UnitView<'_>) -> bool {
    walker_sphere(w) < FULL
}

fn can_move_on_terrain(w: &UnitView<'_>, terrain: i32) -> bool {
    let Some(c) = w.class() else { return false };
    let stats = w.stats().map(|s| s.fields());
    // SAFETY: live records of the walker's class and stats
    unsafe { crate::units::cls_answers::can_move_on_terrain(&*c.fields().ptr(), stats.map(|s| &*s.ptr()), terrain) }
}

/// `EEAIHasResourceUnit`: the wood kind of the first wood unit on a tile not in deep water, 0 if none.
fn tile_resource_kind(tl: Option<TileView<'_>>) -> i32 {
    let Some(tl) = tl else { return 0 };
    if tl.terrain_type() == TERRAIN_DEEP_WATER {
        return 0;
    }
    for u in tl.units().flatten() {
        let Some(c) = u.class() else { continue };
        let r: c_long = crate::world_field!(c.fields(), mResourceType);
        if r as i64 == RESOURCE_WOOD {
            return resource_kind(c.unit_type());
        }
    }
    0
}

/// `EEAIHasResourceUnit(tile)` for the C++ (the continent manager's forests, the choke point pass).
///
/// # Safety
/// `tile` 0 or a live `EETile*`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_tile_resource_kind(tile: u64) -> i32 {
    let t = token();
    tile_resource_kind(TileView::of(&t, tile))
}

/// The manager's inline `GetContinentID(x, y)`.
fn continent_id(x: i32, y: i32) -> u16 {
    let Some(r) = cm() else { return 0 };
    match r.grid_continent_id(x as c_long, y as c_long) {
        Some(id) => id,
        None => {
            let c = cm_io::find_continent(r, x as c_long, y as c_long);
            // SAFETY: a listed continent is a live object
            if c == 0 {
                0
            } else {
                unsafe { cm_io::continent(c).id }
            }
        }
    }
}

fn table_entry(t: crate::astar::cm::EeAStarCmTable, index: usize) -> u64 {
    cm().and_then(|r| r.tables().tables[t as usize].get(index).copied()).unwrap_or(0)
}

/// `GetTileFlag`: the world map's tile's AI value, 0 off it.
fn tile_flag(x: i32, y: i32) -> i32 {
    let t = token();
    world_map(&t).and_then(|m| m.tile(i64::from(x), i64::from(y))).map_or(0, |tl| i32::from(tl.ai_value()))
}

fn path_area_info(h: u64) -> EeAStarPathAreaInfo {
    if h == 0 {
        return EeAStarPathAreaInfo::default();
    }
    // SAFETY: a listed path area is a live object
    let p = unsafe { cm_io::path_area(h) };
    EeAStarPathAreaInfo { found: true, id: p.id, continent_id: p.continent_id, kind: p.kind as i32 }
}

// ---- the callbacks

extern "C" fn has_tile(ctx: *mut c_void, x: i32, y: i32) -> bool {
    let v = view(ctx);
    let t = token();
    note_bool(v, Q_HAS_TILE, x, y, tile(&t, v, x, y).is_some(), 0, 0)
}

extern "C" fn terrain(ctx: *mut c_void, x: i32, y: i32) -> i32 {
    let v = view(ctx);
    let t = token();
    let a = tile(&t, v, x, y).map_or(0, |tl| tl.terrain_type() as i32);
    note(v, Q_TERRAIN, x, y, a, 0, 0)
}

extern "C" fn is_edge(ctx: *mut c_void, x: i32, y: i32) -> bool {
    let v = view(ctx);
    let t = token();
    let a = map(&t, v).is_some_and(|m| {
        let r = m.rts();
        let (a, b, c): (c_long, c_long, c_long) =
            (crate::world_field!(r, mL195608), crate::world_field!(r, mL19560C), crate::world_field!(r, mL195610));
        let (x, y) = (x as c_long, y as c_long);
        x.wrapping_add(y) == a || y.wrapping_sub(x) == c || x.wrapping_add(y) == b || x.wrapping_sub(y) == c
    });
    note_bool(v, Q_EDGE, x, y, a, 0, 0)
}

extern "C" fn has_units(ctx: *mut c_void, x: i32, y: i32) -> bool {
    let v = view(ctx);
    let t = token();
    let a = tile(&t, v, x, y).is_some_and(|tl| tl.units().next().is_some());
    note_bool(v, Q_HAS_UNITS, x, y, a, 0, 0)
}

extern "C" fn ai_value(ctx: *mut c_void, x: i32, y: i32) -> i32 {
    let v = view(ctx);
    let t = token();
    let a = tile(&t, v, x, y).map_or(0, |tl| i32::from(tl.ai_value()));
    note(v, Q_AI_VALUE, x, y, a, 0, 0)
}

extern "C" fn height_level_cb(ctx: *mut c_void, x: i32, y: i32) -> i32 {
    let v = view(ctx);
    let t = token();
    let a = map(&t, v).map_or(0, |m| {
        let step: f32 = crate::world_field!(m.rts(), mF1955F4);
        m.tile(i64::from(x), i64::from(y)).map_or(0, |tl| height_level(tl.height(), step))
    });
    note(v, Q_HEIGHT_LEVEL, x, y, a, 0, 0)
}

extern "C" fn continent_id_cb(ctx: *mut c_void, x: i32, y: i32) -> u16 {
    let v = view(ctx);
    note(v, Q_CONTINENT_ID, x, y, i32::from(continent_id(x, y)), 0, 0) as u16
}

extern "C" fn continent_at(ctx: *mut c_void, x: i32, y: i32, out: *mut EeAStarContinentInfo) {
    let v = view(ctx);
    let c = table_entry(crate::astar::cm::EeAStarCmTable::Continents, usize::from(continent_id(x, y)));
    let info = if c == 0 {
        EeAStarContinentInfo::default()
    } else {
        // SAFETY: a listed continent is a live object
        let r = unsafe { cm_io::continent(c) };
        EeAStarContinentInfo { id: r.id, category: r.category }
    };
    note(v, Q_CONTINENT_AT, x, y, i32::from(info.id), i32::from(info.category), 0);
    // SAFETY: the caller's out record
    unsafe { *out = info };
}

extern "C" fn tile_flag_cb(ctx: *mut c_void, x: i32, y: i32) -> i32 {
    let v = view(ctx);
    note(v, Q_TILE_FLAG, x, y, tile_flag(x, y), 0, 0)
}

extern "C" fn tile_value(ctx: *mut c_void, x: i32, y: i32) -> i32 {
    let v = view(ctx);
    let a = match cm() {
        Some(r) if !r.tile_values.is_null() => {
            let at = (r.width as c_long).wrapping_mul(x as c_long).wrapping_add(y as c_long);
            // SAFETY: the manager's value grid, as the C++ indexed it
            unsafe { *r.tile_values.offset(at as isize) as i32 }
        }
        _ => tile_flag(x, y),
    };
    note(v, Q_TILE_VALUE, x, y, a, 0, 0)
}

extern "C" fn path_area_at(ctx: *mut c_void, x: i32, y: i32, out: *mut EeAStarPathAreaInfo) {
    let v = view(ctx);
    // SAFETY: the manager's query (its CNTQ record)
    let info = path_area_info(unsafe { ee_astar_view_cm_path_area_at(x, y) });
    note(v, Q_PATH_AREA_AT, x, y, i32::from(info.found), i32::from(info.id), i32::from(info.continent_id));
    note(v, Q_PATH_AREA_AT, x, y, info.kind, 0, 0);
    // SAFETY: the caller's out record
    unsafe { *out = info };
}

extern "C" fn path_area(ctx: *mut c_void, id: u16, out: *mut EeAStarPathAreaInfo) {
    let v = view(ctx);
    let info = path_area_info(table_entry(crate::astar::cm::EeAStarCmTable::PathAreas, usize::from(id)));
    note(v, Q_PATH_AREA, i32::from(id), 0, i32::from(info.id), i32::from(info.continent_id), info.kind);
    // SAFETY: the caller's out record (the C++ marked it found, the area dereferenced unchecked)
    unsafe { *out = EeAStarPathAreaInfo { found: true, ..info } };
}

extern "C" fn stored_path_area_id(ctx: *mut c_void, storage: *const c_void, x: i32, y: i32) -> u16 {
    let v = view(ctx);
    // SAFETY: the sector fill's path area fill
    let a = unsafe { crate::astar::flood::ee_astar_flood_id(storage as *const _, x, y) };
    note(v, Q_STORED_PATH_AREA_ID, x, y, a as i32, 0, 0) as u16
}

extern "C" fn choke_tile(ctx: *mut c_void, index: i32, serial: *mut u32) -> bool {
    let v = view(ctx);
    let s = crate::astar::choke::grid_choke_tile(index);
    // SAFETY: the caller's out word
    unsafe { *serial = s.unwrap_or(0) };
    match s {
        Some(s) => note_bool(v, Q_CHOKE_TILE, index, 0, true, s as i32, 0),
        None => note_bool(v, Q_CHOKE_TILE, index, 0, false, 0, 0),
    }
}

extern "C" fn resource_kind_cb(ctx: *mut c_void, x: i32, y: i32) -> i32 {
    let v = view(ctx);
    let t = token();
    note(v, Q_RESOURCE_KIND, x, y, tile_resource_kind(tile(&t, v, x, y)), 0, 0)
}

extern "C" fn has_blocker(ctx: *mut c_void, x: i32, y: i32) -> bool {
    let v = view(ctx);
    let t = token();
    let a = tile_units(&t, v, x, y).iter().any(TileUnit::blocks_area);
    note_bool(v, Q_HAS_BLOCKER, x, y, a, 0, 0)
}

extern "C" fn first_blocker_id(ctx: *mut c_void, x: i32, y: i32) -> i32 {
    let v = view(ctx);
    let t = token();
    let a = tile_units(&t, v, x, y).iter().find(|u| u.blocks_area()).map_or(-1, |u| u.id as i32);
    note(v, Q_FIRST_BLOCKER_ID, x, y, a, 0, 0)
}

extern "C" fn first_blocker(ctx: *mut c_void, x: i32, y: i32, out: *mut EeAStarBlocker) {
    let v = view(ctx);
    let t = token();
    let b = tile_units(&t, v, x, y).iter().find(|u| u.blocks_area()).map_or(
        EeAStarBlocker { found: false, unit_id: -1, player_id: 0, is_gate: false, is_wall: false },
        |u| EeAStarBlocker {
            found: true,
            unit_id: u.id as i32,
            player_id: u.player_id,
            is_gate: u.category == CATEGORY_GATE,
            is_wall: u.is_wall,
        },
    );
    note(v, Q_FIRST_BLOCKER, x, y, i32::from(b.found), b.unit_id, b.player_id);
    note(v, Q_FIRST_BLOCKER, x, y, i32::from(b.is_gate), i32::from(b.is_wall), 0);
    // SAFETY: the caller's out record
    unsafe { *out = b };
}

extern "C" fn first_obstacle_id(ctx: *mut c_void, x: i32, y: i32) -> i32 {
    let v = view(ctx);
    let t = token();
    let a = tile_units(&t, v, x, y).iter().find(|u| u.is_obstacle()).map_or(-1, |u| u.id as i32);
    note(v, Q_FIRST_OBSTACLE_ID, x, y, a, 0, 0)
}

extern "C" fn own_wall_first(ctx: *mut c_void, x: i32, y: i32, player_id: i32) -> bool {
    let v = view(ctx);
    let t = token();
    for u in tile_units(&t, v, x, y) {
        if u.is_wall && u.hit_points > 0 && u.category != CATEGORY_GATE && u.player_id == player_id && !u.foundation {
            return note_bool(v, Q_OWN_WALL_FIRST, x, y, true, player_id, 0);
        }
        if u.blocks_area() {
            return note_bool(v, Q_OWN_WALL_FIRST, x, y, false, player_id, 0);
        }
    }
    note_bool(v, Q_OWN_WALL_FIRST, x, y, false, player_id, 0)
}

extern "C" fn area_gate_owner(ctx: *mut c_void, x: i32, y: i32, player_id: i32) -> i32 {
    let v = view(ctx);
    let t = token();
    for u in tile_units(&t, v, x, y) {
        if !(u.hit_points > 0) && u.category != CATEGORY_RESOURCE {
            continue;
        }
        if u.category == CATEGORY_GATE && u.player_id == player_id {
            return note(v, Q_AREA_GATE_OWNER, x, y, u.id as i32, player_id, 0);
        }
        if (u.blocks_tile || u.category == CATEGORY_GATE) && !u.foundation {
            return note(v, Q_AREA_GATE_OWNER, x, y, u.id as i32, player_id, 0);
        }
    }
    note(v, Q_AREA_GATE_OWNER, x, y, -1, player_id, 0)
}

extern "C" fn sector_gate_owner(ctx: *mut c_void, x: i32, y: i32, player_id: i32) -> i32 {
    let v = view(ctx);
    let t = token();
    for u in tile_units(&t, v, x, y) {
        if (u.category == CATEGORY_GATE && u.hit_points > 0 && u.player_id == player_id) || u.blocks_area() {
            return note(v, Q_SECTOR_GATE_OWNER, x, y, u.id as i32, player_id, 0);
        }
    }
    note(v, Q_SECTOR_GATE_OWNER, x, y, -1, player_id, 0)
}

extern "C" fn live_dock(ctx: *mut c_void, x: i32, y: i32) -> bool {
    let v = view(ctx);
    let t = token();
    let a = tile_units(&t, v, x, y)
        .iter()
        .any(|u| (u.unit_type == TYPE_DOCK || u.unit_type == TYPE_NAVY_YARD) && u.hit_points > 0 && !u.foundation);
    note_bool(v, Q_LIVE_DOCK, x, y, a, 0, 0)
}

extern "C" fn live_space_dock(ctx: *mut c_void, x: i32, y: i32) -> bool {
    let v = view(ctx);
    let t = token();
    let a = tile_units(&t, v, x, y).iter().any(|u| {
        (u.unit_type == TYPE_SPACE_DOCK
            || u.unit_type == TYPE_SPACE_TURRET
            || u.unit_type == TYPE_ORBITAL_SPACE_STATION)
            && u.hit_points > 0
            && !u.foundation
    });
    note_bool(v, Q_LIVE_SPACE_DOCK, x, y, a, 0, 0)
}

fn walker_id(t: &WorldRead, walker: Unit) -> i32 {
    unit(t, walker).map_or(0, |w| w.id() as i32)
}

extern "C" fn can_move_on(ctx: *mut c_void, walker: Unit, terrain: i32) -> bool {
    let v = view(ctx);
    let t = token();
    let a = unit(&t, walker).is_some_and(|w| can_move_on_terrain(&w, terrain));
    note_bool(v, Q_CAN_MOVE_ON, terrain, 0, a, walker_id(&t, walker), 0)
}

/// Every unit on the tile `IsPassableBy` the walker (skipping walls or trees).
fn passable(t: &WorldRead, v: &EeAStarWorldView, walker: Unit, x: i32, y: i32, skip: Skip) -> bool {
    let Some(tl) = tile(t, v, x, y) else { return true };
    for u in tl.units().flatten() {
        let c = u.class();
        if skip == Skip::Walls && c.is_some_and(|c| c.wall()) {
            continue;
        }
        if skip == Skip::Trees && c.is_some_and(|c| c.tree()) {
            continue;
        }
        // SAFETY: the unit's virtual over two live units
        if unsafe { ee_units_pm_host_passable_by(u.handle(), walker as u64) } == 0 {
            return false;
        }
    }
    true
}

extern "C" fn are_passable(ctx: *mut c_void, walker: Unit, x: i32, y: i32, skip: Skip) -> bool {
    let v = view(ctx);
    let t = token();
    let a = passable(&t, v, walker, x, y, skip);
    if skip != Skip::None {
        // `ArePassableExcept` is a modifier's read: not noted
        return a;
    }
    note_bool(v, Q_ARE_PASSABLE, x, y, a, walker_id(&t, walker), 0)
}

extern "C" fn crowd_cost(ctx: *mut c_void, walker: Unit, x: i32, y: i32) -> i32 {
    let v = view(ctx);
    let t = token();
    let (group, small) = unit(&t, walker).map_or((0, false), |w| (w.active_group_handle(), walker_small(&w)));
    let mut cost = 0i32;
    for u in tile_movers(&t, v, x, y) {
        cost = cost.wrapping_add(crowd_step(&mover(&u), walker as u64, group, small));
    }
    note(v, Q_CROWD_COST, x, y, cost, walker_id(&t, walker), 0)
}

extern "C" fn is_wide(ctx: *mut c_void, walker: Unit) -> bool {
    let v = view(ctx);
    let t = token();
    let a = unit(&t, walker).is_some_and(|w| walker_sphere(&w) >= FULL);
    note_bool(v, Q_IS_WIDE, 0, 0, a, walker_id(&t, walker), 0)
}

extern "C" fn formation_rank(ctx: *mut c_void, walker: Unit) -> i32 {
    let v = view(ctx);
    let t = token();
    let a = unit(&t, walker).map_or(0, |w| {
        let r: c_long = crate::world_field!(w.complex(), mFormationRank);
        r as i32
    });
    note(v, Q_FORMATION_RANK, 0, 0, a, walker_id(&t, walker), 0)
}

fn terrain_mask_bit14(w: &UnitView<'_>) -> bool {
    w.stats().is_some_and(|s| {
        let m: std::ffi::c_ulong = crate::world_field!(s.fields(), mTerrainMask);
        (m >> 14) & 1 != 0
    })
}

extern "C" fn ignores_formation(ctx: *mut c_void, walker: Unit) -> bool {
    let v = view(ctx);
    let t = token();
    let a = unit(&t, walker).is_some_and(|w| terrain_mask_bit14(&w));
    note_bool(v, Q_IGNORES_FORMATION, 0, 0, a, walker_id(&t, walker), 0)
}

extern "C" fn rank_probe_blocked(ctx: *mut c_void, walker: Unit, continent: u16, x: i32, y: i32) -> bool {
    let v = view(ctx);
    let t = token();
    let a = world_map(&t).and_then(|m| m.tile(i64::from(x), i64::from(y))).is_some_and(|tl| {
        tl.ai_value() != 0
            || !unit(&t, walker).is_some_and(|w| can_move_on_terrain(&w, tl.terrain_type() as i32))
            || continent != continent_id(x, y)
    });
    note_bool(v, Q_RANK_PROBE, x, y, a, walker_id(&t, walker), i32::from(continent))
}

extern "C" fn can_enter(ctx: *mut c_void, modifier: *mut c_void, x: i32, y: i32, at_goal: bool) -> bool {
    let v = view(ctx);
    let tl = {
        let t = token();
        tile(&t, v, x, y).map_or(0, |tl| tl.handle())
    };
    // SAFETY: the goal's live modifier
    let a = unsafe { modifier_obj::can_enter(modifier as *mut EeAStarModifier, tl, x, y, at_goal) };
    note_bool(v, Q_CAN_ENTER, x, y, a, i32::from(at_goal), 0)
}

#[allow(clippy::too_many_arguments)]
extern "C" fn modifier_cost(
    ctx: *mut c_void,
    modifier: *mut c_void,
    fx: i32,
    fy: i32,
    x: i32,
    y: i32,
    px: i32,
    py: i32,
) -> i32 {
    let v = view(ctx);
    // SAFETY: the goal's live modifier
    let cost = unsafe { modifier_obj::cost(modifier as *mut EeAStarModifier, fx, fy, x, y, px, py) };
    note(v, Q_MODIFIER_COST, x, y, cost, fx, fy);
    note(v, Q_MODIFIER_COST, x, y, cost, px, py)
}

// the modifiers' own reads (not noted)

extern "C" fn step_blocker(ctx: *mut c_void, walker: Unit, sea: bool, x: i32, y: i32) -> bool {
    let v = view(ctx);
    let t = token();
    let Some(w) = unit(&t, walker) else { return false };
    let group = w.active_group_handle();
    let category = w.class().map_or(0, |c| c.category());
    tile_movers(&t, v, x, y).iter().any(|u| {
        let m = mover(u);
        if sea {
            sea_blocker(&m, walker as u64, group, category)
        } else {
            land_blocker(&m, walker as u64, group)
        }
    })
}

extern "C" fn unit_region_state(ctx: *mut c_void, x: i32, y: i32) -> i32 {
    let v = view(ctx);
    let t = token();
    let Some(m) = map(&t, v) else { return -1 };
    let regions = m.unit_regions();
    // SAFETY: the map's live region table
    unsafe {
        let size = crate::combat::regions::ee_combat_regions_size(regions);
        let n = crate::combat::regions::ee_combat_regions_len(regions, (x / size) as u32, (y / size) as u32);
        if n < 0 {
            -1
        } else if n == 0 {
            0
        } else {
            1
        }
    }
}

extern "C" fn epoch(_ctx: *mut c_void, walker: Unit) -> i32 {
    let t = token();
    unit(&t, walker).and_then(|w| w.player()).and_then(|p| p.tech_tree()).map_or(0, |tr| tr.current_epoch() as i32)
}

extern "C" fn is_alone(_ctx: *mut c_void, walker: Unit) -> bool {
    let t = token();
    let Some(w) = unit(&t, walker) else { return true };
    let g = w.active_group_handle();
    // SAFETY: the walker's live active group
    g == 0 || unsafe { crate::world::GroupView::of(&t, g) }.is_none_or(|g| g.len() <= 1)
}

extern "C" fn water_state(_ctx: *mut c_void, walker: Unit) -> i32 {
    let t = token();
    unit(&t, walker).map_or(0, |w| {
        let s: c_long = crate::world_field!(w.complex(), mWaterState);
        s as i32
    })
}

extern "C" fn is_sea_going(_ctx: *mut c_void, walker: Unit) -> bool {
    let t = token();
    unit(&t, walker).and_then(|w| w.class()).is_some_and(|c| {
        let d: u8 = crate::world_field!(c.fields(), mDomains);
        d & MOVEMENT_SEA != 0
    })
}

extern "C" fn has_hill(_ctx: *mut c_void, x: i32, y: i32) -> bool {
    // SAFETY: the manager's query (its CNTQ record)
    unsafe { ee_astar_view_cm_hill_at(x, y) != 0 }
}

extern "C" fn random_at_most(_ctx: *mut c_void, chance: f32) -> bool {
    // SAFETY: one draw of the physics stream
    unsafe { ee_units_pm_host_random() <= chance }
}

/// The callbacks over `view` (`EEAIAStarView::Callbacks`).
///
/// # Safety
/// `view` outlives every call of the callbacks.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_world_view_callbacks(view: *mut EeAStarWorldView) -> EeAStarViewCallbacks {
    EeAStarViewCallbacks {
        ctx: view as *mut c_void,
        has_tile,
        terrain,
        is_edge,
        height_level: height_level_cb,
        continent_id: continent_id_cb,
        choke_tile,
        ai_value,
        tile_flag: tile_flag_cb,
        can_move_on,
        are_passable,
        formation_rank,
        step_blocker,
        unit_region_state,
        epoch,
        is_alone,
        water_state,
        is_sea_going,
        has_hill,
        random_at_most,
        crowd_cost,
        is_wide,
        ignores_formation,
        rank_probe_blocked,
        can_enter,
        modifier_cost,
        has_units,
        continent_at,
        tile_value,
        path_area_at,
        path_area,
        stored_path_area_id,
        resource_kind: resource_kind_cb,
        has_blocker,
        first_blocker_id,
        first_blocker,
        first_obstacle_id,
        own_wall_first,
        area_gate_owner,
        sector_gate_owner,
        live_dock,
        live_space_dock,
    }
}

#[cfg(test)]
#[path = "world_view_tests.rs"]
mod tests;
