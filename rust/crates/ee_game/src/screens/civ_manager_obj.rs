//! `EECivilizationManager`'s and `EECivPowersManager`'s remaining C++ bodies in Rust over one-statement shims (UI
//! lane round 12): the db files' bytes read whole (`EECivReadAll`: whole 0x1000-byte chunks, then single bytes) for
//! the three tables' loads, the premade civilizations' copies (all, or of an era), `ApplyUpgrade` (the capture's
//! halves around the simulation's apply, its world [`Host`] the civilization host `civ_manager_calls.rs`), the
//! powers' test of a player, and the out-parameter members (the premade match, a category's name id and record, the
//! legacy id, an upgrade's split). The C++ members are one forward each.
#![allow(clippy::unnecessary_cast)]
// c_long and c_ulong are 32 bits on Windows, 64 on LP64

// the forwards are the FFI module's (absent from a test binary)
#![cfg_attr(test, allow(dead_code))]

use core::ffi::{c_long, c_ulong};

use ee_sim::tech::civ_apply::{CivHost, EeTechCivCall, EeTechCivReply};

use crate::screens::game_cpp::{Cpp, EeScreensShimOut};

/// The simulation's world calls (`EECivHostCall`'s, now Rust's): the civilization host's cases.
pub(crate) struct Host<'a, C: Cpp + ?Sized> {
    pub c: &'a mut C,
}

impl<C: Cpp + ?Sized> CivHost for Host<'_, C> {
    fn call(&mut self, call: &EeTechCivCall) -> EeTechCivReply {
        let mut reply = EeTechCivReply::default();
        crate::screens::civ_manager_calls::host(self.c, call, &mut reply);
        reply
    }
}

/// `EECivReadAll(file)`: the file's bytes (whole chunks of 0x1000 while a chunk reads, then byte by byte); None
/// when it does not open. The file is closed either way.
pub(crate) fn read_all<C: Cpp + ?Sized>(c: &mut C, file: usize) -> Option<Vec<u8>> {
    let result = c.civm_file_open(file);
    let mut bytes = Vec::new();
    while result == 0 && c.civm_read_chunk(file) == 0 {
        c.civm_chunk_bytes(EeScreensShimOut::of(&mut bytes));
    }
    while result == 0 && c.civm_read_byte(file) == 0 {
        bytes.push(c.civm_byte());
    }
    c.civm_file_close(file);
    (result == 0).then_some(bytes)
}

/// The tables a load fills: the civilization builder's categories (with the powers' extra cost), the premade
/// civilizations; any other `which` the powers.
pub const EE_SCREENS_CIVM_LOAD_TABLES: u32 = 0;
pub const EE_SCREENS_CIVM_LOAD_PREMADE: u32 = 1;

/// `Load(file)`, `LoadPremadeCivilizations(file)`, `EECivPowersManager::Load(file)` -> the table's result,
/// `kErrorFailed` (1) when the file does not read.
pub(crate) fn load<C: Cpp + ?Sized>(c: &mut C, file: usize, which: u32) -> c_long {
    let Some(bytes) = read_all(c, file) else { return 1 };
    // SAFETY: the bytes live for the call
    unsafe {
        (match which {
            EE_SCREENS_CIVM_LOAD_TABLES => {
                let cost = c.civm_powers_cost() as i32;
                ee_sim::tech::civ::ee_tech_civ_load(bytes.as_ptr(), bytes.len(), cost)
            }
            EE_SCREENS_CIVM_LOAD_PREMADE => ee_sim::tech::civ::ee_tech_civ_load_premade(bytes.as_ptr(), bytes.len()),
            _ => ee_sim::tech::civ::ee_tech_civ_load_powers(bytes.as_ptr(), bytes.len()),
        }) as c_long
    }
}

/// `GetCivilizations(out)` (`era` None) / `GetCivilizationsOfType(out, era)`: the premade civilizations' records
/// copied into the C++ vector `out`, emptied first.
pub(crate) fn civilizations<C: Cpp + ?Sized>(c: &mut C, out: usize, era: Option<i32>) {
    c.civm_civs_clear(out);
    for i in 0..ee_sim::tech::civ::ee_tech_civ_civilization_count() {
        let entry = ee_sim::tech::civ::ee_tech_civ_civilization(i);
        // SAFETY: a premade civilization in place
        if era.is_none_or(|e| unsafe { (*entry).mEraNameID } == e) {
            c.civm_civs_push(out, entry as usize);
        }
    }
}

/// `ApplyUpgrade(upgrade, player)`: the capture's first half, the simulation's apply over [`Host`], the second.
pub(crate) fn apply_upgrade<C: Cpp + ?Sized>(c: &mut C, upgrade: u32, player: c_long) -> bool {
    let commit = &mut ee_sim::app::capture::commit;
    let capture =
        crate::screens::civ_capture::apply_begin(c, upgrade, player, &crate::screens::civ_capture::MANAGER, commit);
    let result = ee_sim::tech::civ_apply::apply_upgrade(&mut Host { c: &mut *c }, upgrade, player as i32);
    if let Some(a) = capture {
        crate::screens::civ_capture::apply_end(c, *a, result, commit);
    }
    result
}

/// `EECivPowersManager::HasPower(player, id)`: one of the player's purchased upgrades gives the power.
pub(crate) fn has_power<C: Cpp + ?Sized>(c: &mut C, player: usize, id: c_long) -> bool {
    let data = c.civm_purchased_data(player) as *const u32;
    let count = c.civm_purchased_count(player);
    // SAFETY: the player's purchased upgrades, unchanged across the call
    unsafe { ee_sim::tech::civ::ee_tech_civ_has_power(data, count, id as i32) }
}

/// `SplitAttribute(upgrade)` -> (category, level index).
pub(crate) fn split(upgrade: u32) -> (c_ulong, c_ulong) {
    ((upgrade >> 16) as c_ulong, (upgrade & 0xffff) as c_ulong)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn a_file_reads_in_chunks_then_bytes() {
        let mut fake = Fake::default();
        fake.answer("civm_read_chunk", &[0, 1]).text("civm_chunk_bytes", b"ab");
        fake.answer("civm_read_byte", &[0, 0, 1]).answer("civm_byte", &[7, 8]);
        assert_eq!(read_all(&mut fake, 3), Some(b"ab\x07\x08".to_vec()));
        assert!(fake.calls().ends_with("civm_read_byte(3)\ncivm_file_close(3)"));
        let mut fake = Fake::default();
        fake.answer("civm_file_open", &[2]);
        assert_eq!(read_all(&mut fake, 3), None);
        assert_eq!(fake.calls(), "civm_file_open(3)\ncivm_file_close(3)");
        let mut fake = Fake::default();
        fake.answer("civm_file_open", &[2]);
        assert_eq!(load(&mut fake, 3, 2), 1);
    }

    #[test]
    fn an_upgrade_splits_into_its_category_and_level() {
        assert_eq!(split(0x0003_0002), (3, 2));
        let mut fake = Fake::default();
        civilizations(&mut fake, 9, Some(1));
        assert!(fake.calls().starts_with("civm_civs_clear(9)"));
    }
}
