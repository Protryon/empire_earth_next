//! TEST ONLY: the box selection's pick and the active groups' Fill test over every player's units (USPK and USFL
//! records; was C++, EEUnitStore.cpp's `EESelectPickSweep`, `EEFillSweep` and their captures; the units' behaviour
//! lane, round 2). EE-SIM runs both sweeps every five game minutes while the capture runs; the game's own picks and
//! fills write the same records (case 100) when the capture asks for them.

fn put(out: &mut Vec<u8>, v: u32) {
    out.extend_from_slice(&v.to_le_bytes());
}

/// The USPK record: the case, the player's index, the ids, the picked ids.
pub fn pick_record(case: u32, player_index: i32, ids: &[u32], picked: &[u32]) -> Vec<u8> {
    let mut r = Vec::with_capacity(16 + 4 * (ids.len() + picked.len()));
    put(&mut r, case);
    put(&mut r, player_index as u32);
    put(&mut r, ids.len() as u32);
    ids.iter().for_each(|&i| put(&mut r, i));
    put(&mut r, picked.len() as u32);
    picked.iter().for_each(|&i| put(&mut r, i));
    r
}

/// The USFL record: the case, the player's index, the slot, the units' ids, the taken indices.
pub fn fill_record(case: u32, player_index: i32, slot: i64, ids: &[u32], taken: &[u32]) -> Vec<u8> {
    let mut r = Vec::with_capacity(20 + 4 * (ids.len() + taken.len()));
    put(&mut r, case);
    put(&mut r, player_index as u32);
    put(&mut r, slot as i32 as u32);
    put(&mut r, ids.len() as u32);
    ids.iter().for_each(|&i| put(&mut r, i));
    put(&mut r, taken.len() as u32);
    taken.iter().for_each(|&i| put(&mut r, i));
    r
}

/// The pick sweep's cases of a player (its list's ids `own`, the next player's `next`): 1 every unit in the list's
/// order; 2 reversed, then again; 3 ids that are not its units (the next player's (200 at most), its own ids plus one
/// (100 at most), none, 0), then its own; 4 the selections the game sends (63 ids at most).
pub fn pick_cases(own: &[u32], next: &[u32]) -> Vec<(u32, Vec<u32>)> {
    let mut cases = vec![(1, own.to_vec())];
    let mut twice: Vec<u32> = own.iter().rev().copied().collect();
    twice.extend_from_slice(own);
    cases.push((2, twice));
    let mut others: Vec<u32> = next.iter().take(200).copied().collect();
    others.extend(own.iter().take(100).map(|&i| i.wrapping_add(1)));
    others.push(u32::MAX);
    others.push(0);
    others.extend_from_slice(own);
    cases.push((3, others));
    for chunk in own.chunks(63) {
        cases.push((4, chunk.to_vec()));
    }
    cases
}

/// The fill sweep's cases of a player (its list's complex units' ids): every slot 0..5 and one past them, the list
/// in its order (1) and reversed (2).
pub fn fill_cases(own: &[u32]) -> Vec<(u32, i64, Vec<u32>)> {
    let reversed: Vec<u32> = own.iter().rev().copied().collect();
    let mut cases = Vec::new();
    for slot in 0..=6 {
        cases.push((1, slot, own.to_vec()));
        cases.push((2, slot, reversed.clone()));
    }
    cases
}

#[cfg(not(test))]
mod game {
    use super::*;

    /// "USPK"
    const PICK_TAG: u32 = 0x4b50_5355;
    /// "USFL"
    const FILL_TAG: u32 = 0x4c46_5355;
    use crate::app::capture::{commit, ee_app_capture_active};
    use crate::units::ffi::{ee_units_fill_members, ee_units_select_pick};
    use crate::world::view::{WorldRead, WorldView};

    /// The player's index in the world (`EEWorld::GetPlayerIndex`'s loop: the last match; -1 for none).
    fn player_index(t: &WorldRead, player: u64) -> i32 {
        let Some(w) = WorldView::global(t) else { return -1 };
        let mut index = -1;
        for i in 0..w.num_players() as usize {
            if w.player(i).map_or(0, |p| p.handle()) == player {
                index = i as i32;
            }
        }
        index
    }

    fn slice<'a>(p: *const u32, n: usize) -> &'a [u32] {
        if n == 0 || p.is_null() {
            &[]
        } else {
            // SAFETY: the callers' contracts: `p` points to `n` words
            unsafe { std::slice::from_raw_parts(p, n) }
        }
    }

