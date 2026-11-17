//! The app and world lane's Rust (docs/progress/r6-app-and-world.md): whole classes of
//! `src/Empire Earth/Empire Earth` behind their C++ types.
//!
//! - [`game_options`]: `EEGameOptions` and `EEGameOptionsPlayer` as records the C++ classes derive from, their
//!   defaults, validating copy, lobby defaults and saved game.
//! - [`settings`]: `EEGameSettings`' plain members as a record the C++ class derives from, their defaults, and
//!   config.yaml's rows read and written (the preferences' `Load` and `Save`; [`settings_ffi`]: its C ABI).
//! - [`camera`]: `EECameraControl` whole: its plain members as a record the C++ class derives from, every body over
//!   a host ([`camera_ffi`]: its C ABI).
//! - [`ambient`]: the ambient sounds whole (`EEAmbientSoundManager`, `EEAmbientSound`, the world analyzer's counts):
//!   records the C++ classes derive from, every decision over a host ([`ambient_ffi`]: its C ABI).
//! - [`map`]: `EEMap`'s sight, cliffs, terrain painting, water colours, checksum and tile count over a host
//!   ([`map_ffi`]: its C ABI; it reads and edits the terrain mesh itself).
//! - [`minimap`]: `EEMiniMap`'s cells (sight counts, building and unit dots, the unit layer's pixels), the terrain
//!   pixels' colours and the redraw's walk ([`minimap_ffi`]: its C ABI, the object the C++ class owns).
//! - [`cheats`]: `EECheatCodes`' table, chat line parse, lookup, list and dispatch ([`cheats_ffi`]: its C ABI).
//! - [`storm`]: `EEEMStorm`'s name, fades, step and route costs as a record the C++ class derives from and bodies
//!   over a host ([`storm_ffi`]: its C ABI).
//! - [`save_io`]: a saved game's primitives over a host callback (`EESavePrimitive`'s operations).
//! - [`ffi`]: the C ABI the C++ classes call.

pub mod ambient;
pub mod ambient_ffi;
pub mod camera;
pub mod camera_ffi;
pub mod ffi;
pub mod game_options;
pub mod map_ffi;
pub mod minimap;
pub mod minimap_ffi;
pub mod save_io;
pub mod settings;
pub mod settings_ffi;

#[cfg(test)]
mod ambient_tests;
#[cfg(test)]
mod camera_tests;
#[cfg(test)]
mod game_options_tests;
#[cfg(test)]
mod minimap_tests;
#[cfg(test)]
mod settings_tests;
