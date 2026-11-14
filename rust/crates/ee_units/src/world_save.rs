//! The saved world's running state (`EEPWorldHelper`'s port parts, save versions 0.254 to 0.279; RUST_PLAN R5b.4, the
//! thirtieth round): what the original did not save and a loaded game needs to step on as the saved one would. The
//! sequence of `EEPWorldHelper::Load`/`Save` after the world's settings is here ([`load_live`], [`save_live`]): the
//! order of the world's unit chain and of the stepped units, the forests' renewals, the computer players' schedule,
//! the order of the units on each tile and in each map region, the dead units the living still hold (the ghosts),
//! the order of each player's unit list; the other systems' parts (the path manager, the area effects, the
//! experiment timers, the towns, the continent manager, the resource state, the searches, the active groups) are
//! host operations in their places.
//!
//! A load reads each part into a record and asks the host to put it back at the first tick (`EEAfterLoad`, in the
//! order the parts were read, between the other systems' own); [`restore`] is that step, over host operations on
//! the world's units (by address). The ghosts' records stay here until the first tick: `EEFindLoadedGhostUnit` asks
//! for one by id ([`ee_units_world_ghost_find`]) and makes it, the made units are held until then
//! ([`ee_units_world_ghost_made`]).
//!
//! Kept: every count and order of the C++ (a part read until its first failed read, then no restore of it and no
//! later part); a chain restored only when it holds the same units x-sorted; the stepped lists made the saved ones
//! with the references `UpdateActiveUnits` keeps; a tile or region restored only when it holds the same units; a
//! player's units in the saved order with any unit the save did not list after the last of its id's bucket.

#![allow(clippy::unnecessary_cast)]

use crate::player_save::{
    EeUnitsPlayerSaveIo, PlayerSaveHost, EE_UNITS_PLAYER_SAVE_READ_BOOL, EE_UNITS_PLAYER_SAVE_READ_FLOAT,
    EE_UNITS_PLAYER_SAVE_READ_LONG, EE_UNITS_PLAYER_SAVE_READ_ULONG,
};
use std::ffi::{c_void, CString};
use std::os::raw::{c_long, c_ulong};
use std::sync::Mutex;

/// One host operation's values: numbers, a placement, and a list the host fills (up to `cap`, answering the full
/// count) or reads (`cap` entries).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeUnitsWorldIo {
    pub a: i64,
    pub b: i64,
    pub f: f32,
    pub v: [i64; 8],
    pub m: [f32; 16],
    pub list: *mut u64,
    pub cap: usize,
}

impl Default for EeUnitsWorldIo {
    fn default() -> Self {
        EeUnitsWorldIo { a: 0, b: 0, f: 0.0, v: [0; 8], m: [0.0; 16], list: std::ptr::null_mut(), cap: 0 }
    }
}

/// The host: `(ctx, op, io) -> the answer`.
pub type EeUnitsWorldFn = unsafe extern "C" fn(*mut c_void, i32, *mut EeUnitsWorldIo) -> i64;

