//! The continent manager's queries and changes as its callers make them (was EEAIContinentManager.cpp): each one
//! its CNTQ record (cm_capture.rs) around the record's lookups (cm_query.rs, cm_io.rs, cm_repair.rs). The C++
//! class's members are inline forwarders to these (ee/EEAIContinentManager.h); a unit is passed as its handle and
//! read through the world's views (its id, its tile).

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::ffi::{c_long, c_void};

use crate::astar::cm::{EeAStarCmRecord, EeAStarCmTable};
use crate::astar::cm_capture::{self as cap, Query};
use crate::astar::cm_io;
use crate::astar::cm_query;
use crate::astar::continent_grid::EePoint;
use crate::astar::hull::EeHullPoint;
use crate::astar::manager;
use crate::world::{UnitView, WorldRead};

fn token() -> WorldRead {
    // SAFETY: the simulation thread's reads of the live world
    unsafe { WorldRead::begin() }
}

/// A unit's id (-1 for none) and tile (`mTileX`, `mTileY`).
fn unit_facts(unit: u64) -> (i64, c_long, c_long) {
    let t = token();
    // SAFETY: 0 or a live unit
    match unsafe { UnitView::of(&t, unit) } {
        Some(u) => {
            let (x, y) = u.tile_xy();
            (u.id() as i64, x as c_long, y as c_long)
        }
        None => (-1, 0, 0),
    }
}

/// A unit's tile (`mTileX`, `mTileY`), for the manager's lookups by a unit.
///
/// # Safety
/// `unit` a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_unit_tile(unit: u64) -> EeHullPoint {
    let (_, x, y) = unit_facts(unit);
    EeHullPoint::new(x, y)
}

unsafe fn rec<'a>(r: *const EeAStarCmRecord) -> &'a EeAStarCmRecord {
    &*r
}

unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if n == 0 || p.is_null() {
        &[]
    } else {
        std::slice::from_raw_parts(p, n)
    }
}

fn pairs(p: &[EeHullPoint]) -> Vec<(c_long, c_long)> {
    p.iter().map(|t| (t.x, t.y)).collect()
}

/// `GetConnectedIDs` (`with_self`) / `GetAdjacentIDs`: the continents adjacent to `id` into `out` (room for the
/// matrix's width); their count (0 for an unknown id).
///
/// # Safety
/// `r` a live record; `out` holds `flags_width` values.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_adjacent_ids(r: *const EeAStarCmRecord, id: u16, with_self: bool, out: *mut u16) -> usize {
    let r = rec(r);
    let q = Query::new(if with_self { cap::Q_CONNECTED_IDS } else { cap::Q_ADJACENT_IDS });
    q.i(id as i64);
    let ids = if (id as c_long) < r.flags_width { r.adjacent_ids(id, with_self) } else { Vec::new() };
    if !ids.is_empty() {
        std::ptr::copy_nonoverlapping(ids.as_ptr(), out, ids.len());
    }
    q.shorts(&ids);
    ids.len()
}

/// `HasShoreTile(tiles)`.
///
/// # Safety
/// `r` a live record; `tiles` holds `n` points.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_has_shore_tile(r: *const EeAStarCmRecord, tiles: *const EeHullPoint, n: usize) -> bool {
    let (r, tiles) = (rec(r), slice(tiles, n));
    let q = Query::new(cap::Q_HAS_SHORE_TILE);
    q.points(&pairs(tiles));
    let result = tiles.iter().any(|&t| r.tables().is_shore(t));
    q.i(result as i64);
    result
}

/// `InSameContinent(first, second)`: the two units' tiles have one continent id.
///
/// # Safety
/// `r` a live record; the units live.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_in_same_continent(r: *const EeAStarCmRecord, first: u64, second: u64) -> bool {
    let r = rec(r);
    let q = Query::new(cap::Q_IN_SAME_CONTINENT);
    let (a, ax, ay) = unit_facts(first);
    let (b, bx, by) = unit_facts(second);
    q.i(a);
    q.i(b);
    let result = cm_io::continent_id_at(r, ax, ay) == cm_io::continent_id_at(r, bx, by);
    q.i(result as i64);
    result
}

/// `InSameArea(tiles, kind)`.
///
/// # Safety
/// `r` a live record; `tiles` holds `n` points.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_in_same_area(r: *const EeAStarCmRecord, tiles: *const EeHullPoint, n: usize, kind: c_long) -> bool {
    let (r, tiles) = (rec(r), slice(tiles, n));
    let q = Query::new(cap::Q_IN_SAME_AREA);
    q.points(&pairs(tiles));
    q.i(kind as i64);
    let result = cm_query::in_same_area(r, tiles, kind);
    q.i(result as i64);
    result
}

