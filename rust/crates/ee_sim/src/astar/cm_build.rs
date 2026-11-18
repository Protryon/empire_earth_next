//! The continent manager's build steps that store into its records (EEAIContinentManager.cpp): what
//! `CreateContinentGrid` keeps of `continent_grid`'s grid (the continent ids, the adjacency matrix, the neighbours,
//! the shore lists, each continent's shore tiles), `UpdateTileValues` over `tile_values` (the tile value grid and the
//! map tiles' AI values), and `EEAIRectangleArea::UpdateContinents` (a scenario area's tiles by continent category,
//! the continent with most of them). The map's tiles, their terrain, their wood and their AI values are the C++'s
//! (`cm_io`'s host).
//!
//! RUST_PLAN 2.1: the C++'s loops and their orders (the tile values from the last tile down; the rectangle's
//! columns, then its rows; the counts' `std::map` by id, the first most-counted id kept).

// `c_long`/`c_ulong` are 32 bits on Windows, 64 on LP64: their casts are needed on one or the other
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_long, c_ulong};

use crate::astar::cm::{EeAStarCmRecord, EeAStarCmShore, EeAStarCmTable};
use crate::astar::cm_io::{self, EeAStarCmIo};
use crate::astar::continent_grid::EePoint;
use crate::astar::ffi::{self, EeContinentGrid};
use crate::astar::hull::EeHullPoint;

/// tile (`i0`, `i1`)'s `GetResourceWeight` (10 per wood unit)
pub const EE_ASTAR_CM_RESOURCE_WEIGHT: i32 = 40;

const CATEGORY_LAND: u8 = 1;
const CATEGORY_WATER: u8 = 2;
const CATEGORY_SPACE: u8 = 4;

fn host(op: i32, i: [i64; 4]) -> i64 {
    let mut io = EeAStarCmIo { h: [0, 0], i };
    cm_io::call_host(op, &mut io)
}

fn table(r: &EeAStarCmRecord, t: EeAStarCmTable, index: usize) -> u64 {
    r.tables().tables[t as usize].get(index).copied().unwrap_or(0)
}

unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if n == 0 || p.is_null() {
        &[]
    } else {
        std::slice::from_raw_parts(p, n)
    }
}

fn hull_point(p: EePoint) -> EeHullPoint {
    EeHullPoint { x: p.x as c_long, y: p.y as c_long }
}

/// `CreateContinentGrid`'s store: the grid's continent ids and adjacency into the record's grids (made before, of
/// the map's tiles and the continent count squared), each neighbour pair onto its continent, the shore and ice shore
/// lists, each continent's shore tiles; the grid freed.
///
/// # Safety
/// `grid` from `ee_astar_continent_grid_build`; the continents live.
pub unsafe fn apply_continent_grid(r: &mut EeAStarCmRecord, grid: *mut EeContinentGrid) {
    let mut n = 0usize;
    let ids = slice(ffi::ee_astar_continent_grid_ids(grid, &mut n), n);
    if let Some(g) = r.tables_mut().continent_ids.as_mut() {
        for (k, &id) in ids.iter().enumerate() {
            if let Some(v) = g.get_mut(k) {
                *v = id;
            }
        }
    }
    let flags = slice(ffi::ee_astar_continent_grid_flags(grid, &mut n), n);
    if let Some(g) = r.tables_mut().tile_flags.as_mut() {
        for (k, &f) in flags.iter().enumerate() {
            if let Some(v) = g.get_mut(k) {
                *v = f != 0;
            }
        }
    }
    let neighbors = slice(ffi::ee_astar_continent_grid_neighbors(grid, &mut n), n);
    for pair in neighbors {
        let c = table(r, EeAStarCmTable::Continents, pair.id as usize);
        let other = table(r, EeAStarCmTable::Continents, pair.neighbor as usize);
        cm_io::continent(c).lists_mut().neighbors_mut().push(other);
    }
    let shore: Vec<EeHullPoint> =
        slice(ffi::ee_astar_continent_grid_shore(grid, &mut n), n).iter().map(|&p| hull_point(p)).collect();
    let ice: Vec<EeHullPoint> =
        slice(ffi::ee_astar_continent_grid_ice_shore(grid, &mut n), n).iter().map(|&p| hull_point(p)).collect();
    r.tables_mut().shore[EeAStarCmShore::Shore as usize] = shore;
    r.tables_mut().shore[EeAStarCmShore::IceShore as usize] = ice;
    let own = slice(ffi::ee_astar_continent_grid_continent_shore(grid, &mut n), n);
    for s in own {
        let c = table(r, EeAStarCmTable::Continents, s.id as usize);
        cm_io::continent(c).lists_mut().add_shore_tile(hull_point(s.tile));
    }
    ffi::ee_astar_continent_grid_free(grid);
}

