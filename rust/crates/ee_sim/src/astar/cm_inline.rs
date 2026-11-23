//! The continent manager's inline lookups the C++ header wrote out (ee/EEAIContinentManager.h: the tile and unit
//! overloads of GetContinentID, GetContinentAt, GetPathSector, GetPathSubSectorID, GetPathSubSectorAt, the area
//! lookups by a unit, CanGetToUnit) and the list fillers of its members that hand back vectors (the adjacent ids,
//! the choke points, the tiles next to an area): each member is now one call of these. A unit is its handle, read
//! through the world's views (its tile); none of these writes a capture record of its own (the CNTQ records are
//! the queries' they call, in the order the C++ called them).

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::ffi::{c_long, c_void};

use crate::astar::cm::{EeAStarCmRecord, EeAStarCmTable};
use crate::astar::cm_io;
use crate::astar::cm_queries;
use crate::astar::continent_grid::EePoint;

/// The handle at `id` of table `t` (0 past its end).
fn table_at(r: &EeAStarCmRecord, t: EeAStarCmTable, id: usize) -> u64 {
    r.tables().tables[t as usize].get(id).copied().unwrap_or(0)
}

/// The continent of id `id` (`GetContinentAt`'s second half): 0 for id 0 or past the table.
pub fn continent_of_id(r: &EeAStarCmRecord, id: u16) -> u64 {
    if id == 0 {
        0
    } else {
        table_at(r, EeAStarCmTable::Continents, id as usize)
    }
}

/// The path sector at tile (x, y) (`GetPathSector(tile)`): 0 off the map.
pub fn sector_at(r: &EeAStarCmRecord, x: c_long, y: c_long) -> u64 {
    r.sector_index(x, y).map_or(0, |i| table_at(r, EeAStarCmTable::PathSectors, i))
}

/// The sub sector of id `id` (`GetPathSubSectorAt`'s second half): 0 for id 0.
pub fn sub_sector_of_id(r: &EeAStarCmRecord, id: u16) -> u64 {
    if id == 0 {
        0
    } else {
        table_at(r, EeAStarCmTable::PathSubSectors, id as usize)
    }
}

unsafe fn rec<'a>(r: *const EeAStarCmRecord) -> &'a EeAStarCmRecord {
    &*r
}

/// A unit's tile (the C++ `ee_astar_unit_tile`).
fn unit_tile(unit: u64) -> (c_long, c_long) {
    // SAFETY: the callers hand 0 or a live unit
    let t = unsafe { cm_queries::ee_astar_unit_tile(unit) };
    (t.x as c_long, t.y as c_long)
}

/// `GetContinentID(x, y)`: the grid's id, `FindContinentID` without the grid, 0 off the map.
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_continent_id_at(r: *const EeAStarCmRecord, x: c_long, y: c_long) -> u16 {
    cm_io::continent_id_at(rec(r), x, y)
}

/// `GetContinentID(unit)`: the id at the unit's tile (0 off the grid).
///
/// # Safety
/// `r` a live record; `unit` 0 or a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_unit_continent_id(r: *const EeAStarCmRecord, unit: u64) -> u16 {
    let (x, y) = unit_tile(unit);
    cm_io::continent_id_at(rec(r), x, y)
}

/// `GetContinentAt(x, y)`: the continent at the tile (0 if none).
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_continent_at(r: *const EeAStarCmRecord, x: c_long, y: c_long) -> u64 {
    let r = rec(r);
    continent_of_id(r, cm_io::continent_id_at(r, x, y))
}

/// `GetContinentAt(unit)`: the continent at the unit's tile (0 off the grid or none).
///
/// # Safety
/// `r` a live record; `unit` 0 or a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_unit_continent_at(r: *const EeAStarCmRecord, unit: u64) -> u64 {
    let (x, y) = unit_tile(unit);
    ee_astar_cm_continent_at(r, x, y)
}

/// `GetContinent(unit, kind)`: the region id of kind `kind` at the unit's tile (with its CNTQ record).
///
/// # Safety
/// `r` a live record; `unit` 0 or a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_unit_region_id(r: *const EeAStarCmRecord, unit: u64, kind: c_long) -> u16 {
    let (x, y) = unit_tile(unit);
    cm_queries::ee_astar_cmq_region_id(r, x, y, kind)
}

