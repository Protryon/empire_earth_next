//! The continent manager's saved regions put back (AI Thread cluster round 7; was EEAIContinentSave.cpp's
//! EERestoreContinentRegions and its TownOrHerd): the block read, the players' towns and herds given their saved
//! serials and filed again, the serial counter; a damaged block rebuilds the regions as an older save's load does.
//! The towns' ids and what only C++ holds (the manager's Clear and BuildRegions, the players' RegisterHerds and
//! RegisterTowns, the debug output) are one host case each (EE_ASTAR_CM_* from 41).

#![cfg(not(test))]
#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::collections::BTreeMap;
use std::ffi::CString;
use std::os::raw::{c_long, c_ulong};

use crate::astar::cm::EeAStarCmRecord;
use crate::astar::cm_io::{self, EeAStarCmIo};
use crate::world::{PlayerView, WorldRead, WorldView};

/// the town `h[0]`'s `mTownID`
pub const EE_ASTAR_CM_TOWN_ID: i32 = 41;
/// `gContinentManager.Clear(gWorld.mMap)`, `.BuildRegions(gWorld.mMap)`
pub const EE_ASTAR_CM_CLEAR_MAP: i32 = 42;
pub const EE_ASTAR_CM_BUILD_MAP: i32 = 43;
/// player `i[0]`'s `RegisterHerds()` (`i[1]` 0) or `RegisterTowns()` (1)
pub const EE_ASTAR_CM_REGISTER: i32 = 44;
/// `ESDebugOut("%s", (const char*)h[0])`
pub const EE_ASTAR_CM_DEBUG_OUT: i32 = 45;

fn host(op: i32, h: u64, i: [i64; 4]) -> i64 {
    let mut io = EeAStarCmIo { h: [h, 0], i };
    cm_io::call_host(op, &mut io)
}

fn debug(text: &str) {
    let c = CString::new(text).unwrap_or_default();
    host(EE_ASTAR_CM_DEBUG_OUT, c.as_ptr() as u64, [0; 4]);
}

/// Every player's towns (`k` 0) or herds (1), in the players' order: (player index, its list).
fn owned(k: usize) -> Vec<(usize, Vec<u64>)> {
    let t = unsafe { WorldRead::begin() };
    let Some(w) = WorldView::global(&t) else { return Vec::new() };
    (0..w.num_players() as usize)
        .filter_map(|p| {
            let v: PlayerView<'_> = w.player(p)?;
            let list = if k == 0 {
                (0..v.towns_len()).map(|i| v.town_handle(i)).collect()
            } else {
                (0..v.herds_len()).map(|i| v.herd_handle(i)).collect()
            };
            Some((p, list))
        })
        .collect()
}

/// A town's (`k` 0) or herd's id.
fn id_of(k: usize, h: u64) -> c_long {
    if k == 0 {
        host(EE_ASTAR_CM_TOWN_ID, h, [0; 4]) as c_long
    } else {
        // SAFETY: a listed live herd
        unsafe { crate::world::view::herd_id(h) as c_long }
    }
}

/// `EE_ASTAR_CM_OWNER`'s answer (was TownOrHerd): 7, the town's id and its player for a town; 8 for a herd; 0 else.
pub(crate) fn owner(h: u64, io: &mut EeAStarCmIo) {
    io.i[0] = 0;
    io.i[1] = -1;
    io.i[2] = -1;
    let (towns, herds) = (owned(0), owned(1));
    for ((p, ts), (_, hs)) in towns.iter().zip(herds.iter()) {
        if ts.contains(&h) {
            io.i = [7, id_of(0, h) as i64, *p as i64, io.i[3]];
            return;
        }
        if hs.contains(&h) {
            io.i = [8, id_of(1, h) as i64, *p as i64, io.i[3]];
            return;
        }
    }
}

/// `EERestoreContinentRegions`: the regions of the save being loaded put back (its block taken: `cm_file.rs`); false
/// without one.
///
/// # Safety
/// `r` is the manager's live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_restore_loaded(r: *mut EeAStarCmRecord) -> bool {
    let bytes = crate::astar::cm_file::take_loaded();
    // SAFETY: the caller's contract; the block's bytes
    !bytes.is_empty() && unsafe { ee_astar_cm_restore_regions(r, bytes.as_ptr(), bytes.len()) }
}

