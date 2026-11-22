//! TEST ONLY: the civilization builder lane's capture records (was EECivCapture.cpp; ee/EECivCapture.h;
//! tools/sim_capture.py): CVMG, the manager (its Rust head, then each civilization file loaded and matched against the
//! premade ones, once, at the first CVAP); CVAP, an upgrade applied to a player (the player and its class upgrades
//! before and after, and for a power level the powers, the tech tree and its nodes); CVLD, a civilization file
//! loaded; CVAF, a civilization file applied to a player. Reads the world through the cases of EECivCapture.cpp's
//! `EECore10OpC2`; changes nothing but the capture file (CVMG loads the premade civilizations' files through
//! EELoadCivilization, as the game's choice form does).

use std::ffi::{c_long, c_ulong, c_void, CStr};
use std::sync::atomic::{AtomicBool, Ordering};

use ee_sim::units::player_lists::ee_units_lists_len;
use ee_sim::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved};

use super::calamity_capture::Record;
use super::{h, hp, pa, ptr, rec, Owned, O, P};

/// `kCivPowerFlamingArrows`, `kCivPowerJustInTimeManufacturing` (ee/EECivPowerIDs.h).
const CC_POWER_FIRST: i64 = 0x26;
const CC_POWER_LAST: i64 = 0x3c;
/// `EE_UNITS_LISTS_MEDIA`.
const CC_LISTS_MEDIA: u32 = 6;

fn active() -> bool {
    ee_sim::app::capture::ee_app_capture_active()
}

/// A C string's bytes (none for NULL).
fn c_text<'a>(text: P) -> &'a [u8] {
    if text == 0 {
        return &[];
    }
    // SAFETY: a C string the C++ answered, read before its next call
    unsafe { CStr::from_ptr(text as *const std::ffi::c_char) }.to_bytes()
}

/// A text as its length and bytes.
fn text(r: &mut Record, bytes: &[u8]) {
    r.u32(bytes.len() as u32);
    r.bytes(bytes);
}

/// The bytes a C++ capture writer appends (`op` given a scratch EESimCaptureRecord and an object).
fn cpp(r: &mut Record, op: O, object: P) {
    let scratch = Owned::new(O::LcRecNew, O::LcRecDelete);
    h(op, scratch.0, &[pa(object)]);
    let size = h(O::LcRecSize, scratch.0, &[]) as usize;
    if size > 0 {
        let data = hp(O::LcRecData, scratch.0, &[]);
        // SAFETY: the scratch record's bytes, read before it goes
        r.bytes(unsafe { std::slice::from_raw_parts(data as *const u8, size) });
    }
}

/// An upgrade list (a std::vector<EECivUpgrade>) as its count and items.
fn upgrades(r: &mut Record, list: P) {
    let n = h(O::LcUpgVecSize, list, &[]);
    r.u32(n as u32);
    for i in 0..n {
        r.u32(h(O::LcUpgVecAt, list, &[i]) as u32);
    }
}

/// The player's capture fields (EETechCapturePlayer's, then four of its own when there is a player).
fn player_fields(r: &mut Record, player: P) {
    cpp(r, O::LcRecTechPlayer, player);
    if player == 0 {
        return;
    }
    // SAFETY: the player's records, copied
    let (f, s): (EeUnitsPlayerFields, EeUnitsPlayerSaved) =
        unsafe { (*rec(O::PlFields, player), *rec(O::PlSaved, player)) };
    r.f32(f.mConversionFactor);
    r.f32(s.mHighGroundBonus);
    r.i32(f.mShareBonus as i32);
    r.i32(f.mMiningLimit as i32);
}

/// The player of an index (none out of 0..17).
fn player_of(index: c_long) -> P {
    if (0..17).contains(&index) {
        hp(O::LcWorldPlayer, 0, &[index as i64])
    } else {
        0
    }
}