/// The area lookups by a unit (`GetChokePointAt`, `GetForestAt`, `GetPathAreaAt` of a unit: kinds 0, 1, 3 of
/// `ee_astar_cmq_area_at`) at its tile.
///
/// # Safety
/// `r` a live record; `unit` 0 or a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_unit_area_at(r: *const EeAStarCmRecord, kind: i32, unit: u64) -> u64 {
    let (x, y) = unit_tile(unit);
    cm_queries::ee_astar_cmq_area_at(r, kind, x, y)
}

/// `GetPathSector(tile)`: 0 off the map.
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_sector_at(r: *const EeAStarCmRecord, x: c_long, y: c_long) -> u64 {
    sector_at(rec(r), x, y)
}

/// `GetPathSector(unit)`: the sector at the unit's tile.
///
/// # Safety
/// `r` a live record; `unit` 0 or a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_unit_sector_at(r: *const EeAStarCmRecord, unit: u64) -> u64 {
    let (x, y) = unit_tile(unit);
    sector_at(rec(r), x, y)
}

/// `GetPathSubSectorID(x, y)`: the grid's id, `FindPathSubSectorID` without it, 0 off the map.
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_sub_sector_id_at(r: *const EeAStarCmRecord, x: c_long, y: c_long) -> u16 {
    cm_io::sub_sector_id_at(rec(r), x, y)
}

/// `GetPathSubSectorID(unit)`: the id at the unit's tile (0 off the grid).
///
/// # Safety
/// `r` a live record; `unit` 0 or a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_unit_sub_sector_id(r: *const EeAStarCmRecord, unit: u64) -> u16 {
    let (x, y) = unit_tile(unit);
    cm_io::sub_sector_id_at(rec(r), x, y)
}

/// `GetPathSubSectorAt(x, y)`: the sub sector at the tile (0 if none).
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_sub_sector_at(r: *const EeAStarCmRecord, x: c_long, y: c_long) -> u64 {
    let r = rec(r);
    sub_sector_of_id(r, cm_io::sub_sector_id_at(r, x, y))
}

/// `CanGetToUnit(unit, target)`: `CanGetTo(unit, the target's tile)`.
///
/// # Safety
/// `unit`, `target` live units.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_can_get_to_unit(unit: u64, target: u64) -> bool {
    let (x, y) = unit_tile(target);
    crate::astar::cm_unit::ee_astar_cm_can_get_to(unit, x, y)
}

/// `AdjacentIDs(out, id, with_self)`: `out` resized to the matrix's side (`resize(list, n)`, `data(list)` its
/// ids), filled, then cut to the ids found.
///
/// # Safety
/// `r` a live record; `resize` resizes the C++ list `list`, `data` answers its ids.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_adjacent_ids_into(
    r: *const EeAStarCmRecord,
    id: u16,
    with_self: bool,
    list: *mut c_void,
    resize: extern "C" fn(*mut c_void, usize),
    data: extern "C" fn(*mut c_void) -> *mut u16,
) {
    let side = rec(r).flags_width.max(0) as usize;
    resize(list, side);
    let out = if side == 0 { std::ptr::null_mut() } else { data(list) };
    let n = cm_queries::ee_astar_cmq_adjacent_ids(r, id, with_self, out);
    resize(list, n);
}

/// `GetChokePoints(out, id, clear)`: `clear(list)` first when `clear`, then the continent's choke points appended
/// (`ee_astar_cmq_choke_points` over the list as it is then).
///
/// # Safety
/// `r` a live record; `before`, `count_before` the list's entries after the clear; the callbacks the C++ list's.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_astar_cmq_choke_points_into(
    r: *const EeAStarCmRecord,
    id: u16,
    clear: bool,
    before: *const u64,
    count_before: usize,
    list: *mut c_void,
    clear_list: extern "C" fn(*mut c_void),
    push: extern "C" fn(*mut c_void, u64),
) {
    let (before, count_before) = if clear {
        clear_list(list);
        (std::ptr::null(), 0)
    } else {
        (before, count_before)
    };
    cm_queries::ee_astar_cmq_choke_points(r, id, clear, before, count_before, list, push);
}