// ---- the host operations. 1-8: the file's primitives (player_save.rs's numbers; `a` the number, `f` the float,
// `b` the bool)
/// `a` = whether the save has the part `HAS_*` (ee/EESaveGameFiles.h's predicates of the file's version)
pub const EE_UNITS_WORLD_HAS: i32 = 20;
/// another system's part `PART_*` loaded (`b`: `EESaveHasTownAreas` for the towns); its result
pub const EE_UNITS_WORLD_PART_LOAD: i32 = 21;
/// another system's part `PART_*` saved
pub const EE_UNITS_WORLD_PART_SAVE: i32 = 22;
/// `EEAfterLoad` of [`ee_units_world_restore`] with the record at `a`
pub const EE_UNITS_WORLD_AFTER_LOAD: i32 = 23;
/// `ESDebugOut` of the NUL-terminated text at `list`
pub const EE_UNITS_WORLD_DEBUG: i32 = 24;
/// a unit's `mUnitID` (`a`: the unit)
pub const EE_UNITS_WORLD_UNIT_ID: i32 = 25;
/// the world's unit chain (`gWorld.mUnits`, by `mTileNode->mNext`): its units
pub const EE_UNITS_WORLD_CHAIN: i32 = 30;
/// `gWorld.GetUnit(a)`
pub const EE_UNITS_WORLD_UNIT: i32 = 31;
/// the unit `a` is linked in the chain: a tile node, with a previous unit or the chain's first
pub const EE_UNITS_WORLD_IN_CHAIN: i32 = 32;
/// the unit `a`'s `mTileX`
pub const EE_UNITS_WORLD_TILE_X: i32 = 33;
/// `a->Link(b)`, `b` 0 for the chain's head (`&gWorld.mUnits`)
pub const EE_UNITS_WORLD_LINK: i32 = 34;
/// the stepped units (`a` 0), the activation (1) or the deactivation queue (2): its units
pub const EE_UNITS_WORLD_STEP_LIST: i32 = 35;
/// the complex unit of id `a` in the loaded game (`dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(a))`)
pub const EE_UNITS_WORLD_COMPLEX: i32 = 36;
/// the unit `b` leaves list `a`: `Dereference`, for the stepped units a second one and `mActiveUnits.erase`
pub const EE_UNITS_WORLD_STEP_DROP: i32 = 37;
/// the unit `b` joins list `a`: `Reference`, for the stepped units a second one and `mActiveUnits[id]`
pub const EE_UNITS_WORLD_STEP_ADD: i32 = 38;
/// list `a` = the `cap` units at `list`
pub const EE_UNITS_WORLD_STEP_SET: i32 = 39;
/// the continent manager's forests (null entries kept)
pub const EE_UNITS_WORLD_FORESTS: i32 = 40;
/// the forest `a`'s `mNextRenewal`
pub const EE_UNITS_WORLD_FOREST_RENEWAL: i32 = 41;
/// the forest `a`'s tiles (`BuildPointList(.., 1)`), each `x | y << 32` as two 32-bit words
pub const EE_UNITS_WORLD_FOREST_TILES: i32 = 42;
/// `gContinentManager.GetForestAt(a, b)`
pub const EE_UNITS_WORLD_FOREST_AT: i32 = 43;
/// the forest `a`'s `mNextRenewal` = `b`
pub const EE_UNITS_WORLD_FOREST_SET_RENEWAL: i32 = 44;
/// the forest `a`'s `mTiles.Size()`
pub const EE_UNITS_WORLD_FOREST_SIZE: i32 = 45;
/// the computer players' schedule: `a` = `mNextPlayer`, `b` = `mNextUpdateTime`, the answer the players' count
pub const EE_UNITS_WORLD_AI_SCHEDULE: i32 = 46;
/// the schedule = `a`, `b`
pub const EE_UNITS_WORLD_AI_SET_SCHEDULE: i32 = 47;
/// the map's tile count (`mSize * mSize`, 0 without a map)
pub const EE_UNITS_WORLD_TILE_COUNT: i32 = 48;
/// tile `a`'s unit nodes (`mUnits`, by `mNext`): each node's unit (0 for none); none for a NULL tile
pub const EE_UNITS_WORLD_TILE_UNITS: i32 = 49;
/// tile `a`'s node `b` holds the unit at `list[0]`
pub const EE_UNITS_WORLD_TILE_SET: i32 = 50;
/// the map regions' order (`ee_combat_regions_order`), the 32-bit words
pub const EE_UNITS_WORLD_REGION_ORDER: i32 = 51;
/// `ee_combat_regions_restore` of region `a` with the `cap` ids at `list`
pub const EE_UNITS_WORLD_REGION_RESTORE: i32 = 52;
/// the units the ghosts are saved for (`EESaveGhosts`' gathering): the complex ones first, `a` = their count
pub const EE_UNITS_WORLD_GHOSTS: i32 = 53;
/// the ghost unit `a` (complex when `b`): `v` = its id, class type (-1 without a class), player (-1 without one),
/// hit points, tile x, tile y, built flag, resource type; `a` = its resource amount; `m` its placement
pub const EE_UNITS_WORLD_GHOST_FIELDS: i32 = 54;
/// `Dereference` of the unit `a` (a made ghost at the first tick)
pub const EE_UNITS_WORLD_DEREFERENCE: i32 = 55;
/// `gWorld.mNumPlayers`
pub const EE_UNITS_WORLD_PLAYER_COUNT: i32 = 56;
/// player `a`'s units (`mFirstUnit`, by `mNext`); none for a NULL player
pub const EE_UNITS_WORLD_PLAYER_UNITS: i32 = 57;
/// player `a`'s `GetUnit(b)`
pub const EE_UNITS_WORLD_PLAYER_UNIT: i32 = 58;
/// player `a`'s list = the `cap` units at `list` (`mPrevious`, `mNext`, `mFirstUnit`, `mLastUnit`)
pub const EE_UNITS_WORLD_PLAYER_SET_LIST: i32 = 59;
/// player `a`'s bucket `b`'s first = the unit at `list[0]`
pub const EE_UNITS_WORLD_PLAYER_SET_BUCKET: i32 = 60;

// the parts' predicates (`EE_UNITS_WORLD_HAS`'s `a`)
pub const EE_UNITS_WORLD_HAS_RUNNING_STATE: i64 = 1;
pub const EE_UNITS_WORLD_HAS_REGION_ORDER: i64 = 2;
pub const EE_UNITS_WORLD_HAS_EXPERIMENT_TIMERS: i64 = 3;
pub const EE_UNITS_WORLD_HAS_TOWNS: i64 = 4;
pub const EE_UNITS_WORLD_HAS_TOWN_AREAS: i64 = 5;
pub const EE_UNITS_WORLD_HAS_GHOSTS: i64 = 6;
pub const EE_UNITS_WORLD_HAS_GHOST_RESOURCES: i64 = 7;
pub const EE_UNITS_WORLD_HAS_GOAL_GHOSTS: i64 = 8;
pub const EE_UNITS_WORLD_HAS_CONTINENT_REGIONS: i64 = 9;
pub const EE_UNITS_WORLD_HAS_RESOURCE_STATE: i64 = 10;
pub const EE_UNITS_WORLD_HAS_PLAYER_UNIT_ORDER: i64 = 11;
pub const EE_UNITS_WORLD_HAS_PATH_SEARCHES: i64 = 12;
pub const EE_UNITS_WORLD_HAS_ACTIVE_GROUPS: i64 = 13;
pub const EE_UNITS_WORLD_HAS_GRAPH_SEARCHES: i64 = 14;

// the other systems' parts (`EE_UNITS_WORLD_PART_LOAD`/`_SAVE`'s `a`)
/// `gPathManager.LoadRunningState` / `SaveRunningState`
pub const EE_UNITS_WORLD_PART_PATH_RUNNING: i64 = 1;
/// `EEAELoadRunningState` / `EEAESaveRunningState`
pub const EE_UNITS_WORLD_PART_AREA_EFFECTS: i64 = 2;
/// `EECPLoadExperimentTimers` / `EECPSaveExperimentTimers`
pub const EE_UNITS_WORLD_PART_EXPERIMENT_TIMERS: i64 = 3;
/// `EELoadTowns(file, b)` / `EESaveTowns`
pub const EE_UNITS_WORLD_PART_TOWNS: i64 = 4;
/// `EELoadContinentRegions` / `EESaveContinentRegions`
pub const EE_UNITS_WORLD_PART_CONTINENT_REGIONS: i64 = 5;
/// `EECPLoadResourceState` / `EECPSaveResourceState`
pub const EE_UNITS_WORLD_PART_RESOURCE_STATE: i64 = 6;
/// `gPathManager.LoadSearches` / `SaveSearches`
pub const EE_UNITS_WORLD_PART_PATH_SEARCHES: i64 = 7;
/// `EELoadActiveGroups` / `EESaveActiveGroups`
pub const EE_UNITS_WORLD_PART_ACTIVE_GROUPS: i64 = 8;
/// `gPathManager.LoadGraphSearches` / `SaveGraphSearches`
pub const EE_UNITS_WORLD_PART_GRAPH_SEARCHES: i64 = 9;

