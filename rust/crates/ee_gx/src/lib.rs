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
pub mod cformat;
pub mod device;
pub mod directdraw;
pub mod format;
pub mod gpu;
pub mod gx;
pub mod keys;
pub mod modes;
pub mod plat;
pub mod shaders;
pub mod stats;
pub mod surface;
pub mod uniform;

#[cfg(not(test))]
mod ffi;

#[cfg(test)]
mod fake;
#[cfg(test)]
mod gx_tests;