/// CVMG: the manager's Rust head, each civilization file loaded and matched against the premade ones (as loaded,
/// then with an upgrade 1 more), then an unknown file.
fn capture_manager() {
    let mut r = Record::default();
    let mut size: u32 = 0;
    let head = ee_sim::tech::capture_rec::ee_tech_capture_civ_manager(&mut size);
    // SAFETY: the head's bytes, freed after the copy
    unsafe {
        r.bytes(std::slice::from_raw_parts(head, size as usize));
        ee_sim::tech::capture_rec::ee_tech_capture_free(head, size);
    }
    let civs = Owned::new(O::LcCivVecNew, O::LcCivVecDelete);
    h(O::LcCivGetAll, civs.0, &[]);
    for i in 0..h(O::LcCivVecSize, civs.0, &[]) {
        let name = Owned::new(O::LcWideNew, O::LcWideDelete);
        let loaded = Owned::new(O::LcUpgVecNew, O::LcUpgVecDelete);
        r.i32(h(O::LcCivLoad, civs.0, &[i, pa(name.0), pa(loaded.0)]) as i32);
        let mut id: c_long = -7;
        r.u32(h(O::LcCivMatch, civs.0, &[i, pa(name.0), pa(loaded.0), ptr(&mut id)]) as u32);
        r.i32(id as i32);
        h(O::LcUpgVecPush, loaded.0, &[1]);
        r.u32(h(O::LcCivMatch, civs.0, &[i, pa(name.0), pa(loaded.0), ptr(&mut id)]) as u32);
        r.i32(id as i32);
    }
    let mut id: c_long = -7;
    let none = Owned::new(O::LcUpgVecNew, O::LcUpgVecDelete);
    r.u32(h(O::LcCivMatchNone, none.0, &[ptr(&mut id)]) as u32);
    r.i32(id as i32);
    r.commit(b"CVMG");
}

/// An upgrade's capture in flight (EECivCaptureApply: Begin makes it, End writes and frees it).
pub struct CivApply {
    upgrade: u32,
    player: c_long,
    types: Vec<c_long>,
    record: Record,
}

static MANAGER_DONE: AtomicBool = AtomicBool::new(false);

/// The upgrade's class upgrades (by the unit types of its category's set).
fn class_upgrades(r: &mut Record, player: P, types: &[c_long]) {
    for &t in types {
        cpp(r, O::LcRecTechUpgrade, hp(O::LcPlUpgradeOf, player, &[t as i64]));
    }
}

/// CVAP's first half (EECivCaptureApplyBegin): the upgrade, the player and its fields, the category's unit types
/// and their class upgrades; NULL when the capture is off.
#[no_mangle]
pub extern "C" fn ee_gamecpp_civcap_apply_begin(upgrade: u32, player: c_long) -> *mut c_void {
    if !active() {
        return std::ptr::null_mut();
    }
    if !MANAGER_DONE.swap(true, Ordering::Relaxed) {
        capture_manager();
    }
    let mut c = Box::new(CivApply { upgrade, player, types: Vec::new(), record: Record::default() });
    c.record.u32(upgrade);
    c.record.i32(player as i32);
    let pl = player_of(player);
    player_fields(&mut c.record, pl);
    let mut category: c_ulong = 0;
    h(O::LcCivSplit, 0, &[upgrade as i64, ptr(&mut category), 0]);
    let mut entry: P = 0;
    if pl != 0 && h(O::LcCivCategory, 0, &[category as i64, ptr(&mut entry)]) != 0 {
        let set = ee_sim::app::unit_sets::ee_unit_sets_get(h(O::LcCivCategorySet, entry, &[]) as c_ulong as usize);
        if !set.is_null() {
            // SAFETY: a live unit set (ForEach's walk: every type in the set)
            let mut t = unsafe { ee_sim::app::unit_sets::ee_unit_set_next(set, 0) };
            while t != 0 {
                c.types.push(t as c_long);
                // SAFETY: as above
                t = unsafe { ee_sim::app::unit_sets::ee_unit_set_next(set, t) };
            }
        }
    }
    c.record.u32(c.types.len() as u32);
    for i in 0..c.types.len() {
        let t = c.types[i];
        c.record.i32(t as i32);
        cpp(&mut c.record, O::LcRecTechUpgrade, hp(O::LcPlUpgradeOf, pl, &[t as i64]));
    }
    Box::into_raw(c) as *mut c_void
}