/// `GetTilesNextTo(x, y, out, id)`: the answer, `out` cleared and given the tiles (at most eight).
///
/// # Safety
/// `r` a live record; the callbacks the C++ list's.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cmq_tiles_next_to_into(
    r: *const EeAStarCmRecord,
    x: c_long,
    y: c_long,
    id: u16,
    list: *mut c_void,
    clear_list: extern "C" fn(*mut c_void),
    push: extern "C" fn(*mut c_void, c_long, c_long),
) -> bool {
    let mut tiles = [EePoint::default(); 8];
    let mut n = 0usize;
    let result = cm_queries::ee_astar_cmq_tiles_next_to(r, x, y, id, tiles.as_mut_ptr(), &mut n);
    clear_list(list);
    for t in &tiles[..n] {
        push(list, t.x as c_long, t.y as c_long);
    }
    result
}

/// `EEAIContinent::GetShoreInRegions(ids, out, kind)`: `clear(list)`, then the tiles found pushed.
///
/// # Safety
/// `h` a live continent; `ids` holds `n` ids; the callbacks the C++ list's.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_shore_in_regions_into(
    h: u64,
    ids: *const u16,
    n: usize,
    kind: c_long,
    list: *mut c_void,
    clear_list: extern "C" fn(*mut c_void),
    push: extern "C" fn(*mut c_void, c_long, c_long),
) {
    clear_list(list);
    crate::astar::area_shore::ee_astar_continent_shore_in_regions(h, ids, n, kind, list, push);
}

/// `EEWriteAreaBytes(area, out)`: the area's save bytes handed to `assign(list, data, len)`.
///
/// # Safety
/// `h` a live area; `assign` replaces the C++ list's contents.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_area_bytes_into(
    h: u64,
    list: *mut c_void,
    assign: extern "C" fn(*mut c_void, *const u8, usize),
) {
    let b = cm_io::ee_astar_cm_area_bytes(h);
    let mut n = 0usize;
    let data = cm_io::ee_astar_cm_bytes_data(b, &mut n);
    assign(list, data, n);
    cm_io::ee_astar_cm_bytes_free(b);
}

/// `BuildRegions(map)` with the scenario manager's areas (`scenario::entry::region_inputs`, read first as the
/// C++ read them) and the region size and the continent grid's keeping the C++ computed.
///
/// # Safety
/// As `ee_astar_cm_build_regions`.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_build_regions_scenario(
    r: *mut EeAStarCmRecord,
    map: u64,
    region: c_long,
    keep_continent_ids: bool,
) {
    let (inputs, ids) = crate::scenario::entry::region_inputs();
    crate::astar::cm_passes::ee_astar_cm_build_regions(
        r,
        map,
        region,
        if inputs.is_empty() { std::ptr::null() } else { inputs.as_ptr() },
        if ids.is_empty() { std::ptr::null() } else { ids.as_ptr() },
        inputs.len(),
        keep_continent_ids,
    );
}

