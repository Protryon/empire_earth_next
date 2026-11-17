//! The civilization builder whole: the original `EECivilizationBuilder.cpp` (the builder forms' helpers, the
//! civilization timer, the upgrade moves) and the builder's half of `EECBCivilizationFiles.cpp` (its state, the
//! saved civilization files' writing, the load into the builder, its open and close, its buy, return, delete and
//! accept), with the edited civilization ([`crate::civ_edit`], the original `EECBEditableCivilization` and
//! `EECBCivilizationCreator`) and the upgrade list box's logic ([`crate::civ_list`], `EECBUIListBox`), over one
//! host ([`CbHost`]) whose ops are each one toolkit, file or game call (`Civilization Builder/EECBRust.cpp`).
//!
//! The forms: 7 the builder (203 the available upgrades, 204 the bought ones, 196 the name, 206 the points spent,
//! 209/210 the points left, 448 accept), 15 the name dialog (410 the name), 16 save (420 the files, 562 the name),
//! 17 load (425 the files), 35 the predefined civilizations' choice. A civilization file is the keyed text when
//! every upgrade has a key, else the legacy binary form ([`legacy_file`]); `EELoadCivilization`, the AI's and the
//! campaign's `EEApplyCivilizationFile` and the server's `EECBRequestUpgrades` stay C++ (the simulation's).

use core::ffi::c_long;
use std::sync::atomic::{AtomicPtr, Ordering};
use std::sync::Mutex;

use crate::screens::civ_edit::Editable;
use crate::screens::civ_list;

// ---- the host's ops (`ee_screenscpp_cb(op, p, a, b, c, text, length, out)`; a form and a control are ids)

/// (a form) -> 1 when it exists.
pub const EE_SCREENS_CBW_FORM_EXISTS: u32 = 1;
/// (a form, b control) -> 0 and the edit box's first line, 3 without the form or the edit box.
pub const EE_SCREENS_CBW_EDIT_TEXT: u32 = 2;
/// (a form, b control, text) the edit box's text -> 0, 3 without it.
pub const EE_SCREENS_CBW_SET_EDIT_TEXT: u32 = 3;
/// (a form, b control) the edit box's `SelectText`, then the form's tab stop on it.
pub const EE_SCREENS_CBW_EDIT_SELECT: u32 = 4;
/// (a form, b control) -> 0 and the text label's first line, 3 without it.
pub const EE_SCREENS_CBW_LABEL_TEXT: u32 = 5;
/// (a form, b control, text) the text label's text -> 0, 3 without it.
pub const EE_SCREENS_CBW_SET_LABEL_TEXT: u32 = 6;
/// (a form, b control, c number) the text label's `SetText(long)` -> 0, 3 without it.
pub const EE_SCREENS_CBW_SET_LABEL_NUMBER: u32 = 7;
/// (a form, b control, c mode) the control's display mode (by id; the original assumed the control).
pub const EE_SCREENS_CBW_DISPLAY: u32 = 8;
/// (a form) `gUIManager->Push` -> its result, 3 without the form.
pub const EE_SCREENS_CBW_PUSH: u32 = 9;
/// `gUserInterface->CloseForm(NULL)` -> its result.
pub const EE_SCREENS_CBW_CLOSE_FORM: u32 = 10;
/// (a form) -> 1 when the current form is that form (`GetCurrentForm() == GetFormByID(a)`).
pub const EE_SCREENS_CBW_CURRENT_IS: u32 = 11;
/// (a form, b control) the civilization files listed in the file list box (`Users\default\Civilizations\*.civ`).
pub const EE_SCREENS_CBW_FILE_LIST_FILL: u32 = 12;
/// (a form, b control; or p a form) -> -1 without the file list, else its `GetCurrentSelection` result and the
/// index.
pub const EE_SCREENS_CBW_FILE_SELECTION: u32 = 13;
/// (a form, b control, c line; or p a form) -> the file list's line.
pub const EE_SCREENS_CBW_FILE_LINE: u32 = 14;
/// (a form, b control) -> the file list's `DeleteSelectedFile` result.
pub const EE_SCREENS_CBW_FILE_DELETE: u32 = 15;
/// (p a form, a form id) -> 1 when `p` is that form.
pub const EE_SCREENS_CBW_IS_FORM: u32 = 16;
/// (a form, b control) -> the control as an `EECBUIListBox*`, 0 without it.
pub const EE_SCREENS_CBW_LIST: u32 = 17;
/// (a form, b open event, c close event) the form's open and close events (`EEScreenEvent`s of those codes).
pub const EE_SCREENS_CBW_FORM_EVENTS: u32 = 18;
/// (p a form) -> its id.
pub const EE_SCREENS_CBW_FORM_ID: u32 = 19;
/// (a form, b control index, c mode) the control's display mode (by index).
pub const EE_SCREENS_CBW_DISPLAY_INDEX: u32 = 20;

// the upgrade list box's (p the `EECBUIListBox*`; the base classes' members, not its overrides)
/// -> `GetNumLines()`.
pub const EE_SCREENS_CBW_L_COUNT: u32 = 30;
/// (a line) -> the line.
pub const EE_SCREENS_CBW_L_LINE: u32 = 31;
/// (a line, text) the line replaced.
pub const EE_SCREENS_CBW_L_SET_LINE: u32 = 32;
/// (text) `UIListBox::AppendLine`.
pub const EE_SCREENS_CBW_L_APPEND: u32 = 33;
/// (a line) `UIListBox::DeleteLine`.
pub const EE_SCREENS_CBW_L_DELETE: u32 = 34;
/// `DeleteAllEntries()`.
pub const EE_SCREENS_CBW_L_DELETE_ALL: u32 = 35;
/// (a line, b fire, c scroll) `UIListBox::ChangeSelectionIndex`.
pub const EE_SCREENS_CBW_L_SELECT: u32 = 36;
/// -> `GetCurrentSelection`'s result and the index.
pub const EE_SCREENS_CBW_L_CURRENT: u32 = 37;
/// -> `GetAutoSelect()`.
pub const EE_SCREENS_CBW_L_AUTO: u32 = 38;
/// -> `GetHasSelection()`.
pub const EE_SCREENS_CBW_L_HAS: u32 = 39;
/// -> `GetSelectionIndex()`.
pub const EE_SCREENS_CBW_L_INDEX: u32 = 40;
/// `SetDirty(true)`.
pub const EE_SCREENS_CBW_L_DIRTY: u32 = 41;
/// (a flag) -> the list's flag: [`EE_SCREENS_CBW_FLAG_UPGRADES_ONLY`] ...
pub const EE_SCREENS_CBW_L_FLAG: u32 = 42;
/// (a flag, b value) the list's flag set.
pub const EE_SCREENS_CBW_L_SET_FLAG: u32 = 43;
/// `UIControl::DrawTextA()`, then -> `GetDrawCount()`.
pub const EE_SCREENS_CBW_L_DRAW_BEGIN: u32 = 44;
/// (a line) -> the drawn copy's line (`mCopyLineArray`).
pub const EE_SCREENS_CBW_L_COPY_LINE: u32 = 45;
/// (a line, b flags [`EE_SCREENS_CBW_DRAW_RIGHT`] | [`EE_SCREENS_CBW_DRAW_BOLD`], c colour, text) the text drawn in
/// the line's rectangle -> `DrawClippedText`'s result.
pub const EE_SCREENS_CBW_L_DRAW: u32 = 46;
/// -> `DrawScrollBarText()`.
pub const EE_SCREENS_CBW_L_DRAW_END: u32 = 47;

/// The list's flags: upgrade lines only selectable (`mSelectUpgradesOnly`), points shown (`mShowPoints`), lines
/// sorted (`mSortLines`), the points shown as of the last `CopyData` (`mCopiedShowPoints`).
pub const EE_SCREENS_CBW_FLAG_UPGRADES_ONLY: i64 = 0;
pub const EE_SCREENS_CBW_FLAG_SHOW_POINTS: i64 = 1;
pub const EE_SCREENS_CBW_FLAG_SORT: i64 = 2;
pub const EE_SCREENS_CBW_FLAG_COPIED_POINTS: i64 = 3;
/// A line's text right aligned at the rectangle's top right (else left at its top left); in bold.
pub const EE_SCREENS_CBW_DRAW_RIGHT: i64 = 1;
pub const EE_SCREENS_CBW_DRAW_BOLD: i64 = 2;
/// The colours: the header colour as of the last `CopyData` (`mC148`), black, dark blue (0, 0, 0.39).
pub const EE_SCREENS_CBW_COLOUR_HEADER: i64 = 0;
pub const EE_SCREENS_CBW_COLOUR_BLACK: i64 = 1;
pub const EE_SCREENS_CBW_COLOUR_DARK_BLUE: i64 = 2;

