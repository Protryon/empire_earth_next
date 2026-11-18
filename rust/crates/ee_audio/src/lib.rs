//! The sound manager, the mixer and the music engine.

// `c_long`/`c_ulong` are `i32`/`u32` on Windows and `i64`/`u64` on Linux and macOS: a cast or `from` that one data
// model needs is to the same type on the other. The Windows gate keeps both lints; off Windows they would flag
// only those.
#![cfg_attr(not(windows), allow(clippy::unnecessary_cast, clippy::useless_conversion))]

/// The music engine: a DirectMusic performance on rustysynth (the port of dmusic). No C interface.
/// cbindgen:ignore
pub mod dmusic;
/// The crate ee_mixer before the restructure.
pub mod mixer;
/// The crate ee_sound before the restructure.
pub mod sound;
