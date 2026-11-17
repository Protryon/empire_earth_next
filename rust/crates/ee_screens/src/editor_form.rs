//! The scenario editor's main form whole in Rust (`EEUIFScenarioEditor`, once `Scenario Editor/EEUIFScenarioEditor.cpp`
//! 0x19cc39..0x1a0315): its constructor (the button texture, the edge scroll, the poll, the backgrounds, the mini
//! map, the player combo box, the load text and help labels, the brush group, undo and redo, the seven tab groups,
//! the tab and tool bar buttons, the hide-UI check box, the camera's events and the hot keys), the destructor, the
//! tabs (`SelectTab`, `LeaveTab`, the full view), `Open`, `CopyData`, `DrawBackground`, `UpdateKeyboard`,
//! `UpdateMouse`, the load thread's posts, the selection (`SelectUnit`, `DeselectUnit`, `DeselectFirstUnit`,
//! `ClearSelection`), `SetGameView`, `SetLoadText`, and the trigger-only save's report (Ctrl+S, once
//! `EEUIESaveScenarioTriggersOnly`) over the scenario's records.
//!
//! The form keeps its controls and state in one slot array (`EE_SCREENS_SEFM_*`, atomics: the graphics thread
//! reads the current tab, the panel and the UI's hidden flag while the UI thread runs the form) and its selection as
//! unit ids (`EEUnitHandle`'s ids, resolved by the host on each read). The world it reads and posts to and the
//! controls only it makes are the form's host, `ee_screenscpp_sefm` (ops from 800, `Scenario Editor/
//! EEUIFScenarioEditor.cpp`); the shared control ops (below 100) are `ee_screenscpp_sepl`. The C++ class keeps its
//! civilization (`EECivilization`, the builder's) and forwards; a host call may come back into the form (a tab
//! group's `Enter` reading the form's groups, a check box's event), so no lock is held across one.

use core::ffi::{c_long, c_ulong};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Mutex;

use ee_render::ge::types::GeRectangle;
use ee_scenario::fields::{
    EeScenarioConditionFields, EeScenarioEffectFields, EeScenarioObjectFields, EeScenarioTriggerFields,
};

use crate::editor_players::{display, is_shown, rect, SeplHost};

// ---- the form's slots (`ee_screens_sefm_get` / `_set`)

pub const EE_SCREENS_SEFM_BUTTON_TEXTURE: u32 = 0;
pub const EE_SCREENS_SEFM_EDGE_SCROLL: u32 = 1;
pub const EE_SCREENS_SEFM_POLL: u32 = 2;
pub const EE_SCREENS_SEFM_PANEL_BACKGROUND: u32 = 3;
pub const EE_SCREENS_SEFM_FULL_BACKGROUND: u32 = 4;
pub const EE_SCREENS_SEFM_MINI_MAP: u32 = 5;
pub const EE_SCREENS_SEFM_PLAYER_COMBO: u32 = 6;
pub const EE_SCREENS_SEFM_LOAD_TEXT: u32 = 7;
pub const EE_SCREENS_SEFM_HELP_LABEL: u32 = 8;
pub const EE_SCREENS_SEFM_BRUSH_GROUP: u32 = 9;
pub const EE_SCREENS_SEFM_UNDO: u32 = 10;
pub const EE_SCREENS_SEFM_REDO: u32 = 11;
/// The tab groups, by group (`kEditorTab*`): 12 .. 18.
pub const EE_SCREENS_SEFM_GROUP: u32 = 12;
/// The tab buttons, by tab: 19 .. 25.
pub const EE_SCREENS_SEFM_TAB_BUTTON: u32 = 19;
pub const EE_SCREENS_SEFM_CIV_BUILDER: u32 = 26;
pub const EE_SCREENS_SEFM_LOAD_BUTTON: u32 = 27;
pub const EE_SCREENS_SEFM_TEST_BUTTON: u32 = 28;
pub const EE_SCREENS_SEFM_SAVE_BUTTON: u32 = 29;
pub const EE_SCREENS_SEFM_EXIT_BUTTON: u32 = 30;
pub const EE_SCREENS_SEFM_QUICK_SAVE_BUTTON: u32 = 31;
pub const EE_SCREENS_SEFM_QUICK_LOAD_BUTTON: u32 = 32;
pub const EE_SCREENS_SEFM_HIDE_UI: u32 = 33;
pub const EE_SCREENS_SEFM_NEW_PLAYER_EVENT: u32 = 34;
pub const EE_SCREENS_SEFM_WHEEL_ZOOM_EVENT: u32 = 35;
pub const EE_SCREENS_SEFM_ZOOM_IN_EVENT: u32 = 36;
pub const EE_SCREENS_SEFM_ZOOM_OUT_EVENT: u32 = 37;
pub const EE_SCREENS_SEFM_REVOLVE_EVENT: u32 = 38;
pub const EE_SCREENS_SEFM_ROTATE_EVENT: u32 = 39;
pub const EE_SCREENS_SEFM_DISABLE_MOUSE_EVENT: u32 = 40;
pub const EE_SCREENS_SEFM_ENABLE_MOUSE_EVENT: u32 = 41;
pub const EE_SCREENS_SEFM_GRAB_SCROLL: u32 = 42;
pub const EE_SCREENS_SEFM_LOAD_THREAD: u32 = 43;
pub const EE_SCREENS_SEFM_CURRENT_TAB: u32 = 44;
pub const EE_SCREENS_SEFM_ENTITY_GROUP: u32 = 45;
pub const EE_SCREENS_SEFM_NUM_CARGO_UNITS: u32 = 46;
pub const EE_SCREENS_SEFM_ENTITY_GROUP_ENABLED: u32 = 47;
pub const EE_SCREENS_SEFM_B2B1: u32 = 48;
pub const EE_SCREENS_SEFM_B2B2: u32 = 49;
pub const EE_SCREENS_SEFM_SWITCHING: u32 = 50;
pub const EE_SCREENS_SEFM_SUSPENDED: u32 = 51;
pub const EE_SCREENS_SEFM_UI_HIDDEN: u32 = 52;
pub const EE_SCREENS_SEFM_WAS_UI_HIDDEN: u32 = 53;
pub const EE_SCREENS_SEFM_EXIT_AFTER_SAVE: u32 = 54;
/// The view rectangle saved by `Open` (left, top, right, bottom as `f32` bits): 55 .. 58.
pub const EE_SCREENS_SEFM_SAVED_VIEW: u32 = 55;
/// The bottom panel's rectangle (`mR1F0`, set by `CreateBackground`; `f32` bits): 59 .. 62.
pub const EE_SCREENS_SEFM_PANEL: u32 = 59;
const SLOTS: usize = 63;

// ---- the form's members (`ee_screens_sefm_call`)

pub const EE_SCREENS_SEFM_BUILD: u32 = 1;
pub const EE_SCREENS_SEFM_SELECT_TAB: u32 = 2;
pub const EE_SCREENS_SEFM_LEAVE_TAB: u32 = 3;
pub const EE_SCREENS_SEFM_ENTER_FULL_VIEW: u32 = 4;
pub const EE_SCREENS_SEFM_LEAVE_FULL_VIEW: u32 = 5;
pub const EE_SCREENS_SEFM_EXIT_EDITOR: u32 = 6;
pub const EE_SCREENS_SEFM_STORE_GROUP: u32 = 7;
pub const EE_SCREENS_SEFM_RESUME_GROUP: u32 = 8;
pub const EE_SCREENS_SEFM_POST_LOAD_EVENT: u32 = 9;
pub const EE_SCREENS_SEFM_POST_SHOW_LOADED: u32 = 10;
pub const EE_SCREENS_SEFM_OPEN: u32 = 11;
pub const EE_SCREENS_SEFM_COPY_DATA: u32 = 12;
pub const EE_SCREENS_SEFM_DRAW_BACKGROUND: u32 = 13;
pub const EE_SCREENS_SEFM_UPDATE_KEYBOARD: u32 = 14;
pub const EE_SCREENS_SEFM_UPDATE_MOUSE: u32 = 15;
pub const EE_SCREENS_SEFM_SELECT_UNIT: u32 = 16;
pub const EE_SCREENS_SEFM_DESELECT_UNIT: u32 = 17;
pub const EE_SCREENS_SEFM_DESELECT_FIRST: u32 = 18;
pub const EE_SCREENS_SEFM_CLEAR_SELECTION: u32 = 19;
pub const EE_SCREENS_SEFM_SET_GAME_VIEW: u32 = 20;
pub const EE_SCREENS_SEFM_SET_LOAD_TEXT: u32 = 21;
pub const EE_SCREENS_SEFM_SAVE_TRIGGERS: u32 = 22;

// ---- the form's host (`ee_screenscpp_sefm`)