// the game's
/// (a string id) -> `gResource->GetString`'s result and the text.
pub const EE_SCREENS_CBW_STRING: u32 = 60;
/// (a the value's f32 bits) -> the value as `IDS_FORMAT_PERCENT_6_2G` prints it.
pub const EE_SCREENS_CBW_PERCENT: u32 = 61;
/// (text a file name) -> 1 when a premade civilization's file has that name (`PlatStrICmp`), with the message
/// (`IDS_FILE_NAME_YOU_SPECIFIED_IS_RESERVED` with the name).
pub const EE_SCREENS_CBW_RESERVED: u32 = 62;
/// (a upgrade) -> `gCivilizationManager.LookupLevel` found, then its value's and bonus' f32 bits.
pub const EE_SCREENS_CBW_LOOKUP_LEVEL: u32 = 63;
/// (a upgrade) -> `gCivilizationManager.GetExtraCost`.
pub const EE_SCREENS_CBW_EXTRA_COST: u32 = 64;
/// (a category) -> `GetCategoryNameID` found, then the id.
pub const EE_SCREENS_CBW_CATEGORY_NAME_ID: u32 = 65;
/// (a level) -> `EECivilizationManager::LevelNameID`.
pub const EE_SCREENS_CBW_LEVEL_NAME_ID: u32 = 66;
/// (a level) -> `EECivilizationManager::IsPowerLevel`.
pub const EE_SCREENS_CBW_IS_POWER_LEVEL: u32 = 67;
/// -> every upgrade (`gCivilizationManager.GetUpgrades()`).
pub const EE_SCREENS_CBW_ALL_UPGRADES: u32 = 68;
/// (a upgrade) -> `UpgradeKey` found, and the key.
pub const EE_SCREENS_CBW_UPGRADE_KEY: u32 = 69;
/// (text) -> the text as a quoted YAML scalar (`EEDataText::QuoteText`).
pub const EE_SCREENS_CBW_QUOTE: u32 = 70;

// a civilization's (p the `EECivilization*`)
pub const EE_SCREENS_CBW_C_BASE_COST: u32 = 80;
pub const EE_SCREENS_CBW_C_EXTRA_COST: u32 = 81;
/// (a base, b step, c count) -> `ComputeCost`.
pub const EE_SCREENS_CBW_C_COMPUTE: u32 = 82;
/// (a upgrade) -> `CountInCategory(a, NULL)`.
pub const EE_SCREENS_CBW_C_COUNT: u32 = 83;
pub const EE_SCREENS_CBW_C_AVAILABLE: u32 = 84;
pub const EE_SCREENS_CBW_C_UNCHANGED: u32 = 85;
pub const EE_SCREENS_CBW_C_MAX: u32 = 86;
/// -> its purchased upgrades.
pub const EE_SCREENS_CBW_C_PURCHASED: u32 = 87;
/// -> its name.
pub const EE_SCREENS_CBW_C_NAME: u32 = 88;
/// (a player, b the name's length, text the name then the upgrades as little-endian u32s) the request
/// (`EEMCivUpgradesRequested`: to the server in a multiplayer session, else executed at once).
pub const EE_SCREENS_CBW_REQUEST: u32 = 89;

// the world's
pub const EE_SCREENS_CBW_MULTIPLAYER: u32 = 100;
/// -> the current player's `mGameResult`.
pub const EE_SCREENS_CBW_GAME_RESULT: u32 = 101;
/// -> `gWorld.PauseGame()`.
pub const EE_SCREENS_CBW_PAUSE: u32 = 102;
pub const EE_SCREENS_CBW_UNPAUSE: u32 = 103;
/// -> the current player's civilization (an `EECivilization*`), 0 without a current player.
pub const EE_SCREENS_CBW_PLAYER_CIV: u32 = 104;
/// -> `gWorld.mCurrentPlayer`.
pub const EE_SCREENS_CBW_PLAYER_ID: u32 = 105;
/// (a 0 chosen, 1 locked) -> the current player's flag.
pub const EE_SCREENS_CBW_PLAYER_FLAG: u32 = 106;
/// (a 0 chosen, 1 locked, b value).
pub const EE_SCREENS_CBW_SET_PLAYER_FLAG: u32 = 107;
pub const EE_SCREENS_CBW_FREE_CAMERA: u32 = 108;
/// -> `gWorld.mGameOptions.CanBuyCivilization()`.
pub const EE_SCREENS_CBW_CAN_BUY: u32 = 109;
/// -> `GetCampaignMapName` found, and the name.
pub const EE_SCREENS_CBW_CAMPAIGN_NAME: u32 = 110;
/// -> `gAllowCustomCivs`.
pub const EE_SCREENS_CBW_CUSTOM_CIVS: u32 = 111;
/// (a 0 "sounds\button1.wav", 1 "sounds\button2.wav") -> `gSoundManager->Load(name, 0, 1)`.
pub const EE_SCREENS_CBW_SOUND_LOAD: u32 = 120;
pub const EE_SCREENS_CBW_SOUND_UNLOAD: u32 = 121;
/// (a sound) `Play(a, 1, 0, 1, false, NULL)`.
pub const EE_SCREENS_CBW_SOUND_PLAY: u32 = 122;
/// (text) `DisplayMessage(text, 1, NULL, NULL, true)`.
pub const EE_SCREENS_CBW_MESSAGE: u32 = 130;
/// (text, a the answer [`EE_SCREENS_CBW_ANSWER_WRITE`] ..., p its data) `DisplayQuestion` with Yes that answer.
pub const EE_SCREENS_CBW_QUESTION: u32 = 131;
/// The Yes answers: the file written (over the existing one), the selected file deleted, the changes accepted.
pub const EE_SCREENS_CBW_ANSWER_WRITE: i64 = 0;
pub const EE_SCREENS_CBW_ANSWER_DELETE: i64 = 1;
pub const EE_SCREENS_CBW_ANSWER_ACCEPT: i64 = 2;
/// (a) the main game form's `mIsButtonBlinking`.
pub const EE_SCREENS_CBW_BLINK_FLAG: u32 = 140;
/// (a 0 the builder button (form 11 by id, control 5), 1 the large alert (`mMainGameForm`'s control 234)) -> 1 when
/// it is there.
pub const EE_SCREENS_CBW_MAIN_BUTTON: u32 = 141;
/// (a button, b blinking) its `SetIsBlinking`.
pub const EE_SCREENS_CBW_MAIN_BLINK: u32 = 142;
/// (a button, b mode) its display mode.
pub const EE_SCREENS_CBW_MAIN_DISPLAY: u32 = 143;

// the civilization file being saved (the original's file spec 0x565518)
/// (text a directory) its volume the startup volume, its directory.
pub const EE_SCREENS_CBW_F_DIRECTORY: u32 = 150;
/// (text) its file name.
pub const EE_SCREENS_CBW_F_NAME: u32 = 151;
/// (a 0 read, 1 write) -> `Open`'s result.
pub const EE_SCREENS_CBW_F_OPEN: u32 = 152;
pub const EE_SCREENS_CBW_F_CLOSE: u32 = 153;
pub const EE_SCREENS_CBW_F_DELETE: u32 = 154;
pub const EE_SCREENS_CBW_F_CREATE: u32 = 155;
/// (text) -> `Write`'s result.
pub const EE_SCREENS_CBW_F_WRITE: u32 = 156;
/// -> its file name.
pub const EE_SCREENS_CBW_F_FILE: u32 = 157;
/// -> `gStartupDirectory`.
pub const EE_SCREENS_CBW_STARTUP_DIRECTORY: u32 = 158;
/// (text the saved file's name) the players using it (1 ..) reset (`EEAResetAllPlayerInfo`) -> how many.
pub const EE_SCREENS_CBW_RELOAD_PLAYERS: u32 = 159;
/// (text a civilization file's name) -> `EELoadCivilization` from the game's directory, else (into the same name
/// and upgrades) the user's: the result, the name and the upgrades.
pub const EE_SCREENS_CBW_LOAD_CIVILIZATION: u32 = 160;

