//! TEST ONLY: the unit store's pick and Fill records (USPK, USFL; `ee/EEUnitStore.h`) and their sweeps over every
//! player's units (EE-SIM, every five game minutes), moved here from EEUnitStore.cpp in the game's leaves round 1.
//! The players and their unit lists are read through the world views; the picks and the Fill tests are the store's
//! (select.rs, group.rs), as the C++ called them.

use crate::units::store::with_store;
use crate::world::view::{UnitView, WorldRead, WorldView};
use ee_base::sim_capture;
use std::ffi::c_void;

const TAG_PICK: u32 = 0x4b50_5355; // "USPK"
const TAG_FILL: u32 = 0x4c46_5355; // "USFL"

/// `EEWorld::GetPlayerIndex`: the last of `mPlayers[0 .. mNumPlayers)` that is `player`, -1 when none.
fn player_index(w: &WorldView, player: u64) -> i32 {
    let mut index = -1;
    for i in 0..w.num_players() as usize {
        if w.player(i).is_some_and(|p| p.handle() == player) {
            index = i as i32;
        }
    }
    index
}

/// A USPK record: the case, the player's index, the ids, the picked ids.
pub fn pick_record(case: u32, player_index: i32, ids: &[u32], picked: &[u32]) -> sim_capture::Record {
    let mut r = sim_capture::Record::new(TAG_PICK);
    r.u32(case);
    r.i32(player_index);
    r.u32(ids.len() as u32);
    ids.iter().for_each(|&v| r.u32(v));
    r.u32(picked.len() as u32);
    picked.iter().for_each(|&v| r.u32(v));
    r
}

/// A USFL record: the case, the player's index, the slot, the units' ids, the indices taken.
pub fn fill_record(case: u32, player_index: i32, slot: i64, ids: &[u32], taken: &[u32]) -> sim_capture::Record {
    let mut r = sim_capture::Record::new(TAG_FILL);
    r.u32(case);
    r.i32(player_index);
    r.i32(slot as i32);
    r.u32(ids.len() as u32);
    ids.iter().for_each(|&v| r.u32(v));
    r.u32(taken.len() as u32);
    taken.iter().for_each(|&v| r.u32(v));
    r
}

fn pick(player: u64, ids: &[u32]) -> Vec<u32> {
    let mut picked = Vec::with_capacity(ids.len());
    if !ids.is_empty() {
        with_store(|s| crate::units::select::select_pick(s, player, ids, &mut picked));
    }
    picked
}

fn fill(ids: &[u32], slot: i64) -> Vec<u32> {
    let mut taken = Vec::with_capacity(ids.len());
    if !ids.is_empty() {
        with_store(|s| crate::units::group::fill_members(s, ids, slot, &mut taken));
    }
    taken
}

/// # Safety
/// `p` readable for `n` (or null with 0).
unsafe fn words<'a>(p: *const u32, n: usize) -> &'a [u32] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// `EESelectPickCapture`: a USPK record of one pick (case 100: `EEPlayer::SelectUnits`).
///
/// # Safety
/// The caller holds the world; `player` is a player object; the id lists are readable for their counts.
#[no_mangle]
pub unsafe extern "C" fn ee_units_select_pick_capture(
    case: u32,
    player: u64,
    ids: *const u32,
    count: usize,
    picked: *const u32,
    picked_count: usize,
) {
    if !sim_capture::active() {
        return;
    }
    // SAFETY: the caller holds the world
    let token = unsafe { WorldRead::begin() };
    let index = WorldView::global(&token).map_or(-1, |w| player_index(&w, player));
    // SAFETY: the caller's lists
    let (ids, picked) = unsafe { (words(ids, count), words(picked, picked_count)) };
    pick_record(case, index, ids, picked).commit();
}

/// `EEFillCapture`: a USFL record of one Fill test (case 100: `EEActiveGroup::Fill`); `units` the selection's
/// complex unit objects.
///
/// # Safety
/// The caller holds the world; `units` holds `count` live unit objects; `taken` is readable for `taken_count`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_fill_capture(
    case: u32,
    player: u64,
    units: *const *const c_void,
    count: usize,
    slot: i64,
    taken: *const u32,
    taken_count: usize,
) {
    if !sim_capture::active() {
        return;
    }
    // SAFETY: the caller holds the world
    let token = unsafe { WorldRead::begin() };
    let index = WorldView::global(&token).map_or(-1, |w| player_index(&w, player));
    let units: &[*const c_void] = if units.is_null() || count == 0 {
        &[]
    } else {
        // SAFETY: the caller's list
        unsafe { std::slice::from_raw_parts(units, count) }
    };
    // SAFETY: live units (the caller's)
    let ids: Vec<u32> =
        units.iter().map(|&u| unsafe { UnitView::of(&token, u as u64) }.map_or(0, |v| v.id())).collect();
    // SAFETY: the caller's list
    let taken = unsafe { words(taken, taken_count) };
    fill_record(case, index, slot, &ids, taken).commit();
}

