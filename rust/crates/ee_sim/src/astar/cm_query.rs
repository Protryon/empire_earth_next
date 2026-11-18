//! The continent manager's queries over its records (EEAIContinentManager.cpp: the area lookups by tile, the
//! choke point lists, the connections, the largest and random areas, `InSameArea`, `GetContinent`) and its
//! `RemoveForestTile`, with `EEAIContinent::GetPathAreaAt` and `EEAIPathSector::GetPathAreaAt`. The C++ keeps the
//! CNTQ capture around each (the record of the outermost call) and the questions only the C++ can answer go through
//! `cm_io`'s host: an area's `Contains` (its hull test is captured), whether a map tile exists, `EEGetRandomInt`, a
//! forest's `RemoveTile` (a virtual that rebuilds its hull), the tiles' AI values, deleting a forest.
//!
//! RUST_PLAN 2.1: the C++'s loops (from the last element down where it walked down), its unchecked table reads as
//! checked ones that give none, the same host questions in the same order.

// `c_long`/`c_ulong` are 32 bits on Windows, 64 on LP64: their casts are needed on one or the other
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_long, c_void};

use crate::astar::cm::{EeAStarCmRecord, EeAStarCmTable};
use crate::astar::cm_io::{self, EeAStarCmIo};
use crate::astar::continent::EeAStarContinentList;
use crate::astar::hull::EeHullPoint;
use crate::astar::queries::{self, EeContinentSize};

/// `kContinentCategoryNone`, `Land`, `MapEdge`
pub const CATEGORY_NONE: u8 = 0;
pub const CATEGORY_LAND: u8 = 1;
pub const CATEGORY_MAP_EDGE: u8 = 5;
/// `kAIPathWaterBody`, `LandMass`, `SpaceRegion`, `NonSpace`
pub const PATH_WATER_BODY: c_long = 1;
pub const PATH_LAND_MASS: c_long = 2;
pub const PATH_SPACE_REGION: c_long = 3;
pub const PATH_NON_SPACE: c_long = 4;

/// tile (`i0`, `i1`) of the map exists (`gWorld.mMap && GetTileAtLocation`)
pub const EE_ASTAR_CM_HAS_TILE: i32 = 20;
/// `EEGetRandomInt(0, i0, *(URandom*)h0)`
pub const EE_ASTAR_CM_RANDOM: i32 = 21;
/// forest `h0`'s `RemoveTile((i0, i1))` (1/0)
pub const EE_ASTAR_CM_FOREST_REMOVE_TILE: i32 = 22;

fn host(op: i32, h: u64, i: [i64; 4]) -> i64 {
    let mut io = EeAStarCmIo { h: [h, 0], i };
    cm_io::call_host(op, &mut io)
}

fn at(r: &EeAStarCmRecord, t: EeAStarCmTable, index: usize) -> u64 {
    r.tables().tables[t as usize].get(index).copied().unwrap_or(0)
}

/// `GetContinentAt(x, y)`: the continent of the tile's id, 0 if none.
pub fn continent_at(r: &EeAStarCmRecord, x: c_long, y: c_long) -> u64 {
    match cm_io::continent_id_at(r, x, y) {
        0 => 0,
        id => at(r, EeAStarCmTable::Continents, id as usize),
    }
}

/// The first of `ids` (walked from the last) whose object in table `t` contains the tile.
fn first_containing(r: &EeAStarCmRecord, t: EeAStarCmTable, ids: &[usize], x: c_long, y: c_long) -> u64 {
    for &id in ids.iter().rev() {
        let h = at(r, t, id);
        if h != 0 && cm_io::contains(h, x, y) {
            return h;
        }
    }
    0
}

fn continent_list(c: u64, list: EeAStarContinentList) -> Vec<usize> {
    // SAFETY: a live continent
    unsafe { cm_io::continent(c).lists() }.map_or(Vec::new(), |l| l.ids(list).iter().map(|&v| v as usize).collect())
}

/// `GetChokePointAt(tile)`: a choke point of the tile's land continent that contains it.
pub fn choke_point_at(r: &EeAStarCmRecord, x: c_long, y: c_long) -> u64 {
    let c = continent_at(r, x, y);
    // SAFETY: a live continent
    if c == 0 || unsafe { cm_io::continent(c).category } != CATEGORY_LAND {
        return 0;
    }
    let ids: Vec<usize> = unsafe { cm_io::continent(c).lists() }
        .map_or(Vec::new(), |l| l.choke_points().iter().map(|&v| v as usize).collect());
    first_containing(r, EeAStarCmTable::ChokePoints, &ids, x, y)
}