/// The area lookups by a tile: 0 the choke point (`GetChokePointAt`), 1 the forest, 2 the hill, 3 the path area.
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_area_at(r: *const EeAStarCmRecord, kind: i32, x: c_long, y: c_long) -> u64 {
    let r = rec(r);
    let q = Query::new(match kind {
        0 => cap::Q_CHOKE_POINT_AT,
        1 => cap::Q_FOREST_AT,
        2 => cap::Q_HILL_AT,
        _ => cap::Q_PATH_AREA_AT,
    });
    q.tile(x as i64, y as i64);
    let result = match kind {
        0 => cm_query::choke_point_at(r, x, y),
        1 => cm_query::forest_at(r, x, y),
        2 => cm_query::hill_at(r, x, y),
        _ => cm_query::path_area_at(r, x, y),
    };
    q.area(result);
    result
}

/// `GetChokePoints(out, id, clear)`: continent `id`'s choke points (every one for 0) appended to the list
/// (`before`, `count_before`: the list on entry, after the clear when `clear`); `push(ctx, handle)` each.
///
/// # Safety
/// `r` a live record; `before` holds `count_before` handles; `push` accepts `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_choke_points(
    r: *const EeAStarCmRecord,
    id: u16,
    clear: bool,
    before: *const u64,
    count_before: usize,
    ctx: *mut c_void,
    push: extern "C" fn(*mut c_void, u64),
) {
    let r = rec(r);
    let q = Query::new(cap::Q_CHOKE_POINTS);
    q.i(id as i64);
    q.i(clear as i64);
    q.i(count_before as i64);
    let mut list: Vec<u64> = if clear { Vec::new() } else { slice(before, count_before).to_vec() };
    for h in cm_query::choke_points(r, id) {
        push(ctx, h);
        list.push(h);
    }
    q.areas(&list);
}

/// `GetContinentCenter(id)`: its hull's center, (0, 0) for an unknown id.
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_continent_center(r: *const EeAStarCmRecord, id: u16) -> EeHullPoint {
    let r = rec(r);
    let q = Query::new(cap::Q_CONTINENT_CENTER);
    q.i(id as i64);
    let c = r.tables().tables[EeAStarCmTable::Continents as usize].get(id as usize).copied().unwrap_or(0);
    let p = if c == 0 { EeHullPoint::default() } else { cm_io::area(c).hull_center };
    q.tile(p.x as i64, p.y as i64);
    p
}

/// `GetContinentTiles(id, out)`: `push(ctx, x, y)` for each of continent `id`'s tiles after the caller's
/// `count_before` (the list is rebuilt: `clear(ctx)` first; nothing for an unknown id).
///
/// # Safety
/// `r` a live record; the callbacks accept `ctx`; `before` holds `count_before` points (the list on entry).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_continent_tiles(
    r: *const EeAStarCmRecord,
    id: u16,
    before: *const EeHullPoint,
    count_before: usize,
    ctx: *mut c_void,
    clear: extern "C" fn(*mut c_void),
    push: extern "C" fn(*mut c_void, c_long, c_long),
) {
    let r = rec(r);
    let q = Query::new(cap::Q_CONTINENT_TILES);
    q.i(id as i64);
    q.i(count_before as i64);
    let c = r.tables().tables[EeAStarCmTable::Continents as usize].get(id as usize).copied().unwrap_or(0);
    if c == 0 {
        q.points(&pairs(slice(before, count_before)));
        return;
    }
    let mut points = Vec::new();
    cm_io::area_tiles(c).set.points(crate::astar::sparse::PointList::All(1), &mut points);
    clear(ctx);
    for p in &points {
        push(ctx, p.x as c_long, p.y as c_long);
    }
    q.points(&points.iter().map(|p| (p.x as c_long, p.y as c_long)).collect::<Vec<_>>());
}

/// `AreConnected(first, second, flags)`.
///
/// # Safety
/// The continents 0 or live.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_are_connected(first: u64, second: u64, flags: c_long) -> bool {
    let q = Query::new(cap::Q_CONNECTED);
    q.area(first);
    q.area(second);
    q.i(flags as i64);
    let result = cm_query::are_connected(first, second, flags as i32);
    q.i(result as i64);
    result
}

