//! The app and world lane's Rust (docs/progress/r6-app-and-world.md): whole classes of
//! `src/Empire Earth/Empire Earth` behind their C++ types.
//!
//! - [`game_options`]: `EEGameOptions` and `EEGameOptionsPlayer` as records the C++ classes derive from, their
//!   defaults, validating copy, lobby defaults and saved game.
//! - [`settings`]: `EEGameSettings`' plain members as a record the C++ class derives from, their defaults, and
//!   config.yaml's rows read and written (the preferences' `Load` and `Save`; [`settings_ffi`]: its C ABI).
//! - [`camera`]: `EECameraControl` whole: its plain members as a record the C++ class derives from, every body over
//!   a host ([`camera_ffi`]: its C ABI).
//! - [`save_io`]: a saved game's primitives over a host callback (`EESavePrimitive`'s operations).
//! - [`ffi`]: the C ABI the C++ classes call.

pub mod camera;
pub mod camera_ffi;
pub mod ffi;
pub mod game_options;
pub mod save_io;
pub mod settings;
pub mod settings_ffi;

#[cfg(test)]
mod camera_tests;
#[cfg(test)]
mod game_options_tests;
#[cfg(test)]
mod settings_tests;