/// `EEAIRectangleArea::GetRandomPoint`'s choice when the area's random tile is off the map: the area's tiles
/// (`points`, BuildPointList's order) that are on the map, walked from the back, shuffled by `EEAIRandomShuffle`
/// (`random(min, max)` the physics' draws), the first of them; none when no tile is on the map.
pub fn rect_on_map_point(
    points: &[EePoint],
    on_map: impl Fn(EePoint) -> bool,
    random: impl FnMut(i32, i32) -> i32,
) -> Option<EePoint> {
    let mut tiles: Vec<EePoint> = points.iter().rev().copied().filter(|&p| on_map(p)).collect();
    crate::units::uu_direct::ai_random_shuffle(&mut tiles, random);
    tiles.first().copied()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    thread_local! {
        static LIST: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
        static IDS: RefCell<Vec<u16>> = const { RefCell::new(Vec::new()) };
    }

    extern "C" fn clear_list(_: *mut c_void) {
        LIST.with(|l| l.borrow_mut().push("clear".into()));
    }
    extern "C" fn push_choke(_: *mut c_void, c: u64) {
        LIST.with(|l| l.borrow_mut().push(format!("choke {c}")));
    }
    extern "C" fn resize(_: *mut c_void, n: usize) {
        IDS.with(|v| v.borrow_mut().resize(n, 0));
    }
    extern "C" fn data(_: *mut c_void) -> *mut u16 {
        IDS.with(|v| v.borrow_mut().as_mut_ptr())
    }

    fn record() -> EeAStarCmRecord {
        let mut r = std::mem::MaybeUninit::<EeAStarCmRecord>::uninit();
        // SAFETY: ee_astar_cm_init writes the whole record
        unsafe {
            crate::astar::cm::ee_astar_cm_init(r.as_mut_ptr());
            r.assume_init()
        }
    }

    #[test]
    fn the_rectangles_point_is_a_shuffled_on_map_tile() {
        let p = |x, y| EePoint { x, y };
        let points = [p(1, 1), p(2, 1), p(3, 1), p(4, 1)];
        // the on-map tiles from the back: (4,1), (2,1), (1,1); draws (0, 1) -> 0, (0, 2) -> 2
        let mut draws = vec![];
        let got = rect_on_map_point(
            &points,
            |q| q.x != 3,
            |a, b| {
                draws.push((a, b));
                if b == 2 {
                    2
                } else {
                    0
                }
            },
        );
        assert_eq!(draws, vec![(0, 1), (0, 2)]);
        // [(4,1),(2,1),(1,1)] -> swap(1,0) -> [(2,1),(4,1),(1,1)] -> swap(2,2) -> first (2,1)
        assert_eq!(got, Some(p(2, 1)));
        assert_eq!(rect_on_map_point(&points, |_| false, |_, _| 0), None);
    }

    #[test]
    fn lookups_of_an_empty_manager_are_none() {
        let mut r = record();
        // SAFETY: the live record
        unsafe {
            assert_eq!(ee_astar_cm_continent_id_at(&r, 3, 4), 0);
            assert_eq!(ee_astar_cm_continent_at(&r, -1, 4), 0);
            assert_eq!(ee_astar_cm_sector_at(&r, 0, 0), 0);
            assert_eq!(ee_astar_cm_sub_sector_id_at(&r, 0, 0), 0);
            assert_eq!(ee_astar_cm_sub_sector_at(&r, 0, 0), 0);
            crate::astar::cm::ee_astar_cm_free(&mut r);
        }
    }

    #[test]
    fn ids_of_a_table_entry() {
        let mut r = record();
        // SAFETY: the live record
        unsafe {
            crate::astar::cm::ee_astar_cm_table_resize(&mut r, EeAStarCmTable::Continents, 3);
            crate::astar::cm::ee_astar_cm_table_set(&mut r, EeAStarCmTable::Continents, 2, 0x50);
        }
        assert_eq!(continent_of_id(&r, 2), 0x50);
        assert_eq!(continent_of_id(&r, 0), 0);
        assert_eq!(continent_of_id(&r, 7), 0);
        assert_eq!(sub_sector_of_id(&r, 0), 0);
        // SAFETY: as above
        unsafe { crate::astar::cm::ee_astar_cm_free(&mut r) };
    }

    #[test]
    fn adjacent_ids_resize_to_the_side_then_to_the_count() {
        let mut r = record();
        // SAFETY: the live record; no matrix: side 0, no ids
        unsafe {
            ee_astar_cmq_adjacent_ids_into(&r, 1, true, std::ptr::null_mut(), resize, data);
            crate::astar::cm::ee_astar_cm_free(&mut r);
        }
        IDS.with(|v| assert!(v.borrow().is_empty()));
    }

    #[test]
    fn choke_points_clear_first_when_asked() {
        let mut r = record();
        LIST.with(|l| l.borrow_mut().clear());
        // SAFETY: the live record; no continent 5: nothing appended
        unsafe {
            ee_astar_cmq_choke_points_into(
                &r,
                5,
                true,
                std::ptr::null(),
                0,
                std::ptr::null_mut(),
                clear_list,
                push_choke,
            );
            ee_astar_cmq_choke_points_into(
                &r,
                5,
                false,
                std::ptr::null(),
                0,
                std::ptr::null_mut(),
                clear_list,
                push_choke,
            );
            crate::astar::cm::ee_astar_cm_free(&mut r);
        }
        LIST.with(|l| assert_eq!(*l.borrow(), vec!["clear".to_string()]));
    }
}
