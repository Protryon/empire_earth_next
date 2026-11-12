//! The game's Rust static library. `include/rust/ee_rust.h` is generated from the `extern "C"` items here
//! (`cargo run -p xtask -- cbindgen`).

use std::ffi::c_char;

use ee_codec as _;
use ee_fs as _;
// the Graphics Engine's math and the table trigonometry (ee_ge/src/ffi.rs), called by the C++ GE classes and UMath.cpp
use ee_ge as _;
// the Graphics Thread's and the camera's math (ee_gfx/src/ffi.rs), called by EECameraControl.cpp
use ee_gfx as _;
// the DX7 renderer plug-in behind the C++ trampolines (ee_dx7/src/ffi.rs); it imports them, absent from a test binary
#[cfg(not(test))]
use ee_dx7 as _;
#[cfg(not(test))] // its FFI imports the platform layer's window functions (absent from a test binary)
use ee_gx as _;
#[cfg(not(test))] // its FFI imports the C++ shim
use ee_mixer as _;
#[cfg(not(test))] // its FFI imports the platform layer, absent from a test binary
use ee_net_tcp as _;
use ee_random as _;
#[cfg(not(test))]
// the sound manager behind SESoundManager.cpp: its FFI imports the C++ trampolines (ee_soundcpp_*)
use ee_sound as _;
// the UI toolkit's algorithms behind the C++ UI* classes (ee_ui); its FFI imports C++ host functions, absent from a
// test binary
#[cfg(not(test))]
use ee_ui as _;
// the random map generator's `extern "C"` API (ee_rmap/src/ffi.rs) reaches the staticlib through this dependency
use ee_rmap as _;
// formations (ee_formations/src/ffi.rs): the geometry the formation classes call
use ee_formations as _;
// the tile A* searches' storage and machine (ee_astar/src/ffi.rs), called by the C++ EEAIAStar* classes
use ee_astar as _;
// the combat and unit formulas (ee_combat/src/ffi.rs) the C++ unit methods call (ee/EECombatFormulas.h)
use ee_combat as _;
// the tech tree's research and prerequisites, the event effects' arithmetic, the calamities' timing (ee_tech/src/ffi.rs),
// called by the C++ tech tree, EEEvent.cpp and the calamity classes
use ee_tech as _;
// the data layer's `extern "C"` API (ee_data/src/ffi.rs) reaches the staticlib through this dependency
use ee_data as _;

// glTF -> .cem and PNG -> .sst (ee_openformats/src/ffi.rs): the open-format readers GEOpenFormatsFile.cpp calls
use ee_openformats as _;

// The platform layer's leaves: its `#[no_mangle]` functions and variables reach the staticlib through this
// dependency.
use ee_plat_core as _;

// The platform layer's PlatFile, PlatThread and PlatSocket: `#[no_mangle]` exports, as above.
use ee_plat_sys as _;

// The window (PlatWindow.h on SDL3) and the fonts (PlatFont.h): `#[no_mangle]` exports. Two
// crates so that a program using only text does not pull in an object file that references SDL (the linker pulls whole
// objects); ee_plat_sdl's exports are compiled out of `cargo test`, whose binaries do not link SDL.
#[cfg(not(test))]
use ee_plat_sdl as _;
use ee_plat_ui as _;

// the WON library's leaves (ee_won/src/ffi.rs): the buffers' and text helpers, CRC-16, MD5, Blowfish, ElGamal, the auth
// blocks and the CD keys behind the C++ forwarders of src/Empire Earth/WONAPI
use ee_won as _;

// the unit registry behind EEUnitHandle<T> and the unit store (ee_units/src/ffi.rs), called by RTSUnit.cpp and
// EEUnitStore.cpp; its host imports the C++ world calls (EEUnitHost.cpp), absent from a test binary
#[cfg(not(test))]
use ee_units as _;

/// The combat reads' bridge to the unit store (ee_combat reads.rs `Stored`: ee_combat cannot depend on ee_units, whose
/// host imports C++): ee_units' record under `id`, its plain fields into `out`; false for none (always in a test
/// binary, which has no store's host).
///
/// # Safety
/// `out` must point to a writable record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_store_unit(id: u32, out: *mut ee_combat::reads::EeCombatStoredUnit) -> bool {
    #[cfg(not(test))]
    {
        let Some(r) = ee_units::store::with_store(|s| s.get(id).copied()) else { return false };
        // SAFETY: the function's contract
        unsafe { *out = combat_stored(&r) };
        true
    }
    #[cfg(test)]
    {
        let _ = (id, out);
        false
    }
}

/// The bridge by the object (the combat lane's handle, an `EEUnit*`'s address: ee_units' object index), so a read
/// needs no id read from the host first: the record of that object into `out`; false for none (always in a test binary).
///
/// # Safety
/// `out` must point to a writable record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_store_unit_of(object: u64, out: *mut ee_combat::reads::EeCombatStoredUnit) -> bool {
    #[cfg(not(test))]
    {
        let Some(r) = ee_units::store::with_store(|s| s.by_object(object).copied()) else { return false };
        // SAFETY: the function's contract
        unsafe { *out = combat_stored(&r) };
        true
    }
    #[cfg(test)]
    {
        let _ = (object, out);
        false
    }
}

#[cfg(not(test))]
fn combat_stored(r: &ee_units::store::EeUnitRecord) -> ee_combat::reads::EeCombatStoredUnit {
    ee_combat::reads::EeCombatStoredUnit {
        object: r.object,
        object_kind: r.object_kind,
        built: r.built,
        tile_x: r.tile_x,
        tile_y: r.tile_y,
        hit_points: r.hit_points,
        unit_type: r.unit_type,
        category: r.category,
        command: r.command,
        location: r.location,
        target_search_timer: r.target_search_timer,
    }
}

// the game messages' wire codec (ee_netmsg/src/ffi.rs), called by EEMessage.cpp's ExportMessage/ImportMessage
use ee_netmsg as _;
// the game screens' decisions (ee_screens: the settings form's presets and value tables) behind the C++ event classes
use ee_screens as _;

/// Version of the Rust library's C interface; bumped when a seam changes incompatibly.
pub const EE_RUST_ABI_VERSION: u32 = 1;

/// Returns `EE_RUST_ABI_VERSION`.
#[no_mangle]
pub extern "C" fn ee_rust_abi_version() -> u32 {
    EE_RUST_ABI_VERSION
}

/// Installs the panic hook: a Rust panic gives `log` one line, then the process aborts. Call once at startup.
///
/// # Safety
/// `log` must be callable from any thread for the life of the process, with a NUL-terminated line valid during the
/// call.
#[no_mangle]
pub unsafe extern "C" fn ee_rust_init(log: unsafe extern "C" fn(line: *const c_char)) {
    ee_panic::install(log);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn abi_version_is_exported_unchanged() {
        assert_eq!(ee_rust_abi_version(), EE_RUST_ABI_VERSION);
    }
}