// the forms and controls
pub(crate) const FORM_BUILDER: i64 = 7;
pub(crate) const FORM_NAME: i64 = 15;
pub(crate) const FORM_SAVE: i64 = 16;
pub(crate) const FORM_LOAD: i64 = 17;
pub(crate) const FORM_CHOICE: i64 = 35;
const CTL_NAME_TITLE: i64 = 0xc4;
const CTL_POINTS_LEFT_LABEL: i64 = 0xd1;
const CTL_POINTS_LEFT: i64 = 0xd2;
const CTL_POINTS_SPENT: i64 = 0xce;
const CTL_ACCEPT: i64 = 0x1c0;
const CTL_NEW_NAME: i64 = 0x19a;
const CTL_SAVE_LIST: i64 = 0x1a4;
const CTL_LOAD_LIST: i64 = 0x1a9;
const CTL_SAVE_NAME: i64 = 0x232;
pub(crate) const LIST_AVAILABLE: i64 = 0xcb;
pub(crate) const LIST_BOUGHT: i64 = 0xcc;
const MAIN_BUTTON: i64 = 0;
const MAIN_ALERT: i64 = 1;
const DISPLAY_HIDDEN: i64 = 0;
const DISPLAY_SHOWN: i64 = 1;
pub(crate) const ERROR_FAILED: c_long = 3;
const ERROR_NOT_ALLOWED: c_long = 0xd;
const ERROR_INVALID_NAME: c_long = 9;

// the language's strings (include/ee/EEStringIDs.h; the tests check them)
pub(crate) const IDS_CIVILIZATION_NAME: i64 = 7064;
pub(crate) const IDS_ERROR_DELETING_FILE: i64 = 10048;
pub(crate) const IDS_ERROR_CREATING_FILE: i64 = 10049;
pub(crate) const IDS_ERROR_SAVING_FILE: i64 = 10050;
pub(crate) const IDS_INVALID_FILE_NAME: i64 = 10068;
pub(crate) const IDS_DEFAULT_CIVILIZATION: i64 = 29780;
pub(crate) const IDS_CIVILIZATION_WITH_THAT_NAME_EXISTS_OVERWRITE: i64 = 29781;
pub(crate) const IDS_ARE_YOU_SURE_YOU_WANT_TO_DELETE: i64 = 29782;
pub(crate) const IDS_YOU_CANT_LOAD_CIVILIZATION_AFTER: i64 = 29784;
pub(crate) const IDS_YOU_HAVE_NOT_SPENT_ALL: i64 = 29795;
pub(crate) const IDS_CUSTOM_CIVILIZATION: i64 = 30492;
pub(crate) const IDS_SPECIAL_POWERS: i64 = 32850;

/// The events this module's code gives the forms (`EEScreenEvent` codes, `civ_builder.rs` and `civ_choice.rs`).
const EVENT_BUTTONS: i64 = crate::screens::civ_builder::EE_SCREENS_CB_BUTTONS as i64;
const EVENT_CLEAN_UP: i64 = crate::screens::civ_builder::EE_SCREENS_CB_CLEAN_UP as i64;
const EVENT_CHOICE_OPEN: i64 = crate::screens::civ_choice::EE_SCREENS_PDCS_OPEN as i64;
const EVENT_CHOICE_CLOSE: i64 = crate::screens::civ_choice::EE_SCREENS_PDCS_CLOSE as i64;

/// An op's outputs: a text and values.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeScreensCbOut {
    pub text: Vec<u8>,
    pub values: Vec<i64>,
}

/// The builder's calls into the toolkit, its files and the game: one op each (`EE_SCREENS_CBW_*`).
pub trait CbHost {
    /// The op `op` on the object `p` with the values `a`, `b`, `c` and the text `text`; its answer, its texts and
    /// values in `out`.
    #[allow(clippy::too_many_arguments)]
    fn op(&mut self, op: u32, p: usize, a: i64, b: i64, c: i64, text: &[u8], out: &mut EeScreensCbOut) -> i64;
}

/// The ops' shorthands.
pub(crate) trait Cb: CbHost {
    fn v(&mut self, op: u32, p: usize, a: i64, b: i64, c: i64) -> i64 {
        self.op(op, p, a, b, c, &[], &mut EeScreensCbOut::default())
    }
    fn vt(&mut self, op: u32, p: usize, a: i64, b: i64, text: &[u8]) -> i64 {
        self.op(op, p, a, b, 0, text, &mut EeScreensCbOut::default())
    }
    fn t(&mut self, op: u32, p: usize, a: i64, b: i64, c: i64) -> (i64, Vec<u8>) {
        let mut out = EeScreensCbOut::default();
        let r = self.op(op, p, a, b, c, &[], &mut out);
        (r, out.text)
    }
    fn vs(&mut self, op: u32, p: usize, a: i64, b: i64) -> (i64, Vec<i64>) {
        let mut out = EeScreensCbOut::default();
        let r = self.op(op, p, a, b, 0, &[], &mut out);
        (r, out.values)
    }
    /// A string of the language (its text even when `GetString` fails: what it left).
    fn string(&mut self, id: i64) -> (i64, Vec<u8>) {
        self.t(EE_SCREENS_CBW_STRING, 0, id, 0, 0)
    }
}

impl<H: CbHost + ?Sized> Cb for H {}

/// The builder's state: the original's globals (the last saved file's name, the two sounds, whether the builder
/// paused the game, whether the players using the saved file reload it, whether the upgrade lists are read-only,
/// whether a premade civilization was loaded). Locked only around its own reads and writes.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct CbState {
    pub last_file: Vec<u8>,
    pub buy_sound: i64,
    pub refused_sound: i64,
    pub paused: bool,
    pub reload_players: bool,
    pub locked: bool,
    pub premade_loaded: bool,
}

static STATE: Mutex<CbState> = Mutex::new(CbState {
    last_file: Vec::new(),
    buy_sound: 0,
    refused_sound: 0,
    paused: false,
    reload_players: false,
    locked: false,
    premade_loaded: false,
});

pub(crate) fn state() -> CbState {
    STATE.lock().unwrap().clone()
}

fn with_state<R>(f: impl FnOnce(&mut CbState) -> R) -> R {
    f(&mut STATE.lock().unwrap())
}

/// The edited civilization (the original's `gEditableCivilization`): made by the builder's open paths and the
/// campaign's save, deleted by the builder form's close event, the campaign and the world's reset.
static EDITABLE: AtomicPtr<Editable> = AtomicPtr::new(core::ptr::null_mut());

/// The edited civilization, if any. It lives until [`free_editable`]; nothing in this module frees it while a
/// reference is held (the close event that frees it runs from the UI thread's queue, never inside these calls).
pub(crate) fn editable() -> Option<&'static Editable> {
    let p = EDITABLE.load(Ordering::Acquire);
    // SAFETY: a Box::into_raw of set_editable, freed only by free_editable (see above)
    unsafe { p.as_ref() }
}

/// The edited civilization replaced (the original leaked the one before: kept so).
pub(crate) fn set_editable(e: Editable) {
    EDITABLE.store(Box::into_raw(Box::new(e)), Ordering::Release);
}

/// `delete gEditableCivilization; gEditableCivilization = NULL`.
pub(crate) fn free_editable() {
    let p = EDITABLE.swap(core::ptr::null_mut(), Ordering::AcqRel);
    if !p.is_null() {
        // SAFETY: made by set_editable
        drop(unsafe { Box::from_raw(p) });
    }
}

// ---------------------------------------------------------------------------------------------- the files

/// The legacy binary civilization file: the name's length and bytes, the upgrade count, each upgrade's legacy
/// `(category << 6) | level` id, little-endian u32s (the original's `FSFile::Write`s).
pub fn legacy_file(name: &[u8], upgrades: &[u32]) -> Vec<u8> {
    let mut out = Vec::with_capacity(8 + name.len() + 4 * upgrades.len());
    out.extend_from_slice(&(name.len() as u32).to_le_bytes());
    out.extend_from_slice(name);
    out.extend_from_slice(&(upgrades.len() as u32).to_le_bytes());
    for &u in upgrades {
        out.extend_from_slice(&to_legacy(u).0.to_le_bytes());
    }
    out
}

