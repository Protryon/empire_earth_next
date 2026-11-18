//! `RepairPathArea` (EEAIContinentManager.cpp 0x2372b): tile (x, y) changed kind, so it leaves its path area for
//! the largest same-kind neighbouring area (the others merged into it), its single neighbour's area or a new path
//! area; the sectors and sub sectors follow the new id, an emptied area goes. `repair` chooses (`repair_plan`,
//! `adjacent_area_id`); this does the bookkeeping over the records. The C++ answers through `cm_io`'s host what
//! needs the world or the objects' C++ geometry: the tile's kind and wall, its terrain, an area's `Create` and
//! `RemoveTile` (their hulls are captured), making and deleting a path area.
//!
//! RUST_PLAN 2.1: the C++'s order of every lookup (each `GetAdjacentAreaID` reads the tile and its eight neighbours
//! again: their `Contains` calls are captured), its loops from the last element down, its `std::sort`/`std::unique`.

// `c_long`/`c_ulong` are 32 bits on Windows, 64 on LP64: their casts are needed on one or the other
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_long;

use crate::astar::cm::{EeAStarCmFree, EeAStarCmRecord, EeAStarCmTable};
use crate::astar::cm_io::{self, EeAStarCmIo};
use crate::astar::cm_query;
use crate::astar::continent::EeAStarContinentList;
use crate::astar::continent_grid::EePoint;
use crate::astar::hull::EeHullPoint;
use crate::astar::repair::{self, EeAreaTile, NEIGHBOURS};
use crate::astar::sparse::PointList;

/// `EEAIGetTileKind((i0, i1))`
pub const EE_ASTAR_CM_TILE_KIND: i32 = 30;
/// `EEAIGetTileWallID((i0, i1))`
pub const EE_ASTAR_CM_WALL_ID: i32 = 31;
/// tile (`i0`, `i1`)'s terrain, -1 without a tile
pub const EE_ASTAR_CM_TERRAIN: i32 = 32;
/// area `h0`'s `Create` of the `i0` points at `h1` (`EeHullPoint`s)
pub const EE_ASTAR_CM_AREA_CREATE: i32 = 33;
/// area `h0`'s `RemoveTile((i0, i1))` (1/0; a forest's own)
pub const EE_ASTAR_CM_AREA_REMOVE_TILE: i32 = 34;

const TILE_FOREST: i32 = 4;
const TILE_WATER: i32 = 2;
const TILE_MAP_EDGE: i32 = 6;
const TILE_UNIT_OBSTACLE: i32 = 0xc;
const TERRAIN_DEEP_WATER: i64 = 1;
const TERRAIN_SHALLOW_WATER: i64 = 10;
const TERRAIN_SPACE: i64 = 12;

fn host(op: i32, h: [u64; 2], i: [i64; 4]) -> i64 {
    let mut io = EeAStarCmIo { h, i };
    cm_io::call_host(op, &mut io)
}

fn table(r: &EeAStarCmRecord, t: EeAStarCmTable, index: usize) -> u64 {
    r.tables().tables[t as usize].get(index).copied().unwrap_or(0)
}

fn set_table(r: &mut EeAStarCmRecord, t: EeAStarCmTable, index: usize, h: u64) {
    if let Some(slot) = r.tables_mut().tables[t as usize].get_mut(index) {
        *slot = h;
    }
}

/// `EEAIAreaTileAt`: the tile as `repair` reads it, and its path area.
fn area_tile(r: &EeAStarCmRecord, x: c_long, y: c_long) -> (EeAreaTile, u64) {
    let terrain = host(EE_ASTAR_CM_TERRAIN, [0, 0], [x as i64, y as i64, 0, 0]);
    let mut t = EeAreaTile { tile: terrain >= 0, ..Default::default() };
    let mut a = 0;
    if t.tile {
        t.terrain = terrain as i32;
        a = cm_query::path_area_at(r, x, y);
    }
    t.area = a != 0;
    if a != 0 {
        // SAFETY: a live path area
        let (p, size) = unsafe { (cm_io::path_area(a), cm_io::area_tiles(a).set.size()) };
        t.id = p.id;
        t.continent = p.continent_id;
        t.kind = p.kind as i32;
        t.unit_id = p.unit_id as i32;
        t.size = size;
        t.land_mass = p.land_mass;
        t.water_body = p.water_body;
        t.space_region = p.space_region;
        t.surface_region = p.surface_region;
    }
    (t, a)
}

