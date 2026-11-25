//! TEST ONLY: the civilization builder lane's records of the simulation capture (`ee/EECivCapture.h`;
//! tools/sim_capture.py, every profile; was `EECivCapture.cpp`'s bodies, UI lane round 12) in Rust over the
//! one-statement shims of that unit. With `EE_SIM_CAPTURE` set:
//!   CVMG  once, at the first ApplyUpgrade: the manager's and the powers' data and every query over it
//!         (`ee_tech_capture_civ_manager`), then each premade civilization's file loaded and matched (and matched
//!         again with an upgrade added), then a match of no file
//!   CVAP  around one `ApplyUpgrade`: the upgrade, the player, the player's fields the levels change and every class
//!         upgrade of the category's unit set, before and after; a power level adds the player's loaded media count,
//!         `HasPower` of every power, its tech tree and each node's flags
//!   CVLD  after one `EELoadCivilization`: the name, the directory (1 for the user's), the result, the name read,
//!         the upgrades
//!   CVAF  after one `EEApplyCivilizationFile`: the name, the player's fields, the result, its civilization after
//! The tech tree's own records (the player's, an upgrade's, a tree's) are gamecpp tech_capture.rs'.

use core::ffi::{c_char, c_long};
use std::sync::atomic::{AtomicBool, Ordering};

use ee_sim::tech::civ::EeTechCivEntry;
use ee_sim::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved};

use crate::gamecpp::tech_capture as tc;
use crate::screens::game_cpp::{Cpp, EeScreensShimOut, EeScreensShimText};

/// Whether CVMG (the manager's record, once) is written.
pub(crate) static MANAGER: AtomicBool = AtomicBool::new(false);

fn tag(name: &[u8; 4]) -> u32 {
    u32::from_le_bytes(*name)
}

/// The level bits of an upgrade (`kCivUpgradeLevelBits`, `kCivUpgradeLevelMask`).
const LEVEL_BITS: u32 = 16;
const LEVEL_MASK: u32 = 0xffff;
/// `kCivPowerFlamingArrows` and `kCivPowerJustInTimeManufacturing` (`ee/EECivPowerIDs.h`): HasPower's range.
const POWER_FIRST: i32 = 0x26;
const POWER_JIT: i32 = 0x3c;

/// A record's payload.
#[derive(Default)]
pub(crate) struct Record(pub Vec<u8>);