/// `EECivilizationManager::ToLegacy`: the legacy id and whether it fits.
pub fn to_legacy(upgrade: u32) -> (u32, bool) {
    let category = upgrade >> 16;
    let level = upgrade & 0xffff;
    ((category << 6) | (level & 0x3f), category < 0x400 && level < 0x40)
}

/// The keyed civilization text (the port's form): the header comment, `eedata: civ`, `format: 3`, the quoted name
/// and the upgrades' keys.
pub fn keyed_file(quoted_name: &[u8], keys: &[Vec<u8>]) -> Vec<u8> {
    let mut out = b"# a civilization (the civilization builder): its upgrades as <category id>.<attribute id>\n\
eedata: civ\nformat: 3\nname: "
        .to_vec();
    out.extend_from_slice(quoted_name);
    out.extend_from_slice(b"\nupgrades: [");
    for (i, k) in keys.iter().enumerate() {
        if i > 0 {
            out.extend_from_slice(b", ");
        }
        out.extend_from_slice(k);
    }
    out.extend_from_slice(b"]\n");
    out
}

// ---------------------------------------------------------------------------------------------- the helpers

fn edit_text<H: CbHost + ?Sized>(h: &mut H, form: i64, control: i64) -> (c_long, Vec<u8>) {
    let (r, t) = h.t(EE_SCREENS_CBW_EDIT_TEXT, 0, form, control, 0);
    (r as c_long, t)
}

/// `CivBuilderAcceptName`: a non-empty typed name the builder's title, then the dialog closed.
pub fn accept_name<H: CbHost + ?Sized>(h: &mut H) -> c_long {
    let (r, text) = edit_text(h, FORM_NAME, CTL_NEW_NAME);
    if r != 0 {
        return ERROR_FAILED;
    }
    let mut result = ERROR_FAILED;
    if !text.is_empty() {
        result = set_name(h, &text);
        if result == 0 {
            result = close_form(h);
        }
    }
    result
}

/// `CivBuilderGetName`: the builder's title -> 0 and the name, 3 without it.
pub fn get_name<H: CbHost + ?Sized>(h: &mut H) -> (c_long, Vec<u8>) {
    let (r, t) = h.t(EE_SCREENS_CBW_LABEL_TEXT, 0, FORM_BUILDER, CTL_NAME_TITLE, 0);
    (if r == 0 { 0 } else { ERROR_FAILED }, if r == 0 { t } else { Vec::new() })
}

/// `CivBuilderGetTypedName` (the name dialog) and `CivBuilderGetTypedFileName` (the save dialog).
pub fn typed_name<H: CbHost + ?Sized>(h: &mut H, file: bool) -> (c_long, Vec<u8>) {
    if file {
        edit_text(h, FORM_SAVE, CTL_SAVE_NAME)
    } else {
        edit_text(h, FORM_NAME, CTL_NEW_NAME)
    }
}

/// `CivBuilderGetSelectedUpgrade`: the selected line's upgrade, bonus and cost in the available list (`available`)
/// or the bought one; `None` when the list or a selection past the first line is missing. A selected line that does
/// not read whole keeps the caller's values for what it lacks (the original's out parameters).
pub fn selected_upgrade<H: CbHost + ?Sized>(
    h: &mut H,
    available: bool,
    init: (u32, u32, u32),
) -> Option<(u32, u32, u32)> {
    let list = upgrade_list(h, FORM_BUILDER, if available { LIST_AVAILABLE } else { LIST_BOUGHT });
    if list == 0 {
        return None;
    }
    civ_list::selected_upgrade(h, list, init).ok()
}

/// `CivBuilderHidePoints`: the points left's label and number and the accept button hidden; 3.
pub fn hide_points<H: CbHost + ?Sized>(h: &mut H) -> c_long {
    if h.v(EE_SCREENS_CBW_FORM_EXISTS, 0, FORM_BUILDER, 0, 0) != 0 {
        for c in [CTL_POINTS_LEFT_LABEL, CTL_POINTS_LEFT, CTL_ACCEPT] {
            h.v(EE_SCREENS_CBW_DISPLAY, 0, FORM_BUILDER, c, DISPLAY_HIDDEN);
        }
    }
    ERROR_FAILED
}

/// `CivBuilderCloseForm`.
pub fn close_form<H: CbHost + ?Sized>(h: &mut H) -> c_long {
    h.v(EE_SCREENS_CBW_CLOSE_FORM, 0, 0, 0, 0) as c_long
}

/// `CivBuilderFillUpgradeLists`: the bought list the civilization's upgrades, the available one every other; both
/// selectable unless the lists are read-only (the bought list first, as the original).
pub fn fill_upgrade_lists<H: CbHost + ?Sized>(h: &mut H, upgrades: &[u32]) -> c_long {
    if h.v(EE_SCREENS_CBW_FORM_EXISTS, 0, FORM_BUILDER, 0, 0) == 0 {
        return ERROR_FAILED;
    }
    let selectable = !state().locked as i64;
    let bought = upgrade_list(h, FORM_BUILDER, LIST_BOUGHT);
    civ_list::set_civilization_upgrades(h, bought, upgrades);
    h.v(EE_SCREENS_CBW_L_SET_FLAG, bought, EE_SCREENS_CBW_FLAG_UPGRADES_ONLY, selectable, 0);
    let available = upgrade_list(h, FORM_BUILDER, LIST_AVAILABLE);
    civ_list::set_upgrades_except(h, available, upgrades);
    h.v(EE_SCREENS_CBW_L_SET_FLAG, available, EE_SCREENS_CBW_FLAG_UPGRADES_ONLY, selectable, 0);
    0
}

/// `CivBuilderPushForm` (`CivBuilderPushBuilderForm` with the builder).
pub fn push_form<H: CbHost + ?Sized>(h: &mut H, form: i64) -> c_long {
    h.v(EE_SCREENS_CBW_PUSH, 0, form, 0, 0) as c_long
}

/// `CivBuilderSetName`: the builder's title.
pub fn set_name<H: CbHost + ?Sized>(h: &mut H, name: &[u8]) -> c_long {
    h.vt(EE_SCREENS_CBW_SET_LABEL_TEXT, 0, FORM_BUILDER, CTL_NAME_TITLE, name) as c_long
}

/// `EEShowCivBuilderPoints`: the points spent (max - left) and left (0 when negative).
pub fn show_points<H: CbHost + ?Sized>(h: &mut H, left: i64, max: i64) -> c_long {
    if h.v(EE_SCREENS_CBW_FORM_EXISTS, 0, FORM_BUILDER, 0, 0) == 0 {
        return ERROR_FAILED;
    }
    if h.v(EE_SCREENS_CBW_SET_LABEL_NUMBER, 0, FORM_BUILDER, CTL_POINTS_SPENT, max.wrapping_sub(left)) != 0 {
        return ERROR_FAILED;
    }
    if h.v(EE_SCREENS_CBW_SET_LABEL_NUMBER, 0, FORM_BUILDER, CTL_POINTS_LEFT, left.max(0)) != 0 {
        return ERROR_FAILED;
    }
    0
}

/// The edited civilization's points shown (`EEShowCivBuilderPoints(GetPointsLeft(), GetMaxPoints())`: the points
/// left an unsigned long passed as a long).
fn show_edit_points<H: CbHost + ?Sized>(h: &mut H, e: &Editable) {
    let left = e.points_left() as i32 as i64;
    let max = e.max_points(h);
    show_points(h, left, max);
}

/// `CivBuilderSaveAs`: the civilization saved under the save dialog's typed name, which is remembered.
pub fn save_as<H: CbHost + ?Sized>(h: &mut H) -> c_long {
    let (mut r, name) = typed_name(h, true);
    if r == 0 {
        r = save_civilization(h, &name, false);
        with_state(|s| s.last_file = name);
    }
    r
}