/// The button texture (`textures\SE Buttons`, referenced), the edge scroll, the poll (500, started), the panel's
/// bitmap (`textures\scenpart`) and the full view's background (`textures\Scenfull`) made into the five words at p0.
pub const EE_SCREENS_SEFM_OP_BASICS: u32 = 800;
/// The mini map at (f0, f1), f2 square, the control index i0; group 0, hidden -> into p0.
pub const EE_SCREENS_SEFM_OP_MINI_MAP: u32 = 801;
/// The player combo box (0x149, 140 x 20 at the top left) at the index i0 with its selection's new player event
/// (`EE_SCREENS_SE_NEW_PLAYER`); group 0, hidden -> the box into p0, the event into p1.
pub const EE_SCREENS_SEFM_OP_PLAYER_COMBO: u32 = 802;
/// The load text label (i0 = 0: id 9, right aligned, at (500, 0)) or the help label (i0 = 1: id 0x10, the form's
/// tool tips on, 600 ms) at the index i1; group 0, hidden -> into p0.
pub const EE_SCREENS_SEFM_OP_LABEL: u32 = 803;
/// The brush group at (f0, f1) from the index *p1 (advanced) -> into p0.
pub const EE_SCREENS_SEFM_OP_BRUSH_GROUP: u32 = 804;
/// The undo (i0 = 0) or redo (1) button at (f0, f1), the index i1, its text white -> into p0.
pub const EE_SCREENS_SEFM_OP_UNDO_REDO: u32 = 805;
/// The tab group i0 (`kEditorTab*`) made from the index *p1 (advanced) -> into p0 (NULL for another group).
pub const EE_SCREENS_SEFM_OP_TAB_GROUP: u32 = 806;
/// A button from the button texture (`EeScreensSefmButton` at p1) -> into p0.
pub const EE_SCREENS_SEFM_OP_BUTTON: u32 = 807;
/// The quick save/load action (`EESEAQuickSaveLoad` over the scenario extension) -> into p0.
pub const EE_SCREENS_SEFM_OP_QUICK_ACTION: u32 = 808;
/// The hide-UI check box (its `EESEERust` hide-UI event on both changes) at the index i0; group 0, hidden -> p0.
pub const EE_SCREENS_SEFM_OP_HIDE_UI: u32 = 809;
/// The form's control group i0 enabled (i1 = 1) or disabled.
pub const EE_SCREENS_SEFM_OP_GROUP_ENABLE: u32 = 810;
/// A screen event of code i0 (`EECreateScreenEvent`) / an `EESEERust` of kind i0 over the object p1 -> into p0.
pub const EE_SCREENS_SEFM_OP_SCREEN_EVENT: u32 = 811;
pub const EE_SCREENS_SEFM_OP_SEE_EVENT: u32 = 812;
/// The form's mouse wheel event (i0 = 0) or middle button's pressed and unpressed events (i0 = 1) set to p0.
pub const EE_SCREENS_SEFM_OP_MOUSE_EVENT: u32 = 813;
/// A hot key (`UIHotKey`) on the key i0 (`KEY_*`) with the just-pressed, pressed and just-unpressed events (three
/// words at p0) added to the form p1's group 0.
pub const EE_SCREENS_SEFM_OP_HOT_KEY: u32 = 814;
/// The form p1's `AddHotKey(key i0, modifiers i1 and i2, none, the event p0 in event slot i3 & 3, group i3 >> 4,
/// false)`.
pub const EE_SCREENS_SEFM_OP_ADD_HOT_KEY: u32 = 815;
/// The grab scroll (`EECreateGrabScroll(1)`) -> into p0 / released (`EEReleaseGrabScroll(1)`).
pub const EE_SCREENS_SEFM_OP_GRAB_SCROLL: u32 = 816;
pub const EE_SCREENS_SEFM_OP_RELEASE_GRAB_SCROLL: u32 = 817;
/// The tab button p0 checked (i0 = 0: `Check`, 1: `CheckDirect`).
pub const EE_SCREENS_SEFM_OP_TAB_CHECK: u32 = 818;
/// p0 dereferenced as an event (i0 = 0), a texture (1), a bitmap (2), an action (3) or the edge scroll (4).
pub const EE_SCREENS_SEFM_OP_DEREFERENCE: u32 = 819;
/// p0 deleted as a control (i0 = 0), a tab group (1), the poll (2), the brush group (3) or the full background (4).
pub const EE_SCREENS_SEFM_OP_DELETE: u32 = 820;
/// The tab group p0's member i0 (`EE_SCREENS_SEFM_GROUP_*`) with the point (f0, f1) or rectangle f.
pub const EE_SCREENS_SEFM_OP_GROUP_CALL: u32 = 821;
/// The game's render list set to its base object (i0 = 1, the full view) or every object (0).
pub const EE_SCREENS_SEFM_OP_RENDER_LIST: u32 = 822;
/// The manager redraws everything and the current form's controls are made dirty.
pub const EE_SCREENS_SEFM_OP_REDRAW_ALL: u32 = 823;
/// The brush group p0's highlight shown (i0 = 1) or not.
pub const EE_SCREENS_SEFM_OP_HIGHLIGHT: u32 = 824;
/// The unit of id i0 (`EEUnitHandleResolve`) -> into p0 / the unit p0's id (`EEUnitHandleID`).
pub const EE_SCREENS_SEFM_OP_RESOLVE: u32 = 825;
pub const EE_SCREENS_SEFM_OP_UNIT_ID: u32 = 826;
/// The unit p0's editor selection flag set to i0, then referenced (i1 = 1) or dereferenced (i1 = -1).
pub const EE_SCREENS_SEFM_OP_UNIT_SELECT: u32 = 827;
/// -> whether the unit p0 is placed and has cargo room or cargo.
pub const EE_SCREENS_SEFM_OP_UNIT_CARGO: u32 = 828;
/// The unit tab p0's `ShowCargo(i0)` / `ShowUnitStatus(p1)`.
pub const EE_SCREENS_SEFM_OP_SHOW_CARGO: u32 = 829;
pub const EE_SCREENS_SEFM_OP_SHOW_STATUS: u32 = 830;
/// The game view set to f (`EESetGameViewRectangle`).
pub const EE_SCREENS_SEFM_OP_GAME_VIEW: u32 = 831;
/// The load thread: made over the form (i0 = 0, into p0), or p1's post i0 (`EE_SCREENS_SEFM_POST_*`) with p2.
pub const EE_SCREENS_SEFM_OP_LOAD_THREAD: u32 = 832;
/// -> whether the user interface is in the scenario editor; the world's active units rebuilt first (i0 = 1).
pub const EE_SCREENS_SEFM_OP_TEST_PREPARE: u32 = 833;
/// `Open`'s camera: the editor camera mode, the units refreshed, no sight updates, the editor's turn.
pub const EE_SCREENS_SEFM_OP_OPEN_CAMERA: u32 = 834;
/// The full background p0's textures loaded.
pub const EE_SCREENS_SEFM_OP_LOAD_BACKGROUND: u32 = 835;
/// `Open`'s world: the fade in, the player tab p0 refreshed, the music and ambient sounds paused, the edge scroll
/// p1's edges the window, the free camera, the view rectangle saved into f, the editor's game view, the mini map's
/// view.
pub const EE_SCREENS_SEFM_OP_OPEN_WORLD: u32 = 836;
/// `Open`'s end: the scenario's camera move off, the mini map redrawn, `UIForm::Open`.
pub const EE_SCREENS_SEFM_OP_OPEN_FORM: u32 = 837;
/// The help label p0 made the form's help text label.
pub const EE_SCREENS_SEFM_OP_HELP_LABEL: u32 = 838;
/// The editor's exit action posted.
pub const EE_SCREENS_SEFM_OP_POST_EXIT: u32 = 839;
/// `CopyData`'s world: the view rectangle (less a pixel) made dirty (i0 = 1), the mini map p0 dirty, the
/// placement's ghost at the mouse.
pub const EE_SCREENS_SEFM_OP_COPY_WORLD: u32 = 840;
/// -> whether a control holds the mouse (i0 = 0) or the keyboard (1).
pub const EE_SCREENS_SEFM_OP_CAPTURED: u32 = 841;
/// The edge scroll p0 updated at the mouse.
pub const EE_SCREENS_SEFM_OP_EDGE_SCROLL: u32 = 842;
/// The full background p0 drawn in f -> its result / the panel's bitmap p0 drawn on the form p1 clipped to f into
/// the panel (four floats at p2).
pub const EE_SCREENS_SEFM_OP_DRAW_FULL: u32 = 843;
pub const EE_SCREENS_SEFM_OP_DRAW_PANEL: u32 = 844;
/// -> the key i0 (`KEY_*`): 1 just pressed, 2 held (its value).
pub const EE_SCREENS_SEFM_OP_KEY: u32 = 845;
/// The form's control i0 fires its button-up event (if there is one).
pub const EE_SCREENS_SEFM_OP_FIRE: u32 = 846;
/// The terrain's grid mode stepped (backwards with i0 = 1).
pub const EE_SCREENS_SEFM_OP_GRID: u32 = 847;
/// -> the player tab p0's player count / the combo box p0's selection set to i0.
pub const EE_SCREENS_SEFM_OP_PLAYER_COUNT: u32 = 848;
pub const EE_SCREENS_SEFM_OP_COMBO_SELECT: u32 = 849;
/// The label p0's text set to the `UWideString` p1.
pub const EE_SCREENS_SEFM_OP_LABEL_TEXT: u32 = 850;
/// -> the mouse's location on the form into f0, f1.
pub const EE_SCREENS_SEFM_OP_MOUSE: u32 = 851;
/// -> the scenario's list i0 (`LIST_*`) count / its record i1's numbers (its `EeScenario*Fields`) into p0, NULL
/// for none.
pub const EE_SCREENS_SEFM_OP_SM_COUNT: u32 = 852;
pub const EE_SCREENS_SEFM_OP_SM_RECORD: u32 = 853;
/// -> the bytes of the list i0's record i1's name (i2 = 0) or description (1) into the p0 buffer of i3 bytes: the
/// length (more than i3: call again), -1 for no record.
pub const EE_SCREENS_SEFM_OP_SM_TEXT: u32 = 854;
/// -> the length of the record i1's vector i0 (`VEC_*`) / its element i2 into i0 .. i2.
pub const EE_SCREENS_SEFM_OP_SM_VEC_LEN: u32 = 855;
pub const EE_SCREENS_SEFM_OP_SM_VEC_AT: u32 = 856;
/// The scenario's triggers saved to ScenarioTriggers.scn -> the open's result / the report's i0 bytes at p0
/// written to ScenarioTriggers.txt (a text file).
pub const EE_SCREENS_SEFM_OP_REPORT_SAVE: u32 = 857;
pub const EE_SCREENS_SEFM_OP_REPORT_WRITE: u32 = 858;

/// A tab group's members (`EE_SCREENS_SEFM_OP_GROUP_CALL`).
pub const EE_SCREENS_SEFM_GROUP_UPDATE: i32 = 0;
pub const EE_SCREENS_SEFM_GROUP_LEAVE: i32 = 1;
pub const EE_SCREENS_SEFM_GROUP_DRAW: i32 = 2;
pub const EE_SCREENS_SEFM_GROUP_DISABLE: i32 = 3;
pub const EE_SCREENS_SEFM_GROUP_LOAD: i32 = 4;
pub const EE_SCREENS_SEFM_GROUP_ENTER: i32 = 5;
pub const EE_SCREENS_SEFM_GROUP_STORE: i32 = 6;
pub const EE_SCREENS_SEFM_GROUP_ENABLE: i32 = 7;
pub const EE_SCREENS_SEFM_GROUP_MOUSE_MOVE: i32 = 8;
pub const EE_SCREENS_SEFM_GROUP_UPDATE_KEYS: i32 = 9;

/// The load thread's posts (`EE_SCREENS_SEFM_OP_LOAD_THREAD`).
pub const EE_SCREENS_SEFM_POST_401: i32 = 1;
pub const EE_SCREENS_SEFM_POST_404: i32 = 2;
pub const EE_SCREENS_SEFM_POST_405: i32 = 3;
pub const EE_SCREENS_SEFM_POST_40B: i32 = 4;
pub const EE_SCREENS_SEFM_POST_40C: i32 = 5;

/// The scenario's lists (`EE_SCREENS_SEFM_OP_SM_*`).
pub const EE_SCREENS_SEFM_LIST_TRIGGERS: i32 = 0;
pub const EE_SCREENS_SEFM_LIST_CONDITIONS: i32 = 1;
pub const EE_SCREENS_SEFM_LIST_EFFECTS: i32 = 2;
pub const EE_SCREENS_SEFM_LIST_OBJECTS: i32 = 3;
pub const EE_SCREENS_SEFM_LIST_AREAS: i32 = 4;
/// A record's vectors: a trigger's condition wrappers (not, condition, operator), its effect wrappers (delay,
/// effect, operator), an effect's objects.
pub const EE_SCREENS_SEFM_VEC_CONDITIONS: i32 = 0;
pub const EE_SCREENS_SEFM_VEC_EFFECTS: i32 = 1;
pub const EE_SCREENS_SEFM_VEC_OBJECTS: i32 = 2;