const OK: i32 = 0;

/// The host's side.
pub trait WorldHost {
    fn call(&mut self, op: i32, io: &mut EeUnitsWorldIo) -> i64;

    fn ask(&mut self, op: i32, a: i64, b: i64) -> i64 {
        self.call(op, &mut EeUnitsWorldIo { a, b, ..Default::default() })
    }
    /// A list the host fills: asked with room for 64, again with room for all when longer.
    fn list(&mut self, op: i32, a: i64, b: i64) -> Vec<u64> {
        let mut out = vec![0u64; 64];
        let mut io = EeUnitsWorldIo { a, b, list: out.as_mut_ptr(), cap: out.len(), ..Default::default() };
        let n = self.call(op, &mut io).max(0) as usize;
        if n > out.len() {
            out = vec![0u64; n];
            let mut io = EeUnitsWorldIo { a, b, list: out.as_mut_ptr(), cap: out.len(), ..Default::default() };
            self.call(op, &mut io);
        }
        out.truncate(n);
        out
    }
    /// The host reads `items`.
    fn give(&mut self, op: i32, a: i64, b: i64, items: &[u64]) -> i64 {
        let mut copy = items.to_vec();
        let mut io = EeUnitsWorldIo { a, b, list: copy.as_mut_ptr(), cap: copy.len(), ..Default::default() };
        self.call(op, &mut io)
    }
    fn has(&mut self, which: i64) -> bool {
        self.ask(EE_UNITS_WORLD_HAS, which, 0) != 0
    }
    fn debug(&mut self, text: &str) {
        let c = CString::new(text).unwrap_or_default();
        let mut io = EeUnitsWorldIo { list: c.as_ptr() as *mut u64, ..Default::default() };
        self.call(EE_UNITS_WORLD_DEBUG, &mut io);
    }
    fn unit_id(&mut self, unit: u64) -> c_ulong {
        self.ask(EE_UNITS_WORLD_UNIT_ID, unit as i64, 0) as c_ulong
    }
}

/// The file's primitives through the world host.
struct File<'a, H: ?Sized>(&'a mut H);

impl<H: WorldHost + ?Sized> PlayerSaveHost for File<'_, H> {
    fn call(&mut self, op: i32, _index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        let mut w = EeUnitsWorldIo { a: io.i, b: io.b as i64, f: io.f, ..Default::default() };
        let r = self.0.call(op, &mut w) as i32;
        if matches!(
            op,
            EE_UNITS_PLAYER_SAVE_READ_ULONG
                | EE_UNITS_PLAYER_SAVE_READ_LONG
                | EE_UNITS_PLAYER_SAVE_READ_FLOAT
                | EE_UNITS_PLAYER_SAVE_READ_BOOL
        ) {
            io.i = w.a;
            io.f = w.f;
            io.b = w.b as u8;
        }
        r
    }
}

/// A dead unit the living still hold, as saved (`EESavedGhost`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsWorldGhost {
    pub id: c_ulong,
    pub unit_type: c_ulong,
    pub player: c_ulong,
    pub hit_points: c_long,
    pub tile_x: c_long,
    pub tile_y: c_long,
    pub placement: [[f32; 4]; 4],
    /// 0.272: -1 before (the class's own)
    pub resource_type: c_ulong,
    pub resource_amount: c_long,
    pub is_built: bool,
    /// 0.275: false for a simple unit; true before
    pub is_complex: bool,
}

/// A part read from the file, put back at the first tick.
#[derive(Clone, Debug, PartialEq)]
pub enum Pending {
    /// the chain's ids and the three stepped lists' ids
    UnitOrder(Vec<c_ulong>, [Vec<c_ulong>; 3]),
    /// each forest's renewal and tiles
    Forests(Vec<(c_ulong, Vec<(c_long, c_long)>)>),
    /// the next player and its update time
    AiSchedule(c_ulong, c_ulong),
    /// each tile's index and its units' ids
    TileOrder(Vec<(c_ulong, Vec<c_ulong>)>),
    /// each region's index and its units' ids
    RegionOrder(Vec<(c_ulong, Vec<c_ulong>)>),
    /// the ghosts' made units dereferenced, their records dropped
    Ghosts,
    /// each player's unit ids in order
    PlayerUnitOrder(Vec<Vec<c_ulong>>),
}

/// The loaded ghosts' records and the units made from them, until the first tick.
struct Ghosts {
    records: Vec<EeUnitsWorldGhost>,
    made: Vec<usize>,
}

static GHOSTS: Mutex<Ghosts> = Mutex::new(Ghosts { records: Vec::new(), made: Vec::new() });

fn ghosts() -> std::sync::MutexGuard<'static, Ghosts> {
    GHOSTS.lock().unwrap_or_else(|e| e.into_inner())
}

fn after_load<H: WorldHost + ?Sized>(h: &mut H, pending: Pending) {
    let p = Box::into_raw(Box::new(pending));
    h.ask(EE_UNITS_WORLD_AFTER_LOAD, p as usize as i64, 0);
}

// ---- the load

/// `count` ids after a count: the ids read until the first failure.
fn read_ids<F: PlayerSaveHost + ?Sized>(f: &mut F, result: &mut i32, into: &mut Vec<c_ulong>) {
    let mut count: c_ulong = 0;
    *result = f.read_ulong(&mut count);
    let mut i: c_ulong = 0;
    while i < count && *result == OK {
        let mut id: c_ulong = 0;
        *result = f.read_ulong(&mut id);
        into.push(id);
        i += 1;
    }
}

/// `LoadUnitOrder`: the chain's ids, then the three lists'.
fn load_unit_order<H: WorldHost + ?Sized>(h: &mut H) -> i32 {
    let mut chain = Vec::new();
    let mut lists: [Vec<c_ulong>; 3] = Default::default();
    let mut result = OK;
    read_ids(&mut File(h), &mut result, &mut chain);
    for list in lists.iter_mut() {
        if result != OK {
            break;
        }
        read_ids(&mut File(h), &mut result, list);
    }
    if result == OK {
        after_load(h, Pending::UnitOrder(chain, lists));
    }
    result
}