/// `GetChokePoints(id)`: continent `id`'s choke points (from its last), or every one (from the last) for id 0.
pub fn choke_points(r: &EeAStarCmRecord, id: u16) -> Vec<u64> {
    let c = at(r, EeAStarCmTable::Continents, id as usize);
    if c != 0 {
        // SAFETY: a live continent
        let ids: Vec<c_long> = unsafe { cm_io::continent(c).lists() }.map_or(Vec::new(), |l| l.choke_points().to_vec());
        ids.iter().rev().map(|&i| at(r, EeAStarCmTable::ChokePoints, i as usize)).filter(|&h| h != 0).collect()
    } else if id == 0 {
        r.tables().tables[EeAStarCmTable::ChokePoints as usize].iter().rev().copied().filter(|&h| h != 0).collect()
    } else {
        Vec::new()
    }
}

/// `GetForestAt(tile)` / `GetHillAt(tile)`: the tile's continent's forest (hill) that contains it.
pub fn forest_at(r: &EeAStarCmRecord, x: c_long, y: c_long) -> u64 {
    match continent_at(r, x, y) {
        0 => 0,
        c => first_containing(r, EeAStarCmTable::Forests, &continent_list(c, EeAStarContinentList::Forests), x, y),
    }
}

pub fn hill_at(r: &EeAStarCmRecord, x: c_long, y: c_long) -> u64 {
    match continent_at(r, x, y) {
        0 => 0,
        c => first_containing(r, EeAStarCmTable::Hills, &continent_list(c, EeAStarContinentList::Hills), x, y),
    }
}

/// `EEAIContinent::GetPathAreaAt`: its path areas (from the last) holding the tile.
pub fn continent_path_area_at(r: &EeAStarCmRecord, c: u64, x: c_long, y: c_long) -> u64 {
    first_containing(r, EeAStarCmTable::PathAreas, &continent_list(c, EeAStarContinentList::PathAreas), x, y)
}

/// `EEAIPathSector::GetPathAreaAt`: the sector's path areas (from the last) holding the tile.
pub fn sector_path_area_at(r: &EeAStarCmRecord, s: u64, x: c_long, y: c_long) -> u64 {
    // SAFETY: a live sector
    let ids: Vec<usize> = unsafe { cm_io::sector(s).lists.as_ref() }
        .map_or(Vec::new(), |l| l.path_area_ids.iter().map(|&v| v as usize).collect());
    first_containing(r, EeAStarCmTable::PathAreas, &ids, x, y)
}

/// `GetPathAreaAt(tile)`: its sub sector's area when that is current, else found through the path sector
/// (through the continent while repairing); 0 off the map.
pub fn path_area_at(r: &EeAStarCmRecord, x: c_long, y: c_long) -> u64 {
    if host(EE_ASTAR_CM_HAS_TILE, 0, [x as i64, y as i64, 0, 0]) == 0 {
        return 0;
    }
    let sub = match cm_io::sub_sector_id_at(r, x, y) {
        0 => 0,
        id => at(r, EeAStarCmTable::PathSubSectors, id as usize),
    };
    if sub != 0 {
        // SAFETY: a live sub sector
        let s = unsafe { cm_io::sub(sub) };
        if !s.is_stale {
            let a = at(r, EeAStarCmTable::PathAreas, s.path_area_id as usize);
            if a != 0 {
                return a;
            }
        }
    }
    if r.state != 2 {
        match r.sector_index(x, y).map(|i| at(r, EeAStarCmTable::PathSectors, i)) {
            Some(s) if s != 0 => sector_path_area_at(r, s, x, y),
            _ => 0,
        }
    } else {
        match continent_at(r, x, y) {
            0 => 0,
            c => continent_path_area_at(r, c, x, y),
        }
    }
}

/// `GetContinent(tile, kind)`: the region id of that kind of the tile's path area, 0 if none.
pub fn region_id_at(r: &EeAStarCmRecord, x: c_long, y: c_long, kind: c_long) -> u16 {
    let a = path_area_at(r, x, y);
    if a == 0 {
        return 0;
    }
    // SAFETY: a live path area
    let p = unsafe { cm_io::path_area(a) };
    match kind {
        PATH_WATER_BODY => p.water_body,
        PATH_LAND_MASS => p.land_mass,
        PATH_SPACE_REGION => p.space_region,
        PATH_NON_SPACE => p.surface_region,
        _ => 0,
    }
}

/// `InSameArea`: every tile with the first one's region id of that kind (true for none).
pub fn in_same_area(r: &EeAStarCmRecord, tiles: &[EeHullPoint], kind: c_long) -> bool {
    let mut first = 0u16;
    for t in tiles {
        let id = region_id_at(r, t.x, t.y, kind);
        if first == 0 {
            first = id;
        }
        if id != first {
            return false;
        }
    }
    true
}

