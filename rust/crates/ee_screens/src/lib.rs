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
//! - [`lobby_form`]: the lobby whole (form 26 and its load screens): its events, state and every toolkit and game
//!   call, over the C++ host.
//! - [`lobby_options`]: the lobby's game options (what is shown and editable), the option events' messages and
//!   the start check.
//! - [`splash`]: the loading screen whole (form 23: its open event over the C++ host).
//! - [`single_player`]: the single player screen (form 6): the random map's slot table; the player name check
//!   (`EEIsValidName`) it shares with the multiplayer screens.
//! - [`command_panel`]: the in-game command panel: its layout (which buttons show where), each button's
//!   availability and icon, its tooltip's plan and the targeting modes of its commands.
//! - [`settings_form`]: the game settings form whole (its events, state and every toolkit and engine call, over the
//!   C++ host).
//! - [`campaign_editor`]: the campaign editor's events whole (form 24: its events and its questions' answers over
//!   the C++ host and the editor's C++ model).
//! - [`custom_campaign`]: the custom campaign form whole (its events over the C++ host).
//! - [`civ_builder`]: the civilization builder's events whole (its form's and dialogs' control events, the form's
//!   open and close events, over the C++ builder helpers).
//! - [`civ_choice`]: the in-game civilization choice form whole (form 35: the predefined civilizations, over the C++
//!   host; the posted loads' files by slot).
//! - [`debug_sound`]: the debug sound panel whole (form 22: its sliders' events over the C++ host).
//! - [`dialog`]: the game's message box (`EEUIFError`: each kind's buttons and controls, its layout on open), over
//!   the C++ host.
//! - [`diplomacy`]: the diplomacy form whole (form 27: its events and tribute helpers over the C++ host; the players
//!   as records).
//! - [`post_game`]: the post-game summary whole (form 25: its graphs, bars and stars, the stats server's reports,
//!   over the C++ host; the statistics as records).
//! - [`save_load`]: the load and save screens whole (the in-game save and load forms, the single player and scenario
//!   load forms).
//! - [`scenario_editor`]: the scenario editor's events (its trigger tab's buttons, the object, area, condition,
//!   effect and trigger screens' events), over the editor's C++ objects.
//! - [`scenario_screens`]: the campaign scenario select form and the scenario intro form whole.
//! - [`chat_log`]: the chat log whole (its last 40 lines, saved with the game; the file through the C++ host).
//! - [`chat`]: the in-game chat form and the chat log form's events whole (forms 18 and 29, over the C++ host).
//! - [`factories`]: the UI manager's control-event factories (one table in their registration order) and its
//!   form-event factory.
//! - [`frame`]: the framed form background's draw (`EEUIBGenericBackground`: the tinted panel, the edges, corners
//!   and side ornaments), over the C++ host.
//! - [`hot_keys`]: the main game form's hot key events whole (the control groups, the unit selection keys, the closest
//!   aircraft; the player's units as records).
//! - [`main_game`]: the main game form's events whole (form 11: its buttons' and hot keys' events, the formation
//!   change, over the C++ host).
//! - [`actions`]: the UI thread's action, undo and redo queues (`EEUserInterface`: post, run, undo, redo; the actions
//!   C++ objects held as handles).
//! - [`camera`]: the camera, scroll and zoom events whole (the main game form's and the scenario editor's: the free
//!   camera's turns, the arrow keys, the zoom keys, the wheel, the grab scroll with its per-form record).
//! - [`main_game_options`]: the in-game options menu whole (form 28: its events over the C++ host).
//! - [`menus`]: the out-of-game menus whole (the main menu, the editors' sub-menu, the single player screen): their
//!   events' codes, the control-event table and one dispatcher over the host's toolkit and game calls.
//! - [`editor_actions`]: the scenario editor's actions over a world-edit host: the terrain, elevation and bulldozer
//!   paints' queued areas and runs, the trigger area's rectangle.
//! - [`editor_whole`]: the scenario editor's actions whole in Rust behind one C++ action (`EESEARust`): save, test,
//!   exit, quick save, the tech lists, the rotation, the trigger area, the blank map (the elevation file's heights)
//!   and the random map, the wall and gate placements, the unit placement and move ([`editor_place`]).
//! - [`hud`]: the in-game HUD's status box: the attribute texts (base+bonus), the armor attributes' values,
//!   `Fill`'s attribute icons and bars, `Refresh`'s texts, bars and the carried resource's icon.

pub mod actions;
#[cfg(not(test))]
mod actions_ffi;
pub mod camera;
#[cfg(not(test))]
mod camera_ffi;
pub mod campaign_editor;
#[cfg(not(test))]
mod campaign_editor_ffi;
pub mod chat;
#[cfg(not(test))]
mod chat_ffi;
pub mod chat_log;
#[cfg(not(test))]
mod chat_log_ffi;
pub mod civ_builder;
#[cfg(not(test))]
mod civ_builder_ffi;
pub mod civ_choice;
#[cfg(not(test))]
mod civ_choice_ffi;
pub mod command_panel;
mod command_panel_ffi;
pub mod custom_campaign;
pub mod debug_sound;
#[cfg(not(test))]
mod debug_sound_ffi;
pub mod dialog;
#[cfg(not(test))]
mod dialog_ffi;
pub mod diplomacy;
#[cfg(not(test))]
mod diplomacy_ffi;
pub mod editor_actions;
#[cfg(not(test))]
mod editor_actions_ffi;
pub mod editor_events;
pub mod editor_place;
pub mod editor_whole;
#[cfg(not(test))]
mod editor_whole_ffi;
pub mod factories;
pub mod frame;
#[cfg(not(test))]
mod frame_ffi;
pub mod hot_keys;
#[cfg(not(test))]
mod hot_keys_ffi;
pub mod hud;
mod hud_ffi;
pub mod lobby;
pub mod lobby_form;
#[cfg(not(test))]
mod lobby_form_ffi;
pub mod lobby_options;
pub mod main_game;
#[cfg(not(test))]
mod main_game_ffi;
pub mod main_game_options;
#[cfg(not(test))]
mod main_game_options_ffi;
pub mod menus;
// imports the C++ host's toolkit and game calls (UI Events/EEScreens.cpp), absent from a test binary
#[cfg(not(test))]
mod menus_ffi;
pub mod post_game;
#[cfg(not(test))]
mod post_game_ffi;
pub mod save_load;
#[cfg(not(test))]
mod save_load_ffi;
pub mod scenario_editor;
#[cfg(not(test))]
mod scenario_editor_ffi;
pub mod scenario_screens;
#[cfg(not(test))]
mod scenario_screens_ffi;
pub mod settings;
pub mod settings_form;
#[cfg(not(test))]
mod settings_form_ffi;
pub mod single_player;
mod single_player_ffi;
pub mod splash;
#[cfg(not(test))]
mod splash_ffi;