/// `LoadForests`: each forest's renewal and its tiles.
fn load_forests<H: WorldHost + ?Sized>(h: &mut H) -> i32 {
    let mut f = File(h);
    let mut forests = Vec::new();
    let mut count: c_ulong = 0;
    let mut result = f.read_ulong(&mut count);
    let mut i: c_ulong = 0;
    while i < count && result == OK {
        let mut renewal: c_ulong = 0;
        let mut tiles: c_ulong = 0;
        result = f.read_ulong(&mut renewal);
        if result == OK {
            result = f.read_ulong(&mut tiles);
        }
        let mut list = Vec::new();
        let mut t: c_ulong = 0;
        while t < tiles && result == OK {
            let (mut x, mut y): (c_long, c_long) = (0, 0);
            result = f.read_long(&mut x);
            if result == OK {
                result = f.read_long(&mut y);
            }
            list.push((x, y));
            t += 1;
        }
        forests.push((renewal, list));
        i += 1;
    }
    if result == OK {
        after_load(h, Pending::Forests(forests));
    }
    result
}

/// `LoadAISchedule`.
fn load_ai_schedule<H: WorldHost + ?Sized>(h: &mut H) -> i32 {
    let mut f = File(h);
    let (mut next, mut time): (c_ulong, c_ulong) = (0, 0);
    let mut result = f.read_ulong(&mut next);
    if result == OK {
        result = f.read_ulong(&mut time);
    }
    if result == OK {
        after_load(h, Pending::AiSchedule(next, time));
    }
    result
}

/// `LoadTileOrder` / `LoadRegionOrder`: each entry's index and its ids.
fn load_indexed<H: WorldHost + ?Sized>(h: &mut H, region: bool) -> i32 {
    let mut f = File(h);
    let mut entries = Vec::new();
    let mut count: c_ulong = 0;
    let mut result = f.read_ulong(&mut count);
    let mut t: c_ulong = 0;
    while t < count && result == OK {
        let mut index: c_ulong = 0;
        let mut units: c_ulong = 0;
        result = f.read_ulong(&mut index);
        if result == OK {
            result = f.read_ulong(&mut units);
        }
        let mut ids = Vec::new();
        let mut k: c_ulong = 0;
        while k < units && result == OK {
            let mut id: c_ulong = 0;
            result = f.read_ulong(&mut id);
            ids.push(id);
            k += 1;
        }
        entries.push((index, ids));
        t += 1;
    }
    if result == OK {
        after_load(h, if region { Pending::RegionOrder(entries) } else { Pending::TileOrder(entries) });
    }
    result
}

/// `LoadGhosts`: the records (0.272's resource and 0.275's kind by the save's version).
fn load_ghosts<H: WorldHost + ?Sized>(h: &mut H, has_resources: bool, has_kinds: bool) -> i32 {
    let mut f = File(h);
    let mut records = Vec::new();
    let mut count: c_ulong = 0;
    let mut result = f.read_ulong(&mut count);
    let mut i: c_ulong = 0;
    while i < count && result == OK {
        let mut g = EeUnitsWorldGhost::default();
        result = f.read_ulong(&mut g.id);
        if result == OK {
            result = f.read_ulong(&mut g.unit_type);
        }
        if result == OK {
            result = f.read_ulong(&mut g.player);
        }
        if result == OK {
            result = f.read_long(&mut g.hit_points);
        }
        if result == OK {
            result = f.read_long(&mut g.tile_x);
        }
        if result == OK {
            result = f.read_long(&mut g.tile_y);
        }
        if result == OK {
            result = f.read_bool(&mut g.is_built);
        }
        for r in 0..4 {
            for c in 0..4 {
                if result == OK {
                    result = f.read_float(&mut g.placement[r][c]);
                }
            }
        }
        g.resource_type = c_ulong::MAX;
        if has_resources && result == OK {
            result = f.read_ulong(&mut g.resource_type);
            if result == OK {
                result = f.read_long(&mut g.resource_amount);
            }
        }
        g.is_complex = true;
        if has_kinds && result == OK {
            result = f.read_bool(&mut g.is_complex);
        }
        records.push(g);
        i += 1;
    }
    if result == OK {
        ghosts().records = records;
        after_load(h, Pending::Ghosts);
    }
    result
}

/// `LoadPlayerUnitOrder`: each player's unit ids.
fn load_player_unit_order<H: WorldHost + ?Sized>(h: &mut H) -> i32 {
    let mut f = File(h);
    let mut players: c_ulong = 0;
    let mut orders: Vec<Vec<c_ulong>> = Vec::new();
    let mut result = f.read_ulong(&mut players);
    let mut p: c_ulong = 0;
    while p < players && result == OK {
        orders.push(Vec::new());
        read_ids(&mut f, &mut result, orders.last_mut().expect("pushed"));
        p += 1;
    }
    if result == OK {
        after_load(h, Pending::PlayerUnitOrder(orders));
    }
    result
}

fn part_load<H: WorldHost + ?Sized>(h: &mut H, part: i64, b: i64) -> i32 {
    h.ask(EE_UNITS_WORLD_PART_LOAD, part, b) as i32
}