/// The keys the form reads or binds (the host's table of `kKey*`).
pub const EE_SCREENS_SEFM_KEY_SEMICOLON: i32 = 0;
pub const EE_SCREENS_SEFM_KEY_SLASH: i32 = 1;
pub const EE_SCREENS_SEFM_KEY_T: i32 = 2;
pub const EE_SCREENS_SEFM_KEY_SHIFT: i32 = 3;
pub const EE_SCREENS_SEFM_KEY_ALT: i32 = 4;
/// `1` .. `8`: 5 .. 12.
pub const EE_SCREENS_SEFM_KEY_1: i32 = 5;
pub const EE_SCREENS_SEFM_KEY_RBRACKET: i32 = 13;
pub const EE_SCREENS_SEFM_KEY_LBRACKET: i32 = 14;
pub const EE_SCREENS_SEFM_KEY_GRAVE: i32 = 15;
pub const EE_SCREENS_SEFM_KEY_HOME: i32 = 16;
pub const EE_SCREENS_SEFM_KEY_S: i32 = 17;
pub const EE_SCREENS_SEFM_KEY_L: i32 = 18;
pub const EE_SCREENS_SEFM_KEY_TAB: i32 = 19;
pub const EE_SCREENS_SEFM_KEY_D: i32 = 20;
pub const EE_SCREENS_SEFM_KEY_CONTROL: i32 = 21;
pub const EE_SCREENS_SEFM_KEY_NONE: i32 = 22;
pub const EE_SCREENS_SEFM_KEY_COUNT: i32 = 23;

/// A button the form makes from its button texture (`EE_SCREENS_SEFM_OP_BUTTON`): a tab button (`EEUIFSEMainButton`,
/// kind 0: id, flag, tab, checked; the normal and pressed corners, `rect`'s top left its location) or a plain
/// button (`UIButton`, kind 1: the normal and pressed sources 32 x 32 stretched over `dest`, the control `rect`, its
/// button-up event a screen event of code `event` or, with `object`, an `EESEERust` of kind `event` over it); help
/// string `help`, control index `index`, added to group 0.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScreensSefmButton {
    pub kind: u32,
    pub id: u32,
    pub index: u32,
    pub help: u32,
    pub flag: u32,
    pub tab: i32,
    pub checked: u32,
    pub event: u32,
    pub object: usize,
    pub normal: [f32; 2],
    pub pressed: [f32; 2],
    pub dest: [f32; 4],
    pub rect: [f32; 4],
}

/// The trigger-only save event's kind (`EESEERust`: Ctrl+S runs `EE_SCREENS_SEFM_SAVE_TRIGGERS`).
pub const EE_SCREENS_SEE_SAVE_TRIGGERS_ONLY: u32 = 17;

/// One editor tab (0x4c46d0): its group, its order along the bottom, its help string, the normal and pressed corners
/// in the button texture, its control group, whether it is a full view and whether it paints with the brush.
#[derive(Clone, Copy, Debug)]
pub struct EditorTab {
    pub group: u32,
    pub order: u32,
    pub help: u32,
    pub normal: [f32; 2],
    pub pressed: [f32; 2],
    pub flag: u32,
    pub full_view: bool,
    pub brush: bool,
}

const fn tab(
    group: u32,
    help: u32,
    normal: [f32; 2],
    pressed: [f32; 2],
    flag: u32,
    full_view: bool,
    brush: bool,
) -> EditorTab {
    EditorTab { group, order: group, help, normal, pressed, flag, full_view, brush }
}

/// `sEditorTabs` (0x4c46d0): map, elevation, terrain, units, triggers, players, instructions.
pub const EDITOR_TABS: [EditorTab; 7] = [
    tab(0, 0x213a, [0.0, 50.0], [32.0, 50.0], 1, false, false),
    tab(1, 0x213b, [128.0, 50.0], [160.0, 50.0], 2, false, true),
    tab(2, 0x213c, [64.0, 50.0], [96.0, 50.0], 3, false, true),
    tab(3, 0x213d, [0.0, 82.0], [32.0, 82.0], 6, false, false),
    tab(4, 0x213e, [192.0, 50.0], [224.0, 50.0], 14, false, false),
    tab(5, 0x1b61, [64.0, 82.0], [96.0, 82.0], 4, true, false),
    tab(6, 0x2143, [128.0, 82.0], [160.0, 82.0], 7, true, false),
];

/// One tool bar button (0x4c47b0): its id, its location, the normal and pressed corners, its help string.
#[derive(Clone, Copy, Debug)]
pub struct ToolbarButton {
    pub id: u32,
    pub x: u32,
    pub y: u32,
    pub normal: [u32; 2],
    pub pressed: [u32; 2],
    pub help: u32,
}

const fn tool(id: u32, x: u32, normal: [u32; 2], pressed: [u32; 2], help: u32) -> ToolbarButton {
    ToolbarButton { id, x, y: 0x2e0, normal, pressed, help }
}

/// `sToolbarButtons`: quick load, quick save, save, load, test, exit.
pub const TOOLBAR_BUTTONS: [ToolbarButton; 6] = [
    tool(0xc, 0x287, [0x80, 0x72], [0xa0, 0x72], 0x2140),
    tool(0xd, 0x2aa, [0x00, 0xc4], [0x20, 0xc4], 0x2141),
    tool(0xb, 0x2cd, [0x40, 0x72], [0x60, 0x72], 0x1b59),
    tool(0xa, 0x2f0, [0x00, 0x72], [0x20, 0x72], 0x1b58),
    tool(0xe, 0x313, [0x40, 0xc4], [0x60, 0xc4], 0x2142),
    tool(0xf, 0x336, [0x80, 0xc4], [0xa0, 0xc4], 0x213f),
];

const TOOLBAR_LOAD: u32 = 10;
const TOOLBAR_SAVE: u32 = 11;
const TOOLBAR_QUICK_LOAD: u32 = 12;
const TOOLBAR_QUICK_SAVE: u32 = 13;
const TOOLBAR_TEST: u32 = 14;

/// The mini map's size and the screen height (0x444f88, 0x44507c).
const MINI_MAP_SIZE: f32 = 169.84;
const SCREEN_HEIGHT: f32 = 768.0;
/// The bottom panel (`CreateBackground`).
pub const PANEL: [f32; 4] = [0.0, 606.0, 1023.0, 767.0];
/// The civilization builder's button's id and help string, its normal and pressed sources.
const CIV_BUILDER_ID: u32 = 17;
pub const EE_SCREENS_SEFM_IDS_CIVILIZATION_BUILDER_TOOL: u32 = 8537;
/// The quick load and save events' `EESEERust` kinds (`kSEEQuickLoad`, `kSEEQuickSave`).
const SEE_QUICK_LOAD: u32 = 3;
const SEE_QUICK_SAVE: u32 = 4;
/// What `EE_SCREENS_SEFM_OP_DEREFERENCE` releases.
const DEREF_EVENT: i32 = 0;
const DEREF_TEXTURE: i32 = 1;
const DEREF_BITMAP: i32 = 2;
const DEREF_ACTION: i32 = 3;
const DEREF_SCROLL: i32 = 4;
/// The editor's control group 0 (the form's own controls).
const GROUP_FORM: i32 = 0;

// the scenario's records (`ee/EEScenarioIDs.h`; the host static_asserts them)
const CONDITION_OBJECT: c_ulong = 1;
const CONDITION_PLAYER_ATTRIBUTE: c_ulong = 2;
const EFFECT_NONE: c_ulong = 0;
const EFFECT_OBJECT: c_ulong = 1;
const EFFECT_PLAYER: c_ulong = 2;
const OBJECT_ACTION_CLASS_NAME: c_ulong = 10;
const OBJECT_ACTION_CLASS_ATTRIBUTE: c_ulong = 12;
const PLAYER_ACTION_MODIFY_TECH_TREE: c_ulong = 5;
const OPERATOR_AND: i32 = 1;
const OPERATOR_OR: i32 = 2;

fn op<H: SeplHost + ?Sized>(h: &mut H, op: u32, p: [usize; 3], i: [i32; 4], f: [f32; 4]) -> i32 {
    let (mut i, mut f) = (i, f);
    h.op(op, p, &mut i, &mut f)
}

/// An op whose result is a pointer written to p0.
fn make<H: SeplHost + ?Sized>(h: &mut H, code: u32, p: [usize; 2], i: [i32; 4], f: [f32; 4]) -> usize {
    let mut out = 0usize;
    op(h, code, [&mut out as *mut usize as usize, p[0], p[1]], i, f);
    out
}

fn bits(r: [f32; 4]) -> [usize; 4] {
    r.map(|x| x.to_bits() as usize)
}

fn ge(r: [f32; 4]) -> GeRectangle {
    GeRectangle { left: r[0], top: r[1], right: r[2], bottom: r[3] }
}

/// The editor form.
pub struct EditorForm {
    /// the C++ `EEUIFScenarioEditor`
    shell: usize,
    slots: [AtomicUsize; SLOTS],
    /// the selected units' ids (`mSelectedUnits`, in its order)
    selected: Mutex<Vec<u32>>,
}

impl EditorForm {
    pub fn new(shell: usize) -> Self {
        EditorForm { shell, slots: core::array::from_fn(|_| AtomicUsize::new(0)), selected: Mutex::new(Vec::new()) }
    }

    pub fn get(&self, slot: u32) -> usize {
        self.slots.get(slot as usize).map_or(0, |s| s.load(Ordering::Relaxed))
    }

    pub fn set(&self, slot: u32, value: usize) {
        if let Some(s) = self.slots.get(slot as usize) {
            s.store(value, Ordering::Relaxed);
        }
    }

    fn flag(&self, slot: u32) -> bool {
        self.get(slot) != 0
    }

    fn set_flag(&self, slot: u32, value: bool) {
        self.set(slot, usize::from(value));
    }

