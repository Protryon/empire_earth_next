//! The game's screens on the UI toolkit (docs/RUST_PLAN.md "R6 game screens").
//!
//! The game's forms are the UI database's (`dbuiforms`, `dbuicontrols`); what makes them the game's screens is the
//! event classes the C++ hangs on their controls (`src/Empire Earth/UI Events`, `User Interface Thread`). Those
//! classes keep their objects, the controls and every call into the toolkit and the simulation; their decisions move
//! here as functions over plain values, each pinned by a golden trace of the unmodified unit.
//!
//! - [`settings`]: the game settings form (form 5): the graphics quality presets, the video tab's controls from the
//!   settings, the port's value combo boxes, the values `ApplySettings` takes from the controls, the audio and
//!   interface tabs' conversions.
//! - [`lobby`]: the game lobby (form 0x1a: the random map setup and the multiplayer staging screen): the player
//!   rows' controls, type menus, names and colours.
//! - [`lobby_options`]: the lobby's game options (what is shown and editable), the option events' messages and
//!   the start check.
//! - [`single_player`]: the single player screen (form 6): the random map's slot table; the player name check
//!   (`EEIsValidName`) it shares with the multiplayer screens.
//! - [`hud`]: the in-game HUD's status box: the attribute texts (base+bonus) and the armor attributes' values.

pub mod hud;
mod hud_ffi;
pub mod lobby;
mod lobby_ffi;
pub mod lobby_options;
mod lobby_options_ffi;
pub mod settings;
mod settings_ffi;
pub mod single_player;
mod single_player_ffi;
