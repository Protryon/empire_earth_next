//! The renderer: the Graphics Engine, the graphics thread, GX (wgpu), the DX7 plug-in and the open formats (glTF, PNG
//! frames).

// `c_long`/`c_ulong` are `i32`/`u32` on Windows and `i64`/`u64` on Linux and macOS: a cast or `from` that one data
// model needs is to the same type on the other. The Windows gate keeps both lints; off Windows they would flag
// only those.
#![cfg_attr(not(windows), allow(clippy::unnecessary_cast, clippy::useless_conversion))]

/// The crate ee_dx7 before the restructure.
pub mod dx7;
/// The crate ee_ge before the restructure.
pub mod ge;
/// The Graphics Engine's named constants (render states, pixel formats, fonts, clip planes) for its C++ headers.
pub mod gestate;
// the value types' C ABI under lle/GERust.h (a small header every C++ unit includes)
pub mod geval;
/// The crate ee_gfx before the restructure.
pub mod gfx;
/// The crate ee_gx before the restructure (its C ABI: abi.rs, ffi.rs; the rest is cbindgen:ignore).
pub mod gx;
/// The crate ee_openformats before the restructure.
pub mod openformats;
/// The records C++ classes derive from or embed (include/rust/render/records.h, declaring no function).
pub mod records;