/// `EEPWorldHelper::Load`'s running state (from 0.254, `EESaveHasLiveState`): each part while the ones before it
/// read and the save has it.
pub fn load_live<H: WorldHost + ?Sized>(h: &mut H) {
    if load_unit_order(h) != OK || load_forests(h) != OK {
        return;
    }
    if !h.has(EE_UNITS_WORLD_HAS_RUNNING_STATE) || load_ai_schedule(h) != OK {
        return;
    }
    if part_load(h, EE_UNITS_WORLD_PART_PATH_RUNNING, 0) != OK
        || part_load(h, EE_UNITS_WORLD_PART_AREA_EFFECTS, 0) != OK
        || load_indexed(h, false) != OK
    {
        return;
    }
    if !h.has(EE_UNITS_WORLD_HAS_REGION_ORDER) || load_indexed(h, true) != OK {
        return;
    }
    if !h.has(EE_UNITS_WORLD_HAS_EXPERIMENT_TIMERS) || part_load(h, EE_UNITS_WORLD_PART_EXPERIMENT_TIMERS, 0) != OK {
        return;
    }
    if !h.has(EE_UNITS_WORLD_HAS_TOWNS) {
        return;
    }
    let areas = h.has(EE_UNITS_WORLD_HAS_TOWN_AREAS);
    if part_load(h, EE_UNITS_WORLD_PART_TOWNS, areas as i64) != OK {
        return;
    }
    if !h.has(EE_UNITS_WORLD_HAS_GHOSTS) {
        return;
    }
    let resources = h.has(EE_UNITS_WORLD_HAS_GHOST_RESOURCES);
    let kinds = h.has(EE_UNITS_WORLD_HAS_GOAL_GHOSTS);
    if load_ghosts(h, resources, kinds) != OK {
        return;
    }
    if !h.has(EE_UNITS_WORLD_HAS_CONTINENT_REGIONS) || part_load(h, EE_UNITS_WORLD_PART_CONTINENT_REGIONS, 0) != OK {
        return;
    }
    if !h.has(EE_UNITS_WORLD_HAS_RESOURCE_STATE) || part_load(h, EE_UNITS_WORLD_PART_RESOURCE_STATE, 0) != OK {
        return;
    }
    if !h.has(EE_UNITS_WORLD_HAS_PLAYER_UNIT_ORDER) || load_player_unit_order(h) != OK {
        return;
    }
    if !h.has(EE_UNITS_WORLD_HAS_PATH_SEARCHES) || part_load(h, EE_UNITS_WORLD_PART_PATH_SEARCHES, 0) != OK {
        return;
    }
    if h.has(EE_UNITS_WORLD_HAS_ACTIVE_GROUPS) && part_load(h, EE_UNITS_WORLD_PART_ACTIVE_GROUPS, 0) != OK {
        return;
    }
    if h.has(EE_UNITS_WORLD_HAS_GRAPH_SEARCHES) {
        part_load(h, EE_UNITS_WORLD_PART_GRAPH_SEARCHES, 0);
    }
}

// ---- the save

/// `count` ids after their count, until the first failed write.
fn write_ids<F: PlayerSaveHost + ?Sized>(f: &mut F, ids: &[c_ulong]) -> i32 {
    let mut result = f.write_ulong(ids.len() as c_ulong);
    for &id in ids {
        if result != OK {
            break;
        }
        result = f.write_ulong(id);
    }
    result
}

fn ids_of<H: WorldHost + ?Sized>(h: &mut H, units: &[u64]) -> Vec<c_ulong> {
    units.iter().map(|&u| h.unit_id(u)).collect()
}

/// `SaveUnitOrder`.
fn save_unit_order<H: WorldHost + ?Sized>(h: &mut H) {
    let chain = h.list(EE_UNITS_WORLD_CHAIN, 0, 0);
    let chain = ids_of(h, &chain);
    let mut result = write_ids(&mut File(h), &chain);
    for l in 0..3 {
        if result != OK {
            break;
        }
        let units = h.list(EE_UNITS_WORLD_STEP_LIST, l, 0);
        let ids = ids_of(h, &units);
        result = write_ids(&mut File(h), &ids);
    }
}

/// The forests the host lists, with a renewal due.
fn renewing_forests<H: WorldHost + ?Sized>(h: &mut H) -> Vec<(u64, c_ulong)> {
    let forests = h.list(EE_UNITS_WORLD_FORESTS, 0, 0);
    let mut out = Vec::new();
    for forest in forests {
        if forest == 0 {
            continue;
        }
        let renewal = h.ask(EE_UNITS_WORLD_FOREST_RENEWAL, forest as i64, 0) as c_ulong;
        if renewal != 0 {
            out.push((forest, renewal));
        }
    }
    out
}

/// `SaveForests`.
fn save_forests<H: WorldHost + ?Sized>(h: &mut H) {
    let forests = renewing_forests(h);
    let mut result = File(h).write_ulong(forests.len() as c_ulong);
    for (forest, renewal) in forests {
        if result != OK {
            break;
        }
        let tiles = h.list(EE_UNITS_WORLD_FOREST_TILES, forest as i64, 0);
        let mut f = File(h);
        result = f.write_ulong(renewal);
        if result == OK {
            result = f.write_ulong(tiles.len() as c_ulong);
        }
        for tile in tiles {
            if result != OK {
                break;
            }
            result = f.write_long(tile as u32 as i32 as c_long);
            if result == OK {
                result = f.write_long((tile >> 32) as u32 as i32 as c_long);
            }
        }
    }
}

/// `SaveAISchedule` (its writes' results ignored).
fn save_ai_schedule<H: WorldHost + ?Sized>(h: &mut H) {
    let mut io = EeUnitsWorldIo::default();
    h.call(EE_UNITS_WORLD_AI_SCHEDULE, &mut io);
    let mut f = File(h);
    f.write_ulong(io.a as c_ulong);
    f.write_ulong(io.b as c_ulong);
}

/// `SaveTileOrder`: every tile with two units or more (a node without a unit as id -1); the writes' results ignored.
fn save_tile_order<H: WorldHost + ?Sized>(h: &mut H) {
    let count = h.ask(EE_UNITS_WORLD_TILE_COUNT, 0, 0).max(0) as c_ulong;
    let mut tiles: Vec<(c_ulong, Vec<c_ulong>)> = Vec::new();
    for i in 0..count {
        let units = h.list(EE_UNITS_WORLD_TILE_UNITS, i as i64, 0);
        if units.len() < 2 {
            continue;
        }
        let ids = units.iter().map(|&u| if u == 0 { c_ulong::MAX } else { h.unit_id(u) }).collect();
        tiles.push((i, ids));
    }
    let mut f = File(h);
    f.write_ulong(tiles.len() as c_ulong);
    for (index, ids) in &tiles {
        f.write_ulong(*index);
        f.write_ulong(ids.len() as c_ulong);
        for &id in ids {
            f.write_ulong(id);
        }
    }
}

