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
/// The crate ee_gfx before the restructure.
pub mod gfx;
/// The crate ee_gx before the restructure.
/// cbindgen:ignore
pub mod gx;
/// The crate ee_openformats before the restructure.
pub mod openformats;