impl Record {
    fn u32(&mut self, v: u32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
    fn i32(&mut self, v: i64) {
        self.0.extend_from_slice(&(v as i32).to_le_bytes());
    }
    fn f32(&mut self, v: f32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
    /// A text: its length, its bytes.
    fn text(&mut self, bytes: &[u8]) {
        self.u32(bytes.len() as u32);
        self.0.extend_from_slice(bytes);
    }
    fn upgrades(&mut self, list: &[u32]) {
        self.u32(list.len() as u32);
        for &u in list {
            self.u32(u);
        }
    }
}

/// The player's fields the levels and powers change (the tech capture's player record first; none past it for no
/// player).
fn player_fields<C: Cpp + ?Sized>(c: &mut C, r: &mut Record, player: usize) {
    r.0.extend_from_slice(&tc::player_record(player));
    if player == 0 {
        return;
    }
    // SAFETY: the live player's records, copied out
    let (saved, fields) = unsafe {
        (*(c.civc_player_saved(player) as *const EeUnitsPlayerSaved), *(c.civc_player_fields(player) as *const EeUnitsPlayerFields))
    };
    r.f32(fields.mConversionFactor);
    r.f32(saved.mHighGroundBonus);
    r.i32(fields.mShareBonus as i64);
    r.i32(fields.mMiningLimit as i64);
}

/// A class's upgrade record (none when the class does not exist).
fn class_upgrade<C: Cpp + ?Sized>(c: &mut C, r: &mut Record, player: usize, kind: i64) {
    let upgrade = if c.civh_class_listed(kind) { c.civc_class_upgrade(player, kind) } else { 0 };
    r.0.extend_from_slice(&tc::upgrade_record(upgrade));
}

/// The player of index `index` (0 for none: outside 0..17).
fn player_of<C: Cpp + ?Sized>(c: &mut C, index: c_long) -> usize {
    if (0..17).contains(&index) {
        c.civc_player(index as i64)
    } else {
        0
    }
}

/// The NUL-terminated name read last and its character count.
fn loaded_name<C: Cpp + ?Sized>(c: &mut C) -> (Vec<u8>, u32) {
    let mut name = Vec::new();
    c.civc_name(EeScreensShimOut::of(&mut name));
    name.push(0);
    (name, c.civc_name_length())
}

fn loaded<C: Cpp + ?Sized>(c: &mut C) -> Vec<u32> {
    (0..c.civc_loaded_count()).map(|i| c.civc_loaded_at(i)).collect()
}

/// `MatchesPremadeCivilization(file, name, upgrades, &id)` -> (whether, the name id it always writes).
fn matches(file: *const c_char, name: &[u8], characters: u32, upgrades: &[u32]) -> (bool, i32) {
    let mut id = -7;
    // SAFETY: the C strings live for the call
    let m = unsafe {
        ee_sim::tech::civ::ee_tech_civ_matches_premade(
            file,
            name.as_ptr() as *const c_char,
            characters,
            upgrades.as_ptr(),
            upgrades.len(),
            &mut id,
        )
    };
    (m, id)
}

/// CVMG.
fn manager<C: Cpp + ?Sized>(c: &mut C, commit: &mut impl FnMut(u32, &[u8])) {
    let mut r = Record::default();
    let mut size = 0u32;
    let head = ee_sim::tech::capture_rec::ee_tech_capture_civ_manager(&mut size);
    // SAFETY: the record's bytes, freed below
    r.0.extend_from_slice(unsafe { core::slice::from_raw_parts(head, size as usize) });
    // SAFETY: the pointer and size the capture returned
    unsafe { ee_sim::tech::capture_rec::ee_tech_capture_free(head, size) };
    let civs: Vec<EeTechCivEntry> = (0..ee_sim::tech::civ::ee_tech_civ_civilization_count())
        // SAFETY: a premade civilization in place
        .map(|i| unsafe { *ee_sim::tech::civ::ee_tech_civ_civilization(i) })
        .collect();
    for civ in &civs {
        // SAFETY: the entry's NUL-terminated file name
        let file = unsafe { core::ffi::CStr::from_ptr(civ.mFileName.as_ptr()) }.to_bytes().to_vec();
        c.civc_name_clear();
        c.civc_loaded_clear();
        r.i32(c.civc_civ_load(EeScreensShimText::of(&file)));
        let (name, characters) = loaded_name(c);
        let mut list = loaded(c);
        let (m, id) = matches(civ.mFileName.as_ptr(), &name, characters, &list);
        r.u32(m as u32);
        r.i32(id as i64);
        list.push(1);
        let (m, id) = matches(civ.mFileName.as_ptr(), &name, characters, &list);
        r.u32(m as u32);
        r.i32(id as i64);
    }
    let (m, id) = matches(c"no such file".as_ptr(), b"x\0", 1, &[]);
    r.u32(m as u32);
    r.i32(id as i64);
    commit(tag(b"CVMG"), &r.0);
}

/// CVAP's state between `ApplyUpgrade`'s two halves.
pub(crate) struct Apply {
    upgrade: u32,
    player: c_long,
    types: Vec<i64>,
    record: Record,
}

/// CVAP's first half: None when the capture is off (the manager's record first, once).
pub(crate) fn apply_begin<C: Cpp + ?Sized>(
    c: &mut C,
    upgrade: u32,
    player: c_long,
    manager_done: &AtomicBool,
    commit: &mut impl FnMut(u32, &[u8]),
) -> Option<Box<Apply>> {
    if !c.aec_active() {
        return None;
    }
    if !manager_done.swap(true, Ordering::Relaxed) {
        manager(c, commit);
    }
    let mut a = Box::new(Apply { upgrade, player, types: Vec::new(), record: Record::default() });
    a.record.u32(upgrade);
    a.record.i32(player as i64);
    let p = player_of(c, player);
    player_fields(c, &mut a.record, p);
    if p != 0 {
        let category = ee_sim::tech::civ::ee_tech_civ_category(upgrade >> LEVEL_BITS);
        // SAFETY: a category in place, read before any C++ call
        let set = if category.is_null() { None } else { Some(unsafe { (*category).mUnitSet }) };
        if let Some(set) = set {
            if c.civh_set_exists(set as i64) {
                c.civc_types_clear();
                c.civc_collect(set as i64);
                a.types = (0..c.civc_types_count()).map(|i| c.civc_type_at(i)).collect();
            }
        }
    }
    a.record.u32(a.types.len() as u32);
    for i in 0..a.types.len() {
        let kind = a.types[i];
        a.record.i32(kind);
        class_upgrade(c, &mut a.record, p, kind);
    }
    Some(a)
}

/// CVAP's second half with `ApplyUpgrade`'s result.
pub(crate) fn apply_end<C: Cpp + ?Sized>(c: &mut C, mut a: Apply, result: bool, commit: &mut impl FnMut(u32, &[u8])) {
    a.record.u32(result as u32);
    let p = player_of(c, a.player);
    player_fields(c, &mut a.record, p);
    for i in 0..a.types.len() {
        let kind = a.types[i];
        class_upgrade(c, &mut a.record, p, kind);
    }
    let level = a.upgrade & LEVEL_MASK;
    if p != 0 && ee_sim::tech::civ::ee_tech_civ_is_power_level(level) {
        a.record.u32(c.civc_media_count(p) as u32);
        let data = c.civm_purchased_data(p) as *const u32;
        let count = c.civm_purchased_count(p);
        for id in POWER_FIRST..=POWER_JIT + 2 {
            // SAFETY: the player's purchased upgrades, unchanged across the loop
            a.record.u32(unsafe { ee_sim::tech::civ::ee_tech_civ_has_power(data, count, id) } as u32);
        }
        a.record.0.extend_from_slice(&tc::tree_nodes_record(p));
    }
    commit(tag(b"CVAP"), &a.record.0);
}

/// CVLD after `EELoadCivilization(name, directory)`.
pub(crate) fn load<C: Cpp + ?Sized>(
    c: &mut C,
    name: &[u8],
    directory: &[u8],
    result: c_long,
    civ_name: &[u8],
    upgrades: &[u32],
    commit: &mut impl FnMut(u32, &[u8]),
) {
    if !c.aec_active() {
        return;
    }
    let mut r = Record::default();
    r.text(name);
    let user = c.civc_is_user_directory(EeScreensShimText::of(directory));
    r.u32(user as u32);
    if !user {
        r.text(directory);
    }
    r.i32(result as i64);
    r.text(civ_name);
    r.upgrades(upgrades);
    commit(tag(b"CVLD"), &r.0);
}

/// CVAF after `EEApplyCivilizationFile(name, player)`.
pub(crate) fn apply_file<C: Cpp + ?Sized>(
    c: &mut C,
    name: &[u8],
    player: usize,
    result: c_long,
    commit: &mut impl FnMut(u32, &[u8]),
) {
    if !c.aec_active() {
        return;
    }
    let mut r = Record::default();
    r.text(name);
    player_fields(c, &mut r, player);
    r.i32(result as i64);
    if player != 0 {
        let civ = c.civc_civ(player);
        let bought: Vec<u32> = (0..c.civc_civ_purchased_count(civ)).map(|i| c.civc_civ_purchased_at(civ, i)).collect();
        r.upgrades(&bought);
        let deferred: Vec<u32> = (0..c.civc_civ_deferred_count(civ)).map(|i| c.civc_civ_deferred_at(civ, i)).collect();
        r.upgrades(&deferred);
        let mut text = Vec::new();
        c.civc_civ_name(civ, EeScreensShimOut::of(&mut text));
        r.text(&text);
        r.u32(c.civc_civ_available(civ));
        r.u32(c.civc_civ_max(civ));
        r.u32(c.civc_civ_no_points(civ) as u32 | (c.civc_civ_unchanged(civ) as u32) << 1);
        // SAFETY: the live player's record
        r.u32(unsafe { (*(c.civc_player_saved(player) as *const EeUnitsPlayerSaved)).mCivUpgradesPurchased } as u32);
    }
    commit(tag(b"CVAF"), &r.0);
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn off_nothing_is_read() {
        let mut fake = Fake::default();
        let done = AtomicBool::new(false);
        assert!(apply_begin(&mut fake, 0x10002, 1, &done, &mut |_, _: &[u8]| panic!()).is_none());
        load(&mut fake, b"a", b"d", 0, b"n", &[], &mut |_, _: &[u8]| panic!());
        apply_file(&mut fake, b"a", 0, 0, &mut |_, _: &[u8]| panic!());
        assert_eq!(fake.calls(), "aec_active()\naec_active()\naec_active()");
    }

    #[test]
    fn a_load_names_the_user_directory_by_a_flag() {
        let mut fake = Fake::default();
        fake.answer("aec_active", &[1, 1]).answer("civc_is_user_directory", &[1, 0]);
        let mut records = Vec::new();
        load(&mut fake, b"ab", b"dir", 3, b"N", &[7], &mut |t, p: &[u8]| records.push((t, p.to_vec())));
        load(&mut fake, b"ab", b"dir", 3, b"N", &[], &mut |t, p: &[u8]| records.push((t, p.to_vec())));
        assert_eq!(records[0].0, tag(b"CVLD"));
        assert_eq!(
            records[0].1,
            [2, 0, 0, 0, b'a', b'b', 1, 0, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, b'N', 1, 0, 0, 0, 7, 0, 0, 0]
        );
        assert_eq!(records[1].1[6..17], [0, 0, 0, 0, 3, 0, 0, 0, b'd', b'i', b'r'][..11]);
    }

    #[test]
    fn a_file_without_a_player_has_its_hook_record_only() {
        let mut fake = Fake::default();
        fake.answer("aec_active", &[1]);
        let mut records = Vec::new();
        apply_file(&mut fake, b"x", 0, -2, &mut |t, p: &[u8]| records.push((t, p.to_vec())));
        assert_eq!(records, [(tag(b"CVAF"), vec![1, 0, 0, 0, b'x', 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff])]);
        assert_eq!(fake.calls(), "aec_active()");
    }

    #[test]
    fn the_apply_halves_of_a_player_outside_the_table() {
        let mut fake = Fake::default();
        let done = AtomicBool::new(true);
        fake.answer("aec_active", &[1]);
        let a = apply_begin(&mut fake, 0x10002, 20, &done, &mut |_, _: &[u8]| panic!()).unwrap();
        let mut records = Vec::new();
        apply_end(&mut fake, *a, true, &mut |t, p: &[u8]| records.push((t, p.to_vec())));
        assert_eq!(records[0].0, tag(b"CVAP"));
        assert_eq!(
            records[0].1,
            [2, 0, 1, 0, 20, 0, 0, 0, 255, 255, 255, 255, 0, 0, 0, 0, 1, 0, 0, 0, 255, 255, 255, 255]
        );
        assert!(!fake.calls().contains("civc_player("));
    }
}