/// CVAP's second half (EECivCaptureApplyEnd): the result, the player and its class upgrades again, and for a power
/// level the media list's length, the powers, the tech tree and its nodes' flags; the record written.
///
/// # Safety
/// `capture` is NULL or what [`ee_gamecpp_civcap_apply_begin`] answered, not yet ended.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_civcap_apply_end(capture: *mut c_void, result: bool) {
    if capture.is_null() {
        return;
    }
    // SAFETY: the caller's contract
    let c = *unsafe { Box::from_raw(capture as *mut CivApply) };
    let mut r = c.record;
    r.u32(result as u32);
    let pl = player_of(c.player);
    player_fields(&mut r, pl);
    class_upgrades(&mut r, pl, &c.types);
    let mut level: c_ulong = 0;
    h(O::LcCivSplit, 0, &[c.upgrade as i64, 0, ptr(&mut level)]);
    if pl != 0 && h(O::LcCivIsPowerLevel, 0, &[level as i64]) != 0 {
        // SAFETY: the player's lists
        r.u32(unsafe { ee_units_lists_len(hp(O::LcPlLists, pl, &[]) as *const _, CC_LISTS_MEDIA) } as u32);
        for id in CC_POWER_FIRST..=CC_POWER_LAST + 2 {
            r.u32(h(O::LcPlHasPower, pl, &[id]) as u32);
        }
        let tree = hp(O::LcPlTechTree, pl, &[]);
        if tree != 0 {
            cpp(&mut r, O::LcRecTechTree, tree);
            let ids = Owned::new(O::LcLongVecNew, O::LcLongVecDelete);
            h(O::LcTreeNodeIDs, tree, &[pa(ids.0)]);
            let n = h(O::LcLongVecSize, ids.0, &[]);
            r.u32(n as u32);
            for i in 0..n {
                let id = h(O::LcLongVecAt, ids.0, &[i]);
                let node = hp(O::LcTreeGet, tree, &[id]);
                r.i32(id as i32);
                r.u32(if node != 0 { h(O::LcNodeFlags, node, &[]) as u32 } else { 0 });
            }
        }
    }
    r.commit(b"CVAP");
}

/// CVLD: a civilization file loaded (its name, its directory unless the user's, the result, the civilization's
/// name and upgrades).
#[no_mangle]
pub extern "C" fn ee_gamecpp_civcap_load(
    name: *const c_void,
    directory: *const c_void,
    result: c_long,
    civ_name: *const c_void,
    list: *const c_void,
) {
    if !active() {
        return;
    }
    let mut r = Record::default();
    text(&mut r, c_text(hp(O::LcUStringText, name as P, &[])));
    let user = h(O::LcCivDirIsUser, directory as P, &[]) != 0;
    r.u32(user as u32);
    if !user {
        text(&mut r, c_text(hp(O::LcUStringText, directory as P, &[])));
    }
    r.i32(result as i32);
    text(&mut r, c_text(hp(O::LcWideText, civ_name as P, &[])));
    upgrades(&mut r, list as P);
    r.commit(b"CVLD");
}

/// CVAF: a civilization file applied to a player (the name, the player's fields, the result, and with a player its
/// civilization and whether its upgrades were bought).
#[no_mangle]
pub extern "C" fn ee_gamecpp_civcap_apply_file(name: *const c_void, player: *mut c_void, result: c_long) {
    if !active() {
        return;
    }
    let pl = player as P;
    let mut r = Record::default();
    text(&mut r, c_text(hp(O::LcUStringText, name as P, &[])));
    player_fields(&mut r, pl);
    r.i32(result as i32);
    if pl != 0 {
        let civ = hp(O::LcPlCivilization, pl, &[]);
        upgrades(&mut r, hp(O::LcCivPurchased, civ, &[]));
        upgrades(&mut r, hp(O::LcCivDeferred, civ, &[]));
        text(&mut r, c_text(hp(O::LcCivName, civ, &[])));
        r.u32(h(O::LcCivPoints, civ, &[]) as u32);
        r.u32(h(O::LcCivMaxPoints, civ, &[]) as u32);
        r.u32(h(O::LcCivFlags, civ, &[]) as u32);
        // SAFETY: the player's record
        r.u32(unsafe { (*rec::<EeUnitsPlayerSaved>(O::PlSaved, pl)).mCivUpgradesPurchased } as u32);
    }
    r.commit(b"CVAF");
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_text_is_its_length_then_its_bytes() {
        let mut r = Record::default();
        text(&mut r, b"abc");
        let mut want = Record::default();
        want.u32(3);
        want.bytes(b"abc");
        assert_eq!(r.payload(), want.payload());
        assert!(c_text(0).is_empty());
    }

    #[test]
    fn the_powers_run_from_flaming_arrows_two_past_just_in_time() {
        assert_eq!((CC_POWER_FIRST..=CC_POWER_LAST + 2).count(), 25);
        assert_eq!(player_of(-1), 0);
        assert_eq!(player_of(17), 0);
    }
}
