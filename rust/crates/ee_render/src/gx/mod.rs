//! GX in Rust (docs/RUST_PLAN.md R4.1): the subset of DirectDraw 7 and Direct3D 7 the renderer uses, on the `wgpu` crate
//! at the same version the C API (wgpu-native v24.0.3.1) used. `include/gx/GX.h` keeps its classes as thin C++ shims
//! forwarding to the `extern "C"` functions of `ffi.rs`; this crate owns the objects.
//!
//! Layout: `abi` (the C structures and constants), `format`/`keys`/`uniform`/`modes`/`surface`/`stats`/`shaders` (pure
//! logic, unit tested against golden values from the C++), `gx`/`device`/`directdraw` (the object model over a `Backend`
//! and a `Platform`, tested against fakes), `gpu` (the wgpu `Backend`, thin; device tests skip when no adapter is
//! reachable), `plat`/`ffi` (the real `Platform` and the C ABI, compiled out of `cargo test`).

// `c_ulong` is `u32` on Windows and `u64` elsewhere: the casts that are no-ops on one platform are needed on the other
#![allow(clippy::unnecessary_cast)]

pub mod abi;
/// cbindgen:ignore
pub mod cformat;
/// cbindgen:ignore
pub mod consts;
/// cbindgen:ignore
pub mod device;
/// cbindgen:ignore
pub mod directdraw;
/// cbindgen:ignore
pub mod format;
/// cbindgen:ignore
pub mod gpu;
/// cbindgen:ignore
#[allow(clippy::module_inception)] // the module was the crate ee_gx
pub mod gx;
/// cbindgen:ignore
pub mod keys;
/// cbindgen:ignore
pub mod modes;
/// cbindgen:ignore
pub mod plat;
/// cbindgen:ignore
pub mod shaders;
/// cbindgen:ignore
pub mod stats;
/// cbindgen:ignore
pub mod surface;
/// cbindgen:ignore
pub mod uniform;

#[cfg(not(test))]
pub(crate) mod ffi;

#[cfg(test)]
mod fake;
#[cfg(test)]
mod gx_tests;
