//! The platform layer, the leaves (utilities, errors, time, threads, strings, codecs, files, input), the GE math and
//! the PNG reader: what every other crate of the game builds on.

/// The crate ee_codec before the restructure.
pub mod codec;
/// The crate ee_error before the restructure.
pub mod error;
/// The crate ee_fs before the restructure.
pub mod fs;
/// The crate ee_input before the restructure.
pub mod input;
pub mod math;
/// The crate ee_panic before the restructure.
/// cbindgen:ignore
pub mod panic;
/// The crate ee_plat_core before the restructure.
/// cbindgen:ignore
pub mod plat_core;
/// The crate ee_plat_sdl before the restructure.
/// cbindgen:ignore
pub mod plat_sdl;
/// The crate ee_plat_sys before the restructure.
/// cbindgen:ignore
pub mod plat_sys;
/// The crate ee_plat_ui before the restructure.
/// cbindgen:ignore
pub mod plat_ui;
pub mod png;
/// The crate ee_random before the restructure.
/// cbindgen:ignore
pub mod random;
/// The crate ee_time before the restructure.
pub mod time;
/// The crate ee_ts before the restructure.
pub mod ts;
/// The crate ee_ustring before the restructure.
pub mod ustring;
/// The crate ee_util before the restructure.
pub mod util;