/// `EEAIAreaNeighbours`: the eight neighbours in the C++'s order and their areas.
fn neighbours(r: &EeAStarCmRecord, x: c_long, y: c_long) -> ([EeAreaTile; 8], [u64; 8]) {
    let mut tiles = [EeAreaTile::default(); 8];
    let mut areas = [0u64; 8];
    for (k, (dx, dy)) in NEIGHBOURS.iter().enumerate() {
        let (t, a) = area_tile(r, x + *dx as c_long, y + *dy as c_long);
        tiles[k] = t;
        areas[k] = a;
    }
    (tiles, areas)
}

/// `GetAdjacentAreaID(kind, tile, area kind)`: the tile and its neighbours read again, then `repair`'s choice.
pub fn adjacent_area_id(r: &EeAStarCmRecord, kind: i32, x: c_long, y: c_long, area_kind: i32) -> u16 {
    if host(cm_io::EE_ASTAR_CM_MAP_SIZE, [0, 0], [0; 4]) < 0 {
        return 0;
    }
    let (centre, _) = area_tile(r, x, y);
    let (around, _) = neighbours(r, x, y);
    repair::adjacent_area_id(kind, area_kind, &centre, &around)
}

fn points_of(h: u64, out: &mut Vec<EeHullPoint>) {
    let mut list = Vec::new();
    // SAFETY: a live area
    unsafe { cm_io::area_tiles(h).set.points(PointList::All(1), &mut list) };
    out.clear();
    out.extend(list.iter().map(|p| EeHullPoint { x: p.x as c_long, y: p.y as c_long }));
}

fn create(h: u64, tiles: &[EeHullPoint]) {
    host(EE_ASTAR_CM_AREA_CREATE, [h, tiles.as_ptr() as u64], [tiles.len() as i64, 0, 0, 0]);
}

/// The area goes: its continent's and sectors' lists, the sub sectors that named it take `new_id`, the serial table,
/// the table (its id freed), the object.
fn drop_area(r: &mut EeAStarCmRecord, old: u64, new_id: u16, sector_ids: &[u16], from_sector_table_index: bool) {
    // SAFETY: a live path area
    let (old_id, old_continent, serial) =
        unsafe { (cm_io::path_area(old).id, cm_io::path_area(old).continent_id, cm_io::area(old).serial) };
    let c = table(r, EeAStarCmTable::Continents, old_continent as usize);
    if c != 0 {
        // SAFETY: a live continent
        unsafe { cm_io::continent(c).lists_mut().remove_id(EeAStarContinentList::PathAreas, old_id) };
    }
    for &s in sector_ids.iter().rev() {
        let sector = table(r, EeAStarCmTable::PathSectors, s as usize);
        if sector != 0 {
            // SAFETY: a live sector
            let lists = unsafe { &mut *cm_io::sector(sector).lists };
            lists.remove_path_area_id(old_id);
            if from_sector_table_index {
                lists.add_path_area_id(new_id);
            }
        }
    }
    let subs = r.tables().tables[EeAStarCmTable::PathSubSectors as usize].clone();
    for &s in subs.iter().rev() {
        // SAFETY: a live sub sector
        if s != 0 && unsafe { cm_io::sub(s).path_area_id } == old_id {
            unsafe { cm_io::sub(s).path_area_id = new_id };
        }
    }
    r.tables_mut().remove_area(serial);
    set_table(r, EeAStarCmTable::PathAreas, old_id as usize, 0);
    r.tables_mut().free[EeAStarCmFree::PathAreas as usize].push(old_id);
    host(cm_io::EE_ASTAR_CM_DELETE, [old, 0], [EeAStarCmTable::PathAreas as i64, 0, 0, 0]);
}