/// `CivBuilderShowNameDialog`: the name dialog with the builder's name (or "Civilization Name") selected.
pub fn show_name_dialog<H: CbHost + ?Sized>(h: &mut H) -> c_long {
    let mut r = push_form(h, FORM_NAME);
    if r == 0 {
        let (got, mut text) = get_name(h);
        r = got;
        if text.is_empty() {
            text = h.string(IDS_CIVILIZATION_NAME).1;
        }
        h.vt(EE_SCREENS_CBW_SET_EDIT_TEXT, 0, FORM_NAME, CTL_NEW_NAME, &text);
        h.v(EE_SCREENS_CBW_EDIT_SELECT, 0, FORM_NAME, CTL_NEW_NAME, 0);
    }
    r
}

/// `CivBuilderShowFileDialog`: a dialog opened, its file list filled.
pub fn show_file_dialog<H: CbHost + ?Sized>(h: &mut H, form: i64, control: i64) -> c_long {
    let r = push_form(h, form);
    if r != 0 {
        return r;
    }
    h.v(EE_SCREENS_CBW_FILE_LIST_FILL, 0, form, control, 0);
    0
}

/// `CivBuilderShowSaveDialog`: the save dialog with the last saved file's name (or the civilization's).
pub fn show_save_dialog<H: CbHost + ?Sized>(h: &mut H) -> c_long {
    let mut r = show_file_dialog(h, FORM_SAVE, CTL_SAVE_LIST);
    if r != 0 {
        return r;
    }
    let last = state().last_file;
    let text = if !last.is_empty() {
        last
    } else {
        let (got, name) = get_name(h);
        r = got;
        name
    };
    h.vt(EE_SCREENS_CBW_SET_EDIT_TEXT, 0, FORM_SAVE, CTL_SAVE_NAME, &text);
    r
}

/// `CivBuilderGetUpgradeList`: the form's control as an upgrade list (0 without it).
pub fn upgrade_list<H: CbHost + ?Sized>(h: &mut H, form: i64, control: i64) -> usize {
    h.v(EE_SCREENS_CBW_LIST, 0, form, control, 0) as usize
}

/// `CivBuilderMoveUpgrade`: an upgrade (the source list's selected line when 0) moved to the bought list
/// (`buy`) or back; then the lines reformatted (on a buy only the source list's).
pub fn move_upgrade<H: CbHost + ?Sized>(h: &mut H, form: i64, buy: bool, upgrade: u32) -> bool {
    let (from_id, to_id) = if buy { (LIST_AVAILABLE, LIST_BOUGHT) } else { (LIST_BOUGHT, LIST_AVAILABLE) };
    let from = upgrade_list(h, form, from_id);
    let to = upgrade_list(h, form, to_id);
    if from == 0 || to == 0 || from == to {
        return false;
    }
    let index =
        if upgrade == 0 { civ_list::current_selection(h, from) } else { civ_list::find_upgrade(h, from, upgrade) };
    if let Some(index) = index {
        let line = civ_list::line_at(h, from, index);
        if civ_list::token(&line, civ_list::TOKEN_NAME).is_some() {
            civ_list::delete_line(h, from, index);
            civ_list::reset_selection(h, from);
            civ_list::add_line(h, to, &line);
        }
    }
    if buy {
        civ_list::reformat(h, from, true);
    } else {
        civ_list::reformat(h, from, false);
        civ_list::reformat(h, to, true);
    }
    true
}

/// `EEProcessTimerExpired`: unless a builder form is on top, the predefined civilizations' choice once (the choice
/// locked), or the builder when custom civilizations are allowed.
pub fn process_timer_expired<H: CbHost + ?Sized>(h: &mut H) -> c_long {
    if h.v(EE_SCREENS_CBW_CURRENT_IS, 0, FORM_BUILDER, 0, 0) != 0
        || h.v(EE_SCREENS_CBW_CURRENT_IS, 0, FORM_CHOICE, 0, 0) != 0
    {
        return 0;
    }
    if h.v(EE_SCREENS_CBW_CUSTOM_CIVS, 0, 0, 0, 0) != 0 {
        return edit_civilization(h);
    }
    if h.v(EE_SCREENS_CBW_PLAYER_FLAG, 0, 0, 0, 0) == 0 {
        set_blinking(h, false);
        h.v(EE_SCREENS_CBW_SET_PLAYER_FLAG, 0, 0, 1, 0);
        h.v(EE_SCREENS_CBW_SET_PLAYER_FLAG, 0, 1, 1, 0);
        h.v(EE_SCREENS_CBW_FORM_EVENTS, 0, FORM_CHOICE, EVENT_CHOICE_OPEN, EVENT_CHOICE_CLOSE);
        h.v(EE_SCREENS_CBW_PUSH, 0, FORM_CHOICE, 0, 0);
    }
    0
}

/// `CivBuilderResetUpgradeSelection` (`CivBuilderResetAvailableSelection`, `CivBuilderResetBoughtSelection`) on
/// the form (by its id).
pub fn reset_upgrade_selection<H: CbHost + ?Sized>(h: &mut H, form: i64, control: i64) -> c_long {
    let list = upgrade_list(h, form, control);
    if list == 0 {
        return ERROR_FAILED;
    }
    civ_list::reset_selection(h, list);
    0
}

/// `CivBuilderReturnAllUpgrades`: every bought upgrade moved back; true.
pub fn return_all_upgrades<H: CbHost + ?Sized>(h: &mut H) -> bool {
    let bought = upgrade_list(h, FORM_BUILDER, LIST_BOUGHT);
    let lines = if bought == 0 { Vec::new() } else { civ_list::lines(h, bought) };
    for line in lines {
        if civ_list::is_upgrade_line(&line) {
            if let Some(u) = civ_list::number(&line, civ_list::TOKEN_UPGRADE) {
                move_upgrade(h, FORM_BUILDER, false, u);
            }
        }
    }
    true
}

/// `CivBuilderSaveNameSelected`: the save dialog's selected file into its name box.
pub fn save_name_selected<H: CbHost + ?Sized>(h: &mut H) -> c_long {
    let (r, values) = h.vs(EE_SCREENS_CBW_FILE_SELECTION, 0, FORM_SAVE, CTL_SAVE_LIST);
    if r < 0 {
        return ERROR_FAILED;
    }
    if r == 0 {
        let index = values.first().copied().unwrap_or(0);
        let (_, text) = h.t(EE_SCREENS_CBW_FILE_LINE, 0, FORM_SAVE, CTL_SAVE_LIST, index);
        h.vt(EE_SCREENS_CBW_SET_EDIT_TEXT, 0, FORM_SAVE, CTL_SAVE_NAME, &text);
    }
    r as c_long
}

/// `EESetCivBuilderButtonBlinking`: the main game form's builder button and its large alert blink (the alert
/// shown) or not.
pub fn set_blinking<H: CbHost + ?Sized>(h: &mut H, blinking: bool) -> c_long {
    h.v(EE_SCREENS_CBW_BLINK_FLAG, 0, blinking as i64, 0, 0);
    if h.v(EE_SCREENS_CBW_MAIN_BUTTON, 0, MAIN_BUTTON, 0, 0) == 0 {
        return 0;
    }
    h.v(EE_SCREENS_CBW_MAIN_BLINK, 0, MAIN_BUTTON, blinking as i64, 0);
    if h.v(EE_SCREENS_CBW_MAIN_BUTTON, 0, MAIN_ALERT, 0, 0) == 0 {
        return 0;
    }
    h.v(EE_SCREENS_CBW_MAIN_DISPLAY, 0, MAIN_ALERT, if blinking { DISPLAY_SHOWN } else { DISPLAY_HIDDEN }, 0);
    h.v(EE_SCREENS_CBW_MAIN_BLINK, 0, MAIN_ALERT, blinking as i64, 0);
    0
}

/// `CivBuilderMoveToBought` / `CivBuilderMoveToAvailable`: the move, the button sound on success; 0xd when it
/// cannot be moved.
pub fn move_to<H: CbHost + ?Sized>(h: &mut H, buy: bool, upgrade: u32) -> c_long {
    if !move_upgrade(h, FORM_BUILDER, buy, upgrade) {
        return ERROR_NOT_ALLOWED;
    }
    let sound = state().buy_sound;
    h.v(EE_SCREENS_CBW_SOUND_PLAY, 0, sound, 0, 0);
    0
}