    fn current_tab(&self) -> &'static EditorTab {
        &EDITOR_TABS[self.get(EE_SCREENS_SEFM_CURRENT_TAB).min(6)]
    }

    /// The current tab's group.
    fn group(&self) -> usize {
        self.get(EE_SCREENS_SEFM_GROUP + self.current_tab().group)
    }

    fn group_call<H: SeplHost + ?Sized>(&self, h: &mut H, which: i32, f: [f32; 4]) {
        let g = self.group();
        op(h, EE_SCREENS_SEFM_OP_GROUP_CALL, [g, 0, 0], [which, 0, 0, 0], f);
    }

    fn unit_screen(&self) -> usize {
        self.get(EE_SCREENS_SEFM_GROUP + 3)
    }

    /// The saved view rectangle (`mR1E0`).
    pub fn saved_view(&self) -> [f32; 4] {
        core::array::from_fn(|k| f32::from_bits(self.get(EE_SCREENS_SEFM_SAVED_VIEW + k as u32) as u32))
    }

    pub fn selected_count(&self) -> usize {
        self.selected.lock().map_or(0, |s| s.len())
    }

    /// The selected unit `index`'s id (`EEUnitHandle`'s `kNone` past the end).
    pub fn selected_id(&self, index: usize) -> u32 {
        self.selected.lock().ok().and_then(|s| s.get(index).copied()).unwrap_or(u32::MAX)
    }

    fn resolve<H: SeplHost + ?Sized>(h: &mut H, id: u32) -> usize {
        make(h, EE_SCREENS_SEFM_OP_RESOLVE, [0, 0], [id as i32, 0, 0, 0], [0.0; 4])
    }

    fn first_unit<H: SeplHost + ?Sized>(&self, h: &mut H) -> usize {
        match self.selected.lock().ok().and_then(|s| s.first().copied()) {
            Some(id) => Self::resolve(h, id),
            None => 0,
        }
    }

    // ---- the constructor

    /// The constructor's controls and events (after `SetRectangle`; the civilization and the music name are the
    /// shell's), the map tab checked last.
    pub fn build<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let mut index: u32 = 0;
        let mut basics = [0usize; 5];
        op(h, EE_SCREENS_SEFM_OP_BASICS, [basics.as_mut_ptr() as usize, self.shell, 0], [0; 4], [0.0; 4]);
        for (k, slot) in [
            EE_SCREENS_SEFM_BUTTON_TEXTURE,
            EE_SCREENS_SEFM_EDGE_SCROLL,
            EE_SCREENS_SEFM_POLL,
            EE_SCREENS_SEFM_PANEL_BACKGROUND,
            EE_SCREENS_SEFM_FULL_BACKGROUND,
        ]
        .into_iter()
        .enumerate()
        {
            self.set(slot, basics[k]);
        }
        self.set_flag(EE_SCREENS_SEFM_ENTITY_GROUP_ENABLED, false);
        self.set_flag(EE_SCREENS_SEFM_B2B1, false);
        self.set(EE_SCREENS_SEFM_ENTITY_GROUP, 0);
        self.set_flag(EE_SCREENS_SEFM_B2B2, false);
        self.set(EE_SCREENS_SEFM_LOAD_THREAD, 0);
        for (k, v) in bits(PANEL).into_iter().enumerate() {
            self.set(EE_SCREENS_SEFM_PANEL + k as u32, v);
        }

        let mini = [1024.0 - MINI_MAP_SIZE, SCREEN_HEIGHT - MINI_MAP_SIZE, MINI_MAP_SIZE, 0.0];
        let m = make(h, EE_SCREENS_SEFM_OP_MINI_MAP, [self.shell, 0], [index as i32, 0, 0, 0], mini);
        self.set(EE_SCREENS_SEFM_MINI_MAP, m);
        index += 1;

        let mut event = 0usize;
        let combo = make(
            h,
            EE_SCREENS_SEFM_OP_PLAYER_COMBO,
            [self.shell, &mut event as *mut usize as usize],
            [index as i32, 0, 0, 0],
            [0.0; 4],
        );
        self.set(EE_SCREENS_SEFM_PLAYER_COMBO, combo);
        self.set(EE_SCREENS_SEFM_NEW_PLAYER_EVENT, event);
        index += 1;
        let load = make(h, EE_SCREENS_SEFM_OP_LABEL, [self.shell, 0], [0, index as i32, 0, 0], [0.0; 4]);
        self.set(EE_SCREENS_SEFM_LOAD_TEXT, load);
        index += 1;
        let help = make(h, EE_SCREENS_SEFM_OP_LABEL, [self.shell, 0], [1, index as i32, 0, 0], [0.0; 4]);
        self.set(EE_SCREENS_SEFM_HELP_LABEL, help);
        index += 1;

        self.create_panels(h, &mut index);
        self.create_tab_buttons(h, &mut index);
        self.create_toolbar_buttons(h, &mut index);

        let hide = make(h, EE_SCREENS_SEFM_OP_HIDE_UI, [self.shell, 0], [index as i32, 0, 0, 0], [0.0; 4]);
        self.set(EE_SCREENS_SEFM_HIDE_UI, hide);
        index += 1;
        let _ = index;
        op(h, EE_SCREENS_SEFM_OP_GROUP_ENABLE, [0, self.shell, 0], [GROUP_FORM, 1, 0, 0], [0.0; 4]);

        self.create_camera_keys(h);

        self.set_flag(EE_SCREENS_SEFM_SWITCHING, false);
        self.set_flag(EE_SCREENS_SEFM_SUSPENDED, false);
        self.set_flag(EE_SCREENS_SEFM_UI_HIDDEN, false);
        self.set(EE_SCREENS_SEFM_NUM_CARGO_UNITS, 0);
        self.set_flag(EE_SCREENS_SEFM_EXIT_AFTER_SAVE, false);
        let map_button = self.get(EE_SCREENS_SEFM_TAB_BUTTON);
        op(h, EE_SCREENS_SEFM_OP_TAB_CHECK, [map_button, 0, 0], [0; 4], [0.0; 4]);
    }

    fn screen_event<H: SeplHost + ?Sized>(h: &mut H, code: u32) -> usize {
        make(h, EE_SCREENS_SEFM_OP_SCREEN_EVENT, [0, 0], [code as i32, 0, 0, 0], [0.0; 4])
    }

    /// The camera's events and hot keys, the trigger-only save and load, the grab scroll, the debug hot keys.
    fn create_camera_keys<H: SeplHost + ?Sized>(&self, h: &mut H) {
        use crate::camera::*;
        use crate::main_game::{
            EE_SCREENS_MG_DUMP_MEDIA_EDITOR, EE_SCREENS_MG_DUMP_MEDIA_EDITOR_ALL, EE_SCREENS_MG_TOGGLE_HIDDEN,
        };
        use crate::scenario_editor::{EE_SCREENS_SE_HOME_ROTATION, EE_SCREENS_SE_LOAD_TRIGGERS_ONLY};
        let shell = self.shell;
        let wheel = Self::screen_event(h, EE_SCREENS_CAM_WHEEL_ZOOM);
        self.set(EE_SCREENS_SEFM_WHEEL_ZOOM_EVENT, wheel);
        op(h, EE_SCREENS_SEFM_OP_MOUSE_EVENT, [wheel, shell, 0], [0; 4], [0.0; 4]);
        let zoom_in = Self::screen_event(h, EE_SCREENS_CAM_ZOOM_IN);
        self.set(EE_SCREENS_SEFM_ZOOM_IN_EVENT, zoom_in);
        let hot = |h: &mut H, key: i32, events: [usize; 3]| {
            op(h, EE_SCREENS_SEFM_OP_HOT_KEY, [events.as_ptr() as usize, shell, 0], [key, 0, 0, 0], [0.0; 4]);
        };
        hot(h, EE_SCREENS_SEFM_KEY_RBRACKET, [0, zoom_in, 0]);
        let zoom_out = Self::screen_event(h, EE_SCREENS_CAM_ZOOM_OUT);
        self.set(EE_SCREENS_SEFM_ZOOM_OUT_EVENT, zoom_out);
        hot(h, EE_SCREENS_SEFM_KEY_LBRACKET, [0, zoom_out, 0]);
        let disable = Self::screen_event(h, EE_SCREENS_CAM_DISABLE_MOUSE_MOVE);
        let enable = Self::screen_event(h, EE_SCREENS_CAM_ENABLE_MOUSE_MOVE);
        let revolve = Self::screen_event(h, EE_SCREENS_CAM_REVOLVE);
        let rotate = Self::screen_event(h, EE_SCREENS_CAM_ROTATE);
        self.set(EE_SCREENS_SEFM_DISABLE_MOUSE_EVENT, disable);
        self.set(EE_SCREENS_SEFM_ENABLE_MOUSE_EVENT, enable);
        self.set(EE_SCREENS_SEFM_REVOLVE_EVENT, revolve);
        self.set(EE_SCREENS_SEFM_ROTATE_EVENT, rotate);
        hot(h, EE_SCREENS_SEFM_KEY_GRAVE, [disable, rotate, enable]);
        let home = Self::screen_event(h, EE_SCREENS_SE_HOME_ROTATION);
        hot(h, EE_SCREENS_SEFM_KEY_HOME, [home, 0, 0]);
        op(h, EE_SCREENS_SEFM_OP_DEREFERENCE, [home, 0, 0], [DEREF_EVENT, 0, 0, 0], [0.0; 4]);

        let add = |h: &mut H, key: i32, mods: [i32; 2], slot: i32, group: i32, event: usize| {
            op(
                h,
                EE_SCREENS_SEFM_OP_ADD_HOT_KEY,
                [event, shell, 0],
                [key, mods[0], mods[1], slot | (group << 4)],
                [0.0; 4],
            );
        };
        let ctrl = [EE_SCREENS_SEFM_KEY_CONTROL, EE_SCREENS_SEFM_KEY_NONE];
        let save = make(
            h,
            EE_SCREENS_SEFM_OP_SEE_EVENT,
            [0, 0],
            [EE_SCREENS_SEE_SAVE_TRIGGERS_ONLY as i32, 0, 0, 0],
            [0.0; 4],
        );
        add(h, EE_SCREENS_SEFM_KEY_S, ctrl, 2, 0, save);
        let load = Self::screen_event(h, EE_SCREENS_SE_LOAD_TRIGGERS_ONLY);
        add(h, EE_SCREENS_SEFM_KEY_L, ctrl, 2, 0, load);
        let grab = make(h, EE_SCREENS_SEFM_OP_GRAB_SCROLL, [0, 0], [0; 4], [0.0; 4]);
        self.set(EE_SCREENS_SEFM_GRAB_SCROLL, grab);
        op(h, EE_SCREENS_SEFM_OP_MOUSE_EVENT, [grab, shell, 0], [1, 0, 0, 0], [0.0; 4]);
        let toggle = Self::screen_event(h, EE_SCREENS_MG_TOGGLE_HIDDEN);
        add(h, EE_SCREENS_SEFM_KEY_TAB, [EE_SCREENS_SEFM_KEY_NONE; 2], 0, 0, toggle);
        let dump = Self::screen_event(h, EE_SCREENS_MG_DUMP_MEDIA_EDITOR);
        add(h, EE_SCREENS_SEFM_KEY_D, ctrl, 2, 1, dump);
        let dump_all = Self::screen_event(h, EE_SCREENS_MG_DUMP_MEDIA_EDITOR_ALL);
        add(h, EE_SCREENS_SEFM_KEY_D, [EE_SCREENS_SEFM_KEY_CONTROL, EE_SCREENS_SEFM_KEY_SHIFT], 2, 1, dump_all);
    }

    /// `CreatePanels`: the brush group, the undo and redo buttons, the seven tab groups.
    fn create_panels<H: SeplHost + ?Sized>(&self, h: &mut H, index: &mut u32) {
        let shell = self.shell;
        let idx = index as *mut u32 as usize;
        let brush = make(h, EE_SCREENS_SEFM_OP_BRUSH_GROUP, [shell, idx], [0; 4], [5.0, 616.0, 0.0, 0.0]);
        self.set(EE_SCREENS_SEFM_BRUSH_GROUP, brush);
        let undo = make(h, EE_SCREENS_SEFM_OP_UNDO_REDO, [shell, 0], [0, *index as i32, 0, 0], [5.0, 718.0, 0.0, 0.0]);
        self.set(EE_SCREENS_SEFM_UNDO, undo);
        *index += 1;
        let redo = make(h, EE_SCREENS_SEFM_OP_UNDO_REDO, [shell, 0], [1, *index as i32, 0, 0], [75.0, 718.0, 0.0, 0.0]);
        self.set(EE_SCREENS_SEFM_REDO, redo);
        *index += 1;
        for t in &EDITOR_TABS {
            let idx = index as *mut u32 as usize;
            let g = make(h, EE_SCREENS_SEFM_OP_TAB_GROUP, [shell, idx], [t.group as i32, 0, 0, 0], [0.0; 4]);
            self.set(EE_SCREENS_SEFM_GROUP + t.group, g);
        }
    }

    /// `CreateTabButtons`: the seven tab buttons along the bottom, the civilization builder's button after them.
    fn create_tab_buttons<H: SeplHost + ?Sized>(&self, h: &mut H, index: &mut u32) {
        for (i, t) in EDITOR_TABS.iter().enumerate() {
            let b = EeScreensSefmButton {
                kind: 0,
                id: 220 + i as u32,
                index: *index,
                help: t.help,
                flag: t.flag,
                tab: i as i32,
                checked: u32::from(i == 0),
                normal: t.normal,
                pressed: t.pressed,
                rect: [t.order as f32 * 44.0, 735.0, 0.0, 0.0],
                ..Default::default()
            };
            let c = make(
                h,
                EE_SCREENS_SEFM_OP_BUTTON,
                [self.shell, &b as *const EeScreensSefmButton as usize],
                [0; 4],
                [0.0; 4],
            );
            self.set(EE_SCREENS_SEFM_TAB_BUTTON + i as u32, c);
            *index += 1;
        }
        self.set(EE_SCREENS_SEFM_ENTITY_GROUP, EDITOR_TABS[0].flag as usize);
        // the map button's tab (`mTabButtons[kEditorTabMap]->mTab`: the 0 it was made with)
        self.set(EE_SCREENS_SEFM_CURRENT_TAB, 0);
        let x = rect(h, self.get(EE_SCREENS_SEFM_TAB_BUTTON + 6))[2] + 12.0;
        let dest = [x, 735.0, x + 32.0 - 1.0, 735.0 + 32.0 - 1.0];
        let b = EeScreensSefmButton {
            kind: 1,
            id: CIV_BUILDER_ID,
            index: *index,
            help: EE_SCREENS_SEFM_IDS_CIVILIZATION_BUILDER_TOOL,
            event: crate::scenario_editor::EE_SCREENS_SE_OPEN_CIV_BUILDER,
            normal: [192.0, 116.0],
            pressed: [224.0, 116.0],
            dest,
            rect: dest,
            ..Default::default()
        };
        let c = make(
            h,
            EE_SCREENS_SEFM_OP_BUTTON,
            [self.shell, &b as *const EeScreensSefmButton as usize],
            [0; 4],
            [0.0; 4],
        );
        self.set(EE_SCREENS_SEFM_CIV_BUILDER, c);
        *index += 1;
    }

    /// `CreateToolbarButtons`: the six tool bar buttons and their events; the quick buttons share one action.
    fn create_toolbar_buttons<H: SeplHost + ?Sized>(&self, h: &mut H, index: &mut u32) {
        use crate::scenario_editor::*;
        let quick = make(h, EE_SCREENS_SEFM_OP_QUICK_ACTION, [0, 0], [0; 4], [0.0; 4]);
        for t in &TOOLBAR_BUTTONS {
            let (x, y) = (t.x as f32, t.y as f32);
            let dest = [x, y, (t.x + 31) as f32, (t.y + 31) as f32];
            let (event, object, slot) = match t.id {
                TOOLBAR_LOAD => (EE_SCREENS_SE_LOAD_SCENARIO, 0, EE_SCREENS_SEFM_LOAD_BUTTON),
                TOOLBAR_SAVE => (EE_SCREENS_SE_SAVE_SCENARIO, 0, EE_SCREENS_SEFM_SAVE_BUTTON),
                TOOLBAR_QUICK_LOAD => (SEE_QUICK_LOAD, quick, EE_SCREENS_SEFM_QUICK_LOAD_BUTTON),
                TOOLBAR_QUICK_SAVE => (SEE_QUICK_SAVE, quick, EE_SCREENS_SEFM_QUICK_SAVE_BUTTON),
                TOOLBAR_TEST => (EE_SCREENS_SE_TEST_SCENARIO, 0, EE_SCREENS_SEFM_TEST_BUTTON),
                // 15, the exit (the original's other ids are none of the table's)
                _ => (EE_SCREENS_SE_EXIT_SCENARIO, 0, EE_SCREENS_SEFM_EXIT_BUTTON),
            };
            let b = EeScreensSefmButton {
                kind: 1,
                id: t.id,
                index: *index,
                help: t.help,
                event,
                object,
                normal: [t.normal[0] as f32, t.normal[1] as f32],
                pressed: [t.pressed[0] as f32, t.pressed[1] as f32],
                dest,
                // `GERectangle(GE2DPoint(left, top), 32, 32)`
                rect: [x, y, x + (32.0 - 1.0), y + (32.0 - 1.0)],
                ..Default::default()
            };
            let c = make(
                h,
                EE_SCREENS_SEFM_OP_BUTTON,
                [self.shell, &b as *const EeScreensSefmButton as usize],
                [0; 4],
                [0.0; 4],
            );
            self.set(slot, c);
            *index += 1;
        }
        op(h, EE_SCREENS_SEFM_OP_DEREFERENCE, [quick, 0, 0], [DEREF_ACTION, 0, 0, 0], [0.0; 4]);
    }

    /// The destructor's releases and deletes, in the original's order.
    pub fn destroy<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let deref = |h: &mut H, p: usize, kind: i32| {
            op(h, EE_SCREENS_SEFM_OP_DEREFERENCE, [p, 0, 0], [kind, 0, 0, 0], [0.0; 4]);
        };
        let delete = |h: &mut H, p: usize, kind: i32| {
            if p != 0 {
                op(h, EE_SCREENS_SEFM_OP_DELETE, [p, 0, 0], [kind, 0, 0, 0], [0.0; 4]);
            }
        };
        for (slot, kind) in [
            (EE_SCREENS_SEFM_WHEEL_ZOOM_EVENT, DEREF_EVENT),
            (EE_SCREENS_SEFM_REVOLVE_EVENT, DEREF_EVENT),
            (EE_SCREENS_SEFM_ROTATE_EVENT, DEREF_EVENT),
            (EE_SCREENS_SEFM_ENABLE_MOUSE_EVENT, DEREF_EVENT),
            (EE_SCREENS_SEFM_DISABLE_MOUSE_EVENT, DEREF_EVENT),
            (EE_SCREENS_SEFM_EDGE_SCROLL, DEREF_SCROLL),
            (EE_SCREENS_SEFM_NEW_PLAYER_EVENT, DEREF_EVENT),
            (EE_SCREENS_SEFM_PANEL_BACKGROUND, DEREF_BITMAP),
        ] {
            deref(h, self.get(slot), kind);
        }
        for i in 0..7 {
            delete(h, self.get(EE_SCREENS_SEFM_TAB_BUTTON + i), 0);
            delete(h, self.get(EE_SCREENS_SEFM_GROUP + i), 1);
        }
        deref(h, self.get(EE_SCREENS_SEFM_BUTTON_TEXTURE), DEREF_TEXTURE);
        delete(h, self.get(EE_SCREENS_SEFM_MINI_MAP), 0);
        delete(h, self.get(EE_SCREENS_SEFM_POLL), 2);
        delete(h, self.get(EE_SCREENS_SEFM_UNDO), 0);
        delete(h, self.get(EE_SCREENS_SEFM_REDO), 0);
        delete(h, self.get(EE_SCREENS_SEFM_BRUSH_GROUP), 3);
        delete(h, self.get(EE_SCREENS_SEFM_FULL_BACKGROUND), 4);
        for slot in [
            EE_SCREENS_SEFM_LOAD_TEXT,
            EE_SCREENS_SEFM_LOAD_BUTTON,
            EE_SCREENS_SEFM_TEST_BUTTON,
            EE_SCREENS_SEFM_SAVE_BUTTON,
            EE_SCREENS_SEFM_EXIT_BUTTON,
            EE_SCREENS_SEFM_QUICK_SAVE_BUTTON,
            EE_SCREENS_SEFM_QUICK_LOAD_BUTTON,
            EE_SCREENS_SEFM_CIV_BUILDER,
            EE_SCREENS_SEFM_HIDE_UI,
            EE_SCREENS_SEFM_PLAYER_COMBO,
            EE_SCREENS_SEFM_HELP_LABEL,
        ] {
            delete(h, self.get(slot), 0);
        }
        deref(h, self.get(EE_SCREENS_SEFM_ZOOM_IN_EVENT), DEREF_EVENT);
        deref(h, self.get(EE_SCREENS_SEFM_ZOOM_OUT_EVENT), DEREF_EVENT);
        deref(h, self.get(EE_SCREENS_SEFM_GRAB_SCROLL), DEREF_EVENT);
        op(h, EE_SCREENS_SEFM_OP_RELEASE_GRAB_SCROLL, [0; 3], [0; 4], [0.0; 4]);
    }

    // ---- the tabs

    /// `LeaveTab`: the brush highlight off, the group left (unless switching), its controls disabled.
    pub fn leave_tab<H: SeplHost + ?Sized>(&self, h: &mut H) {
        op(h, EE_SCREENS_SEFM_OP_HIGHLIGHT, [self.get(EE_SCREENS_SEFM_BRUSH_GROUP), 0, 0], [0; 4], [0.0; 4]);
        if !self.flag(EE_SCREENS_SEFM_SWITCHING) {
            self.group_call(h, EE_SCREENS_SEFM_GROUP_LEAVE, [0.0; 4]);
        }
        if self.flag(EE_SCREENS_SEFM_ENTITY_GROUP_ENABLED) {
            self.set_flag(EE_SCREENS_SEFM_ENTITY_GROUP_ENABLED, false);
            let g = self.get(EE_SCREENS_SEFM_ENTITY_GROUP) as i32;
            op(h, EE_SCREENS_SEFM_OP_GROUP_ENABLE, [0, self.shell, 0], [g, 0, 0, 0], [0.0; 4]);
        }
    }

    /// `SelectTab(flag, tab)`: the full view entered or left, the tab's controls enabled, its group entered and
    /// loaded, the brush highlight on for a painting tab.
    pub fn select_tab<H: SeplHost + ?Sized>(&self, h: &mut H, flag: u32, tab: usize) {
        let (from, to) = (self.current_tab().full_view, EDITOR_TABS[tab.min(6)].full_view);
        if from && !to {
            self.leave_full_view(h);
        } else if !from && to {
            self.enter_full_view(h);
        }
        self.leave_tab(h);
        self.set(EE_SCREENS_SEFM_CURRENT_TAB, tab);
        op(h, EE_SCREENS_SEFM_OP_GROUP_ENABLE, [0, self.shell, 0], [flag as i32, 1, 0, 0], [0.0; 4]);
        self.set_flag(EE_SCREENS_SEFM_ENTITY_GROUP_ENABLED, true);
        self.set(EE_SCREENS_SEFM_ENTITY_GROUP, flag as usize);
        self.group_call(h, EE_SCREENS_SEFM_GROUP_ENTER, [0.0; 4]);
        self.group_call(h, EE_SCREENS_SEFM_GROUP_LOAD, [0.0; 4]);
        if self.current_tab().brush {
            op(h, EE_SCREENS_SEFM_OP_HIGHLIGHT, [self.get(EE_SCREENS_SEFM_BRUSH_GROUP), 0, 0], [1, 0, 0, 0], [0.0; 4]);
        }
    }

    /// `EnterFullView` / `LeaveFullView`: the game's render list, the mini map hidden or shown, everything redrawn.
    pub fn enter_full_view<H: SeplHost + ?Sized>(&self, h: &mut H) {
        op(h, EE_SCREENS_SEFM_OP_RENDER_LIST, [0; 3], [1, 0, 0, 0], [0.0; 4]);
        display(h, self.get(EE_SCREENS_SEFM_MINI_MAP), false);
        op(h, EE_SCREENS_SEFM_OP_REDRAW_ALL, [0; 3], [0; 4], [0.0; 4]);
    }

    pub fn leave_full_view<H: SeplHost + ?Sized>(&self, h: &mut H) {
        op(h, EE_SCREENS_SEFM_OP_RENDER_LIST, [0; 3], [0; 4], [0.0; 4]);
        display(h, self.get(EE_SCREENS_SEFM_MINI_MAP), true);
        op(h, EE_SCREENS_SEFM_OP_REDRAW_ALL, [0; 3], [0; 4], [0.0; 4]);
    }

    /// `ExitEditor(test)`: back on the map tab, the load thread asked to test the scenario (from the editor or the
    /// game) or to quit; the form suspended.
    pub fn exit_editor<H: SeplHost + ?Sized>(&self, h: &mut H, test: bool) {
        op(h, EE_SCREENS_SEFM_OP_TAB_CHECK, [self.get(EE_SCREENS_SEFM_TAB_BUTTON), 0, 0], [1, 0, 0, 0], [0.0; 4]);
        self.set_flag(EE_SCREENS_SEFM_SWITCHING, true);
        let post = if test {
            if op(h, EE_SCREENS_SEFM_OP_TEST_PREPARE, [0; 3], [1, 0, 0, 0], [0.0; 4]) != 0 {
                EE_SCREENS_SEFM_POST_404
            } else {
                EE_SCREENS_SEFM_POST_405
            }
        } else {
            EE_SCREENS_SEFM_POST_401
        };
        self.post(h, post, 0);
        self.set(EE_SCREENS_SEFM_LOAD_THREAD, 0);
        self.set_flag(EE_SCREENS_SEFM_SWITCHING, false);
        self.set_flag(EE_SCREENS_SEFM_SUSPENDED, true);
    }

    fn post<H: SeplHost + ?Sized>(&self, h: &mut H, post: i32, object: usize) {
        let thread = self.get(EE_SCREENS_SEFM_LOAD_THREAD);
        op(h, EE_SCREENS_SEFM_OP_LOAD_THREAD, [0, thread, object], [post, 0, 0, 0], [0.0; 4]);
    }

    /// `PostLoadEvent` (`Post40B`) / `PostShowLoadedObject` (`Post40C`): the form suspended, the object passed on.
    pub fn post_suspended<H: SeplHost + ?Sized>(&self, h: &mut H, post: i32, object: usize) {
        self.set_flag(EE_SCREENS_SEFM_SUSPENDED, true);
        self.post(h, post, object);
    }

    /// `ResumeCurrentGroup`: the form resumed, the group loaded again.
    pub fn resume_group<H: SeplHost + ?Sized>(&self, h: &mut H) {
        self.set_flag(EE_SCREENS_SEFM_SUSPENDED, false);
        self.group_call(h, EE_SCREENS_SEFM_GROUP_LOAD, [0.0; 4]);
    }

    /// `RefreshCurrentGroup`: the group stores its settings.
    pub fn store_group<H: SeplHost + ?Sized>(&self, h: &mut H) {
        self.group_call(h, EE_SCREENS_SEFM_GROUP_STORE, [0.0; 4]);
    }

    /// `SetGameView(rectangle, hidden)`: the game view set, the group disabled while the UI is hidden.
    pub fn set_game_view<H: SeplHost + ?Sized>(&self, h: &mut H, r: [f32; 4], hidden: bool) {
        op(h, EE_SCREENS_SEFM_OP_GAME_VIEW, [0; 3], [0; 4], r);
        self.set_flag(EE_SCREENS_SEFM_UI_HIDDEN, hidden);
        let which = if hidden { EE_SCREENS_SEFM_GROUP_DISABLE } else { EE_SCREENS_SEFM_GROUP_ENABLE };
        self.group_call(h, which, [0.0; 4]);
    }

    /// `SetLoadText(text)`: shown with the text (the `UWideString` at `text`) unless empty, else hidden.
    pub fn set_load_text<H: SeplHost + ?Sized>(&self, h: &mut H, text: usize, empty: bool) {
        let label = self.get(EE_SCREENS_SEFM_LOAD_TEXT);
        if !empty {
            display(h, label, true);
            op(h, EE_SCREENS_SEFM_OP_LABEL_TEXT, [label, text, 0], [0; 4], [0.0; 4]);
        } else {
            display(h, label, false);
        }
    }

    /// `Open`: the editor camera, the load thread, the backgrounds, the full view, the paused music and ambient
    /// sounds, the editor's game view and mini map, the current tab entered and loaded; the exit action posted if a
    /// save asked to exit afterwards.
    pub fn open<H: SeplHost + ?Sized>(&self, h: &mut H) {
        self.set_flag(EE_SCREENS_SEFM_SWITCHING, true);
        self.set_flag(EE_SCREENS_SEFM_SUSPENDED, false);
        op(h, EE_SCREENS_SEFM_OP_OPEN_CAMERA, [0; 3], [0; 4], [0.0; 4]);
        if self.get(EE_SCREENS_SEFM_LOAD_THREAD) == 0 {
            let t = make(h, EE_SCREENS_SEFM_OP_LOAD_THREAD, [0, self.shell], [0; 4], [0.0; 4]);
            self.set(EE_SCREENS_SEFM_LOAD_THREAD, t);
        }
        op(h, EE_SCREENS_SEFM_OP_LOAD_BACKGROUND, [self.get(EE_SCREENS_SEFM_FULL_BACKGROUND), 0, 0], [0; 4], [0.0; 4]);
        if self.current_tab().full_view {
            self.enter_full_view(h);
        } else {
            self.leave_full_view(h);
        }
        let (players, scroll) = (self.get(EE_SCREENS_SEFM_GROUP + 5), self.get(EE_SCREENS_SEFM_EDGE_SCROLL));
        let (mut i, mut f) = ([0i32; 4], [0.0f32; 4]);
        h.op(EE_SCREENS_SEFM_OP_OPEN_WORLD, [players, scroll, 0], &mut i, &mut f);
        for (k, b) in bits(f).into_iter().enumerate() {
            self.set(EE_SCREENS_SEFM_SAVED_VIEW + k as u32, b);
        }
        self.group_call(h, EE_SCREENS_SEFM_GROUP_ENTER, [0.0; 4]);
        self.group_call(h, EE_SCREENS_SEFM_GROUP_LOAD, [0.0; 4]);
        op(h, EE_SCREENS_SEFM_OP_OPEN_FORM, [0, self.shell, 0], [0; 4], [0.0; 4]);
        let help = self.get(EE_SCREENS_SEFM_HELP_LABEL);
        display(h, help, false);
        op(h, EE_SCREENS_SEFM_OP_HELP_LABEL, [help, self.shell, 0], [0; 4], [0.0; 4]);
        self.set_flag(EE_SCREENS_SEFM_SWITCHING, false);
        if !self.flag(EE_SCREENS_SEFM_EXIT_AFTER_SAVE) {
            return;
        }
        op(h, EE_SCREENS_SEFM_OP_POST_EXIT, [0; 3], [0; 4], [0.0; 4]);
        self.set_flag(EE_SCREENS_SEFM_EXIT_AFTER_SAVE, false);
    }

    /// `CopyData` before the form's own: the game view dirty (unless full view), the mini map dirty, the ghost at
    /// the mouse, the group updated, the edge scroll updated (unless full view or a control holds the mouse outside
    /// the elevation and terrain tabs).
    pub fn copy_data<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let t = self.current_tab();
        let mini = self.get(EE_SCREENS_SEFM_MINI_MAP);
        op(h, EE_SCREENS_SEFM_OP_COPY_WORLD, [mini, self.shell, 0], [i32::from(!t.full_view), 0, 0, 0], [0.0; 4]);
        self.set_flag(EE_SCREENS_SEFM_WAS_UI_HIDDEN, self.flag(EE_SCREENS_SEFM_UI_HIDDEN));
        op(h, EE_SCREENS_SEFM_OP_COPY_WORLD, [0, self.shell, 0], [2, 0, 0, 0], [0.0; 4]);
        self.group_call(h, EE_SCREENS_SEFM_GROUP_UPDATE, [0.0; 4]);
        let t = self.current_tab();
        if !t.full_view
            && (op(h, EE_SCREENS_SEFM_OP_CAPTURED, [0, self.shell, 0], [0; 4], [0.0; 4]) == 0
                || t.group == 1
                || t.group == 2)
        {
            op(h, EE_SCREENS_SEFM_OP_EDGE_SCROLL, [self.get(EE_SCREENS_SEFM_EDGE_SCROLL), 0, 0], [0; 4], [0.0; 4]);
        }
    }

    /// `DrawBackground(rectangle)`: the full view's background or the bottom panel clipped to the rectangle, then
    /// the group; the full view's result.
    pub fn draw_background<H: SeplHost + ?Sized>(&self, h: &mut H, r: [f32; 4]) -> i32 {
        let mut result = 0;
        if self.current_tab().full_view {
            result = op(h, EE_SCREENS_SEFM_OP_DRAW_FULL, [self.get(EE_SCREENS_SEFM_FULL_BACKGROUND), 0, 0], [0; 4], r);
        } else if let Some(clip) = ge(r).and(&ge(PANEL)) {
            let clip = [clip.left, clip.top, clip.right, clip.bottom];
            let panel = self.get(EE_SCREENS_SEFM_PANEL_BACKGROUND);
            op(h, EE_SCREENS_SEFM_OP_DRAW_PANEL, [panel, self.shell, PANEL.as_ptr() as usize], [0; 4], clip);
        }
        self.group_call(h, EE_SCREENS_SEFM_GROUP_DRAW, r);
        result
    }

    fn key<H: SeplHost + ?Sized>(h: &mut H, key: i32) -> i32 {
        op(h, EE_SCREENS_SEFM_OP_KEY, [0; 3], [key, 0, 0, 0], [0.0; 4])
    }

    /// `UpdateKeyboard` before the form's own: `;` quick saves, `/` quick loads, T / Shift+T step the grid, 1-8 and
    /// Alt+1-8 pick a player while the player combo box is shown; else the group's keys. Returns whether the form's
    /// own runs (nothing handled, not suspended).
    pub fn update_keyboard<H: SeplHost + ?Sized>(&self, h: &mut H) -> bool {
        if self.flag(EE_SCREENS_SEFM_SUSPENDED) {
            return false;
        }
        let captured = |h: &mut H| op(h, EE_SCREENS_SEFM_OP_CAPTURED, [0, self.shell, 0], [1, 0, 0, 0], [0.0; 4]) != 0;
        let mut handled = false;
        if !captured(h) {
            if Self::key(h, EE_SCREENS_SEFM_KEY_SEMICOLON) & 1 != 0 {
                op(h, EE_SCREENS_SEFM_OP_FIRE, [0, self.shell, 0], [13, 0, 0, 0], [0.0; 4]);
                handled = true;
            }
            if Self::key(h, EE_SCREENS_SEFM_KEY_SLASH) & 1 != 0 {
                op(h, EE_SCREENS_SEFM_OP_FIRE, [0, self.shell, 0], [12, 0, 0, 0], [0.0; 4]);
                handled = true;
            }
            if Self::key(h, EE_SCREENS_SEFM_KEY_T) & 1 != 0 {
                let shift = Self::key(h, EE_SCREENS_SEFM_KEY_SHIFT) & 2 != 0;
                op(h, EE_SCREENS_SEFM_OP_GRID, [0; 3], [i32::from(shift), 0, 0, 0], [0.0; 4]);
                handled = true;
            }
            let combo = self.get(EE_SCREENS_SEFM_PLAYER_COMBO);
            if is_shown(h, combo) {
                let players = self.get(EE_SCREENS_SEFM_GROUP + 5);
                let alt = Self::key(h, EE_SCREENS_SEFM_KEY_ALT);
                let mut player = 0;
                for k in 0..8 {
                    if Self::key(h, EE_SCREENS_SEFM_KEY_1 + k) & 1 != 0 {
                        player = k + 1;
                    }
                }
                if player > 0 {
                    if alt & 2 != 0 {
                        player += 8;
                    }
                    if player <= op(h, EE_SCREENS_SEFM_OP_PLAYER_COUNT, [players, 0, 0], [0; 4], [0.0; 4]) {
                        op(h, EE_SCREENS_SEFM_OP_COMBO_SELECT, [combo, 0, 0], [player - 1, 0, 0, 0], [0.0; 4]);
                    }
                    handled = true;
                }
            }
        }
        if !handled {
            if !captured(h) {
                self.group_call(h, EE_SCREENS_SEFM_GROUP_UPDATE_KEYS, [0.0; 4]);
            }
            return true;
        }
        false
    }

    /// `UpdateMouse` before the form's own: the group gets the mouse unless a control holds it. Returns whether the
    /// form's own runs (not suspended).
    pub fn update_mouse<H: SeplHost + ?Sized>(&self, h: &mut H) -> bool {
        if self.flag(EE_SCREENS_SEFM_SUSPENDED) {
            return false;
        }
        if op(h, EE_SCREENS_SEFM_OP_CAPTURED, [0, self.shell, 0], [0; 4], [0.0; 4]) == 0 {
            let (mut i, mut f) = ([0i32; 4], [0.0f32; 4]);
            h.op(EE_SCREENS_SEFM_OP_MOUSE, [0, self.shell, 0], &mut i, &mut f);
            self.group_call(h, EE_SCREENS_SEFM_GROUP_MOUSE_MOVE, f);
        }
        true
    }

    // ---- the selection

    fn has_cargo<H: SeplHost + ?Sized>(h: &mut H, unit: usize) -> bool {
        op(h, EE_SCREENS_SEFM_OP_UNIT_CARGO, [unit, 0, 0], [0; 4], [0.0; 4]) != 0
    }

    fn mark<H: SeplHost + ?Sized>(h: &mut H, unit: usize, selected: bool, reference: i32) {
        op(h, EE_SCREENS_SEFM_OP_UNIT_SELECT, [unit, 0, 0], [i32::from(selected), reference, 0, 0], [0.0; 4]);
    }

    fn cargo_add(&self, delta: isize) -> isize {
        let n = (self.get(EE_SCREENS_SEFM_NUM_CARGO_UNITS) as isize).wrapping_add(delta);
        self.set(EE_SCREENS_SEFM_NUM_CARGO_UNITS, n as usize);
        n
    }

    fn show_cargo<H: SeplHost + ?Sized>(&self, h: &mut H, shown: bool) {
        op(h, EE_SCREENS_SEFM_OP_SHOW_CARGO, [self.unit_screen(), 0, 0], [i32::from(shown), 0, 0, 0], [0.0; 4]);
    }

    fn show_status<H: SeplHost + ?Sized>(&self, h: &mut H, unit: usize) {
        op(h, EE_SCREENS_SEFM_OP_SHOW_STATUS, [self.unit_screen(), unit, 0], [0; 4], [0.0; 4]);
    }

    /// `SelectUnit(unit)`: referenced and added; a placed carrier counts and shows the cargo panel; a lone unit's
    /// status shown.
    pub fn select_unit<H: SeplHost + ?Sized>(&self, h: &mut H, unit: usize) {
        let id = op(h, EE_SCREENS_SEFM_OP_UNIT_ID, [unit, 0, 0], [0; 4], [0.0; 4]) as u32;
        Self::mark(h, unit, true, 1);
        let count = {
            let mut s = self.selected.lock().unwrap_or_else(|e| e.into_inner());
            s.push(id);
            s.len()
        };
        if Self::has_cargo(h, unit) {
            self.cargo_add(1);
            self.show_cargo(h, true);
        }
        if count == 1 {
            self.show_status(h, unit);
        }
    }

    /// The selection's entry `k` removed.
    fn remove(&self, k: usize) {
        let mut s = self.selected.lock().unwrap_or_else(|e| e.into_inner());
        if k < s.len() {
            s.remove(k);
        }
    }

    /// `DeselectFirstUnit`: the first unit taken out (the cargo panel hidden with the last carrier), dereferenced,
    /// the next one's status shown.
    pub fn deselect_first<H: SeplHost + ?Sized>(&self, h: &mut H) {
        if self.selected_count() == 0 {
            return;
        }
        let unit = self.first_unit(h);
        Self::mark(h, unit, false, 0);
        self.remove(0);
        if unit != 0 && Self::has_cargo(h, unit) && self.cargo_add(-1) == 0 {
            self.show_cargo(h, false);
        }
        Self::mark(h, unit, false, -1);
        let first = self.first_unit(h);
        self.show_status(h, first);
    }

    /// `DeselectUnit(unit)`: its entry (the first resolving to it) taken out as `DeselectFirstUnit`; the status
    /// shown again only if it was the first.
    pub fn deselect_unit<H: SeplHost + ?Sized>(&self, h: &mut H, unit: usize) {
        let ids: Vec<u32> = self.selected.lock().map(|s| s.clone()).unwrap_or_default();
        let Some(k) = ids.iter().position(|&id| Self::resolve(h, id) == unit) else {
            return;
        };
        let found = Self::resolve(h, ids[k]);
        Self::mark(h, found, false, 0);
        let first = found == Self::resolve(h, ids[0]);
        if found != 0 && Self::has_cargo(h, found) && self.cargo_add(-1) == 0 {
            self.show_cargo(h, false);
        }
        self.remove(k);
        Self::mark(h, found, false, -1);
        if first {
            let next = self.first_unit(h);
            self.show_status(h, next);
        }
    }

    /// `ClearSelection`: every unit (from the last) unmarked and dereferenced, the cargo panel and the status
    /// hidden.
    pub fn clear_selection<H: SeplHost + ?Sized>(&self, h: &mut H) {
        loop {
            let last = self.selected.lock().ok().and_then(|s| s.last().copied());
            let Some(id) = last else { break };
            let unit = Self::resolve(h, id);
            Self::mark(h, unit, false, -1);
            let mut s = self.selected.lock().unwrap_or_else(|e| e.into_inner());
            s.pop();
        }
        self.set(EE_SCREENS_SEFM_NUM_CARGO_UNITS, 0);
        self.show_cargo(h, false);
        self.show_status(h, 0);
    }

    // ---- the trigger-only save (Ctrl+S)

    /// The scenario's triggers saved to ScenarioTriggers.scn and, if it opened, ScenarioTriggers.txt written with
    /// the report ([`trigger_report`]); the open's result.
    pub fn save_triggers<H: SeplHost + ?Sized>(h: &mut H) -> i32 {
        let result = op(h, EE_SCREENS_SEFM_OP_REPORT_SAVE, [0; 3], [0; 4], [0.0; 4]);
        if result == 0 {
            let text = trigger_report(&mut Records(h));
            op(
                h,
                EE_SCREENS_SEFM_OP_REPORT_WRITE,
                [text.as_ptr() as usize, 0, 0],
                [text.len() as i32, 0, 0, 0],
                [0.0; 4],
            );
        }
        result
    }
}