/// `RepairPathArea(tile)`.
pub fn repair_path_area(r: &mut EeAStarCmRecord, x: c_long, y: c_long) {
    let sub = match cm_io::sub_sector_id_at(r, x, y) {
        0 => 0,
        id => table(r, EeAStarCmTable::PathSubSectors, id as usize),
    };
    let area = cm_query::path_area_at(r, x, y);
    let terrain = host(EE_ASTAR_CM_TERRAIN, [0, 0], [x as i64, y as i64, 0, 0]);
    let kind = host(EE_ASTAR_CM_TILE_KIND, [0, 0], [x as i64, y as i64, 0, 0]) as i32;
    let continent = cm_io::continent_id_at(r, x, y);
    // SAFETY: a live path area
    if area == 0 || unsafe { cm_io::path_area(area).kind } as i32 == kind {
        return;
    }
    if sub != 0 {
        // SAFETY: a live sub sector
        unsafe { cm_io::sub(sub).is_stale = true };
    }
    let mut merged: Vec<u64> = Vec::new();
    let mut tiles: Vec<EeHullPoint> = Vec::new();
    let mut other: Vec<EeHullPoint> = Vec::new();
    let mut new_id: u16 = 0;
    let tile = EeHullPoint { x, y };
    if terrain >= 0 {
        let (around, areas) = neighbours(r, x, y);
        let wall = if kind == TILE_UNIT_OBSTACLE {
            host(EE_ASTAR_CM_WALL_ID, [0, 0], [x as i64, y as i64, 0, 0]) as i32
        } else {
            -1
        };
        // SAFETY: a live path area
        let plan = repair::repair_plan(kind, unsafe { cm_io::path_area(area).continent_id }, continent, wall, &around);
        if plan.action == repair::REPAIR_JOIN {
            let best = areas[plan.target as usize];
            points_of(best, &mut tiles);
            // SAFETY: a live area (its tile set only: no hull)
            unsafe { cm_io::area_tiles(best).set.insert(EePoint { x: x as i32, y: y as i32 }) };
            tiles.push(tile);
            for k in 0..plan.merged_count as usize {
                let o = areas[plan.merged[k] as usize];
                merged.push(o);
                points_of(o, &mut other);
                tiles.extend_from_slice(&other);
            }
            create(best, &tiles);
            // SAFETY: a live path area
            new_id = unsafe { cm_io::path_area(best).id };
        } else if plan.action == repair::REPAIR_SINGLE {
            let single = areas[plan.target as usize];
            points_of(single, &mut tiles);
            tiles.push(tile);
            create(single, &tiles);
            // SAFETY: a live path area
            new_id = unsafe { cm_io::path_area(single).id };
        } else {
            let single = host(cm_io::EE_ASTAR_CM_NEW, [0, 0], [EeAStarCmTable::PathAreas as i64, 0, 0, 0]) as u64;
            let id = r.new_path_area_id();
            r.last_serial = r.last_serial.wrapping_add(1);
            let serial = r.last_serial;
            {
                // SAFETY: the new path area
                let p = unsafe { cm_io::path_area(single) };
                p.continent_id = cm_io::continent_id_at(r, x, y);
                p.kind = kind as c_long;
                p.id = id;
                unsafe { cm_io::area(single).serial = serial };
            }
            if kind == TILE_UNIT_OBSTACLE {
                let wall = host(EE_ASTAR_CM_WALL_ID, [0, 0], [x as i64, y as i64, 0, 0]);
                // SAFETY: as above
                unsafe { cm_io::path_area(single).unit_id = wall as c_long };
            }
            let region = |r: &mut EeAStarCmRecord, which: c_long| -> u16 {
                let found = adjacent_area_id(r, which as i32, x, y, kind);
                if found != 0 {
                    return found;
                }
                let next = match which {
                    cm_query::PATH_SPACE_REGION => &mut r.next_space_region,
                    cm_query::PATH_NON_SPACE => &mut r.next_surface_region,
                    cm_query::PATH_WATER_BODY => &mut r.next_water_body,
                    _ => &mut r.next_land_mass,
                };
                let id = *next as u16;
                *next = next.wrapping_add(1);
                id
            };
            if terrain == TERRAIN_SPACE {
                let id = region(r, cm_query::PATH_SPACE_REGION);
                unsafe { cm_io::path_area(single).space_region = id };
            } else {
                let id = region(r, cm_query::PATH_NON_SPACE);
                unsafe { cm_io::path_area(single).surface_region = id };
            }
            if terrain == TERRAIN_DEEP_WATER {
                let id = region(r, cm_query::PATH_WATER_BODY);
                unsafe { cm_io::path_area(single).water_body = id };
            } else if kind != TILE_FOREST {
                let id = region(r, cm_query::PATH_LAND_MASS);
                unsafe { cm_io::path_area(single).land_mass = id };
                if (terrain == TERRAIN_DEEP_WATER || terrain == TERRAIN_SHALLOW_WATER)
                    && (kind == TILE_WATER || (kind > TILE_MAP_EDGE && kind <= TILE_UNIT_OBSTACLE))
                {
                    let id = region(r, cm_query::PATH_WATER_BODY);
                    unsafe { cm_io::path_area(single).water_body = id };
                }
            }
            tiles.push(tile);
            create(single, &tiles);
            let areas_table = &mut r.tables_mut().tables[EeAStarCmTable::PathAreas as usize];
            if id as usize >= areas_table.len() {
                areas_table.push(single);
            } else {
                areas_table[id as usize] = single;
            }
            new_id = id;
            r.tables_mut().add_area(serial, single);
            let c = cm_query::continent_at(r, x, y);
            if c != 0 {
                // SAFETY: a live continent
                unsafe { cm_io::continent(c).lists_mut().add_id(EeAStarContinentList::PathAreas, id) };
            }
        }
    }
    // the merged areas hand their sectors and sub sectors to the new id
    let mut sector_ids: Vec<u16> = Vec::new();
    for &old in merged.iter().rev() {
        // SAFETY: a live path area
        let ids = unsafe { cm_io::path_area(old).sector_ids().to_vec() };
        sector_ids.extend_from_slice(&ids);
        drop_area(r, old, new_id, &ids, true);
    }
    // the tile leaves its old area; an emptied area goes
    host(EE_ASTAR_CM_AREA_REMOVE_TILE, [area, 0], [x as i64, y as i64, 0, 0]);
    // SAFETY: a live area
    if unsafe { cm_io::area_tiles(area).set.size() } == 0 {
        let ids = unsafe { cm_io::path_area(area).sector_ids().to_vec() };
        drop_area(r, area, new_id, &ids, false);
    }
    // the new area joins the tile's sector and the merged areas' sectors
    let new_area = table(r, EeAStarCmTable::PathAreas, new_id as usize);
    if new_area == 0 {
        return;
    }
    let join = |r: &mut EeAStarCmRecord, s: u64| {
        // SAFETY: a live sector and path area
        unsafe {
            (*cm_io::sector(s).lists).add_path_area_id(new_id);
            let sid = cm_io::sector(s).id;
            cm_io::path_area(new_area).add_sector_id(sid);
        }
        let _ = r;
    };
    if let Some(s) = r.sector_index(x, y).map(|i| table(r, EeAStarCmTable::PathSectors, i)) {
        if s != 0 {
            join(r, s);
        }
    }
    sector_ids.sort_unstable();
    sector_ids.dedup();
    for &id in sector_ids.iter().rev() {
        let s = table(r, EeAStarCmTable::PathSectors, id as usize);
        if s != 0 {
            join(r, s);
        }
    }
}

