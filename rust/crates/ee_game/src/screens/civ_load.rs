//! `EECBCivilizationFiles.cpp`'s `EELoadCivilization` in Rust over one-statement shims (UI lane round 12): the file
//! found on the data path (else the startup volume's spec, through the unit's global spec as the original built
//! it), read whole, parsed by `ee_sim::tech::civ_file`, its name and upgrades handed back, a parse error shown;
//! then the capture's CVLD hook. The C++ function is one forward.

// the forward is the FFI module's (absent from a test binary)
#![cfg_attr(test, allow(dead_code))]

use core::ffi::{c_long, c_ulong};

use crate::screens::game_cpp::{
    Cpp, EeScreensShimOut, EeScreensShimPtrOut, EeScreensShimText, EeScreensShimULongOut,
};

/// `kCivKey`: a civilization file's extension.
const CIV_KEY: &[u8] = b".civ";

/// `EELoadCivilization(name, directory, out name, out upgrades)` -> the read's result. `name` and `directory` are
/// the `UString*`s, `out_name` the `UWideString*`, `out_upgrades` the `std::vector<EECivUpgrade>*`.
pub(crate) fn load_civilization<C: Cpp + ?Sized>(
    c: &mut C,
    name: usize,
    directory: usize,
    out_name: usize,
    out_upgrades: usize,
) -> c_long {
    let mut n = Vec::new();
    c.civl_string(name, EeScreensShimOut::of(&mut n));
    let mut d = Vec::new();
    c.civl_string(directory, EeScreensShimOut::of(&mut d));
    let mut file_name = n.clone();
    file_name.extend_from_slice(CIV_KEY);
    let mut path = d.clone();
    path.extend_from_slice(&file_name);
    let mut file = 0usize;
    c.civl_global_close();
    if c.civl_find(EeScreensShimText::of(&path), EeScreensShimPtrOut::of(&mut file)) != 0 {
        file = c.civl_spec_new();
        c.civl_global_volume();
        c.civl_global_directory(EeScreensShimText::of(&d));
        c.civl_global_file(EeScreensShimText::of(&file_name));
        c.civl_spec_assign_global(file);
    }
    let mut result = c.civl_open(file) as c_long;
    if result == 0 {
        let mut size: c_ulong = 0;
        let mut bytes = Vec::new();
        if c.civl_length(file, EeScreensShimULongOut::of(&mut size)) == 0 {
            c.civl_buffer_resize(size as u64);
            if size == 0 || c.civl_read(file, size as u64) == 0 {
                c.civl_buffer(EeScreensShimOut::of(&mut bytes));
            }
        }
        let read = ee_sim::tech::civ_file::read_file(&ee_sim::tech::civ::tables(), &bytes, &file_name);
        result = read.result as c_long;
        if let Some(text) = &read.name {
            c.civl_set_name(out_name, EeScreensShimText::of(text));
        }
        for &u in &read.upgrades {
            c.civl_upgrade_push(out_upgrades, u);
        }
        if let Some(error) = read.error.as_ref().filter(|e| !e.is_empty()) {
            c.civl_message(EeScreensShimText::of(error));
        }
    }
    c.civl_close(file);
    c.civl_delete(file);
    c.civl_capture(name, directory, result as i64, out_name, out_upgrades);
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn a_file_not_on_the_data_path_is_the_startup_volumes() {
        let mut fake = Fake::default();
        fake.text("civl_string", b"cbgate1").text("civl_string", b"Civilizations\\");
        fake.answer("civl_find", &[2]).answer("civl_spec_new", &[40]).answer("civl_open", &[5]);
        assert_eq!(load_civilization(&mut fake, 1, 2, 3, 4), 5);
        let calls = fake.calls();
        assert!(calls.contains("civl_find(\"Civilizations\\\\cbgate1.civ\", out)\ncivl_spec_new()\ncivl_global_volume()"));
        assert!(calls.ends_with("civl_open(40)\ncivl_close(40)\ncivl_delete(40)\ncivl_capture(1, 2, 5, 3, 4)"), "{calls}");
    }

    #[test]
    fn an_empty_file_reads_as_nothing() {
        let mut fake = Fake::default();
        fake.text("civl_string", b"x").text("civl_string", b"d/").out("civl_find", 30);
        let r = load_civilization(&mut fake, 1, 2, 3, 4);
        let calls = fake.calls();
        assert!(calls.contains("civl_length(30, out)\ncivl_buffer_resize(0)\ncivl_buffer(out)\n"), "{calls}");
        assert!(calls.ends_with(&format!("civl_capture(1, 2, {r}, 3, 4)")));
    }
}
