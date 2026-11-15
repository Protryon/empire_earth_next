//! The app and world lane's Rust (docs/progress/r6-app-and-world.md): whole classes of
//! `src/Empire Earth/Empire Earth` behind their C++ types.
//!
//! - [`game_options`]: `EEGameOptions` and `EEGameOptionsPlayer` as records the C++ classes derive from, their
//!   defaults, validating copy, lobby defaults and saved game.
//! - [`save_io`]: a saved game's primitives over a host callback (`EESavePrimitive`'s operations).
//! - [`ffi`]: the C ABI the C++ classes call.

pub mod ffi;
pub mod game_options;
pub mod save_io;

#[cfg(test)]
mod game_options_tests;