/// `EERestoreContinentRegions` after its block was taken: false when the block could not be read (the regions are
/// built again).
///
/// # Safety
/// `r` is the manager's live record; `data` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_restore_regions(r: *mut EeAStarCmRecord, data: *const u8, len: usize) -> bool {
    // SAFETY: the caller's contract
    let load = unsafe { cm_io::ee_astar_cm_read_regions(r, data, len) };
    if load.is_null() {
        debug("EEAIContinentSave: the saved regions could not be read: the load builds its own\n");
        host(EE_ASTAR_CM_CLEAR_MAP, 0, [0; 4]);
        host(EE_ASTAR_CM_BUILD_MAP, 0, [0; 4]);
        for (p, _) in owned(0) {
            host(EE_ASTAR_CM_REGISTER, 0, [p as i64, 0, 0, 0]);
            host(EE_ASTAR_CM_REGISTER, 0, [p as i64, 1, 0, 0]);
        }
        return false;
    }
    // SAFETY: the load just made
    let info = unsafe { cm_io::ee_astar_cm_load_info(load) };
    if info.unknown_owners != 0 {
        debug(&format!(
            "EEAIContinentSave: {} serials named areas of no known owner in the saved game\n",
            info.unknown_owners
        ));
    }
    if info.unfiled_sub_sectors != 0 {
        debug(&format!("EEAIContinentSave: {} saved sub sectors are in no sector\n", info.unfiled_sub_sectors));
    }
    // the herds and towns the load made again: their saved serials (by player and id), else the load's
    let mut saved: [BTreeMap<(i64, i64), c_ulong>; 2] = [BTreeMap::new(), BTreeMap::new()];
    for (k, map) in saved.iter_mut().enumerate() {
        let mut count = 0usize;
        // SAFETY: as above
        let o = unsafe { cm_io::ee_astar_cm_load_owned(load, k == 1, &mut count) };
        for i in 0..count {
            // SAFETY: the load's list
            let e = unsafe { &*o.add(i) };
            map.insert((e.player, e.index), e.serial as c_ulong);
        }
    }
    // SAFETY: as above, not used again
    unsafe { cm_io::ee_astar_cm_load_free(load) };
    let mut unmatched: c_ulong = 0;
    let (towns, herds) = (owned(0), owned(1));
    for ((p, ts), (_, hs)) in towns.iter().zip(herds.iter()) {
        for (k, list) in [ts, hs].into_iter().enumerate() {
            for &area in list {
                if area == 0 {
                    continue;
                }
                match saved[k].remove(&(*p as i64, id_of(k, area) as i64)) {
                    // SAFETY: the listed live area's record
                    Some(serial) => unsafe { cm_io::area(area).serial = serial },
                    None => unmatched += 1,
                }
                // SAFETY: the manager's record, a live area
                unsafe { crate::astar::cm_queries::ee_astar_cm_add_object_area(r, area) };
            }
        }
    }
    if unmatched != 0 || !saved[0].is_empty() || !saved[1].is_empty() {
        debug(&format!(
            "EEAIContinentSave: {} herds and towns of the load are not the saved ones ({} saved towns, {} saved herds left)\n",
            unmatched,
            saved[0].len(),
            saved[1].len()
        ));
    }
    // SAFETY: the caller's record
    let r = unsafe { &mut *r };
    r.clearing = info.clearing;
    // an area of the load past the saved counter (a herd or town the saved game did not have) keeps the counter
    let last = r.tables().areas.last().map(|a| a.serial);
    match last {
        Some(s) if s > info.last_serial => {
            debug(&format!(
                "EEAIContinentSave: an area of the load has serial {} past the saved {}: the counter stays\n",
                s, info.last_serial
            ));
            r.last_serial = s;
        }
        _ => r.last_serial = info.last_serial,
    }
    true
}