/// `SaveRegionOrder`: the combat lane's words as the unsigned longs they were (0xffffffff as -1).
fn save_region_order<H: WorldHost + ?Sized>(h: &mut H) {
    let words = h.list(EE_UNITS_WORLD_REGION_ORDER, 0, 0);
    let mut f = File(h);
    for w in words {
        let w = w as u32;
        f.write_ulong(if w != u32::MAX { w as c_ulong } else { c_ulong::MAX });
    }
}

/// `SaveGhosts`: each unit's record (a simple unit: no placement, not built); the writes' results ignored.
fn save_ghosts<H: WorldHost + ?Sized>(h: &mut H) {
    let mut io = EeUnitsWorldIo::default();
    let mut units = vec![0u64; 64];
    io.list = units.as_mut_ptr();
    io.cap = units.len();
    let n = h.call(EE_UNITS_WORLD_GHOSTS, &mut io).max(0) as usize;
    if n > units.len() {
        units = vec![0u64; n];
        io.list = units.as_mut_ptr();
        io.cap = n;
        h.call(EE_UNITS_WORLD_GHOSTS, &mut io);
    }
    units.truncate(n);
    let complex = io.a.max(0) as usize;
    File(h).write_ulong(units.len() as c_ulong);
    for (i, &unit) in units.iter().enumerate() {
        let is_complex = i < complex;
        let mut g = EeUnitsWorldIo { a: unit as i64, b: is_complex as i64, ..Default::default() };
        h.call(EE_UNITS_WORLD_GHOST_FIELDS, &mut g);
        let mut f = File(h);
        f.write_ulong(g.v[0] as c_ulong);
        f.write_ulong(g.v[1] as c_ulong);
        f.write_ulong(g.v[2] as c_ulong);
        f.write_long(g.v[3] as c_long);
        f.write_long(g.v[4] as c_long);
        f.write_long(g.v[5] as c_long);
        f.write_bool(is_complex && g.v[6] != 0);
        for v in g.m {
            f.write_float(if is_complex { v } else { 0.0 });
        }
        f.write_ulong(g.v[7] as c_ulong);
        f.write_long(g.a as c_long);
        f.write_bool(is_complex);
    }
}

/// `SavePlayerUnitOrder`.
fn save_player_unit_order<H: WorldHost + ?Sized>(h: &mut H) {
    let players = h.ask(EE_UNITS_WORLD_PLAYER_COUNT, 0, 0).max(0) as c_ulong;
    let mut result = File(h).write_ulong(players);
    let mut p: c_ulong = 0;
    while p < players && result == OK {
        let units = h.list(EE_UNITS_WORLD_PLAYER_UNITS, p as i64, 0);
        let ids = ids_of(h, &units);
        result = write_ids(&mut File(h), &ids);
        p += 1;
    }
}

fn part_save<H: WorldHost + ?Sized>(h: &mut H, part: i64) {
    h.ask(EE_UNITS_WORLD_PART_SAVE, part, 0);
}

/// `EEPWorldHelper::Save`'s running state (from 0.254): the parts the version has.
pub fn save_live<H: WorldHost + ?Sized>(h: &mut H) {
    save_unit_order(h);
    save_forests(h);
    if !h.has(EE_UNITS_WORLD_HAS_RUNNING_STATE) {
        return;
    }
    save_ai_schedule(h);
    part_save(h, EE_UNITS_WORLD_PART_PATH_RUNNING);
    part_save(h, EE_UNITS_WORLD_PART_AREA_EFFECTS);
    save_tile_order(h);
    if h.has(EE_UNITS_WORLD_HAS_REGION_ORDER) {
        save_region_order(h);
    }
    if h.has(EE_UNITS_WORLD_HAS_EXPERIMENT_TIMERS) {
        part_save(h, EE_UNITS_WORLD_PART_EXPERIMENT_TIMERS);
    }
    if h.has(EE_UNITS_WORLD_HAS_TOWNS) {
        part_save(h, EE_UNITS_WORLD_PART_TOWNS);
    }
    if h.has(EE_UNITS_WORLD_HAS_GHOSTS) {
        save_ghosts(h);
    }
    if h.has(EE_UNITS_WORLD_HAS_CONTINENT_REGIONS) {
        part_save(h, EE_UNITS_WORLD_PART_CONTINENT_REGIONS);
    }
    if h.has(EE_UNITS_WORLD_HAS_RESOURCE_STATE) {
        part_save(h, EE_UNITS_WORLD_PART_RESOURCE_STATE);
    }
    if h.has(EE_UNITS_WORLD_HAS_PLAYER_UNIT_ORDER) {
        save_player_unit_order(h);
    }
    if h.has(EE_UNITS_WORLD_HAS_PATH_SEARCHES) {
        part_save(h, EE_UNITS_WORLD_PART_PATH_SEARCHES);
    }
    if h.has(EE_UNITS_WORLD_HAS_ACTIVE_GROUPS) {
        part_save(h, EE_UNITS_WORLD_PART_ACTIVE_GROUPS);
    }
    if h.has(EE_UNITS_WORLD_HAS_GRAPH_SEARCHES) {
        part_save(h, EE_UNITS_WORLD_PART_GRAPH_SEARCHES);
    }
}

// ---- the restores at the first tick

