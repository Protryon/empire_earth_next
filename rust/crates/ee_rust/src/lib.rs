//! The game's Rust static library. `include/rust/ee_rust.h` is generated from the `extern "C"` items here
//! (`cargo run -p xtask -- cbindgen`).

use std::ffi::c_char;

// wgpu-native's `#[no_mangle]` C API (webgpu.h) reaches the staticlib through this dependency.
use ee_codec as _;
#[cfg(not(test))] // its FFI imports the platform layer's window functions (absent from a test binary)
use ee_gx as _;
#[cfg(not(test))] // its FFI imports the C++ shim
use ee_mixer as _;
#[cfg(not(test))] // its FFI imports the platform layer, absent from a test binary
use ee_net_tcp as _;
use ee_random as _;
use wgpu_native as _;
// the data layer's `extern "C"` API (ee_data/src/ffi.rs) reaches the staticlib through this dependency
use ee_data as _;

// The platform layer's leaves: its `#[no_mangle]` functions and variables reach the staticlib through this
// dependency.
use ee_plat_core as _;

// The platform layer's PlatFile, PlatThread and PlatSocket: `#[no_mangle]` exports, as above.
use ee_plat_sys as _;

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
