//! The game's screens on the UI toolkit (docs/RUST_PLAN.md "R6 game screens").
//!
//! The game's forms are the UI database's (`dbuiforms`, `dbuicontrols`); what makes them the game's screens is the
//! event classes the C++ hangs on their controls (`src/Empire Earth/UI Events`, `User Interface Thread`). Those
//! classes keep their objects, the controls and every call into the toolkit and the simulation; their decisions move
//! here as functions over plain values, each pinned by a golden trace of the unmodified unit.
//!
//! - [`settings`]: the game settings form (form 5): the graphics quality presets, the video tab's controls from the
//!   settings, the port's value combo boxes.
//! - [`lobby`]: the game lobby (form 0x1a: the random map setup and the multiplayer staging screen): the player
//!   rows' controls, type menus, names and colours.

pub mod lobby;
mod lobby_ffi;
pub mod settings;
mod settings_ffi;
