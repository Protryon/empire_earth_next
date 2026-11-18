//! The sub sectors' links and neighbours over their records (EEAIContinentManager.cpp): `LinkSubSectors` for each
//! call a link walk lists (`ApplySubSectorLinks`: `sub_sector_links`' walk over the whole grid or a grown sector),
//! and `RemoveSubSectors` (a sector's sub sectors unlinked from their neighbours, out of the table and the id grid,
//! deleted through the host, their ids freed).
//!
//! RUST_PLAN 2.1: the C++'s orders (the walk's calls in order, the neighbours from the last, `std::find` from the
//! front, the multimap's inserts after the equal keys).

// `c_long`/`c_ulong` are 32 bits on Windows, 64 on LP64: their casts are needed on one or the other
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_ulong;

use crate::astar::cm::{EeAStarCmFree, EeAStarCmRecord, EeAStarCmTable};
use crate::astar::cm_io::{self, EeAStarCmIo};
use crate::astar::ffi::{ee_astar_sub_sector_links_free, ee_astar_sub_sector_links_get, EeSubSectorLinks};
use crate::astar::sector_rec::EeAStarSubSectorLink;
use crate::astar::sparse::PointList;

fn table(r: &EeAStarCmRecord, t: EeAStarCmTable, index: usize) -> u64 {
    r.tables().tables[t as usize].get(index).copied().unwrap_or(0)
}

/// `IsNeighbor`.
unsafe fn is_neighbor(sub: u64, other: u64) -> bool {
    (*cm_io::sub(sub).lists).neighbors.contains(&other)
}

/// `LinkSubSectors(sub, other id, add, (first, second))`.
///
/// # Safety
/// `sub` a live sub sector; the table's sub sectors live.
pub unsafe fn link_sub_sectors(r: &EeAStarCmRecord, sub: u64, other_id: u16, add: bool, first: u64, second: u64) {
    let other = table(r, EeAStarCmTable::PathSubSectors, other_id as usize);
    if other == 0 {
        return;
    }
    let sub_id = cm_io::sub(sub).id;
    if !add {
        if !(*cm_io::sub(sub).lists).links_of(other_id).is_empty() {
            (*cm_io::sub(sub).lists).remove_links(other_id);
            (*cm_io::sub(other).lists).remove_links(sub_id);
        }
    } else {
        if !is_neighbor(sub, other) {
            (*cm_io::sub(sub).lists).insert_link(EeAStarSubSectorLink { id: other_id, first, second });
        }
        if !is_neighbor(other, sub) {
            (*cm_io::sub(other).lists).insert_link(EeAStarSubSectorLink { id: sub_id, first, second });
        }
    }
    if !is_neighbor(sub, other) {
        (*cm_io::sub(sub).lists).neighbors.push(other);
    }
    if !is_neighbor(other, sub) {
        (*cm_io::sub(other).lists).neighbors.push(sub);
    }
}

/// `ApplySubSectorLinks`: the walk's calls in order (the links freed after).
///
/// # Safety
/// `links` from `ee_astar_sub_sector_links`; the table's sub sectors live.
pub unsafe fn apply_links(r: &EeAStarCmRecord, links: *mut EeSubSectorLinks) {
    let mut count = 0usize;
    let data = ee_astar_sub_sector_links_get(links, &mut count);
    let list = if count == 0 { &[][..] } else { std::slice::from_raw_parts(data, count) };
    for l in list {
        let first = if l.first != 0 { table(r, EeAStarCmTable::PathSubSectors, l.first as usize) } else { 0 };
        let second = if l.second != 0 { table(r, EeAStarCmTable::PathSubSectors, l.second as usize) } else { 0 };
        let sub = table(r, EeAStarCmTable::PathSubSectors, l.sub as usize);
        link_sub_sectors(r, sub, l.other, l.add, first, second);
    }
    ee_astar_sub_sector_links_free(links);
}

/// `RemoveSubSectors(sector)`.
///
/// # Safety
/// `sector` a live sector; its sub sectors and their neighbours live.
pub unsafe fn remove_sub_sectors(r: &mut EeAStarCmRecord, sector: u64) {
    let subs = (*cm_io::sector(sector).lists).sub_sectors.clone();
    let mut ids: Vec<u16> = Vec::with_capacity(subs.len());
    let mut tiles = Vec::new();
    for entry in &subs {
        let s = entry.sub;
        let id = cm_io::sub(s).id;
        let neighbors = (*cm_io::sub(s).lists).neighbors.clone();
        for &n in neighbors.iter().rev() {
            (*cm_io::sub(n).lists).remove_neighbor(s, id);
        }
        if let Some(slot) = r.tables_mut().tables[EeAStarCmTable::PathSubSectors as usize].get_mut(id as usize) {
            *slot = 0;
        }
        cm_io::sub_tiles(s).set.points(PointList::All(1), &mut tiles);
        let width = r.width;
        if let Some(g) = r.tables_mut().sub_sector_ids.as_mut() {
            for p in tiles.iter().rev() {
                let at = width.wrapping_mul(p.x as c_ulong).wrapping_add(p.y as c_ulong) as usize;
                if let Some(v) = g.get_mut(at) {
                    *v = 0;
                }
            }
        }
        ids.push(id);
    }
    // `DeleteSubSectors`: the objects, then the map
    for entry in &subs {
        let mut io = EeAStarCmIo { h: [entry.sub, 0], i: [EeAStarCmTable::PathSubSectors as i64, 0, 0, 0] };
        cm_io::call_host(cm_io::EE_ASTAR_CM_DELETE, &mut io);
    }
    (*cm_io::sector(sector).lists).sub_sectors.clear();
    r.tables_mut().free[EeAStarCmFree::SubSectors as usize].extend_from_slice(&ids);
}

// ---- the C interface

/// `ApplySubSectorLinks`.
///
/// # Safety
/// As `apply_links`; `r` the manager's record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_apply_links(r: *const EeAStarCmRecord, links: *mut EeSubSectorLinks) {
    apply_links(&*r, links);
}

/// `RemoveSubSectors`.
///
/// # Safety
/// As `remove_sub_sectors`; `r` the manager's record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_remove_sub_sectors(r: *mut EeAStarCmRecord, sector: u64) {
    remove_sub_sectors(&mut *r, sector);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_link_to_no_sub_sector_does_nothing() {
        let mut r: Box<std::mem::MaybeUninit<EeAStarCmRecord>> = Box::new(std::mem::MaybeUninit::uninit());
        // SAFETY: init writes the record whole; the other id names no sub sector, so no record is read
        unsafe {
            crate::astar::cm::ee_astar_cm_init(r.as_mut_ptr());
            let r = &mut *r.as_mut_ptr();
            link_sub_sectors(r, 0, 5, true, 0, 0);
            r.free();
        }
    }
}