/// `RestoreUnitOrder`: the chain relinked in the saved order when it holds the same units x-sorted; the stepped
/// lists made the saved ones (a saved unit the load lacks left out), with their references.
fn restore_unit_order<H: WorldHost + ?Sized>(h: &mut H, chain: &[c_ulong], lists: &[Vec<c_ulong>; 3]) {
    let count = h.list(EE_UNITS_WORLD_CHAIN, 0, 0).len();
    let mut same = count == chain.len();
    let mut units: Vec<u64> = Vec::new();
    let mut last_x: Option<c_long> = None;
    for &id in chain {
        if !same {
            break;
        }
        let unit = h.ask(EE_UNITS_WORLD_UNIT, id as i64, 0) as u64;
        same = unit != 0 && h.ask(EE_UNITS_WORLD_IN_CHAIN, unit as i64, 0) != 0;
        if same {
            let x = h.ask(EE_UNITS_WORLD_TILE_X, unit as i64, 0) as c_long;
            same = last_x.is_none_or(|l| l <= x);
            last_x = Some(x);
        }
        units.push(unit);
    }
    if same {
        let mut prev = 0u64;
        for &unit in &units {
            h.ask(EE_UNITS_WORLD_LINK, unit as i64, prev as i64);
            prev = unit;
        }
    } else {
        h.debug("EEPWorldHelper: the loaded units are not the saved ones: their order is the load's\n");
    }
    for (l, saved) in lists.iter().enumerate() {
        let l = l as i64;
        let mut order: Vec<u64> = Vec::new();
        for &id in saved {
            let unit = h.ask(EE_UNITS_WORLD_COMPLEX, id as i64, 0) as u64;
            if unit != 0 && !order.contains(&unit) {
                order.push(unit);
            } else if unit == 0 {
                h.debug(&format!("EEPWorldHelper: saved stepped unit {id} is not in the loaded game\n"));
            }
        }
        let current = h.list(EE_UNITS_WORLD_STEP_LIST, l, 0);
        for &unit in &current {
            if !order.contains(&unit) {
                h.ask(EE_UNITS_WORLD_STEP_DROP, l, unit as i64);
            }
        }
        for &unit in &order {
            if !current.contains(&unit) {
                h.ask(EE_UNITS_WORLD_STEP_ADD, l, unit as i64);
            }
        }
        h.give(EE_UNITS_WORLD_STEP_SET, l, 0, &order);
    }
}

/// The forests' renewals to the rebuilt forest on each saved forest's tiles.
fn restore_forests<H: WorldHost + ?Sized>(h: &mut H, forests: &[(c_ulong, Vec<(c_long, c_long)>)]) {
    let mut same = true;
    for (renewal, tiles) in forests {
        let mut done: Vec<i64> = Vec::new();
        for &(x, y) in tiles {
            let forest = h.ask(EE_UNITS_WORLD_FOREST_AT, x as i64, y as i64);
            if forest == 0 || done.contains(&forest) {
                continue;
            }
            done.push(forest);
            h.ask(EE_UNITS_WORLD_FOREST_SET_RENEWAL, forest, *renewal as i64);
            same = same && h.ask(EE_UNITS_WORLD_FOREST_SIZE, forest, 0) as c_ulong == tiles.len() as c_ulong;
        }
    }
    if !same {
        h.debug("EEPWorldHelper: a rebuilt forest is not the saved one: its renewal is the saved forest's\n");
    }
}

/// The schedule when the loaded game has the saved player.
fn restore_ai_schedule<H: WorldHost + ?Sized>(h: &mut H, next: c_ulong, time: c_ulong) {
    let players = h.call(EE_UNITS_WORLD_AI_SCHEDULE, &mut EeUnitsWorldIo::default()) as c_ulong;
    if next < players {
        h.ask(EE_UNITS_WORLD_AI_SET_SCHEDULE, next as i64, time as i64);
    } else {
        h.debug("EEPWorldHelper: the loaded computer players are not the saved ones: their schedule is the load's\n");
    }
}

/// `RestoreTileOrder`: each tile holding the saved units, its nodes given them in the saved order.
fn restore_tile_order<H: WorldHost + ?Sized>(h: &mut H, tiles: &[(c_ulong, Vec<c_ulong>)]) {
    let count = h.ask(EE_UNITS_WORLD_TILE_COUNT, 0, 0);
    let mut other: c_ulong = 0;
    for (index, ids) in tiles {
        if count == 0 || *index >= count as c_ulong {
            break;
        }
        let nodes = h.list(EE_UNITS_WORLD_TILE_UNITS, *index as i64, 0);
        let mut units: Vec<u64> = Vec::new();
        if nodes.len() == ids.len() {
            for &id in ids {
                let mut found = 0u64;
                for &unit in &nodes {
                    if found != 0 {
                        break;
                    }
                    if unit != 0 && h.unit_id(unit) == id && !units.contains(&unit) {
                        found = unit;
                    }
                }
                if found == 0 {
                    break;
                }
                units.push(found);
            }
        }
        if units.len() != nodes.len() || units.len() != ids.len() {
            other += 1;
            continue;
        }
        for (n, &unit) in units.iter().enumerate() {
            h.give(EE_UNITS_WORLD_TILE_SET, *index as i64, n as i64, &[unit]);
        }
    }
    if other != 0 {
        h.debug(&format!("EEPWorldHelper: {other} tiles hold other units than the saved ones: their order is the load's\n"));
    }
}

/// `RestoreRegionOrder`: the combat lane's restore of each saved region.
fn restore_region_order<H: WorldHost + ?Sized>(h: &mut H, regions: &[(c_ulong, Vec<c_ulong>)]) {
    let mut other: c_ulong = 0;
    for (index, ids) in regions {
        let words: Vec<u64> = ids.iter().map(|&id| id as u32 as u64).collect();
        let restored = h.give(EE_UNITS_WORLD_REGION_RESTORE, *index as u32 as i64, 0, &words) as i32;
        if restored < 0 {
            break;
        }
        other += (restored == 0) as c_ulong;
    }
    if other != 0 {
        h.debug(&format!(
            "EEPWorldHelper: {other} map regions hold other units than the saved ones: their order is the load's\n"
        ));
    }
}