/// `RepairPathArea`.
///
/// # Safety
/// `r` the manager's record; its objects live; the layout and host registered.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_repair_path_area(r: *mut EeAStarCmRecord, x: c_long, y: c_long) {
    repair_path_area(&mut *r, x, y);
}

/// `GetAdjacentAreaID`.
///
/// # Safety
/// As `ee_astar_cm_repair_path_area`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_adjacent_area_id(
    r: *const EeAStarCmRecord,
    kind: i32,
    x: c_long,
    y: c_long,
    area_kind: i32,
) -> u16 {
    adjacent_area_id(&*r, kind, x, y, area_kind)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn neighbours_are_the_cpps_order() {
        assert_eq!(NEIGHBOURS[0], (-1, -1));
        assert_eq!(NEIGHBOURS[7], (1, 1));
    }

    #[test]
    fn no_area_no_change() {
        let mut r: Box<std::mem::MaybeUninit<EeAStarCmRecord>> = Box::new(std::mem::MaybeUninit::uninit());
        // SAFETY: init writes the record whole; without a host no tile exists, so nothing is read or changed
        unsafe {
            crate::astar::cm::ee_astar_cm_init(r.as_mut_ptr());
            let r = &mut *r.as_mut_ptr();
            repair_path_area(r, 3, 4);
            assert_eq!(r.last_serial, 0);
            assert_eq!(adjacent_area_id(r, 1, 3, 4, 2), 0);
            r.free();
        }
    }
}