// ---------------------------------------------------------------------------------------------- the files unit's

/// `CivBuilderCanChange`.
pub fn can_change<H: CbHost + ?Sized>(h: &mut H) -> bool {
    editable().is_some_and(|e| e.can_change(h))
}

/// `CivBuilderClose`: a game the builder paused resumed, the edits applied when asked, the form closed, the
/// sounds unloaded.
pub fn close<H: CbHost + ?Sized>(h: &mut H, apply: bool) -> c_long {
    if state().paused {
        h.v(EE_SCREENS_CBW_UNPAUSE, 0, 0, 0, 0);
    }
    if apply {
        if let Some(e) = editable() {
            e.apply(h);
        }
    }
    close_form(h);
    unload_sounds(h)
}

/// `CivBuilderLoadSounds`: the buy/sell and refused sounds; 3 when either fails.
pub fn load_sounds<H: CbHost + ?Sized>(h: &mut H) -> c_long {
    let buy = h.v(EE_SCREENS_CBW_SOUND_LOAD, 0, 0, 0, 0);
    let refused = h.v(EE_SCREENS_CBW_SOUND_LOAD, 0, 1, 0, 0);
    with_state(|s| {
        s.buy_sound = buy;
        s.refused_sound = refused;
    });
    if buy == 0 || refused == 0 {
        ERROR_FAILED
    } else {
        0
    }
}

/// `CivBuilderUnloadSounds`.
pub fn unload_sounds<H: CbHost + ?Sized>(h: &mut H) -> c_long {
    let s = state();
    h.v(EE_SCREENS_CBW_SOUND_UNLOAD, 0, s.buy_sound, 0, 0);
    h.v(EE_SCREENS_CBW_SOUND_UNLOAD, 0, s.refused_sound, 0, 0);
    0
}

/// `CivBuilderGetUserDirectory`: `Users\default\Civilizations\` under the startup directory.
pub fn user_directory<H: CbHost + ?Sized>(h: &mut H) -> Vec<u8> {
    let (_, mut d) = h.t(EE_SCREENS_CBW_STARTUP_DIRECTORY, 0, 0, 0, 0);
    d.extend_from_slice(b"Users\\default\\Civilizations\\");
    d
}

/// The overwrite question's close flag: the original passed a local bool's address that the answer read after the
/// function returned; false (what the unmodified port read: the save dialog stays open).
const OVERWRITE_CLOSE: bool = false;

/// `CivBuilderSaveCivilization`: a premade civilization's file name refused; an existing file's overwrite asked
/// first (unless `overwrite`); else written.
pub fn save_civilization<H: CbHost + ?Sized>(h: &mut H, name: &[u8], overwrite: bool) -> c_long {
    if !overwrite {
        let mut out = EeScreensCbOut::default();
        if h.op(EE_SCREENS_CBW_RESERVED, 0, 0, 0, 0, name, &mut out) != 0 {
            h.vt(EE_SCREENS_CBW_MESSAGE, 0, 0, 0, &out.text);
            return ERROR_INVALID_NAME;
        }
    }
    let directory = user_directory(h);
    h.vt(EE_SCREENS_CBW_F_DIRECTORY, 0, 0, 0, &directory);
    let mut file = name.to_vec();
    file.extend_from_slice(b".civ");
    h.vt(EE_SCREENS_CBW_F_NAME, 0, 0, 0, &file);
    if h.v(EE_SCREENS_CBW_F_OPEN, 0, 0, 0, 0) == 0 && !overwrite {
        let (r, text) = h.string(IDS_CIVILIZATION_WITH_THAT_NAME_EXISTS_OVERWRITE);
        if r == 0 {
            let mut out = EeScreensCbOut::default();
            h.op(EE_SCREENS_CBW_QUESTION, 0, EE_SCREENS_CBW_ANSWER_WRITE, 0, 0, &text, &mut out);
        }
        r as c_long
    } else {
        write_civilization(h, !overwrite);
        0
    }
}

/// `CivBuilderWriteCivilization`: the edited civilization written to the file (the keyed text, or the legacy form
/// when an upgrade has no key), the players using that file told to reload it, an error shown; the builder's form
/// closed after a clean write when asked (`close`).
pub fn write_civilization<H: CbHost + ?Sized>(h: &mut H, close: bool) {
    let (_, file) = h.t(EE_SCREENS_CBW_F_FILE, 0, 0, 0, 0);
    let mut message = Vec::new();
    if h.v(EE_SCREENS_CBW_F_CLOSE, 0, 0, 0, 0) != 0 {
        h.v(EE_SCREENS_CBW_F_DELETE, 0, 0, 0, 0);
    } else {
        h.v(EE_SCREENS_CBW_F_DELETE, 0, 0, 0, 0);
        let mut r = h.v(EE_SCREENS_CBW_F_CREATE, 0, 0, 0, 0);
        if r == 0 {
            r = h.v(EE_SCREENS_CBW_F_OPEN, 0, 1, 0, 0);
            if r == 0 {
                let e = editable();
                let name = e.map(|e| e.name()).unwrap_or_default();
                let upgrades = e.map(|e| e.copy_upgrade_ids(h)).unwrap_or_default();
                let bytes = keyed_text(h, &name, &upgrades).unwrap_or_else(|| legacy_file(&name, &upgrades));
                r = h.vt(EE_SCREENS_CBW_F_WRITE, 0, 0, 0, &bytes);
                if r != 0 {
                    message = h.string(IDS_ERROR_SAVING_FILE).1;
                }
            } else {
                message = h.string(IDS_ERROR_SAVING_FILE).1;
            }
        } else if r == ERROR_INVALID_NAME as i64 {
            message = h.string(IDS_INVALID_FILE_NAME).1;
        } else {
            message = h.string(IDS_ERROR_CREATING_FILE).1;
        }
        if r != 0 {
            h.v(EE_SCREENS_CBW_F_DELETE, 0, 0, 0, 0);
        } else if state().reload_players {
            h.vt(EE_SCREENS_CBW_RELOAD_PLAYERS, 0, 0, 0, &file);
        }
    }
    h.v(EE_SCREENS_CBW_F_CLOSE, 0, 0, 0, 0);
    if message.is_empty() {
        if close {
            close_form(h);
        }
    } else {
        h.vt(EE_SCREENS_CBW_MESSAGE, 0, 0, 0, &message);
    }
}

/// The keyed text of a civilization, `None` when an upgrade has no key.
fn keyed_text<H: CbHost + ?Sized>(h: &mut H, name: &[u8], upgrades: &[u32]) -> Option<Vec<u8>> {
    let mut keys = Vec::with_capacity(upgrades.len());
    for &u in upgrades {
        let (found, key) = h.t(EE_SCREENS_CBW_UPGRADE_KEY, 0, u as i64, 0, 0);
        if found == 0 {
            return None;
        }
        keys.push(key);
    }
    let (_, quoted) = {
        let mut out = EeScreensCbOut::default();
        let r = h.op(EE_SCREENS_CBW_QUOTE, 0, 0, 0, 0, name, &mut out);
        (r, out.text)
    };
    Some(keyed_file(&quoted, &keys))
}

/// `CivBuilderLoadCivilization`: a civilization file (the game's, else the user's) into the builder: the edit reset
/// and named, every bought upgrade returned, its upgrades bought.
pub fn load_civilization<H: CbHost + ?Sized>(h: &mut H, name: &[u8]) -> c_long {
    let mut out = EeScreensCbOut::default();
    let mut r = h.op(EE_SCREENS_CBW_LOAD_CIVILIZATION, 0, 0, 0, 0, name, &mut out) as c_long;
    if r != 0 {
        return r;
    }
    let (civ_name, upgrades) = (out.text, out.values);
    if let Some(e) = editable() {
        e.reset_to_start(h);
        e.set_name(&civ_name);
    }
    with_state(|s| s.premade_loaded = true);
    if return_all_upgrades(h) {
        for u in upgrades {
            buy_upgrade(h, u as u32);
        }
    } else {
        r = ERROR_FAILED;
    }
    r
}

