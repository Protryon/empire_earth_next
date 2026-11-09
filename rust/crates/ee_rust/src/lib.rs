//! The game's Rust static library. `include/rust/ee_rust.h` is generated from the `extern "C"` items here
//! (`cargo run -p xtask -- cbindgen`).

use std::ffi::c_char;

use ee_codec as _;
use ee_fs as _;
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
// the UI toolkit's algorithms behind the C++ UI* classes (ee_ui); its FFI imports C++ host functions, absent from a
// test binary
#[cfg(not(test))]
use ee_ui as _;
// the random map generator's `extern "C"` API (ee_rmap/src/ffi.rs) reaches the staticlib through this dependency
use ee_rmap as _;
// the data layer's `extern "C"` API (ee_data/src/ffi.rs) reaches the staticlib through this dependency
use ee_data as _;

// glTF -> .cem and PNG -> .sst (ee_openformats/src/ffi.rs): the open-format readers GEOpenFormatsFile.cpp calls
use ee_openformats as _;

// The platform layer's leaves: its `#[no_mangle]` functions and variables reach the staticlib through this
// dependency.
use ee_plat_core as _;

// The platform layer's PlatFile, PlatThread and PlatSocket: `#[no_mangle]` exports, as above.
use ee_plat_sys as _;

// The window (PlatWindow.h on SDL3) and the fonts (PlatFont.h, the Windows-1252 conversions): `#[no_mangle]` exports. Two
// crates so that a program using only text does not pull in an object file that references SDL (the linker pulls whole
// objects); ee_plat_sdl's exports are compiled out of `cargo test`, whose binaries do not link SDL.
#[cfg(not(test))]
use ee_plat_sdl as _;
use ee_plat_ui as _;

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