/// `AreConnected`: the same continent, or a non-zero equal chain number of the chain the flags need.
pub fn are_connected(first: u64, second: u64, flags: i32) -> bool {
    if first == second && first != 0 {
        return true;
    }
    if first == 0 || second == 0 {
        return false;
    }
    let chain = queries::connected_chain(flags);
    if chain < 0 {
        return false;
    }
    // SAFETY: live continents
    let (a, b) =
        unsafe { (cm_io::continent(first).counts[chain as usize], cm_io::continent(second).counts[chain as usize]) };
    a != 0 && a == b
}

/// `GetLargestContinent(kind)`: the id of the largest continent of that category, 0 if none.
pub fn largest_continent(r: &EeAStarCmRecord, kind: i8) -> u16 {
    let continents = &r.tables().tables[EeAStarCmTable::Continents as usize];
    let sizes: Vec<EeContinentSize> = continents
        .iter()
        .map(|&c| {
            if c == 0 {
                EeContinentSize::default()
            } else {
                // SAFETY: a live continent
                unsafe {
                    EeContinentSize {
                        present: true,
                        category: cm_io::continent(c).category,
                        size: cm_io::area_tiles(c).set.size(),
                    }
                }
            }
        })
        .collect();
    match queries::largest_continent(&sizes, kind) {
        -1 => 0,
        // SAFETY: a live continent
        best => unsafe { cm_io::continent(continents[best as usize]).id },
    }
}

fn random_of(list: &[u64], random: u64) -> u64 {
    if list.is_empty() {
        return 0;
    }
    let k = host(EE_ASTAR_CM_RANDOM, random, [list.len() as i64 - 1, 0, 0, 0]);
    list.get(k as usize).copied().unwrap_or(0)
}

/// `GetRandomArea(kind, random, continent)`: one of the serial table's areas of that type whose hull center is on
/// that continent (any for 0).
pub fn random_area(r: &EeAStarCmRecord, kind: c_long, random: u64, continent: u16) -> u64 {
    let mut list = Vec::new();
    for a in &r.tables().areas {
        if a.area == 0 {
            continue;
        }
        // SAFETY: a live area
        let rec = unsafe { cm_io::area(a.area) };
        if rec.area_type == kind
            && (continent == 0 || cm_io::continent_id_at(r, rec.hull_center.x, rec.hull_center.y) == continent)
        {
            list.push(a.area);
        }
    }
    random_of(&list, random)
}

/// `GetRandomContinent(kind, random)`: one of the continents (from the last) of that category (any but the map
/// edge for none).
pub fn random_continent(r: &EeAStarCmRecord, kind: u8, random: u64) -> u64 {
    let mut list = Vec::new();
    for &c in r.tables().tables[EeAStarCmTable::Continents as usize].iter().rev() {
        if c == 0 {
            continue;
        }
        // SAFETY: a live continent
        let category = unsafe { cm_io::continent(c).category };
        if (kind == CATEGORY_NONE && category != CATEGORY_MAP_EDGE) || category == kind {
            list.push(c);
        }
    }
    random_of(&list, random)
}

/// `RemoveForestTile`'s change once the tile has no wood left (`empty`): the tile leaves its forest, the AI values
/// of its 3x3 block drop by 10 (not below 0), an emptied forest goes (its continent's list, the table, the serial
/// table, the object).
pub fn remove_forest_tile(r: &mut EeAStarCmRecord, x: c_long, y: c_long, empty: bool) {
    let f = forest_at(r, x, y);
    if f == 0 || !empty {
        return;
    }
    host(EE_ASTAR_CM_FOREST_REMOVE_TILE, f, [x as i64, y as i64, 0, 0]);
    for (dx, dy) in [(0, 0), (-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)] {
        let (tx, ty) = (x as i64 + dx, y as i64 + dy);
        let v = host(cm_io::EE_ASTAR_CM_AI_VALUE, 0, [tx, ty, 0, 0]);
        if v > 0 {
            host(cm_io::EE_ASTAR_CM_SET_AI_VALUE, 0, [tx, ty, (v - 10).max(0), 0]);
        }
    }
    // SAFETY: a live forest
    let (tiles, continent_id, id, serial) = unsafe {
        (cm_io::area_tiles(f).set.size(), cm_io::forest(f).continent_id, cm_io::forest(f).id, cm_io::area(f).serial)
    };
    let c = at(r, EeAStarCmTable::Continents, continent_id as usize);
    if tiles >= 1 || c == 0 {
        return;
    }
    // SAFETY: a live continent
    unsafe { cm_io::continent(c).lists_mut().remove_id(EeAStarContinentList::Forests, id) };
    if let Some(slot) = r.tables_mut().tables[EeAStarCmTable::Forests as usize].get_mut(id as usize) {
        *slot = 0;
    }
    r.tables_mut().remove_area(serial);
    host(cm_io::EE_ASTAR_CM_DELETE, f, [EeAStarCmTable::Forests as i64, 0, 0, 0]);
}