/// `CivBuilderEditCurrentCivilization`: the current player's civilization edited.
pub fn edit_current_civilization<H: CbHost + ?Sized>(h: &mut H) {
    let civ = h.v(EE_SCREENS_CBW_PLAYER_CIV, 0, 0, 0, 0) as usize;
    let player = h.v(EE_SCREENS_CBW_PLAYER_ID, 0, 0, 0, 0) as u8;
    set_editable(Editable::new(h, civ, player, None));
}

/// `CivBuilderOpenForm`: the builder for the current player (its buttons by the kind of game: the open event's
/// flags), its lists, name and points shown, its sounds loaded.
pub fn open_form<H: CbHost + ?Sized>(h: &mut H) -> c_long {
    let buttons = |accept: bool, exit: bool, load: bool, save: bool, hide: bool| {
        EVENT_BUTTONS
            + (accept as i64 | (exit as i64) << 1 | (load as i64) << 2 | (save as i64) << 3 | (hide as i64) << 4)
    };
    let event = if h.v(EE_SCREENS_CBW_PLAYER_CIV, 0, 0, 0, 0) != 0 {
        if h.v(EE_SCREENS_CBW_FREE_CAMERA, 0, 0, 0, 0) != 0 {
            with_state(|s| {
                s.locked = false;
                s.premade_loaded = false;
            });
            buttons(false, true, true, true, true)
        } else {
            let locked = h.v(EE_SCREENS_CBW_PLAYER_FLAG, 0, 1, 0, 0) != 0;
            with_state(|s| s.locked = locked);
            if h.v(EE_SCREENS_CBW_CAN_BUY, 0, 0, 0, 0) != 0 {
                with_state(|s| s.premade_loaded = false);
                let was_open = h.v(EE_SCREENS_CBW_PLAYER_FLAG, 0, 0, 0, 0) != 0;
                h.v(EE_SCREENS_CBW_SET_PLAYER_FLAG, 0, 1, 1, 0);
                if !was_open {
                    buttons(true, false, true, false, false)
                } else {
                    buttons(false, true, false, false, false)
                }
            } else {
                buttons(true, true, false, false, false)
            }
        }
    } else {
        with_state(|s| s.locked = false);
        buttons(false, true, true, true, false)
    };
    h.v(EE_SCREENS_CBW_FORM_EVENTS, 0, FORM_BUILDER, event, EVENT_CLEAN_UP);
    let mut r = push_form(h, FORM_BUILDER);
    if r != 0 {
        return r;
    }
    r = load_sounds(h);
    if r != 0 {
        return r;
    }
    let Some(e) = editable() else { return ERROR_FAILED };
    let upgrades = e.civilization_upgrades(h);
    r = fill_upgrade_lists(h, &upgrades);
    if r != 0 {
        return r;
    }
    let (found, campaign) = h.t(EE_SCREENS_CBW_CAMPAIGN_NAME, 0, 0, 0, 0);
    let name = if found != 0 { campaign } else { e.name() };
    r = set_name(h, &name);
    if r == 0 {
        show_edit_points(h, e);
        let directory = user_directory(h);
        h.vt(EE_SCREENS_CBW_F_DIRECTORY, 0, 0, 0, &directory);
        set_blinking(h, false);
    }
    r
}

/// `CivBuilderShowLoad`: the load dialog, or the message that the civilization may no longer change.
pub fn show_load<H: CbHost + ?Sized>(h: &mut H) -> c_long {
    if can_change(h) {
        show_file_dialog(h, FORM_LOAD, CTL_LOAD_LIST)
    } else {
        let (r, text) = h.string(IDS_YOU_CANT_LOAD_CIVILIZATION_AFTER);
        h.vt(EE_SCREENS_CBW_MESSAGE, 0, 0, 0, &text);
        r as c_long
    }
}

/// `CivBuilderEditCivilization`: the game paused, the builder opened on the current player's civilization, which
/// is then chosen.
pub fn edit_civilization<H: CbHost + ?Sized>(h: &mut H) -> c_long {
    let r = h.v(EE_SCREENS_CBW_PAUSE, 0, 0, 0, 0) as c_long;
    with_state(|s| s.paused = r == 0);
    if r != 9 && r != 2 && r != 0 {
        return r;
    }
    edit_current_civilization(h);
    with_state(|s| s.reload_players = false);
    let r = open_form(h);
    if r == 0 {
        h.v(EE_SCREENS_CBW_SET_PLAYER_FLAG, 0, 0, 1, 0);
    }
    r
}

/// `CivBuilderCreateCivilization`: the game paused, the builder opened on a new civilization made from `civ`
/// (unlimited points with the free camera).
pub fn create_civilization<H: CbHost + ?Sized>(h: &mut H, civ: usize, reload: bool) -> c_long {
    let mut r = h.v(EE_SCREENS_CBW_PAUSE, 0, 0, 0, 0) as c_long;
    with_state(|s| s.paused = r == 0);
    if r == 9 || r == 2 || r == 0 {
        let unlimited = h.v(EE_SCREENS_CBW_FREE_CAMERA, 0, 0, 0, 0) != 0;
        set_editable(Editable::new(h, civ, 0, Some(unlimited)));
        with_state(|s| s.reload_players = reload);
        r = open_form(h);
    }
    r
}

/// `CivBuilderBuyUpgrade`: an upgrade (the selected available one when 0) bought and the points shown; the
/// refused sound when it is not affordable.
pub fn buy_upgrade<H: CbHost + ?Sized>(h: &mut H, upgrade: u32) -> c_long {
    if !can_change(h) {
        return ERROR_NOT_ALLOWED;
    }
    let mut chosen = upgrade;
    if upgrade == 0 {
        match selected_upgrade(h, true, (0, 0, 0)) {
            Some((u, _, _)) => chosen = u,
            None => return ERROR_FAILED,
        }
    }
    let Some(e) = editable() else { return 0 };
    if e.add_upgrade(h, chosen) {
        let r = move_to(h, true, upgrade);
        if r == 0 {
            show_edit_points(h, e);
        }
        r
    } else {
        let sound = state().refused_sound;
        h.v(EE_SCREENS_CBW_SOUND_PLAY, 0, sound, 0, 0);
        0
    }
}

/// `CivBuilderReturnUpgrade`: the selected bought upgrade sold back, unless the civilization owned it before the
/// edit.
pub fn return_upgrade<H: CbHost + ?Sized>(h: &mut H) -> c_long {
    if !can_change(h) {
        return ERROR_FAILED;
    }
    let Some((upgrade, _, _)) = selected_upgrade(h, false, (0, 0, 0)) else { return ERROR_FAILED };
    let Some(e) = editable() else { return ERROR_FAILED };
    if e.civilization_upgrades(h).contains(&upgrade) {
        return ERROR_FAILED;
    }
    e.remove_upgrade(h, upgrade);
    let r = move_to(h, false, 0);
    if r == 0 {
        show_edit_points(h, e);
    }
    r
}

/// `CivBuilderAskDelete`: whether to delete the dialog's selected file.
pub fn ask_delete<H: CbHost + ?Sized>(h: &mut H, form: usize) -> c_long {
    let (r, text) = h.string(IDS_ARE_YOU_SURE_YOU_WANT_TO_DELETE);
    if r == 0 {
        h.op(EE_SCREENS_CBW_QUESTION, form, EE_SCREENS_CBW_ANSWER_DELETE, 0, 0, &text, &mut EeScreensCbOut::default());
    }
    r as c_long
}

/// `CivBuilderDeleteSelected`: the file selected in the load or save dialog `form` deleted; a failure shown.
pub fn delete_selected<H: CbHost + ?Sized>(h: &mut H, form: usize) {
    let (form_id, control) = if h.v(EE_SCREENS_CBW_IS_FORM, form, FORM_LOAD, 0, 0) != 0 {
        (FORM_LOAD, CTL_LOAD_LIST)
    } else if h.v(EE_SCREENS_CBW_IS_FORM, form, FORM_SAVE, 0, 0) != 0 {
        (FORM_SAVE, CTL_SAVE_LIST)
    } else {
        return;
    };
    let (r, _) = h.vs(EE_SCREENS_CBW_FILE_SELECTION, 0, form_id, control);
    if r == 0 && h.v(EE_SCREENS_CBW_FILE_DELETE, 0, form_id, control, 0) != 0 {
        let (_, text) = h.string(IDS_ERROR_DELETING_FILE);
        h.vt(EE_SCREENS_CBW_MESSAGE, 0, 0, 0, &text);
    }
}