/// `RestorePlayerUnitOrder`: the player's units in the saved order, then any the save did not list after the last
/// unit of its bucket; each bucket's first unit follows.
fn restore_player_unit_order<H: WorldHost + ?Sized>(h: &mut H, player: c_ulong, ids: &[c_ulong]) {
    let players = h.ask(EE_UNITS_WORLD_PLAYER_COUNT, 0, 0) as c_ulong;
    if player >= players {
        return;
    }
    let current = h.list(EE_UNITS_WORLD_PLAYER_UNITS, player as i64, 0);
    let mut order: Vec<u64> = Vec::new();
    for &id in ids {
        let unit = h.ask(EE_UNITS_WORLD_PLAYER_UNIT, player as i64, id as i64) as u64;
        if unit != 0 && !order.contains(&unit) {
            order.push(unit);
        }
    }
    let buckets: Vec<(u64, c_ulong)> = current.iter().map(|&u| (u, h.unit_id(u) & 0x7f)).collect();
    let bucket_of = |h: &mut H, u: u64| -> c_ulong {
        buckets.iter().find(|(x, _)| *x == u).map_or_else(|| h.unit_id(u) & 0x7f, |&(_, b)| b)
    };
    for &unit in &current {
        if order.contains(&unit) {
            continue;
        }
        let bucket = bucket_of(h, unit);
        let mut at = order.len();
        for k in (1..=order.len()).rev() {
            if bucket_of(h, order[k - 1]) == bucket {
                at = k;
                break;
            }
        }
        order.insert(at, unit);
    }
    if order.len() != current.len() {
        h.debug(&format!(
            "EEPWorldHelper: player {player}'s saved units are not its list's: their order is the load's\n"
        ));
        return;
    }
    if order == current {
        return;
    }
    h.give(EE_UNITS_WORLD_PLAYER_SET_LIST, player as i64, 0, &order);
    let mut seen = [false; 0x80];
    for &unit in &order {
        let bucket = bucket_of(h, unit) as usize;
        if !seen[bucket] {
            h.give(EE_UNITS_WORLD_PLAYER_SET_BUCKET, player as i64, bucket as i64, &[unit]);
        }
        seen[bucket] = true;
    }
}

/// The ghosts made from the records are dereferenced (the loaded units' references keep them from here on) and the
/// records dropped.
fn restore_ghosts<H: WorldHost + ?Sized>(h: &mut H) {
    let made = std::mem::take(&mut ghosts().made);
    for unit in made {
        h.ask(EE_UNITS_WORLD_DEREFERENCE, unit as i64, 0);
    }
    ghosts().records.clear();
}

/// One part put back at the first tick.
pub fn restore<H: WorldHost + ?Sized>(h: &mut H, pending: &Pending) {
    match pending {
        Pending::UnitOrder(chain, lists) => restore_unit_order(h, chain, lists),
        Pending::Forests(forests) => restore_forests(h, forests),
        Pending::AiSchedule(next, time) => restore_ai_schedule(h, *next, *time),
        Pending::TileOrder(tiles) => restore_tile_order(h, tiles),
        Pending::RegionOrder(regions) => restore_region_order(h, regions),
        Pending::Ghosts => restore_ghosts(h),
        Pending::PlayerUnitOrder(orders) => {
            for (p, ids) in orders.iter().enumerate() {
                restore_player_unit_order(h, p as c_ulong, ids);
            }
        }
    }
}

// ---- FFI

struct FfiHost {
    ctx: *mut c_void,
    f: EeUnitsWorldFn,
}

impl WorldHost for FfiHost {
    fn call(&mut self, op: i32, io: &mut EeUnitsWorldIo) -> i64 {
        // SAFETY: the C++ callback with its own context; io lives for the call
        unsafe { (self.f)(self.ctx, op, io) }
    }
}

/// `EEPWorldHelper::Load`'s running state over the host.
///
/// # Safety
/// `f` is valid with `ctx` for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_world_load_live(ctx: *mut c_void, f: EeUnitsWorldFn) {
    load_live(&mut FfiHost { ctx, f });
}

/// `EEPWorldHelper::Save`'s running state over the host.
///
/// # Safety
/// As [`ee_units_world_load_live`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_world_save_live(ctx: *mut c_void, f: EeUnitsWorldFn) {
    save_live(&mut FfiHost { ctx, f });
}

/// The part `pending` (an `EE_UNITS_WORLD_AFTER_LOAD`'s `a`) put back, then freed.
///
/// # Safety
/// `pending` is an `EE_UNITS_WORLD_AFTER_LOAD` record not restored before; `f` is valid with `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_world_restore(pending: *mut c_void, ctx: *mut c_void, f: EeUnitsWorldFn) {
    if pending.is_null() {
        return;
    }
    // SAFETY: the box after_load made
    let pending = unsafe { Box::from_raw(pending as *mut Pending) };
    restore(&mut FfiHost { ctx, f }, &pending);
}

/// The loaded ghost of id `id` into `out`: false when no record has the id.
///
/// # Safety
/// `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_units_world_ghost_find(id: c_ulong, out: *mut EeUnitsWorldGhost) -> bool {
    let g = ghosts();
    let Some(record) = g.records.iter().find(|r| r.id == id) else {
        return false;
    };
    // SAFETY: the contract
    unsafe { *out = *record };
    true
}

/// A unit made from a loaded ghost (one reference, dropped at the first tick).
#[no_mangle]
pub extern "C" fn ee_units_world_ghost_made(unit: *mut c_void) {
    ghosts().made.push(unit as usize);
}

/// The made ghost unit whose id `unit_id` answers `id`, or null.
///
/// # Safety
/// `unit_id` is valid for every made unit.
#[no_mangle]
pub unsafe extern "C" fn ee_units_world_ghost_made_find(
    id: c_ulong,
    unit_id: unsafe extern "C" fn(*mut c_void) -> c_ulong,
) -> *mut c_void {
    let made = ghosts().made.clone();
    for unit in made {
        // SAFETY: the contract
        if unsafe { unit_id(unit as *mut c_void) } == id {
            return unit as *mut c_void;
        }
    }
    std::ptr::null_mut()
}

#[cfg(test)]
#[path = "world_save_tests.rs"]
mod tests;