/// `GetLargestContinent(kind)`.
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_largest_continent(r: *const EeAStarCmRecord, kind: c_long) -> c_long {
    let r = rec(r);
    let q = Query::new(cap::Q_LARGEST_CONTINENT);
    q.i(kind as i64);
    let result = cm_query::largest_continent(r, kind as i8) as c_long;
    q.i(result as i64);
    result
}

/// `GetContinent(tile, kind)`: the kind's region id of the tile's path area.
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_region_id(r: *const EeAStarCmRecord, x: c_long, y: c_long, kind: c_long) -> u16 {
    let r = rec(r);
    let q = Query::new(cap::Q_CONTINENT);
    q.tile(x as i64, y as i64);
    q.i(kind as i64);
    let result = cm_query::region_id_at(r, x, y, kind);
    q.i(result as i64);
    result
}

/// `GetPathSubSectorAt(unit)`: the sub sector at the unit's tile (0 if none).
///
/// # Safety
/// `r` a live record; `unit` live.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_sub_sector_at(r: *const EeAStarCmRecord, unit: u64) -> u64 {
    let r = rec(r);
    let q = Query::new(cap::Q_SUB_SECTOR_AT);
    let (id, x, y) = unit_facts(unit);
    q.i(id);
    let sid = cm_io::sub_sector_id_at(r, x, y);
    let h = if sid == 0 {
        0
    } else {
        r.tables().tables[EeAStarCmTable::PathSubSectors as usize].get(sid as usize).copied().unwrap_or(0)
    };
    q.i(if h == 0 { -1 } else { cm_io::sub(h).id as i64 });
    h
}

/// `GetRandomArea(kind, random, continent)` (`continents` false) / `GetRandomContinent(kind, random)`.
///
/// # Safety
/// `r` a live record; `random` a live `URandom*`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_random_area(
    r: *const EeAStarCmRecord,
    continents: bool,
    kind: c_long,
    random: u64,
    continent: u16,
) -> u64 {
    let r = rec(r);
    let q = Query::new(if continents { cap::Q_RANDOM_CONTINENT } else { cap::Q_RANDOM_AREA });
    q.i(kind as i64);
    if !continents {
        q.i(continent as i64);
    }
    let result = if continents {
        cm_query::random_continent(r, kind as u8, random)
    } else {
        cm_query::random_area(r, kind, random, continent)
    };
    q.area(result);
    result
}

/// `GetTilesNextTo(x, y, out, id)` without a CNTQ scope (its CNTN record): whether tile (x, y) is on continent
/// `id` or on an adjacent one with a neighbour tile on `id`; `out` gets those neighbours.
pub fn tiles_next_to(r: &EeAStarCmRecord, x: c_long, y: c_long, id: u16, out: &mut Vec<EePoint>) -> bool {
    let flags: &[bool] = if r.tile_flags.is_null() {
        &[]
    } else {
        // SAFETY: the matrix's flags_width x flags_width entries
        unsafe { std::slice::from_raw_parts(r.tile_flags, (r.flags_width * r.flags_width) as usize) }
    };
    let result = manager::tiles_next_to(
        x as i32,
        y as i32,
        id,
        r.width as u32,
        r.height as u32,
        flags,
        r.flags_width as i32,
        &mut |tx, ty| cm_io::continent_id_at(r, tx as c_long, ty as c_long),
        out,
    );
    let tiles: Vec<(c_long, c_long)> = out.iter().map(|p| (p.x as c_long, p.y as c_long)).collect();
    cap::next_to(x, y, id, result, &tiles);
    result
}

/// `GetTilesNextTo(x, y, out, id)`: `out` gets up to 8 tiles, `count` their number.
///
/// # Safety
/// `r` a live record; `out` holds 8 points; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_tiles_next_to(
    r: *const EeAStarCmRecord,
    x: c_long,
    y: c_long,
    id: u16,
    out: *mut EePoint,
    count: *mut usize,
) -> bool {
    let mut tiles = Vec::new();
    let result = tiles_next_to(rec(r), x, y, id, &mut tiles);
    let n = tiles.len().min(8);
    std::ptr::copy_nonoverlapping(tiles.as_ptr(), out, n);
    *count = n;
    result
}

/// `IsNextToArea(x, y, id)`.
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_is_next_to_area(r: *const EeAStarCmRecord, x: c_long, y: c_long, id: u16) -> bool {
    let q = Query::new(cap::Q_NEXT_TO_AREA);
    q.i(x as i64);
    q.i(y as i64);
    q.i(id as i64);
    let mut tiles = Vec::new();
    let result = tiles_next_to(rec(r), x, y, id, &mut tiles);
    q.i(result as i64);
    result
}