// ---- the C interface

/// The tile lookups: `kind` 0 choke point, 1 forest, 2 hill, 3 path area, 4 continent (`GetContinentAt`).
///
/// # Safety
/// `r` the manager's record; its objects live.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_area_at(r: *const EeAStarCmRecord, kind: i32, x: c_long, y: c_long) -> u64 {
    let r = &*r;
    match kind {
        0 => choke_point_at(r, x, y),
        1 => forest_at(r, x, y),
        2 => hill_at(r, x, y),
        3 => path_area_at(r, x, y),
        _ => continent_at(r, x, y),
    }
}

/// `GetContinent(tile, kind)`.
///
/// # Safety
/// As `ee_astar_cm_area_at`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_region_id_at(
    r: *const EeAStarCmRecord,
    x: c_long,
    y: c_long,
    kind: c_long,
) -> u16 {
    region_id_at(&*r, x, y, kind)
}

/// `EEAIContinent::GetPathAreaAt` (`object` a continent) or `EEAIPathSector::GetPathAreaAt` (a sector).
///
/// # Safety
/// As `ee_astar_cm_area_at`; `object` a live continent or sector.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_object_path_area_at(
    r: *const EeAStarCmRecord,
    object: u64,
    sector: bool,
    x: c_long,
    y: c_long,
) -> u64 {
    if sector {
        sector_path_area_at(&*r, object, x, y)
    } else {
        continent_path_area_at(&*r, object, x, y)
    }
}

/// `InSameArea`.
///
/// # Safety
/// As `ee_astar_cm_area_at`; `tiles` holds `count` points.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_in_same_area(
    r: *const EeAStarCmRecord,
    tiles: *const EeHullPoint,
    count: usize,
    kind: c_long,
) -> bool {
    let tiles = if count == 0 { &[][..] } else { std::slice::from_raw_parts(tiles, count) };
    in_same_area(&*r, tiles, kind)
}

/// `AreConnected` (the continents' addresses).
///
/// # Safety
/// `first`, `second` live continents or 0.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_are_connected(first: u64, second: u64, flags: i32) -> bool {
    are_connected(first, second, flags)
}

/// `GetLargestContinent`.
///
/// # Safety
/// As `ee_astar_cm_area_at`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_largest_continent(r: *const EeAStarCmRecord, kind: i8) -> u16 {
    largest_continent(&*r, kind)
}

/// `GetRandomArea` (`continent` true: `GetRandomContinent(kind)`, else of area type `kind` on `on_continent`).
///
/// # Safety
/// As `ee_astar_cm_area_at`; `random` a live `URandom*`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_random_area(
    r: *const EeAStarCmRecord,
    continent: bool,
    kind: c_long,
    random: *mut c_void,
    on_continent: u16,
) -> u64 {
    if continent {
        random_continent(&*r, kind as u8, random as u64)
    } else {
        random_area(&*r, kind, random as u64, on_continent)
    }
}

/// `GetChokePoints(id)` into `out` (room for `capacity`); their count (more than `capacity`: call again).
///
/// # Safety
/// As `ee_astar_cm_area_at`; `out` holds `capacity` handles.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_choke_points(
    r: *const EeAStarCmRecord,
    id: u16,
    out: *mut u64,
    capacity: usize,
) -> usize {
    let list = choke_points(&*r, id);
    if list.len() <= capacity && !list.is_empty() {
        std::ptr::copy_nonoverlapping(list.as_ptr(), out, list.len());
    }
    list.len()
}

/// `RemoveForestTile` at tile (x, y), `empty` whether no wood with resources is left on it.
///
/// # Safety
/// As `ee_astar_cm_area_at`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_remove_forest_tile(r: *mut EeAStarCmRecord, x: c_long, y: c_long, empty: bool) {
    remove_forest_tile(&mut *r, x, y, empty);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn connections() {
        assert!(!are_connected(0, 0, 1));
        assert!(are_connected(7, 7, 0));
        assert!(!are_connected(7, 0, 1));
    }

    #[test]
    fn the_same_area_of_no_tiles() {
        let mut r: Box<std::mem::MaybeUninit<EeAStarCmRecord>> = Box::new(std::mem::MaybeUninit::uninit());
        // SAFETY: init writes the record whole; no tiles: no lookup runs
        unsafe {
            crate::astar::cm::ee_astar_cm_init(r.as_mut_ptr());
            let r = &mut *r.as_mut_ptr();
            assert!(in_same_area(r, &[], PATH_LAND_MASS));
            assert!(choke_points(r, 3).is_empty());
            assert_eq!(largest_continent(r, 0), 0);
            r.free();
        }
    }
}
