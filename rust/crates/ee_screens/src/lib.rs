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
//! - [`command_panel`]: the in-game command panel: its layout (which buttons show where), each button's
//!   availability and icon, its tooltip's plan and the targeting modes of its commands.
//! - [`settings_form`]: the game settings form whole (its events, state and every toolkit and engine call, over the
//!   C++ host).
//! - [`custom_campaign`]: the custom campaign form whole (its events over the C++ host).
//! - [`menus`]: the out-of-game menus whole (the main menu, the editors' sub-menu, the single player screen): their
//!   events' codes, the control-event table and one dispatcher over the host's toolkit and game calls.
//! - [`hud`]: the in-game HUD's status box: the attribute texts (base+bonus), the armor attributes' values,
//!   `Fill`'s attribute icons and bars, `Refresh`'s texts, bars and the carried resource's icon.

pub mod command_panel;
mod command_panel_ffi;
pub mod custom_campaign;
pub mod hud;
mod hud_ffi;
pub mod lobby;
mod lobby_ffi;
pub mod lobby_options;
mod lobby_options_ffi;
pub mod menus;
// imports the C++ host's toolkit and game calls (UI Events/EEScreens.cpp), absent from a test binary
#[cfg(not(test))]
mod menus_ffi;
pub mod settings;
pub mod settings_form;
#[cfg(not(test))]
mod settings_form_ffi;
pub mod single_player;
mod single_player_ffi;