/// `RepairPathArea(tile)`.
///
/// # Safety
/// `r` the live record, not in use elsewhere.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_repair_path_area(r: *mut EeAStarCmRecord, x: c_long, y: c_long) {
    let q = Query::new(cap::Q_REPAIR_PATH_AREA);
    q.tile(x as i64, y as i64);
    crate::astar::cm_repair::repair_path_area(&mut *r, x, y);
    q.state(&*r);
}

/// `RemoveForestTile(unit)`: once no wood with resources is left on the unit's tile, the tile leaves its forest.
///
/// # Safety
/// `r` the live record, not in use elsewhere; `unit` live.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_remove_forest_tile(r: *mut EeAStarCmRecord, unit: u64) {
    let q = Query::new(cap::Q_REMOVE_FOREST_TILE);
    let (id, x, y) = unit_facts(unit);
    q.i(id);
    if unit != 0 {
        q.tile(x as i64, y as i64);
    }
    let t = token();
    let tile = crate::world::WorldView::global(&t).and_then(|w| w.map()).and_then(|m| m.tile(x as i64, y as i64));
    let mut empty = true;
    if let Some(tl) = tile {
        if crate::astar::world_view::ee_astar_tile_resource_kind(tl.handle()) != 0 {
            for u in tl.units().flatten() {
                let wood = u.class().is_some_and(|c| {
                    let v: c_long = crate::world_field!(c.fields(), mResourceType);
                    v as i64 == RESOURCE_WOOD
                });
                if wood && u.resource_amount() > 0 {
                    empty = false;
                    break;
                }
            }
        }
    }
    cm_query::remove_forest_tile(&mut *r, x, y, empty);
    q.state(&*r);
}

const RESOURCE_WOOD: i64 = 3;

/// A query left in C++ (`CanGetTo`, `AreConnectedForUnit`, `UpdateWallUnit`) records an area as Rust does.
///
/// # Safety
/// `h` 0 or a live area.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cntq_area(h: u64) {
    if h == 0 {
        cap_i(-1);
        cap_i(-1);
    } else {
        let a = cm_io::area(h);
        cap_i(a.serial as i64);
        cap_i(a.area_type as i64);
    }
}

fn cap_i(v: i64) {
    cap::ee_astar_cntq_i(v as i32);
}

/// A C++ query's change ends with the state's digest.
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cntq_state(r: *const EeAStarCmRecord) {
    let d = cm_io::capture_digest(&*r);
    cap_i(d as u32 as i32 as i64);
    cap_i((d >> 32) as u32 as i32 as i64);
}

/// `FindContinentID(x, y)`: the continent found through the tile's path sector, its id (0 if none).
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_found_continent_id(r: *const EeAStarCmRecord, x: c_long, y: c_long) -> u16 {
    match cm_io::find_continent(rec(r), x, y) {
        0 => 0,
        c => cm_io::continent(c).id,
    }
}

/// `FindPathSubSectorID(x, y)`: the sub sector found through the tile's path sector, its id (0 if none).
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_sub_sector_id_found(r: *const EeAStarCmRecord, x: c_long, y: c_long) -> u16 {
    match cm_io::find_sub_sector(rec(r), x, y) {
        0 => 0,
        s => cm_io::sub(s).id,
    }
}

/// `AddArea(area)`: the serial table gets the area under its serial.
///
/// # Safety
/// `r` the live record; `area` a live area.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_add_object_area(r: *mut EeAStarCmRecord, area: u64) {
    let serial = cm_io::area(area).serial;
    (*r).tables_mut().add_area(serial, area);
}

/// `RemoveArea(area)`: its serial leaves the table; whether it was there.
///
/// # Safety
/// `r` the live record; `area` a live area.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_remove_object_area(r: *mut EeAStarCmRecord, area: u64) -> bool {
    let serial = cm_io::area(area).serial;
    (*r).tables_mut().remove_area(serial)
}

/// `RemoveSectorTiles(sector)`: its pending tiles dropped.
///
/// # Safety
/// `r` the live record; `sector` a live sector.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_remove_sector_tiles_of(r: *mut EeAStarCmRecord, sector: u64) {
    let id = cm_io::sector(sector).id;
    (*r).tables_mut().remove_sector_tiles(id);
}