// ---- the trigger-only save's report over the scenario's records

/// The scenario's records as the report reads them.
pub trait ScenarioRecords {
    fn count(&mut self, list: i32) -> usize;
    fn trigger(&mut self, index: usize) -> Option<EeScenarioTriggerFields>;
    fn condition(&mut self, index: usize) -> Option<EeScenarioConditionFields>;
    fn effect(&mut self, index: usize) -> Option<EeScenarioEffectFields>;
    fn object(&mut self, index: usize) -> Option<EeScenarioObjectFields>;
    fn text(&mut self, list: i32, index: usize, which: i32) -> Vec<u8>;
    /// A record's vector (`EE_SCREENS_SEFM_VEC_*`): each element's three numbers (an effect's objects: the first).
    fn vec(&mut self, which: i32, index: usize) -> Vec<[i32; 3]>;
}

/// The records through the form's host.
pub struct Records<'a, H: SeplHost + ?Sized>(pub &'a mut H);

impl<H: SeplHost + ?Sized> Records<'_, H> {
    fn record(&mut self, list: i32, index: usize) -> usize {
        make(self.0, EE_SCREENS_SEFM_OP_SM_RECORD, [0, 0], [list, index as i32, 0, 0], [0.0; 4])
    }
}

impl<H: SeplHost + ?Sized> ScenarioRecords for Records<'_, H> {
    fn count(&mut self, list: i32) -> usize {
        op(self.0, EE_SCREENS_SEFM_OP_SM_COUNT, [0; 3], [list, 0, 0, 0], [0.0; 4]).max(0) as usize
    }
    fn trigger(&mut self, index: usize) -> Option<EeScenarioTriggerFields> {
        let p = self.record(EE_SCREENS_SEFM_LIST_TRIGGERS, index) as *const EeScenarioTriggerFields;
        // SAFETY: the host gives the record's numbers (the C++ record's base) or NULL; read while the UI thread
        // runs the save
        (!p.is_null()).then(|| unsafe { *p })
    }
    fn condition(&mut self, index: usize) -> Option<EeScenarioConditionFields> {
        let p = self.record(EE_SCREENS_SEFM_LIST_CONDITIONS, index) as *const EeScenarioConditionFields;
        // SAFETY: as `trigger`
        (!p.is_null()).then(|| unsafe { *p })
    }
    fn effect(&mut self, index: usize) -> Option<EeScenarioEffectFields> {
        let p = self.record(EE_SCREENS_SEFM_LIST_EFFECTS, index) as *const EeScenarioEffectFields;
        // SAFETY: as `trigger`
        (!p.is_null()).then(|| unsafe { *p })
    }
    fn object(&mut self, index: usize) -> Option<EeScenarioObjectFields> {
        let p = self.record(EE_SCREENS_SEFM_LIST_OBJECTS, index) as *const EeScenarioObjectFields;
        // SAFETY: as `trigger`
        (!p.is_null()).then(|| unsafe { *p })
    }
    fn text(&mut self, list: i32, index: usize, which: i32) -> Vec<u8> {
        let mut out = vec![0u8; 256];
        loop {
            let cap = out.len();
            let n = op(
                self.0,
                EE_SCREENS_SEFM_OP_SM_TEXT,
                [out.as_mut_ptr() as usize, 0, 0],
                [list, index as i32, which, cap as i32],
                [0.0; 4],
            );
            if n < 0 {
                return Vec::new();
            }
            if n as usize <= cap {
                out.truncate(n as usize);
                return out;
            }
            out.resize(n as usize, 0);
        }
    }
    fn vec(&mut self, which: i32, index: usize) -> Vec<[i32; 3]> {
        let n = op(self.0, EE_SCREENS_SEFM_OP_SM_VEC_LEN, [0; 3], [which, index as i32, 0, 0], [0.0; 4]).max(0);
        (0..n)
            .map(|k| {
                let (mut i, mut f) = ([which, index as i32, k, 0], [0.0f32; 4]);
                self.0.op(EE_SCREENS_SEFM_OP_SM_VEC_AT, [0; 3], &mut i, &mut f);
                [i[0], i[1], i[2]]
            })
            .collect()
    }
}