/// `UpdateTileValues(water)`: every tile's terrain (and, for the land pass, its wood weight) to `tile_values`; the
/// non-zero values into the tile value grid and the map tiles' AI values; the ice shore list emptied.
pub fn update_tile_values(r: &mut EeAStarCmRecord, water: bool) {
    let size = r.width as i64;
    let count = (size.max(0) * size.max(0)) as usize;
    let mut terrain = vec![0i32; count];
    let mut weights = vec![0i32; if water { 0 } else { count }];
    for x in (0..size).rev() {
        for y in (0..size).rev() {
            let at = (x * size + y) as usize;
            terrain[at] = host(crate::astar::cm_repair::EE_ASTAR_CM_TERRAIN, [x, y, 0, 0]) as i32;
            if !water {
                weights[at] = host(EE_ASTAR_CM_RESOURCE_WEIGHT, [x, y, 0, 0]) as i32;
            }
        }
    }
    let points = |list: &[EeHullPoint]| -> Vec<EePoint> {
        list.iter().map(|p| EePoint { x: p.x as i32, y: p.y as i32 }).collect()
    };
    let shore = points(&r.tables().shore[EeAStarCmShore::Shore as usize]);
    let ice = points(&r.tables().shore[EeAStarCmShore::IceShore as usize]);
    let mut values = vec![0i32; count];
    let mut bytes = vec![0i8; count];
    // SAFETY: the buffers hold `count` entries each (the weights none for the water pass, which reads none)
    unsafe {
        ffi::ee_astar_update_tile_values(
            size as i32,
            terrain.as_ptr(),
            water,
            shore.as_ptr(),
            shore.len(),
            ice.as_ptr(),
            ice.len(),
            weights.as_ptr(),
            values.as_mut_ptr(),
            bytes.as_mut_ptr(),
        )
    };
    let width = r.width;
    for x in (0..size).rev() {
        for y in (0..size).rev() {
            let at = (x * size + y) as usize;
            if values[at] == 0 {
                continue;
            }
            let cell = width.wrapping_mul(x as c_ulong).wrapping_add(y as c_ulong) as usize;
            if let Some(g) = r.tables_mut().tile_values.as_mut() {
                if let Some(v) = g.get_mut(cell) {
                    *v = values[at] as c_long;
                }
            }
            host(cm_io::EE_ASTAR_CM_SET_AI_VALUE, [x, y, bytes[at] as i64, 0]);
        }
    }
    r.tables_mut().shore[EeAStarCmShore::IceShore as usize].clear();
}

/// `EEAIRectangleArea::UpdateContinents`: its tiles on the map counted by their continent's category, the
/// continent with most of them (the lowest id on a tie, 0 for none) stored and returned.
///
/// # Safety
/// `area` a live rectangle area; the continents live.
pub unsafe fn rect_update_continents(r: &EeAStarCmRecord, area: u64) -> u16 {
    let rec = cm_io::rect(area);
    rec.num_land_tiles = 0;
    rec.num_water_tiles = 0;
    rec.num_space_tiles = 0;
    let mut counts: std::collections::BTreeMap<u16, c_long> = std::collections::BTreeMap::new();
    let (left, right, top, bottom) = (rec.rect.left, rec.rect.right, rec.rect.top, rec.rect.bottom);
    let mut x = left;
    while x <= right {
        let mut y = top;
        while y <= bottom {
            if host(crate::astar::cm_repair::EE_ASTAR_CM_TERRAIN, [x as i64, y as i64, 0, 0]) >= 0 {
                let id = cm_io::continent_id_at(r, x, y);
                let c = if id != 0 { table(r, EeAStarCmTable::Continents, id as usize) } else { 0 };
                if c != 0 {
                    let rec = cm_io::rect(area);
                    match cm_io::continent(c).category {
                        CATEGORY_LAND => rec.num_land_tiles += 1,
                        CATEGORY_WATER => rec.num_water_tiles += 1,
                        CATEGORY_SPACE => rec.num_space_tiles += 1,
                        _ => {}
                    }
                    *counts.entry(cm_io::continent(c).id).or_insert(0) += 1;
                }
            }
            y += 1;
        }
        x += 1;
    }
    let mut best = 0u16;
    let mut most: c_long = 0;
    for (&id, &n) in &counts {
        if n > most {
            most = n;
            best = id;
        }
    }
    cm_io::rect(area).continent_id = best;
    best
}

// ---- the C interface

/// `CreateContinentGrid`'s store (the grid freed).
///
/// # Safety
/// As `apply_continent_grid`; `r` the manager's record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_apply_continent_grid(r: *mut EeAStarCmRecord, grid: *mut EeContinentGrid) {
    apply_continent_grid(&mut *r, grid);
}

/// `UpdateTileValues`.
///
/// # Safety
/// `r` the manager's record; the layout and host registered.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_update_tile_values(r: *mut EeAStarCmRecord, water: bool) {
    update_tile_values(&mut *r, water);
}

/// `EEAIRectangleArea::UpdateContinents`.
///
/// # Safety
/// As `rect_update_continents`; `r` the manager's record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_rect_update_continents(r: *const EeAStarCmRecord, area: u64) -> u16 {
    rect_update_continents(&*r, area)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tile_values_of_an_empty_map() {
        let mut r: Box<std::mem::MaybeUninit<EeAStarCmRecord>> = Box::new(std::mem::MaybeUninit::uninit());
        // SAFETY: init writes the record whole; a 0-tile map asks the host nothing
        unsafe {
            crate::astar::cm::ee_astar_cm_init(r.as_mut_ptr());
            let r = &mut *r.as_mut_ptr();
            r.tables_mut().shore[EeAStarCmShore::IceShore as usize].push(EeHullPoint { x: 1, y: 1 });
            update_tile_values(r, true);
            assert!(r.tables().shore[EeAStarCmShore::IceShore as usize].is_empty());
            r.free();
        }
    }
}