/// `EEFillMembers`: `EEActiveGroup::Fill`'s test over the store for the selection's complex unit objects `units`:
/// the indices (in order) of those an active group takes for formation slot `slot`; `out` has room for `count`.
///
/// # Safety
/// The caller holds the world; `units` holds `count` live unit objects; `out` is writable for `count`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_fill_members_of(
    units: *const *const c_void,
    count: usize,
    slot: i64,
    out: *mut u32,
) -> usize {
    if units.is_null() || count == 0 {
        return 0;
    }
    // SAFETY: the caller holds the world
    let token = unsafe { WorldRead::begin() };
    // SAFETY: the caller's list
    let units = unsafe { std::slice::from_raw_parts(units, count) };
    // SAFETY: live units (the caller's)
    let ids: Vec<u32> =
        units.iter().map(|&u| unsafe { UnitView::of(&token, u as u64) }.map_or(0, |v| v.id())).collect();
    let taken = fill(&ids, slot);
    // SAFETY: the caller's contract; at most one index per unit
    unsafe { std::ptr::copy_nonoverlapping(taken.as_ptr(), out, taken.len()) };
    taken.len()
}

/// `EESelectPickSweep`: the pick over every player's units (USPK cases 1..4).
///
/// # Safety
/// The caller holds the world (EE-SIM's physics thread).
#[no_mangle]
pub unsafe extern "C" fn ee_units_select_pick_sweep() {
    if !sim_capture::active() {
        return;
    }
    // SAFETY: the caller holds the world
    let token = unsafe { WorldRead::begin() };
    let Some(w) = WorldView::global(&token) else { return };
    let num = w.num_players() as usize;
    for p in 0..num {
        let Some(player) = w.player(p) else { continue };
        let handle = player.handle();
        let case = |c: u32, ids: &[u32]| {
            let picked = pick(handle, ids);
            pick_record(c, player_index(&w, handle), ids, &picked).commit();
        };
        let own: Vec<u32> = player.units().map(|u| u.id()).collect();
        // 1: every unit of its list in the list's order; 2: reversed, then again (every id twice)
        case(1, &own);
        let twice: Vec<u32> = own.iter().rev().chain(own.iter()).copied().collect();
        case(2, &twice);
        // 3: ids that are not its units (the next player's, the ids after its own, none, 0), then its own
        let mut others: Vec<u32> = Vec::new();
        if let Some(next) = w.player((p + 1) % num) {
            others.extend(next.units().take(200).map(|u| u.id()));
        }
        others.extend(own.iter().take(100).map(|&id| id.wrapping_add(1)));
        others.push(0xffff_ffff);
        others.push(0);
        others.extend_from_slice(&own);
        case(3, &others);
        // 4: the selections the game sends (at most 63 ids)
        for chunk in own.chunks(63) {
            case(4, chunk);
        }
    }
}

/// `EEFillSweep`: the Fill test over every player's complex units for every slot (USFL cases 1, 2).
///
/// # Safety
/// The caller holds the world (EE-SIM's physics thread).
#[no_mangle]
pub unsafe extern "C" fn ee_units_fill_sweep() {
    if !sim_capture::active() {
        return;
    }
    // SAFETY: the caller holds the world
    let token = unsafe { WorldRead::begin() };
    let Some(w) = WorldView::global(&token) else { return };
    for p in 0..w.num_players() as usize {
        let Some(player) = w.player(p) else { continue };
        let index = player_index(&w, player.handle());
        let own: Vec<u32> = player.units().filter(|u| u.is_complex()).map(|u| u.id()).collect();
        let reversed: Vec<u32> = own.iter().rev().copied().collect();
        // every slot (0..5) and one past them, the list in its order (1) and reversed (2)
        for slot in 0..=6i64 {
            for (case, ids) in [(1u32, &own), (2, &reversed)] {
                let taken = fill(ids, slot);
                fill_record(case, index, slot, ids, &taken).commit();
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn records_are_the_cpp_layout() {
        let r = pick_record(3, -1, &[5, 6], &[6]);
        let w: Vec<u32> = r.data.chunks(4).map(|c| u32::from_le_bytes([c[0], c[1], c[2], c[3]])).collect();
        assert_eq!(r.tag, TAG_PICK);
        assert_eq!(w, [3, 0xffff_ffff, 2, 5, 6, 1, 6]);
        let r = fill_record(1, 2, 6, &[9], &[]);
        let w: Vec<u32> = r.data.chunks(4).map(|c| u32::from_le_bytes([c[0], c[1], c[2], c[3]])).collect();
        assert_eq!(w, [1, 2, 6, 1, 9, 0]);
    }
}