/// C's `%ld` of a number (`long`: 32 bits on Windows).
fn ld(v: c_long) -> String {
    v.to_string()
}

fn operator(out: &mut Vec<u8>, op: i32) {
    if op == OPERATOR_AND {
        out.extend_from_slice(b" AND");
    } else if op == OPERATOR_OR {
        out.extend_from_slice(b" OR");
    }
}

/// ScenarioTriggers.txt (`EEUIESaveScenarioTriggersOnly::ProcessEvent` 0x19ccc0): every trigger (name, description,
/// off, timed, looping, its conditions and effects), then the effects, conditions, objects and areas no trigger,
/// condition, effect or object uses, then the looping triggers' effects that set a name, a class attribute or the
/// tech tree each time.
pub fn trigger_report<R: ScenarioRecords + ?Sized>(r: &mut R) -> Vec<u8> {
    let mut out = Vec::new();
    let n_triggers = r.count(EE_SCREENS_SEFM_LIST_TRIGGERS);
    let n_conditions = r.count(EE_SCREENS_SEFM_LIST_CONDITIONS);
    let n_effects = r.count(EE_SCREENS_SEFM_LIST_EFFECTS);
    let n_objects = r.count(EE_SCREENS_SEFM_LIST_OBJECTS);
    let n_areas = r.count(EE_SCREENS_SEFM_LIST_AREAS);
    let conditions_of: Vec<Vec<[i32; 3]>> = (0..n_triggers).map(|t| r.vec(EE_SCREENS_SEFM_VEC_CONDITIONS, t)).collect();
    let effects_of: Vec<Vec<[i32; 3]>> = (0..n_triggers).map(|t| r.vec(EE_SCREENS_SEFM_VEC_EFFECTS, t)).collect();
    let line = |out: &mut Vec<u8>, parts: &[&[u8]]| {
        for p in parts {
            out.extend_from_slice(p);
        }
    };

    for t in 0..n_triggers {
        let Some(fields) = r.trigger(t) else { continue };
        let name = r.text(EE_SCREENS_SEFM_LIST_TRIGGERS, t, 0);
        line(&mut out, &[&name, b"\n"]);
        let text = r.text(EE_SCREENS_SEFM_LIST_TRIGGERS, t, 1);
        if !text.is_empty() {
            line(&mut out, &[b"Description:\n\"", &text, b"\"\n"]);
        }
        if !fields.mIsOn {
            out.extend_from_slice(b"\tTrigger starts OFF\n");
        }
        if fields.mConditionDuration != 0 {
            line(&mut out, &[b"\tConditions True for ", ld(fields.mConditionDuration as c_long).as_bytes(), b"\n"]);
        }
        if fields.mIsLooping {
            out.extend_from_slice(b"\tLooping\n");
        }
        out.extend_from_slice(b"\tIf ");
        for &[not, value, oper] in &conditions_of[t] {
            if not != 0 {
                out.extend_from_slice(b" NOT");
            }
            if r.condition(value as u32 as usize).is_some() && (value as u32 as usize) < n_conditions {
                let name = r.text(EE_SCREENS_SEFM_LIST_CONDITIONS, value as u32 as usize, 0);
                line(&mut out, &[b" (", &name, b") "]);
            } else {
                out.extend_from_slice(b" (BAD CONDITION) ");
            }
            operator(&mut out, oper);
        }
        out.extend_from_slice(b"\n\tThen ");
        for &[delay, effect, oper] in &effects_of[t] {
            if delay != 0 {
                line(&mut out, &[b" (Delay ", ld(delay as c_long).as_bytes(), b" sec) "]);
            }
            if r.effect(effect as u32 as usize).is_some() && (effect as u32 as usize) < n_effects {
                let name = r.text(EE_SCREENS_SEFM_LIST_EFFECTS, effect as u32 as usize, 0);
                line(&mut out, &[b" (", &name, b") "]);
            } else {
                out.extend_from_slice(b" (BAD EFFECT) ");
            }
            operator(&mut out, oper);
        }
        out.extend_from_slice(b"\n\n");
    }

    let effects: Vec<Option<EeScenarioEffectFields>> = (0..n_effects).map(|e| r.effect(e)).collect();
    let effect_objects: Vec<Vec<[i32; 3]>> = (0..n_effects).map(|e| r.vec(EE_SCREENS_SEFM_VEC_OBJECTS, e)).collect();
    for (i, e) in effects.iter().enumerate().skip(1) {
        let Some(e) = e else { continue };
        let mut unused = e.mType != EFFECT_NONE;
        for list in effects_of.iter() {
            if !unused {
                break;
            }
            if list.iter().any(|w| w[1] as u32 as usize == i) {
                unused = false;
            }
        }
        if unused {
            let name = r.text(EE_SCREENS_SEFM_LIST_EFFECTS, i, 0);
            line(&mut out, &[b"Effect: (", &name, b") is not used in any trigger\n"]);
        }
    }

    let conditions: Vec<Option<EeScenarioConditionFields>> = (0..n_conditions).map(|c| r.condition(c)).collect();
    let used_condition = |c: usize| conditions_of.iter().any(|l| l.iter().any(|w| w[1] as u32 as usize == c));
    for (i, c) in conditions.iter().enumerate().skip(1) {
        let Some(c) = c else { continue };
        let unused = c.mType >= CONDITION_PLAYER_ATTRIBUTE && !used_condition(i);
        if unused {
            let name = r.text(EE_SCREENS_SEFM_LIST_CONDITIONS, i, 0);
            line(&mut out, &[b"Condition: (", &name, b") is not used in any trigger\n"]);
        }
    }

    let objects: Vec<Option<EeScenarioObjectFields>> = (0..n_objects).map(|o| r.object(o)).collect();
    for i in 1..n_objects {
        if objects[i].is_none() {
            continue;
        }
        let iv = i as c_ulong;
        let mut unused = true;
        for (j, e) in effects.iter().enumerate().skip(1) {
            if !unused {
                break;
            }
            let Some(e) = e else { continue };
            if effect_objects[j].iter().any(|o| o[0] as u32 as c_ulong == iv) {
                unused = false;
            }
            if unused
                && (e.mTargetObject == iv
                    || e.mPlaceObject == iv
                    || e.mTextObject == iv
                    || e.mObject == iv
                    || e.mTaskObject == iv
                    || e.mLocationObject == iv)
            {
                unused = false;
            }
        }
        for (j, c) in conditions.iter().enumerate().skip(1) {
            if !unused {
                break;
            }
            let Some(c) = c else { continue };
            if c.mType != CONDITION_OBJECT || c.mObjectIndex != iv {
                continue;
            }
            if used_condition(j) {
                unused = false;
            }
        }
        for o in objects.iter().skip(1).flatten() {
            if !unused {
                break;
            }
            if o.mParent == iv || o.mNearObject == iv {
                unused = false;
            }
        }
        if unused {
            let name = r.text(EE_SCREENS_SEFM_LIST_OBJECTS, i, 0);
            line(&mut out, &[b"Object: (", &name, b") is not used in any condition, effect or other object\n"]);
        }
    }

    for i in 2..n_areas {
        let iv = i as c_ulong;
        let mut unused = !effects
            .iter()
            .skip(1)
            .flatten()
            .any(|e| e.mTargetArea == iv || e.mPlaceArea == iv || e.mTaskArea == iv || e.mLocationArea == iv);
        if unused && objects.iter().skip(1).flatten().any(|o| o.mArea == iv) {
            unused = false;
        }
        if unused {
            let name = r.text(EE_SCREENS_SEFM_LIST_AREAS, i, 0);
            line(&mut out, &[b"Area: (", &name, b") is not used in any condition, effect or Object\n"]);
        }
    }

    for (t, effects_of_t) in effects_of.iter().enumerate() {
        let Some(fields) = r.trigger(t) else { continue };
        if !fields.mIsLooping {
            continue;
        }
        for w in effects_of_t {
            let k = w[1] as u32 as usize;
            // a missing effect is listed as BAD EFFECT above
            let Some(e) = effects.get(k).copied().flatten() else { continue };
            if (e.mType == EFFECT_OBJECT
                && (e.mObjectAction == OBJECT_ACTION_CLASS_NAME || e.mObjectAction == OBJECT_ACTION_CLASS_ATTRIBUTE))
                || (e.mType == EFFECT_PLAYER && e.mPlayerAction == PLAYER_ACTION_MODIFY_TECH_TREE)
            {
                let tn = r.text(EE_SCREENS_SEFM_LIST_TRIGGERS, t, 0);
                let en = r.text(EE_SCREENS_SEFM_LIST_EFFECTS, k, 0);
                line(
                    &mut out,
                    &[
                        b"WARNING: Potential performance issue with looping Trigger (",
                        &tn,
                        b") calling effect (",
                        &en,
                        b") which repeatedly sets the objects name, class attribute or player tech\n",
                    ],
                );
            }
        }
    }
    out
}

#[cfg(test)]
mod tests;