/// `CivBuilderAskAccept`: the warning that not every point was spent, asking before the builder closes.
pub fn ask_accept<H: CbHost + ?Sized>(h: &mut H, form: usize) -> c_long {
    let (r, text) = h.string(IDS_YOU_HAVE_NOT_SPENT_ALL);
    if r == 0 {
        h.op(EE_SCREENS_CBW_QUESTION, form, EE_SCREENS_CBW_ANSWER_ACCEPT, 0, 0, &text, &mut EeScreensCbOut::default());
    }
    r as c_long
}

/// `CivBuilderAcceptChanges`: the builder closed, a changed civilization applied (an unchanged one named from the
/// campaign's map, else "Custom Civilization" unless a premade one was loaded).
pub fn accept_changes<H: CbHost + ?Sized>(h: &mut H) {
    let e = editable();
    let choices = can_change(h) && e.is_some_and(|e| e.has_choices(h));
    if !choices {
        close(h, false);
        return;
    }
    if !e.is_some_and(|e| e.is_unchanged(h)) {
        close(h, true);
        return;
    }
    let (found, name) = h.t(EE_SCREENS_CBW_CAMPAIGN_NAME, 0, 0, 0, 0);
    if found != 0 {
        if set_name(h, &name) == 0 {
            if let Some(e) = editable() {
                e.set_name(&name);
            }
            close(h, true);
        }
    } else {
        if !state().premade_loaded {
            let (_, name) = h.string(IDS_CUSTOM_CIVILIZATION);
            if let Some(e) = editable() {
                e.set_name(&name);
            }
        }
        close(h, true);
    }
}

/// The question's Yes (`EE_SCREENS_CBW_ANSWER_*`) with its data (a form).
pub fn answer<H: CbHost + ?Sized>(h: &mut H, which: i64, form: usize) {
    match which {
        EE_SCREENS_CBW_ANSWER_WRITE => write_civilization(h, OVERWRITE_CLOSE),
        EE_SCREENS_CBW_ANSWER_DELETE => delete_selected(h, form),
        _ => accept_changes(h),
    }
}

// ---------------------------------------------------------------------------------------------- the events' ops

/// The text the builder's events keep (`EE_SCREENS_TEXT_CB_NAME`: the typed name, or the load list's file).
static KEPT_TEXT: Mutex<Vec<u8>> = Mutex::new(Vec::new());

fn keep(text: Vec<u8>) {
    *KEPT_TEXT.lock().unwrap() = text;
}

/// One of the builder's events' ops (`civ_builder.rs`'s `EE_SCREENS_GAME_CB_*` / `EE_SCREENS_OP_CB_SHOW`) on the
/// event's form and entity; `None` for another op (the screens' host's).
pub fn event_call<H: CbHost + ?Sized>(h: &mut H, form: usize, entity: usize, op: u32, a: u32) -> Option<u32> {
    use crate::screens::civ_builder::*;
    let _ = entity;
    Some(match op {
        EE_SCREENS_GAME_CB_SHOW_NAME_DIALOG => show_name_dialog(h) as u32,
        EE_SCREENS_GAME_CB_TYPED => {
            keep(Vec::new());
            let (r, text) = typed_name(h, a != 0);
            if r == 0 {
                keep(text);
            }
            r as u32
        }
        EE_SCREENS_GAME_CB_ACCEPT_NAME => accept_name(h) as u32,
        EE_SCREENS_GAME_CB_CLOSE => close(h, a != 0) as u32,
        EE_SCREENS_GAME_CB_SAVE_AS => save_as(h) as u32,
        EE_SCREENS_GAME_CB_SHOW_LOAD => show_load(h) as u32,
        EE_SCREENS_GAME_CB_SHOW_SAVE => show_save_dialog(h) as u32,
        EE_SCREENS_GAME_CB_SELECTION => {
            // the load list (by the manager) or the event form's save list; the load list's file kept
            let (p, f, c) = if a != 0 { (form, 0, CTL_SAVE_LIST) } else { (0, FORM_LOAD, CTL_LOAD_LIST) };
            let (r, values) = h.vs(EE_SCREENS_CBW_FILE_SELECTION, p, f, c);
            if r < 0 {
                return Some(EE_SCREENS_CB_NO_LIST);
            }
            if r == 0 && a == 0 {
                let index = values.first().copied().unwrap_or(0);
                keep(h.t(EE_SCREENS_CBW_FILE_LINE, p, f, c, index).1);
            }
            r as u32
        }
        EE_SCREENS_GAME_CB_SET_NAME_FROM_CIVILIZATION => {
            let name = editable().map(|e| e.name()).unwrap_or_default();
            set_name(h, &name) as u32
        }
        EE_SCREENS_GAME_CB_CLOSE_FORM => close_form(h) as u32,
        EE_SCREENS_GAME_CB_UNCHANGED => editable().is_some_and(|e| e.is_unchanged(h)) as u32,
        EE_SCREENS_GAME_CB_SAVE_NAME_SELECTED => save_name_selected(h) as u32,
        EE_SCREENS_GAME_CB_CAN_CHANGE => can_change(h) as u32,
        EE_SCREENS_GAME_CB_RESET_AVAILABLE | EE_SCREENS_GAME_CB_RESET_BOUGHT => {
            let id = h.v(EE_SCREENS_CBW_FORM_ID, form, 0, 0, 0);
            let list = if op == EE_SCREENS_GAME_CB_RESET_AVAILABLE { LIST_AVAILABLE } else { LIST_BOUGHT };
            reset_upgrade_selection(h, id, list) as u32
        }
        EE_SCREENS_GAME_CB_LOCKED => state().locked as u32,
        EE_SCREENS_GAME_CB_BUY => buy_upgrade(h, 0) as u32,
        EE_SCREENS_GAME_CB_RETURN => return_upgrade(h) as u32,
        EE_SCREENS_GAME_CB_POINTS_LEFT => editable().map_or(0, |e| e.points_left()),
        EE_SCREENS_GAME_CB_ASK_ACCEPT => ask_accept(h, form) as u32,
        EE_SCREENS_GAME_CB_ACCEPT_CHANGES => {
            accept_changes(h);
            0
        }
        EE_SCREENS_GAME_CB_FORM_ID => h.v(EE_SCREENS_CBW_FORM_ID, form, 0, 0, 0) as u32,
        EE_SCREENS_GAME_CB_ASK_DELETE => ask_delete(h, form) as u32,
        EE_SCREENS_GAME_CB_CLEAN_UP => {
            free_editable();
            0
        }
        EE_SCREENS_OP_CB_SHOW => {
            let mode = if a & 1 != 0 { DISPLAY_SHOWN } else { DISPLAY_HIDDEN };
            h.v(EE_SCREENS_CBW_DISPLAY_INDEX, 0, FORM_BUILDER, (a >> 1) as i64, mode);
            0
        }
        EE_SCREENS_GAME_CB_HIDE_POINTS => {
            hide_points(h);
            0
        }
        _ => return None,
    })
}

/// The events' text in (`EE_SCREENS_TEXT_CB_NAME`: the kept text); `None` for another.
pub fn event_text(which: u32) -> Option<Vec<u8>> {
    (which == crate::screens::civ_builder::EE_SCREENS_TEXT_CB_NAME).then(|| KEPT_TEXT.lock().unwrap().clone())
}

/// The events' texts out: the edited civilization's name, the builder's title, a civilization file loaded; `None`
/// for another.
pub fn event_put_text<H: CbHost + ?Sized>(h: &mut H, which: u32, text: &[u8]) -> Option<u32> {
    use crate::screens::civ_builder::*;
    Some(match which {
        EE_SCREENS_TEXT_CB_CIVILIZATION_NAME => {
            if let Some(e) = editable() {
                e.set_name(text);
            }
            0
        }
        EE_SCREENS_TEXT_CB_SET_NAME => set_name(h, text) as u32,
        EE_SCREENS_TEXT_CB_LOAD => load_civilization(h, text) as u32,
        _ => return None,
    })
}

#[cfg(test)]
pub(crate) mod tests;