    /// TEST ONLY: the USPK record of a pick (`EESelectPickCapture`; `player` an `EEPlayer*`).
    ///
    /// # Safety
    /// `ids` points to `count` ids and `picked` to `picked_count`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_select_pick_capture(
        case: u32,
        player: u64,
        ids: *const u32,
        count: usize,
        picked: *const u32,
        picked_count: usize,
    ) {
        if !ee_app_capture_active() {
            return;
        }
        // SAFETY: the simulation thread's call
        let t = unsafe { WorldRead::begin() };
        commit(PICK_TAG, &pick_record(case, player_index(&t, player), slice(ids, count), slice(picked, picked_count)));
    }

    /// TEST ONLY: the USFL record of a fill (`EEFillCapture`; the units as their ids).
    ///
    /// # Safety
    /// `ids` points to `count` ids and `taken` to `taken_count`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_fill_capture(
        case: u32,
        player: u64,
        ids: *const u32,
        count: usize,
        slot: i64,
        taken: *const u32,
        taken_count: usize,
    ) {
        if !ee_app_capture_active() {
            return;
        }
        // SAFETY: as above
        let t = unsafe { WorldRead::begin() };
        commit(
            FILL_TAG,
            &fill_record(case, player_index(&t, player), slot, slice(ids, count), slice(taken, taken_count)),
        );
    }

    /// The ids of the units of the player at `index` (its list's order; complex units only when `complex`).
    fn list_ids(w: &WorldView<'_>, index: usize, complex: bool) -> Option<Vec<u32>> {
        let p = w.player(index)?;
        Some(p.units().filter(|u| !complex || u.is_complex()).map(|u| u.id()).collect())
    }

    /// TEST ONLY: the pick sweep (`EESelectPickSweep`).
    #[no_mangle]
    pub extern "C" fn ee_units_select_pick_sweep() {
        if !ee_app_capture_active() {
            return;
        }
        // SAFETY: EE-SIM's sweep between ticks
        let t = unsafe { WorldRead::begin() };
        let Some(w) = WorldView::global(&t) else { return };
        let n = w.num_players() as usize;
        for p in 0..n {
            let Some(own) = list_ids(&w, p, false) else { continue };
            let player = w.player(p).map_or(0, |v| v.handle());
            let next = list_ids(&w, (p + 1) % n, false).unwrap_or_default();
            for (case, ids) in pick_cases(&own, &next) {
                let mut picked = vec![0u32; ids.len() + 1];
                // SAFETY: the ids and room for as many picks
                let count = unsafe { ee_units_select_pick(player, ids.as_ptr(), ids.len(), picked.as_mut_ptr()) };
                commit(PICK_TAG, &pick_record(case, player_index(&t, player), &ids, &picked[..count]));
            }
        }
    }

    /// TEST ONLY: the fill sweep (`EEFillSweep`).
    #[no_mangle]
    pub extern "C" fn ee_units_fill_sweep() {
        if !ee_app_capture_active() {
            return;
        }
        // SAFETY: as above
        let t = unsafe { WorldRead::begin() };
        let Some(w) = WorldView::global(&t) else { return };
        for p in 0..w.num_players() as usize {
            let Some(own) = list_ids(&w, p, true) else { continue };
            let player = w.player(p).map_or(0, |v| v.handle());
            for (case, slot, ids) in fill_cases(&own) {
                let mut taken = vec![0u32; ids.len() + 1];
                // SAFETY: as above
                let count = unsafe { ee_units_fill_members(ids.as_ptr(), ids.len(), slot, taken.as_mut_ptr()) };
                commit(FILL_TAG, &fill_record(case, player_index(&t, player), slot, &ids, &taken[..count]));
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_pick_cases() {
        let own: Vec<u32> = (1..=70).collect();
        let cases = pick_cases(&own, &[500, 501]);
        assert_eq!(cases[0], (1, own.clone()));
        assert_eq!(cases[1].1.len(), 140);
        assert_eq!(cases[1].1[0], 70);
        let others = &cases[2].1;
        assert_eq!(&others[..3], &[500, 501, 2]);
        assert_eq!(others.len(), 2 + 70 + 2 + 70);
        assert_eq!(&others[72..74], &[u32::MAX, 0]);
        // 70 ids: one chunk of 63 and one of 7
        assert_eq!(cases.len(), 3 + 2);
        assert_eq!(cases[4], (4, (64..=70).collect()));
        // no units: cases 1-3 only (case 3 still holds the two sentinels)
        let empty = pick_cases(&[], &[]);
        assert_eq!(empty.len(), 3);
        assert_eq!(empty[2].1, vec![u32::MAX, 0]);
    }

    #[test]
    fn the_fill_cases_and_the_records() {
        let cases = fill_cases(&[7, 8]);
        assert_eq!(cases.len(), 14);
        assert_eq!(cases[1], (2, 0, vec![8, 7]));
        assert_eq!(cases[13], (2, 6, vec![8, 7]));
        let r = fill_record(1, -1, 6, &[7, 8], &[1]);
        let w: Vec<u32> = r.chunks(4).map(|b| u32::from_le_bytes([b[0], b[1], b[2], b[3]])).collect();
        assert_eq!(w, vec![1, u32::MAX, 6, 2, 7, 8, 1, 1]);
        let r = pick_record(4, 2, &[9], &[]);
        assert_eq!(r.len(), 4 * 5);
    }
}
